#pragma once
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
    static DnaProfile analyse(const float* const* channels, int channelCount, std::size_t frames, double sampleRate)
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
        p.valid = true; return p;
    }
};
