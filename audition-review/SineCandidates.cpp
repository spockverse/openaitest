// Isolated listening experiment. No production/plugin state changes.
#include "../tests/ControlFixtures.h"
#include <chrono>
struct Setting {int mode=0;float frequency=97,depth=.55f,bits=8,rate=6000;bool post=false,stepped=false,reactive=true;};
struct SineCandidate {
 Setting cfg;double fs,phase=0,holdPhase=1;float re=1,im=0,rotR=1,rotI=0,freq=0,depth=0,held=0,low=0,deltaLP=0,fast=0,slow=0;float lowK,upperK,fastK,slowK,controlK,depthStep,levels;int tickCount=0;
 SineCandidate(Setting s,double sampleRate):cfg(s),fs(sampleRate){freq=s.frequency;depth=s.depth;lowK=1-std::exp(-6.2831853f*240/float(fs));upperK=1-std::exp(-6.2831853f*6000/float(fs));fastK=1-std::exp(-1/(.002f*float(fs)));slowK=1-std::exp(-1/(.05f*float(fs)));controlK=1-std::exp(-64/(.02f*float(fs)));depthStep=1/(.01f*float(fs));levels=std::exp2(std::clamp(s.bits,2.f,16.f)-1);}
 float tick(float x){fast+=fastK*(std::abs(x)-fast);slow+=slowK*(std::abs(x)-slow);
  if(tickCount++%64==0){float activity=cfg.reactive?std::clamp((fast-slow)/(slow+.02f),-.5f,1.f):0;float target=std::clamp(cfg.frequency*(1+.12f*activity),20.f,std::min(2000.f,float(fs*.2)));freq+=controlK*(target-freq);re=std::cos(6.283185307179586*phase);im=std::sin(6.283185307179586*phase);float angle=6.2831853f*freq/float(fs);rotR=std::cos(angle);rotI=std::sin(angle);}
  float carrier=im;float nr=re*rotR-im*rotI;im=im*rotR+re*rotI;re=nr;phase+=freq/fs;phase-=std::floor(phase);depth+=std::clamp(cfg.depth-depth,-depthStep,depthStep);
  if(cfg.mode==0)return x*((1-depth)+depth*carrier);
  low+=lowK*(x-low);float mid=x-low;
  if(cfg.mode==1){float shaped=(mid*1.8f)/std::sqrt(1+mid*mid*3.24f)/1.3416408f;float change=depth*(shaped*carrier-mid);deltaLP+=upperK*(change-deltaLP);return x+deltaLP;}
  if(cfg.stepped)carrier=std::round(carrier*3)/3;
  float source=cfg.post?mid:mid*((1-depth)+depth*carrier);
  if(holdPhase>=1){holdPhase-=std::floor(holdPhase);held=std::round(source*levels)/levels;}
  holdPhase+=std::clamp(double(cfg.rate),500.,fs)/fs;
  float processed=cfg.post?held*((1-depth)+depth*carrier):held;
  return low+processed;
 }
};
static std::vector<float> makeInput(int k){auto a=bass(k==0?2:k==4?0:1,sr*2);for(int i=0;i<int(a.size());++i){float t=float(i)/sr;if(k==1)a[i]=.23f*std::sin(6.2831853f*55*t+2.3f*std::sin(6.2831853f*137*t));if(k==2)a[i]=.18f*std::sin(6.2831853f*55*t)+.1f*std::sin(6.2831853f*165*t+std::sin(t*4))+.06f*std::sin(6.2831853f*440*t)*(.5f+.5f*std::sin(t*8));if(k==3)a[i]=.3f*std::tanh(a[i]*6);}return a;}
static std::vector<float> readRaw(const char*p){std::ifstream f(p,std::ios::binary|std::ios::ate);auto bytes=f.tellg();require(bytes>0,"Missing private raw input");std::vector<float>a(std::size_t(bytes)/4);f.seekg(0);f.read(reinterpret_cast<char*>(a.data()),bytes);return a;}
static void append(std::vector<float>&out,const std::vector<float>&input){auto a=match(input);out.insert(out.end(),a.begin(),a.end());out.resize(out.size()+sr/4,0);}
int main(int argc,char**argv){std::filesystem::path dir(argv[1]);std::filesystem::create_directories(dir);std::vector<std::vector<float>> inputs;if(argc>2){for(int i=2;i<argc;++i)inputs.push_back(readRaw(argv[i]));}else for(int k=0;k<5;++k)inputs.push_back(makeInput(k));
 std::vector<float>dry,old;for(auto&a:inputs){append(dry,a);append(old,render(a,.55f,-.45f,nullptr,0,1,4,0));}wav(dir/"00-dry.wav",dry);wav(dir/"01-v042-baseline.wav",old);
 std::ofstream notes(dir/"prototype-cost.csv");notes<<"file,render_microseconds,samples\n";
 for(int mode=0;mode<3;++mode)for(int choice=0;choice<2;++choice)for(int order=0;order<(mode==2?2:1);++order){Setting s;s.mode=mode;s.frequency=choice?277:97;s.depth=choice?.7f:.55f;s.bits=choice?5:8;s.rate=choice?2000:6000;s.post=order;s.stepped=mode==2&&choice;std::vector<float>out;double us=0;std::size_t count=0;
  for(auto&a:inputs){SineCandidate fx(s,sr);std::vector<float>b;b.reserve(a.size());auto start=std::chrono::steady_clock::now();for(float v:a){float z=fx.tick(v);require(std::isfinite(z)&&std::abs(z)<3,"Unstable candidate");b.push_back(z);}us+=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();count+=a.size();append(out,b);}
  std::string name=std::string(1,char('A'+mode))+"-"+std::to_string(int(s.frequency))+"Hz"+(mode==2?(order?"-quantize-before-ring":"-ring-before-quantize"):"")+".wav";wav(dir/name,out);notes<<name<<','<<us<<','<<count<<'\n';}
 for(double rate:{44100.,48000.,96000.})for(int mode=0;mode<3;++mode){Setting s;s.mode=mode;SineCandidate f(s,rate);for(int i=0;i<rate*.1;++i)require(f.tick(0)==0,"Carrier leakage on silence");Setting pure=s;pure.mode=0;pure.depth=0;SineCandidate bypass(pure,rate);for(int i=0;i<1000;++i){float x=.3f*std::sin(i*.04f);require(bypass.tick(x)==x,"Zero-depth dry failed");}}
 Setting s;s.reactive=false;SineCandidate f1(s,48000),f2(s,48000);std::vector<float>a(10000),b(10000);for(int offset=0;offset<10000;offset+=64)for(int i=offset;i<std::min(offset+64,10000);++i)a[i]=f1.tick(.2f*std::sin(i*.04f));for(int offset=0;offset<10000;offset+=257)for(int i=offset;i<std::min(offset+257,10000);++i)b[i]=f2.tick(.2f*std::sin(i*.04f));require(a==b,"Block partition changed oscillator state");std::cout<<"Sine A/B/C rendered. Silence at 44.1/48/96 kHz, zero-depth dry and phase/block partition checks passed. No plugin integration.\n";}
