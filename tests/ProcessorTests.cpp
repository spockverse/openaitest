#include "PluginProcessor.h"
#include "PluginEditor.h"
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
static juce::MemoryBlock wav(bool silence = false, float frequency = .13f)
{
    juce::AudioBuffer<float> buffer(2, 24000);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float envelope = i % 6000 < 900 ? .8f : .08f;
        const float value = silence ? 0 : envelope * std::sin(static_cast<float>(i) * frequency);
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
static void checkDnaRouting()
{
    WubrotProcessor p; p.setParameterValue("mutate",0);
    p.loadSampleBytes(wav(false,.015f),"tonal-fixture.wav");waitForSample(p);
    juce::MemoryBlock stateA;p.getStateInformation(stateA);
    auto xmlA=juce::AudioProcessor::getXmlFromBinary(stateA.getData(),static_cast<int>(stateA.getSize()));
    require(xmlA&&juce::JSON::parse(xmlA->getStringAttribute("dnaSpectrum")).size()==20,"spectral profile not embedded after actual WAV loading");
    const auto zeroA=render(p);p.setParameterValue("mutate",100);const auto fullA=render(p);
    p.loadSampleBytes(wav(false,.4f),"bright-fixture.wav");waitForSample(p);
    juce::MemoryBlock stateB;p.getStateInformation(stateB);
    auto xmlB=juce::AudioProcessor::getXmlFromBinary(stateB.getData(),static_cast<int>(stateB.getSize()));
    require(xmlB&&xmlB->getStringAttribute("dnaSpectrum")!=xmlA->getStringAttribute("dnaSpectrum"),"different WAVs published identical spectral parameters");
    const auto fullB=render(p);p.setParameterValue("mutate",0);const auto zeroB=render(p);
    require(zeroA==zeroB,"sample identity leaked through MUTATE zero in processor");
    require(fullA!=fullB,"published DNA never reached actual processor output");
    double delta=0,energy=0;for(std::size_t i=0;i<fullA.size();++i){delta+=(fullA[i]-fullB[i])*(fullA[i]-fullB[i]);energy+=fullA[i]*fullA[i];}
    std::cout<<"Real WAV worker -> profile exchange -> DSP: MUTATE0 identical; MUTATE100 normalized waveform difference "<<std::sqrt(delta/(energy+1e-20))<<'\n';
}
static void checkRegionEditor()
{
    juce::AudioBuffer<float> audio(1,48000);
    for(int i=0;i<48000;++i){const float t=i/48000.0f;audio.setSample(0,i,i<24000?.4f*std::sin(1256.637f*t):(.4f*std::sin(10750.0f*t)+.25f*std::sin(18428.0f*t))*(i%6000<1500?1.0f:.1f));}
    juce::MemoryBlock bytes;juce::WavAudioFormat format;
    {std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(new juce::MemoryOutputStream(bytes,false),48000,1,16,{},0));require(writer&&writer->writeFromAudioSampleBuffer(audio,0,48000),"region fixture failed");}
    WubrotProcessor p;p.loadSampleBytes(bytes,"two-regions.wav");waitForSample(p);p.setParameterValue("mutate",100);p.setParameterValue("drive",20);
    auto select=[&](float start,float end){p.setParameterValue("regionStart",start);p.setParameterValue("regionEnd",end);p.serviceRegionEdit();waitForSample(p);};
    select(0,.45f);const auto first=render(p);juce::MemoryBlock firstState;p.getStateInformation(firstState);
    select(.55f,1);const auto second=render(p);require(first!=second,"selected WAV region did not affect audio");
    juce::MemoryBlock secondState;p.getStateInformation(secondState);
    auto a=juce::AudioProcessor::getXmlFromBinary(firstState.getData(),static_cast<int>(firstState.getSize())),b=juce::AudioProcessor::getXmlFromBinary(secondState.getData(),static_cast<int>(secondState.getSize()));
    require(a->getStringAttribute("dnaSpectrum")!=b->getStringAttribute("dnaSpectrum"),"selection did not reanalyse spectral content");
    p.setParameterValue("dnaSpeed",4);const auto fast=render(p);p.setParameterValue("dnaSpeed",0);const auto slow=render(p);require(fast!=slow,"speed did not alter modulation");
    p.setParameterValue("mutate",0);const auto zeroA=render(p);select(0,.45f);require(render(p)==zeroA,"region influenced MUTATE zero");
    p.setParameterValue("mutate",100);const auto recalled=render(p);juce::MemoryBlock state;p.getStateInformation(state);
    WubrotProcessor restored;restored.setStateInformation(state.getData(),static_cast<int>(state.getSize()));waitForSample(restored);
    require(static_cast<bool>(restored.uiState()["editable"]),"project did not retain editable source");require(render(restored)==recalled,"region/speed recall changed audio");
    restored.setParameterValue("regionStart",.55f);restored.setParameterValue("regionEnd",1);restored.serviceRegionEdit();waitForSample(restored);require(render(restored)!=recalled,"region edit after reopen needs external WAV");
}
static void checkHostClock()
{
    struct Clock:juce::AudioPlayHead { bool playing=false;juce::int64 samples=0;juce::Optional<PositionInfo> getPosition()const override{PositionInfo p;p.setIsPlaying(playing);p.setTimeInSamples(samples);return p;} } clock;
    WubrotProcessor p;p.loadSampleBytes(wav(),"clock.wav");waitForSample(p);p.prepareToPlay(48000,128);p.setPlayHead(&clock);
    auto block=[&](bool audio){juce::AudioBuffer<float> buffer(2,128);buffer.clear();if(audio)for(int ch=0;ch<2;++ch)for(int i=0;i<128;++i)buffer.setSample(ch,i,.2f);juce::MidiBuffer midi;p.processBlock(buffer,midi);if(clock.playing)clock.samples+=128;return static_cast<double>(p.uiState()["playhead"]);};
    for(int i=0;i<10;++i)require(std::abs(block(true))<1e-15,"stopped DAW moved playhead");
    clock.playing=true;for(int i=0;i<10;++i)block(true);const auto moving=block(true);require(moving>0,"playing DAW did not move playhead");
    clock.playing=false;for(int i=0;i<10;++i)require(std::abs(block(true)-moving)<1e-15,"DAW stop did not freeze actual profile position");
    clock.playing=true;for(int i=0;i<10;++i)require(std::abs(block(false)-moving)<1e-15,"DAW silence did not freeze profile");
    require(block(true)>moving,"profile did not resume with input");clock.samples=48000;require(block(true)<.02,"seek did not relocate profile");
    const auto first=block(true);const auto stale=clock.samples;
    for(int i=0;i<10;++i){clock.samples=stale;block(true);}require(block(true)>first,"stale host timestamps repeatedly reset profile");
    p.setPlayHead(nullptr);
}
static void checkLoadingRoutes()
{
    WubrotProcessor p;
    for(int bits:{16,24,32})for(double rate:{44100.,48000.,88200.,96000.,192000.})
    {
        juce::AudioBuffer<float> signal(1,2048);for(int i=0;i<2048;++i)signal.setSample(0,i,.3f*std::sin(i*.1f));
        juce::MemoryBlock bytes;juce::WavAudioFormat format;
        {std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(new juce::MemoryOutputStream(bytes,false),rate,1,bits,{},0));require(writer&&writer->writeFromAudioSampleBuffer(signal,0,2048),"WAV encoding failed");}
        p.loadSampleBytes(bytes,"format-test.wav");waitForSample(p);const auto fromBytes=render(p);
        const auto file=juce::File::createTempFile(bits==24&&rate==48000?".tmp":".wav");require(file.replaceWithData(bytes.getData(),bytes.getSize()),"file fixture failed");
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditorIfNeeded());auto* target=dynamic_cast<juce::FileDragAndDropTarget*>(editor.get());require(target!=nullptr,"native file-drop target missing");
        target->filesDropped(juce::StringArray{file.getFullPathName()},0,0);waitForSample(p);require(render(p)==fromBytes,"same WAV differs between native path and byte upload");file.deleteFile();
    }
    const juce::File mp3(juce::String(WUBROT_TEST_SOURCE_DIR)+"/tests/fixtures/synthetic-tone.mp3");p.loadSampleFile(mp3);waitForSample(p);require(p.uiState()["name"].toString()==mp3.getFileName(),"MP3 decode failed");
    juce::MemoryBlock mp3State;p.getStateInformation(mp3State);WubrotProcessor restored;restored.setStateInformation(mp3State.getData(),static_cast<int>(mp3State.getSize()));waitForSample(restored);require(static_cast<bool>(restored.uiState()["editable"]),"MP3 source did not reopen for editing");require(render(restored)==render(p),"MP3 profile state recall changed audio");
    p.loadSampleFile(juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("missing-wubrot.wav"));require(p.uiState()["status"].toString().containsIgnoreCase("access"),"missing path has no diagnostic");
}
int main()
{
    juce::ScopedJuceInitialiser_GUI initialise;
    try
    {
        checkHostClock();
        checkLoadingRoutes();
        checkDnaRouting();
        checkRegionEditor();
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
        legacyXml->removeAttribute("dnaSpectrum");
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
