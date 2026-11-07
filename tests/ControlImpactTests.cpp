#include "ControlFixtures.h"
#include "fixtures/CrushDSP-v041.h"
#include <sstream>

struct Harmonics {
  double thd, harmonicRms, level, peak, upperShare;
};
static Harmonics measure(const std::vector<float> &audio) {
  const std::vector<float> tail(audio.end() - sr, audio.end());
  double harmonics = 0, upper = 0, peak = 0;
  for (int h = 2; h <= 128; ++h) {
    const double v = magnitude(tail, 55 * h);
    harmonics += v * v;
    if (h * 55 >= 1000)
      upper += v * v;
  }
  for (float v : audio)
    peak = std::max(peak, double(std::abs(v)));
  const double fundamental = magnitude(tail, 55);
  return {std::sqrt(harmonics) / std::max(1e-9, fundamental),
          std::sqrt(harmonics * .5), rms(tail), peak,
          upper / (harmonics + fundamental * fundamental + 1e-20)};
}
int main(int argc, char **argv) {
  std::ostringstream report;
  report << "# Control effectiveness: synthetic, not subjective quality "
            "verification\n\n"
         << "Model 2 = pre-regression v0.2.37; model 3 = v0.2.41; model 4 = "
            "revised. "
         << "DRIVE's unchanged host range is 0-36 dB: 18 is the midpoint. "
         << "THD uses 55 Hz sine harmonics 2-128 over the final second; this "
            "is a control-effectiveness check, not a musical-quality score. "
         << "Upper share is harmonic energy at/above 1 kHz divided by measured "
            "harmonic plus fundamental energy.\n\n"
         << "| Model | X | Y | DRIVE | THD | Harmonic RMS | Output RMS | Peak "
            "| Upper share |\n|---|---|---|---|---|---|---|---|---|\n";
  auto metal = analyse(reference(0)), vocal = analyse(reference(1)),
       noise = analyse(reference(2));
  const std::array<const DnaProfile *, 3> donors{&metal, &vocal, &noise};
  const auto sine = bass(0, sr * 2);
  for (auto xy :
       {std::array<float, 2>{.45f, 0}, std::array<float, 2>{.55f, -.45f}}) {
    std::array<Harmonics, 3> revised{};
    std::array<std::vector<float>, 3> audio;
    for (int model : {2, 3, 4})
      for (int d = 0; d < 3; ++d) {
        const auto out =
            render(sine, xy[0], xy[1], nullptr, 0, 1, model, float(d * 18));
        const auto m = measure(out);
        report << '|' << model << '|' << xy[0] << '|' << xy[1] << '|' << d * 18
               << '|' << m.thd << '|' << m.harmonicRms << '|' << m.level << '|'
               << m.peak << '|' << m.upperShare << "|\n";
        if (model == 4) {
          revised[std::size_t(d)] = m;
          audio[std::size_t(d)] = out;
          std::cout << "Drive xy=" << xy[0] << "," << xy[1] << " d=" << d * 18
                    << " THD=" << m.thd << " harmonicRMS=" << m.harmonicRms
                    << " level=" << m.level << std::endl;
        }
      }
    for (int d = 1; d < 3; ++d) {
      require(revised[d].thd > revised[d - 1].thd * 1.15,
              "DRIVE failed to increase nonlinear harmonic proportion");
      require(revised[d].harmonicRms > revised[d - 1].harmonicRms * 1.1,
              "DRIVE compensation suppressed generated harmonics");
      require(spectralDistance(audio[d - 1], audio[d]) > .02,
              "DRIVE only changed level");
    }
    const double minLevel =
        std::min({revised[0].level, revised[1].level, revised[2].level});
    const double maxLevel =
        std::max({revised[0].level, revised[1].level, revised[2].level});
    require(maxLevel / minLevel < 1.6,
            "DRIVE has excessive output-level swing");
  }
  report << "\n## Normalized spectral changes with MUTATE (identical low-pass "
            "input, DRIVE 0)\n\n";
  const auto low = bass(2, sr * 2);
  std::array<std::array<std::vector<float>, 3>, 3> mutation;
  for (int donor = 0; donor < 3; ++donor) {
    for (int m = 0; m < 3; ++m)
      mutation[donor][m] =
          render(low, .55f, -.45f, donors[donor], m * .5f, 1, 4);
    report << "Reference " << donor
           << " (55+165 Hz input; harmonic ratio includes source partials):\n\n"
           << "| MUTATE | Harmonic/fundamental ratio | Output RMS | Peak | "
              "Upper share |\n"
           << "|---|---|---|---|---|\n";
    for (int m = 0; m < 3; ++m) {
      const auto metrics = measure(mutation[donor][m]);
      report << '|' << m * 50 << '|' << metrics.thd << '|' << metrics.level
             << '|' << metrics.peak << '|' << metrics.upperShare << "|\n";
    }
    const double half =
        spectralDistance(mutation[donor][0], mutation[donor][1]);
    const double full =
        spectralDistance(mutation[donor][1], mutation[donor][2]);
    report << "Reference " << donor << ": 0 to 50 " << half << "; 50 to 100 "
           << full << "\n\n";
    require(half > .03 && full > .06,
            "MUTATE failed to reshape the spectral distribution");
  }
  for (int m = 1; m < 3; ++m)
    for (int a = 0; a < 3; ++a)
      for (int b = a + 1; b < 3; ++b) {
        const double distance =
            spectralDistance(mutation[a][m], mutation[b][m]);
        report << "MUTATE " << m * 50 << " references " << a << '/' << b << ": "
               << distance << "\n\n";
        require(distance > .03,
                "Different DNA profiles collapsed into generic distortion");
      }
  for (float radius : {.3f, .55f, .9f}) {
    std::array<std::vector<float>, 4> quadrants, previous;
    const float z = radius * .70710678f;
    for (int q = 0; q < 4; ++q) {
      quadrants[q] =
          render(low, q & 1 ? z : -z, q & 2 ? z : -z, nullptr, 0, 1, 4);
      previous[q] =
          render(low, q & 1 ? z : -z, q & 2 ? z : -z, nullptr, 0, 1, 3);
    }
    double least = 100, priorLeast = 100;
    for (int a = 0; a < 4; ++a)
      for (int b = a + 1; b < 4; ++b) {
        least = std::min(least, spectralDistance(quadrants[a], quadrants[b]));
        priorLeast =
            std::min(priorLeast, spectralDistance(previous[a], previous[b]));
      }
    report << "Equal-radius " << radius
           << " least quadrant spectral difference: " << least << " (v0.2.41 "
           << priorLeast << ")" << "\n\n";
    std::cout << "Quadrant radius=" << radius << " least=" << least
              << std::endl;
    // Reject identical/level-only regions, without treating an arbitrary
    // distance as musical quality.
    require(least > .005, "Signed XY regions lost spectral differentiation");
  }
  for (int kind = 0; kind < 5; ++kind) {
    const auto input = bass(kind);
    require(render(input, 0, 0, &metal, 1, 1, 4, 36) == input,
            "Dry centre changed");
    require(render(input, .6f, -.4f, nullptr, 1, 1, 4) ==
                render(input, .6f, -.4f, &metal, 0, 1, 4),
            "MUTATE zero/absent DNA core mismatch");
    if (kind == 4)
      require(render(input, .6f, -.4f, &metal, 1, 1, 4, 36) == input,
              "Independent sound during silence");
    // The approved version-3 sound, including its control ramps, must remain
    // exact.
    CrushDSP current;
    V041CrushDSP frozen;
    current.prepare(sr);
    frozen.prepare(sr);
    CrushDSP::Parameters p;
    V041CrushDSP::Parameters old;
    for (std::size_t i = 0; i < input.size(); ++i) {
      auto configure = [&](auto &c) {
        const float x = std::sin(float(i / 1536) * .7f) * .65f,
                    y = std::cos(float(i / 2048) * .4f) * .6f;
        const auto xy = XYMapping::map(x, y, sr, 3);
        c.bits = xy.bits;
        c.rate = xy.rate;
        c.activity = xy.activity;
        c.centered = true;
        c.processingVersion = 3;
        c.signedX = x;
        c.signedY = y;
        c.drive = float((i / 3072) % 3) * 18;
        c.mutate = float((i / 4096) % 3) * .5f;
        c.mix = i % 16000 < 12000 ? 1 : 0;
        c.output = 0;
        c.enabled = i % 19000 < 17000;
        c.speed = i % 11000 < 5500 ? .5f : 2.f;
      };
      configure(p);
      configure(old);
      float a[]{input[i], input[i]}, b[]{input[i], input[i]};
      const auto *dna = i % 20000 < 16000 ? &vocal : nullptr;
      current.processFrame(a, 2, p, dna);
      frozen.processFrame(b, 2, old, dna);
      require(a[0] == b[0] && a[1] == b[1],
              "Version-3 project sound was silently reinterpreted");
    }
  }
  double hottest = 0;
  for (int kind = 0; kind < 4; ++kind)
    for (float inputPeak : {.08f, .35f, .9f}) {
      auto hot = bass(kind);
      float peak = 0;
      for (float v : hot)
        peak = std::max(peak, std::abs(v));
      for (float &v : hot)
        v *= inputPeak / peak;
      for (int q = 0; q < 4; ++q)
        for (float drive : {0.f, 18.f, 36.f})
          for (const auto *dna : std::array<const DnaProfile *, 4>{
                   nullptr, &metal, &vocal, &noise}) {
            const auto out = render(hot, q & 1 ? .6f : -.6f, q & 2 ? .6f : -.6f,
                                    dna, 1, 1, 4, drive);
            for (float v : out)
              hottest = std::max(hottest, double(std::abs(v)));
          }
    }
  require(hottest < 1, "Excessive full-level revised output");
  report << "Input peaks 0.08/0.35/0.9 stress observed maximum: " << hottest
         << "\n\n";
  auto quiet = analyse(reference(0, .1f));
  require(difference(render(low, .55f, -.45f, &metal, 1, 1, 4),
                     render(low, .55f, -.45f, &quiet, 1, 1, 4)) < .015,
          "Reference loudness masquerades as DNA shape");
  const auto source = bass(3, sr * 2);
  // Full-boundary quantization/hold settings and note gaps deserve separate
  // checks.
  auto edgeInput = bass(1);
  for (auto &v : edgeInput)
    v *= 3.6f; // Saw peak 0.9.
  for (auto xy :
       std::array<std::array<float, 2>, 8>{{{-.70710678f, -.70710678f},
                                            {.70710678f, -.70710678f},
                                            {-.70710678f, .70710678f},
                                            {.70710678f, .70710678f},
                                            {-1, 0},
                                            {1, 0},
                                            {0, -1},
                                            {0, 1}}})
    for (const auto *dna :
         std::array<const DnaProfile *, 4>{nullptr, &metal, &vocal, &noise}) {
      const auto out = render(edgeInput, xy[0], xy[1], dna, 1, 1, 4, 36);
      for (float v : out)
        hottest = std::max(hottest, double(std::abs(v)));
    }
  require(hottest < 1, "Full XY boundary produced excessive output");
  report << "Including exact XY boundaries, observed stress peak: " << hottest
         << "\n\n";
  for (const auto *dna : donors) {
    const auto notes = render(source, .55f, -.45f, dna, 1, 1, 4, 36);
    for (std::size_t i = 0; i < notes.size(); ++i)
      if (std::fmod(double(i) / sr, .25) > .225)
        require(notes[i] == 0,
                "Tonal processing continued independently between notes");
    const auto tone = render(sine, .55f, -.45f, dna, 1, 1, 4, 36);
    require(magnitude(tone, 55) > .02 &&
                magnitude(tone, 55) > magnitude(tone, 60) * 3,
            "Source pitch identity lost at extreme DRIVE/DNA");
  }
  if (argc > 1) {
    const std::filesystem::path dir(argv[1]);
    std::filesystem::create_directories(dir);
    wav(dir / "impact-dry-synthetic-matched.wav", match(source));
    for (int model : {2, 3, 4})
      for (int d = 0; d < 3; ++d)
        wav(dir / ("impact-model" + std::to_string(model) + "-drive" +
                   std::to_string(d * 50) + "-matched.wav"),
            match(render(source, .55f, -.45f, nullptr, 0, 1, model,
                         float(d * 18))));
    const std::array<std::string, 3> names{"metallic", "vowel", "percussion"};
    for (int model : {3, 4})
      for (int donor = 0; donor < 3; ++donor)
        for (int m = 1; m < 3; ++m)
          wav(dir /
                  ("impact-model" + std::to_string(model) + "-" + names[donor] +
                   "-mutate" + std::to_string(m * 50) + "-matched.wav"),
              match(render(source, .55f, -.45f, donors[donor], m * .5f, 1,
                           model)));
    for (int q = 0; q < 4; ++q)
      wav(dir / ("impact-quadrant" + std::to_string(q) + "-matched.wav"),
          match(render(source, q & 1 ? .55f : -.55f, q & 2 ? .55f : -.55f,
                       nullptr, 0, 1, 4)));
    std::ofstream(dir / "Control-impact.md") << report.str();
  }
  std::cout << report.str()
            << "PASS: control effectiveness, unchanged "
               "mapping/neutral/silence, exact v0.2.41 recall and bounded "
               "peaks. Subjective listening remains manual.\n";
}
