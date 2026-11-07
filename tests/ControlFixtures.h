#pragma once
// Shared synthetic sources, level matching and spectral measurements.
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
static void require(bool v, const char *m) {
  if (!v) {
    std::cerr << m << '\n';
    std::exit(1);
  }
}
static std::vector<float> bass(int kind, int samples = sr) {
  std::vector<float> a(samples);
  for (int i = 0; i < samples; ++i) {
    double t = double(i) / sr, phase = std::fmod(t * 55, 1.);
    float v = 0;
    if (kind == 0)
      v = .35f * float(std::sin(t * 345.575));
    if (kind == 1)
      v = .25f * float(2 * phase - 1);
    if (kind == 2)
      v = .35f * float(std::sin(t * 345.575)) +
          .08f * float(std::sin(t * 1036.726));
    if (kind == 3)
      v = (.28f * float(std::sin(t * 345.575)) +
           .13f * float(std::sin(t * 2073.45))) *
          std::max(0.f, 1 - float(std::fmod(t, .25)) / .14f);
    a[i] = v;
  }
  return a;
}
static std::vector<float> reference(int kind, float gain = 1) {
  std::vector<float> a(sr);
  unsigned r = 13;
  for (int i = 0; i < sr; ++i) {
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
static DnaProfile analyse(const std::vector<float> &a, bool modern = true) {
  const float *p[]{a.data()};
  return DnaProfile::analyse(p, 1, a.size(), sr, modern);
}
static double rms(const std::vector<float> &a) {
  double sum = 0;
  for (float v : a)
    sum += v * v;
  return std::sqrt(sum / a.size());
}
static std::vector<float> match(std::vector<float> a) {
  float g = float(.12 / std::max(1e-12, rms(a)));
  for (float &v : a)
    v *= g;
  return a;
}
static double difference(const std::vector<float> &a,
                         const std::vector<float> &b) {
  double sum = 0;
  for (std::size_t i = 0; i < a.size(); ++i)
    sum += (a[i] - b[i]) * (a[i] - b[i]);
  return std::sqrt(sum / a.size()) / std::max(1e-12, rms(a));
}
static std::vector<float> render(const std::vector<float> &a, float x, float y,
                                 const DnaProfile *dna = nullptr,
                                 float mutation = 1, float speed = 1,
                                 int version = 3, float drive = 0) {
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
  for (float v : a) {
    float f[]{v, v};
    dsp.processFrame(f, 2, p, dna);
    require(std::isfinite(f[0]) && std::abs(f[0]) < 2,
            "Unstable modern output");
    out.push_back(f[0]);
  }
  return out;
}
static double magnitude(const std::vector<float> &a, double hz) {
  const double c = 2 * std::cos(6.28318530718 * hz / sr);
  double z1 = 0, z2 = 0;
  for (float v : a) {
    const double z = v + c * z1 - z2;
    z2 = z1;
    z1 = z;
  }
  return 2 * std::sqrt(std::max(0., z1 * z1 + z2 * z2 - c * z1 * z2)) /
         a.size();
}
static std::array<double, 8> spectrum(const std::vector<float> &a) {
  std::array<double, 8> s{};
  for (int h = 1; h <= 128; ++h) {
    const auto b = std::min(7, int(std::log2(h)));
    const auto m = magnitude(a, 55 * h);
    s[std::size_t(b)] += m * m;
  }
  double sum = 0;
  for (auto &v : s) {
    v = std::sqrt(v);
    sum += v;
  }
  for (auto &v : s)
    v /= sum + 1e-20;
  return s;
}
static double spectralDistance(const std::vector<float> &a,
                               const std::vector<float> &b) {
  auto x = spectrum(a), y = spectrum(b);
  double d = 0;
  for (std::size_t i = 0; i < x.size(); ++i)
    d += std::abs(x[i] - y[i]);
  return d;
}
static void wav(const std::filesystem::path &f, const std::vector<float> &a) {
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
  for (float v : a) {
    require(std::abs(v) <= 1, "Audition clipping");
    word(unsigned(int(v * 32767)), 2);
  }
}
