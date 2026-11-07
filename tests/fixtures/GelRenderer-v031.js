// Frozen v0.2.31 renderer: zero-reactivity pixel regression reference.

class BaselineGelRenderer {
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
   const fragment=shader(gl.FRAGMENT_SHADER,`
    precision highp float;varying vec2 uv;
    uniform sampler2D artwork;uniform sampler2D normals;uniform vec2 body;uniform float idleTime;uniform float idleAmount;uniform vec2 anchor;uniform vec2 materialLag;
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
     vec2 tension=materialLag*exp(-distanceToOrb*distanceToOrb/.07)*1.5;
     vec2 refraction=(normal*.055+slosh+tension+idle*idleAmount)*edge*contact;
     vec2 source=.5+mapped*.435-refraction;
     vec3 colour=texture2D(artwork,clamp(source,vec2(0.),vec2(1.))).rgb;
     float light=(clamp(dot(normal,vec2(-.7,-.5)),-.5,.5)+cos(phase)*.10*idleAmount)*edge;
     colour=colour*(1.+light*.35)+vec3(.08,.025,.1)*max(0.,light);
     float rim=smoothstep(.455,.463,radius)*(1.-smoothstep(.464,.4671875,radius));
     colour+=vec3(.30,.10,.43)*rim*(.5+.5*sin(atan(p.y,p.x)*3.+.8));
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
   this.anchor=gl.getUniformLocation(program,'anchor');this.materialLag=gl.getUniformLocation(program,'materialLag');this.idleTime=gl.getUniformLocation(program,'idleTime');this.idleAmount=gl.getUniformLocation(program,'idleAmount');this.body=gl.getUniformLocation(program,'body');this.program=program;this.gl=gl;
  }catch(error){
   // Keep the existing artwork usable if WebGL is unavailable or fails to initialise.
   this.gl=null;
  }
  surface.dataset.renderer=this.gl?'webgl':'canvas-fallback';
 }
 render(image,gel,idleTime=0,idleAmount=0){
  const gl=this.gl;if(!gl||gl.isContextLost())return false;
  if(!image.complete||!image.naturalWidth)return true;
  gl.useProgram(this.program);gl.viewport(0,0,640,640);
  gl.activeTexture(gl.TEXTURE0);gl.bindTexture(gl.TEXTURE_2D,this.artwork);
  if(!this.ready){gl.texImage2D(gl.TEXTURE_2D,0,gl.RGBA,gl.RGBA,gl.UNSIGNED_BYTE,image);this.ready=true;}
  gl.activeTexture(gl.TEXTURE1);gl.bindTexture(gl.TEXTURE_2D,this.normals);
  gl.texSubImage2D(gl.TEXTURE_2D,0,0,0,64,64,gl.RGBA,gl.UNSIGNED_BYTE,gel.normals());
  gl.uniform2f(this.anchor,px*.421875,py*.421875);gl.uniform2f(this.materialLag,(px-membrane.x)*.421875,(py-membrane.y)*.421875);gl.uniform1f(this.idleTime,idleTime);gl.uniform1f(this.idleAmount,idleAmount);gl.uniform2f(this.body,gel.x,gel.y);gl.drawArrays(gl.TRIANGLES,0,6);return true;
 }
}
