#include "CrushDSP.h"
#include "XYMapping.h"
#include "fixtures/CrushDSP-v037.h"
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
static constexpr int sr = 48000;
static void require(bool v, const char *m)
{
    if (!v)
    {
        std::cerr << m << '\n';
        std::exit(1);
    }
}
static std::vector<float> bass(int kind, int samples = sr)
{
    std::vector<float> a(samples);
    for (int i = 0; i < samples; ++i)
    {
        double t = double(i) / sr, phase = std::fmod(t * 55, 1.);
        float v = 0;
        if (kind == 0)
            v = .35f * float(std::sin(t * 345.575));
        if (kind == 1)
            v = .25f * float(2 * phase - 1);
        if (kind == 2)
            v = .35f * float(std::sin(t * 345.575)) + .08f * float(std::sin(t * 1036.726));
        if (kind == 3)
            v = (.28f * float(std::sin(t * 345.575)) + .13f * float(std::sin(t * 2073.45))) *
                std::max(0.f, 1 - float(std::fmod(t, .25)) / .14f);
        a[i] = v;
    }
    return a;
}
static std::vector<float> reference(int kind, float gain = 1)
{
    std::vector<float> a(sr);
    unsigned r = 13;
    for (int i = 0; i < sr; ++i)
    {
        double t = double(i) / sr;
        float v = 0;
        const bool second = i > sr / 2;
        r = 1664525 * r + 1013904223;
        float noise = float(double(r >> 8) / 16777216 - .5);
        if (kind == 0)
            v = .32f * float(std::sin(t * (second ? 13259.1 : 7979.645))) +
                .2f * float(std::sin(t * 21168.54));
        if (kind == 1)
            v = .3f * float(std::sin(t * (second ? 5969.026 : 2827.433))) +
                .2f * float(std::sin(t * (second ? 15707.963 : 8796.459)));
        if (kind == 2)
            v = noise * .5f;
        v *= .2f + .8f * float(std::exp(-std::fmod(t, second ? .17 : .31) * 18));
        a[i] = v * gain;
    }
    return a;
}
static DnaProfile analyse(const std::vector<float> &a, bool modern = true)
{
    const float *p[]{a.data()};
    return DnaProfile::analyse(p, 1, a.size(), sr, modern);
}
static double rms(const std::vector<float> &a)
{
    double sum = 0;
    for (float v : a)
        sum += v * v;
    return std::sqrt(sum / a.size());
}
static std::vector<float> match(std::vector<float> a)
{
    float g = float(.12 / std::max(1e-12, rms(a)));
    for (float &v : a)
        v *= g;
    return a;
}
static double difference(const std::vector<float> &a, const std::vector<float> &b)
{
    double sum = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        sum += (a[i] - b[i]) * (a[i] - b[i]);
    return std::sqrt(sum / a.size()) / std::max(1e-12, rms(a));
}
static std::vector<float> render(const std::vector<float> &a, float x, float y,
                                 const DnaProfile *dna = nullptr, float mutation = 1, float speed = 1,
                                 int version = 3, float drive = 0)
{
    CrushDSP dsp;
    dsp.prepare(sr);
    const auto mapped = XYMapping::map(x, y, sr, version);
    CrushDSP::Parameters p;
    p.bits = mapped.bits;
    p.rate = mapped.rate;
    p.activity = mapped.activity;
    p.centered = true;
    p.processingVersion = version;
    p.signedX = x;
    p.signedY = y;
    p.mutate = mutation;
    p.speed = speed;
    p.mix = 1;
    p.output = 0;
    p.drive = drive;
    std::vector<float> out;
    for (float v : a)
    {
        float f[]{v, v};
        dsp.processFrame(f, 2, p, dna);
        require(std::isfinite(f[0]) && std::abs(f[0]) < 2, "Unstable modern output");
        out.push_back(f[0]);
    }
    return out;
}
static double magnitude(const std::vector<float> &a, double hz)
{
    const double c = 2 * std::cos(6.28318530718 * hz / sr);
    double z1 = 0, z2 = 0;
    for (float v : a)
    {
        const double z = v + c * z1 - z2;
        z2 = z1;
        z1 = z;
    }
    return 2 * std::sqrt(std::max(0., z1 * z1 + z2 * z2 - c * z1 * z2)) / a.size();
}
static std::array<double, 8> spectrum(const std::vector<float> &a)
{
    std::array<double, 8> s{};
    for (int h = 1; h <= 128; ++h)
    {
        const auto b = std::min(7, int(std::log2(h)));
        const auto m = magnitude(a, 55 * h);
        s[std::size_t(b)] += m * m;
    }
    double sum = 0;
    for (auto &v : s)
    {
        v = std::sqrt(v);
        sum += v;
    }
    for (auto &v : s)
        v /= sum + 1e-20;
    return s;
}
static double spectralDistance(const std::vector<float> &a, const std::vector<float> &b)
{
    auto x = spectrum(a), y = spectrum(b);
    double d = 0;
    for (std::size_t i = 0; i < x.size(); ++i)
        d += std::abs(x[i] - y[i]);
    return d;
}
static void wav(const std::filesystem::path &f, const std::vector<float> &a)
{
    std::ofstream out(f, std::ios::binary);
    auto word = [&](unsigned v, int n) {
        for (int i = 0; i < n; ++i)
            out.put(char((v >> (i * 8)) & 255));
    };
    out.write("RIFF", 4);
    word(36 + unsigned(a.size() * 2), 4);
    out.write("WAVEfmt ", 8);
    word(16, 4);
    word(1, 2);
    word(1, 2);
    word(sr, 4);
    word(sr * 2, 4);
    word(2, 2);
    word(16, 2);
    out.write("data", 4);
    word(unsigned(a.size() * 2), 4);
    for (float v : a)
    {
        require(std::abs(v) <= 1, "Audition clipping");
        word(unsigned(int(v * 32767)), 2);
    }
}
int main(int argc, char **argv)
{
    auto metal = analyse(reference(0)), vocal = analyse(reference(1)), noise = analyse(reference(2));
    require(metal.spectralCount == 128, "No temporal descriptors");
    for (int kind = 0; kind < 5; ++kind)
    {
        auto a = bass(kind);
        require(render(a, 0, 0, &metal) == a, "Neutral gate changed");
        require(render(a, .45f, -.45f, nullptr, 1) == render(a, .45f, -.45f, &metal, 0),
                "Absent DNA and MUTATE0 differ");
        if (kind == 4)
            require(render(a, .5f, .5f, &metal) == a, "Independent audible layer on silence");
    }
    // Wet contributions must reach exactly zero, not leave an asymptotic tail.
    for (int rate : {44100, 48000, 96000, 192000})
    {
        CrushDSP dsp;
        dsp.prepare(rate);
        CrushDSP::Parameters p;
        const auto mapping = XYMapping::map(.6f, -.4f, rate, 3);
        p.processingVersion = 3;
        p.centered = true;
        p.bits = mapping.bits;
        p.rate = mapping.rate;
        p.activity = mapping.activity;
        p.signedX = .6f;
        p.signedY = -.4f;
        p.mix = 1;
        p.output = 0;
        p.mutate = 1;
        p.drive = 36;
        for (int i = 0; i < rate / 10; ++i)
        {
            const float original = .25f * std::sin(i * .04f);
            float frame[]{original, original};
            if (i == rate / 20) p.mix = 0;
            dsp.processFrame(frame, 2, p, &metal);
            if (i > rate * .075)
                require(frame[0] == original && frame[1] == original,
                        "Wet mix did not reach exact dry after its 20 ms ramp");
        }
    }
    auto input = bass(1);
    double minimumQuadrant = 10, minimumSpectrum = 10;
    for (float distance : {.3f, .55f, .9f})
    {
        float z = distance * .70710678f;
        std::vector<std::vector<float>> out;
        for (int q = 0; q < 4; ++q)
            out.push_back(match(render(input, q & 1 ? z : -z, q & 2 ? z : -z)));
        for (int i = 0; i < 4; ++i)
            for (int j = i + 1; j < 4; ++j)
            {
                double d = difference(out[i], out[j]), s = spectralDistance(out[i], out[j]);
                minimumQuadrant = std::min(minimumQuadrant, d);
                minimumSpectrum = std::min(minimumSpectrum, s);
                std::cout << "quadrants distance=" << distance << " pair=" << i << ',' << j
                          << " matched=" << d << " spectrum=" << s << '\n';
                require(d > .07 && s > .005, "Quadrants differ only trivially/level");
            }
    }
    auto a = bass(2);
    auto m = match(render(a, .55f, -.45f, &metal)), v = match(render(a, .55f, -.45f, &vocal)),
         n = match(render(a, .55f, -.45f, &noise));
    std::cout << "DNA separation " << difference(m, v) << ' ' << difference(m, n) << ' ' << difference(v, n)
              << '\n';
    require(std::min({difference(m, v), difference(m, n), difference(v, n)}) > .18,
            "DNA spectral transformation too weak");
    auto repeated = analyse(reference(0));
    require(difference(render(a, .55f, -.45f, &metal), render(a, .55f, -.45f, &repeated)) < 1e-4,
            "Nondeterministic analysis");
    auto oldM = match(render(a, .55f, -.45f, &metal, 1, 1, 2)),
         oldV = match(render(a, .55f, -.45f, &vocal, 1, 1, 2)),
         oldN = match(render(a, .55f, -.45f, &noise, 1, 1, 2));
    const double oldDnaMin =
        std::min({difference(oldM, oldV), difference(oldM, oldN), difference(oldV, oldN)});
    const double newDnaMin = std::min({difference(m, v), difference(m, n), difference(v, n)});
    std::cout << "Same-input minimum DNA separation v037=" << oldDnaMin << " new=" << newDnaMin << '\n';
    auto quieter = analyse(reference(0, .1f));
    require(difference(render(a, .55f, -.45f, &metal), render(a, .55f, -.45f, &quieter)) < .015,
            "Reference loudness changed gain/shape");
    auto slow = render(a, .55f, -.45f, &vocal, 1, .5f), fast = render(a, .55f, -.45f, &vocal, 1, 2);
    require(difference(match(slow), match(fast)) > .12, "SPEED failed spectral movement");
    // Exact processing compatibility, including v2 gated centre and automation.
    for (int model : {1, 2})
    {
        CrushDSP now;
        V037CrushDSP old;
        now.prepare(sr);
        old.prepare(sr);
        CrushDSP::Parameters p;
        V037CrushDSP::Parameters q;
        p.centered = q.centered = model == 2;
        p.processingVersion = model;
        for (int i = 0; i < sr; ++i)
        {
            float x = .7f * std::sin(i * .00031f), y = .6f * std::cos(i * .00017f);
            auto map = XYMapping::map(x, y, sr, model);
            p.bits = q.bits = map.bits;
            p.rate = q.rate = map.rate;
            p.activity = q.activity = map.activity;
            p.drive = q.drive = 17;
            p.mutate = q.mutate = .8f;
            float f[]{a[std::size_t(i) % a.size()], -a[std::size_t(i) % a.size()]}, g[]{f[0], f[1]};
            now.processFrame(f, 2, p, &metal);
            old.processFrame(g, 2, q, &metal);
            require(std::memcmp(f, g, sizeof f) == 0, "v037 sound reinterpreted");
        }
    }
    double hottest = 0;
    for (int kind = 0; kind < 4; ++kind)
    {
        auto hot = bass(kind, sr / 2);
        float peak = 0;
        for (float z : hot)
            peak = std::max(peak, std::abs(z));
        for (float &z : hot)
            z *= .9f / peak;
        for (float drive : {0.f, 18.f, 36.f})
            for (int q = 0; q < 4; ++q)
                for (auto donor : {&metal, &vocal, &noise})
                {
                    auto out = render(hot, q & 1 ? .6f : -.6f, q & 2 ? .6f : -.6f, donor, 1, 1, 3, drive);
                    for (float z : out)
                        hottest = std::max(hottest, double(std::abs(z)));
                }
    }
    std::cout << "0.9-input modern stress peak " << hottest << '\n';
    require(hottest < 1.0001, "Uncontrolled modern output peaks");
    // Isolate the diagnosed linear-path attenuation/phase cancellation.
    for (int hz : {1000, 8000, 12000})
    {
        NonlinearStage before, after;
        before.prepare();
        after.prepare();
        std::vector<float> input(sr), oldPath(sr), newPath(sr);
        for (int i = 0; i < sr * 2; ++i)
        {
            float x = .0001f * float(std::sin(6.28318530718 * hz * i / sr));
            const float legacy = .45f * x + .55f * before.tick(x, 0, 1, 0, 0),
                        modern = after.tickResidual(x, 0, 2.5f, 0, 1.2f, .55f,16);
            if (i >= sr)
            {
                input[std::size_t(i - sr)] = x;
                oldPath[std::size_t(i - sr)] = legacy;
                newPath[std::size_t(i - sr)] = modern;
            }
        }
        const double oldGain = magnitude(oldPath, hz) / magnitude(input, hz),
                     newGain = magnitude(newPath, hz) / magnitude(input, hz);
        std::cout << "Linear shaper path " << hz << "Hz old=" << oldGain << " residual=" << newGain << '\n';
        require(newGain > .999 && newGain < 1.001, "Residual shaper attenuates the original linear path");
    }
    if (argc > 1)
    {
        std::filesystem::path dir(argv[1]);
        std::filesystem::create_directories(dir);
        auto source = bass(3, sr * 2);
        wav(dir / "regions-dry-synthetic.wav", source);
        wav(dir / "regions-dry-synthetic-matched.wav", match(source));
        for (int q = 0; q < 4; ++q)
        {
            const float x = q & 1 ? .5f : -.5f, y = q & 2 ? .5f : -.5f;
            wav(dir / ("regions-" +
                       std::array<std::string, 4>{"top-left-crunch", "top-right-metallic", "bottom-left-warm",
                                                  "bottom-right-stepped"}[q] +
                       "-v037-matched.wav"),
                match(render(source, x, y, nullptr, 1, 1, 2)));
            wav(dir / ("regions-" +
                       std::array<std::string, 4>{"top-left-crunch", "top-right-metallic", "bottom-left-warm",
                                                  "bottom-right-stepped"}[q] +
                       "-new-matched.wav"),
                match(render(source, x, y)));
        }
        wav(dir / "dna-metallic-v037-matched.wav", match(render(source, .55f, -.45f, &metal, 1, 1, 2)));
        wav(dir / "dna-vowel-v037-matched.wav", match(render(source, .55f, -.45f, &vocal, 1, 1, 2)));
        wav(dir / "dna-percussion-v037-matched.wav", match(render(source, .55f, -.45f, &noise, 1, 1, 2)));
        wav(dir / "dna-metallic-new-matched.wav", match(render(source, .55f, -.45f, &metal)));
        wav(dir / "dna-vowel-new-matched.wav", match(render(source, .55f, -.45f, &vocal)));
        wav(dir / "dna-percussion-new-matched.wav", match(render(source, .55f, -.45f, &noise)));
        wav(dir / "dna-none-new-matched.wav", match(render(source, .55f, -.45f)));
        wav(dir / "dna-vowel-M25-new-matched.wav", match(render(source, .55f, -.45f, &vocal, .25f)));
        wav(dir / "dna-vowel-speed0.5-new-matched.wav", match(render(source, .55f, -.45f, &vocal, 1, .5f)));
        wav(dir / "dna-vowel-speed2-new-matched.wav", match(render(source, .55f, -.45f, &vocal, 1, 2.f)));
        auto donorAudio = reference(1);
        std::vector<float> firstHalf(donorAudio.begin(), donorAudio.begin() + sr / 2),
            lastHalf(donorAudio.begin() + sr / 2, donorAudio.end());
        auto firstRegion = analyse(firstHalf), lastRegion = analyse(lastHalf);
        wav(dir / "dna-vowel-region-first-new-matched.wav", match(render(source, .55f, -.45f, &firstRegion)));
        wav(dir / "dna-vowel-region-last-new-matched.wav", match(render(source, .55f, -.45f, &lastRegion)));
        std::ofstream report(dir / "Regions-validation.txt");
        report << "Synthetic automated checks, not subjective listening verification.\nMinimum matched "
                  "quadrant difference: "
               << minimumQuadrant << "\nMinimum normalized spectral distance: " << minimumSpectrum << "\n";
    }
    std::cout << "PASS signed regions, spectral/temporal DNA, zero/no-DNA equivalence, silence and v1/v2 "
                 "exact recall\n";
}
