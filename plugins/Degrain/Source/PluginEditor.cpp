#include "PluginEditor.h"
#include "BinaryData.h"

DegrainEditor::DegrainEditor(DegrainProcessor& p)
    : AudioProcessorEditor(&p)
    , processorRef(p)
{
    // 1. Create relays (MUST match APVTS parameter IDs exactly)

    // Float params (13)
    delayTimeRelay       = std::make_unique<juce::WebSliderRelay>("delay_time");
    grainSizeRelay       = std::make_unique<juce::WebSliderRelay>("grain_size");
    grainRateRelay       = std::make_unique<juce::WebSliderRelay>("grain_rate");
    feedbackRelay        = std::make_unique<juce::WebSliderRelay>("feedback");
    diffusionRelay       = std::make_unique<juce::WebSliderRelay>("diffusion");
    stereoSpreadRelay    = std::make_unique<juce::WebSliderRelay>("stereo_spread");
    detuneRelay          = std::make_unique<juce::WebSliderRelay>("detune");
    lowCutRelay          = std::make_unique<juce::WebSliderRelay>("low_cut");
    highCutRelay         = std::make_unique<juce::WebSliderRelay>("high_cut");
    reverseMixRelay      = std::make_unique<juce::WebSliderRelay>("reverse_mix");
    grainMutationRelay   = std::make_unique<juce::WebSliderRelay>("grain_mutation");
    freqSpreadRelay      = std::make_unique<juce::WebSliderRelay>("freq_spread");
    grainRandomRelay     = std::make_unique<juce::WebSliderRelay>("grain_random");
    dryWetRelay          = std::make_unique<juce::WebSliderRelay>("dry_wet");
    outputGainRelay      = std::make_unique<juce::WebSliderRelay>("output_gain");

    // Bool params (5)
    delaySyncToggleRelay     = std::make_unique<juce::WebToggleButtonRelay>("delay_sync_toggle");
    grainRateSyncToggleRelay = std::make_unique<juce::WebToggleButtonRelay>("grain_rate_sync_toggle");
    freezeRelay              = std::make_unique<juce::WebToggleButtonRelay>("freeze");
    delayOnRelay             = std::make_unique<juce::WebToggleButtonRelay>("delay_on");
    grainOnRelay             = std::make_unique<juce::WebToggleButtonRelay>("grain_on");

    // Choice params (3)
    delaySyncDivisionRelay     = std::make_unique<juce::WebComboBoxRelay>("delay_sync_division");
    grainRateSyncDivisionRelay = std::make_unique<juce::WebComboBoxRelay>("grain_rate_sync_division");
    grainShapeRelay            = std::make_unique<juce::WebComboBoxRelay>("grain_shape");

    // 2. Create WebView with all relays registered
    webView = std::make_unique<juce::WebBrowserComponent>(
        juce::WebBrowserComponent::Options{}
            .withNativeIntegrationEnabled()
            .withResourceProvider([this](const auto& url) { return getResource(url); })
            .withKeepPageLoadedWhenBrowserIsHidden()
            // Float relays
            .withOptionsFrom(*delayTimeRelay)
            .withOptionsFrom(*grainSizeRelay)
            .withOptionsFrom(*grainRateRelay)
            .withOptionsFrom(*feedbackRelay)
            .withOptionsFrom(*diffusionRelay)
            .withOptionsFrom(*stereoSpreadRelay)
            .withOptionsFrom(*detuneRelay)
            .withOptionsFrom(*lowCutRelay)
            .withOptionsFrom(*highCutRelay)
            .withOptionsFrom(*reverseMixRelay)
            .withOptionsFrom(*grainMutationRelay)
            .withOptionsFrom(*freqSpreadRelay)
            .withOptionsFrom(*grainRandomRelay)
            .withOptionsFrom(*dryWetRelay)
            .withOptionsFrom(*outputGainRelay)
            // Bool relays
            .withOptionsFrom(*delaySyncToggleRelay)
            .withOptionsFrom(*grainRateSyncToggleRelay)
            .withOptionsFrom(*freezeRelay)
            .withOptionsFrom(*delayOnRelay)
            .withOptionsFrom(*grainOnRelay)
            // Choice relays
            .withOptionsFrom(*delaySyncDivisionRelay)
            .withOptionsFrom(*grainRateSyncDivisionRelay)
            .withOptionsFrom(*grainShapeRelay)
    );

    // 3. Create attachments (Pattern 12: 3 params, nullptr for undoManager)
    auto& apvts = processorRef.getAPVTS();

    // Float attachments (13)
    delayTimeAttachment    = std::make_unique<juce::WebSliderParameterAttachment>(*apvts.getParameter("delay_time"),    *delayTimeRelay,    nullptr);
    grainSizeAttachment    = std::make_unique<juce::WebSliderParameterAttachment>(*apvts.getParameter("grain_size"),    *grainSizeRelay,    nullptr);
    grainRateAttachment    = std::make_unique<juce::WebSliderParameterAttachment>(*apvts.getParameter("grain_rate"),    *grainRateRelay,    nullptr);
    feedbackAttachment     = std::make_unique<juce::WebSliderParameterAttachment>(*apvts.getParameter("feedback"),      *feedbackRelay,     nullptr);
    diffusionAttachment    = std::make_unique<juce::WebSliderParameterAttachment>(*apvts.getParameter("diffusion"),     *diffusionRelay,    nullptr);
    stereoSpreadAttachment = std::make_unique<juce::WebSliderParameterAttachment>(*apvts.getParameter("stereo_spread"), *stereoSpreadRelay, nullptr);
    detuneAttachment       = std::make_unique<juce::WebSliderParameterAttachment>(*apvts.getParameter("detune"),        *detuneRelay,       nullptr);
    lowCutAttachment       = std::make_unique<juce::WebSliderParameterAttachment>(*apvts.getParameter("low_cut"),       *lowCutRelay,       nullptr);
    highCutAttachment      = std::make_unique<juce::WebSliderParameterAttachment>(*apvts.getParameter("high_cut"),      *highCutRelay,      nullptr);
    reverseMixAttachment   = std::make_unique<juce::WebSliderParameterAttachment>(*apvts.getParameter("reverse_mix"),   *reverseMixRelay,   nullptr);
    grainMutationAttachment = std::make_unique<juce::WebSliderParameterAttachment>(*apvts.getParameter("grain_mutation"), *grainMutationRelay, nullptr);
    freqSpreadAttachment   = std::make_unique<juce::WebSliderParameterAttachment>(*apvts.getParameter("freq_spread"),   *freqSpreadRelay,   nullptr);
    grainRandomAttachment  = std::make_unique<juce::WebSliderParameterAttachment>(*apvts.getParameter("grain_random"),  *grainRandomRelay,  nullptr);
    dryWetAttachment       = std::make_unique<juce::WebSliderParameterAttachment>(*apvts.getParameter("dry_wet"),       *dryWetRelay,       nullptr);
    outputGainAttachment   = std::make_unique<juce::WebSliderParameterAttachment>(*apvts.getParameter("output_gain"),   *outputGainRelay,   nullptr);

    // Bool attachments (5)
    delaySyncToggleAttachment     = std::make_unique<juce::WebToggleButtonParameterAttachment>(*apvts.getParameter("delay_sync_toggle"),      *delaySyncToggleRelay,     nullptr);
    grainRateSyncToggleAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment>(*apvts.getParameter("grain_rate_sync_toggle"),  *grainRateSyncToggleRelay, nullptr);
    freezeAttachment              = std::make_unique<juce::WebToggleButtonParameterAttachment>(*apvts.getParameter("freeze"),                  *freezeRelay,              nullptr);
    delayOnAttachment             = std::make_unique<juce::WebToggleButtonParameterAttachment>(*apvts.getParameter("delay_on"),                *delayOnRelay,             nullptr);
    grainOnAttachment             = std::make_unique<juce::WebToggleButtonParameterAttachment>(*apvts.getParameter("grain_on"),                *grainOnRelay,             nullptr);

    // Choice attachments (3)
    delaySyncDivisionAttachment     = std::make_unique<juce::WebComboBoxParameterAttachment>(*apvts.getParameter("delay_sync_division"),      *delaySyncDivisionRelay,     nullptr);
    grainRateSyncDivisionAttachment = std::make_unique<juce::WebComboBoxParameterAttachment>(*apvts.getParameter("grain_rate_sync_division"),  *grainRateSyncDivisionRelay, nullptr);
    grainShapeAttachment            = std::make_unique<juce::WebComboBoxParameterAttachment>(*apvts.getParameter("grain_shape"),               *grainShapeRelay,            nullptr);

    // Add WebView to editor and navigate
    addAndMakeVisible(*webView);
    webView->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());

    setSize(900, 480);
}

