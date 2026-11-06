#pragma once
#include "DnaProfile.h"
#include "TonalDSP.h"
#include "NonlinearStage.h"
#include "CleanCrusher.h"
#include <algorithm>
#include <array>
#include <cmath>

class CrushDSP
{
public:
    struct Parameters
    {
        float drive=12,bits=8,rate=12000,mix=.65f,output=-6,protect=1;
        float mutate=0,speed=1;bool enabled=true,patternEnabled=true,inputActive=true;int mode=0;
    };
    void prepare(double sr)
    {
        sampleRate=std::max(8000.0,sr);
        smoothCoefficient=coefficient(.02f);modulationCoefficient=coefficient(.006f);
        attackCoefficient=coefficient(.002f);releaseCoefficient=coefficient(.025f);slowCoefficient=coefficient(.08f);
        lowCoefficient=1-std::exp(-2*pi*120/static_cast<float>(sampleRate));
        colourCoefficient=1-std::exp(-2*pi*1800/static_cast<float>(sampleRate));
        dcCoefficient=std::exp(-2*pi*12/static_cast<float>(sampleRate));detectorRelease=std::exp(-1/static_cast<float>(sampleRate*.04));
        nonlinear.prepare();crusher.prepare(sampleRate);tonal.prepare(sampleRate);reset();
    }
    void reset()
    {
        clearAudio();smoothedSpectrum={};dnaPresence=0;fast=slow=brightness=0;silentFrames=0;activityHold=0;primed=false;loopTime=loopPatternTime=0;bypassWet=1;patternPosition=0;resetModulation();
    }
    void resetModulation()noexcept{retriggerTime=retriggerPatternTime=0;modulation=0;detector=0;refractory=0;retriggerArmed=false;}
    void setTimeline(double seconds)noexcept{seconds=std::max(0.0,seconds);if(std::abs(seconds-loopTime)>2/sampleRate)loopPatternTime=seconds*current.speed;loopTime=seconds;}
    void seekPattern(double seconds,float speed)noexcept{loopPatternTime=std::max(0.0,seconds)*speed;}
    void noteOn()noexcept{patternPosition=0;retriggerTime=retriggerPatternTime=0;retriggerArmed=true;refractory=static_cast<int>(sampleRate*.05);}
    double getPatternPosition()const noexcept{return patternPosition;}
    double getRetriggerTime()const noexcept{return retriggerTime;}
    void processFrame(float* frame,int channels,const Parameters& target,const DnaProfile* dna=nullptr)noexcept
    {
        const std::array<float,2> original {frame[0],channels==2?frame[1]:0};
        if(!primed){current=target;loopPatternTime=loopTime*current.speed;bypassWet=target.enabled?1.0f:0.0f;primed=true;}
        const float goal=target.enabled?1.0f:0.0f,step=1/static_cast<float>(sampleRate*.005);
        bypassWet+=std::clamp(goal-bypassWet,-step,step);
        smooth(current.drive,std::clamp(target.drive,0.0f,36.0f));smooth(current.bits,std::clamp(target.bits,2.0f,16.0f));
        smooth(current.rate,std::clamp(target.rate,500.0f,static_cast<float>(sampleRate)));
        smooth(current.mix,std::clamp(target.mix,0.0f,1.0f));smooth(current.output,std::clamp(target.output,-24.0f,6.0f));
        smooth(current.protect,std::clamp(target.protect,0.0f,1.0f));smooth(current.mutate,std::clamp(target.mutate,0.0f,1.0f));smooth(current.speed,std::clamp(target.speed,.25f,4.0f));
        if(current.mode!=target.mode){current.mode=target.mode;resetModulation();}
        float peak=0,high=0;
        for(int ch=0;ch<std::min(channels,2);++ch)
        {
            if(!std::isfinite(frame[ch]))frame[ch]=0;
            peak=std::max(peak,std::abs(frame[ch]));
            colour[static_cast<std::size_t>(ch)]+=colourCoefficient*(frame[ch]-colour[static_cast<std::size_t>(ch)]);
            high=std::max(high,std::abs(frame[ch]-colour[static_cast<std::size_t>(ch)]));
        }
        if(refractory>0)--refractory;
        if(target.patternEnabled&&current.mode==1&&refractory==0&&peak>.015f&&peak>detector*1.8f+.008f)noteOn();
        detector=std::max(peak,detector*detectorRelease);
        fast+=(peak>fast?attackCoefficient:releaseCoefficient)*(peak-fast);slow+=slowCoefficient*(peak-slow);
        brightness+=modulationCoefficient*(std::clamp(high/(fast+.002f),0.0f,1.0f)-brightness);
        const bool loaded=dna&&dna->valid;
        const bool running=loaded&&(current.mode==0||(retriggerArmed&&retriggerPatternTime<dna->duration));
        if(peak>1.0e-5f)activityHold=static_cast<int>(sampleRate*.002);else if(activityHold>0)--activityHold;
        const bool advancing=target.patternEnabled&&target.inputActive&&activityHold>0;
        if(loaded&&advancing)patternPosition=current.mode==1&&retriggerPatternTime>=dna->duration?1.0:std::fmod((current.mode==0?loopPatternTime:retriggerPatternTime)/dna->duration,1.0);
        else if(!loaded)patternPosition=0;
        const float env=running?dna->at(advancing?(current.mode==0?loopPatternTime:retriggerPatternTime):patternPosition*dna->duration):0;
        modulation+=modulationCoefficient*(env-modulation);
        dnaPresence+=modulationCoefficient*((loaded?1.0f:0.0f)-dnaPresence);
        const float influence=current.mutate*dnaPresence;
        if(loaded&&dna->spectrum.valid)
        {
            if(!smoothedSpectrum.valid){smoothedSpectrum=dna->spectrum;}
            else{
                for(std::size_t i=0;i<12;++i)smoothedSpectrum.bands[i]+=modulationCoefficient*(dna->spectrum.bands[i]-smoothedSpectrum.bands[i]);
                for(std::size_t i=0;i<5;++i)smoothedSpectrum.features[i]+=modulationCoefficient*(dna->spectrum.features[i]-smoothedSpectrum.features[i]);
                for(std::size_t i=0;i<3;++i){smoothedSpectrum.peaks[i]+=modulationCoefficient*(dna->spectrum.peaks[i]-smoothedSpectrum.peaks[i]);smoothedSpectrum.peakLevels[i]+=modulationCoefficient*(dna->spectrum.peakLevels[i]-smoothedSpectrum.peakLevels[i]);}
            }
        }
        if(loaded&&!dna->spectrum.valid)smoothedSpectrum.valid=false;
        const auto* signature=smoothedSpectrum.valid?&smoothedSpectrum:nullptr;
        const float rough=signature?signature->features[0]:.2f;
        const float harmonic=signature?signature->features[1]:.5f;
        const float even=signature?signature->features[2]:.5f;
        const float crest=signature?signature->features[3]:.3f;
        const float drive=std::clamp(current.drive/24,0.0f,1.0f);
        const float crush=(16-current.bits)/14;
        // Sample rhythm modulates coefficients only, in proportion to live articulation.
        const float articulation=std::clamp((fast-slow)/(slow+.03f),-1.0f,1.0f);
        const float sampleMotion=influence*(modulation-.5f)*(.35f+.2f*std::abs(articulation));
        const float vowel=std::clamp(.2f+.4f*(fast/(fast+.1f))+.25f*articulation+.25f*brightness+.12f*crush+sampleMotion,0.0f,1.0f);
        const float bits=current.bits;
        const float rate=current.rate;
        const float gain=1+current.drive*.16f*(1+influence*(.8f*harmonic+.5f*rough));
        const float asymmetry=influence*(2.8f*(even-.5f)+.5f*(crest-.3f)+.35f*(modulation-.5f));
        const float edge=drive*(.2f+.8f*crush+2.5f*influence*rough);
        const float output=std::pow(10.0f,current.output/20);
        const std::array<float,2> dry {frame[0],channels==2?frame[1]:0};
        const int tail=static_cast<int>(sampleRate*.08);
        if(peak<=0)silentFrames=std::min(silentFrames+1,tail+1);else silentFrames=0;
        if(silentFrames==tail)clearAudio();
        for(int ch=0;ch<std::min(channels,2);++ch)
        {
            const auto c=static_cast<std::size_t>(ch);
            low[c]+=lowCoefficient*(dry[c]-low[c]);
            frame[ch]=dry[c]-current.protect*low[c];
        }
        crusher.process(frame,channels,bits,rate);
        for(int ch=0;ch<std::min(channels,2);++ch)
        {
            const auto c=static_cast<std::size_t>(ch);
            const float source=frame[ch];
            const float shaped=nonlinear.tick(source,ch,gain,asymmetry,edge);
            const float blend=std::min(1.0f,current.drive/6);
            const float driven=source+blend*(shaped-source);
            const float dc=driven-dcInput[c]+dcCoefficient*dcOutput[c];dcInput[c]=driven;dcOutput[c]=dc;
            frame[ch]=dc;
        }
        tonal.process(frame,channels,vowel,2.2f+influence*(3.0f*(1-rough)+crest),drive*(.5f+.45f*influence),signature,influence);
        for(int ch=0;ch<std::min(channels,2);++ch)
        {
            const auto c=static_cast<std::size_t>(ch);
            // No post-crusher hard clip: retain generated harmonics and intentional
            // crusher edges without another alias-generating nonlinear stage.
            const float coloured=frame[ch];
            const float release=std::clamp(static_cast<float>(tail-silentFrames)/static_cast<float>(sampleRate*.02),0.0f,1.0f);
            const float tailGain=release*release*(3-2*release);
            const float wet=(coloured+current.protect*low[c])*tailGain;
            const float processed=(dry[c]+current.mix*(wet-dry[c]))*output;
            frame[ch]=bypassWet<=0?original[c]:bypassWet>=1?processed:original[c]+bypassWet*(processed-original[c]);
        }
        loopTime+=1/sampleRate;retriggerTime+=1/sampleRate;if(advancing){loopPatternTime+=current.speed/sampleRate;retriggerPatternTime+=current.speed/sampleRate;}
    }
private:
    void clearAudio()noexcept{low={};dcInput={};dcOutput={};colour={};nonlinear.reset();crusher.reset();tonal.reset();}
    float coefficient(float seconds)const noexcept{return 1-std::exp(-1/static_cast<float>(sampleRate*seconds));}
    void smooth(float& v,float target)const noexcept{v+=smoothCoefficient*(target-v);}
    static constexpr float pi=3.14159265359f;
    double sampleRate=48000,loopTime=0,retriggerTime=0,patternPosition=0,loopPatternTime=0,retriggerPatternTime=0;
    float smoothCoefficient=.001f,modulationCoefficient=.01f,attackCoefficient=.01f,releaseCoefficient=.001f,slowCoefficient=.001f;
    float lowCoefficient=.01f,colourCoefficient=.2f,dcCoefficient=.995f,detectorRelease=.999f;
    float detector=0,modulation=0,fast=0,slow=0,brightness=0,bypassWet=1;
    int refractory=0,silentFrames=0,activityHold=0;bool primed=false,retriggerArmed=false;
    DnaSpectrum smoothedSpectrum;float dnaPresence=0;
    Parameters current;NonlinearStage nonlinear;CleanCrusher crusher;TonalDSP tonal;
    std::array<float,2> low{},dcInput{},dcOutput{},colour{};
};
