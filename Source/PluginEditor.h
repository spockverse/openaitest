#pragma once
#include "PluginProcessor.h"
#if WUBROT_WEB_UI
#include <juce_gui_extra/juce_gui_extra.h>
class WubrotEditor final : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit WubrotEditor(WubrotProcessor&);
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray& files) override { return !files.isEmpty(); }
    void filesDropped(const juce::StringArray& files,int,int) override { if(!files.isEmpty())processor.loadSampleFile(juce::File(files[0])); }
private:
    class Browser final : public juce::WebBrowserComponent, public juce::FileDragAndDropTarget
    {
    public:
        explicit Browser(const Options& options,WubrotProcessor& p) : WebBrowserComponent(options),processor(p) {}
        bool isInterestedInFileDrag(const juce::StringArray& files) override { return !files.isEmpty(); }
        void filesDropped(const juce::StringArray& files,int,int) override { if(!files.isEmpty())processor.loadSampleFile(juce::File(files[0])); }
        WubrotProcessor& processor;
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
    int lastCatalogueRevision=-1;
    int uiTicks=0;double lastPlayhead=-1;
    juce::Label error;
    std::unique_ptr<juce::FileChooser> chooser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WubrotEditor)
};
#else
// Linux development fallback. Windows/macOS use the actual approved web interface.
class WubrotEditor final : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit WubrotEditor(WubrotProcessor&);
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray& files) override { return !files.isEmpty(); }
    void filesDropped(const juce::StringArray& files,int,int) override { if(!files.isEmpty())processor.loadSampleFile(juce::File(files[0])); }
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
