#include "PluginProcessor.h"
#include "PluginEditor.h"

DegrainProcessor::DegrainProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    // Cache raw parameter pointers (thread-safe atomic reads in processBlock)
    delayTimeParam    = parameters.getRawParameterValue("delay_time");
    grainSizeParam    = parameters.getRawParameterValue("grain_size");
    grainRateParam    = parameters.getRawParameterValue("grain_rate");
    grainShapeParam   = parameters.getRawParameterValue("grain_shape");
    dryWetParam       = parameters.getRawParameterValue("dry_wet");
    outputGainParam   = parameters.getRawParameterValue("output_gain");

    // Phase 4.2 parameter pointers
    feedbackParam     = parameters.getRawParameterValue("feedback");
    diffusionParam    = parameters.getRawParameterValue("diffusion");
    stereoSpreadParam = parameters.getRawParameterValue("stereo_spread");
    detuneParam       = parameters.getRawParameterValue("detune");
    lowCutParam       = parameters.getRawParameterValue("low_cut");
    highCutParam      = parameters.getRawParameterValue("high_cut");

    // Phase 4.3 parameter pointers
    reverseMixParam          = parameters.getRawParameterValue("reverse_mix");
    grainMutationParam       = parameters.getRawParameterValue("grain_mutation");
    freqSpreadParam          = parameters.getRawParameterValue("freq_spread");
    grainRandomParam         = parameters.getRawParameterValue("grain_random");
    freezeParam              = parameters.getRawParameterValue("freeze");
    delaySyncToggleParam     = parameters.getRawParameterValue("delay_sync_toggle");
    delaySyncDivParam        = parameters.getRawParameterValue("delay_sync_division");
    delayRatioParam          = parameters.getRawParameterValue("delay_ratio");
    grainRateSyncToggleParam = parameters.getRawParameterValue("grain_rate_sync_toggle");
    grainRateSyncDivParam    = parameters.getRawParameterValue("grain_rate_sync_division");
    delayOnParam             = parameters.getRawParameterValue("delay_on");
    grainOnParam             = parameters.getRawParameterValue("grain_on");
}

DegrainProcessor::~DegrainProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout DegrainProcessor::createParameterLayout()
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

    // Grain/Delay balance: 0% = all delay, 100% = all grains, 50% = equal.
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"delay_ratio", 1},
        "Ratio",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        50.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { int g = juce::roundToInt(value); return juce::String(g) + "/" + juce::String(100 - g); }));

    // GRAIN SECTION
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"grain_size", 1},
        "Grain Size",
        juce::NormalisableRange<float>(5.0f, 500.0f, 0.01f, 0.3f),
        50.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + " ms"; }));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"grain_rate", 1},
        "Grain Rate",
        juce::NormalisableRange<float>(0.5f, 100.0f, 0.01f, 0.3f),
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

    // DEGRAIN SECTION
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

    // Grain Random: randomises each grain's read position in the buffer.
    // 0% = all grains read the same recent window (ordered); 100% = each grain
    // jumps to a random point in recent history → scrambled playback order.
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"grain_random", 1},
        "Grain Random",
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

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"output_gain", 1},
        "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + " dB"; }));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"delay_on", 1},
        "Delay On",
        true));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"grain_on", 1},
        "Grain On",
        true));

    return layout;
}

// =============================================================================
// Phase 4.1: Hermite Interpolation
// =============================================================================

float DegrainProcessor::hermiteInterp(float ym1, float y0, float y1, float y2, float frac)
{
    float c0 = y0;
    float c1 = 0.5f * (y1 - ym1);
    float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
    float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
    return c0 + frac * (c1 + frac * (c2 + frac * c3));
}

float DegrainProcessor::readBufferHermite(int channel, float delaySamples) const
{
    // Calculate integer and fractional parts of the read position
    float readPosFloat = static_cast<float>(writePos) - delaySamples;
    if (readPosFloat < 0.0f)
        readPosFloat += static_cast<float>(kBufferSize);

    int readPosInt = static_cast<int>(readPosFloat);
    float frac = readPosFloat - static_cast<float>(readPosInt);

    // Four sample points for Hermite interpolation
    const auto& buf = delayBuffer[static_cast<size_t>(channel)];
    float ym1 = buf[static_cast<size_t>((readPosInt - 1) & kBufferMask)];
    float y0  = buf[static_cast<size_t>((readPosInt)     & kBufferMask)];
    float y1  = buf[static_cast<size_t>((readPosInt + 1) & kBufferMask)];
    float y2  = buf[static_cast<size_t>((readPosInt + 2) & kBufferMask)];

    return hermiteInterp(ym1, y0, y1, y2, frac);
}

float DegrainProcessor::readBufferHermiteAbs(int channel, float absPos) const
{
    // Read from an ABSOLUTE buffer position (for fixed-snapshot grain reads)
    // absPos is pre-wrapped to [0, kBufferSize)
    int readPosInt = static_cast<int>(absPos) & kBufferMask;
    float frac = absPos - std::floor(absPos);

    const auto& buf = delayBuffer[static_cast<size_t>(channel)];
    float ym1 = buf[static_cast<size_t>((readPosInt - 1 + kBufferSize) & kBufferMask)];
    float y0  = buf[static_cast<size_t>(readPosInt)];
    float y1  = buf[static_cast<size_t>((readPosInt + 1) & kBufferMask)];
    float y2  = buf[static_cast<size_t>((readPosInt + 2) & kBufferMask)];

    return hermiteInterp(ym1, y0, y1, y2, frac);
}

