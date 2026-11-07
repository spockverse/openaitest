// Real JUCE/WebView2 bridge check. The probe is injected by this test executable,
// never included in the plugin HTML or displayed as a diagnostic panel.
#include "PluginEditor.h"
#include <chrono>
#include <iostream>
#include <thread>

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
    ~WubrotWebViewProbe() override {life.reset();stopTimer();stop.store(true);if(audio.joinable())audio.join();}
    int result=1;
private:
    void evaluate(const juce::String& script,std::function<void(juce::var)> callback)
    {
        pending=true;
        editor.browser->evaluateJavascript(script,[this,guard=std::weak_ptr<int>(life),callback=std::move(callback)](juce::WebBrowserComponent::EvaluationResult r){if(guard.expired())return;pending=false;if(const auto* value=r.getResult())callback(juce::JSON::parse(value->toString()));else if(const auto* error=r.getError())lastError=error->message;});
    }
    void finish(const juce::String& error={})
    {
        if(reportFile!=juce::File()) { auto report=std::make_unique<juce::DynamicObject>();report->setProperty("error",error);report->setProperty("evaluationError",lastError);report->setProperty("nativeReady",editor.visualBridgeReady);report->setProperty("nativeLevel",editor.lastAudioLevel);report->setProperty("phases",records);report->setProperty("latest",lastMetrics);reportFile.getParentDirectory().createDirectory();reportFile.replaceWithText(juce::JSON::toString(juce::var(report.release()),true)); }
        stopTimer();if(error.isEmpty()){std::cout<<"PASS: actual processBlock -> native timer -> JUCE WebView2 -> live shader/canvas; low/mid hits, silence, power and unchanged XY\n";result=0;}else std::cerr<<"FAIL: "<<error<<" last evaluation: "<<lastError<<" native ready="<<editor.visualBridgeReady<<" forwarded="<<editor.lastAudioLevel<<" seq="<<editor.lastAudioSequence<<" ticks="<<editor.uiTicks<<" frontend="<<juce::JSON::toString(lastMetrics,true)<<'\n';
        juce::MessageManager::getInstance()->stopDispatchLoop();
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
                const result=original(...args),p=window.__visualProbe;p.frames++;p.rendered=Math.max(p.rendered,Number(args[4])||0);if(renderer.gl)p.shaderGain=Math.max(p.shaderGain,Number(renderer.gl.getUniform(renderer.program,renderer.reactiveGain))||0);
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
            if(++stage==7){finish();return;}
            const auto action=stage==5?"document.getElementById('enginePower').click();":stage==6?"document.getElementById('enginePower').click();":"";
            evaluate(juce::String("(()=>{")+action+"window.__visualProbe={packets:0,received:0,rendered:0,pixelMean:0,geometryMean:0,shaderGain:0,pixelTests:0,frames:0,surges:0,above:false};return JSON.stringify({ok:true});})()",[this](juce::var){input.store(stage==1||stage==5||stage==6?1:stage==3?2:0);stageStart=juce::Time::getMillisecondCounterHiRes();});
        });
    }
    WubrotProcessor& processor;WubrotEditor& editor;
    std::thread audio;std::atomic<bool>stop{false};std::atomic<int>input{0};
    bool pending=false,installed=false;int stage=0;double started=0,stageStart=0;juce::String lastError;juce::var lastMetrics;
    std::shared_ptr<int> life=std::make_shared<int>(0);
    juce::File reportFile;juce::Array<juce::var> records;
};
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    WubrotProcessor processor;processor.clearSample();processor.prepareToPlay(48000,480);
    std::unique_ptr<juce::AudioProcessorEditor> owned(processor.createEditorIfNeeded());auto* editor=dynamic_cast<WubrotEditor*>(owned.get());
    if(!editor){std::cerr<<"FAIL: no native WebView editor\n";return 1;}
    juce::Component window;window.setBounds(20,20,804,697);window.addAndMakeVisible(*editor);window.addToDesktop(juce::ComponentPeer::windowHasTitleBar);window.setVisible(true);window.toFront(true);
    int result=1;{WubrotWebViewProbe probe(processor,*editor,argc>1?juce::File(argv[1]):juce::File());juce::MessageManager::getInstance()->runDispatchLoop();result=probe.result;}
    window.removeChildComponent(editor);window.removeFromDesktop();owned.reset();return result;
}
