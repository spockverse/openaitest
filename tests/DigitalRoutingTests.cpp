#include "ControlFixtures.h"
#include "fixtures/v045/CrushDSP.h"
struct DigitalRoutingProbe {
 static float step(const CrushDSP& d){return 1/d.sineCrush.crusher.modern[0];}
 static float rate(const CrushDSP& d){return float(d.sineCrush.crusher.sampleRate)*d.sineCrush.crusher.modern[1];}
 static float entering(const CrushDSP& d){return d.sineCrush.crusher.previous[0];}
 static float held(const CrushDSP& d){return d.sineCrush.crusher.held[0];}
 static bool capture(const CleanCrusher& d){return d.phase>=1;}
};
static CrushDSP::Parameters params(float x,float y,int model=8){
 auto m=XYMapping::map(x,y,sr,model);CrushDSP::Parameters p;
 p.processingVersion=model;p.signedX=x;p.signedY=y;p.bits=m.bits;p.rate=m.rate;
 p.activity=m.activity;p.centered=true;p.output=0;p.mix=1;p.drive=0;p.mutate=1;return p;
}
int main(){
 // Quantization thresholds, not a spectral-distance proxy for bit precision.
 for(float bits:{2.f,3.f,5.f,8.f,16.f}){
  CleanCrusher q;q.prepare(sr);q.setModern(bits,sr,0,0,1,32);
  const float levels=std::exp2(bits-1);
  for(int i=-1024;i<=1024;++i){const float x=i/1024.f;float f[]{x,x};q.processModern(f,2);
   require(f[0]==std::round(x*levels)/levels,"Quantizer step does not match bit depth");}
 }
 // Native effective coefficients agree with mapping throughout the circular XY field.
 for(float x:{-.7f,-.35f,0.f,.35f,.7f})for(float y:{-.7f,-.35f,0.f,.35f,.7f}){
  CrushDSP d;d.prepare(sr);auto p=params(x,y);float f[]{.2f,.2f};d.processFrame(f,2,p);
  require(std::abs(DigitalRoutingProbe::step(d)-std::exp2(1-p.bits))<1.e-6f,"Hidden quantization scaling remains");
  require(std::abs(DigitalRoutingProbe::rate(d)-p.rate)<.01f,"Hidden hold-rate override remains");
 }
 for(double fs:{44100.,48000.,96000.,192000.})for(float rate:{32.f,64.f,125.f,500.f,1100.f}){
  CleanCrusher q;q.prepare(fs);q.setModern(16,rate,0,0,1,32);int captures=0;
  for(int i=0;i<int(fs);++i){captures+=DigitalRoutingProbe::capture(q)?1:0;float f[]{float(std::sin(i*.017)),0};q.processModern(f,1);}
  require(std::abs(captures-rate)<=1,"Hold clock outside effective rate range");
 }
 for(double fs:{44100.,48000.,96000.,192000.}){CrushDSP d;d.prepare(fs);auto p=params(.7f,.7f);p.rate=32;p.bits=2;p.drive=36;
  for(int i=0;i<4000;++i){float f[2]{};d.processFrame(f,2,p);require(f[0]==0&&f[1]==0,"New digital routing leaks sound during silence");}}
 // Rate changes retain clock phase across arbitrary processing partitions.
 auto clock=[](int block){CleanCrusher q;q.prepare(sr);std::vector<float> out;
  for(int offset=0;offset<24000;offset+=block)for(int i=offset;i<std::min(offset+block,24000);++i){
   if(i%193==0)q.setModern(5,i%772?32.f:1700.f,0,.04f,193,32);
   float f[]{.3f*std::sin(i*.039f),0};q.processModern(f,1);out.push_back(f[0]);}return out;};
 require(clock(64)==clock(257),"Hold phase depends on audio block size");
 // DRIVE must reach the digital stages, instead of merely changing their post-mix.
 auto feed=[](int model,float drive,bool held){CrushDSP d;d.prepare(sr);auto p=params(.55f,-.45f,model);p.drive=drive;p.bits=6;p.rate=800;
  std::vector<float> out;for(int i=0;i<24000;++i){float v=.35f*std::sin(i*.007199483f);float f[]{v,v};d.processFrame(f,2,p);
   if(i>=4800)out.push_back(held?DigitalRoutingProbe::held(d):DigitalRoutingProbe::entering(d));}return out;};
 require(feed(7,0,false)==feed(7,36,false),"Legacy control routing unexpectedly changed");
 require(difference(match(feed(8,0,false)),match(feed(8,36,false)))>.05,"DRIVE never reaches quantizer input");
 require(difference(match(feed(8,0,true)),match(feed(8,36,true)))>.05,"DRIVE never changes quantized/held output");
 // Keep Sub changes reconstruction, never the excitation delivered to Q.
 {CrushDSP protectedBass,unprotected;protectedBass.prepare(sr);unprotected.prepare(sr);
  auto p=params(.4f,-.35f),u=p;u.protect=0;
  for(float v:bass(0)){float a[]{v,v},b[]{v,v};protectedBass.processFrame(a,2,p);unprotected.processFrame(b,2,u);
   require(DigitalRoutingProbe::entering(protectedBass)==DigitalRoutingProbe::entering(unprotected),"Keep Sub starves the crusher input");}}
 // Captured staircase edges survive reconstruction; no fixed 6 kHz error LP.
 {CrushDSP d;d.prepare(sr);auto p=params(.4f,-.35f);p.bits=3;p.rate=64;
  float previousHeld=0,previousOut=0;double inputEdges=0,outputEdges=0;
  auto source=bass(0);for(std::size_t i=0;i<source.size();++i){float f[]{source[i],source[i]};d.processFrame(f,2,p);
   float held=DigitalRoutingProbe::held(d);if(i>4800&&held!=previousHeld){inputEdges+=std::abs(held-previousHeld);outputEdges+=std::abs(f[0]-previousOut);}
   previousHeld=held;previousOut=f[0];}
  require(inputEdges>1&&outputEdges/inputEdges>.4,"Digital step edges are being suppressed");
  std::cout<<"Reconstructed/raw step-edge ratio "<<outputEdges/inputEdges<<'\n';}
 const auto source=bass(0);auto dna=analyse(reference(0));double minimum=10,peak=0;
 for(auto xy:{std::array<float,2>{-.35f,-.3f},{.35f,-.3f},{-.35f,.3f},{.35f,.3f},{0,-1},{0,1},{-.7f,-.7f},{.7f,.7f}}){
  const auto out=render(source,xy[0],xy[1],nullptr,0,1,8);
  const double retained=magnitude(out,55)/magnitude(source,55);minimum=std::min(minimum,retained);
  require(retained>.8,"Sub preservation lost the source fundamental");
  require(difference(match(out),match(source))>.05,"No measurable crusher contribution");
 }
 for(int k=0;k<4;++k){auto a=bass(k);for(float&v:a)v*=2.5f;
  for(int q=0;q<4;++q)for(float drive:{0.f,18.f,36.f})for(const DnaProfile* profile:{static_cast<const DnaProfile*>(nullptr),static_cast<const DnaProfile*>(&dna)}){
   auto b=render(a,q&1?.7f:-.7f,q&2?.7f:-.7f,profile,1,1,8,drive);
   for(float v:b)peak=std::max(peak,double(std::abs(v)));}}
 require(peak<1.2,"Unstable extreme digital-crush output");
 require(render(source,0,0,&dna,1,1,8,36)==source,"Dry center changed");
 require(render(source,.4f,-.3f,&dna,0,1,8)==render(source,.4f,-.3f,nullptr,1,1,8),"DNA leaks at zero MUTATE");
 CrushDSP current;Legacy045CrushDSP legacy;current.prepare(sr);legacy.prepare(sr);
 auto p=params(.55f,-.45f,7);p.drive=18;Legacy045CrushDSP::Parameters q;
 q.processingVersion=7;q.signedX=p.signedX;q.signedY=p.signedY;q.bits=p.bits;q.rate=p.rate;q.activity=p.activity;
 q.centered=true;q.output=0;q.mix=1;q.drive=18;q.mutate=1;
 for(float v:source){float a[]{v,v},b[]{v,v};current.processFrame(a,2,p,&dna);legacy.processFrame(b,2,q,&dna);require(a[0]==b[0],"v045 project recall changed");}
 std::cout<<"PASS measured quantization grid, 32 Hz hold clock, continuous phase, pre-crush DRIVE, exact v045 recall; minimum 55 Hz amplitude ratio "<<minimum<<", hot peak "<<peak<<". Listening/FL Studio remain manual.\n";
}
