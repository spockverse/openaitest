#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "XYMapping.h"

namespace {
const char* presetFolders[] { "01 Riddim Monster", "02 Liquid Glass", "03 Metal Teeth" };
const char* presetNames[] { "RIDDIM MONSTER", "LIQUID GLASS", "METAL TEETH" };
constexpr std::size_t maxSampleBytes = 20 * 1024 * 1024;
}
WubrotProcessor::WubrotProcessor(juce::File documentsForTests)
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "WubrotState", makeParameters()), presetLibrary(documentsForTests),
      values { parameters.getRawParameterValue("x"), parameters.getRawParameterValue("y"),
               parameters.getRawParameterValue("mutate"), parameters.getRawParameterValue("drive"),
               parameters.getRawParameterValue("mode"), parameters.getRawParameterValue("mix"),
               parameters.getRawParameterValue("output"), parameters.getRawParameterValue("protect"),
               parameters.getRawParameterValue("enabled"), parameters.getRawParameterValue("regionStart"),
               parameters.getRawParameterValue("regionEnd"),parameters.getRawParameterValue("dnaSpeed") }
{ markPresetClean(); startTimerHz(20); }
WubrotProcessor::~WubrotProcessor() { stopTimer(); stopping.store(true); generation.fetch_add(1); presetJobs.removeAllJobs(true,-1); sampleJobs.removeAllJobs(true, -1); }
juce::AudioProcessorValueTreeState::ParameterLayout WubrotProcessor::makeParameters()
{
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    auto add = [&](const char* id, const char* name, float lo, float hi, float initial, const char* unit) {
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { id, 1 }, name,
            juce::NormalisableRange<float>(lo, hi), initial, juce::AudioParameterFloatAttributes().withLabel(unit)));
    };
    add("x", "Crush Field X", -1, 1, 0, ""); add("y", "Crush Field Y", -1, 1, 0, "");
    add("mutate", "Mutate", 0, 100, 100, "%"); add("drive", "Drive", 0, 36, 0, "dB");
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { "mode", 1 }, "DNA Mode", juce::StringArray { "LOOP", "RETRIGGER" }, 0));
    add("mix", "Mix", 0, 100, 100, "%"); add("output", "Output", -24, 6, 0, "dB");
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "protect", 1 }, "Keep Sub", true));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "enabled", 1 }, "Engine On", true));
    add("regionStart","DNA Start",0,1,0,"");add("regionEnd","DNA End",0,1,1,"");
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID {"dnaSpeed",1},"DNA Speed",juce::StringArray{"0.25x","0.5x","1x","2x","4x"},2));
    return p;
}
void WubrotProcessor::prepareToPlay(double rate, int) { runningRate = rate; dsp.prepare(rate); visualAnalyser.prepare(rate); visualReadout.clear(); lastVisualBlockMs=juce::Time::getMillisecondCounterHiRes();lastVisualBlockDurationMs=0; expectedSample = lastReportedSample = -1;lastTransportPlaying=false;patternPosition.store(0); }
bool WubrotProcessor::isBusesLayoutSupported(const BusesLayout& b) const
{
    const auto out = b.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && b.getMainInputChannelSet() == out;
}
void WubrotProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    // Freshness only, not transport/BPM sync: discard a paused callback's old
    // visual envelope before new audio arrives. Never reset the sound engine.
    const auto visualNow=juce::Time::getMillisecondCounterHiRes();
    if(lastVisualBlockMs>=0&&visualNow-lastVisualBlockMs>std::max(120.0,lastVisualBlockDurationMs*3))
    { visualAnalyser.reset(); visualReadout.clear(); }
    lastVisualBlockMs=visualNow;lastVisualBlockDurationMs=buffer.getNumSamples()*1000.0/runningRate.load();
    if(resetProfileClock.exchange(false))dsp.resetModulation();
    float x = values[0]->load(), y = values[1]->load();
    CrushDSP::Parameters p;
    const auto mappingVersion=xyMapping.load();
    const auto mapped=XYMapping::map(x,y,runningRate.load(),mappingVersion);
    p.bits=mapped.bits;p.rate=mapped.rate;p.activity=mapped.activity;p.centered=mappingVersion>=2;p.processingVersion=mappingVersion;
    const float radius=std::max(1.f,std::hypot(x,y));p.signedX=x/radius;p.signedY=y/radius;
    p.mutate = values[2]->load() * .01f; p.drive = values[3]->load(); p.mode = static_cast<int>(values[4]->load());
    p.mix = values[5]->load() * .01f; p.output = values[6]->load(); p.protect = values[7]->load(); p.enabled = values[8]->load() >= .5f;
    constexpr float speeds[]{.25f,.5f,1,2,4};p.speed=speeds[juce::jlimit(0,4,static_cast<int>(values[11]->load()))];
    bool transportKnown=false,playing=true;
    if(auto* hostClock=getPlayHead())if(const auto position=hostClock->getPosition())
    {
        transportKnown=true;playing=position->getIsPlaying();
        auto samples=position->getTimeInSamples();if(!samples)if(const auto seconds=position->getTimeInSeconds())samples=std::llround(*seconds*runningRate.load());
        if(playing&&samples)
        {
            if(expectedSample>=0&&std::abs(*samples-expectedSample)>2&&(*samples!=lastReportedSample||!lastTransportPlaying)){dsp.resetModulation();dsp.seekPattern(static_cast<double>(*samples)/runningRate,p.speed);}
            lastReportedSample=*samples;expectedSample=*samples+buffer.getNumSamples();
        }
    }
    p.inputActive=buffer.getNumSamples()>0&&buffer.getMagnitude(0,buffer.getNumSamples())>1.0e-5f;
    p.patternEnabled=!transportKnown||playing;lastTransportPlaying=playing;
    auto profile = profiles.read();
    if (!p.enabled) { visualAnalyser.reset(); visualReadout.clear(); }
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
        if (p.enabled) visualAnalyser.observe(frame, buffer.getNumChannels());
        dsp.processFrame(frame, buffer.getNumChannels(), p, profile.get());
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) buffer.setSample(ch, i, frame[ch]);
    }
    patternPosition.store(dsp.getPatternPosition());
    visualReadout.publish(visualAnalyser.level(), visualAnalyser.takePeak());
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
void WubrotProcessor::setSampleStatus(std::uint64_t token,const juce::String& message)
{
    const juce::ScopedLock lock(metadataLock);if(token==generation.load())status=message;
}
void WubrotProcessor::finishSampleJob(std::uint64_t token,bool factoryPreset)
{
    const juce::ScopedLock lock(metadataLock);if(token!=generation.load())return;
    regionJob.store(false);loading.store(false);if(factoryPreset)markPresetClean();
}
void WubrotProcessor::loadSampleFile(const juce::File& file, bool factoryPreset)
{
    if(!file.existsAsFile()||!file.hasReadAccess()){setStatus("Cannot access this file. Use the upload icon to browse to a readable local WAV or MP3.");return;}
    const juce::ScopedLock loadingLock(metadataLock);
    if (loading.exchange(true)) { setStatus("A sample is loading. Please wait, then try again."); return; }
    presetRequest.store(-1);
    const auto token = generation.fetch_add(1) + 1;
    sampleJobs.addJob([this, file, token, factoryPreset] {
        if (file.getSize() > static_cast<juce::int64>(maxSampleBytes)) setSampleStatus(token,"Audio file must be 20 MB or smaller.");
        else decodeSample(file.createInputStream(), file.getFileName(), token);
        finishSampleJob(token,factoryPreset);
    });
}
void WubrotProcessor::loadSampleBytes(juce::MemoryBlock bytes, juce::String name)
{
    if (bytes.getSize() > maxSampleBytes) { setStatus("Audio file must be 20 MB or smaller."); return; }
    const juce::ScopedLock loadingLock(metadataLock);
    if (loading.exchange(true)) { setStatus("A sample is loading. Please wait, then try again."); return; }
    presetRequest.store(-1);
    const auto token = generation.fetch_add(1) + 1;
    sampleJobs.addJob([this, data = std::move(bytes), name = std::move(name), token] {
        decodeSample(std::make_unique<juce::MemoryInputStream>(data, false), name, token); finishSampleJob(token);
    });
}
void WubrotProcessor::decodeSample(std::unique_ptr<juce::InputStream> stream, const juce::String& name, std::uint64_t token, bool restoreOnly)
{
    if (token != generation.load()) return;
    if(!stream){setSampleStatus(token,"Cannot open this audio file. Check file access or use the upload icon.");return;}
    setSampleStatus(token,"Analysing spectral shape, harmonics and dynamics...");
    auto prepared=std::make_shared<SampleSource>();
    if(stream)stream->readIntoMemoryBlock(prepared->wav,static_cast<int>(maxSampleBytes+1));
    if(prepared->wav.getSize()>maxSampleBytes){setSampleStatus(token,"Audio file must be 20 MB or smaller.");return;}
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(std::make_unique<juce::MemoryInputStream>(prepared->wav,false)));
    if (!reader || reader->sampleRate < 8000 || reader->sampleRate > 384000 || reader->numChannels > 8 || reader->lengthInSamples < 1)
    { setSampleStatus(token,name.endsWithIgnoreCase(".wav")||name.endsWithIgnoreCase(".mp3")?"Decoding failed. Choose a valid PCM/float WAV or MP3. Previous DNA is unchanged.":"Unsupported audio format. Choose WAV or MP3. Previous DNA is unchanged."); return; }
    const int frames = static_cast<int>(std::min(reader->lengthInSamples, static_cast<juce::int64>(reader->sampleRate * 30)));
    juce::AudioBuffer<float> audio(static_cast<int>(reader->numChannels), frames);
    if (!reader->read(audio.getArrayOfWritePointers(), audio.getNumChannels(), 0, frames)) { setSampleStatus(token,"Audio decoding/read failed. Previous DNA is unchanged."); return; }
    const auto profile = DnaProfile::analyse(audio.getArrayOfReadPointers(), audio.getNumChannels(), static_cast<std::size_t>(frames), reader->sampleRate,xyMapping.load()>=3);
    if (!profile.valid) { setSampleStatus(token,"This WAV is silent. Previous DNA is unchanged."); return; }
    prepared->audio=std::move(audio);prepared->rate=reader->sampleRate;prepared->full=profile;
    const float start=values[9]->load(),end=values[10]->load();
    const auto active=analyseRegion(*prepared,start,end);
    const juce::ScopedLock lock(metadataLock);
    if (token != generation.load()) return;
    if(!restoreOnly&&!profiles.publish(active)){status="DNA is busy. Please load the sample again.";return;}
    source=prepared;analysisArchive.reset();storedWav=prepared->wav;fullEnvelope=profile.envelope;
    if(restoreOnly){status="Project sample editor restored.";return;}
    analysedStart=start;analysedEnd=end;dnaOriginKey=WubrotPresetData::key(profile);updateDnaKey(active);
    sampleName = name; status = !active.valid?"Selected region is silent. DNA influence is inactive.":frames<reader->lengthInSamples?"DNA loaded. Editor uses the first 30 seconds.":"DNA loaded. Selected spectral region shapes incoming harmonics.";
}
DnaProfile WubrotProcessor::analyseRegion(const SampleSource& s,float start,float end)
{
    const float lo=juce::jlimit(0.0f,.99f,std::min(start,end)),hi=juce::jlimit(lo+.005f,1.0f,std::max(start,end));
    const int first=juce::jlimit(0,s.audio.getNumSamples()-1,static_cast<int>(lo*s.audio.getNumSamples()));
    const int last=juce::jlimit(first+1,s.audio.getNumSamples(),static_cast<int>(hi*s.audio.getNumSamples()));
    std::array<const float*,8> channels{};for(int ch=0;ch<s.audio.getNumChannels();++ch)channels[static_cast<std::size_t>(ch)]=s.audio.getReadPointer(ch)+first;
    if(first==0&&last==s.audio.getNumSamples())return s.full;
    return DnaProfile::analyse(channels.data(),s.audio.getNumChannels(),static_cast<std::size_t>(last-first),s.rate,s.full.spectralCount>0);
}
void WubrotProcessor::serviceRegionEdit()
{
    if(loading.load())return;
    const float start=values[9]->load(),end=values[10]->load();
    std::shared_ptr<const SampleSource> snapshot;std::shared_ptr<const WubrotPresetData::Archive> archive;DnaProfile anchor;float anchorStart=0,anchorEnd=1;std::uint64_t token=0;
    {const juce::ScopedLock lock(metadataLock);if((!source&&!analysisArchive)|| (std::abs(start-analysedStart)<1e-6f&&std::abs(end-analysedEnd)<1e-6f))return;snapshot=source;archive=analysisArchive;anchor=recalledRegion;anchorStart=recalledStart;anchorEnd=recalledEnd;if(loading.exchange(true))return;regionJob.store(true);token=generation.load();}
    sampleJobs.addJob([this,snapshot,archive,anchor,anchorStart,anchorEnd,start,end,token]{
        auto active=snapshot?analyseRegion(*snapshot,start,end):(std::abs(start-anchorStart)<1e-6f&&std::abs(end-anchorEnd)<1e-6f?anchor:archive->select(start,end));
        {const juce::ScopedLock lock(metadataLock);if(token==generation.load()){
            if(profiles.publish(active)){analysedStart=start;analysedEnd=end;updateDnaKey(active);status=active.valid?"Selected region shapes the distortion.":"Selected region is silent. DNA influence is inactive.";}
            else{status="DNA busy; retrying region analysis.";}
        }}finishSampleJob(token);
    });
}
void WubrotProcessor::clearSample()
{
    const juce::ScopedLock lock(metadataLock); generation.fetch_add(1); presetRequest.store(-1);
    loading.store(false);regionJob.store(false);source.reset();analysisArchive.reset();storedWav.reset();fullEnvelope={};dnaKey.clear();dnaOriginKey.clear();analysedStart=0;analysedEnd=1;
    profiles.publish({}); sampleName.clear(); status = "DNA cleared. Input-driven harmonic shaping remains active.";
}
juce::File WubrotProcessor::factoryRoot() const
{
    const auto overridePath = juce::SystemStats::getEnvironmentVariable("WUBROT_CONTENT_DIR", {});
    if (overridePath.isNotEmpty()) return juce::File(overridePath).getChildFile("Presets/Factory");
   #if JUCE_WINDOWS
    const auto bundled = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
        .getParentDirectory().getSiblingFile("Resources").getChildFile("WubrotContent/Presets/Factory");
    if (bundled.isDirectory()) return bundled;
   #endif
    return juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory).getChildFile("Spock Audio/WUBROT/Presets/Factory");
}
const juce::String WubrotProcessor::getProgramName(int i) { return i >= 0 && i < 3 ? presetNames[i] : ""; }
bool WubrotProcessor::loadFactoryPreset(int index,juce::String* error)
{
    if(error)error->clear();
    if (index < 0 || index >= 3 || loading.load()) {if(error)*error="Factory preset unavailable while DNA is loading.";return false;}
    const auto folder = factoryRoot().getChildFile(presetFolders[index]);
    const auto manifest = juce::JSON::parse(folder.getChildFile("preset.json"));
    if (!manifest.isObject()) { const juce::String message="Factory preset content is missing or invalid. Replace WUBROT.vst3 with the complete development bundle.";setStatus(message);if(error)*error=message;return false; }
    clearSample();
    const auto data = manifest["parameters"];
    for (const auto* id : { "x", "y", "mutate", "drive", "mode", "mix", "output", "protect", "enabled", "regionStart", "regionEnd", "dnaSpeed" })
    {
        float fallback=parameters.getParameter(id)->convertFrom0to1(parameters.getParameter(id)->getDefaultValue());
        const juce::String parameterId(id);
        if(parameterId=="mutate")fallback=67;
        else if(parameterId=="drive")fallback=8.5f;
        else if(parameterId=="output")fallback=-6;
        parameterGesture(id,true);setParameterValue(id,data.hasProperty(id)?static_cast<float>(data[id]):fallback);parameterGesture(id,false);
    }
    xyMapping.store(1); // Factory sounds retain their original mapping.
    currentPreset.store(index);
    {const juce::ScopedLock lock(metadataLock);activePresetName=presetNames[index];activeUserFile.clear();}
    const auto relative = manifest["sample"].toString();
    const auto file = folder.getChildFile(relative);
    if (relative.isEmpty() || !file.isAChildOf(folder) || !file.existsAsFile())
    { setStatus("Factory settings loaded; its optional factory WAV has not been supplied. Load your own sample.");markPresetClean(); return false; }
    loadSampleFile(file,true); return true;
}
void WubrotProcessor::timerCallback()
{
    serviceRegionEdit();
    if (!loading.load()) { const int requested = presetRequest.exchange(-1); if (requested >= 0) loadFactoryPreset(requested); }
}
juce::var WubrotProcessor::uiState(bool includeWaveform)
{
    const juce::ScopedLock lock(metadataLock);
    auto object = std::make_unique<juce::DynamicObject>();
    for (const auto* id : { "x", "y", "mutate", "drive", "mode", "mix", "output", "protect", "enabled", "regionStart", "regionEnd", "dnaSpeed" })
        object->setProperty(id, parameters.getRawParameterValue(id)->load());
    object->setProperty("name", sampleName); object->setProperty("status", status);
    object->setProperty("loading", loading.load()&&!regionJob.load()); object->setProperty("preset", currentPreset.load());
    object->setProperty("xyMapping",xyMapping.load());
    const auto mapping=XYMapping::map(values[0]->load(),values[1]->load(),runningRate.load(),xyMapping.load());
    object->setProperty("crushBitDepth",mapping.bits);object->setProperty("crushSampleRate",mapping.rate);
    object->setProperty("sampleRate", runningRate.load()); object->setProperty("editorSize", editorSize.load());
    juce::Array<juce::var> wave;
    if(includeWaveform)
    {
        auto reader=profiles.read();
        if(const auto* profile=reader.get();profile&&(profile->valid||source||analysisArchive||storedWav.getSize()>0))
            for(std::size_t i=0;i<128;++i)wave.add(source||analysisArchive||storedWav.getSize()>0?fullEnvelope[i*DnaProfile::bins/128]:profile->envelope[i*DnaProfile::bins/128]);
    }
    object->setProperty("editable",source!=nullptr||analysisArchive!=nullptr);object->setProperty("analysisOnly",source==nullptr&&analysisArchive!=nullptr);object->setProperty("playhead",patternPosition.load());
    object->setProperty("waveformRevision",dnaOriginKey);
    if(includeWaveform)object->setProperty("waveform", wave);
    auto* preset=new juce::DynamicObject();preset->setProperty("name",activePresetName);preset->setProperty("file",activeUserFile);preset->setProperty("kind",activeUserFile.isNotEmpty()?"user":juce::StringArray{presetNames[0],presetNames[1],presetNames[2]}.contains(activePresetName)?"factory":"custom");preset->setProperty("dirty",hasUnsavedPresetChanges());preset->setProperty("busy",presetBusy.load());preset->setProperty("error",libraryError);preset->setProperty("result",presetResult);object->setProperty("presets",preset);

    return juce::var(object.release());
}
juce::ValueTree WubrotProcessor::captureState(bool includeSourceAudio)
{
    const juce::ScopedLock lock(metadataLock);
    auto state = parameters.copyState(); state.setProperty("xyMapping",xyMapping.load(),nullptr); state.removeProperty("envelope", nullptr); state.removeProperty("duration", nullptr); state.removeProperty("dnaCharacter", nullptr); state.removeProperty("dnaSpectrum", nullptr); state.setProperty("version", 1, nullptr); state.setProperty("editorSize", editorSize.load(), nullptr);
    state.setProperty("analysisStart",analysedStart,nullptr);state.setProperty("analysisEnd",analysedEnd,nullptr);
    state.setProperty("sampleName", sampleName, nullptr); state.setProperty("preset", currentPreset.load(), nullptr);
    state.removeProperty("sourceWav",nullptr);state.removeProperty("fullEnvelope",nullptr);
    state.setProperty("dnaOriginKey",dnaOriginKey,nullptr);
    state.setProperty("presetDisplayName",activePresetName,nullptr);state.setProperty("presetUserFile",activeUserFile,nullptr);state.setProperty("presetBaseline",presetBaseline,nullptr);
    state.removeProperty("dnaArchive",nullptr);state.removeProperty("presetProfile",nullptr);state.removeProperty("dnaTemporal",nullptr);
    if(analysisArchive)state.setProperty("dnaArchive",juce::JSON::toString(analysisArchive->toVar(),true),nullptr);

    if(includeSourceAudio&&storedWav.getSize()>0){state.setProperty("sourceWav",storedWav.toBase64Encoding(),nullptr);juce::Array<juce::var> full;for(auto v:fullEnvelope)full.add(v);state.setProperty("fullEnvelope",juce::JSON::toString(juce::var(full),true),nullptr);}
    auto reader = profiles.read();
    if (const auto* profile = reader.get(); profile && profile->valid)
    {
        juce::Array<juce::var> data; for (auto value : profile->envelope) data.add(value);
        state.setProperty("envelope", juce::JSON::toString(juce::var(data), true), nullptr);
        state.setProperty("duration", profile->duration, nullptr);
        if(profile->spectralCount>0)state.setProperty("dnaTemporal",juce::JSON::toString(WubrotPresetData::temporal(*profile),true),nullptr);
        juce::Array<juce::var> characterData; for (auto v : profile->character) characterData.add(v);
        state.setProperty("dnaCharacter", juce::JSON::toString(juce::var(characterData), true), nullptr);
        if(profile->spectrum.valid)
        {
            juce::Array<juce::var> spectrum;
            for(auto v:profile->spectrum.bands)spectrum.add(v);
            for(auto v:profile->spectrum.features)spectrum.add(v);
            for(auto v:profile->spectrum.peaks)spectrum.add(v);
            state.setProperty("dnaSpectrum",juce::JSON::toString(juce::var(spectrum),true),nullptr);
        }
    }
    return state;
}
void WubrotProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    if(auto xml=captureState(true).createXml())copyXmlToBinary(*xml,destination);
}
void WubrotProcessor::setStateInformation(const void* data, int size)
{
    if (size < 1 || size > 32 * 1024 * 1024) return;
    const auto xml = getXmlFromBinary(data, size);
    if (!xml || !xml->hasTagName("WubrotState")) return;
    auto state = juce::ValueTree::fromXml(*xml);
    if(state.hasProperty("xyMapping")&&state["xyMapping"].toString()!="1"&&state["xyMapping"].toString()!="2"&&state["xyMapping"].toString()!="3")
    {setStatus("Unsupported XY mapping version. This project needs a newer WUBROT build.");return;}
    if (!state.getChildWithProperty("id", "enabled").isValid())
    {
        juce::ValueTree on("PARAM"); on.setProperty("id", "enabled", nullptr); on.setProperty("value", 1.0f, nullptr);
        state.appendChild(on, nullptr);
    }
    for(const auto& item:std::array<std::pair<const char*,float>,3>{{{"regionStart",0},{"regionEnd",1},{"dnaSpeed",2}}})
        if(!state.getChildWithProperty("id",item.first).isValid()){juce::ValueTree value("PARAM");value.setProperty("id",item.first,nullptr);value.setProperty("value",item.second,nullptr);state.appendChild(value,nullptr);}
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
    const auto characterData = juce::JSON::parse(state["dnaCharacter"].toString());
    if (auto* array = characterData.getArray(); profile.valid && array && array->size() == static_cast<int>(DnaProfile::bins))
        for (std::size_t i = 0; i < DnaProfile::bins; ++i)
        {
            const float v = static_cast<float>((*array)[static_cast<int>(i)]);
            profile.character[i] = std::isfinite(v) ? juce::jlimit(0.0f, 1.0f, v) : .5f;
        }
    const auto spectralData=juce::JSON::parse(state["dnaSpectrum"].toString());
    if(auto* array=spectralData.getArray();profile.valid&&array&&array->size()==20)
    {
        bool valid=true;
        for(int i=0;i<20;++i)
        {
            const float v=static_cast<float>((*array)[i]);
            if(!std::isfinite(v)||v<0||v>(i<17?1.0f:10000.0f)||(i>=17&&v<100)){valid=false;break;}
            if(i<12)profile.spectrum.bands[static_cast<std::size_t>(i)]=v;
            else if(i<17)profile.spectrum.features[static_cast<std::size_t>(i-12)]=v;
            else profile.spectrum.peaks[static_cast<std::size_t>(i-17)]=v;
        }
        profile.spectrum.valid=valid;
        if(valid)profile.spectrum.prepareDerived();
    }
    if(state.hasProperty("dnaTemporal")&&(!profile.valid||!WubrotPresetData::readTemporal(juce::JSON::parse(state["dnaTemporal"].toString()),profile))){setStatus("Invalid temporal DNA state. Current sound preserved.");return;}
    std::shared_ptr<const WubrotPresetData::Archive> archive;
    if(state.hasProperty("dnaArchive"))archive=WubrotPresetData::Archive::fromVar(juce::JSON::parse(state["dnaArchive"].toString()));
    if(state.hasProperty("presetProfile")){DnaProfile exact;if(WubrotPresetData::readProfile(juce::JSON::parse(state["presetProfile"].toString()),exact))profile=exact;}
    if(int(state.getProperty("xyMapping",1))>=3&&profile.valid&&profile.spectralCount==0){setStatus("Missing temporal DNA descriptors. Current sound preserved.");return;}
    const juce::ScopedLock lock(metadataLock); generation.fetch_add(1); presetRequest.store(-1);
    loading.store(false);regionJob.store(false);analysisArchive=archive;dnaOriginKey=archive?WubrotPresetData::key(archive->full):state.getProperty("dnaOriginKey",profile.valid?WubrotPresetData::key(profile):juce::String()).toString();updateDnaKey(profile);
    activePresetName=state.getProperty("presetDisplayName",getProgramName(juce::jlimit(0,2,static_cast<int>(state["preset"])))).toString();
    activeUserFile=state["presetUserFile"].toString();if(activeUserFile.isNotEmpty()&&(!activeUserFile.endsWithIgnoreCase(".wubpreset")||PresetLibrary::validName(activeUserFile.dropLastCharacters(10)).failed()))activeUserFile.clear();
    editorSize.store(juce::jlimit(0, 2, static_cast<int>(state.getProperty("editorSize", 1))));
    source.reset();storedWav.reset();fullEnvelope=archive?archive->full.envelope:profile.envelope;
    const auto full=juce::JSON::parse(state["fullEnvelope"].toString());
    if(auto* array=full.getArray();array&&array->size()==static_cast<int>(DnaProfile::bins))for(std::size_t i=0;i<DnaProfile::bins;++i){const float v=static_cast<float>((*array)[static_cast<int>(i)]);fullEnvelope[i]=std::isfinite(v)?juce::jlimit(0.0f,1.0f,v):0;}
    storedWav.fromBase64Encoding(state["sourceWav"].toString());if(storedWav.getSize()>maxSampleBytes)storedWav.reset();
    xyMapping.store(juce::jlimit(1,3,static_cast<int>(state.getProperty("xyMapping",1))));
    parameters.replaceState(state); profiles.publish(profile);
    presetBaseline=state.getProperty("presetBaseline",soundSignature()).toString();
    analysedStart=static_cast<float>(state.getProperty("analysisStart",values[9]->load()));analysedEnd=static_cast<float>(state.getProperty("analysisEnd",values[10]->load()));
    recalledRegion=profile;recalledStart=analysedStart;recalledEnd=analysedEnd;
    sampleName = profile.valid||archive||storedWav.getSize()>0 ? state["sampleName"].toString() : juce::String();
    currentPreset.store(juce::jlimit(0, 2, static_cast<int>(state["preset"])));resetProfileClock.store(true);patternPosition.store(0);
    if(storedWav.getSize()>0){loading.store(true);const auto token=generation.load();auto bytes=storedWav;auto name=sampleName;sampleJobs.addJob([this,bytes=std::move(bytes),name,token]{decodeSample(std::make_unique<juce::MemoryInputStream>(bytes,false),name,token,true);finishSampleJob(token);});}
    status = archive?"DNA analysis restored. Original audio is not embedded; region edits use stored analysis.": !profile.valid ? "Project restored without DNA." : profile.spectrum.valid ? "Project spectral DNA restored (no external sample needed)." : "Legacy DNA restored. Reload WAV for richer spectral analysis.";
}
juce::AudioProcessorEditor* WubrotProcessor::createEditor() { return new WubrotEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WubrotProcessor(); }
