// Degrain Plugin Editor — WebView Implementation (JUCE 8)
// Generated: 2026-02-14

#include "PluginEditor.h"
#include "PluginProcessor.h"

DegrainEditor::DegrainEditor (DegrainProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // Set fixed window size (900x480, not resizable)
    setSize (900, 480);
    setResizable (false, false);

    // Build WebView with all relays
    juce::WebBrowserComponent::Options options;
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withNativeIntegrationEnabled()
                     .withResourceProvider ([this] (const auto& url) { return getResource (url); },
                                            juce::URL ("https://degrain.local"))
                     .withOptionsFrom (delayTimeRelay)
                     .withOptionsFrom (delaySyncToggleRelay)
                     .withOptionsFrom (delaySyncDivisionRelay)
                     .withOptionsFrom (grainSizeRelay)
                     .withOptionsFrom (grainRateRelay)
                     .withOptionsFrom (grainRateSyncToggleRelay)
                     .withOptionsFrom (grainRateSyncDivisionRelay)
                     .withOptionsFrom (grainShapeRelay)
                     .withOptionsFrom (feedbackRelay)
                     .withOptionsFrom (diffusionRelay)
                     .withOptionsFrom (stereoSpreadRelay)
                     .withOptionsFrom (detuneRelay)
                     .withOptionsFrom (lowCutRelay)
                     .withOptionsFrom (highCutRelay)
                     .withOptionsFrom (reverseMixRelay)
                     .withOptionsFrom (grainMutationRelay)
                     .withOptionsFrom (freqSpreadRelay)
                     .withOptionsFrom (freezeRelay)
                     .withOptionsFrom (dryWetRelay);

    webView = std::make_unique<juce::WebBrowserComponent> (options);
    addAndMakeVisible (*webView);

    // Navigate to embedded HTML
    webView->goToURL ("https://degrain.local/index.html");

    // Create parameter attachments (get parameters from processor APVTS)
    auto& apvts = processorRef.getAPVTS();

    delayTimeAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter ("delay_time"), delayTimeRelay);
    delaySyncToggleAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter ("delay_sync_toggle"), delaySyncToggleRelay);
    delaySyncDivisionAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter ("delay_sync_division"), delaySyncDivisionRelay);

    grainSizeAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter ("grain_size"), grainSizeRelay);
    grainRateAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter ("grain_rate"), grainRateRelay);
    grainRateSyncToggleAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter ("grain_rate_sync_toggle"), grainRateSyncToggleRelay);
    grainRateSyncDivisionAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter ("grain_rate_sync_division"), grainRateSyncDivisionRelay);
    grainShapeAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter ("grain_shape"), grainShapeRelay);

    feedbackAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter ("feedback"), feedbackRelay);
    diffusionAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter ("diffusion"), diffusionRelay);
    stereoSpreadAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter ("stereo_spread"), stereoSpreadRelay);
    detuneAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter ("detune"), detuneRelay);
    lowCutAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter ("low_cut"), lowCutRelay);
    highCutAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter ("high_cut"), highCutRelay);

    reverseMixAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter ("reverse_mix"), reverseMixRelay);
    grainMutationAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter ("grain_mutation"), grainMutationRelay);
    freqSpreadAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter ("freq_spread"), freqSpreadRelay);

    freezeAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter ("freeze"), freezeRelay);
    dryWetAttachment = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter ("dry_wet"), dryWetRelay);

    // Start timer for real-time updates (e.g., meters, visualizations)
    // Placeholder: adjust rate as needed for your use case
    // startTimer (30); // 30ms refresh (~33fps)
}

DegrainEditor::~DegrainEditor()
{
    stopTimer();
}

void DegrainEditor::paint (juce::Graphics& g)
{
    // Fallback background (shown if WebView fails to load)
    g.fillAll (juce::Colours::black);
}

void DegrainEditor::resized()
{
    // WebView fills entire editor bounds
    if (webView)
        webView->setBounds (getLocalBounds());
}

void DegrainEditor::timerCallback()
{
    // Placeholder for real-time UI updates (meters, waveforms, etc.)
    // Example: send meter values to JavaScript
    // if (webView)
    // {
    //     auto meterLevel = processorRef.getMeterLevel();
    //     webView->emitEventIfBrowserIsVisible ("meterUpdate", meterLevel);
    // }
}

std::optional<juce::WebBrowserComponent::Resource> DegrainEditor::getResource (const juce::String& url)
{
    // Serve embedded HTML from BinaryData
    // Assumes CMakeLists.txt has added Source/ui/public/index.html to BinaryData

    auto path = juce::URL (url).getFileName();

    if (path == "index.html")
    {
        auto data = juce::MemoryBlock (BinaryData::index_html, BinaryData::index_htmlSize);
        return juce::WebBrowserComponent::Resource { std::move (data), "text/html" };
    }

    // Add additional resources here (CSS, JS, images) if needed
    // Example:
    // if (path == "style.css")
    // {
    //     auto data = juce::MemoryBlock (BinaryData::style_css, BinaryData::style_cssSize);
    //     return juce::WebBrowserComponent::Resource { std::move (data), "text/css" };
    // }

    return std::nullopt;
}
