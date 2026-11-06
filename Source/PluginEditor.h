#pragma once
#include "PluginProcessor.h"
#if WUBROT_WEB_UI
#include <juce_gui_extra/juce_gui_extra.h>
class WubrotEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit WubrotEditor(WubrotProcessor&);
    void resized() override;
private:
    class Browser final : public juce::WebBrowserComponent
    {
    public:
        explicit Browser(const Options& options) : WebBrowserComponent(options) {}
        bool pageAboutToLoad(const juce::String& url) override
        { return url == "about:blank" || url.startsWith(getResourceProviderRoot()); }
        void newWindowAttemptingToLoad(const juce::String&) override {}
    };
    juce::WebBrowserComponent::Options makeOptions();
    void handleEvent(const juce::var&);
    void applyEditorSize();
    void timerCallback() override;
    WubrotProcessor& processor;
    std::unique_ptr<Browser> browser;
    juce::String lastState;
    juce::Label error;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WubrotEditor)
};
#else
// Linux development fallback. Windows/macOS use the actual approved web interface.
class WubrotEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit WubrotEditor(WubrotProcessor&);
    void resized() override;
private:
    void timerCallback() override;
    WubrotProcessor& processor;
    juce::GenericAudioProcessorEditor controls;
    juce::TextButton load { "Load ROT DNA WAV" };
    juce::ComboBox presets;
    juce::Label status;
    std::unique_ptr<juce::FileChooser> chooser;
};
#endif
