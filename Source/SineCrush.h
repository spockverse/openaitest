#pragma once
#include "CleanCrusher.h"
#include <array>
#include <algorithm>
#include <cmath>

// Model 5: the audition's complementary bass path and audio-rate sine carrier.
// No audio playback, allocation, lookahead or UI clock. Coefficients update at
// the engine's bounded control rate; oscillator and hold phases survive blocks.
class SineCrush
{
public:
    void prepare(double rate) noexcept
    {
        fs=rate; lowK=1-std::exp(-6.28318530718f*240/float(fs));
        upperK=1-std::exp(-6.28318530718f*6000/float(fs));
        crusher.prepare(fs);reset();
    }
    void reset() noexcept
    {
        low={};delta={};phase=0;re=1;im=0;counter=0;primed=false;
        control={};step={};crusher.reset();
    }
    void set(float x,float y,float activity,float bits,float rate,float drive,
             float protection,float articulation,int frames) noexcept
    {
        const float right=std::clamp((x+1)*.5f,0.f,1.f),bottom=std::clamp((1-y)*.5f,0.f,1.f);
        const float metal=right*(1-bottom),robot=right*bottom,warm=(1-right)*bottom;
        const float targetFrequency=std::clamp((47+330*metal+85*robot)*(1+.1f*articulation),20.f,1600.f);
        const std::array<float,6> goal{targetFrequency,activity*(.12f+.72f*metal+.4f*robot),
            1+drive*(2+6*drive),robot*activity,1/std::sqrt(1+drive*(2+6*drive)),std::clamp(protection,0.f,1.f)};
        if(!primed){control=goal;primed=true;}
        for(std::size_t i=0;i<goal.size();++i)step[i]=(goal[i]-control[i])/float(std::max(1,frames));
        // Keep a finite quantizer range on filtered inputs; intentional clock
        // artifacts remain, with only restrained reconstruction at warm positions.
        crusher.setModern(std::max(4.f,bits),rate,.04f+.12f*warm,.04f+.24f*warm,frames);
    }
    void process(float* frame,int channels) noexcept
    {
        for(std::size_t i=0;i<control.size();++i)control[i]+=step[i];
        if(counter++%64==0){re=std::cos(6.283185307179586*phase);im=std::sin(6.283185307179586*phase);
            const float angle=6.28318530718f*control[0]/float(fs);rotationR=std::cos(angle);rotationI=std::sin(angle);frequency=control[0];}
        float carrier=im;const float next=re*rotationR-im*rotationI;
        im=im*rotationR+re*rotationI;re=next;phase+=frequency/fs;phase-=std::floor(phase);
        carrier+=control[3]*(std::round(carrier*3)/3-carrier);
        std::array<float,2> dry{},body{};
        for(int ch=0;ch<std::min(channels,2);++ch){const auto c=std::size_t(ch);dry[c]=frame[ch];
            low[c]+=lowK*(frame[ch]-low[c]);body[c]=frame[ch]-control[5]*low[c];
            const float input=body[c]*control[2];
            const float shaped=input/std::sqrt(1+input*input)*control[4];
            frame[ch]=4*shaped*((1-control[1])+control[1]*carrier);}
        crusher.processModern(frame,channels);
        for(int ch=0;ch<std::min(channels,2);++ch){const auto c=std::size_t(ch);
            const float change=frame[ch]*.25f-body[c];delta[c]+=upperK*(change-delta[c]);
            frame[ch]=dry[c]+delta[c];}
    }
private:
    double fs=48000,phase=0;float lowK=0,upperK=0,re=1,im=0,rotationR=1,rotationI=0,frequency=47;
    unsigned counter=0;bool primed=false;std::array<float,2> low{},delta{};
    std::array<float,6> control{},step{};CleanCrusher crusher;
};
