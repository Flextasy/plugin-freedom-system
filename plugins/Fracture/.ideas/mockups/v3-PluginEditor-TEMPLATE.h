// Fracture Plugin Editor — WebView Template (JUCE 8)
// Generated: 2026-02-14
// Plugin: Fracture — Granular Delay
// Window: 900x480 (not resizable)

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

class FractureEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    FractureEditor (FractureProcessor&);
    ~FractureEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    // Serve embedded HTML from BinaryData
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    FractureProcessor& processorRef;

    // CRITICAL: Member declaration order MUST be: Relays → WebView → Attachments

    // == RELAYS (19 parameters) ==
    juce::WebSliderRelay delayTimeRelay       { "delay_time" };
    juce::WebToggleButtonRelay delaySyncToggleRelay { "delay_sync_toggle" };
    juce::WebComboBoxRelay delaySyncDivisionRelay { "delay_sync_division" };

    juce::WebSliderRelay grainSizeRelay       { "grain_size" };
    juce::WebSliderRelay grainRateRelay       { "grain_rate" };
    juce::WebToggleButtonRelay grainRateSyncToggleRelay { "grain_rate_sync_toggle" };
    juce::WebComboBoxRelay grainRateSyncDivisionRelay { "grain_rate_sync_division" };
    juce::WebComboBoxRelay grainShapeRelay    { "grain_shape" };

    juce::WebSliderRelay feedbackRelay        { "feedback" };
    juce::WebSliderRelay diffusionRelay       { "diffusion" };
    juce::WebSliderRelay stereoSpreadRelay    { "stereo_spread" };
    juce::WebSliderRelay detuneRelay          { "detune" };
    juce::WebSliderRelay lowCutRelay          { "low_cut" };
    juce::WebSliderRelay highCutRelay         { "high_cut" };

    juce::WebSliderRelay reverseMixRelay      { "reverse_mix" };
    juce::WebSliderRelay grainMutationRelay   { "grain_mutation" };
    juce::WebSliderRelay freqSpreadRelay      { "freq_spread" };

    juce::WebToggleButtonRelay freezeRelay    { "freeze" };
    juce::WebSliderRelay dryWetRelay          { "dry_wet" };

    // == WEBVIEW ==
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // == ATTACHMENTS (19 parameters) ==
    std::unique_ptr<juce::ParameterAttachment> delayTimeAttachment;
    std::unique_ptr<juce::ParameterAttachment> delaySyncToggleAttachment;
    std::unique_ptr<juce::ParameterAttachment> delaySyncDivisionAttachment;

    std::unique_ptr<juce::ParameterAttachment> grainSizeAttachment;
    std::unique_ptr<juce::ParameterAttachment> grainRateAttachment;
    std::unique_ptr<juce::ParameterAttachment> grainRateSyncToggleAttachment;
    std::unique_ptr<juce::ParameterAttachment> grainRateSyncDivisionAttachment;
    std::unique_ptr<juce::ParameterAttachment> grainShapeAttachment;

    std::unique_ptr<juce::ParameterAttachment> feedbackAttachment;
    std::unique_ptr<juce::ParameterAttachment> diffusionAttachment;
    std::unique_ptr<juce::ParameterAttachment> stereoSpreadAttachment;
    std::unique_ptr<juce::ParameterAttachment> detuneAttachment;
    std::unique_ptr<juce::ParameterAttachment> lowCutAttachment;
    std::unique_ptr<juce::ParameterAttachment> highCutAttachment;

    std::unique_ptr<juce::ParameterAttachment> reverseMixAttachment;
    std::unique_ptr<juce::ParameterAttachment> grainMutationAttachment;
    std::unique_ptr<juce::ParameterAttachment> freqSpreadAttachment;

    std::unique_ptr<juce::ParameterAttachment> freezeAttachment;
    std::unique_ptr<juce::ParameterAttachment> dryWetAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FractureEditor)
};
