// Listening prototype only. Not linked into WUBROT or used for project recall.
#include "../tests/ControlFixtures.h"
#include <chrono>
struct Candidate {
 double phase=1,other=1;float held=0,second=0,warm=0,steps=0,lp=0,hp1=0,hp2=0,body=0;std::array<float,3> colour{};
 float tick(float x,float xx,float yy,float drive,float mutate,const DnaProfile* dna,double t){
  float ax=std::tanh(3*xx),ay=std::tanh(3*yy);float weights[]{(1-ax)*(1-ay)/4,(1+ax)*(1-ay)/4,(1-ax)*(1+ay)/4,(1+ax)*(1+ay)/4};
  auto map=XYMapping::map(xx,yy,sr,4);float amount=std::min(1.f,map.activity*8);
  auto shape=dna?dna->spectralAt(float(std::fmod(t,1.))):DnaTemporalFrame{};float m=dna?mutate:0;
  float bits=std::clamp(map.bits+m*(shape.bands[3]-shape.bands[9]),2.f,16.f),levels=std::exp2(bits-1);
  float g=1+drive*3;float u=x*g;float sat=u/std::sqrt(1+u*u);float source=x+drive*.65f*(sat/std::sqrt(g)-x);
  if(phase>=1){phase-=std::floor(phase);held=std::round(source*levels)/levels;float encoded=source/(.5f+.5f*std::abs(source));float q=std::round(encoded*levels)/levels;warm=.5f*q/std::max(.5f,1-.5f*std::abs(q));}
  if(other>=1){other-=std::floor(other);second=std::round(source*levels)/levels;}
  phase+=map.rate/sr;other+=map.rate/sr*(.69+.12*m*shape.features[1]);
  body+=.07f*(warm-body);
  float processed=weights[0]*(.65f*std::round(source*levels)/levels+.35f*held)+weights[1]*(.6f*held+.4f*second)+weights[2]*(.7f*warm+.3f*body)+weights[3]*held;
  // Broad, fixed spectral bands: temporal DNA changes gains, not sweeping high-Q peaks.
  const float c[]{.035f,.12f,.32f};float previous=0,delta=0;
  for(int i=0;i<3;++i){colour[i]+=c[i]*(processed-colour[i]);float band=colour[i]-previous;previous=colour[i];delta+=(shape.bands[std::size_t(3+i*3)]-.45f)*band;}
  processed+=m*.8f*delta;
  float residual=processed-x;
  // Difference-path filtering retains the original LF excitation, without bass EQ boost.
  float k=1-std::exp(-6.2831853f*210/sr);hp1+=k*(residual-hp1);float high=residual-hp1;hp2+=k*(high-hp2);high-=hp2;
  lp+=.48f*(high-lp);float out=x+amount*.85f*lp;
  if(std::abs(out)>.96f)out=std::copysign(.96f+.04f*std::tanh((std::abs(out)-.96f)/.04f),out);
  return amount==0?x:out;
 }
};
static std::vector<float> input(int kind){auto a=bass(kind==0?0:kind==1?2:kind==4?3:1,sr*2);for(int i=0;i<int(a.size());++i){float t=float(i)/sr;if(kind==2)a[i]=.23f*std::sin(6.2831853f*55*t+2.2f*std::sin(6.2831853f*137*t))+.1f*std::sin(6.2831853f*330*t);if(kind==3)a[i]=.32f*std::tanh(5*a[i]);if(kind==5)a[i]=.3f*(2/std::acos(-1.f))*std::asin(std::sin(6.2831853f*55*t));}return a;}
static std::vector<float> proposed(const std::vector<float>&a,float x,float y,float d,float m,const DnaProfile*p){Candidate fx;std::vector<float>b;for(std::size_t i=0;i<a.size();++i){float z=fx.tick(a[i],x,y,d,m,p,double(i)/sr);require(std::isfinite(z)&&std::abs(z)<=1,"Candidate instability");b.push_back(z);}return b;}
static void append(std::vector<float>& montage,const std::vector<float>&a){auto b=match(a);montage.insert(montage.end(),b.begin(),b.end());montage.resize(montage.size()+sr/4,0);}
int main(int argc,char**argv){std::filesystem::path dir=argv[1];std::filesystem::create_directories(dir);auto metal=analyse(reference(0)),vocal=analyse(reference(1)),noise=analyse(reference(2));std::vector<float> dry;
 for(int kind=0;kind<6;++kind)append(dry,input(kind));wav(dir/"00-inputs.wav",dry);
 for(int q=0;q<4;++q)for(int intensity=0;intensity<2;++intensity){float z=(intensity?.9f:.5f)*.70710678f,x=q&1?z:-z,y=q&2?z:-z;std::vector<float> old,newer;for(int kind=0;kind<6;++kind){auto a=input(kind);append(old,render(a,x,y,nullptr,0,1,4,0));append(newer,proposed(a,x,y,0,0,nullptr));}std::string stem="quadrant-"+std::to_string(q)+(intensity?"-high":"-mid");wav(dir/(stem+"-v042.wav"),old);wav(dir/(stem+"-candidate.wav"),newer);}
 for(int d=0;d<3;++d){std::vector<float> old,newer;for(int kind=0;kind<6;++kind){auto a=input(kind);append(old,render(a,.55f,-.45f,nullptr,0,1,4,d*18.f));append(newer,proposed(a,.55f,-.45f,d*.5f,0,nullptr));}wav(dir/("drive-"+std::to_string(d*50)+"-v042.wav"),old);wav(dir/("drive-"+std::to_string(d*50)+"-candidate.wav"),newer);}
 int index=0;for(auto*p:{&metal,&vocal,&noise}){std::vector<float> old,newer;for(int m=0;m<3;++m){auto a=input(1);append(old,render(a,.55f,-.45f,p,m*.5f,1,4));append(newer,proposed(a,.55f,-.45f,0,m*.5f,p));}wav(dir/("dna-"+std::to_string(index)+"-v042.wav"),old);wav(dir/("dna-"+std::to_string(index++)+"-candidate.wav"),newer);}
 for(int q=0;q<4;++q){auto a=std::vector<float>(sr,0);require(proposed(a,q&1?.6f:-.6f,q&2?.6f:-.6f,1,1,&metal)==a,"Silence failed");}auto a=input(1);require(proposed(a,0,0,1,1,&vocal)==a,"Dry centre failed");require(proposed(a,.5f,-.5f,0,0,&metal)==proposed(a,.5f,-.5f,0,0,nullptr),"DNA zero mismatch");std::cout<<"Candidate listening renders complete; silence, centre, zero-DNA and finite bounds passed. Not plugin integration or RT certification.\n";}