DegrainEditor::~DegrainEditor()
{
}

void DegrainEditor::paint(juce::Graphics& g)
{
    // WebView handles all painting
    juce::ignoreUnused(g);
}

void DegrainEditor::resized()
{
    // WebView fills entire editor
    webView->setBounds(getLocalBounds());
}

std::optional<juce::WebBrowserComponent::Resource>
DegrainEditor::getResource(const juce::String& url)
{
    // Pattern 8: Explicit URL mapping (no generic loop)
    auto makeVector = [](const char* data, int size) {
        return std::vector<std::byte>(
            reinterpret_cast<const std::byte*>(data),
            reinterpret_cast<const std::byte*>(data) + size);
    };

    // Root / index.html
    if (url == "/" || url == "/index.html")
        return juce::WebBrowserComponent::Resource {
            makeVector(BinaryData::index_html, BinaryData::index_htmlSize),
            juce::String("text/html") };

    // JUCE frontend library (Pattern 21: ES6 module)
    if (url == "/js/juce/index.js")
        return juce::WebBrowserComponent::Resource {
            makeVector(BinaryData::index_js, BinaryData::index_jsSize),
            juce::String("text/javascript") };

    // Pattern 13: check_native_interop.js required for WebView
    if (url == "/js/juce/check_native_interop.js")
        return juce::WebBrowserComponent::Resource {
            makeVector(BinaryData::check_native_interop_js,
                      BinaryData::check_native_interop_jsSize),
            juce::String("text/javascript") };

    // Resource not found
    juce::Logger::writeToLog("Resource not found: " + url);
    return std::nullopt;
}
