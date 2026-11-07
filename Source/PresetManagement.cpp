#include "PluginProcessor.h"
#include "XYMapping.h"
namespace
{
const char *soundIds[]{"x",      "y",       "mutate",  "drive",       "mode",      "mix",
                       "output", "protect", "enabled", "regionStart", "regionEnd", "dnaSpeed"};
const double lows[]{-1, -1, 0, 0, 0, 0, -24, 0, 0, 0, 0, 0}, highs[]{1, 1, 100, 36, 1, 100, 6, 1, 1, 1, 1, 4};
juce::String signature(const juce::ValueTree &s, const juce::String &dna)
{
    juce::String text = dna;
    for (int i = 0; i < 12; ++i)
    {
        const double v = s.getChildWithProperty("id", soundIds[i])["value"];
        text += ":" + juce::String(static_cast<juce::int64>(
                          std::llround((v - lows[i]) / (highs[i] - lows[i]) * 100000.)));
    }
    if(static_cast<int>(s.getProperty("xyMapping",1))>=2)text+=":xy"+s["xyMapping"].toString();
    return text;
}
} // namespace
void WubrotProcessor::updateDnaKey(const DnaProfile &p)
{
    dnaKey =
        (!p.valid && dnaOriginKey.isEmpty()) ? juce::String() : dnaOriginKey + ":" + WubrotPresetData::key(p);
}
juce::String WubrotProcessor::soundSignature()
{
    const juce::ScopedLock lock(metadataLock);
    juce::String text = dnaKey;
    for (int i = 0; i < 12; ++i)
        text +=
            ":" +
            juce::String(static_cast<juce::int64>(std::llround(
                (values[static_cast<std::size_t>(i)]->load() - lows[i]) / (highs[i] - lows[i]) * 100000.)));
    if(xyMapping.load()>=2)text+=":xy"+juce::String(xyMapping.load());
    return text;
}
void WubrotProcessor::markPresetClean()
{
    const juce::ScopedLock lock(metadataLock);
    presetBaseline = soundSignature();
}
bool WubrotProcessor::hasUnsavedPresetChanges()
{
    const juce::ScopedLock lock(metadataLock);
    return soundSignature() != presetBaseline;
}
juce::Result WubrotProcessor::capturePortableState(juce::ValueTree &state, juce::String &savedSignature,
                                                   std::uint64_t &capturedGeneration)
{
    serviceRegionEdit();
    for (int i = 0; i < 15000 && loading.load() && !stopping.load(); ++i)
        juce::Thread::sleep(2);
    if (loading.load() || stopping.load())
        return juce::Result::fail("DNA analysis is busy. Please try again when it finishes.");
    std::shared_ptr<const SampleSource> snapshot;
    std::shared_ptr<const WubrotPresetData::Archive> archive;
    const auto token = generation.load();
    {
        const juce::ScopedLock lock(metadataLock);
        snapshot = source;
        archive = analysisArchive;
    }
    if (snapshot && !archive)
    {
        auto prepared = std::make_shared<WubrotPresetData::Archive>();
        prepared->full = snapshot->full;
        const int frames = snapshot->audio.getNumSamples();
        for (int i = 0; i < prepared->slices; ++i)
        {
            const int first = i * frames / prepared->slices,
                      last = std::max(first + 1, (i + 1) * frames / prepared->slices);
            std::array<const float *, 8> channels{};
            for (int ch = 0; ch < snapshot->audio.getNumChannels(); ++ch)
                channels[static_cast<std::size_t>(ch)] = snapshot->audio.getReadPointer(ch) + first;
            if(snapshot->full.spectralCount>0){prepared->spectra[std::size_t(i)]=snapshot->full.spectralAt((i+.5f)/prepared->slices).toSpectrum();continue;}
            prepared->spectra[static_cast<std::size_t>(i)] =
                DnaSpectrum::analyse(channels.data(), snapshot->audio.getNumChannels(),
                                     static_cast<std::size_t>(last - first), snapshot->rate);
        }
        archive = prepared;
    }
    const juce::ScopedLock lock(metadataLock);
    if (token != generation.load() || source != snapshot || loading.load())
        return juce::Result::fail("The loaded DNA changed during saving. Try Save again.");
    if ((source || analysisArchive) && (std::abs(values[9]->load() - analysedStart) > 1e-6f ||
                                        std::abs(values[10]->load() - analysedEnd) > 1e-6f))
        return juce::Result::fail("The DNA region changed during saving. Try Save again.");
    capturedGeneration = token;
    analysisArchive = archive;
    state = captureState(false);
    state.removeProperty("sourceWav", nullptr);
    state.removeProperty("dnaTemporal",nullptr); // Included once in the exact presetProfile below.
    state.removeProperty("fullEnvelope", nullptr);
    state.removeProperty("editorSize", nullptr);
    auto reader = profiles.read();
    const DnaProfile empty;
    const auto &active = reader.get() ? *reader.get() : empty;
    if (active.valid && !archive)
        return juce::Result::fail("Reload the original sample once before saving this legacy DNA as a "
                                  "portable preset. Full region analysis is required.");
    float x = static_cast<float>(state.getChildWithProperty("id", "x")["value"]),
          y = static_cast<float>(state.getChildWithProperty("id", "y")["value"]);
    const auto mapped=XYMapping::map(x,y,runningRate.load(),xyMapping.load());
    state.setProperty("crushBitDepth", mapped.bits, nullptr);
    state.setProperty("referenceSampleRate", runningRate.load(), nullptr);
    state.setProperty("crushSampleRate", mapped.rate, nullptr);
    if (!active.valid && !archive)
    {
        state.setProperty("analysisStart", state.getChildWithProperty("id", "regionStart")["value"], nullptr);
        state.setProperty("analysisEnd", state.getChildWithProperty("id", "regionEnd")["value"], nullptr);
    }
    state.setProperty("presetProfile", juce::JSON::toString(WubrotPresetData::profile(active), true),
                      nullptr);
    savedSignature = signature(state, dnaKey);
    return PresetLibrary::validate(state);
}
juce::Result WubrotProcessor::saveUserPreset(const juce::String &name, bool overwrite)
{
    auto r = PresetLibrary::validName(name);
    if (r.failed())
        return r;
    juce::ValueTree state;
    juce::String baseline;
    std::uint64_t capturedGeneration = 0;
    r = capturePortableState(state, baseline, capturedGeneration);
    if (r.failed())
        return r;
    state.setProperty("presetDisplayName", name, nullptr);
    state.setProperty("presetUserFile", name + ".wubpreset", nullptr);
    state.setProperty("presetBaseline", baseline, nullptr);
    r = presetLibrary.save(state, name, overwrite);
    if (r.wasOk())
    {
        const auto filename = presetLibrary.fileFor(name).getFileName();
        const juce::ScopedLock lock(metadataLock);
        if (generation.load() == capturedGeneration)
        {
            activePresetName = name;
            activeUserFile = filename;
            presetBaseline = baseline;
        }
    }
    refreshPresetLibrary();
    if (r.wasOk())
        updateHostDisplay(juce::AudioProcessorListener::ChangeDetails().withNonParameterStateChanged(true));
    return r;
}
juce::Result WubrotProcessor::loadUserPreset(const juce::String &filename)
{
    const auto file = presetLibrary.resolve(filename);
    if (file == juce::File())
        return juce::Result::fail("Invalid user preset filename.");
    juce::ValueTree state;
    juce::String name;
    auto r = PresetLibrary::read(file, state, name);
    if (r.failed())
        return r;
    state.setProperty("editorSize", editorSize.load(), nullptr);
    state.setProperty("presetDisplayName", file.getFileNameWithoutExtension(), nullptr);
    state.setProperty("presetUserFile", file.getFileName(), nullptr);
    juce::MemoryBlock bytes;
    auto xml = state.createXml();
    copyXmlToBinary(*xml, bytes);
    setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
    markPresetClean();
    updateHostDisplay(
        juce::AudioProcessorListener::ChangeDetails().withProgramChanged(true).withNonParameterStateChanged(
            true));
    return juce::Result::ok();
}
juce::Result WubrotProcessor::renameUserPreset(const juce::String &filename, const juce::String &name,
                                               bool overwrite)
{
    auto r = presetLibrary.rename(filename, name, overwrite);
    if (r.wasOk())
    {
        const auto renamedFilename = presetLibrary.fileFor(name).getFileName();
        const juce::ScopedLock lock(metadataLock);
        if (activeUserFile.equalsIgnoreCase(filename))
        {
            activePresetName = name;
            activeUserFile = renamedFilename;
        }
    }
    refreshPresetLibrary();
    if (r.wasOk())
        updateHostDisplay(juce::AudioProcessorListener::ChangeDetails().withNonParameterStateChanged(true));
    return r;
}
juce::Result WubrotProcessor::deleteUserPreset(const juce::String &filename)
{
    auto r = presetLibrary.remove(filename);
    if (r.wasOk())
    {
        const juce::ScopedLock lock(metadataLock);
        if (activeUserFile.equalsIgnoreCase(filename))
        {
            activeUserFile.clear();
            activePresetName = "UNSAVED SOUND";
            presetBaseline.clear();
        }
    }
    refreshPresetLibrary();
    if (r.wasOk())
        updateHostDisplay(juce::AudioProcessorListener::ChangeDetails().withNonParameterStateChanged(true));
    return r;
}
juce::Result WubrotProcessor::importPreset(const juce::File &file, const juce::String &requestedName,
                                           bool overwrite)
{
    juce::ValueTree state;
    juce::String name;
    auto r = PresetLibrary::read(file, state, name);
    if (r.failed())
        return r;
    if (requestedName.isNotEmpty())
        name = requestedName;
    state.setProperty("presetDisplayName", name, nullptr);
    state.setProperty("presetUserFile", name + ".wubpreset", nullptr);
    r = presetLibrary.save(state, name, overwrite);
    refreshPresetLibrary();
    if (r.wasOk())
        updateHostDisplay(juce::AudioProcessorListener::ChangeDetails().withNonParameterStateChanged(true));
    return r;
}
juce::Result WubrotProcessor::exportPreset(const juce::String &filename, const juce::File &destination,
                                           bool overwrite)
{
    auto f = presetLibrary.resolve(filename);
    if (f == juce::File())
        return juce::Result::fail(
            "Only user presets can be exported. Save a factory sound as a user preset first.");
    juce::ValueTree state;
    juce::String name;
    auto r = PresetLibrary::read(f, state, name);
    return r.failed() ? r : PresetLibrary::write(destination, state, name, overwrite);
}
void WubrotProcessor::refreshPresetLibrary()
{
    juce::String error;
    const auto entries = presetLibrary.list(error);
    const juce::ScopedLock lock(metadataLock);
    userPresetEntries = entries;
    libraryError = error;
    libraryRevision.fetch_add(1);
}
juce::var WubrotProcessor::presetCatalogue()
{
    const juce::ScopedLock lock(metadataLock);
    auto *o = new juce::DynamicObject();
    o->setProperty("users", userPresetEntries);
    o->setProperty("error", libraryError);
    return o;
}
void WubrotProcessor::presetCommand(const juce::var &event)
{
    presetPending.fetch_add(1);
    presetBusy.store(true);
    presetJobs.addJob(
        [this, event]
        {
            const auto op = event["operation"].toString(), name = event["name"].toString(),
                       file = event["file"].toString();
            const bool overwrite = static_cast<bool>(event["overwrite"]);
            juce::Result result = juce::Result::ok();
            const bool needsUnsaved = (op == "load" || op == "factory") && hasUnsavedPresetChanges() &&
                                      !static_cast<bool>(event["discardConfirmed"]);
            if (needsUnsaved)
                result = juce::Result::fail("There are unsaved sound changes.");
            else if (op == "refresh")
                refreshPresetLibrary();
            else if (op == "save")
                result = saveUserPreset(name, overwrite);
            else if (op == "load")
                result = loadUserPreset(file);
            else if (op == "factory")
            {
                const int index = static_cast<int>(event["index"]);
                if (index < 0 || index > 2)
                    result = juce::Result::fail("Unknown factory preset.");
                else if (loading.load())
                    result = juce::Result::fail("DNA is busy. Try again when analysis finishes.");
                else
                {
                    juce::String error;
                    loadFactoryPreset(index, &error);
                    if (error.isNotEmpty())
                        result = juce::Result::fail(error);
                    for (int i = 0; i < 15000 && loading.load() && !stopping.load(); ++i)
                        juce::Thread::sleep(2);
                }
            }
            else if (op == "rename")
                result = renameUserPreset(file, name, overwrite);
            else if (op == "delete")
                result = deleteUserPreset(file);
            else if (op == "importFile")
                result = importPreset(juce::File(event["path"].toString()), name, overwrite);
            else if (op == "exportFile")
                result = exportPreset(file, juce::File(event["path"].toString()), overwrite);
            else if (op == "openFolder")
            {
                result = presetLibrary.ensureDirectory();
                if (result.wasOk())
                    juce::MessageManager::callAsync([folder = presetLibrary.directory()]
                                                    { folder.revealToUser(); });
            }
            else
                result = juce::Result::fail("Unknown preset operation.");
            auto *response = new juce::DynamicObject();
            response->setProperty("serial", ++presetResultSerial);
            response->setProperty("needsUnsaved", needsUnsaved);
            response->setProperty("request", event);
            response->setProperty("operation", op);
            response->setProperty("ok", result.wasOk());
            response->setProperty("conflict", result.getErrorMessage().containsIgnoreCase("already exists"));
            const juce::String success = op == "save"                      ? "Preset saved."
                                         : op == "load" || op == "factory" ? "Preset loaded."
                                         : op == "rename"                  ? "Preset renamed."
                                         : op == "delete"     ? "Preset deleted. Current sound preserved."
                                         : op == "importFile" ? "Preset imported into MY PRESETS."
                                         : op == "exportFile" ? "Preset exported."
                                         : op == "openFolder" ? "Opened your Documents preset folder."
                                                              : "";
            response->setProperty("message", result.wasOk() ? success : result.getErrorMessage());
            {
                const juce::ScopedLock lock(metadataLock);
                presetResult = response;
            }
            if (presetPending.fetch_sub(1) == 1)
                presetBusy.store(false);
        });
}
