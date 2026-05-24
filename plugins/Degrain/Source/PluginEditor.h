#pragma once
#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>

class DegrainEditor : public juce::AudioProcessorEditor
{
public:
    explicit DegrainEditor(DegrainProcessor&);
    ~DegrainEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    DegrainProcessor& processorRef;

    // CRITICAL: Member declaration order prevents release build crashes
    // Destruction happens in REVERSE order of declaration
    // Order: Relays -> WebView -> Attachments

    // 1. RELAYS FIRST (no dependencies)
    // Float params (13) - WebSliderRelay
    std::unique_ptr<juce::WebSliderRelay> delayTimeRelay;
    std::unique_ptr<juce::WebSliderRelay> grainSizeRelay;
    std::unique_ptr<juce::WebSliderRelay> grainRateRelay;
    std::unique_ptr<juce::WebSliderRelay> feedbackRelay;
    std::unique_ptr<juce::WebSliderRelay> diffusionRelay;
    std::unique_ptr<juce::WebSliderRelay> stereoSpreadRelay;
    std::unique_ptr<juce::WebSliderRelay> detuneRelay;
    std::unique_ptr<juce::WebSliderRelay> lowCutRelay;
    std::unique_ptr<juce::WebSliderRelay> highCutRelay;
    std::unique_ptr<juce::WebSliderRelay> reverseMixRelay;
    std::unique_ptr<juce::WebSliderRelay> grainMutationRelay;
    std::unique_ptr<juce::WebSliderRelay> freqSpreadRelay;
    std::unique_ptr<juce::WebSliderRelay> grainRandomRelay;
    std::unique_ptr<juce::WebSliderRelay> dryWetRelay;
    std::unique_ptr<juce::WebSliderRelay> outputGainRelay;

    // Bool params (5) - WebToggleButtonRelay
    std::unique_ptr<juce::WebToggleButtonRelay> delaySyncToggleRelay;
    std::unique_ptr<juce::WebToggleButtonRelay> grainRateSyncToggleRelay;
    std::unique_ptr<juce::WebToggleButtonRelay> freezeRelay;
    std::unique_ptr<juce::WebToggleButtonRelay> delayOnRelay;
    std::unique_ptr<juce::WebToggleButtonRelay> grainOnRelay;

    // Choice params (3) - WebComboBoxRelay
    std::unique_ptr<juce::WebComboBoxRelay> delaySyncDivisionRelay;
    std::unique_ptr<juce::WebComboBoxRelay> grainRateSyncDivisionRelay;
    std::unique_ptr<juce::WebComboBoxRelay> grainShapeRelay;

    // 2. WEBVIEW SECOND (depends on relays via withOptionsFrom)
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // 3. ATTACHMENTS LAST (depend on both relays and parameters)
    // Float attachments (13)
    std::unique_ptr<juce::WebSliderParameterAttachment> delayTimeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> grainSizeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> grainRateAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> feedbackAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> diffusionAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> stereoSpreadAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> detuneAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> lowCutAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> highCutAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> reverseMixAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> grainMutationAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> freqSpreadAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> grainRandomAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> dryWetAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> outputGainAttachment;

    // Bool attachments (5)
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> delaySyncToggleAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> grainRateSyncToggleAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> freezeAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> delayOnAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> grainOnAttachment;

    // Choice attachments (3)
    std::unique_ptr<juce::WebComboBoxParameterAttachment> delaySyncDivisionAttachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> grainRateSyncDivisionAttachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> grainShapeAttachment;

    // Helper for resource serving
    std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String& url);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DegrainEditor)
};
