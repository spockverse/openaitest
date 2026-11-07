#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

// Read-only input observer. No audio generation, FFTs, allocation, locks, or DSP
// parameter changes. Three low/mid bands are filtered per stereo channel (no
// phase cancellation). There is no master-bus, sidechain or transport source.
class AudioReactivity
{
public:
    void prepare(double rate) noexcept
    {
        const auto sr = std::max(8000.0, rate);
        auto pole = [sr](double hz) { return static_cast<float>(1.0 - std::exp(-6.283185307179586 * hz / sr)); };
        auto envelope = [sr](double seconds) { return static_cast<float>(1.0 - std::exp(-1.0 / (sr * seconds))); };
        highPass = {pole(30),pole(180),pole(700)};
        lowPass = {pole(180),pole(700),pole(2500)};
        attack = envelope(.002); release = envelope(.022);
        backgroundAttack = envelope(.055); backgroundRelease = envelope(.15);
        riseCoefficient = envelope(.018);
        pulseDecay = 1.0f - envelope(.055);
        guardSamples = static_cast<int>(sr * .065);
        attackSamples = static_cast<int>(sr * .012);
        reset();
    }
    void reset() noexcept
    {
        channels = {}; fast = {}; slow = {}; shortBand = {}; wide = pulse = peak = value = 0;
        guard = attackWindow = 0; armed = true;
    }
    void observe(const float* input, int count) noexcept
    {
        std::array<float,3> bandPeak {}; float raw = 0;
        for (int ch = 0; ch < std::min(2, count); ++ch)
        {
            auto& s = channels[static_cast<std::size_t>(ch)];
            const float v = std::isfinite(input[ch]) ? std::clamp(input[ch], -8.0f, 8.0f) : 0;
            raw = std::max(raw, std::abs(v));
            for(std::size_t b=0;b<3;++b)
            {
                auto& band=s[b];
                band.dc += highPass[b] * (v - band.dc);
                band.low1 += lowPass[b] * ((v - band.dc) - band.low1);
                band.low2 += lowPass[b] * (band.low1 - band.low2);
                bandPeak[b] = std::max(bandPeak[b], std::abs(band.low2));
            }
        }
        wide += (raw > wide ? attack : release) * (raw - wide);
        constexpr std::array<float,3> weights {.50f,.35f,.20f};
        float energy=0,novel=0,flux=0;
        for(std::size_t b=0;b<3;++b)
        {
            fast[b] += (bandPeak[b] > fast[b] ? attack : release) * (bandPeak[b] - fast[b]);
            slow[b] += (fast[b] > slow[b] ? backgroundAttack : backgroundRelease) * (fast[b] - slow[b]);
            energy += weights[b]*fast[b];
            shortBand[b] += riseCoefficient*(fast[b]-shortBand[b]);
            flux += weights[b]*std::max(0.0f,fast[b]-shortBand[b]);
            // Per-band novelty also catches new growl/vowel articulation over a
            // held fundamental, without a free-running beat or transport clock.
            novel += weights[b]*std::max(0.0f,fast[b]-slow[b]*1.35f);
        }
        constexpr float floor = .00003f;
        const float focus = std::clamp((energy / (wide + floor) - .035f) / .30f, 0.0f, 1.0f);
        const float novelty=novel/(energy+floor);
        const float rise=flux/(energy+floor);
        if (guard > 0) --guard;
        if ((novelty < .07f && rise < .045f) || energy < floor) armed = true;
        // Compressed notes can attack below the long-term background level.
        // Short-term positive flux catches those rises without retriggering on
        // steady low-frequency cycles (covered down to 35 Hz in tests).
        if (armed && guard == 0 && energy > floor && (novelty > .24f || rise > .13f) && focus > .12f)
        {
            armed = false; guard = guardSamples; attackWindow = attackSamples;
        }
        pulse *= pulseDecay;
        if (attackWindow > 0)
        {
            --attackWindow;
            const auto contrast = std::clamp(std::max(novelty*1.4f,(rise-.08f)*3.8f), 0.0f, 1.0f);
            pulse = std::max(pulse, contrast * focus);
        }
        if (pulse < .0001f) pulse = 0;
        // A held bass retains a restrained continuous pressure (at most 10%),
        // while attacks dominate briefly. Silence still reaches exact zero.
        const float held=energy>floor&&focus>.12f?.10f*focus*energy/(energy+.0005f):0;
        value=std::clamp(pulse+held*(1-pulse),0.0f,1.0f);
        peak = std::max(peak, value);
    }
    float level() const noexcept { return value; }
    float takePeak() noexcept { const auto p = peak; peak = 0; return p; }
private:
    struct Channel { float dc=0, low1=0, low2=0; };
    std::array<std::array<Channel,3>, 2> channels {};
    std::array<float,3> highPass {},lowPass {},fast {},slow {},shortBand {};
    float attack=0, release=0, backgroundAttack=0, backgroundRelease=0, pulseDecay=0,riseCoefficient=0;
    float wide=0, pulse=0, peak=0, value=0;
    int guardSamples=0, attackSamples=0, guard=0, attackWindow=0;
    bool armed=true;
};

// One producer (audio), one consumer (editor). One bounded CAS retains impacts
// between UI polls. Sequence changes distinguish fresh audio from a stopped host.
class AudioReactivityMailbox
{
public:
    static_assert(std::atomic<float>::is_always_lock_free && std::atomic<std::uint32_t>::is_always_lock_free,
                  "Visual telemetry must be lock-free");
    void publish(float level, float peak) noexcept
    {
        current.store(level, std::memory_order_relaxed);
        auto old = pending.load(std::memory_order_relaxed);
        if (peak > old) pending.compare_exchange_strong(old, peak, std::memory_order_relaxed);
        serial.fetch_add(1, std::memory_order_release);
    }
    std::uint32_t sequence() const noexcept { return serial.load(std::memory_order_acquire); }
    float consume() noexcept { return std::max(current.load(std::memory_order_relaxed), pending.exchange(0, std::memory_order_relaxed)); }
    void clear() noexcept { current.store(0,std::memory_order_relaxed);pending.store(0,std::memory_order_relaxed);serial.fetch_add(1,std::memory_order_release); }
private:
    std::atomic<float> current {0}, pending {0};
    std::atomic<std::uint32_t> serial {0};
};
