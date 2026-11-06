#pragma once
#include "PresetData.h"
class PresetLibrary
{
  public:
    explicit PresetLibrary(juce::File documents = {});
    juce::File directory() const { return root; }
    juce::Result ensureDirectory() const;
    static juce::Result validName(const juce::String &);
    juce::File fileFor(const juce::String &name) const;
    juce::File resolve(const juce::String &filename) const;
    juce::Array<juce::var> list(juce::String &error) const;
    static juce::Result validate(const juce::ValueTree &);
    static juce::Result read(const juce::File &, juce::ValueTree &, juce::String &name);
    static juce::Result write(const juce::File &, const juce::ValueTree &, const juce::String &,
                              bool overwrite);
    juce::Result save(const juce::ValueTree &, const juce::String &, bool overwrite) const;
    juce::Result rename(const juce::String &filename, const juce::String &name, bool overwrite) const;
    juce::Result remove(const juce::String &filename) const;

  private:
    juce::File root;
};
