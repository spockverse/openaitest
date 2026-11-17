#include "ControlFixtures.h"
#include "fixtures/v062/CrushDSP.h"
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
// Project onto two harmonics actually present in the excitation. Intentional
// carrier sidebands must not be mistaken for loss of the original movement.
static Motion sourceMotion(const std::vector<float>& a){
    constexpr int n=8192;Motion m;std::array<double,n> window{};
    for(int i=0;i<n;++i)window[i]=.5-.5*std::cos(6.28318530718*i/(n-1));
    const double c0=2*std::cos(6.28318530718*770/sr),c1=2*std::cos(6.28318530718*2200/sr);
    for(int start=sr/4;start+n<int(a.size());start+=1024){double s0=0,t0=0,s1=0,t1=0;
        for(int i=0;i<n;++i){const double v=a[start+i]*window[i],u0=v+c0*s0-t0,u1=v+c1*s1-t1;t0=s0;s0=u0;t1=s1;s1=u1;}
        const double p0=std::max(0.,s0*s0+t0*t0-c0*s0*t0),p1=std::max(0.,s1*s1+t1*t1-c1*s1*t1);
        m.ratio.push_back(float(std::log((p0+1.e-12)/(p1+1.e-12))));}
    return m;
}
struct DigitalRoutingProbe {
    static float frequency(const CrushDSP& a){return a.sineCrush.control[0];}
    static float shape(const CrushDSP& a){return a.sineCrush.control[3];}
    static float excitation(const CrushDSP& a){return a.excitationControl[3];}
    static void sameDna(const CrushDSP& a,const Legacy062CrushDSP& b){
        // Compare descriptor targets, not the filter's evolving input history
        // or the first ramp's initialization under different core bit depths.
        require(a.tonal.modernGoal==b.tonal.modernGoal,"DNA spectral targets changed");
        require(a.excitationControl==b.excitationControl,"DNA excitation controls changed at DRIVE zero");
        require(a.getPatternPosition()==b.getPatternPosition(),"DNA timing changed");
    }
};
static void enhancerChecks(const std::array<std::vector<float>,4>& inputs,const DnaProfile& dna,const DnaProfile& contrast){
    double worstError=0;
    for(auto xy:{std::array<float,2>{-.4f,-.4f},{.4f,-.4f},{-.4f,.4f},{.4f,.4f},{-.7f,-.7f},{.7f,-.7f},{-.7f,.7f},{.7f,.7f},{-1,0},{1,0},{0,-1},{0,1}}){
        const auto m=XYMapping::map(xy[0],xy[1],sr,13);
        require(m.bits>=8.5f&&m.rate>=8000,"Enhancer mapping reenters destructive precision/clock ranges");
        for(int k=0;k<4;++k){const auto& input=inputs[k];const auto out=render(input,xy[0],xy[1],nullptr,0,1,13);
            const double error=difference(match(out),match(input));worstError=std::max(worstError,error);
            require(error<.35,"Core digital artifacts dominate the source");
            const double gain=rms(out)/rms(input);require(gain>.8&&gain<1.2,"Core colour causes level loss/boost");
            require(magnitude(out,55)>.85*magnitude(input,55),"Enhancer loses fundamental weight");
            if(k==1||k==2)require(correlation(sourceMotion(input).ratio,sourceMotion(out).ratio)>.8,"Enhancer obscures original filter/formant movement");
            if(k==3)require(correlation(motion(input).energy,motion(out).energy)>.99,"Enhancer obscures transient articulation");
        }
    }
    const auto neutral=XYMapping::map(0,0,sr,13);
    require(neutral.bits==16&&neutral.rate==sr&&neutral.activity==0,"Neutral coordinates moved");
    require(render(inputs[1],0,0,&dna,1,1,13,36)==inputs[1],"Enhancer changes dry center");
    for(float drive:{0.f,18.f,36.f}){
        const auto a=render(inputs[2],.6f,-.6f,nullptr,1,1,13,drive),b=render(inputs[2],.6f,-.6f,&dna,0,1,13,drive);
        require(a==b,"DNA leaks into enhancer without mutation");
        require(difference(match(a),match(inputs[2]))<.4,"DRIVE overwhelms the clean tonal path");
    }
    auto a=match(render(inputs[2],.4f,-.4f,&dna,1,1,13)),b=match(render(inputs[2],.4f,-.4f,&contrast,1,1,13));
    require(spectralDistance(a,b)>.05,"DNA references no longer shape the tone");
    // Incoming-audio detectors, descriptor coefficients, timing, and MUTATE's
    // DNA excitation remain identical; only the core crush/DRIVE are retuned.
    CrushDSP now;Legacy062CrushDSP old;now.prepare(sr);old.prepare(sr);
    CrushDSP::Parameters p;Legacy062CrushDSP::Parameters q;
    p.processingVersion=13;q.processingVersion=12;p.centered=q.centered=true;p.rhythmic=q.rhythmic=true;
    p.signedX=q.signedX=.4f;p.signedY=q.signedY=-.4f;p.mutate=q.mutate=1;p.drive=q.drive=0;p.mix=q.mix=1;p.output=q.output=0;
    const auto n=XYMapping::map(.4f,-.4f,sr,13),o=XYMapping::map(.4f,-.4f,sr,12);
    p.bits=n.bits;p.rate=n.rate;p.activity=n.activity;q.bits=o.bits;q.rate=o.rate;q.activity=o.activity;
    for(float v:inputs[2]){float x[]{v,v},y[]{v,v};now.processFrame(x,2,p,&dna);old.processFrame(y,2,q,&dna);DigitalRoutingProbe::sameDna(now,old);}
    std::cout<<"Tonal enhancer worst level-matched core error "<<20*std::log10(worstError)<<" dB below source; DNA descriptor targets/excitation/timing unchanged.\n";
}
static void modulationChecks(const DnaProfile& dna){
    std::array<double,3> sidebands{};
    for(int i=0;i<3;++i){CrushDSP d;d.prepare(sr);CrushDSP::Parameters p;
        p.processingVersion=13;p.centered=true;p.signedX=std::log(2.f)/std::log(20.f);p.signedY=0;
        p.activity=std::abs(p.signedX);p.bits=16;p.rate=sr;p.drive=i*18.f;p.mutate=0;p.mix=1;p.output=0;
        std::vector<float> out;
        for(int n=0;n<sr*2;++n){float v=.1f*std::sin(6.28318530718f*1000*n/sr),f[]{v,v};d.processFrame(f,2,p);if(n>=sr)out.push_back(f[0]);}
        require(std::abs(DigitalRoutingProbe::frequency(d)-600)<.01,"X does not tune the actual carrier frequency");
        require(DigitalRoutingProbe::shape(d)==0&&DigitalRoutingProbe::excitation(d)==0,"DRIVE adds generic excitation instead of modulation");
        require(magnitude(out,1000)>.085,"Modulation suppresses the original tone");
        sidebands[i]=magnitude(out,400)+magnitude(out,1600);
        require(sidebands[i]>magnitude(out,3000)*3,"Modulation sidebands missing or saturation dominates");
    }
    require(sidebands[1]>sidebands[0]*2&&sidebands[2]>sidebands[1]*1.5,"DRIVE fails to increase real modulation depth");
    {CrushDSP d;d.prepare(sr);CrushDSP::Parameters p;p.processingVersion=13;p.centered=true;
        p.signedX=std::log(2.f)/std::log(20.f);p.activity=p.signedX;p.bits=16;p.rate=sr;p.drive=0;p.mutate=0;p.mix=1;p.output=0;
        std::vector<float> out;for(int n=0;n<sr*2;++n){float v=.75f*std::sin(6.28318530718f*1000*n/sr),f[]{v,0};d.processFrame(f,1,p);if(n>=sr)out.push_back(f[0]);}
        require(magnitude(out,3000)/magnitude(out,1000)<.002,"Ordinary hot input is still being conventionally saturated");}
    std::array<double,3> third{},fifth{};int shapeIndex=0;
    for(float y:{-.6f,0.f,.6f}){CrushDSP d;d.prepare(sr);CrushDSP::Parameters p;
        p.processingVersion=13;p.centered=true;p.signedX=std::log(2.f)/std::log(20.f);p.signedY=y;p.activity=std::hypot(p.signedX,y);p.bits=16;p.rate=sr;p.drive=36;p.mutate=0;p.mix=1;p.output=0;
        std::vector<float> out;for(int n=0;n<sr/4;++n){float f[]{.1f*std::sin(6.28318530718f*1000*n/sr),0};d.processFrame(f,1,p);if(n>=sr/8)out.push_back(f[0]);}
        require(std::abs(DigitalRoutingProbe::frequency(d)-600)<.01,"Y changes carrier frequency instead of shape");
        require(std::abs(DigitalRoutingProbe::shape(d)-y)<.001,"Y fails to morph the carrier shape");
        third[shapeIndex]=magnitude(out,800)+magnitude(out,2800);fifth[shapeIndex]=magnitude(out,4000);++shapeIndex;}
    require(third[0]>std::max(1.e-5,third[1]*5),"Rounded carrier shape adds no actual third-harmonic sidebands");
    require(fifth[2]>std::max(1.e-5,fifth[0]*5),"Stepped carrier shape adds no actual higher sidebands");
    auto fixed=dna;for(auto& frame:fixed.spectralFrames)frame=DnaTemporalFrame::from(fixed.spectrum);
    std::array<float,2> frequencies{};
    for(int i=0;i<2;++i){CrushDSP d;d.prepare(sr);CrushDSP::Parameters p;
        p.processingVersion=13;p.centered=true;p.signedX=i?.2f:-.2f;p.signedY=0;p.activity=.2f;p.bits=16;p.rate=sr;p.drive=0;p.mutate=1;p.mix=1;p.output=0;
        for(int n=0;n<sr/2;++n){float f[]{.1f*std::sin(n*.13f),0};d.processFrame(f,1,p,&fixed);}frequencies[i]=DigitalRoutingProbe::frequency(d);}
    require(std::abs(frequencies[1]/frequencies[0]-std::pow(300.f,.2f))<.01,"Full MUTATE disconnects X tuning");
    std::array<float,3> pitched{};std::array<double,3> positions{};
    for(int i=0;i<3;++i){CrushDSP d;d.prepare(sr);CrushDSP::Parameters p;
        p.processingVersion=13;p.centered=true;p.signedX=.2f;p.signedY=.1f;p.activity=std::hypot(p.signedX,p.signedY);p.bits=16;p.rate=sr;p.drive=0;p.mutate=1;p.mix=1;p.output=0;p.pitch=(i-1)*12.f;
        for(int n=0;n<sr/2;++n){float f[]{.1f*std::sin(n*.13f),0};d.processFrame(f,1,p,&fixed);}pitched[i]=DigitalRoutingProbe::frequency(d);positions[i]=d.getPatternPosition();}
    require(std::abs(pitched[1]/pitched[0]-2)<.01&&std::abs(pitched[2]/pitched[1]-2)<.01,"PITCH fails to transpose the sample-derived carrier");
    require(positions[0]==positions[1]&&positions[1]==positions[2],"Carrier PITCH changes DNA timing");
    {CrushDSP d;d.prepare(sr);CrushDSP::Parameters p;p.processingVersion=13;p.centered=true;p.activity=1;p.bits=16;p.rate=sr;p.drive=36;p.mutate=1;
        for(int i=0;i<sr;++i){p.signedX=std::sin(i*.01f);p.signedY=std::cos(i*.007f);float f[2]{};d.processFrame(f,2,p,&dna);require(f[0]==0&&f[1]==0,"Carrier becomes an independent audible tone in silence");}}
    std::cout<<"PASS actual 600 Hz carrier/1000 Hz excitation creates 400/1600 Hz sidebands, DRIVE depth, independent X/Y, full-MUTATE tuning and silence.\n";
}
static void presenceChecks(const std::array<std::vector<float>,4>& inputs,const DnaProfile& dna){
    // Lower DRIVE retains every approved sweet spot, with and without DNA.
    for(int k=0;k<4;++k)for(float y:{-.4f,.4f})for(float drive:{0.f,9.f,18.f})
        for(const DnaProfile* profile:{static_cast<const DnaProfile*>(nullptr),&dna})
            require(render(inputs[k],.4f,y,profile,1,1,13,drive)==render(inputs[k],.4f,y,profile,1,1,14,drive),"Lower DRIVE range changed from approved v062");
    std::array<std::vector<float>,2> tones;
    std::array<float,2> frequency{};
    for(int i=0;i<2;++i){CrushDSP d;d.prepare(sr);CrushDSP::Parameters p;
        p.processingVersion=13+i;p.centered=true;p.signedX=std::log(2.f)/std::log(20.f);p.signedY=.6f;
        p.activity=std::hypot(p.signedX,p.signedY);p.bits=16;p.rate=sr;p.drive=36;p.mutate=0;p.mix=1;p.output=0;
        for(int n=0;n<sr*2;++n){float f[]{.1f*std::sin(6.28318530718f*1000*n/sr),0};d.processFrame(f,1,p);if(n>=sr)tones[i].push_back(f[0]);}
        frequency[i]=DigitalRoutingProbe::frequency(d);
        require(DigitalRoutingProbe::excitation(d)==0,"Presence update adds generic saturation");}
    require(frequency[0]==frequency[1],"Presence boost retunes the approved carrier");
    const double fundamental=magnitude(tones[1],1000)/magnitude(tones[0],1000);
    require(fundamental>.98&&fundamental<1.02,"Presence is just a dry/output gain increase");
    const auto a=match(tones[0]),b=match(tones[1]);
    const double sidebands=(magnitude(b,400)+magnitude(b,1600))/(magnitude(a,400)+magnitude(a,1600));
    const double stepped=magnitude(b,4000)/magnitude(a,4000);
    require(sidebands>1.25&&sidebands<1.45,"Upper DRIVE modulation increase is outside the restrained target");
    require(stepped>1.5&&stepped<2,"Stepped texture increase is missing or excessive");
    require(render(inputs[2],0,0,&dna,1,1,14,36)==inputs[2],"Presence changes dry centre");
    for(int k=0;k<4;++k)for(auto xy:{std::array<float,2>{-.7f,-.7f},{.7f,-.7f},{-.7f,.7f},{.7f,.7f},{0,1},{1,0}}){
        const auto out=render(inputs[k],xy[0],xy[1],nullptr,0,1,14,36);
        require(magnitude(out,55)>.94*magnitude(inputs[k],55),"Extra core texture loses bass fundamentals");
        if(k==1||k==2)require(correlation(sourceMotion(inputs[k]).ratio,sourceMotion(out).ratio)>.75,"Extra texture obscures source filter/formant movement");
        if(k==3)require(correlation(motion(inputs[k]).energy,motion(out).energy)>.98,"Extra texture obscures attacks");}
    for(auto xy:{std::array<float,2>{-.7f,-.7f},{.7f,-.7f},{-.7f,.7f},{.7f,.7f},{0,1},{1,0}})
        for(const DnaProfile* profile:{static_cast<const DnaProfile*>(nullptr),&dna}){
            auto input=inputs[3];for(auto& v:input)v*=3;
            const auto out=render(input,xy[0],xy[1],profile,1,1,14,36);
            for(float v:out)require(std::isfinite(v)&&std::abs(v)<=1,"New upper DRIVE causes output clipping/instability");
            // A full DNA profile deliberately changes spectral balance. At
            // overload, compare against that same approved transformation.
            const auto baseline=render(input,xy[0],xy[1],profile,1,1,13,36);
            require(magnitude(out,55)>.85*magnitude(baseline,55),"Extra texture loses bass against approved DNA/overload response");}
    std::cout<<"Upper DRIVE level-matched sideband lift "<<20*std::log10(sidebands)<<" dB, stepped-partial lift "<<20*std::log10(stepped)<<" dB; fundamental ratio "<<fundamental<<".\n";
}
static void legacyRecall(const DnaProfile& dna){
    for(int version=1;version<=13;++version){
        CrushDSP now;Legacy062CrushDSP old;now.prepare(sr);old.prepare(sr);
        CrushDSP::Parameters p;Legacy062CrushDSP::Parameters q;
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
    enhancerChecks(inputs,dna,contrast);
    modulationChecks(dna);
    presenceChecks(inputs,dna);
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
                std::cout<<"Retained v12 quadrant "<<q<<" filter movement correlation "<<old<<" -> "<<now<<'\n';}
            if(kind==2)require(correlation(in.ratio,b.ratio)>.5,"Vowel movement is disconnected from source");
            if(kind==3)require(correlation(in.energy,b.energy)>.97,"Rhythmic articulation lost");
            require(magnitude(after,55)>.75*magnitude(input,55),"Bass foundation lost");
            require(difference(match(after),match(input))>.08,"Correction only added dry signal");
            const auto enhanced=argc>1&&q==1?render(input,x,y,nullptr,0,1,13):std::vector<float>{};
            if(argc>1&&q==1){for(const auto* sound:{&input,&after,&enhanced}){auto a=match(*sound);const int fade=240;
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
        CrushDSP d;d.prepare(fs);CrushDSP::Parameters p;p.processingVersion=14;p.centered=p.rhythmic=true;p.mix=1;p.output=0;
        for(int i=0;i<int(fs/2);++i){if(i%block==0){p.signedX=.95f*std::sin(i*.001f);p.signedY=.95f*std::cos(i*.0017f);const auto m=XYMapping::map(p.signedX,p.signedY,fs,14);p.activity=m.activity;p.bits=m.bits;p.rate=m.rate;p.drive=float(i%37);p.mutate=float(i%101)/100;}
            float f[]{i<fs/4?.7f*std::sin(float(i)*.031f):0,0};d.processFrame(f,2,p,i/2000%2?&dna:nullptr);
            require(std::isfinite(f[0])&&std::abs(f[0])<1.8f&&f[1]==0,"Automation/silence/crosstalk instability");}
    }
    if(argc>1)wav(argv[1],audition);
    std::cout<<"PASS input filter/formant movement, transient articulation, fundamentals, independent DNA, dry center, multirate automation and exact versions 1-13. Metrics do not establish subjective quality.\n";
}
