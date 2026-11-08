#include "ControlFixtures.h"
#include "fixtures/v046/CrushDSP.h"
// Pixel-derived estimates using the UI's 270-unit orb travel, not the
// 299-unit artwork radius. Neighbourhood checks cover raster uncertainty.
static constexpr std::array<std::array<float,2>,3> photos{{{.05f,-.9987f},{-.48f,-.35f},{.59f,-.60f}}};
struct DigitalRoutingProbe {
 static float rate(const CrushDSP&d){return d.sineCrush.crusher.modern[1]*44100;}
 static float step(const CrushDSP&d){return 1/d.sineCrush.crusher.modern[0];}
 static float carrier(const CrushDSP&d){return d.sineCrush.control[0];}
 static float stepping(const CrushDSP&d){return d.sineCrush.control[3];}
};
static std::vector<float> run(int version,float x,float y,int kind=0,const DnaProfile* dna=nullptr,float mutate=0,float drive=0){
 const double fs=44100;const float radius=std::max(1.f,std::hypot(x,y));x/=radius;y/=radius;
 CrushDSP d;d.prepare(fs);auto m=XYMapping::map(x,y,fs,version);CrushDSP::Parameters p;
 p.processingVersion=version;p.centered=true;p.signedX=x;p.signedY=y;p.activity=m.activity;p.bits=m.bits;p.rate=m.rate;
 p.drive=drive;p.mutate=mutate;p.mix=1;p.output=0;
 std::vector<float> out;for(int i=0;i<44100;++i){const double t=i/fs;float v=.3f*std::sin(6.28318530718*55*t)+.07f*std::sin(6.28318530718*165*t);
  if(kind==1)v=.3f*std::sin(6.28318530718*55*t+2*std::sin(6.28318530718*137*t));
  if(kind==2)v=.9f*std::tanh(6*(2*std::fmod(55*t,1.)-1));
  float f[]{v,v};d.processFrame(f,2,p,dna);require(std::isfinite(f[0])&&std::abs(f[0])<1.5,"Unstable screenshot-position output");out.push_back(f[0]);}
 require(std::abs(DigitalRoutingProbe::rate(d)-m.rate)<.1f,"Screenshot rate does not reach hold clock");
 require(std::abs(DigitalRoutingProbe::step(d)-std::exp2(1-m.bits))<1.e-6,"Screenshot bit precision is overridden");
 return out;
}
static double bandRms(const std::vector<float>& a,float lo,float hi){double lp1=0,lp2=0,hp1=0,hp2=0,sum=0;
 const double l=1-std::exp(-6.28318530718*lo/44100),h=1-std::exp(-6.28318530718*hi/44100);
 for(std::size_t i=0;i<a.size();++i){hp1+=l*(a[i]-hp1);const double high=a[i]-hp1;hp2+=l*(high-hp2);
  lp1+=h*(high-hp2-lp1);lp2+=h*(lp1-lp2);if(i>=4410)sum+=lp2*lp2;}
 return std::sqrt(sum/(a.size()-4410));}
static double fundamental441(const std::vector<float>& a){double z1=0,z2=0,c=2*std::cos(6.28318530718*55/44100);
 for(float v:a){double z=v+c*z1-z2;z2=z1;z1=z;}return 2*std::sqrt(std::max(0.,z1*z1+z2*z2-c*z1*z2))/a.size();}
