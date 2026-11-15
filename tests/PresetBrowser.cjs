const{chromium}=require(process.env.WUBROT_PLAYWRIGHT||'playwright'),fs=require('fs'),path=require('path'),assert=require('assert');
(async()=>{const b=await chromium.launch({...(process.env.WUBROT_CHROMIUM?{executablePath:process.env.WUBROT_CHROMIUM}:{}),args:['--no-sandbox']});try{for(const[width,height]of[[600,520],[804,697],[996,864]]){
 const p=await b.newPage({viewport:{width,height},deviceScaleFactor:1.5,reducedMotion:'no-preference'}),errors=[];p.on('pageerror',e=>errors.push(e.message));
 await p.evaluate(()=>{
  window.listeners={};window.messages=[];window.users=[{id:'Metallic Yoi.wubpreset',name:'Metallic Yoi'},{id:'Digital Rot.wubpreset',name:'Digital Rot'}];window.serial=0;
  window.state={x:0,y:0,mutate:67,drive:8.5,mode:0,enabled:1,regionStart:0,regionEnd:1,dnaSpeed:2,editable:false,preset:0,sampleRate:48000,status:'Ready.',name:'',presets:{name:'RIDDIM MONSTER',file:'',kind:'factory',dirty:false,busy:false}};
  window.publish=()=>listeners.wubrotState(state);window.syncResults=false;window.failNextSave=false;
  window.__JUCE__={backend:{addEventListener:(id,f)=>{const previous=listeners[id];listeners[id]=previous?value=>{previous(value);f(value);}:f;},emitEvent:(id,e)=>{
   messages.push(e);if(e.type==='parameter'){if(state[e.id]!==e.value){state[e.id]=e.value;state.presets.dirty=true;}publish();return;}if(e.type!=='preset')return;
   const complete=()=>{
    let ok=true,conflict=false,warning='',message='Done';const name=e.name?.trim();
    const needsUnsaved=['factory','load'].includes(e.operation)&&state.presets.dirty&&!e.discardConfirmed;
    if(needsUnsaved){ok=false;message='There are unsaved sound changes.';}
    else if(e.operation==='save'){
     if(failNextSave){failNextSave=false;ok=false;message='Cannot write to the preset folder.';}
     else if(users.some(u=>u.name.toLowerCase()===name.toLowerCase())&&!e.overwrite){ok=false;conflict=true;message='A preset with that name already exists.';}
     else{let saved=users.find(u=>u.name.toLowerCase()===name.toLowerCase());if(!saved){saved={id:name+'.wubpreset',name};users.push(saved);}saved.sound=Object.fromEntries(['x','y','drive','mutate','preset'].map(id=>[id,state[id]]));Object.assign(state.presets,{name,file:name+'.wubpreset',kind:'user',dirty:false});}
    }else if(e.operation==='factory'){
     Object.assign(state.presets,{name:['RIDDIM MONSTER','LIQUID GLASS','METAL TEETH'][e.index],file:'',kind:'factory',dirty:false});
     Object.assign(state,{preset:e.index,x:[.3,-.45,.65][e.index],drive:[12,4,20][e.index],mutate:[80,55,90][e.index],name:'',editable:false,waveform:[]});
     warning=message='Factory parameters loaded. DNA sample unavailable: Samples/dna.wav.';state.status=warning;
    }else if(e.operation==='load'){
     const u=users.find(u=>u.id===e.file);if(!u||u.broken){ok=false;message='User preset is missing or invalid.';}
     else{Object.assign(state,u.sound||{});Object.assign(state.presets,{name:u.name,file:u.id,kind:'user',dirty:false});}
    }else if(e.operation==='rename'){const u=users.find(u=>u.id===e.file);u.name=name;u.id=name+'.wubpreset';Object.assign(state.presets,{name,file:u.id});}
    else if(e.operation==='delete'){users=users.filter(u=>u.id!==e.file);Object.assign(state.presets,{name:'UNSAVED SOUND',file:'',kind:'custom',dirty:true});}
    state.presets.busy=false;state.presets.result={serial:++serial,operation:e.operation,request:e,ok,needsUnsaved,conflict,warning,message};listeners.wubrotPresetLibrary?.({users});publish();
   };
   if(syncResults)complete();else{state.presets.busy=true;publish();setTimeout(complete,8);}
  }}};
 });
 await p.setContent(fs.readFileSync(path.join(__dirname,'../prototype/WUBROT.html'),'utf8'));await p.evaluate(()=>publish());
 assert(await p.locator('#presetName').evaluate(e=>e.scrollWidth<=e.clientWidth+1),'Factory name clipped in header');
 const before=await p.locator('#field').boundingBox();await p.locator('#preset').click();await p.waitForTimeout(220);const popup=await p.locator('#presetPopup').boundingBox();assert(popup.x>=0&&popup.y>=0&&popup.x+popup.width<=width&&popup.y+popup.height<=height,'Popup clipped');assert.deepEqual(await p.locator('#field').boundingBox(),before,'Popup moved main interface');assert.equal(await p.locator('.preset-item').count(),3);assert.equal(await p.locator('#presetSearch').evaluate(e=>e===document.activeElement),true);assert.equal(await p.locator('#presetPopup').evaluate(e=>getComputedStyle(e).transitionDuration),'0.17s, 0.17s, 0s');
 await p.locator('#presetSearch').fill('gLaSs');assert.equal(await p.locator('.preset-item').count(),1);await p.keyboard.press('Enter');await p.waitForTimeout(220);assert((await p.locator('#presetName').textContent()).includes('LIQUID GLASS'));assert.equal(await p.locator('#preset').getAttribute('aria-expanded'),'false');
 await p.locator('#preset').click();await p.locator('#presetSaveNew').click();await p.locator('#presetNameInput').fill('Crunch Machine');await p.locator('#presetNameInput').press('Enter');await p.waitForFunction(()=>document.getElementById('presetName').textContent==='Crunch Machine');assert(await p.evaluate(()=>users.some(u=>u.name==='Crunch Machine')));
 await p.locator('[data-tab=user]').click();await p.locator('#presetSearch').fill('cruNCH');assert.equal(await p.locator('.preset-item').count(),1);await p.locator('#presetSearch').fill('no such preset');assert((await p.locator('.preset-empty').textContent()).includes('No presets'));await p.locator('#presetSearch').fill('');
 // Duplicate Save As offers overwrite, preserving the promise so an unsaved-switch save can complete.
 await p.locator('#presetManage').click();await p.locator('[data-operation=saveAs]').click();await p.locator('#presetNameInput').fill('Crunch Machine');await p.locator('#presetNameInput').press('Enter');await p.waitForFunction(()=>document.getElementById('presetDialogTitle').textContent==='PRESET ALREADY EXISTS');await p.locator('#presetDialogButtons button').filter({hasText:'Overwrite'}).click();await p.waitForFunction(()=>document.getElementById('preset').getAttribute('aria-busy')==='false');
 await p.evaluate(()=>{state.presets.dirty=true;publish()});assert.equal(await p.locator('#presetDirty').isVisible(),true);await p.locator('[data-tab=factory]').click();await p.locator('.preset-item').filter({hasText:'METAL TEETH'}).click();await p.locator('#presetDialogButtons button').filter({hasText:'Cancel'}).click();assert((await p.locator('#presetName').textContent()).startsWith('Crunch Machine'));
 await p.locator('.preset-item').filter({hasText:'METAL TEETH'}).click();await p.locator('#presetDialogButtons button').filter({hasText:'Save',exact:true}).click();await p.waitForFunction(()=>document.getElementById('presetName').textContent==='METAL TEETH');await p.waitForTimeout(220);

 // Match native acknowledgements (including operation/dirty guards), not only names.
 const names=['RIDDIM MONSTER','LIQUID GLASS','METAL TEETH'];
 const openFactory=async()=>{if(await p.locator('#preset').getAttribute('aria-expanded')!=='true')await p.locator('#preset').click();await p.waitForFunction(()=>document.getElementById('preset').getAttribute('aria-busy')==='false');await p.locator('[data-tab=factory]').click();await p.locator('#presetSearch').fill('');};
 const pickFactory=async i=>{await openFactory();await p.locator('.preset-item').filter({hasText:names[i]}).click();};
 const settled=async name=>{await p.waitForFunction(name=>document.getElementById('presetName').textContent===name&&document.getElementById('preset').getAttribute('aria-busy')==='false'&&document.getElementById('presetDialogLayer').hidden,name);};
 const edit=async()=>p.evaluate(()=>{state.drive=23;state.presets.dirty=true;publish();});
 const button=text=>p.locator('#presetDialogButtons button').filter({hasText:text,exact:true});
 for(let i=0;i<3;++i){await pickFactory(i);await settled(names[i]);assert.equal(await p.locator('#drive').inputValue(),String([12,4,20][i]));assert((await p.locator('#sampleStatus').textContent()).includes('DNA sample unavailable'));assert.equal(await p.locator('#presetDirty').isVisible(),false);}
 await edit();await pickFactory(1);await button('Cancel').click();await settled(names[2]);assert.equal(await p.locator('#presetDirty').isVisible(),true);assert.equal(await p.locator('#drive').inputValue(),'23');
 await p.locator('.preset-item').filter({hasText:names[1]}).click();await button('Discard').click();await settled(names[1]);
 assert(await p.evaluate(()=>messages.some(e=>e.operation==='factory'&&e.index===1&&e.discardConfirmed===true)),'DISCARD lost its factory target');
 // Factory SAVE opens Save As; its MY PRESETS tab change must not retarget the switch.
 await edit();await pickFactory(2);await button('Save').click();await p.waitForFunction(()=>document.getElementById('presetDialogTitle').textContent==='SAVE PRESET');
 await p.locator('#presetNameInput').fill('Saved Factory Edits');await button('Save preset').click();await settled(names[2]);
 assert(await p.evaluate(()=>users.some(u=>u.id==='Saved Factory Edits.wubpreset')),'Factory edits did not Save As');
 assert(await p.evaluate(()=>messages.filter(e=>e.type==='preset').at(-1).operation==='factory'&&messages.filter(e=>e.type==='preset').at(-1).index===2),'SAVE retargeted factory as user file');
 // Canceling Save As and a failed save both abandon the pending switch safely.
 await edit();await pickFactory(0);await button('Save').click();await button('Cancel').click();await settled(names[2]);assert.equal(await p.locator('#presetDirty').isVisible(),true);
 await p.locator('.preset-item').filter({hasText:names[0]}).click();await button('Save').click();await p.locator('#presetNameInput').fill('Cannot Save');await p.evaluate(()=>failNextSave=true);await button('Save preset').click();await settled(names[2]);
 assert((await p.locator('#presetMessage').textContent()).includes('Cannot write'));assert.equal(await p.locator('#presetDirty').isVisible(),true);
 await p.locator('.preset-item').filter({hasText:names[0]}).click();await button('Discard').click();await settled(names[0]);
 // A host edit arriving after the UI's last state uses the native unsaved guard.
 await openFactory();await p.evaluate(()=>{state.presets={...state.presets,dirty:true};});await p.locator('.preset-item').filter({hasText:names[2]}).click();
 await button('Discard').click();await settled(names[2]);assert.equal(await p.locator('#presetDirty').isVisible(),false);
 // Rapid clicks retain the first pending operation until it completes or is canceled.
 await openFactory();const count=await p.evaluate(()=>messages.filter(e=>e.operation==='factory').length);
 await p.evaluate(()=>document.querySelectorAll('.preset-item').forEach(e=>e.click()));await settled(names[0]);
 assert.equal(await p.evaluate(()=>messages.filter(e=>e.operation==='factory').length),count+1,'Rapid selection duplicated or replaced a pending switch');
 // Invalid user files return an error; the next switch must remain usable.
 await openFactory();await p.evaluate(()=>{users.push({id:'Broken.wubpreset',name:'Broken',broken:true});listeners.wubrotPresetLibrary({users});});await p.locator('[data-tab=user]').click();await p.locator('.preset-item').filter({hasText:'Broken'}).click();await settled(names[0]);
 assert((await p.locator('#presetMessage').textContent()).includes('missing or invalid'));
 await p.locator('.preset-item').filter({hasText:'Saved Factory Edits'}).click();await settled('Saved Factory Edits');await pickFactory(2);await settled(names[2]);
 // Fast acknowledgements must see a registered promise, and stale results cannot reopen dialogs.
 await p.evaluate(()=>syncResults=true);await pickFactory(1);await settled(names[1]);
 await p.evaluate(()=>{state.presets.result={serial:++serial,request:{requestId:-100},operation:'factory',needsUnsaved:true,ok:false,message:'Old request'};publish();});assert.equal(await p.locator('#presetDialogLayer').isVisible(),false);
 await pickFactory(2);await settled(names[2]);await p.evaluate(()=>syncResults=false);

 // Validate the exact script used by Windows' real native WebView2 test here too.
 if(width===804){
  const source=fs.readFileSync(path.join(__dirname,'WebViewAudioTests.cpp'),'utf8');
  const script=[...source.matchAll(/R"JS\(([\s\S]*?)\)JS"/g)].find(m=>m[1].includes('window.__presetProbe='))[1];
  await p.evaluate(script=>eval(script),script);await p.waitForFunction(()=>window.__presetProbe.done);
  const probe=await p.evaluate(()=>window.__presetProbe);assert.equal(probe.error,'');assert.equal(probe.checks.length,6);
  await pickFactory(2);await settled(names[2]);
 }
 await p.locator('#preset').click();await p.waitForTimeout(220);await p.keyboard.press('Escape');await p.waitForTimeout(220);assert.equal(await p.locator('#preset').getAttribute('aria-expanded'),'false');assert.equal(await p.evaluate(()=>document.activeElement.id),'preset');
 await p.locator('#preset').click();await p.waitForTimeout(220);await p.locator('#field').click();await p.waitForTimeout(220);assert.equal(await p.locator('#preset').getAttribute('aria-expanded'),'false');
 await p.evaluate(()=>{state.presets.name='A very long preset name '.repeat(3);state.presets.dirty=true;publish()});const titleBox=await p.locator('#preset').boundingBox(),dirtyBox=await p.locator('#presetDirty').boundingBox();assert(dirtyBox&&dirtyBox.x+dirtyBox.width<titleBox.x+titleBox.width,'Long names hide dirty indicator');await p.evaluate(()=>{state.presets.name='METAL TEETH';state.presets.dirty=false;publish()});
 // 10,000 entries use a bounded DOM window while scrolling and searching.
 await p.locator('#preset').click();await p.waitForTimeout(220);await p.evaluate(()=>{users=Array.from({length:10000},(_,i)=>({id:'Preset '+i+'.wubpreset',name:'Preset '+i}));listeners.wubrotPresetLibrary({users});});await p.locator('[data-tab=user]').click();assert(await p.locator('.preset-item').count()<=12);await p.locator('#presetList').evaluate(e=>e.scrollTop=300000);await p.waitForTimeout(50);assert(await p.locator('.preset-item').count()<=12);await p.locator('#presetSearch').fill('Preset 9999');assert.equal(await p.locator('.preset-item').count(),1);await p.locator('#presetSearch').fill('');await p.locator('[data-tab=factory]').click();
 if(process.env.WUBROT_SCREENSHOTS)await p.screenshot({path:path.join(process.env.WUBROT_SCREENSHOTS,`preset-browser-${width}.png`)});assert.deepEqual(errors,[]);await p.emulateMedia({reducedMotion:'reduce'});assert.equal(await p.locator('#presetPopup').evaluate(e=>getComputedStyle(e).transitionDuration),'0s');console.log(width,'custom browser bounds/animation/search, Save/As/conflict, dirty Save/Cancel, keyboard/outside close, 10k virtualization, reduced motion passed');await p.close();
 }
 const fast=await b.newPage({viewport:{width:804,height:697}});await fast.evaluate(()=>{window.listeners={};window.__JUCE__={backend:{addEventListener:(id,f)=>listeners[id]=f,emitEvent:(id,e)=>{if(e.type==='ready'){listeners.wubrotState({x:.2,y:-.1,mutate:71,drive:12,mode:0,enabled:1,regionStart:0,regionEnd:1,dnaSpeed:2,editable:false,sampleRate:48000,status:'Restored.',name:'',presets:{name:'PROJECT RECALL',file:'PROJECT RECALL.wubpreset',kind:'user',dirty:false,busy:false}});listeners.wubrotPresetLibrary({users:[{id:'PROJECT RECALL.wubpreset',name:'PROJECT RECALL'}]});}}}};});await fast.setContent(fs.readFileSync(path.join(__dirname,'../prototype/WUBROT.html'),'utf8'));assert.equal(await fast.locator('#presetName').textContent(),'PROJECT RECALL');await fast.locator('#preset').click();assert((await fast.locator('.preset-item').textContent()).includes('PROJECT RECALL'));await fast.close();console.log('Synchronous initial native state/catalogue delivered after all listeners are installed');
 }finally{await b.close()}})().catch(e=>{console.error(e);process.exit(1)});
