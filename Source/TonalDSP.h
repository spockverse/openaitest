#pragma once
#include "DnaSpectrum.h"
#include "DnaTemporal.h"
#include <algorithm>
#include <array>
#include <cmath>

// Resonant coloration of incoming/generated harmonics. No oscillator, carrier,
// audio clock or sample playback exists in this stage.
class TonalDSP
{
public:
    void prepare(double sr)noexcept{sampleRate=sr;coefficient=1-std::exp(-1.0f/(.006f*static_cast<float>(sr)));reset();}
    void reset()noexcept{states={};primed=false;modernPrimed=false;warmState={};}
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
    void setModern(float vowel, float amount, float mutation, const DnaTemporalFrame &shape, float warmth,
                   int frames) noexcept
    {
        std::array<float, 26> key{};
        key[0] = vowel;
        key[1] = amount;
        key[2] = mutation;
        key[3] = warmth;
        std::size_t cursor = 4;
        for (float v : shape.bands)
            key[cursor++] = v;
        for (float v : shape.logPeaks)
            key[cursor++] = v;
        for (float v : shape.resonance)
            key[cursor++] = v;
        for (float v : shape.persistence)
            key[cursor++] = v;
        key[cursor++] = shape.features[0];
        if (modernPrimed && key == modernKey)
        {
            modern = modernGoal;
            modernStep.fill(0);
            return;
        }
        modernKey = key;
        const float contrast = mutation * mutation * (.35f + .65f * mutation);
        const std::array<float, 3> base{320 + 650 * vowel, 900 + 1400 * vowel, 2600 + 1500 * vowel};
        std::array<float, 3> weights{};
        float sum = 0;
        for (std::size_t b = 0; b < 3; ++b)
        {
            if (contrast <= 0)
            {
                weights[b] = 1;
                sum += 1;
                continue;
            }
            float reference = std::exp(shape.logPeaks[b]);
            std::size_t nearest = 0;
            for (std::size_t i = 1; i < 12; ++i)
                if (std::abs(reference - DnaSpectrum::centres[i]) <
                    std::abs(reference - DnaSpectrum::centres[nearest]))
                    nearest = i;
            weights[b] = std::exp(contrast * (shape.bands[nearest] - .45f) * 2.6f) *
                         (1 + contrast * .5f * shape.persistence[b]);
            sum += weights[b];
        }
        std::array<float, 12> goal{};
        for (std::size_t b = 0; b < 3; ++b)
        {
            const float hz = std::clamp(
                contrast > 0 ? std::exp(std::log(base[b]) * (1 - contrast) + shape.logPeaks[b] * contrast)
                             : base[b],
                120.f, float(sampleRate * .38));
            goal[b] = std::tan(3.14159265359f * hz / float(sampleRate));
            const float referenceQ =
                shape.resonance[b] * (.5f + .5f * shape.persistence[b]) * (1 - .65f * shape.features[0]);
            const float q = 2.2f + (referenceQ - 2.2f) * contrast;
            goal[3 + b] = 1 / std::clamp(q, 1.5f, 5.5f);
            goal[6 + b] = weights[b] / sum;
        }
        goal[9] = std::clamp(amount + .7f * contrast, 0.f, 1.f);
        goal[10] = std::clamp(warmth, 0.f, .6f);
        goal[11] = 1 - std::exp(-6.28318530718f * (2200 + 2200 * (1 - warmth)) / float(sampleRate));
        modernGoal = goal;
        if (!modernPrimed)
        {
            modern = goal;
            modernPrimed = true;
        }
        for (std::size_t i = 0; i < goal.size(); ++i)
            modernStep[i] = (goal[i] - modern[i]) / float(std::max(1, frames));
    }
    void processModern(float *frame, int channels) noexcept
    {
        for (std::size_t i = 0; i < modern.size(); ++i)
            modern[i] += modernStep[i];
        for (int ch = 0; ch < std::min(channels, 2); ++ch)
        {
            const float x = frame[ch];
            float bands = 0;
            for (std::size_t b = 0; b < 3; ++b)
            {
                auto &s = states[std::size_t(ch)][b];
                const float g = modern[b], k = modern[3 + b], a = 1 / (1 + g * (g + k)),
                            v1 = a * (s[0] + g * (x - s[1])), v2 = s[1] + g * v1;
                s[0] = 2 * v1 - s[0];
                s[1] = 2 * v2 - s[1];
                bands += k * v1 * modern[6 + b];
            }
            float coloured = x + 1.35f * modern[9] * bands;
            warmState[std::size_t(ch)] += modern[11] * (coloured - warmState[std::size_t(ch)]);
            frame[ch] = coloured + modern[10] * (warmState[std::size_t(ch)] - coloured);
        }
    }
private:
    std::array<float,12> modern{},modernStep{},modernGoal{};std::array<float,26> modernKey{};std::array<float,2> warmState{};bool modernPrimed=false;
    double sampleRate=48000;float coefficient=.01f;bool primed=false;
    std::array<float,3> frequencies {};
    std::array<std::array<std::array<float,2>,3>,2> states {};
};
