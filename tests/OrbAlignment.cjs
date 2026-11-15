const{chromium}=require(process.env.WUBROT_PLAYWRIGHT||'playwright');const fs=require('fs'),path=require('path'),assert=require('assert');
async function checkCoordinates(p){
 const host=async(x,y,extra={})=>p.evaluate(({x,y,extra})=>{messages=[];listeners.wubrotState({enabled:1,x,y,mutate:100,drive:0,mode:0,regionStart:0,regionEnd:1,dnaSpeed:.8,dnaRhythm:2,dnaSync:true,dnaPitch:0,sampleRate:48000,status:'Ready.',xyWriteAck:xyWriteSequence,...extra});},{x,y,extra});
 const values=async(x,y)=>{assert.deepEqual(await p.evaluate(()=>[$('xyX').value,$('xyY').value]),[x,y]);assert.deepEqual(await p.evaluate(()=>[$('xyXSlider').getAttribute('aria-valuetext'),$('xyYSlider').getAttribute('aria-valuetext')]),[x+'%',y+'%']);};
 const edit=async(id,value,key='Enter')=>{await p.locator('#xy'+id.toUpperCase()).fill(value);await p.locator('#xy'+id.toUpperCase()).press(key);};
 const params=async()=>p.evaluate(()=>messages.filter(e=>e.type==='parameter'&&(e.id==='x'||e.id==='y')));
 const gestures=async(id)=>assert.deepEqual(await p.evaluate(id=>messages.filter(e=>e.type==='gesture'&&e.id===id).map(e=>e.start),id),[true,false]);
 await host(0,0);await values('50.0','50.0');assert.equal((await params()).length,0);
 await edit('x','75.0');await values('75.0','50.0');let changed=await params();assert.equal(changed.length,1);assert.equal(changed[0].id,'x');assert(Math.abs(changed[0].value-.5)<1e-7);await gestures('x');
 await p.evaluate(()=>{messages=[];listeners.wubrotState({x:0,y:0,xyWriteAck:xyWriteSequence-1});});await values('75.0','50.0');assert.equal((await params()).length,0,'Stale native snapshot echoed values back');
 await host(.5,0);await edit('y','60.0');await values('75.0','60.0');changed=await params();assert.equal(changed.length,1);assert.equal(changed[0].id,'y');assert(Math.abs(changed[0].value+.2)<1e-7);await gestures('y');
 await host(.5,-.2);await edit('x','75.0');assert.equal((await params()).length,0,'Unchanged entry sent duplicate parameter updates');
 await edit('x','33.3','Escape');await values('75.0','60.0');assert.equal((await params()).length,0,'Escape applied draft coordinates');
 for(const invalid of ['', 'NaN','Infinity','-Infinity','1e309','75oops','0x20','1.2.3']){await edit('x',invalid);await values('75.0','60.0');assert.equal((await params()).length,0,'Invalid input reached native parameters');assert.equal(await p.locator('#xyX').getAttribute('aria-invalid'),'true');}
 await host(0,0);await edit('x','120');await values('100.0','50.0');assert.equal((await params())[0].value,1);
 await host(0,0);await edit('y','-20');await values('50.0','0.0');assert.equal((await params())[0].value,1);
 await host(0,0);await edit('x','66,6');await values('66.6','50.0');assert(Math.abs((await params())[0].value-.332)<1e-7);
 await host(0,0);await edit('x','70.0%');await values('70.0','50.0');await p.evaluate(()=>messages=[]);await p.locator('#xyY').fill('65.0');await p.locator('#field').focus();await values('70.0','65.0');await gestures('y');
 // Host automation is authoritative, including independent axes and the existing circular projection.
 for(const [x,y]of [[.25,0],[.25,-.5],[-.4,.6],[1,1]]){await host(x,y);const r=Math.max(1,Math.hypot(x,y));await values(((1+x/r)*50).toFixed(1),((1-y/r)*50).toFixed(1));assert.equal((await params()).length,0,'Host update created an automation feedback loop');}
 await host(.2,-.4);await p.locator('#xyX').fill('81.2');await host(.3,-.6);assert.equal(await p.locator('#xyX').inputValue(),'81.2','Host updates overwrote an in-progress numeric draft');assert.equal(await p.locator('#xyY').inputValue(),'80.0');await p.locator('#xyX').press('Escape');await values('65.0','80.0');
 await host(0,0);
 for(const axis of ['x','y']){
  const slider=p.locator('#xy'+axis.toUpperCase()+'Slider'),b=await slider.boundingBox();
  await p.mouse.move(b.x+b.width/2,b.y+b.height/2);await p.mouse.down();await p.mouse.move(b.x+b.width/2,b.y+b.height*.22,{steps:8});await p.mouse.up();
  assert(await p.evaluate(axis=>coordinatePercent(axis)>70,axis),'Vertical fader drag did not increase the coordinate');await gestures(axis);
  const sent=await params();assert(sent.length>0&&sent.every(e=>e.id===axis),'A fader changed an unrelated axis away from the rim');await host(0,0);
  await slider.focus();await p.keyboard.press('ArrowUp');assert(await p.evaluate(axis=>Math.abs(coordinatePercent(axis)-50.1)<1e-6,axis));await gestures(axis);await host(0,0);
 }
 // Host automation on the untouched axis continues while its partner is held.
 await host(0,0);const slider=await p.locator('#xyXSlider').boundingBox();await p.mouse.move(slider.x+slider.width/2,slider.y+slider.height/2);await p.mouse.down();
 await p.evaluate(()=>listeners.wubrotState({x:0,y:-.2,xyWriteAck:xyWriteSequence}));assert.equal(await p.locator('#xyY').inputValue(),'60.0','A touched X fader blocked Y automation');
 await p.mouse.move(slider.x+slider.width/2,slider.y+slider.height*.3,{steps:5});await p.mouse.up();await gestures('x');assert((await params()).every(e=>e.id==='x'),'Y host automation was echoed back while touching X');
 // OFF retains exact values and interactivity with unlit grey controls.
 await host(.2,-.4,{enabled:0});await values('60.0','70.0');
 assert.equal(await p.locator('#xyXTrack').evaluate(e=>getComputedStyle(e,':after').boxShadow),'none');
 assert(await p.locator('.xy-coordinate-value').first().evaluate(e=>{const c=getComputedStyle(e).color.match(/[\d.]+/g).map(Number);return c[0]===c[1]&&Math.abs(c[1]-c[2])<10;}));
 await edit('x','80.0');await values('80.0','70.0');assert((await params()).length>0);assert.equal(await p.evaluate(()=>frameRequest),0,'OFF controls restarted decorative animation');
 await host(0,0);assert(await p.evaluate(()=>nativeTouches.size===0),'XY gesture remained open');
}

