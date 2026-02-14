#pragma once
#include "PluginProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>

class FractureEditor : public juce::AudioProcessorEditor
{
public:
    FractureEditor(FractureProcessor&);
    ~FractureEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    FractureProcessor& processorRef;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FractureEditor)
};
