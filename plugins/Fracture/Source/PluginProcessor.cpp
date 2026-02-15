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
}

void FractureProcessor::releaseResources()
{
    // Clear delay buffer memory
    for (auto& channelBuf : delayBuffer)
        std::fill(channelBuf.begin(), channelBuf.end(), 0.0f);
}

// =============================================================================
// Phase 4.1: processBlock
// =============================================================================

void FractureProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = juce::jmin(buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();

    if (numChannels == 0 || numSamples == 0)
        return;

    // --- Block-rate parameter reads ---
    const float delayTimeMs   = delayTimeParam->load();
    const float grainSizeMs   = grainSizeParam->load();
    const float grainRateHz   = grainRateParam->load();
    const int   grainShapeIdx = static_cast<int>(grainShapeParam->load());
    const float dryWetPct     = dryWetParam->load();

    // Phase 4.2 parameters
    const float feedbackPct   = feedbackParam->load();
    const float diffusionPct  = diffusionParam->load();
    const float spreadPct     = stereoSpreadParam->load();
    const float detuneCents   = detuneParam->load();
    const float lowCutHz      = lowCutParam->load();
    const float highCutHz     = highCutParam->load();

    const float sr = static_cast<float>(currentSampleRate);

    // Convert parameters to sample-domain values
    const float delaySamples   = (delayTimeMs * 0.001f) * sr;
    const int   grainLenSamples = juce::jmax(1, static_cast<int>((grainSizeMs * 0.001f) * sr));
    const float samplesPerGrain = (grainRateHz > 0.0f) ? (sr / grainRateHz) : sr;
    const uint8_t windowType   = static_cast<uint8_t>(juce::jlimit(0, 3, grainShapeIdx));

    // Phase 4.2: derived values
    const float spreadAmount   = spreadPct * 0.01f;       // 0.0 to 1.0
    const float diffusionAmt   = diffusionPct * 0.01f;    // 0.0 to 1.0
    // Feedback: 0-120% param, cap at 115% (1.15f)
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

        // (2) Capture dry input
        float dryL = channelData[0][samp];
        float dryR = (numChannels > 1) ? channelData[1][samp] : dryL;

        // (3) Grain scheduler: accumulate and spawn (with stereo spread + detune)
        grainAccumulator += 1.0f;
        if (grainAccumulator >= samplesPerGrain)
        {
            grainAccumulator -= samplesPerGrain;
            spawnGrain(grainLenSamples, windowType, spreadAmount, detuneCents);
        }

        // (4) Process all active grains - sum their outputs (with playback rate for detune)
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

            // Compute read delay using fractional position for detuned playback
            int samplesSinceSpawn = (writePos - grain.startReadPos + kBufferSize) & kBufferMask;
            float readDelay = static_cast<float>(samplesSinceSpawn) + delaySamples
                              - fractionalPos;

            // Clamp to valid range (need at least 1 sample for Hermite, max buffer-4)
            readDelay = juce::jlimit(1.0f, static_cast<float>(kBufferSize - 4), readDelay);

            // Apply window envelope (use integer approximation of fractional position)
            int fractionalPosInt = static_cast<int>(fractionalPos);
            float envelope = applyWindow(grain.phase, grain.windowType,
                                         grain.lengthSamples, fractionalPosInt);

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

            // Advance grain playback position (integer counter, fractional handled by playbackRate)
            grain.currentSample++;
        }

        // (5) Apply diffusion to wet signal
        processDiffusion(wetL, wetR, diffusionAmt);

        // (6) Dry/wet mix
        channelData[0][samp] = dryL * (1.0f - mixAmount) + wetL * mixAmount;
        if (numChannels > 1)
            channelData[1][samp] = dryR * (1.0f - mixAmount) + wetR * mixAmount;

        // (7) Feedback path: wet → soft clip → DC block → HP → LP → gain
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

        // (8) Write to buffer: input + feedback
        delayBuffer[0][static_cast<size_t>(writePos)] = dryL + fbL;
        if (numChannels > 1)
            delayBuffer[1][static_cast<size_t>(writePos)] = dryR + fbR;

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
