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
void wav(const std::filesystem::path& path,const std::vector<float>& x)
{
    std::ofstream f(path,std::ios::binary); auto word=[&](unsigned v,int n){for(int i=0;i<n;++i)f.put(static_cast<char>((v>>(8*i))&255));};
    f.write("RIFF",4);word(36+static_cast<unsigned>(x.size()*2),4);f.write("WAVEfmt ",8);word(16,4);word(1,2);word(1,2);word(sr,4);word(sr*2,4);word(2,2);word(16,2);f.write("data",4);word(static_cast<unsigned>(x.size()*2),4);
    for(auto v:x)word(static_cast<unsigned>(static_cast<int>(std::clamp(v,-1.0f,1.0f)*32767)),2);
}
std::vector<float> render(const std::vector<float>& input,float drive,float mutate,const DnaProfile* dna)
{
    CrushDSP dsp; dsp.prepare(sr); CrushDSP::Parameters p;p.drive=drive;p.bits=9;p.rate=16000;p.mix=1;p.mutate=mutate;p.output=-6;
    std::vector<float> out;out.reserve(input.size());
    for(auto v:input){float f[]{v,v};dsp.processFrame(f,2,p,dna);require(std::isfinite(f[0])&&std::abs(f[0])<1.2f,"uncontrolled output peak");out.push_back(f[0]);}return out;
}
int main(int argc,char**argv)
{
    // A pure multiplier must create sum/difference tones, without leaking a carrier.
    TonalDSP tonal;tonal.prepare(sr);std::vector<float> ring(sr);
    for(int i=0;i<sr;++i){float f[]{.5f*static_cast<float>(std::sin(tau*100*i/sr))};tonal.process(f,1,311,1,.5f,4,0);ring[i]=f[0];}
    require(magnitude(ring,211)>.24&&magnitude(ring,411)>.24,"missing ring sidebands");
    require(magnitude(ring,311)<.0001,"carrier leaked into audio");
    // Impulse responses identify moving formant peaks independently of oscillator sidebands.
    for(float vowel:{0.0f,.5f,1.0f})
    {
        tonal.reset();std::vector<float> response(sr/2);
        for(std::size_t i=0;i<response.size();++i){float f[]{i==0?1.0f:0.0f};tonal.process(f,1,311,0,vowel,7,1);response[i]=f[0];}
        const double f1=vowel<.5f?330+1100*vowel:880-1000*(vowel-.5f);
        const double f2=vowel<.5f?850+900*vowel:1300+2200*(vowel-.5f);
        require(magnitude(response,f1)>magnitude(response,100)*2,"first formant peak missing");
        require(magnitude(response,f2)>magnitude(response,3000)*2,"second formant peak missing");
        std::cout<<"vowel "<<vowel<<" peaks "<<f1<<","<<f2<<" Hz\n";
    }
    // Keep one running filter instance while changing vowel targets. A fixed
    // multisine excites every candidate peak; the dominant bands must migrate.
    tonal.reset();
    for (int segment = 0; segment < 3; ++segment)
    {
        std::vector<float> tail;
        for (int i = 0; i < sr; ++i)
        {
            float x = 0;
            for (double hz : {330.,380.,850.,880.,1300.,2400.}) x += .08f * static_cast<float>(std::sin(tau * hz * i / sr));
            float f[]{x}; tonal.process(f,1,311,0,segment*.5f,7,1);
            if (i >= sr/2) tail.push_back(f[0]);
        }
        if(segment==0) require(magnitude(tail,330)>magnitude(tail,1300)*2,"moving vowel did not reach oo");
        if(segment==1) require(magnitude(tail,1300)>magnitude(tail,330)*2,"moving vowel did not reach ah");
        if(segment==2) require(magnitude(tail,2400)>magnitude(tail,850)*2,"moving vowel did not reach ee");
    }
    std::vector<float> input(count),soft(count),metal(count);
    for(int i=0;i<count;++i)
    {
        const double t=static_cast<double>(i)/sr;
        input[i]=.28f*static_cast<float>(std::sin(tau*55*t))+.12f*static_cast<float>(2*std::fmod(t*55,1.0)-1);
        soft[i]=.6f*static_cast<float>(std::sin(tau*110*t));
        metal[i]=static_cast<float>(std::exp(-14*std::fmod(t,.25))*(.4*std::sin(tau*1711*t)+.25*std::sin(tau*2933*t)));
    }
    const float* a[]{soft.data()};const float* b[]{metal.data()};
    auto smooth=DnaProfile::analyse(a,1,soft.size(),sr),clang=DnaProfile::analyse(b,1,metal.size(),sr);
    require(clang.characterAt(.05)>smooth.characterAt(.05)+.3f,"DNA analysis did not distinguish brightness");
    auto flatSmooth=smooth,flatMetal=clang;
    flatSmooth.envelope.fill(1);flatMetal.envelope.fill(1);
    auto timbreA=render(input,20,1,&flatSmooth),timbreB=render(input,20,1,&flatMetal);
    require(difference(timbreA,timbreB)/std::max(rms(timbreA),rms(timbreB))>.35,"DNA timbre must differ even with identical envelopes");
    auto start=std::chrono::steady_clock::now();
    auto noDna=render(input,20,0,nullptr),zero=render(input,20,0,&clang),smoothOut=render(input,20,1,&smooth),metalOut=render(input,20,1,&clang);
    require(noDna==zero,"MUTATE zero retains DNA influence");
    const double dnaDifference=difference(smoothOut,metalOut)/std::max(rms(smoothOut),rms(metalOut));
    require(dnaDifference>.35,"DNA difference too subtle");
    auto silence=render(std::vector<float>(sr),36,1,&clang);require(rms(silence)==0,"DNA or carrier creates audio from silence");
    for(float drive:{0.0f,6.0f,18.0f,36.0f}) {auto result=render(input,drive,0,nullptr); std::cout<<"drive "<<drive<<" RMS "<<rms(result)<<" transformed energy "<<difference(result,input)<<'\n';}
    auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::cout<<"DNA normalized difference "<<dnaDifference<<"; render/test wall seconds "<<elapsed<<'\n';
    if(argc>1)
    {
        std::filesystem::path dir(argv[1]);std::filesystem::create_directories(dir);
        wav(dir/"01-dry-bass.wav",input);wav(dir/"02-new-engine-no-DNA.wav",noDna);wav(dir/"03-smooth-DNA-100.wav",smoothOut);wav(dir/"04-metal-DNA-100.wav",metalOut);
        wav(dir/"DNA-smooth.wav",soft);wav(dir/"DNA-metallic.wav",metal);
        // Listening copies share one target RMS; raw renders above retain actual levels.
        for(auto pair:{std::make_pair("listen-dry.wav",input),std::make_pair("listen-no-DNA.wav",noDna),std::make_pair("listen-smooth-DNA.wav",smoothOut),std::make_pair("listen-metal-DNA.wav",metalOut)})
        {const float gain=static_cast<float>(.16/std::max(1e-9,rms(pair.second)));for(auto& v:pair.second)v*=gain;wav(dir/pair.first,pair.second);}
    }
}
