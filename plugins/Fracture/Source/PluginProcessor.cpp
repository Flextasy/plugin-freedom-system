#include "PluginProcessor.h"
#include "PluginEditor.h"

FractureProcessor::FractureProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

FractureProcessor::~FractureProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout FractureProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // DELAY SECTION
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"delay_time", 1},
        "Delay Time",
        juce::NormalisableRange<float>(1.0f, 2000.0f, 0.01f, 0.3f),
        250.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + " ms"; }));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"delay_sync_toggle", 1},
        "Delay Sync",
        false));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"delay_sync_division", 1},
        "Delay Sync Division",
        juce::StringArray{"1/1", "1/1D", "1/1T", "1/2", "1/2D", "1/2T",
                          "1/4", "1/4D", "1/4T", "1/8", "1/8D", "1/8T",
                          "1/16", "1/16D", "1/16T", "1/32", "1/32D", "1/32T"},
        6)); // Default: 1/4

    // GRAIN SECTION
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"grain_size", 1},
        "Grain Size",
        juce::NormalisableRange<float>(5.0f, 500.0f, 0.01f, 0.3f),
        80.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + " ms"; }));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"grain_rate", 1},
        "Grain Rate",
        juce::NormalisableRange<float>(0.1f, 100.0f, 0.01f, 0.3f),
        10.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + " Hz"; }));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"grain_rate_sync_toggle", 1},
        "Grain Rate Sync",
        false));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"grain_rate_sync_division", 1},
        "Grain Rate Sync Division",
        juce::StringArray{"1/1", "1/1D", "1/1T", "1/2", "1/2D", "1/2T",
                          "1/4", "1/4D", "1/4T", "1/8", "1/8D", "1/8T",
                          "1/16", "1/16D", "1/16T", "1/32", "1/32D", "1/32T"},
        9)); // Default: 1/8

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"grain_shape", 1},
        "Grain Shape",
        juce::StringArray{"Sine", "Triangle", "Square", "Ramp"},
        0)); // Default: Sine

    // CHARACTER SECTION
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"feedback", 1},
        "Feedback",
        juce::NormalisableRange<float>(0.0f, 120.0f, 0.1f),
        40.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + " %"; }));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"diffusion", 1},
        "Diffusion",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        30.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + " %"; }));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"stereo_spread", 1},
        "Stereo Spread",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        50.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + " %"; }));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"detune", 1},
        "Detune",
        juce::NormalisableRange<float>(-50.0f, 50.0f, 0.1f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + " ct"; }));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"low_cut", 1},
        "Low Cut",
        juce::NormalisableRange<float>(20.0f, 5000.0f, 0.01f, 0.25f),
        20.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) {
            return value < 1000.0f ? juce::String(value, 0) + " Hz"
                                   : juce::String(value / 1000.0f, 2) + " kHz";
        }));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"high_cut", 1},
        "High Cut",
        juce::NormalisableRange<float>(200.0f, 20000.0f, 0.01f, 0.25f),
        20000.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) {
            return value < 1000.0f ? juce::String(value, 0) + " Hz"
                                   : juce::String(value / 1000.0f, 2) + " kHz";
        }));

    // FRACTURE SECTION
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"reverse_mix", 1},
        "Reverse Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + " %"; }));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"grain_mutation", 1},
        "Grain Mutation",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + " %"; }));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"freq_spread", 1},
        "Frequency-Dependent Spread",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + " %"; }));

    // GLOBAL
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"freeze", 1},
        "Freeze",
        false));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"dry_wet", 1},
        "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        50.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + " %"; }));

    return layout;
}

void FractureProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Stage 1: Empty preparation (will be implemented in Stage 2+)
    juce::ignoreUnused(sampleRate, samplesPerBlock);
}

void FractureProcessor::releaseResources()
{
    // Stage 1: No resources to release yet
}

void FractureProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    // Stage 1: Pass-through audio (no processing yet)
    // Audio flows through unchanged - DSP implementation will come in Stage 2+
}

juce::AudioProcessorEditor* FractureProcessor::createEditor()
{
    return new FractureEditor(*this);
}

void FractureProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Save APVTS state
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void FractureProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // Restore APVTS state
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FractureProcessor();
}
