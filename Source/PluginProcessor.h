#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "CrushDSP.h"
#include "DnaExchange.h"

class WubrotProcessor final : public juce::AudioProcessor, private juce::Timer
{
public:
    WubrotProcessor();
    ~WubrotProcessor() override;
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void reset() override { dsp.reset(); expectedSample = -1; }
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
    void loadSampleFile(const juce::File&);
    void clearSample();
    bool isLoading() const noexcept { return loading.load(); }
    juce::var uiState();
    std::atomic<int> editorSize { 1 };
    bool loadFactoryPreset(int);
    juce::AudioProcessorValueTreeState parameters;
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
    void timerCallback() override;
    void decodeSample(std::unique_ptr<juce::InputStream>, const juce::String&, std::uint64_t);
    void setStatus(const juce::String&);
    juce::File factoryRoot() const;
    CrushDSP dsp;
    DnaExchange profiles;
    std::array<std::atomic<float>*, 8> values;
    juce::CriticalSection metadataLock; // Never used by processBlock.
    juce::String sampleName, status { "Load a WAV into ROT DNA." };
    juce::ThreadPool sampleJobs { 1 };
    std::atomic<bool> loading { false };
    std::atomic<std::uint64_t> generation { 0 };
    std::atomic<int> presetRequest { 0 }, currentPreset { 0 };
    std::atomic<double> runningRate { 44100 };
    juce::int64 expectedSample = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WubrotProcessor)
};
