#pragma once
#include <algorithm>
#include <array>
#include <cmath>

struct DnaTiming {
    static constexpr int count=11,defaultIndex=2;
    static constexpr std::array<const char*,count> labels{{"1/1","1/2","1/4","1/8D","1/8","1/16D","1/8T","1/16","1/16T","1/32","1/64"}};
    static constexpr std::array<double,count> beats{{4,2,1,.75,.5,.375,1./3,.25,1./6,.125,.0625}};
    // Keep the existing 0..4 normalized automation domain. Legacy state still
    // interprets integer values as its original five rate multipliers.
    static float value(int index) noexcept {return std::clamp(index,0,count-1)*4.f/(count-1);}
    static int index(float value) noexcept {return std::clamp(int(std::lround(value*(count-1)/4.f)),0,count-1);}
    static double tempo(double bpm) noexcept {return std::isfinite(bpm)&&bpm>=1&&bpm<=999?bpm:120.;}
    static float legacySpeed(float value) noexcept {constexpr float speeds[]{.25f,.5f,1,2,4};return speeds[std::clamp(int(std::lround(value)),0,4)];}
    static int nearest(double durationBeats) noexcept {int best=0;double distance=1.e30;for(int i=0;i<count;++i){const double d=std::abs(std::log(std::max(1.e-12,durationBeats)/beats[i]));if(d<distance){distance=d;best=i;}}return best;}
};

class DnaRhythmClock {
public:
    void prepare(double sr) noexcept {sampleRate=sr;reset();}
    void reset() noexcept {phase=0;armed=false;increment=0;lastBpm=lastBeats=0;}
    void configure(double bpm,double beats) noexcept {
        bpm=DnaTiming::tempo(bpm);beats=std::max(.001,beats);
        if(bpm!=lastBpm||beats!=lastBeats){increment=bpm/(60*sampleRate*beats);lastBpm=bpm;lastBeats=beats;}
    }
    void seek(double ppq,double beats,int mode) noexcept {
        if(mode==0){phase=std::isfinite(ppq)?ppq/std::max(.001,beats):0;phase-=std::floor(phase);}
        else {phase=0;armed=false;}
    }
    void trigger() noexcept {phase=0;armed=true;}
    void advance(bool playing,int mode) noexcept {
        if(!playing||(mode==1&&!armed))return;
        phase+=increment;
        if(mode==0){phase-=std::floor(phase+1.e-12);if(phase<0)phase=0;}else phase=std::min(1.,phase);
    }
    double position() const noexcept {return phase;}
    bool isArmed()const noexcept{return armed;}
private:
    double sampleRate=48000,phase=0,increment=0,lastBpm=0,lastBeats=0;bool armed=false;
};

// Energy envelopes suppress waveform-cycle retriggers. All storage is fixed;
// the rising envelope is measured against its recent body and quiet noise floor.
class DnaOnset {
public:
    void prepare(double sr) noexcept {sampleRate=sr;fastK=1-std::exp(-1/(sr*.008));slowK=1-std::exp(-1/(sr*.06));noiseK=1-std::exp(-1/(sr*.5));reset();}
    void reset()noexcept {fast=slow=noise=0;refractory=0;armed=true;}
    void midi()noexcept {refractory=int(sampleRate*.06);armed=false;}
    bool observe(float energy)noexcept {
        if(refractory>0)--refractory;
        const double old=fast;fast+=fastK*(energy-fast);slow+=slowK*(energy-slow);
        if(fast<slow*.8||fast<1.e-6)armed=true;
        if(fast<slow*1.1){noise+=noiseK*(fast-noise);noise=std::min(noise,1.e-5);}
        const bool hit=armed&&refractory==0&&fast>std::max(9.e-6,noise*6)&&fast>slow*1.7&&fast>old;
        if(hit){armed=false;refractory=int(sampleRate*.025);}return hit;
    }
private:
    double sampleRate=48000,fastK=0,slowK=0,noiseK=0,fast=0,slow=0,noise=0;int refractory=0;bool armed=true;
};
