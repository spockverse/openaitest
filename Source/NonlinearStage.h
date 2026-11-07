#pragma once
#include <array>
#include <algorithm>
#include <cmath>

// 4x minimum-phase interpolation / anti-imaging and anti-alias filtering.
// Only nonlinear processing is oversampled; clean bit/rate reduction precedes it.
class NonlinearStage
{
    struct Lowpass
    {
        float z1=0,z2=0,b0=0,b1=0,b2=0,a1=0,a2=0;
        void prepare(float q){const float w=6.28318530718f*.11f,c=std::cos(w),alpha=std::sin(w)/(2*q),norm=1/(1+alpha);b0=(1-c)*.5f*norm;b1=2*b0;b2=b0;a1=-2*c*norm;a2=(1-alpha)*norm;}
        float tick(float x)noexcept{const float y=b0*x+z1;z1=b1*x-a1*y+z2;z2=b2*x-a2*y;return y;}
        void reset()noexcept{z1=z2=0;}
    };
public:
    void prepare(){for(auto& bank:filters){bank[0].prepare(.5411961f);bank[1].prepare(1.306563f);bank[2].prepare(.5411961f);bank[3].prepare(1.306563f);}reset();}
    void reset()noexcept{previous={};for(auto& bank:filters)for(auto& f:bank)f.reset();}
    static float curve(float x,float asymmetry,float edge)noexcept
    {
        const float square=x*x;
        const float bent=x+asymmetry*square/(1+square);
        const float smooth=bent/std::sqrt(1+bent*bent);
        // Bounded input-derived harmonic folding; always exactly zero at x=0.
        return smooth+edge*.16f*std::sin(4*std::atan(x));
    }
    float tick(float x,int channel,float gain,float asymmetry,float edge)noexcept
    {
        auto& bank=filters[static_cast<std::size_t>(channel)];float y=0;
        for(int k=1;k<=4;++k)
        {
            float u=previous[static_cast<std::size_t>(channel)]+(x-previous[static_cast<std::size_t>(channel)])*(k*.25f);
            u=bank[1].tick(bank[0].tick(u));
            y=curve(u*gain,asymmetry,edge);
            y=bank[3].tick(bank[2].tick(y));
        }
        previous[static_cast<std::size_t>(channel)]=x;return y;
    }
    // Oversample only the nonlinear residual. The original excitation is not
    // low-passed or cross-mixed against its delayed, filtered copy.
    float tickResidual(float x, int channel, float gain, float asymmetry, float edge, float amount,
                       float cycles = 4) noexcept
    {
        auto &bank = filters[std::size_t(channel)];
        float residual = 0;
        gain = std::max(1.f, gain);
        const float inverseGain=1/gain;
        for (int k = 1; k <= 4; ++k)
        {
            float u = previous[std::size_t(channel)] + (x - previous[std::size_t(channel)]) * (k * .25f);
            u = bank[1].tick(bank[0].tick(u));
            const float input = u * gain, square = input * input,
                        bent = input + asymmetry * square / (1 + square);
            const float phaseInput=cycles*input;
            // Cancel the folder's small-signal slope with a bounded term, not
            // a delayed broadband gain copy. Keep only nonlinear harmonic change.
            const float fold=std::sin(cycles*std::atan(input))-phaseInput/(1+phaseInput*phaseInput);
            const float shape=bent/std::sqrt(1+bent*bent)+edge*.16f*fold;
            float difference=shape*inverseGain-u;
            residual = bank[3].tick(bank[2].tick(difference));
        }
        previous[std::size_t(channel)] = x;
        return x + amount * residual;
    }
private:
    std::array<float,2> previous {};
    std::array<std::array<Lowpass,4>,2> filters {};
};