int main(){
 auto role=[](float x,float y,bool stepping){CrushDSP d;d.prepare(44100);auto m=XYMapping::map(x,y,44100,9);CrushDSP::Parameters p;
  p.processingVersion=9;p.centered=true;p.signedX=x;p.signedY=y;p.activity=m.activity;p.bits=m.bits;p.rate=m.rate;p.drive=0;
  float f[]{.2f,.2f};d.processFrame(f,2,p);return stepping?DigitalRoutingProbe::stepping(d):DigitalRoutingProbe::carrier(d);};
 require(role(.5f,-.5f,false)>role(-.5f,-.5f,false)+60,"Top-right metallic weighting still vertically reversed");
 require(role(.5f,.5f,true)>role(.5f,-.5f,true)*2,"Bottom-right stepping weighting still vertically reversed");

 std::array<std::vector<float>,3> clips;
 for(int k=0;k<3;++k){auto xy=photos[k];auto raw=run(9,xy[0],xy[1]);require(fundamental441(raw)>.25,"Screenshot position lost bass fundamental");clips[k]=match(raw);
  auto baseline=match(run(8,xy[0],xy[1]));const double oldMid=bandRms(baseline,200,2500),newMid=bandRms(clips[k],200,2500);
  std::cout<<"Photo "<<k+1<<" X/Y="<<xy[0]<<","<<xy[1]<<" matched 200-2500 Hz RMS "<<oldMid<<" -> "<<newMid<<'\n';
  if(k==0)require(newMid>oldMid*1.5,"Upper position still only muffles filtered bass");
  for(float delta:{-.015f,.015f})for(int kind=0;kind<3;++kind)run(9,xy[0]+delta,xy[1]+delta,kind);
 }
 require(difference(clips[1],clips[2])>.2,"Photos 2/3 collapse into the same texture");
 for(int kind=0;kind<3;++kind){std::array<std::vector<float>,4> q;
  for(int i=0;i<4;++i)q[i]=match(run(9,i&1?.35f:-.35f,i&2?.35f:-.35f,kind));
  for(int a=0;a<4;++a)for(int b=a+1;b<4;++b)require(difference(q[a],q[b])>.1,"Intermediate XY regions collapsed");}
 auto neutral=run(9,0,0);for(int i=0;i<44100;++i){double t=i/44100.;float dry=.3f*std::sin(6.28318530718*55*t)+.07f*std::sin(6.28318530718*165*t);require(neutral[i]==dry,"Dry center moved");}
 require(difference(neutral,run(9,.01f,0))<.1,"Subtle center settings lost");
 std::array<DnaProfile,3> ordinary;
 for(int kind=0;kind<3;++kind){const float hz=kind==0?55:kind==1?110:220;std::vector<float>a(sr);for(int i=0;i<sr;++i)a[i]=.3f*std::sin(6.28318530718f*hz*i/sr);
  const float* ptr[]{a.data()};ordinary[kind]=DnaProfile::analyse(ptr,1,a.size(),sr,true,true);
  require(ordinary[kind].valid&&std::abs(ordinary[kind].spectrum.peaks[0]-hz)<5,"Simple sample fundamental lost to leakage peaks");
  for(std::size_t frame=0;frame<ordinary[kind].spectralCount;++frame)require(std::abs(std::exp(ordinary[kind].spectralFrames[frame].logPeaks[0])-hz)<15,"Temporal analysis invents leakage resonances for a simple tone");
  for(float&v:a)v*=.01f;const float* quiet[]{a.data()};auto q=DnaProfile::analyse(quiet,1,a.size(),sr,true,true);
  require(std::abs(q.spectrum.peaks[0]-ordinary[kind].spectrum.peaks[0])<.1,"Reference loudness changed analysis pitch");
  for(int j=0;j<12;++j)require(std::abs(q.spectrum.bands[j]-ordinary[kind].spectrum.bands[j])<.005,"Reference loudness changed spectral shape");
  auto zero=run(9,.59f,-.60f,0,&ordinary[kind],0),none=run(9,.59f,-.60f);
  require(zero==none,"Ordinary DNA leaks at MUTATE zero");
  auto half=match(run(9,.59f,-.60f,0,&ordinary[kind],.5)),full=match(run(9,.59f,-.60f,0,&ordinary[kind],1));
  require(difference(match(zero),half)>.08&&difference(half,full)>.08,"Ordinary DNA remains ineffective at 50/100 percent");
 }
 require(difference(match(run(9,.59f,-.6f,0,&ordinary[0],1)),match(run(9,.59f,-.6f,0,&ordinary[1],1)))>.1,"Different ordinary tones yield identical mutation");
 for(auto xy:photos)for(float drive:{0.f,18.f,36.f})for(const DnaProfile* dna:{static_cast<const DnaProfile*>(nullptr),static_cast<const DnaProfile*>(&ordinary[0])})run(9,xy[0],xy[1],2,dna,1,drive);
 auto source=bass(2);auto profile=analyse(reference(1));CrushDSP current;Legacy046CrushDSP previous;current.prepare(sr);previous.prepare(sr);
 CrushDSP::Parameters p;p.processingVersion=8;p.centered=true;p.activity=.8;p.signedX=.59;p.signedY=-.6;p.bits=4;p.rate=200;p.drive=18;p.mutate=1;
 Legacy046CrushDSP::Parameters q;q.processingVersion=8;q.centered=true;q.activity=.8;q.signedX=.59;q.signedY=-.6;q.bits=4;q.rate=200;q.drive=18;q.mutate=1;
 for(float v:source){float a[]{v,v},b[]{v,v};current.processFrame(a,2,p,&profile);previous.processFrame(b,2,q,&profile);require(a[0]==b[0],"v046 legacy processing changed");}
 for(double fs:{44100.,48000.,96000.}){CrushDSP d;d.prepare(fs);p.processingVersion=9;p.drive=36;p.rate=32;
  for(int i=0;i<12000;++i){float f[2]{};d.processFrame(f,2,p,&ordinary[0]);require(f[0]==0,"Input-free audible modulation");}
  for(int i=0;i<int(fs/10);++i){float v=.3f*std::sin(float(i)*.008f),f[]{v,v};d.processFrame(f,2,p,&ordinary[0]);}
  for(int i=0;i<int(fs);++i){float f[2]{};d.processFrame(f,2,p,&ordinary[0]);if(i>fs*.9)require(std::abs(f[0])<1.e-7,"Modulation continues independently after input stops");}}

 std::cout<<"PASS screenshot/neighbour positions, intermediate regions, ordinary/quiet DNA, exact neutral and v046 recall. Perceptual quality/FL Studio require listening.\n";
}
