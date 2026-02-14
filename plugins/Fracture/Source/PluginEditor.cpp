#include "PluginEditor.h"

FractureEditor::FractureEditor(FractureProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    // Stage 1: Minimal black window
    setSize(900, 480);
}

FractureEditor::~FractureEditor()
{
}

void FractureEditor::paint(juce::Graphics& g)
{
    // Stage 1: Simple black background
    g.fillAll(juce::Colours::black);
}

void FractureEditor::resized()
{
    // Stage 1: No components to layout yet
}
