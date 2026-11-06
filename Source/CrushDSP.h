#pragma once
#include "DnaProfile.h"
#include "TonalDSP.h"
#include <algorithm>
#include <array>
#include <cmath>

class CrushDSP
{
public:
    struct Parameters
    {
        float drive = 12, bits = 8, rate = 12000, mix = .65f, output = -6, protect = 1;
        float mutate = 0;
        bool enabled = true;
        int mode = 0; // LOOP / RETRIGGER
    };
    void prepare(double rate)
    {
        sampleRate = std::max(8000.0, rate);
        smoothCoefficient = 1 - std::exp(-1.0f / (.02f * static_cast<float>(sampleRate)));
        modulationCoefficient = 1 - std::exp(-1.0f / (.003f * static_cast<float>(sampleRate)));
        lowCoefficient = 1 - std::exp(-2 * pi * 120 / static_cast<float>(sampleRate));
        dcCoefficient = std::exp(-2 * pi * 15 / static_cast<float>(sampleRate));
        detectorRelease = std::exp(-1.0f / (.04f * static_cast<float>(sampleRate)));
        tonal.prepare(sampleRate); reset();
    }
    void reset()
    {
        low.fill(0); held.fill(0); dcInput.fill(0); dcOutput.fill(0); previousDrive.fill(0); colourLow.fill(0); body.fill(0);
        tonal.reset(); character = .5f;
        bypassWet = 1; phase = 1; primed = false; loopTime = 0; resetModulation();
    }
    void resetModulation() noexcept { retriggerTime = 0; modulation = 0; detector = 0; refractory = 0; retriggerArmed = false; }
    void setTimeline(double seconds) noexcept { loopTime = std::max(0.0, seconds); }
    void noteOn() noexcept { retriggerTime = 0; retriggerArmed = true; refractory = static_cast<int>(sampleRate * .05); }
    double getRetriggerTime() const noexcept { return retriggerTime; }
    void processFrame(float* frame, int channels, const Parameters& target, const DnaProfile* dna = nullptr) noexcept
    {
        const std::array<float, 2> original { frame[0], channels == 2 ? frame[1] : 0 };
        if (!primed) { current = target; bypassWet = target.enabled ? 1.0f : 0.0f; primed = true; }
        const float goal = target.enabled ? 1.0f : 0.0f;
        const float step = 1.0f / static_cast<float>(sampleRate * .005);
        bypassWet += std::clamp(goal - bypassWet, -step, step);
        smooth(current.drive, std::clamp(target.drive, 0.0f, 36.0f));
        smooth(current.bits, std::clamp(target.bits, 2.0f, 16.0f));
        smooth(current.rate, std::clamp(target.rate, 500.0f, static_cast<float>(sampleRate)));
        smooth(current.mix, std::clamp(target.mix, 0.0f, 1.0f));
        smooth(current.output, std::clamp(target.output, -24.0f, 6.0f));
        smooth(current.protect, std::clamp(target.protect, 0.0f, 1.0f));
        smooth(current.mutate, std::clamp(target.mutate, 0.0f, 1.0f));
        if (current.mode != target.mode) { current.mode = target.mode; resetModulation(); }
        float peak = 0;
        for (std::size_t ch = 0; ch < static_cast<std::size_t>(std::max(0, std::min(channels, 2))); ++ch)
        {
            if (!std::isfinite(frame[ch])) frame[ch] = 0;
            peak = std::max(peak, std::abs(frame[ch]));
        }
        if (refractory > 0) --refractory;
        if (current.mode == 1 && refractory == 0 && peak > .015f && peak > detector * 1.8f + .008f) noteOn();
        detector = std::max(peak, detector * detectorRelease);
        const bool running = dna != nullptr && (current.mode == 0 || (retriggerArmed && retriggerTime < dna->duration));
        const float env = running ? dna->at(current.mode == 0 ? loopTime : retriggerTime) : 0;
        modulation += modulationCoefficient * (env - modulation);
        const double dnaTime = current.mode == 0 ? loopTime : retriggerTime;
        character += modulationCoefficient * ((running ? dna->characterAt(dnaTime) : .5f) - character);
        const float dnaAmount = running ? current.mutate : 0;
        const float influence = current.mutate * modulation;
        const float bits = std::clamp(current.bits - 3 * influence, 2.0f, 16.0f);
        const float rate = std::clamp(current.rate * std::exp2(-1.5f * influence), 500.0f, static_cast<float>(sampleRate));
        const float gain = std::pow(10.0f, current.drive / 20), driveBlend = std::min(1.0f, current.drive / 6);
        const float output = std::pow(10.0f, current.output / 20), steps = std::exp2(bits - 1);
        // XY maps digital destruction into a broad-to-focused spectral emphasis.
        // All motion changes processing coefficients; no oscillator or sample is mixed into audio.
        const float crush = (16 - bits) / 14;
        const float reduction = 1 - std::clamp(rate / static_cast<float>(sampleRate), 0.0f, 1.0f);
        const float movement = std::sin(static_cast<float>(std::fmod(loopTime, 1000.0)) * (1.1f + reduction * 3));
        const float focusHz = 700 + 5300 * (1 - crush) + 250 * movement * crush;
        const float colourCoefficient = 1 - std::exp(-2 * pi * focusHz / static_cast<float>(sampleRate));
        const float bodyCoefficient = 1 - std::exp(-2 * pi * 360 / static_cast<float>(sampleRate));
        // Shape only existing stereo difference: silence and mono remain unchanged.
        const float side = channels == 2 ? .5f * (frame[0] - frame[1]) : 0;
        const float spread = driveBlend * (.6f * crush - .4f * reduction);
        const bool capture = phase >= 1;
        if (capture) phase -= std::floor(phase);
        std::array<float, 2> dryFrame { frame[0], channels == 2 ? frame[1] : 0 };
        for (std::size_t ch = 0; ch < static_cast<std::size_t>(std::max(0, std::min(channels, 2))); ++ch)
        {
            const float dry = frame[ch];
            low[ch] += lowCoefficient * (dry - low[ch]);
            body[ch] += bodyCoefficient * (dry - body[ch]);
            colourLow[ch] += colourCoefficient * (dry - colourLow[ch]);
            const float upper = dry - colourLow[ch];
            const float mid = colourLow[ch] - body[ch];
            // Protect the fundamental while driving its upper harmonics and the midrange harder.
            const float source = dry - current.protect * low[ch];
            const float emphasis = source + (ch == 0 ? side : -side) * spread + driveBlend * (mid * (1.4f * crush + .5f * reduction)
                                                        + upper * (.8f * reduction) + .25f * body[ch]);
            const float drivenInput = emphasis * gain * (1 + crush * .7f);
            const float delta = drivenInput - previousDrive[ch];
            const float driven = std::abs(delta) > .001f ? (primitive(drivenInput) - primitive(previousDrive[ch])) / delta
                                                        : softClip((drivenInput + previousDrive[ch]) * .5f);
            previousDrive[ch] = drivenInput;
            const float shaped = source + driveBlend * (driven - source);
            if (capture) held[ch] = std::round(shaped * steps) / steps;
            const float dc = held[ch] - dcInput[ch] + dcCoefficient * dcOutput[ch];
            dcInput[ch] = held[ch]; dcOutput[ch] = dc;
            frame[ch] = dc;
        }
        const float clock = static_cast<float>(std::fmod(current.mode == 1 ? retriggerTime : loopTime, 1000.0));
        const float sweep = .5f + .5f * std::sin(clock * (4 + 10 * crush) + reduction * 3);
        const float vowel = std::clamp(.15f + .65f * sweep + .15f * crush + dnaAmount * (.8f * character + .7f * modulation - .65f), 0.0f, 1.0f);
        const float carrierHz = 60 + 280 * crush + 110 * reduction + 35 * sweep + dnaAmount * (600 * character + 160 * modulation);
        const float intensity = std::clamp(current.drive / 24, 0.0f, 1.0f);
        tonal.process(frame, channels, carrierHz, intensity * (.35f + .4f * crush + .2f * dnaAmount),
                      vowel, 4 + 3 * dnaAmount * character, intensity * (.65f + .3f * dnaAmount),
                      intensity * (.3f + .5f * crush + .2f * dnaAmount * character));
        for (int ch = 0; ch < std::min(channels, 2); ++ch)
        {
            const auto c = static_cast<std::size_t>(ch);
            const float wet = softClip(frame[ch]) + current.protect * low[c];
            const float processed = (dryFrame[c] + current.mix * (wet - dryFrame[c])) * output;
            frame[ch] = bypassWet <= 0 ? original[c] : bypassWet >= 1 ? processed
                                    : original[c] + bypassWet * (processed - original[c]);
        }
        phase += static_cast<double>(rate) / sampleRate;
        loopTime += 1 / sampleRate; retriggerTime += 1 / sampleRate;
    }
private:
    // Antiderivative antialiasing on the drive stage; crusher aliasing is intentional.
    static float softClip(float x) noexcept { x = std::clamp(x, -1.5f, 1.5f); return x - (4.0f / 27) * x * x * x; }
    static float primitive(float x) noexcept { const float a = std::abs(x); return a >= 1.5f ? a - .5625f : .5f * x * x - x * x * x * x / 27; }
    void smooth(float& v, float target) const noexcept { v += smoothCoefficient * (target - v); }
    static constexpr float pi = 3.14159265359f;
    double sampleRate = 44100, phase = 1, loopTime = 0, retriggerTime = 0;
    float smoothCoefficient = .001f, modulationCoefficient = .01f, lowCoefficient = .01f;
    float dcCoefficient = .995f, detectorRelease = .999f, detector = 0, modulation = 0;
    int refractory = 0;
    bool primed = false, retriggerArmed = false;
    Parameters current;
    TonalDSP tonal;
    float character = .5f, bypassWet = 1;
    std::array<float, 2> low {}, held {}, dcInput {}, dcOutput {}, previousDrive {}, colourLow {}, body {};
};
