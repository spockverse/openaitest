const{chromium}=require(process.env.WUBROT_PLAYWRIGHT||'playwright');const fs=require('fs'),path=require('path'),assert=require('assert');
(async()=>{const b=await chromium.launch({...(process.env.WUBROT_CHROMIUM?{executablePath:process.env.WUBROT_CHROMIUM}:{}),args:['--no-sandbox']});try{for(const dpr of[1,1.25,1.5,2])for(const[width,height]of[[600,520],[804,697],[996,864]]){
 const p=await b.newPage({viewport:{width,height},deviceScaleFactor:dpr});const errors=[];p.on('pageerror',e=>errors.push(e.message));
 await p.evaluate(()=>{window.__JUCE__={backend:{emitEvent(){},addEventListener(){}}}});await p.setContent(fs.readFileSync(path.join(__dirname,'../prototype/WUBROT.html'),'utf8'));
 const field=await p.locator('#field').boundingBox();
 for(const motion of[true,false]){
  await p.evaluate(on=>{motion=on;syncButtons();schedule()},motion);
  for(const[x,y]of[[0,0],[-1,0],[1,0],[0,-1],[0,1],[.707,.707],[-.707,-.707],[.72,-.44]]){
   await p.evaluate(([x,y])=>setXY(x,y,false),[x,y]);await p.waitForTimeout(25);
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
