#include "PluginEditor.h"
#include "EditorGeometry.h"
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
        browser = std::make_unique<Browser>(options,processor,[this]{frontendReady=false;visualBridgeReady=false;}); addAndMakeVisible(*browser);
        browser->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
        startTimerHz(60);
    }
    else
    {
        error.setText("WUBROT audio is running. Install Microsoft Edge WebView2 Runtime, then reopen the editor.", juce::dontSendNotification);
        error.setJustificationType(juce::Justification::centred); addAndMakeVisible(error);
    }
    setResizable(false, false);
    applyEditorSize();
}
void WubrotEditor::applyEditorSize()
{
    const int index = processor.editorSize.load();
    const int w = WubrotGeometry::width(index), h = WubrotGeometry::height(index);
    if (getWidth() != w || getHeight() != h)
    {
        setResizeLimits(w, h, w, h);
        setSize(w, h);
    }
}
void WubrotEditor::resized() { if (browser) browser->setBounds(getLocalBounds()); error.setBounds(getLocalBounds().reduced(40)); }
void WubrotEditor::handleEvent(const juce::var& event)
{
    const auto type = event["type"].toString();
    if (type == "uiSize")
    {
        const int index = juce::jlimit(0, 2, static_cast<int>(event["index"]));
        processor.editorSize.store(index);
        processor.updateHostDisplay(juce::AudioProcessorListener::ChangeDetails().withNonParameterStateChanged(true));
        applyEditorSize();
    }
    else if (type == "parameter")
    {
        const auto id = event["id"].toString();
        if (id == "x" || id == "y" || id == "mutate" || id == "drive" || id == "mode" || id == "enabled" || id == "regionStart" || id == "regionEnd" || id == "dnaSpeed")
            processor.setParameterValue(id, static_cast<float>(event["value"]));
    }
    else if (type == "gesture") processor.parameterGesture(event["id"].toString(), static_cast<bool>(event["start"]));
    else if(type=="preset")
    {
        const auto op=event["operation"].toString();
        if(op=="import"||op=="export")
        {
            if(chooser)return;
            const bool exporting=op=="export";
            chooser=std::make_unique<juce::FileChooser>(exporting?"Export WUBROT preset":"Import WUBROT preset",juce::File(),"*.wubpreset");
            auto safe=juce::Component::SafePointer<WubrotEditor>(this);
            chooser->launchAsync((exporting?juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::warnAboutOverwriting:juce::FileBrowserComponent::openMode)|juce::FileBrowserComponent::canSelectFiles,[safe,event,exporting](const juce::FileChooser& dialog){if(safe){auto file=dialog.getResult();if(file!=juce::File()){if(exporting)file=file.withFileExtension(".wubpreset");auto args=juce::JSON::parse(juce::JSON::toString(event));args.getDynamicObject()->setProperty("operation",exporting?"exportFile":"importFile");args.getDynamicObject()->setProperty("path",file.getFullPathName());if(exporting)args.getDynamicObject()->setProperty("overwrite",file==dialog.getResult());safe->processor.presetCommand(args);}juce::MessageManager::callAsync([safe]{if(safe)safe->chooser.reset();});}});
        }
        else if(op.isNotEmpty())processor.presetCommand(event);
        else processor.setCurrentProgram(static_cast<int>(event["index"]));
    }
    else if(type=="browse")
    {
        if(chooser)return;
        chooser=std::make_unique<juce::FileChooser>("Load ROT DNA audio",juce::File(),"*.wav;*.mp3");
        auto safe=juce::Component::SafePointer<WubrotEditor>(this);
        chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe](const juce::FileChooser& dialog){if(safe){const auto file=dialog.getResult();if(file!=juce::File())safe->processor.loadSampleFile(file);juce::MessageManager::callAsync([safe]{if(safe)safe->chooser.reset();});}});
    }
    else if(type=="filePath")
    {
        const auto path=event["path"].toString();const auto file=path.startsWithIgnoreCase("file://")?juce::URL(path).getLocalFile():juce::File(juce::File::isAbsolutePath(path)?path:juce::String());
        processor.loadSampleFile(file);
    }
    else if(type=="dropError")processor.reportSampleError("This drag source did not provide a readable file. Use the upload icon to select a local WAV or MP3.");
    else if (type == "clear") processor.clearSample();
    else if (type == "ready") { frontendReady=true;visualBridgeReady=false;waveformSent=false;lastState.clear();lastPlayhead=-1;processor.presetCommand(juce::JSON::parse("{\"operation\":\"refresh\"}")); }
    else if (type == "visualReady")
    {
        // The page has installed its listener AND applied its first power/state
        // snapshot. Do not consume/send peaks into an unready WebView.
        processor.consumeAudioVisualLevel();lastAudioSequence=processor.audioVisualSequence();
        lastAudioLevel=-1;visualBridgeReady=true;
    }
    else if (type == "sample")
    {
        const auto encoded = event["base64"].toString();
        if (encoded.length() > 28 * 1024 * 1024) {processor.reportSampleError("Audio transfer too large. Choose a file under 20 MB.");return;}
        juce::MemoryBlock bytes;
        juce::MemoryOutputStream stream(bytes, false);
        if (juce::Base64::convertFromBase64(stream, encoded))
            processor.loadSampleBytes(std::move(bytes), event["name"].toString().substring(0, 256));
        else processor.reportSampleError("Audio transfer failed. Use the upload icon to select the file.");
    }
}
void WubrotEditor::timerCallback()
{
    if (!browser || !isShowing()) { audioWasVisible=false; return; }
    applyEditorSize();
    if(!frontendReady)return;
    if (!audioWasVisible) { if(visualBridgeReady)processor.consumeAudioVisualLevel(); lastAudioSequence=processor.audioVisualSequence(); lastAudioLevel=-1; audioWasVisible=true; }
    const double position=processor.modulationPosition();
    if(std::abs(position-lastPlayhead)>1e-12){auto data=std::make_unique<juce::DynamicObject>();data->setProperty("position",position);browser->emitEventIfBrowserIsVisible("wubrotPlayhead",juce::var(data.release()));lastPlayhead=position;}
    // At most 30 compact visualization packets/s, only for newly processed
    // audio. The browser smoothly expires stale data if the host stops callbacks.
    const auto audioSequence=processor.audioVisualSequence();
    if (visualBridgeReady && uiTicks%2==0 && audioSequence!=lastAudioSequence)
    {
        lastAudioSequence=audioSequence;
        const auto intensity=processor.consumeAudioVisualLevel();
        // Active held notes need fresh packets even at constant intensity, so
        // the UI can distinguish sustain from an audio callback that stopped.
        if (intensity>0 || lastAudioLevel>0 || lastAudioLevel<0)
        {
            auto data=std::make_unique<juce::DynamicObject>(); data->setProperty("intensity",intensity);data->setProperty("sequence",static_cast<juce::int64>(audioSequence));
            browser->emitEventIfBrowserIsVisible("wubrotAudio",juce::var(data.release()));lastAudioLevel=intensity;
        }
    }
    if(++uiTicks%3!=0)return;
    const auto revision=processor.presetLibraryRevision();
    if(lastCatalogueRevision!=revision){browser->emitEventIfBrowserIsVisible("wubrotPresetLibrary",processor.presetCatalogue());lastCatalogueRevision=revision;}
    auto state = processor.uiState(false);
    state.getDynamicObject()->removeProperty("playhead");
    const auto json = juce::JSON::toString(state, true);
    if (lastState != json)
    {
        lastState = json;
        const auto waveRevision = state["waveformRevision"].toString();
        if (!waveformSent || lastWaveformRevision != waveRevision)
        {
            state = processor.uiState(true);
            lastWaveformRevision = state["waveformRevision"].toString();
            waveformSent = true;
        }
        browser->emitEventIfBrowserIsVisible("wubrotState", state);
    }
}
#else
WubrotEditor::WubrotEditor(WubrotProcessor& p) : AudioProcessorEditor(p), processor(p), controls(p)
{
    addAndMakeVisible(controls); addAndMakeVisible(load); addAndMakeVisible(presets); addAndMakeVisible(status);
    for (int i = 0; i < 3; ++i) presets.addItem(processor.getProgramName(i), i + 1);
    presets.setSelectedId(1, juce::dontSendNotification);
    presets.onChange = [this] { processor.setCurrentProgram(presets.getSelectedId() - 1); };
    load.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser>("Load ROT DNA audio", juce::File(), "*.wav;*.mp3");
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
