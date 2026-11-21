// Real JUCE/WebView2 bridge check. The probe is injected by this test executable,
// never included in the plugin HTML or displayed as a diagnostic panel.
#include "PluginEditor.h"
#include <chrono>
#include <iostream>
#include <thread>

namespace {
class ReleaseFixture final:public WubrotUpdates::Transport {
public:
 juce::String tag;
 explicit ReleaseFixture(juce::String v):tag(std::move(v)){}
 juce::String fetch()override{return "{\"tag_name\":\"v"+tag+"\",\"draft\":false,\"prerelease\":false,\"body\":\"Native update bridge fixture.\",\"assets\":[{\"name\":\"WUBROT-Windows-x64.zip\",\"state\":\"uploaded\",\"size\":4000000,\"digest\":\"sha256:"+juce::String::repeatedString("a",64)+"\"}]}";}
 void cancel()override{}
};
class ProgressFixture final:public WubrotUpdates::DownloadTask {
public:
 juce::String version,phase="downloading";int starts=0,cancels=0;
 explicit ProgressFixture(juce::String v):version(std::move(v)){}
 void start()override{++starts;}void cancel()override{++cancels;phase="cancelled";}
 juce::var snapshot()const override{auto d=std::make_unique<juce::DynamicObject>();d->setProperty("phase",phase);d->setProperty("version",version);d->setProperty("received",2400000);d->setProperty("total",4000000);return juce::var(d.release());}
};
}
class WubrotWebViewProbe : private juce::Timer
{
public:
    explicit WubrotWebViewProbe(WubrotProcessor& p,WubrotEditor& e,juce::File report):processor(p),editor(e),reportFile(std::move(report))
    {
        started=juce::Time::getMillisecondCounterHiRes();startTimer(50);
        audio=std::thread([this]{juce::AudioBuffer<float> buffer(2,480);juce::MidiBuffer midi;int sample=0;auto next=std::chrono::steady_clock::now();while(!stop.load()){
            const int kind=input.load();for(int i=0;i<480;++i,++sample){const float t=sample/48000.0f,part=std::fmod(t,.3f);const float env=part<.21f?1.0f:.58f;const float wave=kind==2?.7f*std::sin(t*7539.82237f)+.3f*std::sin(t*5026.54825f):(std::sin(t*408.407045f)>0?1.0f:-1.0f);const float v=kind==0?0:.55f*env*wave;buffer.setSample(0,i,v);buffer.setSample(1,i,-v);}processor.processBlock(buffer,midi);next+=std::chrono::milliseconds(10);std::this_thread::sleep_until(next);
        }});
    }
    ~WubrotWebViewProbe() override {if(updateFixture){updateFixture->release(&editor);editor.updates=nullptr;editor.updateLauncher=[](const juce::String&){return false;};}life.reset();stopTimer();stop.store(true);if(audio.joinable())audio.join();}
    int result=1;
private:
    void evaluate(const juce::String& script,std::function<void(juce::var)> callback)
    {
        pending=true;
        editor.browser->evaluateJavascript(script,[this,guard=std::weak_ptr<int>(life),callback=std::move(callback)](juce::WebBrowserComponent::EvaluationResult r){if(guard.expired())return;pending=false;if(const auto* value=r.getResult())callback(juce::JSON::parse(value->toString()));else if(const auto* error=r.getError())lastError=error->message;});
    }
    void finish(const juce::String& error={})
    {
        if(reportFile!=juce::File()) { auto report=std::make_unique<juce::DynamicObject>();report->setProperty("error",error);report->setProperty("evaluationError",lastError);report->setProperty("nativeReady",editor.visualBridgeReady);report->setProperty("nativeLevel",editor.lastAudioLevel);report->setProperty("phases",records);report->setProperty("driveBridge",driveBridgeValues);report->setProperty("mutateBridge",mutateBridgeValues);report->setProperty("latest",lastMetrics);report->setProperty("presetSwitching",presetMetrics);report->setProperty("xyCoordinates",xyMetrics);reportFile.getParentDirectory().createDirectory();reportFile.replaceWithText(juce::JSON::toString(juce::var(report.release()),true)); }
        stopTimer();if(error.isEmpty()){std::cout<<"PASS: actual processBlock -> native timer -> JUCE WebView2 -> live shader/canvas; low/mid hits, silence, power, unchanged XY and actual preset SAVE/DISCARD/CANCEL/failed-load switching\n";result=0;}else std::cerr<<"FAIL: "<<error<<" last evaluation: "<<lastError<<" native ready="<<editor.visualBridgeReady<<" forwarded="<<editor.lastAudioLevel<<" seq="<<editor.lastAudioSequence<<" ticks="<<editor.uiTicks<<" frontend="<<juce::JSON::toString(lastMetrics,true)<<'\n';
        juce::MessageManager::getInstance()->stopDispatchLoop();
    }
    void checkPresetBrowser()
    {
        if(!presetProbeStarted){
            evaluate(R"JS((()=>{
              window.__presetProbe={done:false,error:'',checks:[]};
              const probe=window.__presetProbe,names=['RIDDIM MONSTER','LIQUID GLASS','METAL TEETH'],$=id=>document.getElementById(id);
              window.__JUCE__.backend.addEventListener('wubrotState',s=>window.__presetNativeState=s);
              const wait=async test=>{const end=performance.now()+7000;while(!test()){if(performance.now()>end)throw Error('Preset UI wait timed out: '+JSON.stringify({name:$('presetName').textContent,busy:$('preset').getAttribute('aria-busy'),dialog:$('presetDialogLayer').hidden?'':$('presetDialogTitle').textContent,native:window.__presetNativeState?.presets}));await new Promise(r=>setTimeout(r,20));}};
              const assert=(ok,message)=>{if(!ok)throw Error(message);},near=(a,b)=>Math.abs(a-b)<.001;
              const button=label=>{const b=[...$('presetDialogButtons').children].find(e=>e.textContent===label);assert(b,'Missing dialog action '+label);b.click();};
              const native=()=>window.__presetNativeState;
              const settled=async name=>wait(()=>$('presetName').textContent===name&&$('preset').getAttribute('aria-busy')==='false'&&$('presetDialogLayer').hidden&&native()?.presets.name===name&&!native().presets.busy);
              const tab=async kind=>{if($('preset').getAttribute('aria-expanded')!=='true')$('preset').click();await wait(()=>$('preset').getAttribute('aria-busy')==='false');document.querySelector('[data-tab='+kind+']').click();$('presetSearch').value='';$('presetSearch').dispatchEvent(new Event('input'));};
              const pick=label=>{const b=[...document.querySelectorAll('.preset-item')].find(e=>e.textContent.endsWith(label));assert(b,'Preset row missing '+label);b.click();};
              const factory=async i=>{await tab('factory');pick(names[i]);await settled(names[i]);assert(native().preset===i&&native().presets.kind==='factory'&&!native().presets.dirty,'Native factory identity/dirty mismatch');assert(Math.abs(native().drive-[12,4,20][i])<.001,'Factory parameters not loaded');assert(!native().name&&!sampleLoaded&&!regionEditable,'Old DNA remained loaded');assert(native().status.includes('DNA sample unavailable'),'Missing nonblocking DNA warning');};
              const edit=async(value=23)=>{nativeEvent('gesture',{id:'drive',start:true});$('drive').value=value;$('drive').dispatchEvent(new Event('input',{bubbles:true}));nativeEvent('gesture',{id:'drive',start:false});await wait(()=>native()?.presets.dirty&&Math.abs(native().drive-value)<.001);};
              const chooseDirty=async(i,decision)=>{await tab('factory');pick(names[i]);await wait(()=>!$('presetDialogLayer').hidden&&$('presetDialogTitle').textContent==='UNSAVED CHANGES');button(decision);};
              (async()=>{
                for(let i=0;i<3;++i)await factory(i);probe.checks.push('all factory parameters load without WAVs');
                await edit();await chooseDirty(1,'Cancel');await settled(names[2]);assert(native().presets.dirty&&near(native().drive,23),'CANCEL changed settings');
                await chooseDirty(1,'Discard');await settled(names[1]);assert(!native().presets.dirty&&near(native().drive,4),'DISCARD did not switch');probe.checks.push('CANCEL preserves and DISCARD resumes');
                await edit();await chooseDirty(2,'Save');await wait(()=>!$('presetDialogLayer').hidden&&$('presetDialogTitle').textContent==='SAVE PRESET');$('presetNameInput').value='Native Saved Factory';button('Save preset');await settled(names[2]);probe.checks.push('factory Save As retains pending factory target');
                await tab('user');pick('Native Saved Factory');await settled('Native Saved Factory');assert(native().presets.kind==='user'&&near(native().drive,23),'Saved factory edits not restored');
                await edit(22);await chooseDirty(1,'Save');await settled(names[1]);await tab('user');pick('Native Saved Factory');await settled('Native Saved Factory');assert(near(native().drive,22),'User SAVE did not persist edits');probe.checks.push('user SAVE then factory/user recall');
                await tab('factory');pick(names[2]);await settled(names[2]);await tab('factory');document.querySelectorAll('.preset-item').forEach(e=>e.click());await settled(names[0]);probe.checks.push('rapid selection settles and unblocks');
                await tab('user');pick('Broken');await wait(()=>$('preset').getAttribute('aria-busy')==='false'&&native()?.presets.result.request.file==='Broken.wubpreset'&&native().presets.result.ok===false);assert($('presetName').textContent===names[0]&&$('presetDialogLayer').hidden,'Failed user load left stale selection/dialog');
                await factory(1);probe.checks.push('failed load preserves state and next switch works');probe.done=true;
              })().catch(error=>{probe.error=String(error);probe.done=true;});
              return JSON.stringify({started:true});
            })())JS",[this](juce::var){presetProbeStarted=true;});
        }else evaluate("JSON.stringify(window.__presetProbe)",[this](juce::var v){presetMetrics=v;if(!bool(v["done"]))return;if(v["error"].toString().isNotEmpty())finish(v["error"].toString());else if(processor.getCurrentProgram()!=1||std::abs(processor.parameters.getRawParameterValue("drive")->load()-4)>.001f||processor.hasUnsavedPresetChanges())finish("Preset DOM/native processor state diverged");else presetFinished=true;});
    }
    void checkXYControls(double now)
    {
        static constexpr float positions[][2]{{.5f,-.2f},{-.4f,-.2f},{-.4f,.6f},{.3f,-.4f},{1,1},{.3f,-.4f},{-.2f,.3f},{.3f,-.4f},{.2f,-.6f},{.3f,-.4f}};
        if(!xySent){
            xySent=true;
            if(xyStage==0||xyStage==9){
                evaluate("(()=>{for(const [id,value] of [['xyX','"+juce::String(xyStage==0?75:65)+"'],['xyY','"+juce::String(xyStage==0?60:70)+"']]){const e=document.getElementById(id);e.focus();e.value=value;e.dispatchEvent(new Event('input',{bubbles:true}));e.dispatchEvent(new KeyboardEvent('keydown',{key:'Enter',bubbles:true}));}return JSON.stringify({ok:true});})()",[](juce::var){});return;
            }
            if(xyStage==5){if(processor.loadUserPreset("XY Recall.wubpreset").failed()){finish("XY preset recall failed");return;}}
            else if(xyStage==7){processor.deleteUserPreset("XY Recall.wubpreset");processor.setStateInformation(xyProject.getData(),int(xyProject.getSize()));}
            else{if(xyStage==8)processor.setParameterValue("enabled",0);if(xyStage!=2)processor.setParameterValue("x",positions[xyStage][0]);if(xyStage!=1)processor.setParameterValue("y",positions[xyStage][1]);}
        }
        evaluate(R"JS(JSON.stringify({x:px,y:py,readouts:[document.getElementById('xyX').value,document.getElementById('xyY').value],off:!engineIsOn(),frameRequest,glow:getComputedStyle(document.getElementById('xyXTrack'),':after').boxShadow}))JS",[this,now](juce::var v){
            lastMetrics=v;v.getDynamicObject()->setProperty("xyStage",xyStage);v.getDynamicObject()->setProperty("nativeX",processor.parameters.getRawParameterValue("x")->load());v.getDynamicObject()->setProperty("nativeY",processor.parameters.getRawParameterValue("y")->load());const auto gx=positions[xyStage][0],gy=positions[xyStage][1];const double radius=std::max(1.,std::hypot(double(gx),double(gy)));
            if(std::abs(processor.parameters.getRawParameterValue("x")->load()-gx)>.0001f||std::abs(processor.parameters.getRawParameterValue("y")->load()-gy)>.0001f||std::abs(double(v["x"])-gx/radius)>.00001||std::abs(double(v["y"])-gy/radius)>.00001)return;
            if(v["readouts"][0].toString()!=juce::String((1+gx/radius)*50,1)||v["readouts"][1].toString()!=juce::String((1-gy/radius)*50,1))return;
            if(xyStage==0)xyAck=editor.xyWriteAck;
            else if(xyStage<=8&&editor.xyWriteAck!=xyAck){finish("Host/project XY updates fed parameters back through WebView");return;}
            if(xyStage>=8&&(!bool(v["off"])||int(v["frameRequest"])!=0||v["glow"].toString()!="none"))return;
            xyMetrics.add(v);
            if(xyStage==3){if(processor.saveUserPreset("XY Recall").failed()){finish("XY preset save failed");return;}processor.getStateInformation(xyProject);}
            if(++xyStage==10){std::cout<<"PASS actual WebView XY numeric controls -> native parameters, independent/simultaneous host updates, circular projection, preset/project recall and OFF interactivity\n";xyFinished=true;return;}
            xySent=false;stageStart=now;
        });
    }
    void checkUpdates()
    {
        if(!updateFixture){
            const auto major=juce::String(WUBROT_CURRENT_VERSION).upToFirstOccurrenceOf(".",false,false).getIntValue();updateVersion=juce::String(major+1)+".0.0";
            auto source=std::make_unique<ReleaseFixture>(WUBROT_CURRENT_VERSION);releaseFixture=source.get();
            updateFixture=std::make_unique<WubrotUpdates::Session>(WUBROT_CURRENT_VERSION,std::move(source),true,[this](const WubrotUpdates::Release& release){auto p=std::make_unique<ProgressFixture>(release.version);progressFixture=p.get();return p;});
            if(editor.updates)editor.updates->release(&editor);editor.updates=updateFixture.get();editor.updateSent=false;editor.lastUpdateProgress.clear();
            editor.updateLauncher=[this](const juce::String& url){updateNotes=url;return true;};updateFixture->checkOnce();return;
        }
        evaluate(R"JS(JSON.stringify({open:!document.getElementById('updateLayer').hidden,percent:document.getElementById('updatePercent').textContent,intro:document.getElementById('updateIntro').textContent,inert:document.querySelector('.grid').inert,badge:!document.getElementById('updateBadge').hidden,menu:!document.getElementById('updateMenu').hidden,check:document.getElementById('updateCheckStatus').textContent}))JS",[this](juce::var state){
            if(updateStage==-2&&bool(state["badge"])&&state["check"].toString().contains("up to date")){releaseFixture->tag=updateVersion;updateStage=-1;evaluate("document.getElementById('updateBadge').click();JSON.stringify({ok:true})",[](juce::var){});}
            else if(updateStage==-1&&bool(state["menu"])){updateStage=0;evaluate("document.getElementById('updateCheck').click();JSON.stringify({ok:true})",[](juce::var){});}
            else if(updateStage==0&&bool(state["open"])) {updateStage=1;evaluate("document.getElementById('updateNotes').click();JSON.stringify({ok:true})",[](juce::var){});}
            else if(updateStage==1&&updateNotes.endsWith("/releases/tag/v"+updateVersion)){updateStage=2;evaluate("document.getElementById('updateNow').click();JSON.stringify({ok:true})",[](juce::var){});}
            else if(updateStage==2&&state["percent"].toString()=="60%"&&progressFixture&&progressFixture->starts==1){updateStage=3;evaluate("document.getElementById('updateLater').click();JSON.stringify({ok:true})",[](juce::var){});}
            else if(updateStage==3&&!bool(state["open"])&&!bool(state["inert"])&&bool(state["badge"])&&progressFixture->cancels==1){updateStage=4;progressFixture=nullptr;evaluate("document.getElementById('updateBadge').click();JSON.stringify({ok:true})",[](juce::var){});}
            else if(updateStage==4&&bool(state["open"])&&!bool(state["badge"])){updateStage=5;evaluate("document.getElementById('updateLater').click();JSON.stringify({ok:true})",[](juce::var){});}
            else if(updateStage==5&&!bool(state["open"])&&!bool(state["inert"])&&bool(state["badge"])){std::cout<<"PASS actual native update notice -> notes link, download command, byte progress, cancellation, grey-gear manual checking, minimized badge and reopen\n";finish();}
        });
    }
    void timerCallback() override
    {
        const auto now=juce::Time::getMillisecondCounterHiRes();
        if(now-started>55000){finish("Real WebView bridge test timed out");return;}
        if(pending||!editor.browser)return;
        if(!installed)
        {
            evaluate(R"JS((()=>{
              if(typeof audioVisual==='undefined'||awaitingNativePower||!jelly.complete)return JSON.stringify({ready:false});
              if(!motion)document.getElementById('motion').click();if(!liquid)document.getElementById('liquid').click();
              window.__visualProbe={packets:0,received:0,rendered:0,pixelMean:0,geometryMean:0,shaderGain:0,pixelTests:0,frames:0,surges:0,above:false};
              window.__JUCE__.backend.addEventListener('wubrotAudio',d=>{const p=window.__visualProbe;p.packets++;p.received=Math.max(p.received,Number(d.intensity)||0);});
              const original=renderer.render.bind(renderer);renderer.render=(...args)=>{
                const result=original(...args),p=window.__visualProbe;p.frames++;p.rendered=Math.max(p.rendered,Number(args[4])||0);if(renderer.gl)p.shaderGain=Math.max(p.shaderGain,Number(renderer.gl.getUniform(renderer.material.p,renderer.material.uniforms.audio))||0);
                if(args[4]>.32&&!p.above){p.surges++;p.above=true;}else if(args[4]<.22)p.above=false;
                if(args[4]>.3&&p.pixelTests<3){
                  const read=()=>{if(renderer.gl){const a=new Uint8Array(640*640*4);renderer.gl.readPixels(0,0,640,640,renderer.gl.RGBA,renderer.gl.UNSIGNED_BYTE,a);return a;}paint(true);return ctx.getImageData(0,0,640,640).data;};
                  const live=read(),geometry=[...args];geometry[5]=0;original(...geometry);const geometric=read(),off=[...args];off[4]=off[5]=0;const keep=audioVisual.value;audioVisual.value=0;original(...off);const base=read();audioVisual.value=keep;original(...args);if(!renderer.gl)paint(true);
                  let delta=0,geometricDelta=0;for(let i=0;i<live.length;i++)if(i%4!==3){delta+=Math.abs(live[i]-base[i]);geometricDelta+=Math.abs(geometric[i]-base[i]);}p.pixelMean=Math.max(p.pixelMean,delta/(640*640*3));p.geometryMean=Math.max(p.geometryMean,geometricDelta/(640*640*3));p.pixelTests++;
                }return result;
              };
              return JSON.stringify({ready:true,hidden:document.hidden,motion,liquid,gpu:!!renderer.gl});
            })())JS",[this,now](juce::var v){if(static_cast<bool>(v["ready"])){std::cout<<"Real frontend ready "<<juce::JSON::toString(v,true)<<'\n';installed=true;stageStart=now;}});
            return;
        }
        // Exercise the real, unchanged HTML input -> native bridge -> APVTS route.
        if(driveBridgeStage<4)
        {
            constexpr float values[]{0,18,36,0};
            const float expected=values[driveBridgeStage];
            if(!driveBridgeSent)
            {
                evaluate("(()=>{const e=document.getElementById('drive');nativeEvent('gesture',{id:'drive',start:true});e.value="+juce::String(expected)+";e.dispatchEvent(new Event('input',{bubbles:true}));nativeEvent('gesture',{id:'drive',start:false});return JSON.stringify({ok:true});})()",[this](juce::var){driveBridgeSent=true;});
                return;
            }
            const float actual=processor.parameters.getRawParameterValue("drive")->load();
            auto* parameter=processor.parameters.getParameter("drive");
            if(std::abs(actual-expected)<.0001f&&std::abs(parameter->getValue()-expected/36)<.0001f)
            {
                driveBridgeValues.add(actual);++driveBridgeStage;driveBridgeSent=false;stageStart=now;
                if(driveBridgeStage==4)std::cout<<"PASS actual WebView DRIVE -> APVTS raw 0/18/36/0, normalized 0/0.5/1/0\n";
            }
            return;
        }
        if(mutateBridgeStage<5)
        {
            constexpr float values[]{0,25,50,75,100};const float expected=values[mutateBridgeStage];
            if(!mutateBridgeSent){
                evaluate("(()=>{const e=document.getElementById('mutate');nativeEvent('gesture',{id:'mutate',start:true});e.value="+juce::String(expected)+";e.dispatchEvent(new Event('input',{bubbles:true}));nativeEvent('gesture',{id:'mutate',start:false});return JSON.stringify({ok:true});})()",[this](juce::var){mutateBridgeSent=true;});return;
            }
            auto* parameter=processor.parameters.getParameter("mutate");
            if(std::abs(processor.parameters.getRawParameterValue("mutate")->load()-expected)<.0001f&&std::abs(parameter->getValue()-expected/100)<.0001f){
                mutateBridgeValues.add(expected);++mutateBridgeStage;mutateBridgeSent=false;stageStart=now;
                if(mutateBridgeStage==5)std::cout<<"PASS actual WebView MUTATE -> APVTS raw 0/25/50/75/100 and normalized 0/.25/.5/.75/1\n";
            }
            return;
        }
        if(visualFinished){if(!presetFinished)checkPresetBrowser();else if(!xyFinished)checkXYControls(now);else checkUpdates();return;}
        const double durations[]{900,1900,650,1900,650,900,1300};
        evaluate(R"JS(JSON.stringify({...window.__visualProbe,value:audioVisual.value,target:audioVisual.target,off:!engineIsOn(),x:px,y:py,frameRequest,receivedAge:performance.now()-audioVisual.received}))JS",[this,now,duration=durations[stage]](juce::var metrics){
            lastMetrics=metrics;
            if(now-stageStart<duration)return;
            std::cout<<"WebView phase "<<stage<<" native="<<editor.lastAudioLevel<<" actual="<<juce::JSON::toString(metrics,true)<<'\n';
            records.add(metrics);
            if(stage==0&&static_cast<double>(metrics["value"])>.001){finish("Silence moved the visual follower");return;}
            if(stage==1||stage==3||stage==6){if(static_cast<int>(metrics["packets"])<8||static_cast<double>(metrics["received"])<.35||static_cast<double>(metrics["rendered"])<.3||static_cast<double>(metrics["pixelMean"])<.8||static_cast<double>(metrics["geometryMean"])<.5||static_cast<int>(metrics["surges"])<2){finish("Audio was lost/suppressed in the actual native bridge or live render");return;}}
            if((stage==2||stage==4)&&static_cast<double>(metrics["value"])>.005){finish("Visual envelope failed to decay during silence");return;}
            if(stage==5&&(!static_cast<bool>(metrics["off"])||static_cast<int>(metrics["frameRequest"])!=0)){finish("Power OFF failed to freeze the actual WebView");return;}
            if(std::abs(static_cast<double>(metrics["x"]))>.0001||std::abs(static_cast<double>(metrics["y"]))>.0001){finish("Audio changed XY");return;}
            if(++stage==7){visualFinished=true;return;}
            const auto action=stage==5?"document.getElementById('enginePower').click();":stage==6?"document.getElementById('enginePower').click();":"";
            evaluate(juce::String("(()=>{")+action+"window.__visualProbe={packets:0,received:0,rendered:0,pixelMean:0,geometryMean:0,shaderGain:0,pixelTests:0,frames:0,surges:0,above:false};return JSON.stringify({ok:true});})()",[this](juce::var){input.store(stage==1||stage==5||stage==6?1:stage==3?2:0);stageStart=juce::Time::getMillisecondCounterHiRes();});
        });
    }
    WubrotProcessor& processor;WubrotEditor& editor;
    std::thread audio;std::atomic<bool>stop{false};std::atomic<int>input{0};
    bool xyFinished=false;int updateStage=-2;ReleaseFixture* releaseFixture=nullptr;juce::String updateNotes,updateVersion;std::unique_ptr<WubrotUpdates::Session> updateFixture;ProgressFixture* progressFixture=nullptr;
    bool presetFinished=false,xySent=false;int xyStage=0,xyAck=0;juce::MemoryBlock xyProject;juce::Array<juce::var> xyMetrics;
    bool visualFinished=false,presetProbeStarted=false;juce::var presetMetrics;
    bool pending=false,installed=false,driveBridgeSent=false,mutateBridgeSent=false;int mutateBridgeStage=0;juce::Array<juce::var> mutateBridgeValues;int driveBridgeStage=0;juce::Array<juce::var> driveBridgeValues;int stage=0;double started=0,stageStart=0;juce::String lastError;juce::var lastMetrics;
    std::shared_ptr<int> life=std::make_shared<int>(0);
    juce::File reportFile;juce::Array<juce::var> records;
};
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto docs=juce::File::createTempFile("-WubrotWebViewDocuments");docs.deleteFile();
    WubrotProcessor processor(docs);processor.clearSample();processor.prepareToPlay(48000,480);
    processor.userPresetDirectory().createDirectory();
    if(!processor.userPresetDirectory().getChildFile("Broken.wubpreset").replaceWithText("invalid preset fixture")){std::cerr<<"FAIL: could not create invalid preset fixture\n";return 1;}
    processor.refreshPresetLibrary();
    std::unique_ptr<juce::AudioProcessorEditor> owned(processor.createEditorIfNeeded());auto* editor=dynamic_cast<WubrotEditor*>(owned.get());
    if(!editor){std::cerr<<"FAIL: no native WebView editor\n";return 1;}
    juce::Component window;window.setBounds(20,20,804,697);window.addAndMakeVisible(*editor);window.addToDesktop(juce::ComponentPeer::windowHasTitleBar);window.setVisible(true);window.toFront(true);
    int result=1;{WubrotWebViewProbe probe(processor,*editor,argc>1?juce::File(argv[1]):juce::File());juce::MessageManager::getInstance()->runDispatchLoop();result=probe.result;}
    window.removeChildComponent(editor);window.removeFromDesktop();owned.reset();docs.deleteRecursively();return result;
}
