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
    void spawnGrain(int grainLengthSamples, uint8_t windowType,
                    float spreadAmount, float detuneCents);

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

    // =========================================================================
    // Phase 4.2: Feedback & Character Components
    // =========================================================================

    // --- DC Blocker (1-pole HPF at ~5 Hz, R = 0.995) ---
    struct DCBlocker
    {
        float x1 = 0.0f, y1 = 0.0f;
        static constexpr float R = 0.995f;

        float process(float input)
        {
            float output = input - x1 + R * y1;
            x1 = input;
            y1 = output;
            return output;
        }

        void reset()
        {
            x1 = 0.0f;
            y1 = 0.0f;
        }
    };

    // --- 1-Pole Filter (6 dB/oct) ---
    struct OnePoleFilter
    {
        float z1 = 0.0f;
        float alpha = 1.0f;

        void setLowpass(float cutoffHz, float sampleRate)
        {
            float dt = 1.0f / sampleRate;
            float rc = 1.0f / (2.0f * juce::MathConstants<float>::pi * cutoffHz);
            alpha = dt / (rc + dt);
        }

        float processLowpass(float input)
        {
            z1 = input * alpha + z1 * (1.0f - alpha);
            return z1;
        }

        float processHighpass(float input)
        {
            return input - processLowpass(input);
        }

        void reset()
        {
            z1 = 0.0f;
        }
    };

    // --- Schroeder Allpass Stage ---
    struct AllpassStage
    {
        static constexpr int kMaxAllpassDelay = 2048;  // Enough for scaled primes
        static constexpr int kAllpassMask = kMaxAllpassDelay - 1;

        std::array<float, kMaxAllpassDelay> delayLine {};
        int apWritePos = 0;
        int delaySamples = 0;
        float coefficient = 0.0f;

        void init(int delaySamps)
        {
            delaySamples = juce::jlimit(1, kMaxAllpassDelay - 1, delaySamps);
            delayLine.fill(0.0f);
            apWritePos = 0;
        }

        float process(float input)
        {
            float delayed = delayLine[static_cast<size_t>((apWritePos - delaySamples) & kAllpassMask)];
            float v = input - coefficient * delayed;
            float output = coefficient * v + delayed;
            delayLine[static_cast<size_t>(apWritePos & kAllpassMask)] = v;
            apWritePos = (apWritePos + 1) & kAllpassMask;
            return output;
        }

        void reset()
        {
            delayLine.fill(0.0f);
            apWritePos = 0;
        }
    };

    // --- Soft clipper ---
    static float softClip(float input) { return std::tanh(input); }

    // --- Feedback path components (per channel) ---
    std::array<DCBlocker, 2> dcBlocker;
    std::array<OnePoleFilter, 2> feedbackHP;
    std::array<OnePoleFilter, 2> feedbackLP;

    // Smoothed feedback gain
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedFeedback;

    // --- Allpass diffusion network (4 stages x 2 channels) ---
    static constexpr int kNumAllpassStages = 4;
    std::array<std::array<AllpassStage, kNumAllpassStages>, 2> allpassNetwork;

    // Base allpass delay times at 48kHz (prime numbers)
    static constexpr int kAllpassDelays48k[4] = { 113, 337, 601, 1009 };

    // Process diffusion network for one sample (both channels)
    void processDiffusion(float& sampleL, float& sampleR, float diffusionAmount);

    // --- Grain seed counter for deterministic hashing ---
    uint32_t grainSeedCounter = 0;

    // --- Cached parameter pointers for Phase 4.2 ---
    std::atomic<float>* feedbackParam   = nullptr;
    std::atomic<float>* diffusionParam  = nullptr;
    std::atomic<float>* stereoSpreadParam = nullptr;
    std::atomic<float>* detuneParam     = nullptr;
    std::atomic<float>* lowCutParam     = nullptr;
    std::atomic<float>* highCutParam    = nullptr;

    // =========================================================================
    // Phase 4.3: Advanced Features (Unique Selling Points)
    // =========================================================================

    // --- Cached parameter pointers for Phase 4.3 ---
    std::atomic<float>* reverseMixParam       = nullptr;
    std::atomic<float>* grainMutationParam    = nullptr;
    std::atomic<float>* freqSpreadParam       = nullptr;
    std::atomic<float>* freezeParam           = nullptr;
    std::atomic<float>* delaySyncToggleParam  = nullptr;
    std::atomic<float>* delaySyncDivParam     = nullptr;
    std::atomic<float>* grainRateSyncToggleParam = nullptr;
    std::atomic<float>* grainRateSyncDivParam    = nullptr;

    // --- Pass Count Buffer (for Grain Mutation) ---
    std::array<std::vector<uint8_t>, 2> passCountBuffer;  // [channel][sample]

    // --- Freeze System ---
    float freezeGain = 1.0f;        // Current crossfade gain (0=frozen, 1=writing)
    float freezeTargetGain = 1.0f;  // Target gain
    float freezeFadeRate = 0.0f;    // Per-sample increment toward target (10ms ramp)
    bool  midiFreeze = false;       // MIDI note-on/off freeze toggle state

    // --- Tempo Sync ---
    double currentBPM = 120.0;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedBPM;

    // Note division multipliers (beats per note)
    static constexpr float kDivisionMultipliers[18] = {
        4.0f,           // 1/1
        6.0f,           // 1/1D
        8.0f / 3.0f,    // 1/1T
        2.0f,           // 1/2
        3.0f,           // 1/2D
        4.0f / 3.0f,    // 1/2T
        1.0f,           // 1/4
        1.5f,           // 1/4D
        2.0f / 3.0f,    // 1/4T
        0.5f,           // 1/8
        0.75f,          // 1/8D
        1.0f / 3.0f,    // 1/8T
        0.25f,          // 1/16
        0.375f,         // 1/16D
        1.0f / 6.0f,    // 1/16T
        0.125f,         // 1/32
        0.1875f,        // 1/32D
        1.0f / 12.0f    // 1/32T
    };

    // --- Frequency-Dependent Grain Size ---
    // Simple biquad-based crossover using cascaded 1-pole filters
    // Low band: <250 Hz, Mid: 250-2500 Hz, High: >2500 Hz
    struct BandSplitter
    {
        OnePoleFilter lowSplit;   // 250 Hz crossover
        OnePoleFilter highSplit;  // 2500 Hz crossover

        void init(float sampleRate)
        {
            lowSplit.setLowpass(250.0f, sampleRate);
            highSplit.setLowpass(2500.0f, sampleRate);
            lowSplit.reset();
            highSplit.reset();
        }

        // Returns low, mid, high band samples
        void process(float input, float& low, float& mid, float& high)
        {
            low = lowSplit.processLowpass(input);
            float aboveLow = input - low;
            float midLow = highSplit.processLowpass(aboveLow);
            high = aboveLow - midLow;
            mid = midLow;
        }

        void reset()
        {
            lowSplit.reset();
            highSplit.reset();
        }
    };

    std::array<BandSplitter, 2> bandSplitter;

    // RMS energy accumulators (~100ms window via exponential decay)
    struct BandEnergy
    {
        float lowRMS  = 0.0f;
        float midRMS  = 0.0f;
        float highRMS = 0.0f;
        float alpha   = 0.0f;  // Exponential decay coefficient

        void init(float sampleRate)
        {
            // ~100ms time constant: alpha = 1 - exp(-1/(sr*0.1))
            alpha = 1.0f - std::exp(-1.0f / (sampleRate * 0.1f));
            lowRMS = 0.0f;
            midRMS = 0.0f;
            highRMS = 0.0f;
        }

        void update(float low, float mid, float high)
        {
            lowRMS  += alpha * (low  * low  - lowRMS);
            midRMS  += alpha * (mid  * mid  - midRMS);
            highRMS += alpha * (high * high - highRMS);
        }
    };

    std::array<BandEnergy, 2> bandEnergy;

    // Updated spawnGrain signature for Phase 4.3
    void spawnGrainAdvanced(int grainLengthSamples, uint8_t windowType,
                            float spreadAmount, float detuneCents,
                            float reverseMix, float mutationAmount,
                            float freqSpread, float lowE, float midE, float highE);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FractureProcessor)
};
