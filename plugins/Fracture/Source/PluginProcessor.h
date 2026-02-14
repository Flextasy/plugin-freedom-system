#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <cmath>
#include <cstdint>

class FractureProcessor : public juce::AudioProcessor
{
public:
    FractureProcessor();
    ~FractureProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Fracture"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Public accessor for APVTS (required for WebView parameter binding)
    juce::AudioProcessorValueTreeState& getAPVTS() { return parameters; }

private:
    // APVTS with all 19 parameters
    juce::AudioProcessorValueTreeState parameters;

    // Parameter layout creation
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // =========================================================================
    // Phase 4.1: Core DSP Components
    // =========================================================================

    // --- Circular Delay Buffer ---
    static constexpr int kBufferSize = 1048576;  // 2^20 samples per channel
    static constexpr int kBufferMask = kBufferSize - 1;
    std::array<std::vector<float>, 2> delayBuffer;  // [channel][sample]
    int writePos = 0;

    // Hermite cubic interpolation for fractional delay reads
    static float hermiteInterp(float ym1, float y0, float y1, float y2, float frac);

    // Read from delay buffer with Hermite interpolation
    float readBufferHermite(int channel, float delaySamples) const;

    // --- Grain Engine ---
    static constexpr int kMaxGrains = 64;

    struct Grain
    {
        bool active = false;
        int startReadPos = 0;         // Buffer position (samples)
        int lengthSamples = 0;        // Grain duration
        int currentSample = 0;        // Playback position within grain
        float playbackRate = 1.0f;    // For pitch shift (1.0 = normal)
        float panL = 1.0f, panR = 1.0f; // Equal-power pan gains
        uint8_t windowType = 0;       // 0=Hann, 1=Triangle, 2=Square, 3=Ramp
        bool isReversed = false;      // Forward or reverse playback
        uint8_t mutationPass = 0;     // Feedback pass count (for Phase 4.3)
        float phase = 0.0f;           // 0.0 to 1.0 (for envelope)
    };

    std::array<Grain, kMaxGrains> grainPool;

    // Grain scheduler state
    float grainAccumulator = 0.0f;    // Sample accumulator for grain triggering

    // Spawn a new grain, stealing oldest if pool is full
    void spawnGrain(int grainLengthSamples, uint8_t windowType);

    // Apply grain window envelope
    static float applyWindow(float phase, uint8_t windowType, int lengthSamples, int currentSample);

    // --- Dry/Wet Mixer ---
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedDryWet;

    // --- Cached parameter pointers (avoid string lookups in processBlock) ---
    std::atomic<float>* delayTimeParam = nullptr;
    std::atomic<float>* grainSizeParam = nullptr;
    std::atomic<float>* grainRateParam = nullptr;
    std::atomic<float>* grainShapeParam = nullptr;
    std::atomic<float>* dryWetParam = nullptr;

    // --- Runtime state ---
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FractureProcessor)
};
