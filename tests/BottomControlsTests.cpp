#include "ControlFixtures.h"
#include "fixtures/v047/CrushDSP.h"

struct DigitalRoutingProbe {
    static float bits(const CrushDSP& d) { return 1+std::log2(d.sineCrush.crusher.modern[0]); }
    static float rate(const CrushDSP& d) { return sr*d.sineCrush.crusher.modern[1]; }
    static float held(const CrushDSP& d) { return d.sineCrush.crusher.held[0]; }
};
static CrushDSP::Parameters parameters(float x,float y,int model,float drive=0,float mutation=0) {
    auto m=XYMapping::map(x,y,sr,model);CrushDSP::Parameters p;
    p.processingVersion=model;p.centered=true;p.signedX=x;p.signedY=y;p.activity=m.activity;
    p.bits=m.bits;p.rate=m.rate;p.drive=drive;p.mutate=mutation;p.mix=1;p.output=0;return p;
}
static double bodyRms(const std::vector<float>& a) {
    double low=0,power=0;const double k=1-std::exp(-6.28318530718*180/sr);
    for(std::size_t i=0;i<a.size();++i){low+=k*(a[i]-low);if(i>sr/10)power+=(a[i]-low)*(a[i]-low);}
    return std::sqrt(power/(a.size()-sr/10));
}
static double textureDistance(const std::vector<float>& a,const std::vector<float>& b) {
    // Integrate all artifact energy, including inharmonic hold-clock sidebands;
    // harmonic-only bins miss the character of deliberate digital decimation.
    const float* pa[]{a.data()};const float* pb[]{b.data()};
    auto x=DnaSpectrum::analyse(pa,1,a.size(),sr,true),y=DnaSpectrum::analyse(pb,1,b.size(),sr,true);
    double d=0;for(std::size_t i=1;i<x.bands.size();++i)d+=std::abs(x.bands[i]-y.bands[i]);return d;
}
static std::array<float,2> effective(float x,float y,float drive,const DnaProfile* dna=nullptr,float mutation=0) {
    CrushDSP d;d.prepare(sr);auto p=parameters(x,y,10,drive,mutation);
    for(float v:bass(2)){float f[]{v,v};d.processFrame(f,2,p,dna);}
    return {DigitalRoutingProbe::bits(d),DigitalRoutingProbe::rate(d)};
}
static DnaProfile ordinary(float hz,float gain=1) {
    std::vector<float>a(sr);for(int i=0;i<sr;++i)a[i]=gain*.3f*std::sin(6.28318530718f*hz*i/sr);
    const float* ptr[]{a.data()};return DnaProfile::analyse(ptr,1,a.size(),sr,true,true);
}
int main() {
    const auto simple=bass(0),filtered=bass(2);const auto moving=analyse(reference(1)),dark=ordinary(110),bright=ordinary(1200);
    // Frozen shipped engine: legacy recall must remain exact even with active controls.
    for(float drive:{0.f,18.f,36.f})for(const DnaProfile* dna:{static_cast<const DnaProfile*>(nullptr),&dark,&moving}) {
        CrushDSP now;Legacy047CrushDSP old;now.prepare(sr);old.prepare(sr);
        auto p=parameters(-.4f,.4f,9,drive,1);Legacy047CrushDSP::Parameters q;
        q.processingVersion=9;q.centered=true;q.signedX=p.signedX;q.signedY=p.signedY;q.activity=p.activity;
        q.bits=p.bits;q.rate=p.rate;q.drive=drive;q.mutate=1;q.mix=1;q.output=0;
        for(float v:filtered){float a[]{v,v},b[]{v,v};now.processFrame(a,2,p,dna);old.processFrame(b,2,q,dna);require(a[0]==b[0],"v047 legacy recall changed");}
    }
    // Approved upper-half base sound stays sample-exact, not just spectrally similar.
    for(float x:{-.7f,-.35f,0.f,.35f,.7f})for(float y:{-.7f,-.35f,0.f})if(std::hypot(x,y)<=1)
        for(int kind=0;kind<4;++kind){auto input=bass(kind);require(render(input,x,y,nullptr,0,1,9)==render(input,x,y,nullptr,0,1,10),"Approved upper-half XY changed");}
    for(float v:{.2f,.35f,.6f}) {
        const auto left=XYMapping::map(-v,v,sr,10),right=XYMapping::map(v,v,sr,10),oldLeft=XYMapping::map(-v,v,sr,9);
        std::cout<<"Lower +/-"<<v<<": BL "<<left.bits<<" bits/"<<left.rate<<" Hz; BR "<<right.bits<<" bits/"<<right.rate<<" Hz\n";
        require(left.bits<oldLeft.bits&&right.rate<left.rate,"Lower character coefficients overlap");
        for(int kind=0;kind<4;++kind){auto a=bass(kind),l=render(a,-v,v,nullptr,0,1,10),r=render(a,v,v,nullptr,0,1,10);
            std::cout<<" lower input "<<kind<<" artifact bands="<<textureDistance(l,r)<<" waveform="<<difference(match(l),match(r))<<'\n';
            require(textureDistance(l,r)>.05&&difference(match(l),match(r))>.15,"Lower regions lack distinct spectral/temporal texture");
            require(magnitude(l,55)>magnitude(a,55)*.75&&magnitude(r,55)>magnitude(a,55)*.75,"Lower regions lost fundamental weight");}
    }
    for(auto xy:{std::array<float,2>{-.35f,-.35f},{.35f,-.35f},{-.35f,.35f},{.35f,.35f}}) {
        std::array<std::vector<float>,3> driveClips,mutateClips;
        std::array<std::array<float,2>,3> controls;
        for(int i=0;i<3;++i){controls[i]=effective(xy[0],xy[1],i*18.f);
            driveClips[i]=match(render(simple,xy[0],xy[1],nullptr,0,1,10,i*18.f));
            mutateClips[i]=match(render(filtered,xy[0],xy[1],&dark,i*.5f,1,10));}
        require(controls[0][0]>controls[1][0]&&controls[1][0]>=controls[2][0],"DRIVE fails to deepen actual quantization");
        require(controls[0][1]>controls[1][1]&&controls[1][1]>controls[2][1],"DRIVE fails to reduce actual hold rate");
        std::cout<<"XY "<<xy[0]<<","<<xy[1]<<" DRIVE matched body RMS "<<bodyRms(driveClips[0])<<","<<bodyRms(driveClips[1])<<","<<bodyRms(driveClips[2])<<"; DNA changes "<<difference(mutateClips[0],mutateClips[1])<<","<<difference(mutateClips[1],mutateClips[2])<<'\n';
        std::cout<<" DRIVE changes "<<difference(driveClips[0],driveClips[1])<<","<<difference(driveClips[1],driveClips[2])<<" harmonic distribution "<<spectralDistance(driveClips[0],driveClips[2])<<'\n';
        require(difference(driveClips[0],driveClips[1])>.2&&difference(driveClips[1],driveClips[2])>.2,"DRIVE only changes level");
        require(bodyRms(driveClips[1])>bodyRms(driveClips[0])*1.08&&bodyRms(driveClips[2])>bodyRms(driveClips[1])*1.15,"DRIVE fails to increase level-matched digital artifact energy");
        require(spectralDistance(driveClips[0],driveClips[2])>.04,"DRIVE fails to redistribute harmonics");
        require(difference(mutateClips[0],mutateClips[1])>.2&&difference(mutateClips[1],mutateClips[2])>.2,"Ordinary DNA too weak at half/full MUTATE");
        auto zero=render(filtered,xy[0],xy[1],&dark,0,1,10),none=render(filtered,xy[0],xy[1],nullptr,1,1,10);
        require(zero==none,"DNA leaks at MUTATE zero");
        for(const auto* profile:{&dark,&bright,&moving}) {
            auto full=render(filtered,xy[0],xy[1],profile,1,1,10),half=render(filtered,xy[0],xy[1],profile,.5,1,10);
            std::cout<<" DNA spectral "<<spectralDistance(zero,half)<<","<<spectralDistance(zero,full)<<" waveform "<<difference(match(zero),match(half))<<","<<difference(match(zero),match(full))<<'\n';
            require(spectralDistance(zero,full)>.035&&difference(match(half),match(full))>.2,"DNA lacks spectral and temporal transformation");
            const auto c0=effective(xy[0],xy[1],0),c50=effective(xy[0],xy[1],0,profile,.5),c100=effective(xy[0],xy[1],0,profile,1);
            require(std::abs(std::log(c100[1]/c0[1]))>std::abs(std::log(c50[1]/c0[1]))+.02,"MUTATE depth does not reach the crusher clock");
            require(magnitude(full,55)>magnitude(filtered,55)*.7,"DNA removed bass weight");
        }
        require(spectralDistance(render(filtered,xy[0],xy[1],&dark,1,1,10),render(filtered,xy[0],xy[1],&bright,1,1,10))>.06,"Contrasting references produce the same spectral shape");
    }
    auto quiet=ordinary(110,.01f);require(std::abs(quiet.spectrum.peaks[0]-dark.spectrum.peaks[0])<.1,"Quiet reference normalization regressed");
    for(float x:{-.6f,.6f}) {
        require(render(filtered,x,.35f,&dark,1,1,10)==render(filtered,x,.35f,&quiet,1,1,10)||difference(render(filtered,x,.35f,&dark,1,1,10),render(filtered,x,.35f,&quiet,1,1,10))<.005,"Reference loudness changes DNA character");
        require(spectralDistance(render(filtered,x,.35f,&moving,1,.5,10),render(filtered,x,.35f,&moving,1,2,10))>.005,"DNA temporal motion/SPEED disconnected");
    }
    double peak=0;
    for(int kind=0;kind<4;++kind)for(float x:{-.7f,0.f,.7f})for(float y:{-.7f,0.f,.7f})if(std::hypot(x,y)<=1)
        for(float drive:{0.f,18.f,36.f})for(const DnaProfile* profile:{static_cast<const DnaProfile*>(nullptr),&dark,&bright,&moving}) {
            auto a=bass(kind);for(auto&v:a)v*=2;
            for(float v:render(a,x,y,profile,1,1,10,drive))peak=std::max(peak,double(std::abs(v)));
        }
    require(peak<=1,"Hot-input output exceeds unity");
    require(render(filtered,0,0,&moving,1,1,10,36)==filtered,"Dry center changed with active controls");
    for(double fs:{44100.,48000.,96000.}){CrushDSP d;d.prepare(fs);auto p=parameters(.6f,.6f,10,36,1);p.rate=32;
        for(int i=0;i<int(fs/10);++i){float f[2]{};d.processFrame(f,2,p,&moving);require(f[0]==0,"DNA generates sound during silence");}
        for(int i=0;i<int(fs/10);++i){float v=.3f*std::sin(i*.02f),f[]{v,v};d.processFrame(f,2,p,&moving);}
        for(int i=0;i<int(fs);++i){float f[2]{};d.processFrame(f,2,p,&moving);if(i>fs*.9)require(std::abs(f[0])<1.e-7f,"Digital modulation continues without input");}}
    std::cout<<"PASS v047 recall, unchanged upper base, distinct lower coefficients/textures, actual DRIVE/DNA path and stability; hot peak "<<peak<<". FL Studio/listening remain manual.\n";
}
