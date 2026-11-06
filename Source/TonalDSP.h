#pragma once
#include <algorithm>
#include <array>
#include <cmath>

// Signal multiplier plus two parallel, topology-preserving state-variable formants.
// The carrier is never summed into the output: silent input stays silent.
class TonalDSP
{
public:
    void prepare(double sr) noexcept { sampleRate = sr; coefficient = 1 - std::exp(-1.0f / (.003f * static_cast<float>(sr))); reset(); }
    void reset() noexcept { phase = highPhase = 0; states = {}; primed = false; }
    void process(float* frame, int channels, float frequency, float depth, float vowel,
                 float resonance, float amount, float texture = 0) noexcept
    {
        if (!primed) { controls = { frequency, depth, vowel, resonance, amount, texture }; primed = true; }
        const std::array<float, 6> target { frequency, depth, vowel, resonance, amount, texture };
        for (std::size_t i = 0; i < controls.size(); ++i) controls[i] += coefficient * (target[i] - controls[i]);
        frequency = controls[0]; depth = controls[1]; vowel = controls[2]; resonance = controls[3]; amount = controls[4]; texture = controls[5];
        constexpr double pi = 3.14159265358979323846;
        const float carrier = static_cast<float>(std::sin(phase));
        phase += 2 * pi * std::clamp(frequency, 30.0f, 1600.0f) / sampleRate;
        if (phase >= 2 * pi) phase -= 2 * pi;
        // A second, inharmonic carrier translates subdued input into upper-mid
        // texture; it carries input dynamics and cannot sound on its own.
        const float upperCarrier = static_cast<float>(std::sin(highPhase) + .25 * std::sin(2 * highPhase));
        highPhase += 2 * pi * std::min(sampleRate * .2, 700.0 + frequency * 2.4) / sampleRate;
        if (highPhase >= 2 * pi) highPhase -= 2 * pi;
        vowel = std::clamp(vowel, 0.0f, 1.0f);
        // /oo/ -> /ah/ -> /ee/: the second peak reverses direction around /ah/.
        const float f1 = vowel < .5f ? 330 + 1100 * vowel : 880 - 1000 * (vowel - .5f);
        const float f2 = vowel < .5f ? 850 + 900 * vowel : 1300 + 2200 * (vowel - .5f);
        const float k = 1 / std::clamp(resonance, 2.0f, 9.0f);
        const std::array<float, 2> g { static_cast<float>(std::tan(pi * f1 / sampleRate)),
                                      static_cast<float>(std::tan(pi * f2 / sampleRate)) };
        for (int ch = 0; ch < std::min(channels, 2); ++ch)
        {
            const float x = frame[ch] * (1 - depth + depth * carrier + .32f * texture * upperCarrier);
            float bands = 0;
            for (std::size_t b = 0; b < 2; ++b)
            {
                auto& s = states[static_cast<std::size_t>(ch)][b];
                const float a = 1 / (1 + g[b] * (g[b] + k));
                const float v1 = a * (s[0] + g[b] * (x - s[1]));
                const float v2 = s[1] + g[b] * v1;
                s[0] = 2 * v1 - s[0]; s[1] = 2 * v2 - s[1];
                bands += k * v1; // unity peak normalisation avoids Q-dependent spikes
            }
            frame[ch] = x * (1 - .55f * amount) + bands * (2.2f * amount);
        }
    }
private:
    bool primed = false;
    float coefficient = .01f;
    std::array<float, 6> controls {};
    double sampleRate = 48000, phase = 0, highPhase = 0;
    std::array<std::array<std::array<float, 2>, 2>, 2> states {};
};
