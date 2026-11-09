#include "PluginProcessor.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <thread>
#include <vector>
#if defined(_WIN32)
#include <malloc.h>
#endif
static thread_local bool callback = false;
static std::atomic<unsigned> allocations{0}, reclamations{0};
void *operator new(std::size_t n)
{
    if (callback)
        ++allocations;
    if (auto *p = std::malloc(std::max(std::size_t(1), n)))
        return p;
    throw std::bad_alloc();
}
void operator delete(void *p) noexcept
{
    if (p && callback)
        ++reclamations;
    std::free(p);
}
void *operator new[](std::size_t n)
{
    return ::operator new(n);
}
void operator delete[](void *p) noexcept
{
    ::operator delete(p);
}
void operator delete(void *p, std::size_t) noexcept
{
    ::operator delete(p);
}
void operator delete[](void *p, std::size_t) noexcept
{
    ::operator delete(p);
}
void *operator new(std::size_t n, std::align_val_t alignment)
{
    if (callback)
        ++allocations;
    void *p = nullptr;
#if defined(_WIN32)
    p = _aligned_malloc(n, std::size_t(alignment));
#else
    if (posix_memalign(&p, std::size_t(alignment), n) != 0)
        p = nullptr;
#endif
    if (!p)
        throw std::bad_alloc();
    return p;
}
void operator delete(void *p, std::align_val_t) noexcept
{
    if (p && callback)
        ++reclamations;
#if defined(_WIN32)
    _aligned_free(p);
#else
    std::free(p);
#endif
}
void operator delete(void *p, std::size_t, std::align_val_t a) noexcept
{
    ::operator delete(p, a);
}
void *operator new[](std::size_t n, std::align_val_t a)
{
    return ::operator new(n, a);
}
void operator delete[](void *p, std::align_val_t a) noexcept
{
    ::operator delete(p, a);
}
void operator delete[](void *p, std::size_t, std::align_val_t a) noexcept
{
    ::operator delete(p, a);
}
static void require(bool v, const char *m)
{
    if (!v)
        throw std::runtime_error(m);
}
static juce::MemoryBlock fixture(int kind)
{
    juce::AudioBuffer<float> b(1, 24000);
    for (int i = 0; i < 24000; ++i)
        b.setSample(0, i,
                    .4f * std::sin(i * (i < 12000 ? .025f : .14f) * (1 + kind * .4f)) +
                        .15f * std::sin(i * .27f));
    juce::MemoryBlock bytes;
    juce::WavAudioFormat f;
    {
        std::unique_ptr<juce::AudioFormatWriter> w(
            f.createWriterFor(new juce::MemoryOutputStream(bytes, false), 48000, 1, 24, {}, 0));
        require(w && w->writeFromAudioSampleBuffer(b, 0, 24000), "Fixture failed");
    }
    return bytes;
}
static void wait(WubrotProcessor &p)
{
    for (int i = 0; i < 10000 && p.isLoading(); ++i)
        juce::Thread::sleep(2);
    require(!p.isLoading(), "Worker timed out");
}
int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        WubrotProcessor p;
        p.prepareToPlay(48000, 128);
        p.setParameterValue("x", .6f);
        p.setParameterValue("y", -.5f);
        std::atomic<bool> stop{false}, bad{false};
        std::atomic<float> peak{0};
        std::atomic<int> blocks{0};
        std::thread audio([&] {
            juce::AudioBuffer<float> b(2, 128);
            juce::MidiBuffer midi;
            int frame = 0;
            float maximum = 0;
            while (!stop.load())
            {
                for (int i = 0; i < 128; ++i, ++frame)
                {
                    float v = .45f * std::sin(frame * .00719948f) + .1f * std::sin(frame * .0431969f);
                    b.setSample(0, i, v);
                    b.setSample(1, i, -v);
                }
                callback = true;
                p.processBlock(b, midi);
                callback = false;
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 128; ++i)
                    {
                        float v = b.getSample(ch, i);
                        if (!std::isfinite(v) || std::abs(v) > 1.8f)
                            bad.store(true);
                        maximum = std::max(maximum, std::abs(v));
                    }
                blocks.fetch_add(1);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            peak.store(maximum);
        });
        struct Guard
        {
            std::atomic<bool> &stop;
            std::thread &audio;
            ~Guard()
            {
                stop.store(true);
                if (audio.joinable())
                    audio.join();
            }
        } guard{stop, audio};
        for (int r = 0; r < 6; ++r)
        {
            p.loadSampleBytes(fixture(r % 2), "synthetic-replace.wav");
            wait(p);
            for (int i = 0; i < 20; ++i)
            {
                p.setParameterValue("x", std::sin(i * .4f) * .7f);
                p.setParameterValue("y", std::cos(i * .31f) * .7f);
                p.setParameterValue("mutate", i % 4 ? 100 : 0);
                p.setParameterValue("drive", float(i % 3) * 12);
                p.setParameterValue("dnaSpeed", float(i % 11)*.4f);p.setParameterValue("dnaPitch",float(i%49)-24);
                auto ui = p.uiState(false);
                require(!ui.hasProperty("waveform"), "Control telemetry serialized waveform");
                juce::Thread::sleep(2);
            }
            p.setParameterValue("regionStart", r % 2 ? .1f : .5f);
            p.setParameterValue("regionEnd", r % 2 ? .4f : 1.f);
            p.serviceRegionEdit();
            wait(p);
            if (r % 2)
                p.clearSample();
        }
        stop.store(true);
        audio.join();
        require(!bad.load(), "Replacement/automation produced instability");
        require(blocks.load() > 30, "No real concurrent callbacks");
        require(allocations.load() == 0 && reclamations.load() == 0,
                "Audio callback allocated or destroyed heap data");
        require(p.getLatencySamples() == 0, "Changed fixed latency");
        // Region/SPEED recall must include the actual frame-indexed spectrum, not just a summary.
        p.loadSampleBytes(fixture(0), "temporal-reference.wav");
        wait(p);
        p.setParameterValue("mutate", 100);
        p.setParameterValue("drive", 0);
        p.setParameterValue("dnaSpeed", 2);
        p.setParameterValue("x", .55f);
        p.setParameterValue("y", -.45f);
        auto render = [&](WubrotProcessor &fx) {
            fx.prepareToPlay(48000, 128);
            std::vector<float> out;
            for (int off = 0; off < 24000; off += 128)
            {
                int n = std::min(128, 24000 - off);
                juce::AudioBuffer<float> b(2, n);
                for (int i = 0; i < n; ++i)
                {
                    float v = .35f * std::sin((off + i) * .00719948f);
                    b.setSample(0, i, v);
                    b.setSample(1, i, v);
                }
                juce::MidiBuffer m;
                callback = true;
                fx.processBlock(b, m);
                callback = false;
                out.insert(out.end(), b.getReadPointer(0), b.getReadPointer(0) + n);
            }
            return out;
        };
        p.setParameterValue("regionStart", 0);
        p.setParameterValue("regionEnd", .45f);
        p.serviceRegionEdit();
        wait(p);
        auto first = render(p);
        p.setParameterValue("regionStart", .55f);
        p.setParameterValue("regionEnd", 1);
        p.serviceRegionEdit();
        wait(p);
        auto second = render(p);
        require(first != second, "Region does not change spectral movement");
        p.setParameterValue("regionStart", 0);
        p.setParameterValue("regionEnd", 1);
        p.serviceRegionEdit();
        wait(p);
        auto normal = render(p);
        p.setParameterValue("dnaSpeed", 4);
        auto quick = render(p);
        require(quick != normal, "Speed disconnected");
        juce::MemoryBlock state;
        p.getStateInformation(state);
        auto xml = juce::AudioProcessor::getXmlFromBinary(state.getData(), int(state.getSize()));
        require(xml->hasAttribute("dnaTemporal"), "Project omitted temporal descriptors");
        WubrotProcessor restored;
        restored.setStateInformation(state.getData(), int(state.getSize()));
        wait(restored);
        require(render(restored) == quick, "Temporal project recall changed audio");
        require(allocations.load() == 0 && reclamations.load() == 0, "Render callback heap activity");
        // The hold clock must not restart at callback boundaries or rate updates.
        auto partition = [](int size, int rate) {
            WubrotProcessor fx;
            fx.prepareToPlay(rate, size);
            std::vector<float> out;
            for (int offset = 0; offset < 12288; offset += size)
            {
                if (offset % 1536 == 0)
                {
                    fx.setParameterValue("x", offset % 3072 ? .6f : -.5f);
                    fx.setParameterValue("y", offset % 4608 ? .55f : -.45f);
                }
                int n = std::min(size, 12288 - offset);
                juce::AudioBuffer<float> b(2, n);
                for (int i = 0; i < n; ++i)
                {
                    float v = .4f * std::sin(float(6.28318530718 * 110 * (offset + i) / rate));
                    b.setSample(0, i, v);
                    b.setSample(1, i, v);
                }
                juce::MidiBuffer m;
                callback = true;
                fx.processBlock(b, m);
                callback = false;
                out.insert(out.end(), b.getReadPointer(0), b.getReadPointer(0) + n);
            }
            return out;
        };
        for (int rate : {44100, 48000, 96000, 192000})
            require(partition(64, rate) == partition(512, rate),
                    "Hold phase depends on block partition/rate-update boundary");
        std::cout << "PASS: " << blocks
                  << " concurrent callbacks, zero C++ heap allocations/reclamations in processBlock, atomic "
                     "replacement/automation, region/SPEED and temporal recall; peak "
                  << peak << '\n';
    }
    catch (const std::exception &e)
    {
        callback = false;
        std::cerr << e.what() << '\n';
        return 1;
    }
}