// =============================================================================
// Phase 4.1: Grain Windowing
// =============================================================================

float DegrainProcessor::applyWindow(float phase, uint8_t windowType, int lengthSamples, int currentSample)
{
    switch (windowType)
    {
        case 0: // Hann
            return 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * phase));

        case 1: // Triangle
            return (phase < 0.5f) ? (2.0f * phase) : (2.0f * (1.0f - phase));

        case 2: // Square with cosine soft edges (~5% fade at each end)
        {
            int fadeLen = juce::jmax(4, lengthSamples / 20); // ~5% fade at each end
            if (currentSample < fadeLen)
            {
                // Cosine fade-in: 0 -> 1 over fadeLen samples
                float t = static_cast<float>(currentSample) / static_cast<float>(fadeLen);
                return 0.5f * (1.0f - std::cos(juce::MathConstants<float>::pi * t));
            }
            if (currentSample >= (lengthSamples - fadeLen))
            {
                // Cosine fade-out: 1 -> 0 over fadeLen samples
                float t = static_cast<float>(lengthSamples - 1 - currentSample) / static_cast<float>(fadeLen);
                return 0.5f * (1.0f - std::cos(juce::MathConstants<float>::pi * t));
            }
            return 1.0f;
        }

        case 3: // Ramp (decay) with cosine fade-in (~5% of grain)
        {
            int fadeLen = juce::jmax(4, lengthSamples / 20);
            float ramp = 1.0f - phase;
            if (currentSample < fadeLen)
            {
                float t = static_cast<float>(currentSample) / static_cast<float>(fadeLen);
                float fadeIn = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::pi * t));
                return ramp * fadeIn;
            }
            return ramp;
        }

        default:
            return 1.0f;
    }
}

// =============================================================================
// Phase 4.1: Grain Spawning
// =============================================================================

void DegrainProcessor::spawnGrain(int grainLengthSamples, uint8_t windowType,
                                   float spreadAmount, float detuneCents)
{
    // Find an inactive grain slot
    int slotIndex = -1;
    for (int i = 0; i < kMaxGrains; ++i)
    {
        if (!grainPool[static_cast<size_t>(i)].active)
        {
            slotIndex = i;
            break;
        }
    }

    // If no free slot, steal the grain with the LOWEST current envelope amplitude.
    // This minimises the click: stealing a near-silent grain creates no discontinuity,
    // whereas stealing the "highest-phase" grain can still be mid-amplitude (Square window).
    if (slotIndex < 0)
    {
        float lowestEnv = std::numeric_limits<float>::max();
        for (int i = 0; i < kMaxGrains; ++i)
        {
            auto& g = grainPool[static_cast<size_t>(i)];
            float fractPos = static_cast<float>(g.currentSample) * g.playbackRate;
            float env = applyWindow(g.phase, g.windowType, g.lengthSamples,
                                    static_cast<int>(fractPos));
            if (env < lowestEnv)
            {
                lowestEnv = env;
                slotIndex = i;
            }
        }
    }

    if (slotIndex < 0)
        return; // Should never happen with the stealing logic above

    // Deterministic hash for this grain (Knuth multiplicative hash)
    uint32_t seed = grainSeedCounter++;
    uint32_t hash = seed * 2654435761u;

    auto& grain = grainPool[static_cast<size_t>(slotIndex)];
    grain.active = true;
    grain.startReadPos = writePos;  // Read from current write position
    grain.lengthSamples = juce::jmax(1, grainLengthSamples);
    grain.currentSample = 0;
    grain.windowType = windowType;
    grain.isReversed = false;       // Phase 4.1: forward only (Phase 4.3 will add reverse)
    grain.mutationPass = 0;
    grain.phase = 0.0f;

    // --- Phase 4.2: Stereo Spread ---
    // spreadAmount: 0.0 = center, 1.0 = full stereo
    // Hash gives a value 0..UINT32_MAX, map to pan angle
    float panNorm = static_cast<float>(hash & 0xFFFFu) / 65535.0f;  // 0.0 to 1.0
    float panAngle = (0.5f + (panNorm - 0.5f) * spreadAmount)       // Scale around center
                     * (juce::MathConstants<float>::pi * 0.5f);      // Map to 0..pi/2
    grain.panL = std::cos(panAngle);
    grain.panR = std::sin(panAngle);

    // --- Phase 4.2: Detune ---
    // detuneCents: ± range in cents. Random within range per grain.
    // Use upper 16 bits of hash for detune randomization
    float detuneNorm = static_cast<float>((hash >> 16) & 0xFFFFu) / 65535.0f; // 0.0 to 1.0
    float centsOffset = (detuneNorm * 2.0f - 1.0f) * detuneCents;  // -detuneCents to +detuneCents
    grain.playbackRate = std::pow(2.0f, centsOffset / 1200.0f);
}

// =============================================================================
// Phase 4.3: Advanced Grain Spawning (reverse, mutation, freq-dependent sizing)
// =============================================================================

