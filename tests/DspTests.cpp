#include "CrushDSP.h"
#include "DnaExchange.h"
#include <vector>
#include <thread>
#include <atomic>
#include <cstdlib>
#include <iostream>

void check(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void checkDna()
{
    std::vector<float> source(48000), inverted(48000);
    for (int i = 0; i < 48000; ++i) { source[static_cast<std::size_t>(i)] = (i % 12000 < 1000 ? .8f : .03f) * std::sin(i * .12f); inverted[static_cast<std::size_t>(i)] = -source[static_cast<std::size_t>(i)]; }
    const float* channels[] { source.data(), inverted.data() };
    auto profile = DnaProfile::analyse(channels, 2, source.size(), 48000);
    check(profile.valid, "opposite-polarity stereo DNA must not cancel during analysis");
    check(std::abs(profile.at(.05) - profile.at(1.05)) < .00001f, "DNA must loop at its analysed duration");
    std::vector<float> silent(48000); const float* silentChannels[] { silent.data() };
    check(!DnaProfile::analyse(silentChannels, 1, silent.size(), 48000).valid, "silent DNA must be rejected");
    auto run = [&](float mutate, const DnaProfile* dna, bool silence = false) {
        CrushDSP dsp; dsp.prepare(48000); CrushDSP::Parameters p;
        p.mutate = mutate; p.bits = 10; p.rate = 12000; p.mix = 1; p.protect = 0;
        std::vector<float> result;
        for (int i = 0; i < 48000; ++i) { float frame[] { silence ? 0.0f : .3f * std::sin(i * .071f), 0 }; dsp.processFrame(frame, 1, p, dna); result.push_back(frame[0]); }
        return result;
    };
    check(run(0, &profile) == run(0, nullptr), "MUTATE zero must remove all DNA influence");
    const auto modulated = run(1, &profile), plain = run(1, nullptr);
    double difference = 0; for (std::size_t i = 0; i < plain.size(); ++i) difference += std::abs(modulated[i] - plain[i]);
    check(difference > 100, "sample envelope must change actual processed audio");
    const auto noLayer = run(1, &profile, true);
    for (float value : noLayer) check(value == 0, "sample must never be layered into silence");
    CrushDSP retrigger; retrigger.prepare(48000); CrushDSP::Parameters p; p.mode = 1;
    for (int i = 0; i < 4000; ++i) { float frame[] { 0, 0 }; retrigger.processFrame(frame, 2, p, &profile); }
    retrigger.noteOn(); check(retrigger.getRetriggerTime() == 0, "MIDI note-on must reset DNA phase");
    float transient[] { .6f, .6f }; retrigger.reset(); retrigger.processFrame(transient, 2, p, &profile);
    check(retrigger.getRetriggerTime() < .0001, "bass transient must restart DNA");
    DnaExchange exchange; std::atomic<bool> finished { false }, consistent { true };
    std::thread audio([&] { int reads = 0; while (!finished.load() || reads < 1000) { auto read = exchange.read(); if (const auto* data = read.get(); data && data->valid) for (auto v : data->envelope) if (v != data->envelope[0]) consistent.store(false); ++reads; } });
    for (int i = 0; i < 2000; ++i) { DnaProfile next; next.valid = true; next.duration = 1; next.envelope.fill(static_cast<float>(i % 100) / 100); exchange.publish(next); }
    finished.store(true); audio.join(); check(consistent.load(), "profile publication must never expose partially-written data");
}
int main()
{
    checkDna();
    constexpr double pi = 3.141592653589793;
    for (double sampleRate : { 44100.0, 48000.0, 96000.0 })
    {
        CrushDSP dsp;
        dsp.prepare(sampleRate);
        CrushDSP::Parameters p;
        p.mix = 0; p.output = 0;
        for (int i = 0; i < 10000; ++i)
        {
            const float input = 0.7f * static_cast<float>(std::sin(2 * pi * 440 * i / sampleRate));
            float frame[] { input, -input };
            dsp.processFrame(frame, 2, p);
            check(frame[0] == input && frame[1] == -input, "dry mix must be transparent");
        }
        dsp.reset(); p.mix = 1; p.bits = 3; p.rate = 4000; p.protect = 0;
        double errorEnergy = 0;
        for (int i = 0; i < 20000; ++i)
        {
            const float input = 0.5f * static_cast<float>(std::sin(2 * pi * 997 * i / sampleRate));
            float frame[] { input, input };
            dsp.processFrame(frame, 2, p);
            check(std::isfinite(frame[0]) && std::abs(frame[0]) < 3, "crushed output must remain finite and bounded");
            check(frame[0] == frame[1], "identical stereo channels must remain identical");
            errorEnergy += (frame[0] - input) * (frame[0] - input);
        }
        check(errorEnergy > 100, "crushing must audibly change the signal");
        // Exercise continuous automation, mono, and extreme parameter combinations.
        for (int i = 0; i < 50000; ++i)
        {
            p.drive = i % 2 ? 36.0f : 0.0f; p.bits = i % 2 ? 2.0f : 16.0f;
            p.rate = i % 2 ? 500.0f : 44100.0f; p.protect = i % 2 ? 1.0f : 0.0f;
            float frame[] { 0.9f * static_cast<float>(std::sin(i * 0.03)), 0 };
            dsp.processFrame(frame, 1, p);
            check(std::isfinite(frame[0]) && std::abs(frame[0]) < 4, "automation must remain stable");
        }
        dsp.reset();
        for (int i = 0; i < 1000; ++i)
        {
            float frame[] { 0, 0 };
            dsp.processFrame(frame, 2, p);
            check(frame[0] == 0 && frame[1] == 0, "reset must clear audio history");
        }
    }
    std::cout << "PASS: DNA analysis/looping, MUTATE zero, audible sample modulation, no sample layering, MIDI/transient reset, concurrent publication, dry transparency, stereo timing, automation, mono and reset at 3 sample rates\n";
}
