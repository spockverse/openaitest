// Load the actual Windows/Linux VST3 bundle through Steinberg interfaces, not the processor class.
#include <pluginterfaces/base/ipluginbase.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <pluginterfaces/vst/ivstparameterchanges.h>
#include <pluginterfaces/vst/ivstmessage.h>
#include <pluginterfaces/vst/ivsthostapplication.h>
#include <pluginterfaces/vst/vstspeaker.h>
#if defined(_WIN32)
 #include <windows.h>
 static void* openLibrary(const char* path) { return reinterpret_cast<void*>(LoadLibraryA(path)); }
 static void* getSymbol(void* handle, const char* name) { return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(handle), name)); }
 static void closeLibrary(void* handle) { FreeLibrary(reinterpret_cast<HMODULE>(handle)); }
#else
 #include <dlfcn.h>
 static void* openLibrary(const char* path) { return dlopen(path, RTLD_NOW | RTLD_LOCAL); }
 static void* getSymbol(void* handle, const char* name) { return dlsym(handle, name); }
 static void closeLibrary(void* handle) { dlclose(handle); }
#endif
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
struct ParameterQueue final : IParamValueQueue
{
    ParamID id=0;ParamValue value=.95;
    tresult PLUGIN_API queryInterface(const TUID iid,void** object)override{if(std::memcmp(iid,IParamValueQueue_iid,16)==0||std::memcmp(iid,FUnknown_iid,16)==0){*object=this;return kResultOk;}*object=nullptr;return kNoInterface;}
    uint32 PLUGIN_API addRef()override{return 1;}uint32 PLUGIN_API release()override{return 1;}
    ParamID PLUGIN_API getParameterId()override{return id;}
    int32 PLUGIN_API getPointCount()override{return 1;}
    tresult PLUGIN_API getPoint(int32 index,int32& offset,ParamValue& result)override{offset=0;result=value;return index==0?kResultOk:kResultFalse;}
    tresult PLUGIN_API addPoint(int32,ParamValue,int32&)override{return kNotImplemented;}
};
struct ParameterChanges final : IParameterChanges
{
    ParameterQueue queue;
    tresult PLUGIN_API queryInterface(const TUID iid,void** object)override{if(std::memcmp(iid,IParameterChanges_iid,16)==0||std::memcmp(iid,FUnknown_iid,16)==0){*object=this;return kResultOk;}*object=nullptr;return kNoInterface;}
    uint32 PLUGIN_API addRef()override{return 1;}uint32 PLUGIN_API release()override{return 1;}
    int32 PLUGIN_API getParameterCount()override{return 1;}
    IParamValueQueue* PLUGIN_API getParameterData(int32 index)override{return index==0?&queue:nullptr;}
    IParamValueQueue* PLUGIN_API addParameterData(const ParamID&,int32&)override{return nullptr;}
};
static bool title(const TChar* value,const char* text){while(*text)if(*value++!=*text++)return false;return *value==0;}
int main(int argc, char** argv)
{
    try
    {
        require(argc == 2, "provide VST3 shared-library path");
        void* library = openLibrary(argv[1]); require(library != nullptr, "VST3 library could not be opened");
       #if defined(_WIN32)
        const auto enter = reinterpret_cast<bool (*)()>(getSymbol(library, "InitDll"));
        const auto exit = reinterpret_cast<bool (*)()>(getSymbol(library, "ExitDll"));
        require(enter && exit && enter(), "VST3 module entry failed");
       #else
        const auto enter = reinterpret_cast<bool (*)(void*)>(getSymbol(library, "ModuleEntry"));
        const auto exit = reinterpret_cast<bool (*)()>(getSymbol(library, "ModuleExit"));
        require(enter && exit && enter(library), "VST3 module entry failed");
       #endif
        const auto getFactory = reinterpret_cast<IPluginFactory* (*)()>(getSymbol(library, "GetPluginFactory"));
        require(getFactory != nullptr, "VST3 factory entry missing");
        auto* factory = getFactory(); require(factory != nullptr, "no VST3 factory");
        PClassInfo info {}; IComponent* component = nullptr;
        for (int i = 0; i < factory->countClasses(); ++i)
            if (factory->getClassInfo(i, &info) == kResultOk && std::strcmp(info.category, kVstAudioEffectClass) == 0)
            { require(factory->createInstance(info.cid, IComponent_iid, reinterpret_cast<void**>(&component)) == kResultOk, "component creation failed"); break; }
        require(component != nullptr, "no audio effect class");
        Host host; require(component->initialize(&host) == kResultOk, "component initialization failed");
        TUID controllerId{};require(component->getControllerClassId(controllerId)==kResultOk,"No controller class");
        IEditController* controller=nullptr;require(factory->createInstance(controllerId,IEditController_iid,reinterpret_cast<void**>(&controller))==kResultOk,"No edit controller");require(controller->initialize(&host)==kResultOk,"Controller initialization failed");
        IConnectionPoint* componentPoint=nullptr;IConnectionPoint* controllerPoint=nullptr;
        require(component->queryInterface(IConnectionPoint_iid,reinterpret_cast<void**>(&componentPoint))==kResultOk&&controller->queryInterface(IConnectionPoint_iid,reinterpret_cast<void**>(&controllerPoint))==kResultOk,"Connection points unavailable");
        require(controllerPoint->connect(componentPoint)==kResultOk&&componentPoint->connect(controllerPoint)==kResultOk,"Controller/component connection failed");
        ParameterChanges changes;bool foundX=false;int defaults=0;
        for(int i=0;i<controller->getParameterCount();++i){ParameterInfo p{};require(controller->getParameterInfo(i,p)==kResultOk,"Missing parameter info");

            if(title(p.title,"Crush Field X")){changes.queue.id=p.id;foundX=true;require(p.defaultNormalizedValue==.5,"Fresh X is off-centre");defaults++;}
            if(title(p.title,"Crush Field Y")){require(p.defaultNormalizedValue==.5,"Fresh Y is off-centre");defaults++;}
            if(title(p.title,"Mutate")){require(p.defaultNormalizedValue==1,"Fresh MUTATE not100");defaults++;}
            if(title(p.title,"Drive")){require(p.defaultNormalizedValue==0,"Fresh DRIVE not0");defaults++;}}
        require(foundX&&defaults==4,"Default parameters absent");
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
            data.inputParameterChanges=block==150?&changes:nullptr;
            require(processor->process(data) == kResultOk, "VST3 process call failed");
            for (int i = 0; i < 128; ++i)
            {
                require(std::isfinite(output[0][i]) && output[0][i] == output[1][i], "invalid or unlinked stereo output");
                if(block<150)require(output[0][i]==input[0][i],"Fresh VST3 centre was not exact dry");
                else difference += std::abs(output[0][i] - input[0][i]);
            }
        }
        require(difference > 100, "actual VST3 did not process the bass");
        processor->setProcessing(false); component->setActive(false); processor->release();componentPoint->disconnect(controllerPoint);controllerPoint->disconnect(componentPoint);componentPoint->release();controllerPoint->release();controller->terminate();controller->release(); component->terminate(); component->release(); factory->release(); exit(); closeLibrary(library);
        std::cout << "PASS: real VST3 discovery, initialization, stereo buses, sample-exact fresh centre, host XY automation at DRIVE0, 38400 frames and shutdown\n";
    }
    catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
