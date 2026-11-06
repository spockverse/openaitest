#include "CrushDSP.h"
#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <cstdlib>
constexpr double tau = 6.283185307179586;
constexpr int sr = 48000, count = sr * 4;
void require(bool ok, const char* message) { if (!ok) { std::cerr << message << '\n'; std::exit(1); } }
double rms(const std::vector<float>& x) { double e=0; for(auto v:x)e+=v*v; return std::sqrt(e/x.size()); }
double difference(const std::vector<float>& a,const std::vector<float>& b) { double e=0;for(std::size_t i=0;i<a.size();++i)e+=(a[i]-b[i])*(a[i]-b[i]);return std::sqrt(e/a.size()); }
double magnitude(const std::vector<float>& x,double hz) { double a=0,b=0;for(std::size_t i=0;i<x.size();++i){a+=x[i]*std::cos(tau*hz*i/sr);b+=x[i]*std::sin(tau*hz*i/sr);}return 2*std::hypot(a,b)/x.size(); }
double harmonicBand(const std::vector<float>& x,int low,int high,int step)
{
    double energy=0;for(int h=low;h<=high;h+=step){const auto v=magnitude(x,55*h);energy+=v*v;}return energy;
}
void wav(const std::filesystem::path& path,const std::vector<float>& x)
{
    std::ofstream f(path,std::ios::binary); auto word=[&](unsigned v,int n){for(int i=0;i<n;++i)f.put(static_cast<char>((v>>(8*i))&255));};
    f.write("RIFF",4);word(36+static_cast<unsigned>(x.size()*2),4);f.write("WAVEfmt ",8);word(16,4);word(1,2);word(1,2);word(sr,4);word(sr*2,4);word(2,2);word(16,2);f.write("data",4);word(static_cast<unsigned>(x.size()*2),4);
    for(auto v:x) { require(std::isfinite(v)&&std::abs(v)<=1,"WAV writer would clip render");word(static_cast<unsigned>(static_cast<int>(v*32767)),2); }
}
std::vector<float> render(const std::vector<float>& input,float drive,float mutate,const DnaProfile* dna,float bits=9,float rate=16000)
{
    CrushDSP dsp; dsp.prepare(sr); CrushDSP::Parameters p;p.drive=drive;p.bits=bits;p.rate=rate;p.mix=1;p.mutate=mutate;p.output=-6;
    std::vector<float> out;out.reserve(input.size());
    for(auto v:input){float f[]{v,v};dsp.processFrame(f,2,p,dna);require(std::isfinite(f[0])&&std::abs(f[0])<1.2f,"uncontrolled output peak");out.push_back(f[0]);}return out;
}
std::vector<float> matched(std::vector<float> x,double target=.12)
{
    const float gain=static_cast<float>(target/std::max(1e-20,rms(x)));for(auto& v:x)v*=gain;
    return x;
}
int main(int argc,char**argv)
{
    std::vector<float> sine(count),saw(count),stab(count),low(count),metal(count),vocal(count),percussion(count),tonal(count);
    unsigned seed=1234567;float filtered=0;
    for(int i=0;i<count;++i)
    {
        const double t=static_cast<double>(i)/sr,phase=std::fmod(t*55,1.0);
        sine[i]=.3f*static_cast<float>(std::sin(tau*55*t));
        saw[i]=.24f*static_cast<float>(2*phase-1);
        const double inNote=std::fmod(t,.5);stab[i]=inNote<.18?saw[i]*static_cast<float>(std::exp(-17*inNote)):0;
        filtered+=.035f*(saw[i]-filtered);low[i]=filtered*2;
        const double beat=std::fmod(t,.5);
        metal[i]=static_cast<float>(std::exp(-12*beat)*(.5*std::sin(tau*1711*t)+.3*std::sin(tau*2933*t)));
        double voice=0;for(int h=1;h<30;++h){double hz=110*h;double w=.05/h+std::exp(-std::pow((hz-650)/200,2))*.16+std::exp(-std::pow((hz-1400)/280,2))*.12;voice+=w*std::sin(tau*hz*t);}
        vocal[i]=static_cast<float>(voice*(.75+.25*std::sin(tau*2*t)));
        seed=1664525*seed+1013904223;const float noise=static_cast<float>((seed>>8)/8388608.0-1);
        percussion[i]=noise*static_cast<float>(.7*std::exp(-25*beat));
        tonal[i]=.4f*static_cast<float>(std::sin(tau*220*t)+.25*std::sin(tau*440*t));
    }
    std::array<DnaProfile,4> profiles;
    const std::array<const std::vector<float>*,4> fixtures {&metal,&vocal,&percussion,&tonal};
    for(std::size_t j=0;j<profiles.size();++j)
    {
        const float* channels[]{fixtures[j]->data()};profiles[j]=DnaProfile::analyse(channels,1,count,sr);
        require(profiles[j].valid&&profiles[j].spectrum.valid,"spectral analysis failed");
        std::cout<<"profile "<<j<<" flat/harmonic/even/crest/brightness ";for(auto v:profiles[j].spectrum.features)std::cout<<v<<' ';
        std::cout<<" peaks ";for(auto v:profiles[j].spectrum.peaks)std::cout<<v<<' ';std::cout<<'\n';
    }
    std::vector<std::pair<std::string,std::vector<float>>> files;
    const std::array<std::pair<const char*,const std::vector<float>*>,4> sources {{{"sine",&sine},{"saw",&saw},{"stab",&stab},{"lowpass",&low}}};
    for(const auto& pair:sources)
    {
        auto out=render(*pair.second,20,0,nullptr);require(rms(out)>.01,"input disappeared");
        require(difference(out,*pair.second)>.01,"input not transformed");
        files.emplace_back(std::string(pair.first)+"-dry.wav",*pair.second);files.emplace_back(std::string(pair.first)+"-processed.wav",out);
    }
    const auto base=render(low,20,0,nullptr),baseMatched=matched(base);
    for(std::size_t j=0;j<profiles.size();++j)
    {
        require(render(low,20,0,&profiles[j])==base,"MUTATE 0 must be bit-identical without DNA");
        auto half=render(low,20,.5f,&profiles[j]),full=render(low,20,1,&profiles[j]);
        const double halfChange=difference(matched(half),baseMatched)/.12,fullChange=difference(matched(full),baseMatched)/.12;
        std::cout<<"DNA "<<j<<" level-matched delta50="<<halfChange<<" delta100="<<fullChange<<'\n';
        require(halfChange>.08&&fullChange>.18,"DNA influence too subtle at 50/100 percent");
        files.emplace_back("DNA-"+std::to_string(j)+"-50.wav",half);files.emplace_back("DNA-"+std::to_string(j)+"-100.wav",full);
        auto silence=render(std::vector<float>(sr),36,1,&profiles[j]);require(rms(silence)==0,"profile generates independent audio");
    }
    for(const auto& profile:profiles)
    {
        const auto withGaps=render(stab,20,1,&profile);
        for(int i=0;i<count;++i)if(std::fmod(static_cast<double>(i)/sr,.5)>.30)require(withGaps[static_cast<std::size_t>(i)]==0,"sample-driven tonal loop continues between stabs");
    }
    for(std::size_t a=0;a<3;++a)for(std::size_t b=a+1;b<3;++b)
    {
        auto x=matched(render(low,20,1,&profiles[a])),y=matched(render(low,20,1,&profiles[b]));
        const double delta=difference(x,y)/.12;std::cout<<"profile pair "<<a<<','<<b<<" matched difference "<<delta<<'\n';
        require(delta>.18,"different sample profiles not distinguishable");
    }
    const auto metalOutput=matched(render(low,20,1,&profiles[0])),vocalOutput=matched(render(low,20,1,&profiles[1])),noiseOutput=matched(render(low,20,1,&profiles[2]));
    const double metallicMidDb=10*std::log10((harmonicBand(metalOutput,20,72,2)+1e-20)/(harmonicBand(vocalOutput,20,72,2)+1e-20));
    const double noisyHighDb=10*std::log10((harmonicBand(noiseOutput,80,180,8)+1e-20)/(harmonicBand(metalOutput,80,180,8)+1e-20));
    std::cout<<"Spectral contrasts: metallic/vocal upper harmonics "<<metallicMidDb<<"dB, percussion/metal high harmonics "<<noisyHighDb<<"dB\n";
    require(metallicMidDb>2&&noisyHighDb>3,"DNA differences are not supported by spectral contrasts");
    // Identical envelope and brightness, different spectral profiles: isolate timbre.
    auto a=profiles[0],b=profiles[1];a.envelope.fill(.5f);b.envelope.fill(.5f);a.character.fill(.5f);b.character.fill(.5f);
    require(difference(matched(render(low,20,1,&a)),matched(render(low,20,1,&b)))>.02,"spectral identity is only envelope/brightness");
    auto stabOut=render(stab,20,0,nullptr);
    double sumA=0,sumB=0,sumAA=0,sumBB=0,sumAB=0;int blocks=0;
    for(int start=0;start+480<=count;start+=480)
    {
        double aEnergy=0,bEnergy=0;for(int i=0;i<480;++i){aEnergy+=stab[static_cast<std::size_t>(start+i)]*stab[static_cast<std::size_t>(start+i)];bEnergy+=stabOut[static_cast<std::size_t>(start+i)]*stabOut[static_cast<std::size_t>(start+i)];}
        const double aEnv=std::sqrt(aEnergy/480),bEnv=std::sqrt(bEnergy/480);sumA+=aEnv;sumB+=bEnv;sumAA+=aEnv*aEnv;sumBB+=bEnv*bEnv;sumAB+=aEnv*bEnv;++blocks;
    }
    const double correlation=(sumAB-sumA*sumB/blocks)/std::sqrt((sumAA-sumA*sumA/blocks)*(sumBB-sumB*sumB/blocks));
    std::cout<<"Stab envelope correlation "<<correlation<<'\n';require(correlation>.95,"processing lost input rhythm/articulation");
    // Absolute transport time must not impose a yoi loop with no DNA.
    CrushDSP normal,shifted;normal.prepare(sr);shifted.prepare(sr);CrushDSP::Parameters p;p.drive=20;p.mix=1;p.bits=16;p.rate=sr;
    std::vector<float> clean;
    for(int i=0;i<count;++i){float x[]{sine[i]},y[]{sine[i]};shifted.setTimeline(100+i*.001);normal.processFrame(x,1,p);shifted.processFrame(y,1,p);require(x[0]==y[0],"clock-driven modulation remains");clean.push_back(x[0]);}
    require(magnitude(clean,55)>magnitude(clean,60)*10,"input pitch lost");
    require(magnitude(clean,165)>.001,"no input-derived harmonic excitation");
    for(int i=0;i<sr;++i){float f[]{0};normal.processFrame(f,1,p);if(i>sr/10)require(f[0]==0,"tonal loop/tail continues in gaps");}
    // Oversampling must reduce folded products, not merely lower overall gain.
    NonlinearStage os;os.prepare();std::vector<float> single(sr),over(sr);
    for(int i=0;i<sr;++i){const float x=.7f*static_cast<float>(std::sin(tau*9000*i/sr));single[i]=NonlinearStage::curve(x*4,.3f,.4f);over[i]=os.tick(x,0,4,.3f,.4f);}
    const double foldedSingle=(magnitude(single,21000)+magnitude(single,3000))/magnitude(single,9000);
    const double foldedOver=(magnitude(over,21000)+magnitude(over,3000))/magnitude(over,9000);
    std::cout<<"folded ratio 1x="<<foldedSingle<<" 4x="<<foldedOver<<'\n';require(foldedOver<foldedSingle*.6,"oversampling insufficient alias rejection");
    if(argc>1)
    {
        const std::filesystem::path dir(argv[1]);std::filesystem::create_directories(dir);
        double commonTarget=.12;
        for(const auto& pair:files){float peak=0;for(auto v:pair.second)peak=std::max(peak,std::abs(v));commonTarget=std::min(commonTarget,.85*rms(pair.second)/std::max(1e-20f,peak));}
        std::cout<<"Audition common RMS "<<commonTarget<<" ("<<20*std::log10(commonTarget)<<" dBFS)\n";
        for(const auto& pair:files){wav(dir/pair.first,pair.second);wav(dir/("listen-"+pair.first),matched(pair.second,commonTarget));}
        wav(dir/"DNA-source-metallic-synthetic.wav",metal);wav(dir/"DNA-source-vocal-like-synthetic.wav",vocal);wav(dir/"DNA-source-percussion-synthetic.wav",percussion);wav(dir/"DNA-source-tonal-synthetic.wav",tonal);
        wav(dir/"01-no-DNA-matched.wav",matched(base,commonTarget));
        for(std::size_t j=0;j<3;++j)wav(dir/(std::to_string(j+2)+"-"+std::array<std::string,3>{"metallic","vocal-like","percussive"}[j]+"-DNA100-matched.wav"),matched(render(low,20,1,&profiles[j]),commonTarget));
    }
    std::cout<<"PASS: input-derived harmonics, spectral DNA, silence/gaps, timeline invariance, pitch identity, alias rejection\n";
}
