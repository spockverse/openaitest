#include "PluginProcessor.h"
#include <iostream>
#include <stdexcept>
static void require(bool ok, const char *text)
{
    if (!ok)
        throw std::runtime_error(text);
}
static void wait(WubrotProcessor &p)
{
    for (int i = 0; i < 15000 && p.isLoading(); ++i)
        juce::Thread::sleep(2);
    require(!p.isLoading(), "analysis timeout");
}
static juce::MemoryBlock fixture(int seconds = 1)
{
    juce::AudioBuffer<float> audio(1, 48000 * seconds);
    for (int i = 0; i < audio.getNumSamples(); ++i)
    {
        const float t = i / 48000.f;
        audio.setSample(0, i,
                        (i < 24000 ? .4f * std::sin(1256.637f * t)
                                   : (.4f * std::sin(10750.f * t) + .2f * std::sin(18428.f * t))) *
                            (i % 6000 < 1800 ? 1.f : .15f));
    }
    juce::MemoryBlock b;
    juce::WavAudioFormat f;
    {
        std::unique_ptr<juce::AudioFormatWriter> w(
            f.createWriterFor(new juce::MemoryOutputStream(b, false), 48000, 1, 16, {}, 0));
        require(w && w->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples()),
                "fixture creation failed");
    }
    return b;
}
static std::vector<float> render(WubrotProcessor &p, bool midi = false)
{
    p.prepareToPlay(48000, 128);
    std::vector<float> out;
    for (int block = 0; block < 150; ++block)
    {
        juce::AudioBuffer<float> b(2, 128);
        juce::MidiBuffer events;
        for (int i = 0; i < 128; ++i)
        {
            const float t = (block * 128 + i) / 48000.f, v = .3f * std::sin(345.575f * t);
            b.setSample(0, i, v);
            b.setSample(1, i, v);
        }
        if (midi && block % 30 == 0)
            events.addEvent(juce::MidiMessage::noteOn(1, 48, (juce::uint8)100), 0);
        p.processBlock(b, events);
        for (int i = 0; i < 128; ++i)
            out.push_back(b.getSample(0, i));
    }
    return out;
}
int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto docs = juce::File::createTempFile("-Documents");
    docs.deleteFile();
    try
    {
        PresetLibrary defaults;
        require(defaults.directory() == juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                            .getChildFile("Wubware Audio/WUBROT/Presets"),
                "Documents directory lookup changed");
        PresetLibrary library(docs);
        require(!library.directory().exists(), "library created before first use");
        for (const auto *n :
             {"../Escape", "C:\\bad", "CON", "NUL.txt", "COM1", "bad.", " leading", "trailing ", "a|b", ""})
            require(PresetLibrary::validName(n).failed(), "unsafe name accepted");
        require(PresetLibrary::validName("Metallic Yoi").wasOk(), "valid name rejected");
        {
            WubrotProcessor factory(docs.getChildFile("Factory Test Documents"));
            const float drives[]{12, 4, 20}, mutates[]{80, 55, 90}, xs[]{.3f, -.45f, .65f};
            for (int i = 0; i < 3; ++i)
            {
                factory.loadFactoryPreset(i);
                wait(factory);
                require(factory.getCurrentProgram() == i, "factory selection not retained");
                require(std::abs(factory.parameters.getRawParameterValue("drive")->load() - drives[i]) <
                                1e-5f &&
                            std::abs(factory.parameters.getRawParameterValue("mutate")->load() - mutates[i]) <
                                1e-5f &&
                            std::abs(factory.parameters.getRawParameterValue("x")->load() - xs[i]) < 1e-5f,
                        "factory settings changed");
                require(!factory.hasUnsavedPresetChanges(), "fresh factory preset is dirty");
            }
            factory.setParameterValue("drive", 22);
            require(factory.saveUserPreset("Modified Factory").wasOk(),
                    "modified factory cannot save as user preset");
        }
        WubrotProcessor p(docs);
        p.setParameterValue("x",.5f);p.setParameterValue("y",-.4f);
        p.loadSampleBytes(fixture(), "synthetic-regions.wav");
        wait(p);
        p.setParameterValue("mutate", 100);
        p.setParameterValue("drive", 21);
        p.setParameterValue("regionStart", .05f);
        p.setParameterValue("regionEnd", .45f);
        p.serviceRegionEdit();
        wait(p);
        const auto original = render(p);
        juce::MemoryBlock directAudioProject;
        p.getStateInformation(directAudioProject);
        require(p.hasUnsavedPresetChanges(), "sound edit not dirty");
        require(p.saveUserPreset("Metallic Yoi").wasOk(), "save failed");
        require(library.directory().isDirectory(), "Documents subfolders not created");
        require(!p.hasUnsavedPresetChanges(), "successful save stayed dirty");
        auto file = library.fileFor("Metallic Yoi");
        const auto text = file.loadFileAsString();
        require(file.existsAsFile() && file.getSize() < 512 * 1024, "preset missing or unexpectedly large");
        require(!text.contains("sourceWav") && text.contains("dnaArchive"),
                "portable preset contains original audio or lacks full analysis");
        juce::ValueTree state;
        juce::String name;
        auto result = PresetLibrary::read(file, state, name);
        if (result.failed())
            std::cerr << result.getErrorMessage() << '\n';
        require(result.wasOk(), "saved file did not validate");
        require(name == "Metallic Yoi", "saved name wrong");
        require(p.saveUserPreset("metallic yoi").failed(), "case-insensitive duplicate silently overwritten");
        require(file.loadFileAsString() == text, "failed save changed existing file");
        p.setParameterValue("drive", 22);
        require(p.hasUnsavedPresetChanges(), "drive change not dirty");
        p.setParameterValue("drive", 21);
        require(!p.hasUnsavedPresetChanges(), "returning parameter to baseline stayed dirty");
        p.prepareToPlay(48000, 128);
        require(!p.hasUnsavedPresetChanges(), "playhead/reset dirtied preset");
        WubrotProcessor recalled(docs);
        require(recalled.loadUserPreset("Metallic Yoi.wubpreset").wasOk(),
                "fresh instance cannot load saved preset");
        require(recalled.uiState()["analysisOnly"], "preset did not disclose analysis-only editing");
        require(recalled.uiState()["editable"], "preset lost region controls");
        require(render(recalled) == original, "analysis-only recall changed exact saved audio");
        require(!recalled.hasUnsavedPresetChanges(), "loaded preset stayed dirty");
        recalled.setParameterValue("regionStart", .6f);
        recalled.setParameterValue("regionEnd", 1);
        recalled.serviceRegionEdit();
        wait(recalled);
        require(render(recalled) != original, "analysis-only regions do not affect audio");
        require(recalled.hasUnsavedPresetChanges(), "region edit not dirty");
        recalled.setParameterValue("regionStart", .05f);
        recalled.setParameterValue("regionEnd", .45f);
        recalled.serviceRegionEdit();
        wait(recalled);
        require(render(recalled) == original, "original preset region did not restore its exact profile");
        require(!recalled.hasUnsavedPresetChanges(), "restored saved region stayed dirty");
        recalled.setParameterValue("dnaSpeed", 4);
        require(render(recalled) != original, "speed lost influence");
        recalled.setParameterValue("dnaSpeed", 2);
        recalled.setParameterValue("mode", 1);
        require(render(recalled, true) != render(recalled, false), "retrigger stopped working");
        require(recalled.saveUserPreset("Digital Rot").wasOk(), "Save As failed");
        require(file.existsAsFile(), "Save As removed original");
        auto exported = docs.getChildFile("exported.wubpreset");
        require(recalled.exportPreset("Digital Rot.wubpreset", exported, false).wasOk(), "export failed");
        require(recalled.importPreset(exported, "Imported Rot", false).wasOk(), "import failed");
        require(recalled.importPreset(exported, "Imported Rot", false).failed(),
                "import duplicate overwrote silently");
        require(recalled.renameUserPreset("Imported Rot.wubpreset", "Renamed Rot").wasOk(), "rename failed");
        require(!library.fileFor("Imported Rot").exists() && library.fileFor("Renamed Rot").existsAsFile(),
                "rename did not move library file");
        juce::MemoryBlock project;
        recalled.getStateInformation(project);
        const auto projectAudio = render(recalled, true);
        require(recalled.deleteUserPreset("Digital Rot.wubpreset").wasOk(), "delete failed");
        WubrotProcessor projectRecall(docs);
        projectRecall.setStateInformation(project.getData(), static_cast<int>(project.getSize()));
        require(render(projectRecall, true) == projectAudio,
                "project recall depended on deleted preset file");
        require(projectRecall.uiState()["analysisOnly"], "project lost analysis editor");
        require(p.uiState()["presets"]["name"].toString() == "Metallic Yoi",
                "instances share selected preset");
        auto corrupt = docs.getChildFile("bad.wubpreset");
        corrupt.replaceWithText("<broken>");
        const auto before = render(p);
        require(p.importPreset(corrupt, "Bad", false).failed(), "malformed preset accepted");
        require(render(p) == before, "rejected import modified audio");
        auto version = text.replace("formatVersion=\"9\"", "formatVersion=\"999\"");
        corrupt.replaceWithText(version);
        require(PresetLibrary::read(corrupt, state, name).failed(), "future version accepted");
        require(PresetLibrary::read(file, state, name).wasOk(), "valid file not readable");
        state.getChildWithProperty("id", "drive").setProperty("value", 999, nullptr);
        require(PresetLibrary::write(file, state, "Metallic Yoi", true).failed(), "invalid parameter saved");
        require(file.loadFileAsString() == text, "invalid write destroyed existing preset");
        auto blocked = docs.getChildFile("not-a-directory");
        blocked.replaceWithText("blocker");
        PresetLibrary unavailable(blocked);
        require(unavailable.ensureDirectory().failed(), "unavailable Documents silently redirected");
        juce::String error;
        const auto entries = library.list(error);
        require(error.isEmpty() && entries.size() == 2, "library refresh returned wrong entries");
        require(!p.loadUserPreset("../../Escape.wubpreset").wasOk(), "path traversal accepted");
        require(recalled.renameUserPreset("Renamed Rot.wubpreset", "renamed rot").wasOk(),
                "case-only rename failed");
        require(library.directory().getChildFile("renamed rot.wubpreset").existsAsFile(),
                "case-only name not applied");
        p.setParameterValue("drive", 23);
        p.presetCommand(juce::JSON::parse("{\"operation\":\"load\",\"file\":\"Metallic Yoi.wubpreset\"}"));
        for (int i = 0; i < 1000 && p.isPresetBusy(); ++i)
            juce::Thread::sleep(2);
        require(p.uiState()["presets"]["result"]["needsUnsaved"],
                "backend discarded edits from stale UI state");
        require(p.parameters.getRawParameterValue("drive")->load() > 22,
                "unsaved guard applied preset anyway");
        p.clearSample();
        require(p.saveUserPreset("Cleared region").wasOk(),
                "cleared DNA with nondefault region could not save");

        WubrotProcessor empty(docs);
        empty.clearSample();
        require(empty.saveUserPreset("No DNA").wasOk(), "no-DNA preset save failed");
        juce::MemoryBlock noDnaState;
        empty.getStateInformation(noDnaState);
        WubrotProcessor emptyRestore(docs);
        emptyRestore.setStateInformation(noDnaState.getData(), static_cast<int>(noDnaState.getSize()));
        require(!emptyRestore.hasUnsavedPresetChanges(), "no-DNA project falsely dirty");
        // A canceled long analysis must not clear the loading flag or replace DNA for a newer project.
        auto large = fixture(30);
        WubrotProcessor rapid(docs);
        for (int i = 0; i < 4; ++i)
        {
            rapid.loadSampleBytes(large, "canceled-synthetic.wav");
            rapid.setStateInformation(directAudioProject.getData(),
                                      static_cast<int>(directAudioProject.getSize()));
            wait(rapid);
            require(rapid.uiState()["editable"],
                    "stale job cleared loading before current source restoration");
            require(rapid.uiState()["name"].toString() == "synthetic-regions.wav",
                    "stale source replaced recalled project");
            require(render(rapid) == original, "rapid project restore changed sound");
        }
        require(PresetLibrary::read(file, state, name).wasOk(), "reload validation fixture failed");
        auto missingArchive = state.createCopy();
        missingArchive.removeProperty("dnaArchive", nullptr);
        require(PresetLibrary::validate(missingArchive).failed(), "incomplete temporal analysis accepted");
        auto badTimeline = state.createCopy();
        badTimeline.setProperty("dnaArchive", "{broken", nullptr);
        require(PresetLibrary::validate(badTimeline).failed(), "corrupt timeline accepted");
        auto badNumber = state.createCopy();
        badNumber.getChildWithProperty("id", "x").setProperty("value", "0x1", nullptr);
        require(PresetLibrary::validate(badNumber).failed(), "nondecimal parameter text accepted");
#if JUCE_WINDOWS
        require(file.setReadOnly(true), "could not set readonly fixture");
        require(PresetLibrary::write(file, state, "Metallic Yoi", true).failed(),
                "read-only preset overwritten");
        require(file.loadFileAsString() == text, "read-only failure changed existing file");
        file.setReadOnly(false);
#endif
        const juce::StringArray expectedIds{"x",       "y",           "mutate",    "drive",
                                            "mode",    "mix",         "output",    "protect",
                                            "enabled", "regionStart", "regionEnd", "dnaSpeed"};
        require(emptyRestore.getParameters().size() == 12, "parameter count changed");
        for (int i = 0; i < 12; ++i)
        {
            auto *parameter = dynamic_cast<juce::RangedAudioParameter *>(emptyRestore.getParameters()[i]);
            require(parameter && parameter->getParameterID() == expectedIds[i] && parameter->isAutomatable(),
                    "parameter identity/order/automation changed");
        }
        require(emptyRestore.getNumPrograms() == 3 &&
                    emptyRestore.parameters.getParameter("dnaSpeed")->getParameterID() == "dnaSpeed",
                "host programs or parameter identity changed");
        std::cout << "PASS: Documents lookup, names/traversal, CRUD, conflicts, atomic validation, "
                     "import/export, compact analysis-only exact recall, editable regions/speed/retrigger, "
                     "independent instances/projects, dirty checks and malformed-file rejection\n";
        docs.deleteRecursively();
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL: " << e.what() << '\n';
        docs.deleteRecursively();
        return 1;
    }
}
