#pragma once
#include "DnaProfile.h"
#include "TonalDSP-v044.h"
#include "NonlinearStage.h"
#include "CleanCrusher.h"
#include "SignedCharacter.h"
#include "SineCrush.h"
#include <algorithm>
#include <array>
#include <cmath>

class Legacy044CrushDSP
{
public:
    struct Parameters
    {
        float drive=12,bits=8,rate=12000,mix=.65f,output=-6,protect=1;
        float mutate=0,speed=1,activity=1,signedX=0,signedY=0;int processingVersion=2;bool centered=false;bool enabled=true,patternEnabled=true,inputActive=true;int mode=0;
    };
    void prepare(double sr)
    {
        sampleRate=std::max(8000.0,sr);
        smoothCoefficient=coefficient(.02f);modulationCoefficient=coefficient(.006f);
        attackCoefficient=coefficient(.002f);releaseCoefficient=coefficient(.025f);slowCoefficient=coefficient(.08f);
        lowCoefficient=1-std::exp(-2*pi*120/static_cast<float>(sampleRate));
        colourCoefficient=1-std::exp(-2*pi*1800/static_cast<float>(sampleRate));
        dcCoefficient=std::exp(-2*pi*12/static_cast<float>(sampleRate));detectorRelease=std::exp(-1/static_cast<float>(sampleRate*.04));
        nonlinear.prepare();crusher.prepare(sampleRate);tonal.prepare(sampleRate);sineCrush.prepare(sampleRate);controlInterval=std::max(1,int(sampleRate/250));reset();
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
        const bool modern=target.processingVersion>=3;
        const bool impact=target.processingVersion>=4;
        const bool sine=target.processingVersion>=5;
        const std::array<float,2> original {frame[0],channels==2?frame[1]:0};
        if(!primed){current=target;loopPatternTime=loopTime*current.speed;bypassWet=target.enabled?1.0f:0.0f;primed=true;}
        const float goal=target.enabled?1.0f:0.0f,step=1/static_cast<float>(sampleRate*.005);
        bypassWet+=std::clamp(goal-bypassWet,-step,step);
        if(modern){smooth(current.signedX,target.signedX);smooth(current.signedY,target.signedY);}
        smooth(current.activity,std::clamp(target.activity,0.0f,1.0f));
        if(target.centered&&target.activity<=0&&current.activity<1.0e-6f)current.activity=0;
        smooth(current.drive,std::clamp(target.drive,0.0f,36.0f));smooth(current.bits,std::clamp(target.bits,2.0f,16.0f));
        if(target.centered&&target.drive<=0&&current.drive<1.0e-6f)current.drive=0;
        smooth(current.rate,std::clamp(target.rate,500.0f,static_cast<float>(sampleRate)));
        if(modern){const float m=std::clamp(target.mix,0.f,1.f),stepMix=1/float(sampleRate*.02);current.mix+=std::clamp(m-current.mix,-stepMix,stepMix);}else smooth(current.mix,std::clamp(target.mix,0.0f,1.0f));smooth(current.output,std::clamp(target.output,-24.0f,6.0f));
        smooth(current.protect,std::clamp(target.protect,0.0f,1.0f));if(modern){const float m=std::clamp(target.mutate,0.f,1.f),stepM=1/float(sampleRate*.02);current.mutate+=std::clamp(m-current.mutate,-stepM,stepM);}else smooth(current.mutate,std::clamp(target.mutate,0.0f,1.0f));smooth(current.speed,std::clamp(target.speed,.25f,4.0f));
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
        const float influence=current.mutate*(modern&&!loaded?0:dnaPresence);
        const bool updateControl=modern&&controlRemaining<=0;
        if(updateControl){modernShape=loaded&&influence>0?dna->spectralAt(float(patternPosition)):DnaTemporalFrame{};modernCharacter=SignedCharacter::at(current.signedX,current.signedY,impact?4.f:2.f);}
        if(!modern){
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
        }
        const auto* signature=smoothedSpectrum.valid?&smoothedSpectrum:nullptr;
        const float rough=modern?modernShape.features[0]:signature?signature->features[0]:.2f;
        const float harmonic=modern?modernShape.features[1]:signature?signature->features[1]:.5f;
        const float even=modern?modernShape.features[2]:signature?signature->features[2]:.5f;
        const float crest=modern?modernShape.features[3]:signature?signature->features[3]:.3f;
        const float driveUnit=std::clamp(current.drive/36.f,0.f,1.f);
        const float drivePush=driveUnit*(.35f+.65f*driveUnit);
        const float originalDrive=std::clamp(current.drive/24,0.0f,1.0f);
        const float drive=target.centered?std::min(1.0f,originalDrive+.32f*current.activity):originalDrive;
        const float crush=(16-current.bits)/14;
        // Sample rhythm modulates coefficients only, in proportion to live articulation.
        const float articulation=std::clamp((fast-slow)/(slow+.03f),-1.0f,1.0f);
        const float sampleMotion=influence*(modulation-.5f)*(.35f+.2f*std::abs(articulation));
        const float vowel=std::clamp(.2f+.4f*(fast/(fast+.1f))+.25f*articulation+.25f*brightness+.12f*crush+sampleMotion,0.0f,1.0f);
        const float bits=current.bits;
        const float rate=current.rate;
        const float gain=1+current.drive*.16f*(1+influence*(.8f*harmonic+.5f*rough))
                         +(target.centered?current.activity*(.6f+influence*(.8f*harmonic+.5f*rough)):0);
        const float asymmetry=influence*(2.8f*(even-.5f)+.5f*(crest-.3f)+.35f*(modulation-.5f));
        const float edge=drive*(.2f+.8f*crush+2.5f*influence*rough);
        if(updateControl)
        {
            const float spectralContrast=impact?influence*(.55f+.45f*influence):influence*influence*(.35f+.65f*influence);
            const float modernGain=impact?1+current.activity*(1+2.5f*modernCharacter.shaper)+14*drivePush+spectralContrast*(1+1.5f*harmonic+rough):1+current.drive*.12f+current.activity*(.7f+.8f*modernCharacter.shaper)+spectralContrast*(.8f*harmonic+.6f*rough);
            const float modernAsym=(impact?current.activity*(.5f+2*modernCharacter.warmth-.6f*modernCharacter.shaper):0)+influence*(2.2f*(even-.5f)+.4f*(modulation-.5f));
            const float modernEdge=impact?std::clamp(current.activity*(.25f+2.2f*modernCharacter.shaper)+2.5f*drivePush+spectralContrast*(1+1.5f*rough),0.f,6.f):std::clamp(.1f+modernCharacter.shaper*.65f+spectralContrast*(.4f+.8f*rough),0.f,1.6f);
            const std::array<float,6> controlGoal{modernGain,modernAsym,modernEdge,std::pow(10.f,current.output/20),4+(impact?6*modernCharacter.shaper+4*drivePush:0)+spectralContrast*(6+10*rough+8*harmonic),impact?.20f/std::sqrt(std::sqrt(modernGain)):0};
            if(!controlPrimed){modernControl=controlGoal;controlPrimed=true;}
            for(std::size_t i=0;i<modernControl.size();++i)modernStep[i]=(controlGoal[i]-modernControl[i])/float(controlInterval);
            crusher.setModern(current.bits,current.rate,modernCharacter.antiAlias,modernCharacter.rebuild,controlInterval);
            tonal.setModern(impact?std::clamp(vowel+.25f*current.signedX-.2f*current.signedY,0.f,1.f):vowel,.12f+modernCharacter.shaper*.38f,influence,modernShape,modernCharacter.warmth,controlInterval,impact);
            if(sine){
                const bool advanced=target.processingVersion>=6;
                const float dnaShift=advanced?influence*(std::clamp(modernShape.logPeaks[0]-6.5510803f,-1.1f,1.1f)+.7f*(modernShape.bands[3]-modernShape.bands[8])):0;
                const float dnaMotion=advanced?influence*(2*rough-1+.5f*(modulation-.5f)):0;
                sineCrush.set(current.signedX,current.signedY,current.activity,current.bits,current.rate,driveUnit,current.protect,articulation,controlInterval,advanced,dnaShift,dnaMotion);
                // Keep optional DNA coloration; no generic moving resonators in the core.
                tonal.setModern(.4f,.25f*influence,influence,modernShape,0,controlInterval,false);
            }
            controlRemaining=controlInterval;
        }
        if(modern){for(std::size_t i=0;i<modernControl.size();++i)modernControl[i]+=modernStep[i];--controlRemaining;}
        const float output=modern?modernControl[3]:std::pow(10.0f,current.output/20);
        const std::array<float,2> dry {frame[0],channels==2?frame[1]:0};
        if(sine)
        {
            sineCrush.process(frame,channels);
            tonal.processModern(frame,channels);
            for(int ch=0;ch<std::min(channels,2);++ch){const auto c=std::size_t(ch);
                const float blend=std::min(1.f,current.activity*8)*current.mix;
                const float processed=(dry[c]+blend*(frame[ch]-dry[c]))*output;
                frame[ch]=original[c]+bypassWet*(processed-original[c]);
            }
            loopTime+=1/sampleRate;retriggerTime+=1/sampleRate;
            if(advancing){loopPatternTime+=current.speed/sampleRate;retriggerPatternTime+=current.speed/sampleRate;}
            return;
        }
        const int tail=static_cast<int>(sampleRate*.08);
        if(peak<=0)silentFrames=std::min(silentFrames+1,tail+1);else silentFrames=0;
        if(silentFrames==tail)clearAudio();
        const float subProtection=impact?current.protect*std::max(.1f,1-.4f*current.activity-.45f*influence-.2f*drivePush):modern?current.protect*(1-.65f*influence*influence):current.protect;
        for(int ch=0;ch<std::min(channels,2);++ch)
        {
            const auto c=static_cast<std::size_t>(ch);
            low[c]+=lowCoefficient*(dry[c]-low[c]);
            frame[ch]=dry[c]-subProtection*low[c];
        }
        // Keep Sub removes much of a sine bass before quantisation. Calibrate
        // that protected-band range so coarse bit settings still generate
        // harmonics instead of rounding the whole signal to zero. Undo the
        // calibration immediately; it must never act as an output gain boost.
        const float quantizerRange=target.centered?1+3*current.activity:1;
        for(int ch=0;ch<std::min(channels,2);++ch)frame[ch]*=quantizerRange;
        if(modern)crusher.processModern(frame,channels);else crusher.process(frame,channels,bits,rate);
        for(int ch=0;ch<std::min(channels,2);++ch)frame[ch]/=quantizerRange;
        for(int ch=0;ch<std::min(channels,2);++ch)
        {
            const auto c=static_cast<std::size_t>(ch);
            const float source=frame[ch];
            const float shaped=modern?source:nonlinear.tick(source,ch,gain,asymmetry,edge);
            const float blend=std::min(1.0f,current.drive/6+(target.centered?.55f*current.activity:0));
            const float driven=modern?nonlinear.tickResidual(source,ch,modernControl[0],modernControl[1],modernControl[2],impact?std::clamp(.45f+.55f*current.activity,0.f,1.f):std::clamp(.2f+.65f*current.activity+originalDrive*.25f,0.f,1.f),modernControl[4],modernControl[5]):source+blend*(shaped-source);
            const float dc=driven-dcInput[c]+dcCoefficient*dcOutput[c];dcInput[c]=driven;dcOutput[c]=dc;
            frame[ch]=dc;
        }
        if(modern)tonal.processModern(frame,channels);else tonal.process(frame,channels,vowel,2.2f+influence*(3.0f*(1-rough)+crest),drive*(.5f+.45f*influence),signature,influence);
        for(int ch=0;ch<std::min(channels,2);++ch)
        {
            const auto c=static_cast<std::size_t>(ch);
            // No post-crusher hard clip: retain generated harmonics and intentional
            // crusher edges without another alias-generating nonlinear stage.
            const float coloured=frame[ch];
            const float release=std::clamp(static_cast<float>(tail-silentFrames)/static_cast<float>(sampleRate*.02),0.0f,1.0f);
            const float tailGain=release*release*(3-2*release);
            // Extra crest headroom is limited to the coarsest quantizer settings.
            const float coarseHeadroom=impact?std::max(0.f,(4-current.bits)*.5f):0;
            const float headroom=impact?1/(1+.28f*current.activity+.1f*drivePush+.25f*influence+.3f*coarseHeadroom*coarseHeadroom):modern?1/(1+.35f*current.activity+.25f*originalDrive):target.centered?1/(1+1.8f*drive+.25f*current.activity):1;
            const float wet=(coloured+subProtection*low[c])*tailGain*headroom;
            // A short continuous centre transition removes quantizer/DC/filter residuals
            // at exactly neutral, without a dry dead zone. Axis mapping still
            // determines the sound throughout the field; DRIVE adds intensity away from neutral.
            const float fieldBlend=target.centered?std::min(1.0f,current.activity*8):1;
            const float processed=(dry[c]+current.mix*fieldBlend*(wet-dry[c]))*output;
            frame[ch]=bypassWet<=0?original[c]:bypassWet>=1?processed:original[c]+bypassWet*(processed-original[c]);
        }
        loopTime+=1/sampleRate;retriggerTime+=1/sampleRate;if(advancing){loopPatternTime+=current.speed/sampleRate;retriggerPatternTime+=current.speed/sampleRate;}
    }
private:
    void clearAudio()noexcept{controlRemaining=0;controlPrimed=false;low={};dcInput={};dcOutput={};colour={};nonlinear.reset();crusher.reset();tonal.reset();sineCrush.reset();}
    float coefficient(float seconds)const noexcept{return 1-std::exp(-1/static_cast<float>(sampleRate*seconds));}
    void smooth(float& v,float target)const noexcept{v+=smoothCoefficient*(target-v);}
    static constexpr float pi=3.14159265359f;
    double sampleRate=48000,loopTime=0,retriggerTime=0,patternPosition=0,loopPatternTime=0,retriggerPatternTime=0;
    float smoothCoefficient=.001f,modulationCoefficient=.01f,attackCoefficient=.01f,releaseCoefficient=.001f,slowCoefficient=.001f;
    float lowCoefficient=.01f,colourCoefficient=.2f,dcCoefficient=.995f,detectorRelease=.999f;
    float detector=0,modulation=0,fast=0,slow=0,brightness=0,bypassWet=1;
    int refractory=0,silentFrames=0,activityHold=0;bool primed=false,retriggerArmed=false;
    DnaSpectrum smoothedSpectrum;float dnaPresence=0;
    int controlInterval=192,controlRemaining=0;bool controlPrimed=false;
    DnaTemporalFrame modernShape;SignedCharacter modernCharacter;std::array<float,6> modernControl{},modernStep{};
    Parameters current;NonlinearStage nonlinear;CleanCrusher crusher;Legacy044TonalDSP tonal;SineCrush sineCrush;
    std::array<float,2> low{},dcInput{},dcOutput{},colour{};
};
