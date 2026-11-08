#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "CrushDSP.h"
#include "DnaExchange.h"
#include "AudioReactivity.h"
#include "PresetLibrary.h"

class WubrotProcessor final : public juce::AudioProcessor, private juce::Timer
{
public:
    explicit WubrotProcessor(juce::File documentsForTests = {});
    ~WubrotProcessor() override;
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void reset() override { dsp.reset(); visualAnalyser.reset(); visualReadout.clear(); expectedSample = lastReportedSample = -1;lastTransportPlaying=false;patternPosition.store(0); }
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    using AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "WUBROT"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return .1; }
    int getNumPrograms() override { return 3; }
    int getCurrentProgram() override { return currentPreset.load(); }
    void setCurrentProgram(int index) override { if (index >= 0 && index < 3) { currentPreset.store(index); presetRequest.store(index); } }
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    void setParameterValue(const juce::String&, float);
    void parameterGesture(const juce::String&, bool);
    void loadSampleBytes(juce::MemoryBlock, juce::String);
    void loadSampleFile(const juce::File&, bool factoryPreset = false);
    void clearSample();
    void reportSampleError(const juce::String& message) { setStatus(message); }
    void serviceRegionEdit();
    bool isLoading() const noexcept { return loading.load(); }
    juce::var uiState(bool includeWaveform=true);
    double modulationPosition() const noexcept { return patternPosition.load(); }
    std::uint32_t audioVisualSequence() const noexcept { return visualReadout.sequence(); }
    float consumeAudioVisualLevel() noexcept { const auto v=visualReadout.consume(); return values[8]->load() >= .5f ? v : 0; }
    std::atomic<int> editorSize { 1 };
    bool loadFactoryPreset(int,juce::String* error=nullptr);
    juce::Result saveUserPreset(const juce::String&,bool overwrite=false);
    juce::Result loadUserPreset(const juce::String& filename);
    juce::Result renameUserPreset(const juce::String& filename,const juce::String& name,bool overwrite=false);
    juce::Result deleteUserPreset(const juce::String& filename);
    juce::Result importPreset(const juce::File&,const juce::String& name,bool overwrite=false);
    juce::Result exportPreset(const juce::String& filename,const juce::File&,bool overwrite);
    void presetCommand(const juce::var&);
    void refreshPresetLibrary();
    juce::File userPresetDirectory()const { return presetLibrary.directory(); }
    bool hasUnsavedPresetChanges();
    juce::var presetCatalogue();
    int presetLibraryRevision()const noexcept { return libraryRevision.load(); }
    bool isPresetBusy()const noexcept { return presetBusy.load(); }
    juce::AudioProcessorValueTreeState parameters;
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
    juce::ValueTree captureState(bool includeSourceAudio);
    juce::String soundSignature();
    void markPresetClean();
    juce::Result capturePortableState(juce::ValueTree&,juce::String& signature,std::uint64_t& capturedGeneration);
    void updateDnaKey(const DnaProfile&);
    PresetLibrary presetLibrary;
    juce::ThreadPool presetJobs {1};
    std::atomic<bool> presetBusy {false}, stopping {false}, resetProfileClock {false};
    juce::String activePresetName {"INIT"}, activeUserFile, presetBaseline, dnaKey, dnaOriginKey;
    juce::String libraryError;
    juce::Array<juce::var> userPresetEntries;
    juce::var presetResult;
    int presetResultSerial=0;
    std::atomic<int> libraryRevision {0}, presetPending {0};
    DnaProfile recalledRegion;float recalledStart=0,recalledEnd=1;
    std::shared_ptr<const WubrotPresetData::Archive> analysisArchive;

    void timerCallback() override;
    void decodeSample(std::unique_ptr<juce::InputStream>, const juce::String&, std::uint64_t, bool restoreOnly = false);
    void setStatus(const juce::String&);
    void setSampleStatus(std::uint64_t,const juce::String&);
    void finishSampleJob(std::uint64_t,bool factoryPreset=false);
    juce::File factoryRoot() const;
    struct SampleSource { juce::AudioBuffer<float> audio; juce::MemoryBlock wav; double rate=48000; DnaProfile full; };
    static DnaProfile analyseRegion(const SampleSource&, float, float);
    std::shared_ptr<const SampleSource> source;
    juce::MemoryBlock storedWav;
    std::array<float,DnaProfile::bins> fullEnvelope {};
    float analysedStart=0,analysedEnd=1;
    std::atomic<double> patternPosition {0};
    std::atomic<int> xyMapping {8};
    CrushDSP dsp;
    AudioReactivity visualAnalyser;
    AudioReactivityMailbox visualReadout;
    double lastVisualBlockMs=-1, lastVisualBlockDurationMs=0;
    DnaExchange profiles;
    std::array<std::atomic<float>*, 12> values;
    juce::CriticalSection metadataLock; // Never used by processBlock.
    juce::String sampleName, status { "Load a WAV into ROT DNA." };
    juce::ThreadPool sampleJobs { 1 };
    std::atomic<bool> loading { false }, regionJob { false };
    std::atomic<std::uint64_t> generation { 0 };
    std::atomic<int> presetRequest { -1 }, currentPreset { 0 };
    std::atomic<double> runningRate { 44100 };
    bool lastTransportPlaying=false;
    juce::int64 lastReportedSample=-1, expectedSample = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WubrotProcessor)
};
