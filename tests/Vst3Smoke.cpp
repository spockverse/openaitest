// Load the actual Linux VST3 bundle through Steinberg interfaces, not the processor class.
#include <pluginterfaces/base/ipluginbase.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivsthostapplication.h>
#include <pluginterfaces/vst/vstspeaker.h>
#include <dlfcn.h>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace Steinberg;
using namespace Steinberg::Vst;
static void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
struct Host final : IHostApplication
{
    tresult PLUGIN_API queryInterface(const TUID id, void** object) override
    {
        if (std::memcmp(id, IHostApplication_iid, 16) == 0 || std::memcmp(id, FUnknown_iid, 16) == 0)
        { *object = static_cast<IHostApplication*>(this); return kResultOk; }
        *object = nullptr; return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override { return 1; }
    uint32 PLUGIN_API release() override { return 1; }
    tresult PLUGIN_API getName(String128 name) override
    { const char* text = "WUBROT Smoke Host"; for (int i = 0; i < 128; ++i) { name[i] = static_cast<TChar>(*text); if (*text) ++text; } return kResultOk; }
    tresult PLUGIN_API createInstance(TUID, TUID, void** object) override { *object = nullptr; return kNoInterface; }
};
int main(int argc, char** argv)
{
    try
    {
        require(argc == 2, "provide VST3 shared-library path");
        void* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL); if (!library) throw std::runtime_error(dlerror());
        const auto enter = reinterpret_cast<bool (*)(void*)>(dlsym(library, "ModuleEntry"));
        const auto exit = reinterpret_cast<bool (*)()>(dlsym(library, "ModuleExit"));
        const auto getFactory = reinterpret_cast<IPluginFactory* (*)()>(dlsym(library, "GetPluginFactory"));
        require(enter && exit && getFactory && enter(library), "VST3 module entry failed");
        auto* factory = getFactory(); require(factory != nullptr, "no VST3 factory");
        PClassInfo info {}; IComponent* component = nullptr;
        for (int i = 0; i < factory->countClasses(); ++i)
            if (factory->getClassInfo(i, &info) == kResultOk && std::strcmp(info.category, kVstAudioEffectClass) == 0)
            { require(factory->createInstance(info.cid, IComponent_iid, reinterpret_cast<void**>(&component)) == kResultOk, "component creation failed"); break; }
        require(component != nullptr, "no audio effect class");
        Host host; require(component->initialize(&host) == kResultOk, "component initialization failed");
        IAudioProcessor* processor = nullptr;
        require(component->queryInterface(IAudioProcessor_iid, reinterpret_cast<void**>(&processor)) == kResultOk, "no audio processor interface");
        SpeakerArrangement stereo = SpeakerArr::kStereo;
        require(processor->setBusArrangements(&stereo, 1, &stereo, 1) == kResultOk, "stereo layout rejected");
        ProcessSetup setup { kRealtime, kSample32, 128, 48000 };
        require(processor->setupProcessing(setup) == kResultOk, "processing setup failed");
        component->activateBus(kAudio, kInput, 0, true); component->activateBus(kAudio, kOutput, 0, true);
        require(component->setActive(true) == kResultOk, "activation failed"); processor->setProcessing(true);
        float input[2][128] {}, output[2][128] {};
        float* inChannels[] { input[0], input[1] }; float* outChannels[] { output[0], output[1] };
        AudioBusBuffers in {}, out {}; in.numChannels = out.numChannels = 2; in.channelBuffers32 = inChannels; out.channelBuffers32 = outChannels;
        ProcessData data {}; data.processMode = kRealtime; data.symbolicSampleSize = kSample32; data.numSamples = 128;
        data.numInputs = data.numOutputs = 1; data.inputs = &in; data.outputs = &out;
        double difference = 0;
        for (int block = 0; block < 300; ++block)
        {
            for (int i = 0; i < 128; ++i) input[0][i] = input[1][i] = .4f * std::sin((block * 128 + i) * .061f);
            require(processor->process(data) == kResultOk, "VST3 process call failed");
            for (int i = 0; i < 128; ++i)
            {
                require(std::isfinite(output[0][i]) && output[0][i] == output[1][i], "invalid or unlinked stereo output");
                difference += std::abs(output[0][i] - input[0][i]);
            }
        }
        require(difference > 100, "actual VST3 did not process the bass");
        processor->setProcessing(false); component->setActive(false); processor->release(); component->terminate(); component->release(); factory->release(); exit(); dlclose(library);
        std::cout << "PASS: real VST3 discovery, initialization, stereo buses, 38400 processed frames and shutdown\n";
    }
    catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
