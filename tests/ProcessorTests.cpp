#include "PluginProcessor.h"
#include "EditorGeometry.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>

static void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static void waitForSample(WubrotProcessor& p)
{
    for (int i = 0; i < 2000 && p.isLoading(); ++i) juce::Thread::sleep(2);
    require(!p.isLoading(), "sample worker timed out");
}
static juce::MemoryBlock wav(bool silence = false)
{
    juce::AudioBuffer<float> buffer(2, 24000);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float envelope = i % 6000 < 900 ? .8f : .08f;
        const float value = silence ? 0 : envelope * std::sin(static_cast<float>(i) * .13f);
        buffer.setSample(0, i, value); buffer.setSample(1, i, -value);
    }
    juce::MemoryBlock bytes; juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(new juce::MemoryOutputStream(bytes, false), 48000, 2, 16, {}, 0));
    require(writer != nullptr && writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples()), "fixture WAV encoding failed");
    writer.reset(); return bytes;
}
static std::vector<float> render(WubrotProcessor& p, bool midi = false)
{
    p.prepareToPlay(48000, 128); std::vector<float> result;
    for (int block = 0; block < 120; ++block)
    {
        juce::AudioBuffer<float> buffer(2, 128); juce::MidiBuffer events;
        if (midi && block % 30 == 0) events.addEvent(juce::MidiMessage::noteOn(1, 48, (juce::uint8) 100), 37);
        for (int i = 0; i < 128; ++i)
        {
            const float value = .35f * std::sin(static_cast<float>(block * 128 + i) * .061f);
            buffer.setSample(0, i, value); buffer.setSample(1, i, value);
        }
        p.processBlock(buffer, events);
        for (int i = 0; i < 128; ++i)
        {
            require(std::isfinite(buffer.getSample(0, i)), "processor produced nonfinite audio");
            require(std::abs(buffer.getSample(0, i) - buffer.getSample(1, i)) < 1.0e-9f, "stereo processing diverged");
            result.push_back(buffer.getSample(0, i));
        }
    }
    return result;
}
int main()
{
    juce::ScopedJuceInitialiser_GUI initialise;
    try
    {
        WubrotProcessor p;
       #if JUCE_WINDOWS
        for (int size = 0; size < 3; ++size)
        {
            p.editorSize.store(size);
            for (int reopen = 0; reopen < 2; ++reopen)
            {
                std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditorIfNeeded());
                require(editor != nullptr, "native editor creation failed");
                require(editor->getWidth() == WubrotGeometry::width(size) && editor->getHeight() == WubrotGeometry::height(size), "native editor size/reopening mismatch");
                require(!editor->isResizable(), "arbitrary editor resizing must be disabled");
            }
        }
        p.editorSize.store(1);
       #endif
        p.loadSampleBytes(wav(), "transient-pattern.wav"); waitForSample(p);
        require(p.uiState()["waveform"].size() == 128, "WAV analysis was not published");
        p.setParameterValue("mutate", 100); p.setParameterValue("protect", 0);
        const auto withDna = render(p);
        p.editorSize.store(2);
        juce::MemoryBlock state; p.getStateInformation(state);
        WubrotProcessor restored; restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        require(restored.editorSize.load() == 2, "editor size did not recall");
        require(restored.uiState()["name"].toString() == "transient-pattern.wav", "sample name did not recall");
        require(render(restored) == withDna, "project recall changed the sound or depends on an external WAV");
        // New spectral DNA is optional; older saved projects contain envelope only.
        auto legacyXml = juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
        require(legacyXml != nullptr, "state XML missing");
        legacyXml->removeAttribute("dnaCharacter");
        if (auto* power=legacyXml->getChildByAttribute("id","enabled")) legacyXml->removeChildElement(power,true);
        juce::MemoryBlock legacyState; juce::AudioProcessor::copyXmlToBinary(*legacyXml, legacyState);
        WubrotProcessor legacy; legacy.setParameterValue("enabled",0); legacy.setStateInformation(legacyState.getData(), static_cast<int>(legacyState.getSize()));
        require(legacy.parameters.getRawParameterValue("enabled")->load()>.5f,"old projects must default ON even after loading a bypassed project");
        require(legacy.uiState()["waveform"].size() == 128, "legacy envelope-only DNA was lost");
        require(!render(legacy).empty(), "legacy project cannot render");
        require(p.parameters.getParameter("enabled")->isAutomatable(),"power must be automatable");
        p.setParameterValue("enabled",0);p.setParameterValue("output",6);
        const auto dryBypass=render(p);
        for(std::size_t i=0;i<dryBypass.size();++i) { const float expected=.35f*std::sin(static_cast<float>(i)*.061f);require(std::memcmp(&dryBypass[i],&expected,sizeof(float))==0,"processor bypass changed dry input"); }
        juce::MemoryBlock offState;p.getStateInformation(offState);restored.setStateInformation(offState.getData(),static_cast<int>(offState.getSize()));
        require(restored.parameters.getRawParameterValue("enabled")->load()<.5f,"bypass state did not recall");
        require(restored.uiState()["name"].toString()=="transient-pattern.wav","bypass lost DNA");
        p.setParameterValue("enabled",1);p.setParameterValue("output",-6);
        p.loadSampleBytes(juce::MemoryBlock("not a WAV", 9), "invalid.wav"); waitForSample(p);
        require(p.uiState()["name"].toString() == "transient-pattern.wav", "failed loading discarded working DNA");
        p.loadSampleBytes(wav(true), "silent.wav"); waitForSample(p);
        require(p.uiState()["name"].toString() == "transient-pattern.wav", "silent loading discarded working DNA");
        p.setParameterValue("mode", 1);
        const auto retrigger = render(p, true);
        require(retrigger != withDna, "RETRIGGER did not change the modulation");
        require(retrigger != render(p, false), "sample-positioned MIDI notes did not restart the DNA pattern");
        p.clearSample();
        juce::MemoryBlock cleared; p.getStateInformation(cleared);
        restored.setStateInformation(cleared.getData(), static_cast<int>(cleared.getSize()));
        require(restored.uiState()["waveform"].size() == 0, "cleared sample returned on project reload");
        const auto withoutDna = render(p);
        require(withoutDna != withDna, "ROT DNA did not change actual processed audio");
        WubrotProcessor axis;
        axis.clearSample(); axis.setParameterValue("mutate", 0); axis.setParameterValue("drive", 0);
        axis.setParameterValue("protect", 0); axis.setParameterValue("output", 0);
        axis.setParameterValue("y", -1); const auto top = render(axis);
        axis.setParameterValue("y", 1); const auto bottom = render(axis);
        auto steps = [](const std::vector<float>& audio) { int count = 0; for (std::size_t i = 1; i < audio.size(); ++i) if (std::abs(audio[i] - audio[i - 1]) > .01f) ++count; return count; };
        require(steps(bottom) > steps(top) * 10, "XY up must reduce the sample rate, not increase it");
        p.setCurrentProgram(2); require(p.getCurrentProgram() == 2, "host preset selection must acknowledge immediately"); juce::Thread::sleep(75); juce::Timer::callPendingTimersSynchronously();
        require(p.getNumPrograms() == 3, "factory preset count");
        const auto folder = juce::File::createTempFile("-wubrot-content"); folder.deleteFile(); folder.createDirectory();
        const auto factory = folder.getChildFile("Presets/Factory/01 Riddim Monster"); factory.createDirectory();
        factory.getChildFile("Samples").createDirectory();
        auto sample = wav(); factory.getChildFile("Samples/dna.wav").replaceWithData(sample.getData(), sample.getSize());
        factory.getChildFile("preset.json").replaceWithText(R"({"sample":"Samples/dna.wav","parameters":{"x":0.4,"y":0.1,"mutate":88,"drive":12,"mode":0}})");
       #if JUCE_WINDOWS
        _putenv_s("WUBROT_CONTENT_DIR", folder.getFullPathName().toRawUTF8());
       #else
        setenv("WUBROT_CONTENT_DIR", folder.getFullPathName().toRawUTF8(), 1);
       #endif
        require(p.loadFactoryPreset(0), "factory sample could not load from nested preset folder"); waitForSample(p);
        require(p.uiState()["name"].toString() == "dna.wav", "factory preset did not load its sample");
        require(std::abs(p.parameters.getRawParameterValue("mutate")->load() - 88) < .01f, "factory parameters not applied");
        folder.deleteRecursively();
        std::cout << "PASS: real WAV decoding, audio modulation, project recall, invalid/silent file recovery, MIDI retrigger, clear-state recall, nested factory preset sample\n";
        return 0;
    }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
