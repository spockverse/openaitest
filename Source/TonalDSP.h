#pragma once
#include "DnaSpectrum.h"
#include <algorithm>
#include <array>
#include <cmath>

// Resonant coloration of incoming/generated harmonics. No oscillator, carrier,
// audio clock or sample playback exists in this stage.
class TonalDSP
{
public:
    void prepare(double sr)noexcept{sampleRate=sr;coefficient=1-std::exp(-1.0f/(.006f*static_cast<float>(sr)));reset();}
    void reset()noexcept{states={};primed=false;}
    void process(float* frame,int channels,float vowel,float resonance,float amount,
                 const DnaSpectrum* profile=nullptr,float mutate=0)noexcept
    {
        vowel=std::clamp(vowel,0.0f,1.0f);
        std::array<float,3> target {320+650*vowel,900+1400*vowel,2600+1500*vowel};
        const bool dna=profile&&profile->valid&&mutate>0;
        for(std::size_t b=0;b<3;++b)
        {
            if(dna)target[b]=(1-mutate)*target[b]+mutate*profile->peaks[b]*(.85f+.3f*vowel);
            target[b]=std::clamp(target[b],150.0f,static_cast<float>(sampleRate*.4));
        }
        if(!primed){frequencies=target;primed=true;}
        std::array<float,3> g {};for(std::size_t b=0;b<3;++b){frequencies[b]+=coefficient*(target[b]-frequencies[b]);g[b]=std::tan(3.14159265359f*frequencies[b]/static_cast<float>(sampleRate));}
        std::array<float,3> weights {1,1,.65f};
        if(dna)for(std::size_t b=0;b<3;++b)
        {
            weights[b]=(1-mutate)*weights[b]+mutate*(.15f+1.85f*profile->peakLevels[b]);
        }
        const float weightSum=weights[0]+weights[1]+weights[2];
        const float k=1/std::clamp(resonance,1.5f,6.0f);
        for(int ch=0;ch<std::min(channels,2);++ch)
        {
            const float x=frame[ch];float bands=0;
            for(std::size_t b=0;b<3;++b)
            {
                auto& s=states[static_cast<std::size_t>(ch)][b];const float a=1/(1+g[b]*(g[b]+k));
                const float v1=a*(s[0]+g[b]*(x-s[1])),v2=s[1]+g[b]*v1;
                s[0]=2*v1-s[0];s[1]=2*v2-s[1];
                bands+=k*v1*weights[b];
            }
            frame[ch]=x*(1-.65f*amount)+bands*(3.2f*amount/weightSum);
        }
    }
private:
    double sampleRate=48000;float coefficient=.01f;bool primed=false;
    std::array<float,3> frequencies {};
    std::array<std::array<std::array<float,2>,3>,2> states {};
};
