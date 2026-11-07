#include "AudioReactivity.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

static void require(bool condition, const char* text) { if (!condition) throw std::runtime_error(text); }
static std::vector<float> response(double rate, float frequency, float level, bool sustain=false, bool opposite=false)
{
    AudioReactivity detector; detector.prepare(rate);
    std::vector<float> values;
    for (int i=0;i<static_cast<int>(rate*.8);++i)
    {
        const double t=i/rate;
        const float envelope=t<.01?static_cast<float>(t/.01):(sustain?1.0f:static_cast<float>(std::exp(-(t-.01)/.035)));
        const float v=level*envelope*static_cast<float>(std::sin(t*6.283185307179586*frequency));
        const float frame[]{v,opposite?-v:v}; detector.observe(frame,2); values.push_back(detector.level());
    }
    return values;
}
static float peak(const std::vector<float>& v) { return *std::max_element(v.begin(),v.end()); }
int main(int argc,char** argv)
{
    try
    {
        for(const double rate:{44100.0,48000.0,96000.0,192000.0})
        {
            AudioReactivity silence;silence.prepare(rate);
            const float zero[2]{};for(int i=0;i<static_cast<int>(rate);++i)silence.observe(zero,2);
            require(silence.level()==0&&silence.takePeak()==0,"Silence generated visual activity");
            const auto kick=response(rate,65,.8f),quiet=response(rate,65,.008f),body=response(rate,160,.6f),treble=response(rate,5000,.8f);
            const auto inverse=response(rate,65,.8f,false,true);
            const auto mid=response(rate,1200,.6f),growl=response(rate,400,.6f);
            require(peak(mid)>.35f&&peak(growl)>.6f,"Midrange/body-rich bass did not react");
            require(peak(kick)>.7f&&peak(quiet)>.6f&&peak(body)>.5f,"Low-end onset response too weak");
            require(std::abs(peak(kick)-peak(quiet))<.15f,"Response is too input-level dependent");
            require(peak(treble)<.03f,"Treble leakage normalized into a false bass flash");
            require(kick==inverse,"Stereo phase cancellation changed the visual response");
            require(kick[static_cast<std::size_t>(rate*.3)]<.04f,"Transient did not settle promptly");
            for(const float frequency:{35,65,120,180,400,1200})
            {
                const auto held=response(rate,frequency,.8f,true);
                const auto tail=held.begin()+static_cast<int>(rate*.4);
                require(*std::max_element(tail,held.end())<.14f,"Sustained bass repeatedly triggered/maximized visual pulses");
                require(*std::min_element(tail,held.end())>.01f,"Sustained bass lost its gentle continuous pressure");
            }
            for(float v:kick)require(std::isfinite(v)&&v>=0&&v<=1,"Unbounded transient intensity");
            std::cout<<rate<<" Hz: low hit "<<peak(kick)<<", quiet "<<peak(quiet)<<", body "<<peak(body)<<", mid "<<peak(mid)<<", treble "<<peak(treble)<<", sustained/release/stereo passed\n";
        }
        AudioReactivityMailbox mailbox;
        const auto initial=mailbox.sequence();mailbox.publish(.2f,.9f);mailbox.publish(.1f,.1f);
        require(mailbox.sequence()!=initial&&mailbox.consume()==.9f,"Impact lost between UI polls");
        require(mailbox.consume()==.1f,"Mailbox replayed a consumed peak");
        mailbox.publish(.7f,.8f);mailbox.clear();require(mailbox.consume()==0,"Reset retained stale visual energy");
        AudioReactivity finite;finite.prepare(48000);const float invalid[]{NAN,INFINITY};finite.observe(invalid,2);
        require(finite.level()==0,"Nonfinite input poisoned the visual observer");
        // Four independently articulated notes, with only upper bass/mid energy.
        AudioReactivity rhythm;rhythm.prepare(48000);std::array<float,4> hits {},valleys {1,1,1,1};
        for(int i=0;i<48000;++i)
        {
            const auto note=i/12000;const float t=(i%12000)/48000.0f;
            const float envelope=std::min(1.0f,t/.004f)*std::exp(-t/.018f);
            const float v=.6f*envelope*(.7f*std::sin(i*.15707963f)+.3f*std::sin(i*.10471976f));const float frame[]{v,-v};
            rhythm.observe(frame,2);if(t<.08f)hits[static_cast<std::size_t>(note)]=std::max(hits[static_cast<std::size_t>(note)],rhythm.level());
            if(t>.20f)valleys[static_cast<std::size_t>(note)]=std::min(valleys[static_cast<std::size_t>(note)],rhythm.level());
        }
        for(std::size_t i=0;i<hits.size();++i)require(hits[i]>.4f&&valleys[i]<hits[i]*.2f,"Mid-heavy rhythmic notes did not produce distinct corresponding pulses");
        std::cout<<"Four mid-heavy rhythmic impacts and intervening decay passed\n";
        if(argc>1)
        {
            // Synthetic kick only: native detector telemetry for the browser replay.
            const auto kick=response(48000,65,.8f);
            std::ofstream out(argv[1]);out<<"{\"stepMs\":33.333333,\"intensities\":[0,0,0";
            for(std::size_t i=0;i<kick.size();i+=1600)out<<','<<*std::max_element(kick.begin()+static_cast<std::ptrdiff_t>(i),kick.begin()+static_cast<std::ptrdiff_t>(std::min(kick.size(),i+1600)));
            out<<",0,0,0,0]}\n";require(static_cast<bool>(out),"Cannot write synthetic visual trace");
        }
        AudioReactivity bench;bench.prepare(48000);const float frame[]{.3f,-.3f};
        const auto start=std::chrono::steady_clock::now();
        for(int i=0;i<480000;++i)bench.observe(frame,2);
        std::cout<<"Observer: "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/10<<" ms per second of stereo audio; final "<<bench.level()<<'\n';
    }
    catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
