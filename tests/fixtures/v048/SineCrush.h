#pragma once
#include "CleanCrusher.h"
#include <array>
#include <algorithm>
#include <cmath>

// Models 5/6 share the complementary bass path and audio-rate sine carrier.
// No audio playback, allocation, lookahead or UI clock. Coefficients update at
// the engine's bounded control rate; oscillator and hold phases survive blocks.
class Legacy048SineCrush
{
public:
    void prepare(double rate) noexcept
    {
        fs=rate; lowK=1-std::exp(-6.28318530718f*240/float(fs));
        upperK=1-std::exp(-6.28318530718f*6000/float(fs));
        legacyLowK=lowK;correctedLowK=1-std::exp(-6.28318530718f*160/float(fs));
        crusher.prepare(fs);reset();
    }
    void reset() noexcept
    {
        low={};delta={};mid={};phase=0;re=1;im=0;counter=0;primed=false;
        control={};step={};crusher.reset();
    }
    void set(float x,float y,float activity,float bits,float rate,float drive,
             float protection,float articulation,int frames,bool enhanced=false,
             float dnaShift=0,float dnaMotion=0,bool correctedRouting=false,bool expressiveRouting=false,
             bool deepControls=false,float crusherDrive=0,float mutation=0,float referenceLogFrequency=6.2146081f) noexcept
    {
        expressive=expressiveRouting;
        deepened=deepControls;
        const float right=std::clamp((x+1)*.5f,0.f,1.f),bottom=std::clamp((1+(expressive?y:-y))*.5f,0.f,1.f);
        const float metal=right*(1-bottom),robot=right*bottom,warm=(1-right)*bottom;
        advanced=enhanced;digital=correctedRouting;
        lowK=digital?correctedLowK:legacyLowK;
        const float crunch=(1-right)*(1-bottom),reach=std::sqrt(std::clamp(activity,0.f,1.f));
        float targetFrequency=advanced
            ? std::clamp((33+740*metal*metal+180*robot+(expressive?395.f:95.f)*crunch)*(1+.3f*articulation)*std::exp(.65f*dnaShift),20.f,1600.f)
            : std::clamp((47+330*metal+85*robot)*(1+.1f*articulation),20.f,1600.f);
        float depth=advanced?std::clamp(reach*(.2f+1.5f*metal+.8f*robot+.2f*crunch)+.15f*dnaMotion,0.f,.9f)
                                  :activity*(.12f+.72f*metal+.4f*robot);
        const float t=deepControls?std::clamp(2*y,0.f,1.f):0,lower=t*t*(3-2*t),lowerRight=(1+std::tanh(3*x))*.5f;
        if(deepControls){
            targetFrequency*=std::exp(lower*(-.5f*(1-lowerRight)+.3f*lowerRight));
            if(mutation>0)targetFrequency=std::exp((1-mutation)*std::log(targetFrequency)+mutation*std::clamp(referenceLogFrequency,3.4011974f,7.3777589f));
            targetFrequency=std::clamp(targetFrequency,20.f,1600.f);
            depth=std::clamp(depth*(1-.6f*lower*(1-lowerRight))+.22f*crusherDrive*reach,0.f,.95f);
        }
        const float band=std::clamp(600+1000*right+600*articulation+800*dnaMotion,400.f,3200.f);
        const std::array<float,10> goal{targetFrequency,depth,
            1+drive*(2+6*drive),advanced?std::clamp(robot*reach*2,0.f,1.f):robot*activity,
            1/std::sqrt(1+drive*(2+6*drive)),std::clamp(protection,0.f,1.f),
            advanced?1-std::exp(-6.28318530718f*band/float(fs)):0,
            advanced?std::clamp(reach*(.25f+.75f*metal)+.2f*dnaMotion,0.f,1.f):0,
            expressive?std::clamp(depth*(.6f*crunch+.55f*robot+.12f*warm)+.15f*crusherDrive*reach,0.f,.8f):0,
            deepControls?1+reach*(.5f*crusherDrive+1.5f*crusherDrive*crusherDrive):1};
        if(!primed){control=goal;primed=true;}
        for(std::size_t i=0;i<goal.size();++i)step[i]=(goal[i]-control[i])/float(std::max(1,frames));
        // Keep a finite quantizer range on filtered inputs; intentional clock
        // artifacts remain, with only restrained reconstruction at warm positions.
        float effectiveBits=digital?bits:advanced?std::max(3.f,bits-reach*(.9f*crunch+.4f*robot)+.6f*dnaMotion):std::max(4.f,bits);
        float effectiveRate=digital?rate:advanced?rate*std::exp(reach*(-1.1f*robot+.25f*warm)):rate;
        if(deepControls){
            // DRIVE changes the digital resolution/clock themselves. DNA adds
            // bounded, reference-derived motion to that same continuous clock.
            effectiveBits-=1.5f*crusherDrive+2*crusherDrive*crusherDrive+.9f*mutation*(1-dnaMotion);
            effectiveRate*=std::exp(-1.25f*crusherDrive+.6f*dnaShift+.5f*dnaMotion);
        }
        crusher.setModern(effectiveBits,effectiveRate,(.04f+.12f*warm)*(1-.65f*lower),(.04f+.24f*warm)*(1-.7f*lower),frames,digital?32.f:500.f);
    }
    const float* foundation()const noexcept{return low.data();}
    void process(float* frame,int channels,const float* original=nullptr) noexcept
    {
        for(std::size_t i=0;i<control.size();++i)control[i]+=step[i];
        if(counter++%64==0){re=std::cos(6.283185307179586*phase);im=std::sin(6.283185307179586*phase);
            const float angle=6.28318530718f*control[0]/float(fs);rotationR=std::cos(angle);rotationI=std::sin(angle);frequency=control[0];}
        float carrier=im;const float harmonicCarrier=2*im*re;const float next=re*rotationR-im*rotationI;
        im=im*rotationR+re*rotationI;re=next;phase+=frequency/fs;phase-=std::floor(phase);
        carrier+=control[3]*(std::round(carrier*3)/3-carrier);
        std::array<float,2> dry{},body{};
        for(int ch=0;ch<std::min(channels,2);++ch){const auto c=std::size_t(ch);dry[c]=original?original[ch]:frame[ch];
            low[c]+=lowK*(dry[c]-low[c]);body[c]=digital?frame[ch]:frame[ch]-control[5]*low[c];
            const float input=body[c]*control[2];
            const float shaped=input/std::sqrt(1+input*input)*control[4];
            if(expressive){
                // Keep the deliberate input-driven degradation before the clock;
                // don't decimate away the entire audio-rate modulation spectrum.
                frame[ch]=shaped*((1-control[8])+control[8]*carrier);
            }else if(advanced){
                // Broad complementary weighting, not a second modulation clock
                // or resonant sweep. Both carriers use the same continuous phase.
                mid[c]+=control[6]*(shaped-mid[c]);
                const float upper=shaped-mid[c];
                const float upperCarrier=carrier+control[7]*(harmonicCarrier-carrier);
                const float upperDepth=control[1]*.78f;
                frame[ch]=(digital?1.f:4.f)*(mid[c]*((1-control[1])+control[1]*carrier)
                           +upper*((1-upperDepth)+upperDepth*upperCarrier));
            }else frame[ch]=(digital?1.f:4.f)*shaped*((1-control[1])+control[1]*carrier);}
        crusher.processModern(frame,channels);
        for(int ch=0;ch<std::min(channels,2);++ch){const auto c=std::size_t(ch);
            if(expressive){
                // Same oscillator and complementary band split as before,
                // applied to held audio so its sidebands survive the hold clock.
                mid[c]+=control[6]*(frame[ch]-mid[c]);const float upper=frame[ch]-mid[c];
                const float upperCarrier=carrier+control[7]*(harmonicCarrier-carrier);
                const float upperDepth=control[1]*.78f;
                frame[ch]=mid[c]*((1-control[1])+control[1]*carrier)
                         +upper*((1-upperDepth)+upperDepth*upperCarrier);
            }
            if(digital){
                // Crush the full excitation, then restore only the protected
                // low-frequency error. The crossover sums to unity if Q(x)=x.
                const float change=frame[ch]-dry[c];delta[c]+=lowK*(change-delta[c]);
                if(deepened&&control[9]>1)frame[ch]=dry[c]+control[9]*(change-control[5]*delta[c]);
                else frame[ch]=dry[c]+change-control[5]*delta[c];
            }else{
                const float change=frame[ch]*.25f-body[c];delta[c]+=upperK*(change-delta[c]);
                frame[ch]=dry[c]+delta[c];
            }}
    }
private:
    friend struct DigitalRoutingProbe;
    float legacyLowK=0,correctedLowK=0;
    double fs=48000,phase=0;float lowK=0,upperK=0,re=1,im=0,rotationR=1,rotationI=0,frequency=47;
    unsigned counter=0;bool primed=false,advanced=false,digital=false,expressive=false,deepened=false;std::array<float,2> low{},delta{},mid{};
    std::array<float,10> control{},step{};CleanCrusher crusher;
};