void DegrainProcessor::spawnGrainAdvanced(int startReadPos, int grainLengthSamples, uint8_t windowType,
                                            float spreadAmount, float detuneCents,
                                            float reverseMix, float mutationAmount,
                                            float freqSpread, float lowE, float midE, float highE,
                                            float randomAmount)
{
    // Find an inactive grain slot
    int slotIndex = -1;
    for (int i = 0; i < kMaxGrains; ++i)
    {
        if (!grainPool[static_cast<size_t>(i)].active)
        {
            slotIndex = i;
            break;
        }
    }

    // If no free slot, steal the grain with the LOWEST current envelope amplitude.
    // Minimises the click: a near-silent grain causes no audible discontinuity.
    if (slotIndex < 0)
    {
        float lowestEnv = std::numeric_limits<float>::max();
        for (int i = 0; i < kMaxGrains; ++i)
        {
            auto& g = grainPool[static_cast<size_t>(i)];
            float fractPos = static_cast<float>(g.currentSample) * g.playbackRate;
            float env = applyWindow(g.phase, g.windowType, g.lengthSamples,
                                    static_cast<int>(fractPos));
            if (env < lowestEnv)
            {
                lowestEnv = env;
                slotIndex = i;
            }
        }
    }

    if (slotIndex < 0)
        return;

    // Deterministic hash for this grain (Knuth multiplicative hash)
    uint32_t seed = grainSeedCounter++;
    uint32_t hash = seed * 2654435761u;

    auto& grain = grainPool[static_cast<size_t>(slotIndex)];
    grain.active = true;
    // Fixed-snapshot read position: pre-computed by caller (writePos - delaySamples, or frozen snap)
    // --- Grain Random: scatter the read position backward into recent history ---
    // Each grain picks a random point up to kRandomScatterSeconds back, scaled by
    // randomAmount. Scatter is always backward, so the grain's forward playback
    // window [start, start+len] stays within already-written samples (valid read).
    if (randomAmount > 0.001f)
    {
        // Independent random from the grain hash (different mix than pan/detune bits)
        uint32_t rh = (hash ^ 0x9E3779B9u) * 2246822519u;
        float rand01 = static_cast<float>((rh >> 9) & 0xFFFFu) / 65535.0f;  // 0..1
        constexpr float kRandomScatterSeconds = 1.5f;
        int maxScatter = static_cast<int>(static_cast<float>(currentSampleRate) * kRandomScatterSeconds);
        int scatterOffset = static_cast<int>(randomAmount * rand01 * static_cast<float>(maxScatter));
        startReadPos = (startReadPos - scatterOffset + kBufferSize) & kBufferMask;
    }
    grain.startReadPos = startReadPos;
    grain.windowType = windowType;
    grain.currentSample = 0;
    grain.phase = 0.0f;

    // --- Phase 4.3: Frequency-Dependent Grain Size (ENHANCED: ×6 low, ×0.1 high) ---
    float effectiveLength = static_cast<float>(grainLengthSamples);
    if (freqSpread > 0.001f)
    {
        float totalEnergy = lowE + midE + highE + 1.0e-10f;
        float lowWeight  = lowE  / totalEnergy;
        float midWeight  = midE  / totalEnergy;
        float highWeight = highE / totalEnergy;

        // More extreme scales for clear audibility
        float lowScale  = juce::jmap(freqSpread, 1.0f, 6.0f);   // Low freqs: up to 6× longer grains
        float highScale = juce::jmap(freqSpread, 1.0f, 0.1f);   // High freqs: down to 10% grain size
        float midScale  = 1.0f;

        float weightedScale = lowWeight * lowScale + midWeight * midScale + highWeight * highScale;
        effectiveLength *= weightedScale;
    }

    // --- Phase 4.3: Grain Mutation (random per grain: ±50% size, ±200 cents pitch) ---
    grain.mutationPass = 0;

    float sizeScale = 1.0f;
    if (mutationAmount > 0.001f)
    {
        uint32_t sizeHash = (hash * 2246822519u) >> 8;
        float sizeRand = static_cast<float>(sizeHash & 0xFFFFu) / 65535.0f; // 0 to 1
        // Scale randomly between [1 - 0.5*m, 1 + 0.5*m]
        sizeScale = 1.0f + (sizeRand * 2.0f - 1.0f) * 0.5f * mutationAmount;
        sizeScale = juce::jlimit(0.1f, 2.0f, sizeScale);
        effectiveLength *= sizeScale;
    }
    grain.sizeScale = sizeScale;

    // Min grain size: 5ms equivalent
    float minSamples = static_cast<float>(currentSampleRate) * 0.005f;
    effectiveLength = juce::jmax(effectiveLength, minSamples);
    grain.lengthSamples = juce::jmax(1, static_cast<int>(effectiveLength));

    // --- Phase 4.3: Reverse Grain Mix ---
    float hashFloat = static_cast<float>((hash >> 8) & 0xFFFFu) / 65535.0f;
    grain.isReversed = (hashFloat < (reverseMix * 0.01f));

    // --- Phase 4.2: Stereo Spread ---
    float panNorm = static_cast<float>(hash & 0xFFFFu) / 65535.0f;
    float panAngle = (0.5f + (panNorm - 0.5f) * spreadAmount)
                     * (juce::MathConstants<float>::pi * 0.5f);
    grain.panL = std::cos(panAngle);
    grain.panR = std::sin(panAngle);

    // --- Phase 4.2 + 4.3: Detune + Mutation Pitch Drift (ENHANCED: ±200 cents) ---
    float detuneNorm = static_cast<float>((hash >> 16) & 0xFFFFu) / 65535.0f;
    float centsOffset = (detuneNorm * 2.0f - 1.0f) * detuneCents;

    // Mutation pitch drift: ±200 cents at full mutation
    if (mutationAmount > 0.001f)
    {
        uint32_t hash2 = (hash >> 4) * 2246822519u;
        float randomBipolar = (static_cast<float>(hash2 & 0xFFFFu) / 65535.0f) * 2.0f - 1.0f;
        float pitchDrift = 200.0f * mutationAmount * randomBipolar;
        centsOffset += pitchDrift;
    }

    grain.playbackRate = std::pow(2.0f, centsOffset / 1200.0f);
}

