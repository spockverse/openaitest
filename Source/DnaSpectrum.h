#pragma once
#include <array>
#include <complex>
#include <vector>
#include <algorithm>
#include <cmath>

// Off-audio-thread analysis only. No audio samples are retained in this profile.
struct DnaSpectrum
{
    static constexpr std::array<float,12> centres {140,220,330,500,750,1100,1600,2300,3300,4700,6500,9000};
    std::array<float,12> bands {};
    // spectral flatness, harmonic concentration, even-harmonic bias, crest, brightness
    std::array<float,5> features {.2f,.5f,.5f,.3f,.3f};
    std::array<float,3> peaks {500,1400,2800};
    std::array<float,3> peakLevels {.5f,.5f,.5f};
    bool valid=false;
    void prepareDerived()noexcept
    {
        for(std::size_t b=0;b<3;++b)
        {
            std::size_t nearest=0;
            for(std::size_t j=1;j<centres.size();++j)
                if(std::abs(std::log(peaks[b]/centres[j]))<std::abs(std::log(peaks[b]/centres[nearest])))nearest=j;
            peakLevels[b]=bands[nearest];
        }
    }
    static DnaSpectrum analyse(const float* const* channels,int count,std::size_t frames,double sr)
    {
        DnaSpectrum s;std::size_t n=1024;
        while(sr/static_cast<double>(n)>24&&n<16384)n*=2;
        std::vector<double> power(n/2);
        std::vector<std::complex<double>> fft(n);
        double energy=0;float maximum=0;std::size_t windows=0;
        // At most 192 evenly spaced windows include the whole sample, up to 30s.
        const auto hop=std::max<std::size_t>(n/2,frames/192);
        for(std::size_t start=0;start<frames;start+=hop)
        {
            for(int ch=0;ch<count;++ch)
            {
                for(std::size_t i=0;i<n;++i)
                {
                    const float raw=start+i<frames?channels[ch][start+i]:0;
                    const double v=std::isfinite(raw)?std::clamp(raw,-8.0f,8.0f):0;
                    fft[i]=v*(.5-.5*std::cos(6.283185307179586*i/(n-1)));
                    energy+=v*v;maximum=std::max(maximum,static_cast<float>(std::abs(v)));
                }
                for(std::size_t i=1,j=0;i<n;++i){std::size_t bit=n>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(fft[i],fft[j]);}
                for(std::size_t len=2;len<=n;len<<=1)
                {
                    const auto step=std::polar(1.0,-6.283185307179586/static_cast<double>(len));
                    for(std::size_t base=0;base<n;base+=len){std::complex<double> w=1;for(std::size_t j=0;j<len/2;++j){auto a=fft[base+j],b=fft[base+j+len/2]*w;fft[base+j]=a+b;fft[base+j+len/2]=a-b;w*=step;}}
                }
                for(std::size_t k=1;k<n/2;++k)power[k]+=std::norm(fft[k]);
                ++windows;
            }
        }
        double total=0,logTotal=0,arithmetic=0,weighted=0;int bins=0;
        std::array<double,12> weights {};
        for(std::size_t k=1;k<n/2;++k)
        {
            const double hz=k*sr/n,p=power[k];total+=p;weighted+=p*hz;
            if(hz>=200&&hz<=std::min(10000.0,sr*.45)){logTotal+=std::log(p+1e-20);arithmetic+=p;++bins;}
            for(std::size_t b=0;b<s.bands.size();++b)
            {
                const double w=std::max(0.0,1-std::abs(std::log2(hz/centres[b]))/.65);
                s.bands[b]+=static_cast<float>(p*w);weights[b]+=w;
            }
        }
        if(total<1e-15||windows==0)return s;
        float highest=0;for(std::size_t b=0;b<s.bands.size();++b){s.bands[b]/=static_cast<float>(std::max(weights[b],1e-12));highest=std::max(highest,s.bands[b]);}
        for(auto& v:s.bands)v=std::sqrt(v/std::max(highest,1e-20f));
        // Distinct upper resonances, rather than three adjacent FFT bins.
        auto remaining=power;
        for(auto& peak:s.peaks)
        {
            std::size_t best=1;double strongest=-1;
            for(std::size_t k=1;k<n/2;++k){double hz=k*sr/n;if(hz>=250&&hz<=std::min(8500.0,sr*.4)&&remaining[k]>strongest){strongest=remaining[k];best=k;}}
            peak=static_cast<float>(best*sr/n);
            for(std::size_t k=1;k<n/2;++k)if(std::abs(std::log2((k*sr/n)/peak))<.45)remaining[k]=0;
        }
        std::sort(s.peaks.begin(),s.peaks.end());
        std::size_t fundamental=1;double strongest=0;
        for(std::size_t k=1;k<n/2;++k)if(k*sr/n>=45&&k*sr/n<=600&&power[k]>strongest){fundamental=k;strongest=power[k];}
        double offset=0;
        if(fundamental>1&&fundamental+1<n/2)
        {
            const double left=std::log(power[fundamental-1]+1e-20),centre=std::log(power[fundamental]+1e-20),right=std::log(power[fundamental+1]+1e-20);
            const double denominator=left-2*centre+right;
            if(std::abs(denominator)>1e-12)offset=std::clamp(.5*(left-right)/denominator,-.5,.5);
        }
        const double fundamentalBin=fundamental+offset;
        double odd=0,even=0,harmonics=0;
        for(std::size_t h=1;h*fundamentalBin<n/2-1&&h<32;++h){const auto k=static_cast<std::size_t>(std::round(h*fundamentalBin));double p=power[k]+power[k-1]+(k+1<n/2?power[k+1]:0);harmonics+=p;if(h>1)(h%2?odd:even)+=p;}
        s.features={static_cast<float>(bins?std::clamp(std::exp(logTotal/bins)/(arithmetic/bins+1e-20),0.0,1.0):0),
                    static_cast<float>(std::clamp(harmonics/total,0.0,1.0)),
                    static_cast<float>(even+odd>1e-12?even/(even+odd):.5),
                    std::clamp((maximum/static_cast<float>(std::sqrt(energy/(windows*n)+1e-20))-1.4f)/6,0.0f,1.0f),
                    static_cast<float>(std::clamp(weighted/total/6000,0.0,1.0))};
        s.valid=true;s.prepareDerived();return s;
    }
};
