#include "ControlFixtures.h"
#include "fixtures/CrushDSP-v042.h"
#include "fixtures/SineCrush-v043.h"
int main(){
 auto a=bass(1);auto dna=analyse(reference(0));
 for(int model:{5,6})for(auto xy:{std::array<float,2>{-.55f,.45f},{.55f,.45f},{-.55f,-.45f},{.55f,-.45f}}){
   auto no=render(a,xy[0],xy[1],nullptr,1,1,model);
   require(no==render(a,xy[0],xy[1],&dna,0,1,model),"DNA leaks at zero MUTATE");
   require(difference(match(no),match(render(a,xy[0],xy[1],nullptr,1,1,model,36)))>.03,"DRIVE only changes level");
   require(difference(match(no),match(render(a,xy[0],xy[1],&dna,1,1,model)))>.03,"DNA disconnected");
   require(difference(no,a)>.03,"XY disabled at DRIVE zero");
 }
 require(render(a,0,0,&dna,1,1,6,36)==a,"Dry center changed");
 for(double fs:{44100.,48000.,96000.,192000.}){
   CrushDSP fx;fx.prepare(fs);CrushDSP::Parameters p;p.processingVersion=6;p.centered=true;p.activity=1;p.bits=2;p.rate=500;p.drive=36;p.mix=1;p.output=0;
   for(int i=0;i<10000;++i){float f[2]{};fx.processFrame(f,2,p);require(f[0]==0&&f[1]==0,"Silent carrier leakage");}
 }
 CrushDSP current;Legacy042CrushDSP old;current.prepare(sr);old.prepare(sr);
 CrushDSP::Parameters p;p.processingVersion=4;p.centered=true;p.activity=.7f;p.bits=4;p.rate=3000;p.drive=18;p.mutate=1;
 Legacy042CrushDSP::Parameters q;q.processingVersion=4;q.centered=true;q.activity=.7f;q.bits=4;q.rate=3000;q.drive=18;q.mutate=1;
 for(float v:a){float x[]{v,v},y[]{v,v};current.processFrame(x,2,p,&dna);old.processFrame(y,2,q,&dna);require(x[0]==y[0],"v042 legacy output changed");}

 // Kernel version 5 remains sample-exact under changing controls, including
 // non-divisible control/block boundaries and sample-rate changes.
 for(double rate:{44100.,48000.,96000.}){
   SineCrush now;Legacy043SineCrush previous;now.prepare(rate);previous.prepare(rate);
   for(int i=0;i<20000;++i){if(i%193==0){float x=std::sin(i*.0007f)*.7f,y=std::cos(i*.0004f)*.7f;
       now.set(x,y,.7f,5,2500,.5f,1,.2f,193);previous.set(x,y,.7f,5,2500,.5f,1,.2f,193);}
     float x[]{a[std::size_t(i)%a.size()],-a[std::size_t(i)%a.size()]},y[]{x[0],x[1]};
     now.process(x,2);previous.process(y,2);require(x[0]==y[0]&&x[1]==y[1],"v043 kernel changed");}
 }
 std::array<std::vector<float>,4> quadrants;
 for(int q=0;q<4;++q){const float x=q&1?.35f:-.35f,y=q&2?-.3f:.3f;
   quadrants[q]=match(render(a,x,y,nullptr,0,1,6));
   const auto baseline=match(render(a,x,y,nullptr,0,1,5));
   std::cout<<"Intermediate quadrant "<<q<<" matched v5/v6 difference "<<difference(baseline,quadrants[q])<<'\n';
   require(difference(baseline,quadrants[q])>.05,"No effective modulation extension");
 }
 for(int q=0;q<4;++q)for(int r=q+1;r<4;++r)
   require(difference(quadrants[q],quadrants[r])>.05,"Intermediate XY regions collapsed");
 const auto sub=bass(0);double minimumFundamental=10,peak=0,baselinePeak=0;
 for(int q=0;q<4;++q){const float x=q&1?.65f:-.65f,y=q&2?-.6f:.6f;
   const auto out=render(sub,x,y,nullptr,0,1,6);
   minimumFundamental=std::min(minimumFundamental,magnitude(out,55)/magnitude(sub,55));
   for(int kind=0;kind<4;++kind){auto hot=bass(kind);for(float&v:hot)v*=2.5f;
     for(float drive:{0.f,18.f,36.f}){auto b=render(hot,x,y,&dna,1,1,6,drive);for(float v:b)peak=std::max(peak,double(std::abs(v)));
       for(float v:render(hot,x,y,&dna,1,1,5,drive))baselinePeak=std::max(baselinePeak,double(std::abs(v)));}}
 }
 require(minimumFundamental>.85,"Fundamental body lost");require(peak<1.2,"Uncontrolled hot-input peaks");
 auto vocal=analyse(reference(1)),noise=analyse(reference(2));
 std::array<std::vector<float>,3> mutations;
 for(int d=0;d<3;++d){const DnaProfile* profile=d==0?&dna:d==1?&vocal:&noise;
   const auto zero=match(render(a,.55f,.45f,profile,0,1,6));
   const auto half=match(render(a,.55f,.45f,profile,.5f,1,6));
   mutations[d]=match(render(a,.55f,.45f,profile,1,1,6));
   require(difference(zero,half)>.05&&difference(half,mutations[d])>.05,"DNA depth ineffective");
 }
 for(int d=0;d<3;++d)for(int e=d+1;e<3;++e)
   require(difference(mutations[d],mutations[e])>.1,"DNA profiles collapsed");
 std::cout<<"Fundamental retention minimum "<<minimumFundamental<<"; hot-input peak "<<peak<<" (v043 "<<baselinePeak<<")"<<'\n';
 std::cout<<"PASS model5/6 center, silence/rates, DRIVE, DNA zero/full, intermediate XY, exact v042/v043 recall. Metrics are not subjective quality verification.\n";
}