// =============================================================================
// Phase 4.2: Diffusion Network
// =============================================================================

void DegrainProcessor::processDiffusion(float& sampleL, float& sampleR, float diffusionAmount)
{
    // Bypass when diffusion is negligible
    if (diffusionAmount < 0.05f)
        return;

    // Coefficient: diffusionAmount * 0.75 (max coefficient = 0.75 at 100%)
    float coeff = diffusionAmount * 0.75f;

    // Process 4 cascaded allpass stages per channel
    for (int stage = 0; stage < kNumAllpassStages; ++stage)
    {
        allpassNetwork[0][static_cast<size_t>(stage)].coefficient = coeff;
        allpassNetwork[1][static_cast<size_t>(stage)].coefficient = coeff;
        sampleL = allpassNetwork[0][static_cast<size_t>(stage)].process(sampleL);
        sampleR = allpassNetwork[1][static_cast<size_t>(stage)].process(sampleR);
    }
}

// =============================================================================
// Phase 4.1: prepareToPlay
// =============================================================================

void DegrainProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    currentSampleRate = sampleRate;

    // Pre-allocate delay buffers (power-of-2 sized, per channel)
    for (auto& channelBuf : delayBuffer)
    {
        channelBuf.resize(static_cast<size_t>(kBufferSize), 0.0f);
        std::fill(channelBuf.begin(), channelBuf.end(), 0.0f);
    }
    for (auto& channelBuf : wetDelayBuffer)
    {
        channelBuf.resize(static_cast<size_t>(kBufferSize), 0.0f);
        std::fill(channelBuf.begin(), channelBuf.end(), 0.0f);
    }
    writePos = 0;

    // Initialize grain pool
    for (auto& grain : grainPool)
    {
        grain.active = false;
        grain.phase = 0.0f;
        grain.currentSample = 0;
    }
    grainAccumulator = 0.0f;

    // Initialize smoothed dry/wet (20ms ramp)
    smoothedDryWet.reset(sampleRate, 0.020);
    smoothedDryWet.setCurrentAndTargetValue(dryWetParam->load() * 0.01f);

    // =========================================================================
    // Phase 4.2: Initialize feedback & character components
    // =========================================================================

    // Smoothed feedback gain (20ms ramp)
    smoothedFeedback.reset(sampleRate, 0.020);
    smoothedFeedback.setCurrentAndTargetValue(feedbackParam->load() * 0.01f);

    // Smoothed diffusion (20ms ramp - prevents allpass coefficient jump → click)
    smoothedDiffusion.reset(sampleRate, 0.020);
    smoothedDiffusion.setCurrentAndTargetValue(diffusionParam->load() * 0.01f);

    // Smoothed grain/delay balance (20ms ramp - click-free crossfade)
    smoothedRatio.reset(sampleRate, 0.020);
    smoothedRatio.setCurrentAndTargetValue(delayRatioParam->load() * 0.01f);

    // DC blockers
    for (auto& dc : dcBlocker)
        dc.reset();

    // Feedback filters (24dB/oct) and wet output filters
    float lowCutHz  = lowCutParam->load();
    float highCutHz = highCutParam->load();
    for (int ch = 0; ch < 2; ++ch)
    {
        feedbackHP[static_cast<size_t>(ch)].reset();
        feedbackLP[static_cast<size_t>(ch)].reset();
        feedbackHP[static_cast<size_t>(ch)].setLowpass(lowCutHz, static_cast<float>(sampleRate));
        feedbackLP[static_cast<size_t>(ch)].setLowpass(highCutHz, static_cast<float>(sampleRate));

        wetHP[static_cast<size_t>(ch)].reset();
        wetLP[static_cast<size_t>(ch)].reset();
        wetHP[static_cast<size_t>(ch)].setLowpass(lowCutHz, static_cast<float>(sampleRate));
        wetLP[static_cast<size_t>(ch)].setLowpass(highCutHz, static_cast<float>(sampleRate));
    }

    // Reset feedback limiter state
    fbLimiterEnv[0]  = fbLimiterEnv[1]  = 0.0f;
    fbLimiterGain[0] = fbLimiterGain[1] = 1.0f;

    // Exponential follower for delay time: init both current and target to avoid initial glide
    {
        float initDelayMs = delayTimeParam->load();
        float initDelaySamples = juce::jmin((initDelayMs * 0.001f) * static_cast<float>(sampleRate),
                                            static_cast<float>(kBufferSize - 4));
        delaySamplesCurrent = initDelaySamples;
        delaySamplesTarget  = initDelaySamples;
    }

    // Allpass diffusion network: scale delay times for current sample rate
    float srRatio = static_cast<float>(sampleRate) / 48000.0f;
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int stage = 0; stage < kNumAllpassStages; ++stage)
        {
            int scaledDelay = juce::jmax(1, static_cast<int>(
                static_cast<float>(kAllpassDelays48k[stage]) * srRatio));
            allpassNetwork[static_cast<size_t>(ch)][static_cast<size_t>(stage)].init(scaledDelay);
        }
    }

    // Reset grain seed counter
    grainSeedCounter = 0;

    // =========================================================================
    // Phase 4.3: Initialize advanced feature components
    // =========================================================================

    // Pass count buffers (same size as delay buffer, per channel)
    for (auto& pcBuf : passCountBuffer)
    {
        pcBuf.resize(static_cast<size_t>(kBufferSize), 0);
        std::fill(pcBuf.begin(), pcBuf.end(), static_cast<uint8_t>(0));
    }

    // Freeze system
    freezeGain = 1.0f;
    freezeTargetGain = 1.0f;
    // 10ms crossfade rate
    freezeFadeRate = 1.0f / static_cast<float>(sampleRate * 0.010);
    midiFreeze = false;
    freezeSnapPos = 0;  // Will be updated per-sample when not frozen

    // Tempo sync: smoothed BPM (50ms multiplicative smoothing)
    smoothedBPM.reset(sampleRate, 0.050);
    smoothedBPM.setCurrentAndTargetValue(120.0f);
    currentBPM = 120.0;

    // Band splitter for frequency-dependent grain size
    for (auto& bs : bandSplitter)
        bs.init(static_cast<float>(sampleRate));

    // Band energy accumulators
    for (auto& be : bandEnergy)
        be.init(static_cast<float>(sampleRate));
}

