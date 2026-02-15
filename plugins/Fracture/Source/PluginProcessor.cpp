#include "PluginProcessor.h"
#include "PluginEditor.h"

FractureProcessor::FractureProcessor()
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
    freezeParam              = parameters.getRawParameterValue("freeze");
    delaySyncToggleParam     = parameters.getRawParameterValue("delay_sync_toggle");
    delaySyncDivParam        = parameters.getRawParameterValue("delay_sync_division");
    grainRateSyncToggleParam = parameters.getRawParameterValue("grain_rate_sync_toggle");
    grainRateSyncDivParam    = parameters.getRawParameterValue("grain_rate_sync_division");
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

// =============================================================================
// Phase 4.1: Hermite Interpolation
// =============================================================================

float FractureProcessor::hermiteInterp(float ym1, float y0, float y1, float y2, float frac)
{
    float c0 = y0;
    float c1 = 0.5f * (y1 - ym1);
    float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
    float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
    return c0 + frac * (c1 + frac * (c2 + frac * c3));
}

float FractureProcessor::readBufferHermite(int channel, float delaySamples) const
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

// =============================================================================
// Phase 4.1: Grain Windowing
// =============================================================================

float FractureProcessor::applyWindow(float phase, uint8_t windowType, int lengthSamples, int currentSample)
{
    switch (windowType)
    {
        case 0: // Hann
            return 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * phase));

        case 1: // Triangle
            return (phase < 0.5f) ? (2.0f * phase) : (2.0f * (1.0f - phase));

        case 2: // Square with 1ms soft edges (approximated as fraction of grain length)
        {
            // 1ms soft edges: use a fixed number of samples for fade
            // At prepareToPlay we don't know sample rate here, so approximate:
            // Use ~48 samples as 1ms at 48kHz. Scale proportionally.
            // Actually, we can compute from lengthSamples and phase.
            // 1ms edge = (sampleRate * 0.001) samples, but we don't have sampleRate here.
            // Use a fraction: fadeLen = max(1, lengthSamples / 100) as a reasonable approximation
            // that gives short fades on short grains and avoids being too long on long grains.
            // Better: use a fixed phase-based edge (e.g. 2% of grain at each end).
            int fadeLen = juce::jmax(1, lengthSamples / 50); // ~2% fade at each end
            if (currentSample < fadeLen)
                return static_cast<float>(currentSample) / static_cast<float>(fadeLen);
            if (currentSample >= (lengthSamples - fadeLen))
                return static_cast<float>(lengthSamples - 1 - currentSample) / static_cast<float>(fadeLen);
            return 1.0f;
        }

        case 3: // Ramp (decay)
            return 1.0f - phase;

        default:
            return 1.0f;
    }
}

// =============================================================================
// Phase 4.1: Grain Spawning
// =============================================================================

