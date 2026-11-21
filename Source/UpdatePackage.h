#pragma once
#include <juce_core/juce_core.h>
#include <functional>
namespace WubrotUpdates
{
struct Package
{
    juce::File zip, target, work;
    juce::String version, sha256, installedSha256;
};
juce::String hashFile(const juce::File&);
juce::Result validateArchive(const Package&);
juce::Result validateModule(const juce::File&, const juce::String& version);
juce::Result applyPackage(const Package&,
    const std::function<bool(const juce::File&,const juce::File&)>& move = {},
    const std::function<juce::Result(const juce::File&,const juce::String&)>& validate = {},
    const std::function<juce::Result()>& beforeSwap = {},
    const std::function<void()>& afterSwap = {});
juce::File moduleFile(const juce::File& bundle);
}
