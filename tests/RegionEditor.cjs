const{chromium}=require(process.env.WUBROT_PLAYWRIGHT||'playwright');const fs=require('fs'),path=require('path'),assert=require('assert');
(async()=>{const b=await chromium.launch({...(process.env.WUBROT_CHROMIUM?{executablePath:process.env.WUBROT_CHROMIUM}:{}),args:['--no-sandbox']});try{for(const[width,height]of[[600,520],[804,697],[996,864]]){
 const p=await b.newPage({viewport:{width,height},deviceScaleFactor:1.5}),errors=[];let choosers=0;p.on('pageerror',e=>errors.push(e.message));p.on('filechooser',()=>choosers++);
 await p.evaluate(()=>{window.messages=[];window.listeners={};window.__JUCE__={backend:{emitEvent:(id,data)=>messages.push(data),addEventListener:(id,fn)=>listeners[id]=fn}}});
 await p.setContent(fs.readFileSync(path.join(__dirname,'../prototype/WUBROT.html'),'utf8'));
 await p.evaluate(()=>listeners.wubrotState({regionStart:.25,regionEnd:.75,dnaSpeed:2,playhead:.6,editable:true,x:0,y:0,mutate:100,drive:20,enabled:1,sampleRate:48000,mode:0,preset:0,status:'Selected region shapes the distortion.',name:'synthetic-test.wav',waveform:Array.from({length:128},(_,i)=>.2+.7*Math.abs(Math.sin(i*.23)))}));
 const wave=await p.locator('#waveEditor').boundingBox(),start=await p.locator('#regionStart').boundingBox();await p.mouse.move(start.x+start.width/2,start.y+start.height/2);await p.mouse.down();await p.mouse.move(wave.x+wave.width*.4,start.y+start.height/2);await p.mouse.up();
 assert(await p.evaluate(()=>messages.some(m=>m.id==='regionStart'&&m.type==='parameter'&&Math.abs(m.value-.4)<.02)));
 await p.locator('#regionEnd').focus();await p.keyboard.press('ArrowLeft');assert(await p.evaluate(()=>messages.some(m=>m.id==='regionEnd'&&m.type==='parameter'&&Math.abs(m.value-.74)<.02)));
 for(let i=0;i<5;++i){await p.locator('#dnaSpeed').selectOption({index:i});assert(await p.evaluate(i=>messages.some(m=>m.id==='dnaSpeed'&&m.value===i),i));}
 await p.locator('#regionLoop').click();assert(await p.evaluate(()=>messages.some(m=>m.id==='mode'&&m.value===1)));
 assert.equal(choosers,0);const play=await p.locator('#regionPlayhead').boundingBox();assert(play.x>=wave.x&&play.x<=wave.x+wave.width);
 const footer=await p.locator('.bottom').boundingBox();assert(footer.y+footer.height<height-10);
 if(process.env.WUBROT_SCREENSHOTS)await p.screenshot({path:path.join(process.env.WUBROT_SCREENSHOTS,`region-${width}.png`)});
 assert.deepEqual(errors,[]);console.log(width,'region drag/keyboard, speeds, mode, playhead, no file dialog, footer passed');await p.close();
 }}finally{await b.close()}})().catch(e=>{console.error(e);process.exit(1)});