void FractureProcessor::spawnGrain(int grainLengthSamples, uint8_t windowType,
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

    // If no free slot, steal the oldest (highest phase)
    if (slotIndex < 0)
    {
        float highestPhase = -1.0f;
        for (int i = 0; i < kMaxGrains; ++i)
        {
            if (grainPool[static_cast<size_t>(i)].phase > highestPhase)
            {
                highestPhase = grainPool[static_cast<size_t>(i)].phase;
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

void FractureProcessor::spawnGrainAdvanced(int grainLengthSamples, uint8_t windowType,
                                            float spreadAmount, float detuneCents,
                                            float reverseMix, float mutationAmount,
                                            float freqSpread, float lowE, float midE, float highE)
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

    // If no free slot, steal the oldest (highest phase)
    if (slotIndex < 0)
    {
        float highestPhase = -1.0f;
        for (int i = 0; i < kMaxGrains; ++i)
        {
            if (grainPool[static_cast<size_t>(i)].phase > highestPhase)
            {
                highestPhase = grainPool[static_cast<size_t>(i)].phase;
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
    grain.startReadPos = writePos;
    grain.windowType = windowType;
    grain.currentSample = 0;
    grain.phase = 0.0f;

    // --- Phase 4.3: Frequency-Dependent Grain Size ---
    float effectiveLength = static_cast<float>(grainLengthSamples);
    if (freqSpread > 0.001f)
    {
        float totalEnergy = lowE + midE + highE + 1.0e-10f;  // Avoid division by zero
        float lowWeight  = lowE  / totalEnergy;
        float midWeight  = midE  / totalEnergy;
        float highWeight = highE / totalEnergy;

        // freqSpread is 0.0 to 1.0
        float lowScale  = juce::jmap(freqSpread, 1.0f, 3.0f);
        float highScale = juce::jmap(freqSpread, 1.0f, 0.35f);
        float midScale  = 1.0f;  // Mid stays at base size

        float weightedScale = lowWeight * lowScale + midWeight * midScale + highWeight * highScale;
        effectiveLength *= weightedScale;
    }

    // --- Phase 4.3: Grain Mutation ---
    // Read pass count from buffer at grain start position
    uint8_t passCount = 0;
    if (!passCountBuffer[0].empty())
        passCount = passCountBuffer[0][static_cast<size_t>(writePos)];
    grain.mutationPass = passCount;

    float t = std::min(static_cast<float>(passCount) / 20.0f, 1.0f);
    float m = mutationAmount;  // 0.0 to 1.0

    // Grain size scaling from mutation
    float sizeScale = 1.0f - (0.6f * t * m);  // Shrinks to 40%
    effectiveLength *= sizeScale;

    // Min grain size: 5ms equivalent
    float minSamples = static_cast<float>(currentSampleRate) * 0.005f;
    effectiveLength = juce::jmax(effectiveLength, minSamples);
    grain.lengthSamples = juce::jmax(1, static_cast<int>(effectiveLength));

    // --- Phase 4.3: Reverse Grain Mix ---
    // Use bits 8-23 of hash for reverse decision
    float hashFloat = static_cast<float>((hash >> 8) & 0xFFFFu) / 65535.0f;
    grain.isReversed = (hashFloat < (reverseMix * 0.01f));

    // --- Phase 4.2: Stereo Spread ---
    float panNorm = static_cast<float>(hash & 0xFFFFu) / 65535.0f;
    float panAngle = (0.5f + (panNorm - 0.5f) * spreadAmount)
                     * (juce::MathConstants<float>::pi * 0.5f);
    grain.panL = std::cos(panAngle);
    grain.panR = std::sin(panAngle);

    // --- Phase 4.2 + 4.3: Detune + Mutation Pitch Drift ---
    float detuneNorm = static_cast<float>((hash >> 16) & 0xFFFFu) / 65535.0f;
    float centsOffset = (detuneNorm * 2.0f - 1.0f) * detuneCents;

    // Mutation pitch drift: ±50 cents max, based on pass count
    if (m > 0.001f && t > 0.001f)
    {
        // Use different hash bits for pitch drift randomization
        uint32_t hash2 = (hash >> 4) * 2246822519u;
        float randomBipolar = (static_cast<float>(hash2 & 0xFFFFu) / 65535.0f) * 2.0f - 1.0f;
        float pitchDrift = 50.0f * t * m * randomBipolar;
        centsOffset += pitchDrift;
    }

    grain.playbackRate = std::pow(2.0f, centsOffset / 1200.0f);
}

// =============================================================================
// Phase 4.2: Diffusion Network
// =============================================================================

void FractureProcessor::processDiffusion(float& sampleL, float& sampleR, float diffusionAmount)
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

void FractureProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    currentSampleRate = sampleRate;

    // Pre-allocate delay buffer (power-of-2 sized, per channel)
    for (auto& channelBuf : delayBuffer)
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

    // DC blockers
    for (auto& dc : dcBlocker)
        dc.reset();

    // Feedback filters (initialize with current parameter values)
    float lowCutHz  = lowCutParam->load();
    float highCutHz = highCutParam->load();
    for (int ch = 0; ch < 2; ++ch)
    {
        feedbackHP[static_cast<size_t>(ch)].reset();
        feedbackLP[static_cast<size_t>(ch)].reset();
        feedbackHP[static_cast<size_t>(ch)].setLowpass(lowCutHz, static_cast<float>(sampleRate));
        feedbackLP[static_cast<size_t>(ch)].setLowpass(highCutHz, static_cast<float>(sampleRate));
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

void FractureProcessor::releaseResources()
{
    // Clear delay buffer memory
    for (auto& channelBuf : delayBuffer)
        std::fill(channelBuf.begin(), channelBuf.end(), 0.0f);

    // Clear pass count buffers
    for (auto& pcBuf : passCountBuffer)
        std::fill(pcBuf.begin(), pcBuf.end(), static_cast<uint8_t>(0));
}

// =============================================================================
// Phase 4.1: processBlock
// =============================================================================

void FractureProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = juce::jmin(buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();

    if (numChannels == 0 || numSamples == 0)
        return;

    // =========================================================================
    // Phase 4.3: Read BPM from host AudioPlayHead
    // =========================================================================
    if (auto* playHead = getPlayHead())
    {
        if (auto posInfo = playHead->getPosition())
        {
            if (posInfo->getBpm().hasValue())
                currentBPM = *posInfo->getBpm();
        }
    }
    // Fallback BPM already initialized to 120.0
    smoothedBPM.setTargetValue(static_cast<float>(currentBPM));

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

    // =========================================================================
    // Phase 4.3: Tempo Sync - compute delay time and grain rate
    // =========================================================================
    float bpm = smoothedBPM.getNextValue();
    if (bpm < 20.0f) bpm = 120.0f;  // Safety

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
    const float delaySamples    = (delayTimeMs * 0.001f) * sr;
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

    // Update feedback filter coefficients (block-rate is fine for 1-pole)
    for (int ch = 0; ch < 2; ++ch)
    {
        feedbackHP[static_cast<size_t>(ch)].setLowpass(lowCutHz, sr);
        feedbackLP[static_cast<size_t>(ch)].setLowpass(highCutHz, sr);
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

        // (3) Grain scheduler: accumulate and spawn with Phase 4.3 features
        grainAccumulator += 1.0f;
        if (grainAccumulator >= samplesPerGrain)
        {
            grainAccumulator -= samplesPerGrain;
            spawnGrainAdvanced(grainLenSamples, windowType, spreadAmount, detuneCents,
                               reverseMix, mutationAmount, freqSpread,
                               lowE, midE, highE);
        }

        // (4) Process all active grains - sum their outputs
        float wetL = 0.0f;
        float wetR = 0.0f;

        for (auto& grain : grainPool)
        {
            if (!grain.active)
                continue;

            // Calculate phase (0.0 to 1.0) using fractional position for detuned grains
            float fractionalPos = static_cast<float>(grain.currentSample) * grain.playbackRate;
            grain.phase = fractionalPos / static_cast<float>(grain.lengthSamples);

            // Check if grain has finished (fractional position exceeds length)
            if (grain.phase >= 1.0f)
            {
                grain.active = false;
                continue;
            }

            // Compute read delay - Phase 4.3: handle reverse playback
            int samplesSinceSpawn = (writePos - grain.startReadPos + kBufferSize) & kBufferMask;
            float readDelay;

            if (grain.isReversed)
            {
                // Reversed grain: audio reads backward from start position
                // Envelope still plays forward (phase 0->1)
                readDelay = static_cast<float>(samplesSinceSpawn) + delaySamples
                            - (static_cast<float>(grain.lengthSamples) - fractionalPos);
            }
            else
            {
                // Normal forward playback
                readDelay = static_cast<float>(samplesSinceSpawn) + delaySamples
                            - fractionalPos;
            }

            // Clamp to valid range (need at least 1 sample for Hermite, max buffer-4)
            readDelay = juce::jlimit(1.0f, static_cast<float>(kBufferSize - 4), readDelay);

            // Apply window envelope (always plays forward regardless of reverse)
            int fractionalPosInt = static_cast<int>(fractionalPos);
            float envelope = applyWindow(grain.phase, grain.windowType,
                                         grain.lengthSamples, fractionalPosInt);

            // Energy compensation for mutation (grains get shorter, so boost slightly)
            if (grain.mutationPass > 0 && mutationAmount > 0.001f)
            {
                float tMut = std::min(static_cast<float>(grain.mutationPass) / 20.0f, 1.0f);
                float sizeScale = 1.0f - (0.6f * tMut * mutationAmount);
                // Compensate: shorter grains get proportional boost (sqrt for energy)
                if (sizeScale > 0.1f)
                    envelope *= std::sqrt(1.0f / sizeScale);
            }

            // Read from delay buffer and apply envelope + pan
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float sample = readBufferHermite(ch, readDelay);
                float grainOut = sample * envelope;

                if (ch == 0)
                    wetL += grainOut * grain.panL;
                else
                    wetR += grainOut * grain.panR;
            }

            // Advance grain playback position
            grain.currentSample++;
        }

        // (5) Apply diffusion to wet signal
        processDiffusion(wetL, wetR, diffusionAmt);

        // (6) Dry/wet mix
        channelData[0][samp] = dryL * (1.0f - mixAmount) + wetL * mixAmount;
        if (numChannels > 1)
            channelData[1][samp] = dryR * (1.0f - mixAmount) + wetR * mixAmount;

        // (7) Feedback path: wet -> soft clip -> DC block -> HP -> LP -> gain
        float fbL = softClip(wetL);
        float fbR = softClip(wetR);

        fbL = dcBlocker[0].process(fbL);
        fbR = dcBlocker[1].process(fbR);

        fbL = feedbackHP[0].processHighpass(fbL);
        fbR = feedbackHP[1].processHighpass(fbR);

        fbL = feedbackLP[0].processLowpass(fbL);
        fbR = feedbackLP[1].processLowpass(fbR);

        fbL *= feedbackGain;
        fbR *= feedbackGain;

        // (8) Write to buffer with freeze gating
        // Phase 4.3 Freeze:
        //   - Clean freeze (mutation == 0%): stop ALL writes (input + feedback)
        //   - Live freeze (mutation > 0%):   stop input writes only, feedback continues
        bool cleanFreeze = (mutationAmount < 0.001f);

        float inputGateL = dryL * freezeGain;
        float inputGateR = dryR * freezeGain;

        if (cleanFreeze)
        {
            // Clean freeze: gate both input and feedback
            float fbGateL = fbL * freezeGain;
            float fbGateR = fbR * freezeGain;
            delayBuffer[0][static_cast<size_t>(writePos)] = inputGateL + fbGateL;
            if (numChannels > 1)
                delayBuffer[1][static_cast<size_t>(writePos)] = inputGateR + fbGateR;
        }
        else
        {
            // Live freeze: gate only input, feedback continues
            delayBuffer[0][static_cast<size_t>(writePos)] = inputGateL + fbL;
            if (numChannels > 1)
                delayBuffer[1][static_cast<size_t>(writePos)] = inputGateR + fbR;
        }

        // Phase 4.3: Update pass count buffer on feedback write
        // Only increment when feedback is actually being written
        bool feedbackWritten = cleanFreeze ? (freezeGain > 0.001f) : true;
        if (feedbackWritten && (feedbackGain > 0.001f))
        {
            size_t wp = static_cast<size_t>(writePos);
            for (int ch = 0; ch < numChannels; ++ch)
            {
                uint8_t pc = passCountBuffer[static_cast<size_t>(ch)][wp];
                if (pc < 255)
                    passCountBuffer[static_cast<size_t>(ch)][wp] = pc + 1;
            }
        }
        else
        {
            // Reset pass count when no feedback is being written
            size_t wp = static_cast<size_t>(writePos);
            for (int ch = 0; ch < numChannels; ++ch)
                passCountBuffer[static_cast<size_t>(ch)][wp] = 0;
        }

        // (9) Advance buffer write pointer
        writePos = (writePos + 1) & kBufferMask;
    }
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
