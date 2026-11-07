#pragma once
#include "DnaSpectrum.h"

// Fixed-size, level-independent spectral shape, prepared off-thread. No PCM.
struct DnaTemporalFrame
{
    std::array<float, 12> bands{};
    std::array<float, 5> features{.2f, .5f, .5f, .3f, .3f};
    std::array<float, 3> logPeaks{6.2146081f, 7.2442275f, 7.9373746f};
    std::array<float, 3> resonance{2.2f, 2.2f, 2.2f};
    std::array<float, 3> persistence{};
    static DnaTemporalFrame from(const DnaSpectrum &s) noexcept
    {
        DnaTemporalFrame f;
        f.bands = s.bands;
        f.features = s.features;
        for (std::size_t i = 0; i < 3; ++i)
        {
            f.logPeaks[i] = std::log(std::clamp(s.peaks[i], 100.f, 10000.f));
            f.resonance[i] = s.resonances[i];
        }
        return f;
    }
    DnaSpectrum toSpectrum() const noexcept
    {
        DnaSpectrum s;
        s.bands = bands;
        s.features = features;
        s.resonances = resonance;
        for (std::size_t i = 0; i < 3; ++i)
            s.peaks[i] = std::exp(logPeaks[i]);
        s.valid = true;
        s.prepareDerived();
        return s;
    }
    static DnaTemporalFrame interpolate(const DnaTemporalFrame &a, const DnaTemporalFrame &b,
                                        float t) noexcept
    {
        DnaTemporalFrame f;
        const auto lerp = [t](float x, float y) { return x + t * (y - x); };
        for (std::size_t i = 0; i < 12; ++i)
            f.bands[i] = lerp(a.bands[i], b.bands[i]);
        for (std::size_t i = 0; i < 5; ++i)
            f.features[i] = lerp(a.features[i], b.features[i]);
        for (std::size_t i = 0; i < 3; ++i)
        {
            f.logPeaks[i] = lerp(a.logPeaks[i], b.logPeaks[i]);
            f.resonance[i] = lerp(a.resonance[i], b.resonance[i]);
            f.persistence[i] = lerp(a.persistence[i], b.persistence[i]);
        }
        return f;
    }
};
