const {chromium}=require(process.env.WUBROT_PLAYWRIGHT||'playwright');
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../prototype/WUBROT.html'),'utf8');
const frozen=fs.readFileSync(path.join(__dirname,'fixtures/GelRenderer-v033.js'),'utf8');
// Read the real shader's material and bounded guide coordinates independently.
// Check original idle pixels, the separate 5/6 px limits, and the guide strokes.
const rendererSource=html.match(/<script id="gel-renderer">([\s\S]*?)<\/script>/)[1];
const probe=rendererSource.replace('class GelRenderer','class SurfaceUVProbe')
 .replace('gl_FragColor=vec4(colour,alpha);',`vec2 q=floor(clamp(source,vec2(0.),vec2(1.))*65535.+.5);
 gl_FragColor=vec4(floor(q.x/256.),mod(q.x,256.),floor(q.y/256.),mod(q.y,256.))/255.;`);
const gridProbe=rendererSource.replace('class GelRenderer','class GridCoordinateProbe').replace('gl_FragColor=vec4(colour,alpha);',`vec2 q=floor(clamp(.5+gridPosition(p,refraction),vec2(0.),vec2(1.))*65535.+.5);gl_FragColor=vec4(floor(q.x/256.),mod(q.x,256.),floor(q.y/256.),mod(q.y,256.))/255.;`);
const positions=[[0,0],[-1,0],[1,0],[0,-1],[0,1],[-Math.SQRT1_2,-Math.SQRT1_2],[Math.SQRT1_2,-Math.SQRT1_2],[-Math.SQRT1_2,Math.SQRT1_2],[Math.SQRT1_2,Math.SQRT1_2],[.4,-.2],[-.6,.35],[.92,.2]];
async function open(b,width,height,fallback=false){
 const p=await b.newPage({viewport:{width,height},deviceScaleFactor:1.5});const errors=[];p.on('pageerror',e=>errors.push(e.message));
 await p.evaluate(fallback=>{if(fallback){const get=HTMLCanvasElement.prototype.getContext;HTMLCanvasElement.prototype.getContext=function(type,...args){if(type==='webgl'&&fallback===true)return null;const context=get.call(this,type,...args);if(type==='webgl'&&context&&fallback==='no-derivatives'){const ext=context.getExtension.bind(context);context.getExtension=name=>name==='OES_standard_derivatives'?null:ext(name);}return context;};}window.listeners={};window.__JUCE__={backend:{emitEvent(){},addEventListener:(id,f)=>listeners[id]=f}};},fallback);
 await p.setContent(html);await p.evaluate(()=>listeners.wubrotState({enabled:1,x:0,y:0,mutate:67,drive:8.5,mode:0,regionStart:0,regionEnd:1,dnaSpeed:2,sampleRate:48000,status:'Ready.'}));
 await p.waitForFunction(()=>jelly.complete&&jelly.naturalWidth);return{p,errors};
}
(async()=>{const b=await chromium.launch({...(process.env.WUBROT_CHROMIUM?{executablePath:process.env.WUBROT_CHROMIUM}:{}),args:['--no-sandbox']});try{
 for(const[width,height]of[[600,520],[804,697],[996,864]]){
  const{p,errors}=await open(b,width,height);await p.addScriptTag({content:frozen+';window.MaterialRendererV033=MaterialRendererV033;'+probe+';window.SurfaceUVProbe=SurfaceUVProbe;'+gridProbe+';window.GridCoordinateProbe=GridCoordinateProbe;'});
  await p.evaluate(()=>{window.materialReference=new MaterialRendererV033(document.createElement('canvas'));window.uvProbe=new SurfaceUVProbe(document.createElement('canvas'));window.guideProbe=new GridCoordinateProbe(document.createElement('canvas'));for(const r of[materialReference,uvProbe,guideProbe])r.gl.canvas.width=r.gl.canvas.height=640;});
  let largestErrorRate=0,totalMarked=0;
  for(let index=0;index<positions.length;index++)for(const boost of[0,1]){
   const metrics=await p.evaluate(({xy,boost,index})=>{
    setXY(...xy,false);gel.reset();gel.impulse(-.3,.1,.4,-.2,.08);gel.advance(.06);membrane.x=px-.025;membrane.y=py+.015;idleClock=2.75+index*.1;audioVisual.value=boost;
    cancelAnimationFrame(frameRequest);frameRequest=0;
    const read=(r,grid,strength=boost)=>{r.render(jelly,gel,idleClock,1,strength,strength,grid);const a=new Uint8Array(640*640*4);r.gl.readPixels(0,0,640,640,r.gl.RGBA,r.gl.UNSIGNED_BYTE,a);return a;};
    const reference=read(materialReference,0,0),rest=read(renderer,0,0),base=read(renderer,0),surface=read(guideProbe,0),materialRest=read(uvProbe,0,0),materialLive=read(uvProbe,0),marked=read(renderer,1);
    let baseDelta=0,alphaDelta=0,changed=0,errors=0,tested=0,maxBend=0,maxAudioPush=0;
    for(let i=0;i<base.length;i++){baseDelta=Math.max(baseDelta,Math.abs(rest[i]-reference[i]));if(i%4===3)alphaDelta=Math.max(alphaDelta,Math.abs(base[i]-marked[i]));else if(base[i]!==marked[i])changed++;}
    const scale=640,coords=new Float32Array(640*640*2),radius=new Float32Array(640*640);
    for(let i=0;i<640*640;i++){const j=i*4;const x=((surface[j]*256+surface[j+1])/65535-.5)*scale,y=((surface[j+2]*256+surface[j+3])/65535-.5)*scale;coords[i*2]=x;coords[i*2+1]=y;radius[i]=Math.hypot(x-px*270,y-py*270);
     if(base[j+3]>253){const sx=(i%640)+.5-320,sy=320-(Math.floor(i/640)+.5);maxBend=Math.max(maxBend,Math.hypot(x-sx,y-sy));const decode=(a,k)=>(a[k]*256+a[k+1])/65535;maxAudioPush=Math.max(maxAudioPush,Math.hypot(decode(materialLive,j)-decode(materialRest,j),decode(materialLive,j+2)-decode(materialRest,j+2))*640);}}
    const stroke=(distance,pixel,width)=>Math.max(0,Math.min(1,.5+.5*width-distance/Math.max(pixel,.0001)));
    for(let y=2;y<638;y+=2)for(let x=2;x<638;x+=2){
     const i=y*640+x,j=i*4;if(base[j+3]<254)continue;
     const ex=x&~1,ey=y&~1,left=y*640+ex,right=left+1,bottom=ey*640+x,top=bottom+640;
     const gx=coords[i*2],gy=coords[i*2+1],r=radius[i];
     const pr=Math.hypot(radius[right]-radius[left],radius[top]-radius[bottom]);
     const px=Math.hypot(coords[right*2]-coords[left*2],coords[top*2]-coords[bottom*2]);
     const py=Math.hypot(coords[right*2+1]-coords[left*2+1],coords[top*2+1]-coords[bottom*2+1]);
     // Ignore isolated derivative singularities, where 16-bit UV readback is
     // insufficient to reconstruct subpixel stroke coverage independently.
     if(Math.min(pr,px,py)<.04)continue;
     const rings=Math.max(...[103,167,238].map(a=>stroke(Math.abs(r-a),pr,.8)))*.47;
     const major=stroke(Math.abs(r-205),pr,1.7)*.69;
     const extent=a=>{const t=Math.max(0,Math.min(1,(Math.abs(a)-294)/2));return 1-t*t*(3-2*t);};
     const guides=Math.max(stroke(Math.abs(gy),py,.8)*extent(gx),stroke(Math.abs(gx),px,.8)*extent(gy))*.50;
     // Rim dots occupy negligible area and use their own distance derivative.
     if(Math.min(Math.hypot(Math.abs(gx)-295,Math.abs(gy)),Math.hypot(Math.abs(gx),Math.abs(gy)-295))<8)continue;
     const ink=Math.max(rings,major,guides);let error=0;
     for(let c=0;c<3;c++)error=Math.max(error,Math.abs(marked[j+c]-(base[j+c]*(1-ink)+[.91,.89,.95][c]*255*ink)));
     tested++;if(error>4)errors++;
    }
    paint(false);return{baseDelta,alphaDelta,changed,errorRate:errors/tested,errors,tested,xy:[px,py],maxBend,maxAudioPush,anchor:[orbAnchor.style.left,orbAnchor.style.top]};
   },{xy:positions[index],boost,index});
   assert.equal(metrics.baseDelta,0,'Grid work changed the approved jelly material/deformation');
   assert(metrics.maxBend<=5.04,`Guide bend exceeded the five-pixel limit: ${metrics.maxBend}`);assert(metrics.maxAudioPush<=6.04,`Audio exceeded its six-pixel limit: ${metrics.maxAudioPush}`);
   assert.equal(metrics.alphaDelta,0,'Grid changed circular boundary alpha');assert(metrics.changed>1000,'Targeting markings are missing');
   assert(metrics.errorRate<.002,`Grid detached from sampled material UV: ${JSON.stringify(metrics)}`);
   largestErrorRate=Math.max(largestErrorRate,metrics.errorRate);totalMarked+=metrics.changed;
   assert(Math.abs(metrics.xy[0]-positions[index][0])<1e-8&&Math.abs(metrics.xy[1]-positions[index][1])<1e-8);
   assert(await p.evaluate(()=>scrollX===0&&scrollY===0&&document.querySelector('.shell').getBoundingClientRect().top===0),'Rendering shifted the fixed editor');
   if(process.env.WUBROT_SCREENSHOTS&&boost===0&&(width===804||index===0||index===6))await p.screenshot({path:path.join(process.env.WUBROT_SCREENSHOTS,`grid-${width}-${index}.png`)});
  }
  // The shader updates markings while the orb overlay remains untouched.
  await p.evaluate(()=>{setXY(.3,-.2,false);cancelAnimationFrame(frameRequest);frameRequest=0;idleClock=4;paint(false);renderer.render(jelly,gel,idleClock,1,1);});
  await p.locator('#enginePower').click();await p.waitForTimeout(300);const before=await p.evaluate(()=>({material:frozenSurface.toDataURL(),field:canvas.toDataURL(),clock:idleClock}));
  await p.evaluate(()=>listeners.wubrotAudio({intensity:1}));await p.waitForTimeout(80);assert.deepEqual(await p.evaluate(()=>({material:frozenSurface.toDataURL(),field:canvas.toDataURL(),clock:idleClock})),before);
  assert.deepEqual(errors,[]);console.log(width,'all 12 XY positions × idle/audio: original idle pixels, bounded grid/audio displacement; coordinate error rate',largestErrorRate,'marked channels',totalMarked,'boundary/orb/OFF passed');await p.close();
 }
 const{p,errors}=await open(b,804,697,true);
 for(const xy of positions){const same=await p.evaluate(xy=>{setXY(...xy,false);cancelAnimationFrame(frameRequest);frameRequest=0;idleClock=3;audioVisual.value=.8;paint(true,false);const geometry=Array.from(fallbackPoints),source=Array.from(fallbackSources);paint(true,true);return{geometry:geometry.every((v,i)=>v===fallbackPoints[i]),source:source.every((v,i)=>v===fallbackSources[i]),marked:typeof paintFallbackGuides==='function'};},xy);assert.deepEqual(same,{geometry:true,source:true,marked:true});}
 if(process.env.WUBROT_SCREENSHOTS)await p.screenshot({path:path.join(process.env.WUBROT_SCREENSHOTS,'grid-canvas-fallback.png')});assert.deepEqual(errors,[]);await p.close();console.log('Fallback guides read the same bounded surface displacement without changing its material mesh');
 const limited=await open(b,804,697,'no-derivatives');
 const result=await limited.p.evaluate(()=>{cancelAnimationFrame(frameRequest);frameRequest=0;if(!renderer.gl)return{gpu:false};const gl=renderer.gl,read=grid=>{renderer.render(jelly,gel,2.75,1,.8,.8,grid);const a=new Uint8Array(640*640*4);gl.readPixels(0,0,640,640,gl.RGBA,gl.UNSIGNED_BYTE,a);return a;};const base=read(0),marked=read(1);return{gpu:true,changed:base.reduce((n,v,i)=>n+(v!==marked[i]),0),error:gl.getError()};});
 assert(result.gpu&&result.changed>1000&&result.error===0);assert.deepEqual(limited.errors,[]);await limited.p.close();console.log('Missing derivative extension preserves WebGL material and shared-coordinate markings');
 }finally{await b.close();}})().catch(e=>{console.error(e);process.exit(1)});
