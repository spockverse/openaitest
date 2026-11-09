#include "ControlFixtures.h"
#include "fixtures/v048/CrushDSP.h"
struct DigitalRoutingProbe {
    static float resonance(const CrushDSP& d){return std::atan(d.tonal.modern[0])*sr/3.14159265359f;}
    static float carrier(const CrushDSP& d){return d.sineCrush.control[0];}
};
static DnaProfile fixedProfile(float hz=300){auto p=analyse(reference(0));p.spectrum.peaks={hz,hz*1.7f,hz*2.8f};p.spectrum.prepareDerived();for(auto& f:p.spectralFrames)f=DnaTemporalFrame::from(p.spectrum);return p;}
static CrushDSP::Parameters controls(float pitch=0){auto m=XYMapping::map(.4f,-.35f,sr,11);CrushDSP::Parameters p;p.processingVersion=11;p.centered=true;p.signedX=.4;p.signedY=-.35;p.activity=m.activity;p.bits=m.bits;p.rate=m.rate;p.drive=18;p.mutate=1;p.mix=1;p.output=0;p.rhythmic=true;p.regionBeats=1;p.bpm=120;p.pitch=pitch;return p;}
int main(){
 for(double fs:{44100.,48000.,96000.})for(double bpm:{80.,120.,174.})for(double beats:DnaTiming::beats){DnaRhythmClock c;c.prepare(fs);c.configure(bpm,beats);
  const int n=int(std::round(fs*60*beats/bpm));for(int i=0;i<n;++i)c.advance(true,0);
  const double expected=std::fmod(n*bpm/(60*fs*beats),1.);const double diff=std::abs(c.position()-expected);
  require(std::min(diff,1-diff)<1.e-8,"Rhythmic region duration inaccurate");const auto held=c.position();for(int i=0;i<123;++i)c.advance(false,0);require(c.position()==held,"Stopped clock moves");
  c.seek(7.3,beats,0);require(std::abs(c.position()-std::fmod(7.3/beats,1.))<1.e-8,"PPQ seek wrong");
  c.reset();c.configure(bpm,beats);for(int i=0;i<n;++i)c.advance(true,1);require(c.position()==0,"Unarmed retrigger runs");c.trigger();for(int i=0;i<n+2;++i)c.advance(true,1);require(c.position()==1,"Retrigger fails to hold END");c.advance(true,1);require(c.position()==1,"Retrigger loops");c.trigger();require(c.position()==0,"Retrigger fails to restart");}
 for(float hz:{30.f,55.f,110.f,700.f}){DnaOnset d;d.prepare(sr);int hits=0;for(int i=0;i<sr*3;++i){float x=.3f*std::sin(6.28318530718f*hz*i/sr);hits+=d.observe(x*x);}require(hits==1,"Sustained waveform cycles retrigger");}
 {DnaOnset d;d.prepare(sr);int hits=0;for(int i=0;i<sr*2;++i){const int n=i%(sr/4);float x=n<sr/10?.3f*std::sin(i*.014f):0;hits+=d.observe(x*x);}require(hits==8,"Rhythmic audio onsets missed/doubled");d.reset();unsigned r=7;for(int i=0;i<sr;++i){r=r*1664525+1013904223;float x=float((r>>8)/16777216.)*.001f;require(!d.observe(x*x),"Background noise retriggers");}}
 auto profile=fixedProfile();auto source=bass(2);CrushDSP d;d.prepare(sr);auto p=controls();
 for(int i=0;i<sr/2+1;++i){float f[2]{};d.processFrame(f,2,p,&profile);}require(d.getPatternPosition()<1.e-8,"Native LOOP not quarter-note synchronized through silence");
 p.patternEnabled=false;auto held=d.getPatternPosition();for(int i=0;i<100;++i){float f[2]{};d.processFrame(f,2,p,&profile);}require(std::abs(d.getPatternPosition()-held)<1.e-4,"Native stopped playhead moves");
 p.mode=1;p.patternEnabled=true;p.audioTriggers=false;for(int i=0;i<200;++i){if(i==63)d.triggerRhythm();float f[2]{};d.processFrame(f,2,p,&profile);if(i==63)require(d.getPatternPosition()==0,"MIDI-offset retrigger delayed");}
 std::array<float,3> frequencies{},carriers{};std::array<double,3> phases{};
 for(int i=0;i<3;++i){CrushDSP c;c.prepare(sr);auto q=controls((i-1)*12.f);std::vector<float>out;
  for(float v:source){float f[]{v,v};c.processFrame(f,2,q,&profile);out.push_back(f[0]);}
  frequencies[i]=DigitalRoutingProbe::resonance(c);carriers[i]=DigitalRoutingProbe::carrier(c);phases[i]=c.getPatternPosition();require(magnitude(out,55)>magnitude(source,55)*.7,"Pitch removes source fundamental");}
 require(std::abs(frequencies[1]/frequencies[0]-2)<.01&&std::abs(frequencies[2]/frequencies[1]-2)<.01,"Pitch not semitone-based spectral transposition");
 require(std::abs(carriers[1]/carriers[0]-2)<.01&&std::abs(carriers[2]/carriers[1]-2)<.01,"Pitch fails to transpose sample-derived carrier");require(phases[0]==phases[1]&&phases[1]==phases[2],"Pitch changes SPEED duration");
 for(float pitch:{-24.f,24.f}){CrushDSP a,b;a.prepare(sr);b.prepare(sr);auto qa=controls(),qb=controls(pitch);for(float v:source){float x[]{v,v},y[]{v,v};a.processFrame(x,2,qa);b.processFrame(y,2,qb);require(x[0]==y[0],"Pitch changes core audio without DNA");}}
 {CrushDSP a,b;a.prepare(sr);b.prepare(sr);auto qa=controls(7),qb=qa;qb.regionBeats=.25;for(float v:source){float x[]{v,v},y[]{v,v};a.processFrame(x,2,qa,&profile);b.processFrame(y,2,qb,&profile);}require(std::abs(DigitalRoutingProbe::resonance(a)-DigitalRoutingProbe::resonance(b))<.01,"SPEED transposes spectral characteristics");}
 for(int kind=0;kind<4;++kind)for(auto xy:{std::array<float,2>{-.35f,-.35f},{.35f,-.35f},{-.35f,.35f},{.35f,.35f}})for(float drive:{0.f,18.f,36.f}){
  auto a=bass(kind);require(render(a,xy[0],xy[1],nullptr,0,1,10,drive)==render(a,xy[0],xy[1],nullptr,0,1,11,drive),"Approved XY/DRIVE core changed");
  require(render(a,xy[0],xy[1],&profile,0,1,11,drive)==render(a,xy[0],xy[1],nullptr,0,1,11,drive),"MUTATE zero leaks DNA");}
 auto contrast=analyse(reference(1));for(auto* dna:{&profile,&contrast}){auto dry=match(render(source,.4,-.35,nullptr,0,1,11));auto a=match(render(source,.4,-.35,dna,.5,1,11)),b=match(render(source,.4,-.35,dna,1,1,11));require(difference(dry,a)>.2&&difference(a,b)>.2&&spectralDistance(dry,b)>.05,"Mutation too weak or only level");}
 require(spectralDistance(render(source,.4,-.35,&profile,1,1,11),render(source,.4,-.35,&contrast,1,1,11))>.05,"DNA references indistinct");
 CrushDSP now;Legacy048CrushDSP old;now.prepare(sr);old.prepare(sr);auto q=controls();q.processingVersion=10;q.rhythmic=false;q.drive=18;Legacy048CrushDSP::Parameters oldQ;oldQ.processingVersion=10;oldQ.centered=true;oldQ.signedX=q.signedX;oldQ.signedY=q.signedY;oldQ.activity=q.activity;oldQ.bits=q.bits;oldQ.rate=q.rate;oldQ.drive=q.drive;oldQ.mutate=1;oldQ.mix=1;oldQ.output=0;
 for(float v:source){float a[]{v,v},b[]{v,v};now.processFrame(a,2,q,&profile);old.processFrame(b,2,oldQ,&profile);require(a[0]==b[0],"v048 legacy sound changed");}
 std::cout<<"PASS rhythmic duration/transport, one-shot END, sustained/noise rejection, MIDI offset, spectral pitch ratio, independent timing, XY/DRIVE identity, mutation and v048 recall.\n";
}