void DegrainProcessor::releaseResources()
{
    // Clear delay buffers
    for (auto& channelBuf : delayBuffer)
        std::fill(channelBuf.begin(), channelBuf.end(), 0.0f);
    for (auto& channelBuf : wetDelayBuffer)
        std::fill(channelBuf.begin(), channelBuf.end(), 0.0f);

    // Clear pass count buffers
    for (auto& pcBuf : passCountBuffer)
        std::fill(pcBuf.begin(), pcBuf.end(), static_cast<uint8_t>(0));
}

// =============================================================================
// Phase 4.1: processBlock
// =============================================================================

void DegrainProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = juce::jmin(buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();

    if (numChannels == 0 || numSamples == 0)
        return;

    // =========================================================================
    // Read BPM, PPQ, and transport state from host AudioPlayHead
    // =========================================================================
    double blockStartPPQ = -1.0;
    bool isTransportPlaying = false;
    if (auto* playHead = getPlayHead())
    {
        if (auto posInfo = playHead->getPosition())
        {
            if (posInfo->getBpm().hasValue())
                currentBPM = *posInfo->getBpm();
            if (posInfo->getPpqPosition().hasValue())
                blockStartPPQ = *posInfo->getPpqPosition();
            isTransportPlaying = posInfo->getIsPlaying();
        }
    }
    // Clamp before setting: Multiplicative SmoothedValue with target=0 produces NaN
    // (pow(0/current, 1/N) = 0, then pow(target/0, 1/N) = Inf or NaN next block)
    smoothedBPM.setTargetValue(juce::jmax(20.0f, static_cast<float>(currentBPM)));

    // =========================================================================
    // Phase 4.3: Process MIDI for freeze toggle (note-on = freeze, note-off = unfreeze)
    // =========================================================================
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            midiFreeze = true;
        else if (msg.isNoteOff())
            midiFreeze = false;
    }

    // --- Block-rate parameter reads ---
    const float delayTimeMsParam = delayTimeParam->load();
    const float grainSizeMs      = grainSizeParam->load();
    const float grainRateHzParam = grainRateParam->load();
    const int   grainShapeIdx    = static_cast<int>(grainShapeParam->load());
    const float dryWetPct        = dryWetParam->load();

    // Output gain: dB → linear
    const float outputGain = std::pow(10.0f, outputGainParam->load() / 20.0f);
    const bool  grainOn    = (grainOnParam->load() >= 0.5f);
    const bool  delayOn    = (delayOnParam->load() >= 0.5f);

    // Phase 4.2 parameters
    const float feedbackPct   = feedbackParam->load();
    const float diffusionPct  = diffusionParam->load();
    const float spreadPct     = stereoSpreadParam->load();
    const float detuneCents   = detuneParam->load();
    const float lowCutHz      = lowCutParam->load();
    const float highCutHz     = highCutParam->load();

    // Phase 4.3 parameters
    const float reverseMix    = reverseMixParam->load();      // 0-100%
    const float mutationPct   = grainMutationParam->load();   // 0-100%
    const float freqSpreadPct = freqSpreadParam->load();      // 0-100%
    const bool  freezeOn      = (freezeParam->load() >= 0.5f) || midiFreeze;
    const bool  delaySyncOn   = (delaySyncToggleParam->load() >= 0.5f);
    const int   delaySyncDiv  = static_cast<int>(delaySyncDivParam->load());
    const bool  grainRateSyncOn = (grainRateSyncToggleParam->load() >= 0.5f);
    const int   grainRateSyncDiv = static_cast<int>(grainRateSyncDivParam->load());

    const float sr = static_cast<float>(currentSampleRate);
    const float mutationAmount = mutationPct * 0.01f;  // 0.0 to 1.0
    const float freqSpread     = freqSpreadPct * 0.01f; // 0.0 to 1.0
    const float grainRandom    = grainRandomParam->load() * 0.01f; // 0.0 to 1.0

    // =========================================================================
    // Phase 4.3: Tempo Sync - compute delay time and grain rate
    // =========================================================================
    float bpm = smoothedBPM.getNextValue();
    if (!(bpm >= 20.0f)) bpm = 120.0f;  // NaN-safe: NaN >= 20 is false → caught here

    float delayTimeMs = delayTimeMsParam;
    if (delaySyncOn)
    {
        int divIdx = juce::jlimit(0, 17, delaySyncDiv);
        delayTimeMs = (60000.0f / bpm) * kDivisionMultipliers[divIdx];
    }

    float grainRateHz = grainRateHzParam;
    if (grainRateSyncOn)
    {
        int divIdx = juce::jlimit(0, 17, grainRateSyncDiv);
        grainRateHz = bpm / (60.0f * kDivisionMultipliers[divIdx]);
    }

    // Convert parameters to sample-domain values
    // Clamp delaySamples: at slow BPM + long divisions it can exceed kBufferSize
    // Also guards against NaN/Inf if bpm was corrupted (static_cast<int>(NaN) = UB)
    const float rawDelaySamples = juce::jmin((delayTimeMs * 0.001f) * sr,
                                             static_cast<float>(kBufferSize - 4));
    delaySamplesTarget = rawDelaySamples;
    // alpha: time constant ~300ms → fast initial pitch glide that asymptotically slows to zero
    const float delayAlpha = 1.0f - std::exp(-1.0f / (sr * 0.300f));
    const int   grainLenSamples = juce::jmax(1, static_cast<int>((grainSizeMs * 0.001f) * sr));
    const float samplesPerGrain = (grainRateHz > 0.0f) ? (sr / grainRateHz) : sr;
    const uint8_t windowType    = static_cast<uint8_t>(juce::jlimit(0, 3, grainShapeIdx));

    // Phase 4.2: derived values
    const float spreadAmount   = spreadPct * 0.01f;       // 0.0 to 1.0
    const float diffusionAmt   = diffusionPct * 0.01f;    // 0.0 to 1.0
    const float feedbackTarget = juce::jmin(feedbackPct * 0.01f, 1.15f);

    // Set smoothed parameter targets
    smoothedDryWet.setTargetValue(dryWetPct * 0.01f);
    smoothedFeedback.setTargetValue(feedbackTarget);
    smoothedDiffusion.setTargetValue(diffusionAmt);
    smoothedRatio.setTargetValue(delayRatioParam->load() * 0.01f);

    // Update feedback and wet filter coefficients (block-rate)
    for (int ch = 0; ch < 2; ++ch)
    {
        feedbackHP[static_cast<size_t>(ch)].setLowpass(lowCutHz, sr);
        feedbackLP[static_cast<size_t>(ch)].setLowpass(highCutHz, sr);
        wetHP[static_cast<size_t>(ch)].setLowpass(lowCutHz, sr);
        wetLP[static_cast<size_t>(ch)].setLowpass(highCutHz, sr);
    }

    // =========================================================================
    // Phase 4.3: Freeze target gain
    // =========================================================================
    freezeTargetGain = freezeOn ? 0.0f : 1.0f;

    // Get buffer write pointers
    float* channelData[2] = { nullptr, nullptr };
    for (int ch = 0; ch < numChannels; ++ch)
        channelData[ch] = buffer.getWritePointer(ch);

    // --- Per-sample processing ---
    for (int samp = 0; samp < numSamples; ++samp)
    {
        // (1) Update smoothed params
        const float mixAmount    = smoothedDryWet.getNextValue();
        const float feedbackGain = smoothedFeedback.getNextValue();
        delaySamplesCurrent += delayAlpha * (delaySamplesTarget - delaySamplesCurrent);
        const float delaySamples = delaySamplesCurrent;

        // Phase 4.3: Update freeze crossfade (10ms linear ramp)
        if (freezeGain < freezeTargetGain)
        {
            freezeGain += freezeFadeRate;
            if (freezeGain > freezeTargetGain)
                freezeGain = freezeTargetGain;
        }
        else if (freezeGain > freezeTargetGain)
        {
            freezeGain -= freezeFadeRate;
            if (freezeGain < freezeTargetGain)
                freezeGain = freezeTargetGain;
        }

        // (2) Capture dry input
        float dryL = channelData[0][samp];
        float dryR = (numChannels > 1) ? channelData[1][samp] : dryL;

        // Phase 4.3: Band energy analysis for frequency-dependent grain sizing
        float lowBand = 0.0f, midBand = 0.0f, highBand = 0.0f;
        float inputMono = (dryL + dryR) * 0.5f;  // Mono sum for analysis
        bandSplitter[0].process(inputMono, lowBand, midBand, highBand);
        bandEnergy[0].update(lowBand, midBand, highBand);

        // Get current band energies (RMS values) for grain spawn decisions
        float lowE  = bandEnergy[0].lowRMS;
        float midE  = bandEnergy[0].midRMS;
        float highE = bandEnergy[0].highRMS;

        // (3) Freeze snap: lock at grain-length offset from write position when not frozen
        if (!freezeOn)
            freezeSnapPos = (writePos - grainLenSamples + kBufferSize) & kBufferMask;

        // Grain scheduler:
        //   PPQ sync  → only fires when transport is playing (prevents spurious spawning when stopped)
        //   Free-run  → always fires (real-time recording works without transport)
        bool spawnGrainNow = false;
        if (grainOn)
        {
            if (grainRateSyncOn && isTransportPlaying && blockStartPPQ >= 0.0)
            {
                double ppqPerSample = currentBPM / (60.0 * currentSampleRate);
                double samplePPQ    = blockStartPPQ + static_cast<double>(samp) * ppqPerSample;
                double prevPPQ      = samplePPQ - ppqPerSample;
                double divBeats     = static_cast<double>(kDivisionMultipliers[grainRateSyncDiv]);
                spawnGrainNow = (std::floor(samplePPQ / divBeats) > std::floor(prevPPQ / divBeats));
            }
            else if (!grainRateSyncOn)
            {
                grainAccumulator += 1.0f;
                if (grainAccumulator >= samplesPerGrain)
                {
                    grainAccumulator -= samplesPerGrain;
                    spawnGrainNow = true;
                }
            }
            // grainRateSyncOn but transport not playing → no grains (silent until transport rolls)
        }

        if (spawnGrainNow)
        {
            spawnGrainAdvanced(freezeSnapPos, grainLenSamples, windowType,
                               spreadAmount, detuneCents,
                               reverseMix, mutationAmount, freqSpread,
                               lowE, midE, highE, grainRandom);
        }

        // (4) Process grain pool (fixed-snapshot reads from main delayBuffer)
        float wetL = 0.0f, wetR = 0.0f;
        float envSum = 0.0f;  // Sum of active grain envelopes → effective overlap count

        for (auto& grain : grainPool)
        {
            if (!grain.active) continue;

            float fractionalPos = static_cast<float>(grain.currentSample) * grain.playbackRate;
            grain.phase = fractionalPos / static_cast<float>(grain.lengthSamples);

            if (grain.phase >= 1.0f)
            {
                grain.active = false;
                continue;
            }

            float readPosRaw;
            if (grain.isReversed)
                readPosRaw = static_cast<float>(grain.startReadPos)
                             + static_cast<float>(grain.lengthSamples) - fractionalPos;
            else
                readPosRaw = static_cast<float>(grain.startReadPos) + fractionalPos;

            const float bufSizeF = static_cast<float>(kBufferSize);
            readPosRaw = std::fmod(readPosRaw + bufSizeF * 2.0f, bufSizeF);

            int fractionalPosInt = static_cast<int>(fractionalPos);
            float envelope = applyWindow(grain.phase, grain.windowType,
                                         grain.lengthSamples, fractionalPosInt);

            // Energy compensation for mutated (smaller) grains
            if (grain.sizeScale < 0.99f && grain.sizeScale > 0.01f)
                envelope *= std::sqrt(1.0f / grain.sizeScale);

            envSum += envelope;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float sample   = readBufferHermiteAbs(ch, readPosRaw);
                float grainOut = sample * envelope;
                if (ch == 0) wetL += grainOut * grain.panL;
                else         wetR += grainOut * grain.panR;
            }

            grain.currentSample++;
        }

        // Overlap normalization: divide by sqrt(effective overlap).
        // A single isolated grain (envSum <= 1) passes at full level → distinct
        // particles stay punchy; dense overlaps are scaled down so the summed
        // output never explodes into the output saturator. This is the core fix
        // for "Square saturates" — Square's ~unity window no longer stacks to N×.
        if (envSum > 1.0f)
        {
            float norm = 1.0f / std::sqrt(envSum);
            wetL *= norm;
            wetR *= norm;
        }

        // (5) Diffusion on grain output (use smoothed coefficient to prevent allpass jump → click)
        processDiffusion(wetL, wetR, smoothedDiffusion.getNextValue());

        // (6) HP/LP filters on grain output
        wetL = wetHP[0].processHighpass(wetL);
        wetR = wetHP[1].processHighpass(wetR);
        wetL = wetLP[0].processLowpass(wetL);
        wetR = wetLP[1].processLowpass(wetR);

        // (7) Wet delay: delay-after-grain echo path
        //     wetDelayBuffer = grain output + feedback echoes of previous grain output
        int delayReadPos = (writePos - static_cast<int>(delaySamples) + kBufferSize) & kBufferMask;
        float delayOutL = 0.0f, delayOutR = 0.0f;

        float fbL = wetDelayBuffer[0][static_cast<size_t>(delayReadPos)];
        float fbR = (numChannels > 1) ? wetDelayBuffer[1][static_cast<size_t>(delayReadPos)] : fbL;

        if (delayOn)
        {
            delayOutL = fbL;
            delayOutR = fbR;
        }

        // Feedback chain: scale → limiter → softClip → DC block → filter
        fbL *= feedbackGain;
        fbR *= feedbackGain;

        // Feedback limiter: envelope follower keeps the signal near clipper threshold
        // so softClip works in its smooth region rather than deep saturation.
        // Attack ~0.5ms (catches sudden spikes), release ~150ms (smooth gain recovery).
        // Threshold 0.85 → tanh(0.85) ≈ 0.69, well within the soft part of the curve.
        {
            const float fbSr         = static_cast<float>(currentSampleRate);
            const float attackCoeff  = 1.0f - std::exp(-1.0f / (fbSr * 0.0005f));
            const float releaseCoeff = 1.0f - std::exp(-1.0f / (fbSr * 0.150f));
            constexpr float limThreshold = 0.70f;

            float fb[2] = { fbL, fbR };
            for (int ch = 0; ch < 2; ++ch)
            {
                const float peak  = std::abs(fb[ch]);
                const float coeff = (peak > fbLimiterEnv[ch]) ? attackCoeff : releaseCoeff;
                fbLimiterEnv[ch] += coeff * (peak - fbLimiterEnv[ch]);

                const float targetGain = (fbLimiterEnv[ch] > limThreshold)
                                         ? (limThreshold / (fbLimiterEnv[ch] + 1e-6f))
                                         : 1.0f;
                // Smooth gain changes at ~5ms to avoid zipper noise
                fbLimiterGain[ch] += (1.0f - std::exp(-1.0f / (fbSr * 0.005f)))
                                     * (targetGain - fbLimiterGain[ch]);
                fbLimiterGain[ch]  = juce::jlimit(0.0f, 1.0f, fbLimiterGain[ch]);
                fb[ch] *= fbLimiterGain[ch];
            }
            fbL = fb[0];
            fbR = fb[1];
        }

        fbL = softClip(fbL);
        fbR = softClip(fbR);
        fbL = dcBlocker[0].process(fbL);
        fbR = dcBlocker[1].process(fbR);
        fbL = feedbackHP[0].processHighpass(fbL);
        fbR = feedbackHP[1].processHighpass(fbR);
        fbL = feedbackLP[0].processLowpass(fbL);
        fbR = feedbackLP[1].processLowpass(fbR);

        // When delay is off: clear feedback so wetDelayBuffer tracks grain output without accumulation
        if (!delayOn) { fbL = 0.0f; fbR = 0.0f; }

        // Write grain output + processed feedback to wet delay buffer.
        // Guard against NaN/Inf accumulation in the feedback loop:
        // if NaN enters here it circulates forever until the buffer is cleared.
        float fbWriteL = wetL + fbL;
        float fbWriteR = wetR + fbR;
        if (!std::isfinite(fbWriteL)) fbWriteL = 0.0f;
        if (!std::isfinite(fbWriteR)) fbWriteR = 0.0f;
        wetDelayBuffer[0][static_cast<size_t>(writePos)] = fbWriteL;
        if (numChannels > 1)
            wetDelayBuffer[1][static_cast<size_t>(writePos)] = fbWriteR;

        // (8) Equal-power crossfade with level compensation:
        //     dryGain = cos(mix * π/2)  → 1.0 at 0%, 0.707 at 50%, 0.0 at 100%
        //     wetGain = sin(mix * π/2)  → 0.0 at 0%, 0.707 at 50%, 1.0 at 100%
        //     No level drop when inserting the plugin (compensation at 50%), and at 100%
        //     wet the dry signal is completely silent as expected.
        //     dryGain also gated by freezeGain (fades to 0 when frozen).
        const float dryGain = std::cos(mixAmount * juce::MathConstants<float>::halfPi);
        const float wetGain = std::sin(mixAmount * juce::MathConstants<float>::halfPi);

        // Grain/Delay balance (Ratio, equal-power). 0 = all delay, 1 = all grains.
        // Delay off → no delay signal, so grains pass at full level regardless.
        const float ratio = smoothedRatio.getNextValue();
        float grainMix = 1.0f, delayMixR = 0.0f;
        if (delayOn)
        {
            grainMix  = std::sin(ratio * juce::MathConstants<float>::halfPi);
            delayMixR = std::cos(ratio * juce::MathConstants<float>::halfPi);
        }
        const float wetSumL = wetL * grainMix + delayOutL * delayMixR;
        const float wetSumR = wetR * grainMix + delayOutR * delayMixR;
        float outL = dryL * freezeGain * dryGain + wetSumL * wetGain * outputGain;
        float outR = dryR * freezeGain * dryGain + wetSumR * wetGain * outputGain;

        // Output safety: transparent below 0.9, smooth knee only above it.
        // Keeps normal-level grain output perfectly clean (no always-on tanh
        // colouration) while still taming rare transients before the hard clamp.
        // The feedback loop has its own limiter+softClip, so self-oscillation
        // is already contained upstream — this is purely a peak safety net.
        outL = softKneeLimit(outL);
        outR = softKneeLimit(outR);

        // Hard clamp to ±1.0 as a final guard (rarely engaged after the soft knee).
        // Also catches any NaN/Inf that survived the feedback guard above.
        outL = juce::jlimit(-1.0f, 1.0f, outL);
        outR = juce::jlimit(-1.0f, 1.0f, outR);
        if (!std::isfinite(outL)) outL = 0.0f;
        if (!std::isfinite(outR)) outR = 0.0f;

        channelData[0][samp] = outL;
        if (numChannels > 1) channelData[1][samp] = outR;

        // (9) Write to main delay buffer: dry input only, gated by freeze
        //     No feedback here — this is purely the grain source buffer
        delayBuffer[0][static_cast<size_t>(writePos)] = dryL * freezeGain;
        if (numChannels > 1)
            delayBuffer[1][static_cast<size_t>(writePos)] = dryR * freezeGain;

        writePos = (writePos + 1) & kBufferMask;
    }
}

juce::AudioProcessorEditor* DegrainProcessor::createEditor()
{
    return new DegrainEditor(*this);
}

void DegrainProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Save APVTS state
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void DegrainProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new DegrainProcessor();
}
