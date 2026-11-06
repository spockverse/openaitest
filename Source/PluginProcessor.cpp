#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
const char* presetFolders[] { "01 Riddim Monster", "02 Liquid Glass", "03 Metal Teeth" };
const char* presetNames[] { "RIDDIM MONSTER", "LIQUID GLASS", "METAL TEETH" };
constexpr std::size_t maxSampleBytes = 20 * 1024 * 1024;
}
WubrotProcessor::WubrotProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "WubrotState", makeParameters()),
      values { parameters.getRawParameterValue("x"), parameters.getRawParameterValue("y"),
               parameters.getRawParameterValue("mutate"), parameters.getRawParameterValue("drive"),
               parameters.getRawParameterValue("mode"), parameters.getRawParameterValue("mix"),
               parameters.getRawParameterValue("output"), parameters.getRawParameterValue("protect") }
{ startTimerHz(20); }
WubrotProcessor::~WubrotProcessor() { stopTimer(); generation.fetch_add(1); sampleJobs.removeAllJobs(true, -1); }
juce::AudioProcessorValueTreeState::ParameterLayout WubrotProcessor::makeParameters()
{
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    auto add = [&](const char* id, const char* name, float lo, float hi, float initial, const char* unit) {
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { id, 1 }, name,
            juce::NormalisableRange<float>(lo, hi), initial, juce::AudioParameterFloatAttributes().withLabel(unit)));
    };
    add("x", "Crush Field X", -1, 1, 0, ""); add("y", "Crush Field Y", -1, 1, 0, "");
    add("mutate", "Mutate", 0, 100, 67, "%"); add("drive", "Drive", 0, 36, 8.5f, "dB");
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { "mode", 1 }, "DNA Mode", juce::StringArray { "LOOP", "RETRIGGER" }, 0));
    add("mix", "Mix", 0, 100, 100, "%"); add("output", "Output", -24, 6, -6, "dB");
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "protect", 1 }, "Keep Sub", true));
    return p;
}
void WubrotProcessor::prepareToPlay(double rate, int) { runningRate = rate; dsp.prepare(rate); expectedSample = -1; }
bool WubrotProcessor::isBusesLayoutSupported(const BusesLayout& b) const
{
    const auto out = b.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && b.getMainInputChannelSet() == out;
}
void WubrotProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    float x = values[0]->load(), y = values[1]->load();
    const float length = std::hypot(x, y); if (length > 1) { x /= length; y /= length; }
    CrushDSP::Parameters p;
    p.bits = 16 - (x + 1) * 7;
    p.rate = static_cast<float>(500 * std::pow(runningRate / 500, (y + 1) * .5));
    p.mutate = values[2]->load() * .01f; p.drive = values[3]->load(); p.mode = static_cast<int>(values[4]->load());
    p.mix = values[5]->load() * .01f; p.output = values[6]->load(); p.protect = values[7]->load();
    if (auto* hostClock = getPlayHead())
        if (const auto position = hostClock->getPosition(); position && position->getIsPlaying())
            if (const auto samples = position->getTimeInSamples())
            {
                if (expectedSample >= 0 && std::abs(*samples - expectedSample) > 2) dsp.resetModulation();
                dsp.setTimeline(static_cast<double>(*samples) / runningRate);
                expectedSample = *samples + buffer.getNumSamples();
            }
    auto profile = profiles.read();
    auto event = midi.cbegin();
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        while (event != midi.cend() && (*event).samplePosition <= i)
        {
            const auto e = *event;
            if (p.mode == 1 && e.numBytes >= 3 && (e.data[0] & 0xf0) == 0x90 && e.data[2] != 0) dsp.noteOn();
            ++event;
        }
        float frame[2] {};
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) frame[ch] = buffer.getSample(ch, i);
        dsp.processFrame(frame, buffer.getNumChannels(), p, profile.get());
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) buffer.setSample(ch, i, frame[ch]);
    }
    midi.clear();
}
void WubrotProcessor::setParameterValue(const juce::String& id, float value)
{
    if (!std::isfinite(value)) return;
    if (auto* parameter = parameters.getParameter(id)) parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
void WubrotProcessor::parameterGesture(const juce::String& id, bool start)
{
    if (auto* p = parameters.getParameter(id)) { if (start) p->beginChangeGesture(); else p->endChangeGesture(); }
}
void WubrotProcessor::setStatus(const juce::String& message) { const juce::ScopedLock lock(metadataLock); status = message; }
void WubrotProcessor::loadSampleFile(const juce::File& file)
{
    if (loading.exchange(true)) { setStatus("A sample is loading. Please wait, then try again."); return; }
    presetRequest.store(-1);
    const auto token = generation.fetch_add(1) + 1;
    sampleJobs.addJob([this, file, token] {
        if (file.getSize() > static_cast<juce::int64>(maxSampleBytes)) setStatus("WAV must be 20 MB or smaller.");
        else decodeSample(file.createInputStream(), file.getFileName(), token);
        loading.store(false);
    });
}
void WubrotProcessor::loadSampleBytes(juce::MemoryBlock bytes, juce::String name)
{
    if (bytes.getSize() > maxSampleBytes) { setStatus("WAV must be 20 MB or smaller."); return; }
    if (loading.exchange(true)) { setStatus("A sample is loading. Please wait, then try again."); return; }
    presetRequest.store(-1);
    const auto token = generation.fetch_add(1) + 1;
    sampleJobs.addJob([this, data = std::move(bytes), name = std::move(name), token] {
        decodeSample(std::make_unique<juce::MemoryInputStream>(data, false), name, token); loading.store(false);
    });
}
void WubrotProcessor::decodeSample(std::unique_ptr<juce::InputStream> stream, const juce::String& name, std::uint64_t token)
{
    if (token != generation.load()) return;
    setStatus("Analysing sample envelope...");
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatReader> reader(stream ? format.createReaderFor(stream.release(), true) : nullptr);
    if (!reader || reader->sampleRate < 8000 || reader->sampleRate > 384000 || reader->numChannels > 8 || reader->lengthInSamples < 1)
    { setStatus("Cannot decode this WAV. Previous DNA is unchanged."); return; }
    const int frames = static_cast<int>(std::min(reader->lengthInSamples, static_cast<juce::int64>(reader->sampleRate * 30)));
    juce::AudioBuffer<float> audio(static_cast<int>(reader->numChannels), frames);
    if (!reader->read(audio.getArrayOfWritePointers(), audio.getNumChannels(), 0, frames)) { setStatus("WAV read failed. Previous DNA is unchanged."); return; }
    const auto profile = DnaProfile::analyse(audio.getArrayOfReadPointers(), audio.getNumChannels(), static_cast<std::size_t>(frames), reader->sampleRate);
    if (!profile.valid) { setStatus("This WAV is silent. Previous DNA is unchanged."); return; }
    const juce::ScopedLock lock(metadataLock);
    if (token != generation.load()) return;
    if (!profiles.publish(profile)) { status = "DNA is busy. Please load the WAV again."; return; }
    sampleName = name; status = "DNA loaded. Envelope shapes the crusher; sample audio is not played.";
}
void WubrotProcessor::clearSample()
{
    const juce::ScopedLock lock(metadataLock); generation.fetch_add(1); presetRequest.store(-1);
    profiles.publish({}); sampleName.clear(); status = "DNA cleared. Conventional bitcrusher.";
}
juce::File WubrotProcessor::factoryRoot() const
{
    const auto overridePath = juce::SystemStats::getEnvironmentVariable("WUBROT_CONTENT_DIR", {});
    if (overridePath.isNotEmpty()) return juce::File(overridePath).getChildFile("Presets/Factory");
    return juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory).getChildFile("Spock Audio/WUBROT/Presets/Factory");
}
const juce::String WubrotProcessor::getProgramName(int i) { return i >= 0 && i < 3 ? presetNames[i] : ""; }
bool WubrotProcessor::loadFactoryPreset(int index)
{
    if (index < 0 || index >= 3 || loading.load()) return false;
    const auto folder = factoryRoot().getChildFile(presetFolders[index]);
    const auto manifest = juce::JSON::parse(folder.getChildFile("preset.json"));
    if (!manifest.isObject()) { setStatus("Factory content is missing. Install the WUBROT content pack or load your WAV."); return false; }
    const auto data = manifest["parameters"];
    for (const auto* id : { "x", "y", "mutate", "drive", "mode", "mix", "output", "protect" })
        if (data.hasProperty(id)) { parameterGesture(id, true); setParameterValue(id, static_cast<float>(data[id])); parameterGesture(id, false); }
    currentPreset.store(index);
    const auto relative = manifest["sample"].toString();
    const auto file = folder.getChildFile(relative);
    if (relative.isEmpty() || !file.isAChildOf(folder) || !file.existsAsFile())
    { clearSample(); setStatus("Preset controls loaded; its factory WAV has not been supplied. Drop your own WAV into ROT DNA."); return false; }
    loadSampleFile(file); return true;
}
void WubrotProcessor::timerCallback()
{
    if (!loading.load()) { const int requested = presetRequest.exchange(-1); if (requested >= 0) loadFactoryPreset(requested); }
}
juce::var WubrotProcessor::uiState()
{
    const juce::ScopedLock lock(metadataLock);
    auto object = std::make_unique<juce::DynamicObject>();
    for (const auto* id : { "x", "y", "mutate", "drive", "mode", "mix", "output", "protect" })
        object->setProperty(id, parameters.getRawParameterValue(id)->load());
    object->setProperty("name", sampleName); object->setProperty("status", status);
    object->setProperty("loading", loading.load()); object->setProperty("preset", currentPreset.load());
    object->setProperty("sampleRate", runningRate.load());
    auto p = profiles.read(); juce::Array<juce::var> wave;
    if (const auto* profile = p.get(); profile && profile->valid)
        for (std::size_t i = 0; i < 128; ++i) wave.add(profile->envelope[i * DnaProfile::bins / 128]);
    object->setProperty("waveform", wave);
    return juce::var(object.release());
}
void WubrotProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    const juce::ScopedLock lock(metadataLock);
    auto state = parameters.copyState(); state.removeProperty("envelope", nullptr); state.removeProperty("duration", nullptr); state.setProperty("version", 1, nullptr);
    state.setProperty("sampleName", sampleName, nullptr); state.setProperty("preset", currentPreset.load(), nullptr);
    auto reader = profiles.read();
    if (const auto* profile = reader.get(); profile && profile->valid)
    {
        juce::Array<juce::var> data; for (auto value : profile->envelope) data.add(value);
        state.setProperty("envelope", juce::JSON::toString(juce::var(data), true), nullptr);
        state.setProperty("duration", profile->duration, nullptr);
    }
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destination);
}
void WubrotProcessor::setStateInformation(const void* data, int size)
{
    if (size < 1 || size > 1024 * 1024) return;
    const auto xml = getXmlFromBinary(data, size);
    if (!xml || !xml->hasTagName("WubrotState")) return;
    auto state = juce::ValueTree::fromXml(*xml);
    DnaProfile profile;
    const auto envelope = juce::JSON::parse(state["envelope"].toString());
    const double duration = state["duration"];
    if (auto* array = envelope.getArray(); array && array->size() == static_cast<int>(DnaProfile::bins) && std::isfinite(duration) && duration > 0 && duration <= 30)
    {
        profile.duration = duration; profile.valid = true;
        for (std::size_t i = 0; i < DnaProfile::bins; ++i)
        {
            const float value = static_cast<float>((*array)[static_cast<int>(i)]);
            if (!std::isfinite(value) || value < 0 || value > 1) { profile = {}; break; }
            profile.envelope[i] = value;
        }
    }
    const juce::ScopedLock lock(metadataLock); generation.fetch_add(1); presetRequest.store(-1);
    parameters.replaceState(state); profiles.publish(profile);
    sampleName = profile.valid ? state["sampleName"].toString() : juce::String();
    currentPreset.store(juce::jlimit(0, 2, static_cast<int>(state["preset"])));
    status = profile.valid ? "Project DNA restored (no external sample needed)." : "Project restored without DNA.";
}
juce::AudioProcessorEditor* WubrotProcessor::createEditor() { return new WubrotEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WubrotProcessor(); }
