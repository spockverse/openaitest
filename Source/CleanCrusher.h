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
    void reset()noexcept{filters={};previous={};held={};phase=1;primed=false;modernPrimed=false;}
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
    // All expensive transforms run only at the bounded engine control rate.
    void setModern(float bits, float rate, float antiAlias, float rebuild, int frames) noexcept
    {
        rate = std::clamp(rate, 500.f, float(sampleRate));
        const std::array<float, 4> key{bits, rate, antiAlias, rebuild};
        if (modernPrimed && key == modernKey)
        {
            modern = modernGoal;
            modernStep.fill(0);
            return;
        }
        modernKey = key;
        const std::array<float, 5> goal{std::exp2(std::clamp(bits, 2.f, 16.f) - 1), rate / float(sampleRate),
                                        std::tan(3.14159265359f * .43f * rate / float(sampleRate)),
                                        std::clamp(antiAlias, 0.f, 1.f), std::clamp(rebuild, 0.f, 1.f)};
        modernGoal = goal;
        if (!modernPrimed)
        {
            modern = goal;
            modernPrimed = true;
        }
        for (std::size_t i = 0; i < 5; ++i)
            modernStep[i] = (goal[i] - modern[i]) / float(std::max(1, frames));
    }
    void processModern(float *frame, int channels) noexcept
    {
        for (std::size_t i = 0; i < 5; ++i)
            modern[i] += modernStep[i];
        const float steps = std::max(2.f, modern[0]),
                    increment = std::clamp(modern[1], float(500 / sampleRate), 1.f),
                    g = std::max(.001f, modern[2]);
        const bool capture = phase >= 1;
        const float fraction = primed ? std::clamp(float(1 - (phase - 1) / increment), 0.f, 1.f) : 1.f;
        if (capture)
            phase -= std::floor(phase);
        for (int ch = 0; ch < std::min(channels, 2); ++ch)
        {
            const auto c = std::size_t(ch);
            auto &f = filters[c];
            const float x = frame[ch], limited = f[1].tick(f[0].tick(x, g, 1 / .5411961f), g, 1 / 1.306563f),
                        source = x + modern[3] * (limited - x);
            if (capture)
                held[c] = std::round((previous[c] + fraction * (source - previous[c])) * steps) / steps;
            previous[c] = source;
            const float rebuilt = f[3].tick(f[2].tick(held[c], g, 1 / .5411961f), g, 1 / 1.306563f);
            frame[ch] = held[c] + modern[4] * (rebuilt - held[c]);
        }
        phase += increment;
        primed = true;
    }
private:
    std::array<float,5> modern{},modernStep{},modernGoal{};std::array<float,4> modernKey{};bool modernPrimed=false;
    double sampleRate=48000,phase=1;bool primed=false;
    std::array<float,2> previous{},held{};
    std::array<std::array<Lowpass,4>,2> filters{};
};
