// Actual WebGL2 geometry, shared-space pressure, lifecycle and bounded resource tests.
const{chromium}=require(process.env.WUBROT_PLAYWRIGHT||'playwright'),fs=require('fs'),path=require('path'),assert=require('assert');
const html=fs.readFileSync(process.env.WUBROT_TEST_HTML||path.join(__dirname,'../prototype/WUBROT.html'),'utf8');
(async()=>{const b=await chromium.launch({...(process.env.WUBROT_CHROMIUM?{executablePath:process.env.WUBROT_CHROMIUM}:{}),args:['--no-sandbox']});try{
for(const[width,height]of[[600,520],[804,697],[996,864]]){
 const p=await b.newPage({viewport:{width,height},deviceScaleFactor:1.5});const errors=[];p.on('pageerror',e=>errors.push(e.message));
 await p.evaluate(()=>{window.listeners={};window.events=[];window.__JUCE__={backend:{addEventListener:(id,f)=>listeners[id]=f,emitEvent:(id,e)=>{events.push(e);if(e.type==='ready')listeners.wubrotState({enabled:1,x:0,y:0,mutate:100,drive:0,mode:0,xyMapping:14,sampleRate:48000});}}};});
 await p.setContent(html);await p.waitForFunction(()=>renderer.ready&&renderer.gl&&!awaitingNativePower);
 const geometry=await p.evaluate(()=>{
  cancelAnimationFrame(frameRequest);frameRequest=0;gel.reset();membrane.x=px;membrane.y=py;
  const gl=renderer.gl;renderer.render(jelly,gel,2,1,0);
  // Transform feedback observes actual vertex shader outputs. Readbacks exist
  // only in this test; production animation never reads GPU data synchronously.
  const vs=gl.getAttachedShaders(renderer.material.p).find(s=>gl.getShaderParameter(s,gl.SHADER_TYPE)===gl.VERTEX_SHADER);
  const program=gl.createProgram(),v=gl.createShader(gl.VERTEX_SHADER),f=gl.createShader(gl.FRAGMENT_SHADER);
  gl.shaderSource(v,gl.getShaderSource(vs));gl.compileShader(v);gl.attachShader(program,v);
  gl.shaderSource(f,'#version 300 es\nprecision highp float;out vec4 c;void main(){c=vec4(1);}');gl.compileShader(f);gl.attachShader(program,f);
  gl.transformFeedbackVaryings(program,['world','normal'],gl.INTERLEAVED_ATTRIBS);gl.linkProgram(program);if(!gl.getProgramParameter(program,gl.LINK_STATUS))throw Error(gl.getProgramInfoLog(program));
  const points=[],optics=[],centres=[];
  for(const[x,y]of[[0,0],[-1,0],[1,0],[0,-1],[0,1],[-.70710678,-.70710678],[.70710678,-.70710678],[-.70710678,.70710678],[.70710678,.70710678],[.4,-.3]]){
   const ax=x*.84375,ay=-y*.84375;centres.push([ax,ay]);points.push(ax,ay,0);
   const len=Math.hypot(ax,ay),dx=len?-ax/len:1,dy=len?-ay/len:0;points.push(ax+dx*.19,ay+dy*.19,0);
  }
  for(let i=0;i<points.length;i+=3){const x=points[i],y=points[i+1],r=Math.hypot(x,y),R=.934375,q=1-r*r/(R*R),z=Math.sqrt(Math.max(.025,q)),dk=q>.025?-.13/(R*R*z):0,edge=Math.max(0,1-Math.pow(r/R,4)),ek=-4*r*r/Math.pow(R,4),rim=.035*Math.exp(-Math.pow((r-.885)/.047,2)),rk=-2*(r-.885)/(.047*.047)*rim/Math.max(r,.00001);const t=y*12-x*4,phase=x*13+y*9+.7*Math.sin(t),fold=.013*Math.sin(phase),fg=.013*Math.cos(phase);optics.push(.13*z+rim+fold*edge,x*dk+fg*(13-2.8*Math.cos(t))*edge+fold*x*ek,y*dk+fg*(9+8.4*Math.cos(t))*edge+fold*y*ek,edge,x*ek,y*ek,x*rk,y*rk);}
  const vao=gl.createVertexArray();gl.bindVertexArray(vao);const buffers=[gl.createBuffer(),gl.createBuffer(),gl.createBuffer()];
  gl.bindBuffer(gl.ARRAY_BUFFER,buffers[0]);gl.bufferData(gl.ARRAY_BUFFER,new Float32Array(points),gl.STATIC_DRAW);gl.enableVertexAttribArray(0);gl.vertexAttribPointer(0,3,gl.FLOAT,false,0,0);
  gl.bindBuffer(gl.ARRAY_BUFFER,buffers[1]);gl.bufferData(gl.ARRAY_BUFFER,new Float32Array(optics),gl.STATIC_DRAW);for(let i=1;i<=2;i++){gl.enableVertexAttribArray(i);gl.vertexAttribPointer(i,4,gl.FLOAT,false,32,(i-1)*16);}
  gl.bindBuffer(gl.TRANSFORM_FEEDBACK_BUFFER,buffers[2]);gl.bufferData(gl.TRANSFORM_FEEDBACK_BUFFER,points.length*8,gl.STREAM_READ);gl.bindBufferBase(gl.TRANSFORM_FEEDBACK_BUFFER,0,buffers[2]);
  gl.useProgram(program);for(let i=0;i<gl.getProgramParameter(renderer.material.p,gl.ACTIVE_UNIFORMS);i++){const u=gl.getActiveUniform(renderer.material.p,i),loc=gl.getUniformLocation(program,u.name);if(loc===null)continue;const value=gl.getUniform(renderer.material.p,gl.getUniformLocation(renderer.material.p,u.name));if(u.type===gl.FLOAT_VEC2)gl.uniform2fv(loc,value);else if(u.type===gl.FLOAT_VEC4)gl.uniform4fv(loc,value);else if(u.type===gl.FLOAT)gl.uniform1f(loc,value);else gl.uniform1i(loc,value);}
  let minimumDent=1,maxNormalError=0;const result=new Float32Array(points.length*2);
  for(let i=0;i<centres.length;i++){
   gl.uniform2fv(gl.getUniformLocation(program,'anchor'),centres[i]);gl.uniform2f(gl.getUniformLocation(program,'lag'),0,0);gl.uniform1f(gl.getUniformLocation(program,'movement'),0);
   gl.enable(gl.RASTERIZER_DISCARD);gl.beginTransformFeedback(gl.POINTS);gl.drawArrays(gl.POINTS,0,points.length/3);gl.endTransformFeedback();gl.disable(gl.RASTERIZER_DISCARD);gl.getBufferSubData(gl.TRANSFORM_FEEDBACK_BUFFER,0,result);
   const index=i*12,x=centres[i][0],y=centres[i][1],r=Math.hypot(x,y),base=.13*Math.sqrt(Math.max(.025,1-r*r/(.934375*.934375)))+.035*Math.exp(-Math.pow((r-.885)/.047,2));
   const staticFold=.013*Math.sin(x*13+y*9+.7*Math.sin(y*12-x*4))*Math.max(0,1-Math.pow(r/.934375,4));minimumDent=Math.min(minimumDent,base+staticFold-result[index+2]);if(Math.abs(result[index]-x)>1e-6||Math.abs(result[index+1]-y)>1e-6)throw Error('Contact centre moved');
   for(let n=0;n<result.length;n+=6){if(!Array.from(result.subarray(n,n+6)).every(Number.isFinite)||result[n+5]<=0)throw Error('Inverted/invalid surface');maxNormalError=Math.max(maxNormalError,Math.abs(Math.hypot(...result.subarray(n+3,n+6))-1));}
  }
  // A live impulse must reach the same screen position, not its Y reflection.
  const previousHeight=result[108+2];for(let y=0;y<64;y++)for(let x=0;x<64;x++)gel.h[y*64+x]=.6*Math.exp(-((x-(.5+.4*.421875)*63)**2+(y-(.5-.3*.421875)*63)**2)/9.68);
  gl.activeTexture(gl.TEXTURE0);gl.bindTexture(gl.TEXTURE_2D,renderer.displacement);gl.texSubImage2D(gl.TEXTURE_2D,0,0,0,64,64,gl.RED,gl.FLOAT,gel.h);
  gl.enable(gl.RASTERIZER_DISCARD);gl.beginTransformFeedback(gl.POINTS);gl.drawArrays(gl.POINTS,0,points.length/3);gl.endTransformFeedback();gl.disable(gl.RASTERIZER_DISCARD);gl.getBufferSubData(gl.TRANSFORM_FEEDBACK_BUFFER,0,result);const wakeLift=result[110]-previousHeight;if(wakeLift<.01||wakeLift>.02)throw Error('Live wave field is mirrored or detached');gel.reset();
  gl.bindBufferBase(gl.TRANSFORM_FEEDBACK_BUFFER,0,null);gl.bindVertexArray(null);for(const x of buffers)gl.deleteBuffer(x);gl.deleteVertexArray(vao);gl.deleteShader(v);gl.deleteShader(f);gl.deleteProgram(program);
  // The orb is a sphere with varying depth, not a textured quad.
  gl.bindVertexArray(renderer.orbMesh.vao);const vb=gl.getVertexAttrib(0,gl.VERTEX_ATTRIB_ARRAY_BUFFER_BINDING);gl.bindBuffer(gl.ARRAY_BUFFER,vb);const sphere=new Float32Array(gl.getBufferParameter(gl.ARRAY_BUFFER,gl.BUFFER_SIZE)/4);gl.getBufferSubData(gl.ARRAY_BUFFER,0,sphere);let zmin=1,zmax=-1;for(let i=2;i<sphere.length;i+=3){zmin=Math.min(zmin,sphere[i]);zmax=Math.max(zmax,sphere[i]);}gl.bindVertexArray(null);
  renderer.render(jelly,gel,2,1,0);const a=Array.from(renderer.rotation);px=.5;py=-.2;renderer.render(jelly,gel,2,1,0);const noRetune=Array.from(renderer.rotation);renderer.render(jelly,gel,3,1,0);const rotated=Array.from(renderer.rotation);
  const gpuError=gl.getError();return{minimumDent,maxNormalError,zmin,zmax,orbIndices:renderer.orbMesh.count,a,noRetune,rotated,gpuError};
 });
 assert(geometry.minimumDent>.045,'Contact indentation disappears at the boundary');assert(geometry.maxNormalError<1e-5);assert(geometry.zmin<-.99&&geometry.zmax>.99&&geometry.orbIndices>3000);assert.deepEqual(geometry.a,geometry.noRetune,'XY changes rotation');assert.notDeepEqual(geometry.a,geometry.rotated);assert(Math.abs(Math.hypot(...geometry.rotated)-1)<1e-6);assert.equal(geometry.gpuError,0);
 await p.evaluate(()=>{for(let i=0;i<60;i++)renderer.frameTime(.12);});assert.equal(await p.evaluate(()=>renderer.detail),0,'Secondary load governor did not engage');await p.evaluate(()=>{for(let i=0;i<120;i++)renderer.frameTime(1/60);});assert.equal(await p.evaluate(()=>renderer.detail),1,'Full detail did not recover');
 const resources=await p.evaluate(()=>{const gl=renderer.gl;let uploads=0,builds=0;const upload=gl.texSubImage2D,create=gl.createBuffer;gl.texSubImage2D=function(...a){uploads++;return upload.apply(this,a)};gl.createBuffer=function(...a){builds++;return create.apply(this,a)};gel.reset();renderer.render(jelly,gel,1,1,0);renderer.render(jelly,gel,2,1,0);const idle=uploads;gel.impulse(0,0,.4,.2,.03);gel.advance(.02);renderer.render(jelly,gel,3,1,1);gl.texSubImage2D=upload;gl.createBuffer=create;return{idle,uploads,builds};});assert.deepEqual(resources,{idle:1,uploads:2,builds:0});
 const poses=[[0,0],[-1,0],[1,0],[0,-1],[0,1],[-.707,-.707],[.707,-.707],[-.707,.707],[.707,.707],[.35,-.2]];
 for(let i=0;i<poses.length;i++){await p.evaluate(xy=>{setXY(...xy);cancelAnimationFrame(frameRequest);frameRequest=0;renderer.render(jelly,gel,2.5,1,.5);paint(false)},poses[i]);if(process.env.WUBROT_SCREENSHOTS&&[0,1,2,5,8].includes(i))await p.screenshot({path:path.join(process.env.WUBROT_SCREENSHOTS,`energy-${width}-${i}.png`)});}
 // Actual context loss preserves the XY bridge, then rebuilds the one scene.
 await p.evaluate(()=>{window.lose=renderer.gl.getExtension('WEBGL_lose_context');lose.loseContext();});await p.waitForTimeout(100);assert(await p.evaluate(()=>!renderer.render(jelly,gel)));await p.evaluate(()=>{setXY(.25,-.3);lose.restoreContext();});await p.waitForFunction(()=>renderer.gl&&!renderer.gl.isContextLost()&&renderer.ready);assert(await p.evaluate(()=>Math.abs(px-.25)<1e-9&&Math.abs(py+.3)<1e-9));
 await p.evaluate(()=>{const gl=renderer.gl;window.oldProgram=renderer.material.p;renderer.dispose();window.resourcesDeleted=!gl.isProgram(oldProgram)&&gl.getParameter(gl.CURRENT_PROGRAM)===null&&renderer.buffers.length===0;});assert(await p.evaluate(()=>resourcesDeleted));assert.deepEqual(errors,[]);await p.close();
 console.log(width,'actual GPU pressure/normal outputs, 3D sphere, quaternion rotation, caches, XY extremes, context recovery and cleanup passed');
}
const failed=await b.newPage({viewport:{width:804,height:697}});await failed.evaluate(()=>{WebGL2RenderingContext.prototype.createBuffer=()=>null;window.listeners={};window.events=[];window.__JUCE__={backend:{addEventListener:(id,f)=>listeners[id]=f,emitEvent:(id,e)=>{events.push(e);if(e.type==='ready')listeners.wubrotState({enabled:1,x:0,y:0,mutate:100,drive:0,mode:0,sampleRate:48000});}}};});await failed.setContent(html);await failed.waitForFunction(()=>jelly.complete&&!awaitingNativePower);assert(await failed.evaluate(()=>!renderer.gl&&!renderer.ready&&!!surface.dataset.rendererError));await failed.evaluate(()=>setXY(.4,-.2));assert(await failed.evaluate(()=>px===.4&&py===-.2&&events.some(e=>e.id==='x'&&e.type==='parameter')));await failed.close();console.log('Partial GPU resource failure retains XY automation and Canvas fallback');
}finally{await b.close();}})().catch(e=>{console.error(e);process.exit(1)});
