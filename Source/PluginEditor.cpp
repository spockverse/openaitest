#include "PluginEditor.h"
#if WUBROT_WEB_UI
#include "WubrotAssets.h"
juce::WebBrowserComponent::Options WubrotEditor::makeOptions()
{
    auto options = juce::WebBrowserComponent::Options().withNativeIntegrationEnabled()
        .withEventListener("wubrot", [this](const juce::var& event) { handleEvent(event); })
        .withResourceProvider([](const juce::String& path) -> std::optional<juce::WebBrowserComponent::Resource> {
            if (path != "/") return std::nullopt;
            const auto* begin = reinterpret_cast<const std::byte*>(WubrotAssets::WUBROT_html);
            return juce::WebBrowserComponent::Resource { { begin, begin + WubrotAssets::WUBROT_htmlSize }, "text/html" };
        });
   #if JUCE_WINDOWS
    options = options.withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options(juce::WebBrowserComponent::Options::WinWebView2()
            .withUserDataFolder(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Spock Audio/WUBROT/WebView")));
   #endif
    return options;
}
WubrotEditor::WubrotEditor(WubrotProcessor& p) : AudioProcessorEditor(p), processor(p)
{
    const auto options = makeOptions();
    if (juce::WebBrowserComponent::areOptionsSupported(options))
    {
        browser = std::make_unique<Browser>(options); addAndMakeVisible(*browser);
        browser->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
        startTimerHz(20);
    }
    else
    {
        error.setText("WUBROT audio is running. Install Microsoft Edge WebView2 Runtime, then reopen the editor.", juce::dontSendNotification);
        error.setJustificationType(juce::Justification::centred); addAndMakeVisible(error);
    }
    setResizable(false, false);
    const int widths[] { 600, 800, 1000 };
    const int width = widths[processor.editorSize.load()]; setResizeLimits(width, width * 5 / 6, width, width * 5 / 6); setSize(width, width * 5 / 6);
}
void WubrotEditor::resized() { if (browser) browser->setBounds(getLocalBounds()); error.setBounds(getLocalBounds().reduced(40)); }
void WubrotEditor::handleEvent(const juce::var& event)
{
    const auto type = event["type"].toString();
    if (type == "uiSize")
    {
        const int index = juce::jlimit(0, 2, static_cast<int>(event["index"]));
        processor.editorSize.store(index);
        const int widths[] { 600, 800, 1000 }; const int w = widths[index]; setResizeLimits(w, w * 5 / 6, w, w * 5 / 6); setSize(w, w * 5 / 6);
    }
    else if (type == "parameter")
    {
        const auto id = event["id"].toString();
        if (id == "x" || id == "y" || id == "mutate" || id == "drive" || id == "mode")
            processor.setParameterValue(id, static_cast<float>(event["value"]));
    }
    else if (type == "gesture") processor.parameterGesture(event["id"].toString(), static_cast<bool>(event["start"]));
    else if (type == "preset") processor.setCurrentProgram(static_cast<int>(event["index"]));
    else if (type == "clear") processor.clearSample();
    else if (type == "ready") lastState.clear();
    else if (type == "sample")
    {
        const auto encoded = event["base64"].toString();
        if (encoded.length() > 28 * 1024 * 1024) return;
        juce::MemoryBlock bytes;
        juce::MemoryOutputStream stream(bytes, false);
        if (juce::Base64::convertFromBase64(stream, encoded))
            processor.loadSampleBytes(std::move(bytes), event["name"].toString().substring(0, 256));
    }
}
void WubrotEditor::timerCallback()
{
    if (!browser || !isShowing()) return;
    auto state = processor.uiState(); const auto json = juce::JSON::toString(state, true);
    if (lastState != json) { lastState = json; browser->emitEventIfBrowserIsVisible("wubrotState", state); }
}
#else
WubrotEditor::WubrotEditor(WubrotProcessor& p) : AudioProcessorEditor(p), processor(p), controls(p)
{
    addAndMakeVisible(controls); addAndMakeVisible(load); addAndMakeVisible(presets); addAndMakeVisible(status);
    for (int i = 0; i < 3; ++i) presets.addItem(processor.getProgramName(i), i + 1);
    presets.setSelectedId(1, juce::dontSendNotification);
    presets.onChange = [this] { processor.setCurrentProgram(presets.getSelectedId() - 1); };
    load.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser>("Load ROT DNA WAV", juce::File(), "*.wav");
        auto safe = juce::Component::SafePointer<WubrotEditor>(this);
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe](const juce::FileChooser& fc) { if (safe && fc.getResult().existsAsFile()) safe->processor.loadSampleFile(fc.getResult()); });
    };
    setSize(650, 680); startTimerHz(10);
}
void WubrotEditor::resized()
{
    presets.setBounds(20, 10, 280, 30); load.setBounds(320, 10, 300, 30);
    controls.setBounds(10, 50, getWidth() - 20, getHeight() - 110); status.setBounds(20, getHeight() - 55, getWidth() - 40, 45);
}
void WubrotEditor::timerCallback() { status.setText(processor.uiState()["status"].toString(), juce::dontSendNotification); }
#endif
