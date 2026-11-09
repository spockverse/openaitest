#pragma once
#include <algorithm>
#include <cmath>
#include "SignedCharacter.h"

// Version 1 remains the recall path for pre-centred projects/factory/user presets.
// Version 2 uses distance along each axis, never angle or a shared crush amount.
struct XYMapping
{
    float bits, rate, activity;
    static XYMapping map(float x,float y,double sampleRate,int version=3) noexcept
    {
        const float length=std::hypot(x,y);
        if(length>1){x/=length;y/=length;}
        if(version<2)return {16-(x+1)*7,static_cast<float>(500*std::pow(sampleRate/500,(y+1)*.5)),1};
        const float minimumRate=version>=8?32.f:500.f;
        const float horizontal=std::pow(std::abs(x),.35f),vertical=std::pow(std::abs(y),.65f);
        XYMapping result{16-14*horizontal,static_cast<float>(sampleRate*std::pow(minimumRate/sampleRate,vertical)),std::min(1.0f,length)};
        if(version>=3){const auto c=SignedCharacter::at(x,y);result.bits=std::clamp(result.bits+c.bitsOffset*horizontal,2.f,16.f);result.rate=std::clamp(result.rate*std::exp(c.logRate*vertical),minimumRate,float(sampleRate));}
        if(version>=10&&y>0){
            const float t=std::min(1.f,2*y),lower=t*t*(3-2*t),right=(1+std::tanh(3*x))*.5f;
            // Lower-left is coarse precision; lower-right is a slower clock.
            // Zero value and slope at the midline preserve the approved top half.
            result.bits=std::clamp(result.bits-lower*(3.4f*(1-right)+.4f*right)*horizontal,2.f,16.f);
            result.rate=std::clamp(result.rate*std::exp(-lower*(.2f*(1-right)+1.3f*right)*vertical),minimumRate,float(sampleRate));
        }
        return result;
    }
};
