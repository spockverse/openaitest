// Frozen approved v0.2.35 material, used with gridAmount=0.

class MaterialRendererV035 {
 constructor(surface){
  this.gl=null;this.ready=false;
  surface.dataset.renderer='canvas-fallback';
  try {
   const gl=surface.getContext('webgl',{alpha:true,antialias:false,depth:false,stencil:false,premultipliedAlpha:false,powerPreference:'low-power'});
   if(!gl||gl.isContextLost())return;
   const shader=(type,source)=>{
    const s=gl.createShader(type);gl.shaderSource(s,source);gl.compileShader(s);
    if(!gl.getShaderParameter(s,gl.COMPILE_STATUS)){const message=gl.getShaderInfoLog(s);gl.deleteShader(s);throw Error(message);}
    return s;
   };
   const vertex=shader(gl.VERTEX_SHADER,`
    attribute vec2 aPosition;varying vec2 uv;
    void main(){uv=vec2(aPosition.x*.5+.5,.5-aPosition.y*.5);gl_Position=vec4(aPosition,0.,1.);}
   `);
   const derivatives=!!gl.getExtension('OES_standard_derivatives');
   const fragment=shader(gl.FRAGMENT_SHADER,`
    ${derivatives?'#extension GL_OES_standard_derivatives : enable':''}
    precision highp float;varying vec2 uv;
    uniform sampler2D artwork;uniform sampler2D normals;uniform vec2 body;uniform float idleTime;uniform float idleAmount;uniform vec2 anchor;uniform vec2 materialLag;uniform float reactiveGain;uniform float reactiveLight;uniform float gridAmount;
    float gridPixel(float v){return ${derivatives?'max(length(vec2(dFdx(v),dFdy(v))),.0001)':'1.'};}
    float gridStroke(float distance,float pixel,float width){return clamp(.5+.5*width-distance/pixel,0.,1.);}
    vec2 gridPosition(vec2 p,vec2 displacement){
     float falloff=exp(-dot(p-anchor,p-anchor)/.045);
     vec2 bend=displacement*.28*falloff;
     bend*=.0078125/(.0078125+length(bend));
     return p-bend;
    }
    float targetingInk(vec2 p,vec2 displacement){
     // Use the existing displacement, limited to five design pixels locally;
     // keep the four circles recognizable even at the circular boundary.
     vec2 axes=gridPosition(p,displacement)*640.;
     vec2 g=axes-anchor*640.;
     float r=length(g),pr=gridPixel(r),px=gridPixel(axes.x),py=gridPixel(axes.y);
     float rings=max(max(gridStroke(abs(r-103.),pr,.8),gridStroke(abs(r-167.),pr,.8)),gridStroke(abs(r-238.),pr,.8))*.47;
     rings=max(rings,gridStroke(abs(r-205.),pr,1.7)*.69);
     float horizontal=gridStroke(abs(axes.y),py,.8)*(1.-smoothstep(294.,296.,abs(axes.x)));
     float vertical=gridStroke(abs(axes.x),px,.8)*(1.-smoothstep(294.,296.,abs(axes.y)));
     float guides=max(horizontal,vertical)*.50;
     vec2 dotDelta=abs(axes);
     float dots=min(length(dotDelta-vec2(295.,0.)),length(dotDelta-vec2(0.,295.)));
     return max(max(rings,guides),gridStroke(dots,gridPixel(dots),3.));
    }
    void main(){
     vec2 p=uv-.5;float radius=length(p);
     // A disk automorphism maps the sphere to the artwork cavity while preserving the circular rim.
     vec2 z=p/.4671875,a=anchor/.4671875,numerator=z-a;
     vec2 denominator=vec2(1.-dot(a,z),-(a.x*z.y-a.y*z.x));
     vec2 mapped=vec2(dot(numerator,denominator),numerator.y*denominator.x-numerator.x*denominator.y)/dot(denominator,denominator);
     vec2 local=p-anchor;float distanceToOrb=length(local);
     float contact=1.-exp(-distanceToOrb*distanceToOrb/.0025);
     float alpha=1.-smoothstep(.465,.4671875,radius);
     if(alpha<=0.){gl_FragColor=vec4(0.);return;}
     float edge=max(0.,1.-pow(radius/.4671875,4.));
     vec4 field=texture2D(normals,uv);
     vec2 normal=vec2(field.r+field.g/256.,field.b+field.a/256.)*(255./16.)-8.;
     vec2 slosh=body*1.2+vec2(-p.y,p.x)*(body.x-body.y)*1.5;
     // Broad, low-amplitude radial waves keep the cached material alive at rest.
     float phase=distanceToOrb*62.-idleTime*3.2;
     float secondary=distanceToOrb*39.+idleTime*1.7+p.x*5.;
     vec2 radial=local/max(distanceToOrb,.025);
     vec2 idle=radial*(sin(phase)*.010+sin(secondary)*.0045);
     idle+=vec2(sin(p.y*19.+idleTime*.64),cos(p.x*17.-idleTime*.71))*.002;
     float locality=exp(-distanceToOrb*distanceToOrb/.07);
     vec2 tension=materialLag*locality*1.5;
     vec2 refraction=(normal*.055+slosh+tension+idle*idleAmount)*edge*contact;
     // Intensify existing ripples only, not the orb's bulk stretch/tension.
     // A smooth six-pixel limit prevents audio from producing long streaks.
     vec2 audioPush=(normal*.055+idle*idleAmount)*edge*contact*reactiveGain*1.45;
     audioPush*=.009375/(.009375+length(audioPush));
     refraction+=audioPush;
     vec2 source=.5+mapped*.435-refraction;
     vec3 colour=texture2D(artwork,clamp(source,vec2(0.),vec2(1.))).rgb;
     float light=(clamp(dot(normal,vec2(-.7,-.5)),-.5,.5)+cos(phase)*.10*idleAmount)*edge;
     colour=colour*(1.+light*.35)+vec3(.08,.025,.1)*max(0.,light);
     colour*=1.+reactiveLight*.26*locality*edge;
     float rim=smoothstep(.455,.463,radius)*(1.-smoothstep(.464,.4671875,radius));
     colour+=vec3(.30,.10,.43)*rim*(.5+.5*sin(atan(p.y,p.x)*3.+.8));
     if(gridAmount>0.)colour=mix(colour,vec3(.91,.89,.95),targetingInk(p,refraction)*gridAmount);
     gl_FragColor=vec4(colour,alpha);
    }
   `);
   const program=gl.createProgram();gl.attachShader(program,vertex);gl.attachShader(program,fragment);gl.linkProgram(program);
   gl.deleteShader(vertex);gl.deleteShader(fragment);
   if(!gl.getProgramParameter(program,gl.LINK_STATUS))throw Error(gl.getProgramInfoLog(program));
   gl.useProgram(program);
   const vertices=gl.createBuffer();gl.bindBuffer(gl.ARRAY_BUFFER,vertices);
   gl.bufferData(gl.ARRAY_BUFFER,new Float32Array([-1,-1,1,-1,-1,1,-1,1,1,-1,1,1]),gl.STATIC_DRAW);
   const position=gl.getAttribLocation(program,'aPosition');gl.enableVertexAttribArray(position);gl.vertexAttribPointer(position,2,gl.FLOAT,false,0,0);
   const texture=(unit)=>{
    gl.activeTexture(unit);const t=gl.createTexture();gl.bindTexture(gl.TEXTURE_2D,t);
    gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MIN_FILTER,gl.LINEAR);gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MAG_FILTER,gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_S,gl.CLAMP_TO_EDGE);gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_T,gl.CLAMP_TO_EDGE);return t;
   };
   this.artwork=texture(gl.TEXTURE0);gl.uniform1i(gl.getUniformLocation(program,'artwork'),0);
   this.normals=texture(gl.TEXTURE1);gl.uniform1i(gl.getUniformLocation(program,'normals'),1);
   gl.texImage2D(gl.TEXTURE_2D,0,gl.RGBA,64,64,0,gl.RGBA,gl.UNSIGNED_BYTE,new Uint8Array(64*64*4).fill(128));
   this.gridAmount=gl.getUniformLocation(program,'gridAmount');this.reactiveGain=gl.getUniformLocation(program,'reactiveGain');this.reactiveLight=gl.getUniformLocation(program,'reactiveLight');this.anchor=gl.getUniformLocation(program,'anchor');this.materialLag=gl.getUniformLocation(program,'materialLag');this.idleTime=gl.getUniformLocation(program,'idleTime');this.idleAmount=gl.getUniformLocation(program,'idleAmount');this.body=gl.getUniformLocation(program,'body');this.program=program;this.gl=gl;
  }catch(error){
   // Keep the existing artwork usable if WebGL is unavailable or fails to initialise.
   this.gl=null;
  }
  surface.dataset.renderer=this.gl?'webgl':'canvas-fallback';
 }
 render(image,gel,idleTime=0,idleAmount=0,reactiveGain=0,reactiveLight=reactiveGain,gridAmount=1){
  const gl=this.gl;if(!gl||gl.isContextLost())return false;
  if(!image.complete||!image.naturalWidth)return true;
  gl.useProgram(this.program);gl.viewport(0,0,640,640);
  gl.activeTexture(gl.TEXTURE0);gl.bindTexture(gl.TEXTURE_2D,this.artwork);
  if(!this.ready){gl.texImage2D(gl.TEXTURE_2D,0,gl.RGBA,gl.RGBA,gl.UNSIGNED_BYTE,image);this.ready=true;}
  gl.activeTexture(gl.TEXTURE1);gl.bindTexture(gl.TEXTURE_2D,this.normals);
  gl.texSubImage2D(gl.TEXTURE_2D,0,0,0,64,64,gl.RGBA,gl.UNSIGNED_BYTE,gel.normals());
  gl.uniform1f(this.gridAmount,gridAmount);gl.uniform1f(this.reactiveGain,reactiveGain);gl.uniform1f(this.reactiveLight,reactiveLight);gl.uniform2f(this.anchor,px*.421875,py*.421875);gl.uniform2f(this.materialLag,(px-membrane.x)*.421875,(py-membrane.y)*.421875);gl.uniform1f(this.idleTime,idleTime);gl.uniform1f(this.idleAmount,idleAmount);gl.uniform2f(this.body,gel.x,gel.y);gl.drawArrays(gl.TRIANGLES,0,6);return true;
 }
}
