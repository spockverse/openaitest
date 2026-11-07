const {chromium}=require(process.env.WUBROT_PLAYWRIGHT||'playwright');
const fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(path.join(__dirname,'../prototype/WUBROT.html'),'utf8');
const frozen=fs.readFileSync(path.join(__dirname,'fixtures/GelRenderer-v035.js'),'utf8');
assert(!/gridPosition|targetingInk|gridAmount|paintFallbackGuides/.test(html),'Deformed/following grid code remains in the plugin');
const positions=[[0,0],[-1,0],[1,0],[0,-1],[0,1],[-Math.SQRT1_2,-Math.SQRT1_2],[Math.SQRT1_2,-Math.SQRT1_2],[-Math.SQRT1_2,Math.SQRT1_2],[Math.SQRT1_2,Math.SQRT1_2],[.4,-.2],[-.6,.35],[.92,.2]];
async function open(b,width,height,fallback=false){
 const p=await b.newPage({viewport:{width,height},deviceScaleFactor:1.5});const errors=[];p.on('pageerror',e=>errors.push(e.message));
 await p.evaluate(fallback=>{if(fallback){const get=HTMLCanvasElement.prototype.getContext;HTMLCanvasElement.prototype.getContext=function(type,...args){return type==='webgl'?null:get.call(this,type,...args);};}window.listeners={};window.__JUCE__={backend:{emitEvent(){},addEventListener:(id,f)=>listeners[id]=f}};},fallback);
 await p.setContent(html);await p.evaluate(()=>listeners.wubrotState({enabled:1,x:0,y:0,mutate:67,drive:8.5,mode:0,regionStart:0,regionEnd:1,dnaSpeed:2,sampleRate:48000,status:'Ready.'}));
 await p.waitForFunction(()=>jelly.complete&&jelly.naturalWidth);return{p,errors};
}
(async()=>{const b=await chromium.launch({...(process.env.WUBROT_CHROMIUM?{executablePath:process.env.WUBROT_CHROMIUM}:{}),args:['--no-sandbox']});try{
 for(const[width,height]of[[600,520],[804,697],[996,864]])for(const fallback of[false,true]){
  const{p,errors}=await open(b,width,height,fallback);
  await p.addScriptTag({content:frozen+';window.MaterialRendererV035=MaterialRendererV035;'});
  await p.evaluate(()=>{
   window.guideSnapshot=targetingGrid.toDataURL();window.gridDraws=[];
   const draw=ctx.drawImage.bind(ctx);ctx.drawImage=(image,...args)=>{if(image===targetingGrid)gridDraws.push({args,matrix:Array.from(ctx.getTransform().toFloat64Array())});return draw(image,...args);};
   if(renderer.gl){window.materialReference=new MaterialRendererV035(document.createElement('canvas'));materialReference.gl.canvas.width=materialReference.gl.canvas.height=640;}
  });
  // Independently rasterize the specified four centered rings and crosshairs.
  const grid=await p.evaluate(()=>{
   const expected=document.createElement('canvas');expected.width=expected.height=640;const g=expected.getContext('2d');
   for(const[radius,width,alpha]of[[103,.8,.47],[167,.8,.47],[205,1.7,.69],[238,.8,.47]]){g.lineWidth=width;g.strokeStyle=`rgba(232,227,242,${alpha})`;g.beginPath();g.ellipse(320,320,radius,radius,0,0,Math.PI*2);g.stroke();}
   g.lineWidth=.8;g.strokeStyle='rgba(232,227,242,.5)';g.beginPath();g.moveTo(25,320);g.lineTo(615,320);g.moveTo(320,25);g.lineTo(320,615);g.stroke();
   g.fillStyle='rgb(232,227,242)';for(const[x,y]of[[25,320],[615,320],[320,25],[320,615]]){g.beginPath();g.ellipse(x,y,1.5,1.5,0,0,Math.PI*2);g.fill();}
   const a=g.getImageData(0,0,640,640).data,b=targetingGrid.getContext('2d').getImageData(0,0,640,640).data;
   let delta=0,ink=0;for(let i=0;i<a.length;i++){delta=Math.max(delta,Math.abs(a[i]-b[i]));if(i%4===3&&b[i])ink++;}
   return{delta,ink};
  });
  assert.equal(grid.delta,0,'Grid lost its fixed centered geometry, line weights or opacity');assert(grid.ink>7000);
  for(let index=0;index<positions.length;index++)for(const boost of[0,1]){
   const result=await p.evaluate(({xy,index,boost})=>{
    setXY(...xy,false);cancelAnimationFrame(frameRequest);frameRequest=0;gel.reset();gel.impulse(-.3,.1,.4,-.2,.08);gel.advance(.06);
    membrane.x=px-.025;membrane.y=py+.015;idleClock=2.75+index*.1;audioVisual.value=boost;
    let materialDelta=0;
    if(renderer.gl){const read=r=>{r.render(jelly,gel,idleClock,1,boost,boost,0);const gl=r.gl,a=new Uint8Array(640*640*4);gl.readPixels(0,0,640,640,gl.RGBA,gl.UNSIGNED_BYTE,a);return a;};const reference=read(materialReference),current=read(renderer);for(let i=0;i<reference.length;i++)materialDelta=Math.max(materialDelta,Math.abs(reference[i]-current[i]));}
    gridDraws.length=0;paint(!renderer.gl);
    const rect=canvas.getBoundingClientRect();
    return{materialDelta,gridSame:targetingGrid.toDataURL()===guideSnapshot,draws:gridDraws,xy:[px,py],anchor:[parseFloat(orbAnchor.style.left),parseFloat(orbAnchor.style.top)],rect:{width:rect.width,height:rect.height},scroll:scrollX||scrollY};
   },{xy:positions[index],index,boost});
   assert.equal(result.materialDelta,0,'Stationary-grid correction changed the approved idle/audio jelly material');
   assert(result.gridSame,'XY/time/audio changed grid pixels');assert.equal(result.draws.length,1,'Grid must be composited once, above either material renderer');
   assert.deepEqual(result.draws[0],{args:[0,0],matrix:[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]},'Grid was translated, scaled or distorted independently of the field');
   assert(Math.abs(result.rect.width-result.rect.height)<.01,'Grid canvas stretched at this editor size');assert.equal(result.scroll,0);
   for(let axis=0;axis<2;axis++){assert(Math.abs(result.xy[axis]-positions[index][axis])<1e-8);assert(Math.abs(result.anchor[axis]-(50+positions[index][axis]*42.1875))<.001,'Orb tracking changed');}
   if(process.env.WUBROT_SCREENSHOTS&&boost===0&&[0,1,2,5,8].includes(index))await p.screenshot({path:path.join(process.env.WUBROT_SCREENSHOTS,`stationary-grid-${width}-${fallback?'canvas':'gpu'}-${index}.png`)});
  }
  // The existing interaction canvas still receives actual pointer input.
  const rect=await p.locator('#field').boundingBox();await p.mouse.move(rect.x+rect.width*.5,rect.y+rect.height*.5);await p.mouse.down();await p.mouse.move(rect.x+rect.width*.65,rect.y+rect.height*.4);await p.mouse.up();
  assert(await p.evaluate(()=>Math.abs(px-.15/.421875)<.01&&Math.abs(py+.1/.421875)<.01),'Fixed overlay blocked XY dragging');
  await p.locator('#enginePower').click();await p.waitForTimeout(300);
  const readFrozen=()=>p.evaluate(()=>({grid:targetingGrid.toDataURL(),material:frozenSurface.toDataURL(),field:canvas.toDataURL(),clock:idleClock,frames:frameRequest}));const before=await readFrozen();
  await p.evaluate(()=>listeners.wubrotAudio({intensity:1}));await p.waitForTimeout(100);assert.deepEqual(await readFrozen(),before,'OFF field moved or flashed');assert.equal(before.frames,0);
  assert.deepEqual(errors,[]);console.log(width,fallback?'Canvas':'WebGL','12 XY positions × idle/audio: stationary centered guides, unchanged material, orb input and OFF freeze passed');await p.close();
 }
 }finally{await b.close();}})().catch(e=>{console.error(e);process.exit(1)});
