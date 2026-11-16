#include "ControlFixtures.h"
#include "fixtures/v060/CrushDSP.h"
#include <complex>

// Synthetic, amplitude-stable sources: static LP bass, moving LP bass, moving
// broad formants, and short pitched stabs. No reference recordings are retained.
static std::vector<float> source(int kind) {
    std::vector<float> a(3*sr);double phase=0;
    for(int i=0;i<int(a.size());++i){
        const double t=double(i)/sr,cut=900*std::exp((kind==1?.95:0)*std::sin(6.28318530718*1.25*t));
        const double f1=700*std::exp(.6*std::sin(6.28318530718*.9*t)),f2=1900*std::exp(.35*std::sin(6.28318530718*1.3*t));
        const double hz=kind==3&&i/(sr/4)%4==1?73.416:55.;phase+=hz/sr;phase-=std::floor(phase);
        double v=0;
        for(int h=1;h<=64;++h){const double f=h*hz;
            const double weight=kind==2?.12+1.2*std::exp(-std::pow(std::log(f/f1)/.23,2))+std::exp(-std::pow(std::log(f/f2)/.25,2)):1/std::sqrt(1+std::pow(f/cut,4));
            v+=.22/h*weight*std::sin(6.28318530718*h*phase);}
        const double env=kind==3?(1-std::exp(-std::fmod(t,.25)/.002))*std::exp(-std::fmod(t,.25)/.075):.8;
        a[i]=float(v*env);
    }
    return a;
}
struct Motion {std::vector<float> ratio,energy;};
// Test-only, windowed spectral measurements, independent of output gain.
static Motion motion(const std::vector<float>& a){
    constexpr int n=2048;Motion m;std::array<std::complex<double>,n> z{};
    for(int start=sr/4;start+n<int(a.size());start+=512){
        double energy=0;
        for(int i=0;i<n;++i){energy+=a[start+i]*a[start+i];z[i]=a[start+i]*(.5-.5*std::cos(6.28318530718*i/(n-1)));}
        for(int i=1,j=0;i<n;++i){int bit=n>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(z[i],z[j]);}
        for(int len=2;len<=n;len<<=1){const auto step=std::polar(1.,-6.28318530718/len);
            for(int base=0;base<n;base+=len){std::complex<double> w=1;for(int j=0;j<len/2;++j){const auto x=z[base+j],y=z[base+j+len/2]*w;z[base+j]=x+y;z[base+j+len/2]=x-y;w*=step;}}}
        double lower=0,upper=0;
        for(int k=1;k<n/2;++k){double hz=double(k)*sr/n,power=std::norm(z[k]);if(hz>=500&&hz<1200)lower+=power;if(hz>=1200&&hz<3500)upper+=power;}
        m.ratio.push_back(float(std::log((lower+1.e-12)/(upper+1.e-12))));m.energy.push_back(float(std::sqrt(energy/n)));
    }
    return m;
}
static double correlation(const std::vector<float>& a,const std::vector<float>& b){
    double x=0,y=0;for(std::size_t i=0;i<a.size();++i){x+=a[i]/a.size();y+=b[i]/b.size();}
    double xx=0,yy=0,xy=0;for(std::size_t i=0;i<a.size();++i){xx+=(a[i]-x)*(a[i]-x);yy+=(b[i]-y)*(b[i]-y);xy+=(a[i]-x)*(b[i]-y);}
    return xy/std::sqrt(std::max(1.e-30,xx*yy));
}
static double deviation(const std::vector<float>& a){double mean=0,s=0;for(float x:a)mean+=x/a.size();for(float x:a)s+=(x-mean)*(x-mean);return std::sqrt(s/a.size());}
static void legacyRecall(const DnaProfile& dna){
    for(int version=1;version<=11;++version){
        CrushDSP now;Legacy060CrushDSP old;now.prepare(sr);old.prepare(sr);
        CrushDSP::Parameters p;Legacy060CrushDSP::Parameters q;
        for(int i=0;i<18000;++i){
            if(i%211==0){const float x=.7f*std::sin(i*.0007f),y=.7f*std::cos(i*.0011f);const auto m=XYMapping::map(x,y,sr,version);
                p.processingVersion=q.processingVersion=version;p.centered=q.centered=version>=2;p.rhythmic=q.rhythmic=version>=11;
                p.signedX=q.signedX=x;p.signedY=q.signedY=y;p.activity=q.activity=m.activity;p.bits=q.bits=m.bits;p.rate=q.rate=m.rate;
                p.drive=q.drive=float(i%37);p.mutate=q.mutate=float(i%101)/100;p.pitch=q.pitch=float(i%49)-24;p.output=q.output=-3;p.mix=q.mix=.8f;}
            float a[]{.3f*std::sin(i*.031f),.2f*std::sin(i*.014f)},b[]{a[0],a[1]};
            const auto* profile=i/4000%2?&dna:nullptr;now.processFrame(a,2,p,profile);old.processFrame(b,2,q,profile);
            require(a[0]==b[0]&&a[1]==b[1],"Saved processing version changed under automation/DNA replacement");
        }
    }
}
int main(int argc,char**argv){
    std::array<std::vector<float>,4> inputs;for(int k=0;k<4;++k)inputs[k]=source(k);
    auto dna=analyse(reference(0)),contrast=analyse(reference(1));legacyRecall(dna);
    for(float x:{-.7f,-.4f,0.f,.4f,.7f})for(float y:{-.7f,-.4f,0.f,.4f,.7f}){
        const auto a=XYMapping::map(x,y,sr,11),b=XYMapping::map(x,y,sr,12);require(a.bits==b.bits&&a.rate==b.rate&&a.activity==b.activity,"XY coordinate mapping changed");}
    std::vector<float> audition;
    for(int q=0;q<4;++q){const float x=q&1?.4f:-.4f,y=q&2?.4f:-.4f;
        for(int kind=0;kind<4;++kind){
            const auto& input=inputs[kind];const auto before=render(input,x,y,nullptr,0,1,11),after=render(input,x,y,nullptr,0,1,12);
            const auto in=motion(input),a=motion(before),b=motion(after);
            if(kind==1){const double old=correlation(in.ratio,a.ratio),now=correlation(in.ratio,b.ratio);
                require(now>.25&&now>old+.4,"Filter movement remains buried beneath aliases");
                require(deviation(b.ratio)>deviation(a.ratio)*1.3,"Filter movement depth did not recover");
                std::cout<<"Quadrant "<<q<<" filter movement correlation "<<old<<" -> "<<now<<'\n';}
            if(kind==2)require(correlation(in.ratio,b.ratio)>.5,"Vowel movement is disconnected from source");
            if(kind==3)require(correlation(in.energy,b.energy)>.97,"Rhythmic articulation lost");
            require(magnitude(after,55)>.75*magnitude(input,55),"Bass foundation lost");
            require(difference(match(after),match(input))>.08,"Correction only added dry signal");
            if(argc>1&&q==1){for(const auto* sound:{&before,&after}){auto a=match(*sound);const int fade=240;
                for(auto& v:a)v*=.5f; // Equal RMS with headroom for sparse stabs.
                for(int i=0;i<fade;++i){a[i]*=float(i)/fade;a[a.size()-1-i]*=float(i)/fade;}
                audition.insert(audition.end(),a.begin(),a.end());audition.resize(audition.size()+sr/5);}}
        }
    }
    for(auto* profile:{&dna,&contrast})for(int kind:{1,2,3}){
        const auto out=render(inputs[kind],.4f,-.4f,profile,1,1,12);const auto in=motion(inputs[kind]),m=motion(out);
        if(kind==3)require(correlation(in.energy,m.energy)>.9,"DNA masks rhythmic articulation");
        else require(deviation(m.ratio)>.15,"DNA leaves static spectral wash");
        require(render(inputs[kind],.4f,-.4f,profile,0,1,12)==render(inputs[kind],.4f,-.4f,nullptr,1,1,12),"DNA leaks at MUTATE zero");
    }
    require(spectralDistance(render(inputs[2],.4f,-.4f,&dna,1,1,12),render(inputs[2],.4f,-.4f,&contrast,1,1,12))>.05,"DNA profiles lost independent character");
    for(float gain:{.25f,1.f,2.f}){auto input=inputs[1];for(auto& v:input)v*=gain;
        const auto out=render(input,.4f,-.4f,nullptr,0,1,12);
        require(correlation(motion(input).ratio,motion(out).ratio)>.45,"Input-level calibration loses moving harmonics");}
    for(float drive:{18.f,36.f})require(difference(match(render(inputs[1],.4f,-.4f,nullptr,0,1,12)),match(render(inputs[1],.4f,-.4f,nullptr,0,1,12,drive)))>.15,"DRIVE contribution lost");
    require(render(inputs[1],0,0,&dna,1,1,12,36)==inputs[1],"Dry center changed");
    for(double fs:{44100.,48000.,96000.})for(int block:{64,128,512}){
        CrushDSP d;d.prepare(fs);CrushDSP::Parameters p;p.processingVersion=12;p.centered=p.rhythmic=true;p.mix=1;p.output=0;
        for(int i=0;i<int(fs/2);++i){if(i%block==0){p.signedX=.95f*std::sin(i*.001f);p.signedY=.95f*std::cos(i*.0017f);const auto m=XYMapping::map(p.signedX,p.signedY,fs,12);p.activity=m.activity;p.bits=m.bits;p.rate=m.rate;p.drive=float(i%37);p.mutate=float(i%101)/100;}
            float f[]{i<fs/4?.7f*std::sin(float(i)*.031f):0,0};d.processFrame(f,2,p,i/2000%2?&dna:nullptr);
            require(std::isfinite(f[0])&&std::abs(f[0])<1.8f&&f[1]==0,"Automation/silence/crosstalk instability");}
    }
    if(argc>1)wav(argv[1],audition);
    std::cout<<"PASS input filter/formant movement, transient articulation, fundamentals, independent DNA, dry center, multirate automation and exact versions 1-11. Metrics do not establish subjective quality.\n";
}