(async()=>{const b=await chromium.launch({...(process.env.WUBROT_CHROMIUM?{executablePath:process.env.WUBROT_CHROMIUM}:{}),args:['--no-sandbox']});try{for(const dpr of[1,1.25,1.5,2])for(const[width,height]of[[600,520],[804,697],[996,864]]){
 const p=await b.newPage({viewport:{width,height},deviceScaleFactor:dpr});const errors=[];p.on('pageerror',e=>errors.push(e.message));
 await p.evaluate(()=>{window.listeners={};window.messages=[];window.__JUCE__={backend:{emitEvent:(id,event)=>messages.push(event),addEventListener:(id,f)=>listeners[id]=f}}});await p.setContent(fs.readFileSync(path.join(__dirname,'../prototype/WUBROT.html'),'utf8'));await p.evaluate(()=>listeners.wubrotState({enabled:1,x:0,y:0,mutate:67,drive:8.5,mode:0,regionStart:0,regionEnd:1,dnaSpeed:2,sampleRate:48000,status:'Ready.'}));
 const field=await p.locator('#field').boundingBox();
 const panel=await p.locator('.field').boundingBox(),rail=await p.locator('.xy-coordinates').boundingBox();
 assert(rail.x>=field.x+field.width+2&&rail.x+rail.width<=panel.x+panel.width&&rail.y>=panel.y&&rail.y+rail.height<=panel.y+panel.height,'Coordinate faders overlap the jelly or panel border');
 for(const axis of ['X','Y'])assert(await p.locator('#xy'+axis).evaluate(e=>e.scrollWidth<=e.clientWidth),'Percentage entry clips');
 if(dpr===1.5)await checkCoordinates(p);

 for(const motion of[true,false]){
  await p.evaluate(on=>{motion=on;syncButtons();schedule()},motion);
  for(const[x,y]of[[0,0],[-1,0],[1,0],[0,-1],[0,1],[.707,.707],[-.707,-.707],[.72,-.44]]){
   await p.evaluate(([x,y])=>setXY(x,y,false),[x,y]);await p.waitForFunction(()=>{const point=orbPoint();return Math.abs(parseFloat(orbAnchor.style.left)-point.x/640*100)<.0001&&Math.abs(parseFloat(orbAnchor.style.top)-point.y/640*100)<.0001});
   const geometry=await p.evaluate(()=>{const r=canvas.getBoundingClientRect(),a=orbAnchor.getBoundingClientRect(),point=orbPoint();const expected=[r.left+point.x/640*r.width,r.top+point.y/640*r.height];return{expected,anchor:[a.x+a.width/2,a.y+a.height/2],svg:Array.from(aura.querySelectorAll('.solar-rim svg')).map(s=>{const m=s.getScreenCTM();return[m.a*100+m.c*100+m.e,m.b*100+m.d*100+m.f]}),roots:Array.from(aura.querySelectorAll('.solar-flare svg>g>path:first-child')).flatMap(path=>{const m=path.getScreenCTM();return[0,path.getTotalLength()].map(t=>{const q=path.getPointAtLength(t);return[m.a*q.x+m.c*q.y+m.e,m.b*q.x+m.d*q.y+m.f]})}),orbRadius:37/640*r.width,square:Math.abs(a.width-a.height)};});
   assert(geometry.square<.05);for(const c of[geometry.anchor,...geometry.svg])assert(Math.hypot(c[0]-geometry.expected[0],c[1]-geometry.expected[1])<.1,'flare centre detached from canvas orb');for(const root of geometry.roots)assert(Math.hypot(root[0]-geometry.expected[0],root[1]-geometry.expected[1])<=geometry.orbRadius*1.12,'flare roots detached from sphere');
  }
 }
 await p.mouse.move(field.x+field.width*.5,field.y+field.height*.5);await p.mouse.down();await p.mouse.move(field.x+field.width*.7,field.y+field.height*.4);await p.mouse.up();
 assert(await p.evaluate(()=>Math.abs(px-.2/.421875)<.01&&Math.abs(py+.1/.421875)<.01),'parameter mapping changed');
 const checkPointer=async()=>assert(await p.evaluate(()=>{const dial=getComputedStyle(document.getElementById('mutateDial')),box=getComputedStyle(xybox);return!document.body.classList.contains('keyboard-nav')&&dial.outlineStyle==='none'&&box.boxShadow==='none';}));
 await checkPointer();await p.locator('#mutate').click();await checkPointer();await p.locator('#drive').click();await checkPointer();
 await p.locator('#field').focus();await p.keyboard.press('ArrowLeft');assert(await p.evaluate(()=>document.body.classList.contains('keyboard-nav')&&getComputedStyle(xybox).boxShadow.includes('190, 126, 231')));
 await p.locator('#mutate').focus();await p.keyboard.press('ArrowRight');assert.equal(await p.locator('#mutateDial').evaluate(el=>getComputedStyle(el).outlineColor),'rgba(190, 126, 231, 0.533)');
 await p.locator('#field').click();await checkPointer();
 assert.deepEqual(errors,[]);if(process.env.WUBROT_SCREENSHOTS&&dpr===1)await p.screenshot({path:path.join(process.env.WUBROT_SCREENSHOTS,`orb-alignment-${width}.png`)});
 console.log(`${width} DPR${dpr}: SVG centres, animation/paused, edges, pointer mapping, pointer/keyboard focus passed`);await p.close();
 }}finally{await b.close()}})().catch(e=>{console.error(e);process.exit(1)});
