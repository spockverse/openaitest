#include "ControlFixtures.h"
#include "fixtures/CrushDSP-v042.h"
int main(){
 auto a=bass(1);auto dna=analyse(reference(0));
 for(auto xy:{std::array<float,2>{-.55f,.45f},{.55f,.45f},{-.55f,-.45f},{.55f,-.45f}}){
   auto no=render(a,xy[0],xy[1],nullptr,1,1,5);
   require(no==render(a,xy[0],xy[1],&dna,0,1,5),"DNA leaks at zero MUTATE");
   require(difference(match(no),match(render(a,xy[0],xy[1],nullptr,1,1,5,36)))>.03,"DRIVE only changes level");
   require(difference(match(no),match(render(a,xy[0],xy[1],&dna,1,1,5)))>.03,"DNA disconnected");
   require(difference(no,a)>.03,"XY disabled at DRIVE zero");
 }
 require(render(a,0,0,&dna,1,1,5,36)==a,"Dry center changed");
 for(double fs:{44100.,48000.,96000.,192000.}){
   CrushDSP fx;fx.prepare(fs);CrushDSP::Parameters p;p.processingVersion=5;p.centered=true;p.activity=1;p.bits=2;p.rate=500;p.drive=36;p.mix=1;p.output=0;
   for(int i=0;i<10000;++i){float f[2]{};fx.processFrame(f,2,p);require(f[0]==0&&f[1]==0,"Silent carrier leakage");}
 }
 CrushDSP current;Legacy042CrushDSP old;current.prepare(sr);old.prepare(sr);
 CrushDSP::Parameters p;p.processingVersion=4;p.centered=true;p.activity=.7f;p.bits=4;p.rate=3000;p.drive=18;p.mutate=1;
 Legacy042CrushDSP::Parameters q;q.processingVersion=4;q.centered=true;q.activity=.7f;q.bits=4;q.rate=3000;q.drive=18;q.mutate=1;
 for(float v:a){float x[]{v,v},y[]{v,v};current.processFrame(x,2,p,&dna);old.processFrame(y,2,q,&dna);require(x[0]==y[0],"v042 legacy output changed");}
 std::cout<<"PASS model5 center, silence/rates, DRIVE, DNA zero/full, XY at DRIVE0, exact v042 recall\n";
}
