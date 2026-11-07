#pragma once
#include "DnaSpectrum.h"
#include "DnaTemporal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

// Immutable modulation data, never a sample-playback buffer.
struct DnaProfile
{
    static constexpr std::size_t bins = 2048;
    std::array<float, bins> envelope {};
    std::array<float, bins> character = [] { std::array<float, bins> a {}; a.fill(.5f); return a; }();
    DnaSpectrum spectrum;
    static constexpr std::size_t spectralCapacity=128;
    std::array<DnaTemporalFrame,spectralCapacity> spectralFrames{};
    std::size_t spectralCount=0;
    DnaTemporalFrame spectralAt(float position)const noexcept
    {
        if(spectralCount==0)return DnaTemporalFrame::from(spectrum);
        const float index=std::clamp(position,0.f,1.f)*float(spectralCount-1);
        const auto i=std::min(std::size_t(index),spectralCount-1),j=std::min(i+1,spectralCount-1);
        return DnaTemporalFrame::interpolate(spectralFrames[i],spectralFrames[j],index-float(i));
    }
    double duration = 0;
    bool valid = false;
    float at(double seconds) const noexcept
    {
        return lookup(envelope, seconds);
    }
    float characterAt(double seconds) const noexcept { return lookup(character, seconds); }
    float lookup(const std::array<float, bins>& values, double seconds) const noexcept
    {
        if (!valid || duration <= 0) return 0;
        const double wrapped = seconds / duration - std::floor(seconds / duration);
        const double position = wrapped * static_cast<double>(bins);
        const auto i = static_cast<std::size_t>(position) % bins;
        const float fraction = static_cast<float>(position - std::floor(position));
        return values[i] + fraction * (values[(i + 1) % bins] - values[i]);
    }
    void analyseTemporal(const float *const *channels, int channelCount, std::size_t frames, double sampleRate)
    {
        auto &p = *this;

        p.spectralCount = spectralCapacity;
        for (std::size_t f = 0; f < spectralCapacity; ++f)
        {
            const auto width = std::min(frames, std::max(std::size_t(1), std::size_t(sampleRate * .06))),
                       centre = std::min(frames - 1, (2 * f + 1) * frames / (2 * spectralCapacity)),
                       first = std::min(frames - width, centre > width / 2 ? centre - width / 2 : std::size_t(0)),
                       last = first + width;
            std::array<const float *, 8> region{};
            for (int ch = 0; ch < channelCount; ++ch)
                region[std::size_t(ch)] = channels[ch] + std::min(first, frames - 1);
            const auto shape = DnaSpectrum::analyse(region.data(), channelCount,
                                                    std::min(frames - first, last - first), sampleRate);
            p.spectralFrames[f] = DnaTemporalFrame::from(shape.valid ? shape : p.spectrum);
        }
        // Confidence of a resonance recurring across the reference, not its loudness.
        for (auto &f : p.spectralFrames)
            for (std::size_t b = 0; b < 3; ++b)
            {
                float matching = 0;
                for (const auto &other : p.spectralFrames)
                {
                    float nearest = 10;
                    for (float hz : other.logPeaks)
                        nearest = std::min(nearest, std::abs(hz - f.logPeaks[b]));
                    matching += nearest < .18f ? 1.f : 0.f;
                }
                f.persistence[b] = matching / float(spectralCapacity);
            }
    }
    static DnaProfile analyse(const float* const* channels, int channelCount, std::size_t frames, double sampleRate,bool temporal=false)
    {
        DnaProfile p;
        if (sampleRate < 8000 || sampleRate > 384000 || channelCount < 1 || channelCount > 8 || frames == 0) return p;
        frames = std::min(frames, static_cast<std::size_t>(sampleRate * 30));
        p.duration = static_cast<double>(frames) / sampleRate;
        float maximum = 0;
        for (std::size_t bin = 0; bin < bins; ++bin)
        {
            const auto start = std::min(frames - 1, bin * frames / bins);
            const auto end = std::min(frames, std::max(start + 1, (bin + 1) * frames / bins));
            double squares = 0, difference = 0, signedEnergy = 0; float peak = 0;
            for (auto i = start; i < end; ++i)
            {
                float magnitude = 0;
                for (int ch = 0; ch < channelCount; ++ch)
                {
                    const auto sample = channels[ch][i];
                    if (std::isfinite(sample)) magnitude = std::max(magnitude, std::min(8.0f, std::abs(sample)));
                }
                for (int ch = 0; ch < channelCount; ++ch)
                {
                    const float v = std::isfinite(channels[ch][i]) ? channels[ch][i] : 0;
                    const float prev = i > 0 && std::isfinite(channels[ch][i - 1]) ? channels[ch][i - 1] : 0;
                    difference += static_cast<double>(v - prev) * (v - prev);
                    signedEnergy += static_cast<double>(v) * v;
                }
                squares += static_cast<double>(magnitude) * magnitude; peak = std::max(peak, magnitude);
            }
            p.envelope[bin] = .65f * peak + .35f * static_cast<float>(std::sqrt(squares / static_cast<double>(end - start)));
            // Derivative energy is a level-independent brightness descriptor, scaled
            // to 48kHz so the same WAV character survives sample-rate conversion.
            p.character[bin] = std::clamp(static_cast<float>(std::sqrt(difference / (signedEnergy + 1e-20)) * sampleRate / 48000 * 3), 0.0f, 1.0f);
            maximum = std::max(maximum, p.envelope[bin]);
        }
        if (maximum < 1.0e-7f) return p;
        for (auto& v : p.envelope) v /= maximum;
        p.spectrum = DnaSpectrum::analyse(channels, channelCount, frames, sampleRate);
        if(temporal)p.analyseTemporal(channels,channelCount,frames,sampleRate);
        p.valid = true; return p;
    }
};
