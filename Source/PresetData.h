#pragma once
#include "DnaProfile.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace WubrotPresetData
{
inline juce::var floats(const float *p, std::size_t n)
{
    juce::Array<juce::var> a;
    for (std::size_t i = 0; i < n; ++i)
        a.add(p[i]);
    return a;
}
inline juce::var spectrum(const DnaSpectrum &s)
{
    juce::Array<juce::var> a;
    a.add(s.valid);
    for (auto v : s.bands)
        a.add(v);
    for (auto v : s.features)
        a.add(v);
    for (auto v : s.peaks)
        a.add(v);
    return a;
}
inline bool numbers(const juce::var &v, float *out, std::size_t n, float lo, float hi)
{
    auto *a = v.getArray();
    if (!a || a->size() != static_cast<int>(n))
        return false;
    for (std::size_t i = 0; i < n; ++i)
    {
        const auto &x = (*a)[static_cast<int>(i)];
        if (!(x.isDouble() || x.isInt() || x.isInt64()))
            return false;
        const double d = x;
        if (!std::isfinite(d) || d < lo || d > hi)
            return false;
        out[i] = static_cast<float>(d);
    }
    return true;
}
inline bool readSpectrum(const juce::var &v, DnaSpectrum &s)
{
    auto *a = v.getArray();
    if (!a || a->size() != 21 || !(*a)[0].isBool())
        return false;
    auto sub = [&](int from, int to)
    {
        juce::Array<juce::var> b;
        for (int i = from; i < to; ++i)
            b.add((*a)[i]);
        return juce::var(b);
    };
    if (!numbers(sub(1, 13), s.bands.data(), 12, 0, 1) || !numbers(sub(13, 18), s.features.data(), 5, 0, 1) ||
        !numbers(sub(18, 21), s.peaks.data(), 3, 30, 10000))
        return false;
    s.valid = static_cast<bool>((*a)[0]);
    s.prepareDerived();
    return true;
}
inline juce::var temporal(const DnaProfile &p)
{
    auto *object = new juce::DynamicObject();
    object->setProperty("version", 2);
    juce::Array<juce::var> frames;
    for (std::size_t i = 0; i < p.spectralCount; ++i)
    {
        const auto &f = p.spectralFrames[i];
        juce::Array<juce::var> values;
        for (float v : f.bands)
            values.add(v);
        for (float v : f.features)
            values.add(v);
        for (float v : f.logPeaks)
            values.add(v);
        for (float v : f.resonance)
            values.add(v);
        for (float v : f.persistence)
            values.add(v);
        frames.add(values);
    }
    object->setProperty("frames", frames);
    return object;
}
inline bool readTemporal(const juce::var &v, DnaProfile &p)
{
    if (!v.isObject() || int(v["version"]) != 2)
        return false;
    const auto *frames = v["frames"].getArray();
    if (!frames || frames->isEmpty() || frames->size() > int(p.spectralCapacity))
        return false;
    for (int i = 0; i < frames->size(); ++i)
    {
        std::array<float, 26> values{};
        if (!numbers((*frames)[i], values.data(), values.size(), 0, 10))
            return false;
        auto &f = p.spectralFrames[std::size_t(i)];
        for (std::size_t j = 0; j < 12; ++j)
        {
            if (values[j] > 1)
                return false;
            f.bands[j] = values[j];
        }
        for (std::size_t j = 0; j < 5; ++j)
        {
            if (values[12 + j] > 1)
                return false;
            f.features[j] = values[12 + j];
        }
        for (std::size_t j = 0; j < 3; ++j)
        {
            f.logPeaks[j] = values[17 + j];
            f.resonance[j] = values[20 + j];
            f.persistence[j] = values[23 + j];
            if (f.logPeaks[j] < 3.401197f || f.logPeaks[j] > 9.210341f || f.resonance[j] < 1.5f ||
                f.resonance[j] > 6 || f.persistence[j] > 1)
                return false;
        }
    }
    p.spectralCount = std::size_t(frames->size());
    return true;
}
inline juce::var profile(const DnaProfile &p)
{
    auto *o = new juce::DynamicObject();
    o->setProperty("valid", p.valid);
    o->setProperty("duration", p.duration);
    o->setProperty("envelope", floats(p.envelope.data(), p.bins));
    o->setProperty("character", floats(p.character.data(), p.bins));
    o->setProperty("spectrum", spectrum(p.spectrum));
    if(p.spectralCount>0)o->setProperty("temporal",temporal(p));
    return o;
}
inline bool readProfile(const juce::var &v, DnaProfile &p)
{
    if (!v.isObject() || !v["valid"].isBool())
        return false;
    p.spectralCount=0;
    p.valid = static_cast<bool>(v["valid"]);
    p.duration = static_cast<double>(v["duration"]);
    return std::isfinite(p.duration) && p.duration >= 0 && p.duration <= 30 && (!p.valid || p.duration > 0) &&
           numbers(v["envelope"], p.envelope.data(), p.bins, 0, 1) &&
           numbers(v["character"], p.character.data(), p.bins, 0, 1) &&
           readSpectrum(v["spectrum"], p.spectrum)&&(!v.hasProperty("temporal")||(p.valid&&readTemporal(v["temporal"],p)));
}
// Only analysis descriptors, never PCM, a wavetable, or a sound-playing buffer.
struct Archive
{
    static constexpr int slices = 64;
    DnaProfile full;
    std::array<DnaSpectrum, slices> spectra;
    juce::var toVar() const
    {
        auto *o = new juce::DynamicObject();
        o->setProperty("version",full.spectralCount>0?2:1);
        o->setProperty("full", profile(full));
        juce::Array<juce::var> a;
        for (const auto &s : spectra)
            a.add(spectrum(s));
        o->setProperty("slices", a);
        return o;
    }
    static std::shared_ptr<const Archive> fromVar(const juce::var &v)
    {
        auto p = std::make_shared<Archive>();
        if (!v.isObject() || (static_cast<int>(v["version"]) != 1&&static_cast<int>(v["version"])!=2) || !readProfile(v["full"], p->full) ||
            !p->full.valid)
            return {};
        if((int(v["version"])==2)!=(p->full.spectralCount>0))return {};
        auto *a = v["slices"].getArray();
        if (!a || a->size() != slices)
            return {};
        for (int i = 0; i < slices; ++i)
            if (!readSpectrum((*a)[i], p->spectra[static_cast<std::size_t>(i)]))
                return {};
        return p;
    }
    DnaProfile select(float start, float end) const
    {
        const float lo = juce::jlimit(0.f, .99f, std::min(start, end)),
                    hi = juce::jlimit(lo + .005f, 1.f, std::max(start, end));
        DnaProfile p;
        p.duration = full.duration * (hi - lo);
        float maximum = 0;
        auto at = [](const auto &a, double pos)
        {
            const double index = std::clamp(pos, 0., 1.) * (a.size() - 1);
            const auto i = static_cast<std::size_t>(index), j = std::min(i + 1, a.size() - 1);
            return a[i] + static_cast<float>(index - i) * (a[j] - a[i]);
        };
        for (std::size_t i = 0; i < p.bins; ++i)
        {
            const double pos = lo + (hi - lo) * i / (p.bins - 1);
            p.envelope[i] = at(full.envelope, pos);
            p.character[i] = at(full.character, pos);
            maximum = std::max(maximum, p.envelope[i]);
        }
        if (maximum < 1e-7f)
            return p;
        for (auto &x : p.envelope)
            x /= maximum;
        p.spectrum.bands.fill(0);
        p.spectrum.features.fill(0);
        p.spectrum.peaks.fill(0);
        double weight = 0;
        for (int i = 0; i < slices; ++i)
        {
            const double overlap = std::max(0., std::min<double>(hi, (i + 1.0) / slices) -
                                                    std::max<double>(lo, static_cast<double>(i) / slices));
            const auto &s = spectra[static_cast<std::size_t>(i)];
            if (!s.valid || overlap <= 0)
                continue;
            weight += overlap;
            for (std::size_t j = 0; j < 12; ++j)
                p.spectrum.bands[j] += static_cast<float>(overlap * s.bands[j]);
            for (std::size_t j = 0; j < 5; ++j)
                p.spectrum.features[j] += static_cast<float>(overlap * s.features[j]);
            for (std::size_t j = 0; j < 3; ++j)
                p.spectrum.peaks[j] += static_cast<float>(overlap * s.peaks[j]);
        }
        if (weight > 0)
        {
            for (auto &x : p.spectrum.bands)
                x /= static_cast<float>(weight);
            for (auto &x : p.spectrum.features)
                x /= static_cast<float>(weight);
            for (auto &x : p.spectrum.peaks)
                x /= static_cast<float>(weight);
            p.spectrum.valid = true;
            p.spectrum.prepareDerived();
        }
        else
            p.spectrum = full.spectrum;
        if(full.spectralCount>0){p.spectralCount=p.spectralCapacity;for(std::size_t i=0;i<p.spectralCount;++i)p.spectralFrames[i]=full.spectralAt(lo+(hi-lo)*float(i)/float(p.spectralCount-1));}
        p.valid = true;
        return p;
    }
};
inline juce::String key(const DnaProfile &p)
{
    return juce::String::toHexString(juce::JSON::toString(profile(p), true).hashCode64());
}
} // namespace WubrotPresetData
