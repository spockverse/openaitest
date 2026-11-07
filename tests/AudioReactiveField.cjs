const { chromium } = require(process.env.WUBROT_PLAYWRIGHT || 'playwright');
const fs = require('fs'), path = require('path'), vm = require('vm'), assert = require('assert');
const html = fs.readFileSync(path.join(__dirname, '../prototype/WUBROT.html'), 'utf8');
const baseline = fs.readFileSync(path.join(__dirname, 'fixtures/GelRenderer-v031.js'), 'utf8');
const tracePath = process.env.WUBROT_VISUAL_TRACE || path.join(__dirname, '../build/VisualTelemetry.json');
const trace = JSON.parse(fs.readFileSync(tracePath, 'utf8'));
const followerSource = html.match(/<script id="audio-reactive-visual">([\s\S]*?)<\/script>/)[1];
const Follower = vm.runInNewContext(followerSource + ';AudioReactiveVisual');
const follower = new Follower();
follower.receive(1,0);follower.receive(0,2);assert(follower.advance(1/60,16)>.5,'A quiet packet erased an impact before the next frame');follower.reset();
for(let i=0;i<100;i++) assert.equal(follower.advance(1/60,i*1000/60),0);
follower.receive(1,0); assert(follower.advance(.016,16)>.5);
for(let i=1;i<60;i++) follower.advance(1/60,i*1000/60);
assert.equal(follower.value,0,'Stopped audio callbacks left a latched surge');
follower.receive(NaN,1000);assert.equal(follower.target,1);follower.receive(20,1000);assert.equal(follower.target,1);
follower.reset();for(let i=0;i<90;i++){follower.receive(.1,i*1000/30);follower.advance(1/30,i*1000/30);}
assert(Math.abs(follower.value-.1)<.0001,'Fresh sustained input lost its gentle continuous response');
async function open(b,width,height,fallback=false){
  const p=await b.newPage({viewport:{width,height},deviceScaleFactor:1.5,reducedMotion:'no-preference'});
  const errors=[];p.on('pageerror',e=>errors.push(e.message));
  await p.evaluate(fallback=>{
    if(fallback){const get=HTMLCanvasElement.prototype.getContext;HTMLCanvasElement.prototype.getContext=function(type,...args){return type==='webgl'?null:get.call(this,type,...args);};}
    window.listeners={};window.messages=[];window.state={enabled:1,x:0,y:0,mutate:67,drive:8.5,mode:0,regionStart:0,regionEnd:1,dnaSpeed:2,sampleRate:48000,status:'Synthetic bass visualization test.'};
    window.__JUCE__={backend:{addEventListener:(id,f)=>listeners[id]=f,emitEvent:(id,data)=>{messages.push(data);if(data.type==='ready')listeners.wubrotState(state);}}};
  },fallback);
  await p.setContent(html);await p.waitForFunction(()=>jelly.complete&&jelly.naturalWidth&&renderer.ready||jelly.complete&&jelly.naturalWidth&&!renderer.gl);
  assert.equal(await p.evaluate(()=>messages.filter(m=>m.type==='visualReady').length),1,'Frontend did not acknowledge installed listeners/initial power state');
  return {p,errors};
}
(async()=>{
  const b=await chromium.launch({...(process.env.WUBROT_CHROMIUM?{executablePath:process.env.WUBROT_CHROMIUM}:{}),args:['--no-sandbox']});
  try{
    for(const[width,height]of[[600,520],[804,697],[996,864]]){
      const{p,errors}=await open(b,width,height);await p.addScriptTag({content:baseline+';window.BaselineGelRenderer=BaselineGelRenderer;'});
      const metrics=await p.evaluate(()=>{
        cancelAnimationFrame(frameRequest);frameRequest=0;
        const old=new BaselineGelRenderer(document.createElement('canvas'));old.gl.canvas.width=old.gl.canvas.height=640;
        // Isolate approved material pixels; GridDeformation separately checks
        // the intentionally relocated targeting marks with grid ink enabled.
        const read=(r,gain=0,light=gain)=>{r.render(jelly,gel,2.75,1,gain,light,0);const v=new Uint8Array(640*640*4);r.gl.readPixels(0,0,640,640,r.gl.RGBA,r.gl.UNSIGNED_BYTE,v);return v;};
        let baselineDelta=0,changed=0,lightLift=0,alphaDelta=0,geometryMean=0;
        for(const[x,y]of[[0,0],[.72,-.4],[-1,0],[0,1]]){
          setXY(x,y);cancelAnimationFrame(frameRequest);frameRequest=0;membrane.x=px-.03;membrane.y=py+.02;gel.advance(.02);
          const original=read(old),zero=read(renderer),surge=read(renderer,1,0),lit=read(renderer,0,1);
          let lightTotal=0,geometryTotal=0;
          for(let i=0;i<original.length;i++){
            baselineDelta=Math.max(baselineDelta,Math.abs(original[i]-zero[i]));
            if(i%4===3){alphaDelta=Math.max(alphaDelta,Math.abs(zero[i]-surge[i]));continue;}
            if(zero[i]!==surge[i])changed++;
            geometryTotal+=Math.abs(surge[i]-zero[i]);
            lightTotal+=lit[i]-zero[i];
          }
          lightLift=Math.max(lightLift,lightTotal/(640*640*3));
          geometryMean=Math.max(geometryMean,geometryTotal/(640*640*3));
        }
        setXY(.3,-.2);gel.reset();membrane.x=px;membrane.y=py;cancelAnimationFrame(frameRequest);frameRequest=0;
        return{baselineDelta,changed,lightLift,alphaDelta,geometryMean};
      });
      assert.equal(metrics.baselineDelta,0,'Silence changed the previous shader render');
      assert(metrics.changed>10000,'Gain did not intensify existing surface deformation');
      assert(metrics.geometryMean>.8,'Ripple intensity changed too few visible pixels');
      assert(metrics.lightLift>.1&&metrics.lightLift<12,'Reflection response missing or excessive');
      assert.equal(metrics.alphaDelta,0,'Reactive deformation changed circular boundaries');
      let peak=0;
      for(const intensity of trace.intensities){await p.evaluate(intensity=>listeners.wubrotAudio({intensity}),intensity);await p.waitForTimeout(trace.stepMs);peak=Math.max(peak,await p.evaluate(()=>audioVisual.value));}
      assert(peak>.45,'Native synthetic-kick trace did not reach the visual follower');
      await p.waitForTimeout(350);assert.equal(await p.evaluate(()=>audioVisual.value),0);
      assert(await p.evaluate(()=>Math.abs(px-.3)<1e-9&&Math.abs(py+.2)<1e-9),'Audio moved the XY parameter');
      // Freeze an active surge; incoming audio must not light or move OFF material.
      await p.evaluate(()=>listeners.wubrotAudio({intensity:1}));await p.waitForTimeout(40);
      await p.locator('#enginePower').click();await p.waitForTimeout(300);
      const before=await p.evaluate(()=>({time:idleClock,boost:audioVisual.value,material:frozenSurface.toDataURL(),field:canvas.toDataURL(),anchor:[orbAnchor.style.left,orbAnchor.style.top]}));
      for(let i=0;i<8;i++){await p.evaluate(()=>listeners.wubrotAudio({intensity:1}));await p.waitForTimeout(20);}
      const after=await p.evaluate(()=>({time:idleClock,boost:audioVisual.value,material:frozenSurface.toDataURL(),field:canvas.toDataURL(),anchor:[orbAnchor.style.left,orbAnchor.style.top]}));
      assert.deepEqual(after,before,'Audio animated or relit powered-OFF field');assert.equal(await p.evaluate(()=>frameRequest),0);
      await p.locator('#enginePower').click();await p.waitForTimeout(180);assert.equal(await p.evaluate(()=>audioVisual.value),0,'Power ON replayed a stale impact');
      await p.evaluate(()=>{Object.defineProperty(document,'hidden',{configurable:true,get:()=>true});document.dispatchEvent(new Event('visibilitychange'));listeners.wubrotAudio({intensity:1});});
      assert.equal(await p.evaluate(()=>frameRequest),0);assert.equal(await p.evaluate(()=>audioVisual.value),0);
      await p.evaluate(()=>{Object.defineProperty(document,'hidden',{configurable:true,get:()=>false});document.dispatchEvent(new Event('visibilitychange'));});
      // Existing static/motion controls remain authoritative.
      await p.locator('#motion').click();await p.waitForTimeout(50);await p.evaluate(()=>listeners.wubrotAudio({intensity:1}));assert.equal(await p.evaluate(()=>audioVisual.value),0);
      await p.locator('#motion').click();
      if(process.env.WUBROT_SCREENSHOTS){await p.evaluate(()=>{audioVisual.receive(1,performance.now());});await p.waitForTimeout(40);await p.screenshot({path:path.join(process.env.WUBROT_SCREENSHOTS,`audio-reactive-${width}.png`)});}
      assert.deepEqual(errors,[]);console.log(width,'grid-disabled v031 material pixels identical; native kick peak',peak.toFixed(3),'reflection lift',metrics.lightLift.toFixed(3),'geometry pixel delta',metrics.geometryMean.toFixed(3),'boundary/XY, decay, OFF, hidden and motion passed');await p.close();
    }
    const{p,errors}=await open(b,804,697,true);
    const fallback=await p.evaluate(()=>{cancelAnimationFrame(frameRequest);frameRequest=0;idleClock=2.75;audioVisual.reset();paint(true);const zero=Array.from(fallbackPoints);audioVisual.value=1;paint(true);let delta=0;for(let i=0;i<zero.length;i++)delta+=Math.abs(zero[i]-fallbackPoints[i]);return{delta,gpu:!!renderer.gl};});
    assert.equal(fallback.gpu,false);assert(fallback.delta>10,'Canvas fallback did not scale its existing displacement');assert.deepEqual(errors,[]);await p.close();
    console.log('Canvas fallback preserves existing membrane mesh and responds to the same intensity');
  }finally{await b.close();}
})().catch(e=>{console.error(e);process.exit(1)});
