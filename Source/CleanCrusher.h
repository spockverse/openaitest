#pragma once
#include <algorithm>
#include <array>
#include <cmath>

// Variable-rate quantizer with anti-alias filtering and analogue-style DAC
// reconstruction. The clock is controlled only by XY, never by the donor sample.
class CleanCrusher
{
    struct Lowpass
    {
        float s1=0,s2=0;
        float tick(float x,float g,float k)noexcept
        {
            const float a=1/(1+g*(g+k)),v1=a*(s1+g*(x-s2)),v2=s2+g*v1;
            s1=2*v1-s1;s2=2*v2-s2;return v2;
        }
    };
public:
    void prepare(double sr)noexcept{sampleRate=sr;reset();}
    void reset()noexcept{filters={};previous={};held={};phase=1;primed=false;}
    void process(float* frame,int channels,float bits,float rate)noexcept
    {
        rate=std::clamp(rate,500.0f,static_cast<float>(sampleRate));
        const double increment=rate/sampleRate;
        const float reconstruction=std::clamp(static_cast<float>((sampleRate-rate)/(sampleRate*.1)),0.0f,1.0f);
        const float g=std::tan(3.14159265359f*.43f*rate/static_cast<float>(sampleRate));
        const float steps=std::exp2(std::clamp(bits,2.0f,16.0f)-1);
        const bool capture=phase>=1;
        const float fraction=primed?std::clamp(static_cast<float>(1-(phase-1)/increment),0.0f,1.0f):1.0f;
        if(capture)phase-=std::floor(phase);
        for(int ch=0;ch<std::min(channels,2);++ch)
        {
            const auto c=static_cast<std::size_t>(ch);auto& f=filters[c];
            const float bandlimited=f[1].tick(f[0].tick(frame[ch],g,1/.5411961f),g,1/1.306563f);
            const float source=frame[ch]+reconstruction*(bandlimited-frame[ch]);
            if(capture)
            {
                const float atClock=previous[c]+fraction*(source-previous[c]);
                held[c]=std::round(atClock*steps)/steps;
            }
            previous[c]=source;
            const float rebuilt=f[3].tick(f[2].tick(held[c],g,1/.5411961f),g,1/1.306563f);
            frame[ch]=held[c]+reconstruction*(rebuilt-held[c]);
        }
        phase+=increment;primed=true;
    }
private:
    double sampleRate=48000,phase=1;bool primed=false;
    std::array<float,2> previous{},held{};
    std::array<std::array<Lowpass,4>,2> filters{};
};
