# Degrain — DSP Architecture Specification

**Plugin:** Degrain — Granular Delay
**Type:** Audio Effect (Stereo In / Stereo Out)
**Generated:** 2026-02-14
**Stage:** 0 (Architecture Definition)

---

## 1. Core Components

### 1.1 Circular Delay Buffer

**JUCE Class:** Custom implementation
**Purpose:** Core audio storage for granular playback
**Parameters Affected:** `delay_time`, `delay_sync_toggle`, `delay_sync_division`

**Configuration:**
- **Buffer Size:** 4 seconds at 192kHz = 768,000 samples per channel
- **Actual Allocation:** 1,048,576 samples (2^20, power-of-2 for bitmask indexing)
- **Channels:** 2 (stereo)
- **Storage:** `juce::AudioBuffer<float>` or `std::vector<float>` per channel
- **Write Pointer:** Single incrementing pointer with wrap-around via bitmask
- **Interpolation:** Hermite (cubic) for fractional delay reads

**Key Operations:**
- `writeSample(channel, sample)` — Write to current position
- `readSample(channel, delaySamples)` — Read with Hermite interpolation
- `advance()` — Increment write pointer with bitmask wrap

### 1.2 Grain Engine

**JUCE Class:** Custom implementation
**Purpose:** Manages grain pool, scheduling, and overlap-add synthesis
**Parameters Affected:** `grain_size`, `grain_rate`, `grain_rate_sync_toggle`, `grain_rate_sync_division`, `grain_shape`

**Configuration:**
- **Grain Pool Size:** 64 pre-allocated `Grain` structs
- **Voice Stealing:** Oldest-first (highest phase grain)
- **Scheduling:** Sample-accurate, trigger based on grain rate
- **Overlap-Add:** Sum all active grain outputs per sample

**Grain Struct:**
```cpp
struct Grain {
    bool active;
    int startReadPos;         // Buffer position (samples)
    int lengthSamples;        // Grain duration
    int currentSample;        // Playback position within grain
    float playbackRate;       // For pitch shift (1.0 = normal)
    float panL, panR;         // Equal-power pan gains
    uint8_t windowType;       // 0=Hann, 1=Triangle, 2=Square, 3=Ramp
    bool isReversed;          // Forward or reverse playback
    uint8_t mutationPass;     // Feedback pass count
    float phase;              // 0.0 to 1.0 (for envelope)
};
```

**Scheduler Algorithm:**
- Accumulate samples since last grain
- When threshold reached (sampleRate / grainRateHz), spawn new grain
- Apply grain-spawn parameters: size, pan, detune, reverse probability, mutation

### 1.3 Grain Windowing

**JUCE Class:** Custom implementation (function pointers)
**Purpose:** Amplitude envelopes for smooth grain boundaries
**Parameters Affected:** `grain_shape`

**Window Functions:**

**Hann (Sine):**
```cpp
float hann(float phase) {
    return 0.5f * (1.0f - std::cos(2.0f * M_PI * phase));
}
```

**Triangle:**
```cpp
float triangle(float phase) {
    return (phase < 0.5f) ? (2.0f * phase) : (2.0f * (1.0f - phase));
}
```

**Square (with 1ms soft edges):**
```cpp
float square(float phase) {
    if (phase < 0.01f || phase > 0.99f)
        return 0.5f * (1.0f - std::cos(M_PI * std::min(phase, 1.0f - phase) / 0.01f));
    return 1.0f;
}
```

**Ramp:**
```cpp
float ramp(float phase) {
    return 1.0f - phase;
}
```

### 1.4 Frequency-Dependent Grain Sizing

**JUCE Class:** Custom implementation (single engine, frequency analysis)
**Purpose:** Scale grain size based on frequency content
**Parameters Affected:** `freq_spread`, `grain_size`

**Approach:** Single grain engine with frequency-weighted size scaling

**Algorithm:**
1. **Frequency Analysis:**
   - Use simple RMS energy measurement per band (low/mid/high)
   - 3-band split via 4th-order Linkwitz-Riley crossover (250 Hz, 2500 Hz)
   - Track energy ratio: `highEnergy / (lowEnergy + epsilon)`

2. **Size Scaling:**
   - Base size from `grain_size` parameter
   - Scale factor: `lowBandScale = juce::jmap(freq_spread, 1.0f, 3.0f)`
   - Scale factor: `highBandScale = juce::jmap(freq_spread, 1.0f, 0.35f)`
   - Apply weighted average based on energy distribution

**Crossover Implementation:**
- Use `juce::dsp::LinkwitzRileyFilter<float>` (4th order)
- Two crossover points: 250 Hz (low/mid), 2500 Hz (mid/high)
- Process in parallel for energy analysis only (not per-grain filtering)

### 1.5 Grain Mutation Engine

**JUCE Class:** Custom implementation
**Purpose:** Track feedback passes and progressively drift grain parameters
**Parameters Affected:** `grain_mutation`

**Configuration:**
- **Pass Count Tracking:** Parallel `std::vector<uint8_t>` buffer (same size as delay buffer)
- **Write Logic:** Increment pass count when writing feedback to buffer (cap at 255)
- **Read Logic:** Grain inherits pass count from buffer read position

**Mutation Parameters:**
```cpp
struct MutationParams {
    float grainSizeScale;     // 1.0 -> 0.4 (shrinks to 40%)
    float pitchDriftCents;    // 0 -> ±50 cents max
    float densityScale;       // 1.0 -> 4.0 (up to 4x grain rate)
    float spreadScale;        // 0.0 -> 1.0 (increases stereo spread)
    float reverseProb;        // 0.0 -> 0.5 (increases reverse probability)
};
```

**Drift Calculation:**
```cpp
MutationParams calculate(uint8_t passCount, float mutationAmount) {
    float t = (float)passCount / 20.0f;  // Normalize (20 passes = full effect)
    t = std::min(t, 1.0f);
    float m = mutationAmount;

    MutationParams p;
    p.grainSizeScale = 1.0f - (0.6f * t * m);
    p.pitchDriftCents = 50.0f * t * m * randomBipolar();
    p.densityScale = 1.0f + (3.0f * t * m);
    p.spreadScale = std::min(1.0f, t * m * 2.0f);
    p.reverseProb = 0.5f * t * m;
    return p;
}
```

**Safety Mechanisms:**
- Cap pass count at 255
- Grain size minimum: 5ms regardless of mutation
- Density cap at 4x to prevent CPU overload
- Energy compensation: `amplitude *= 1.0f / sqrt(densityScale)`

### 1.6 Reverse Grain Mix

**JUCE Class:** Custom implementation (per-grain decision)
**Purpose:** Per-grain forward/reverse playback probability
**Parameters Affected:** `reverse_mix`

**Algorithm:**
```cpp
bool shouldReverse(float reverseMix, uint32_t grainSeed) {
    float randomVal = hashToFloat(grainSeed);  // 0.0 to 1.0
    return randomVal < reverseMix;
}
```

**Playback Implementation:**
- Envelope always plays forward (phase 0.0 -> 1.0)
- Audio read direction: forward = `startPos + sampleIndex`, reverse = `startPos + length - sampleIndex`
- Apply playback rate to read position for pitch shift

### 1.7 Allpass Diffusion Network

**JUCE Class:** Custom implementation (Schroeder allpass)
**Purpose:** Smear transients into dense wash
**Parameters Affected:** `diffusion`

**Configuration:**
- **Stages:** 4 cascaded Schroeder allpass filters
- **Delay Times (at 48kHz):** [113, 337, 601, 1009] samples (all prime numbers)
- **Coefficient:** 0.7 (scaled by diffusion amount parameter)
- **Placement:** Post-mix (after grain engine, before output)

**Schroeder Allpass (Moorer optimized form):**
```cpp
float process(float input) {
    float delayed = delayLine[(writePos - delaySamples) & mask];
    float v = input - coefficient * delayed;
    float output = coefficient * v + delayed;
    delayLine[writePos & mask] = v;
    writePos = (writePos + 1) & mask;
    return output;
}
```

**Scaling Diffusion Amount:**
```cpp
float coeff = diffusionAmount * 0.75f;  // 0% = 0.0, 100% = 0.75
// Below 5%, bypass network entirely to save CPU
```

### 1.8 Stereo Spread

**JUCE Class:** Custom implementation
**Purpose:** Per-grain random panning
**Parameters Affected:** `stereo_spread`

**Algorithm:**
```cpp
float grainPanPosition(uint32_t grainSeed, float spreadAmount) {
    uint32_t hash = grainSeed * 2654435761u;  // Knuth multiplicative hash
    float randomPan = (float)(hash & 0xFFFF) / 65535.0f;  // 0.0 to 1.0
    randomPan = randomPan * 2.0f - 1.0f;  // -1.0 to 1.0
    return randomPan * spreadAmount;
}

void equalPowerPan(float panPosition, float& gainL, float& gainR) {
    float angle = (panPosition + 1.0f) * 0.25f * M_PI;  // 0 to pi/2
    gainL = std::cos(angle);
    gainR = std::sin(angle);
}
```

**Application:** Store panL/panR in Grain struct at spawn time, apply when summing grain output

### 1.9 Detune / Pitch Shift

**JUCE Class:** Custom implementation (playback rate variation)
**Purpose:** Per-grain pitch shift via resampling
**Parameters Affected:** `detune`

**Algorithm:**
```cpp
float playbackRate = std::pow(2.0f, detuneCents / 1200.0f);
```

**Random Detune Per Grain:**
```cpp
float grainDetune(float detuneAmountCents, uint32_t grainSeed) {
    float random = hashToBipolar(grainSeed);  // -1.0 to 1.0
    return random * detuneAmountCents;
}
```

**Interpolation:** Hermite (cubic) for fractional buffer reads (same as delay buffer)

### 1.10 Feedback Path

**JUCE Class:** Custom implementation
**Purpose:** Process output before feeding back to delay buffer
**Parameters Affected:** `feedback`, `low_cut`, `high_cut`

**Signal Chain:**
1. Soft Clipper (tanh saturation)
2. DC Blocker (1-pole HPF at ~5 Hz)
3. Highpass Filter (1-pole, user-controlled)
4. Lowpass Filter (1-pole, user-controlled)
5. Feedback Gain (0-115%)

**Soft Clipper:**
```cpp
float softClip(float input) {
    return std::tanh(input);  // Or fast Pade approximation
}
```

**DC Blocker:**
```cpp
float process(float input) {
    float output = input - x1 + R * y1;  // R = 0.995
    x1 = input;
    y1 = output;
    return output;
}
```

**1-Pole Filters:**
```cpp
// Lowpass
void setLowpass(float cutoffHz, float sampleRate) {
    float dt = 1.0f / sampleRate;
    float rc = 1.0f / (2.0f * M_PI * cutoffHz);
    alpha = dt / (rc + dt);
}

float processLowpass(float input) {
    z1 = input * alpha + z1 * (1.0f - alpha);
    return z1;
}

// Highpass
float processHighpass(float input) {
    float lp = processLowpass(input);
    return input - lp;
}
```

**Self-Oscillation:** Feedback > 100% enables self-oscillation, soft clipper prevents runaway

### 1.11 Freeze System

**JUCE Class:** Custom implementation
**Purpose:** Capture buffer and loop grains indefinitely
**Parameters Affected:** `freeze`, `grain_mutation`

**Auto Mode (based on Mutation):**
- `grain_mutation == 0%`: Clean freeze (stop all writes, including feedback)
- `grain_mutation > 0%`: Live freeze (stop input writes, allow feedback writes)

**Crossfade Implementation:**
```cpp
class FreezeTransition {
    float freezeGain = 1.0f;  // 1.0 = writing, 0.0 = frozen
    float targetGain = 1.0f;
    float fadeRate;  // 1.0 / (0.01 * sampleRate) for 10ms fade

    void setFrozen(bool frozen) {
        targetGain = frozen ? 0.0f : 1.0f;
    }

    float getWriteGain() {
        // Smooth towards target
        if (freezeGain < targetGain)
            freezeGain = std::min(freezeGain + fadeRate, targetGain);
        else if (freezeGain > targetGain)
            freezeGain = std::max(freezeGain - fadeRate, targetGain);
        return freezeGain;
    }
};
```

**MIDI Trigger:** Process MIDI note-on/off to toggle freeze state

### 1.12 Tempo Sync

**JUCE Class:** `juce::AudioPlayHead` (JUCE API)
**Purpose:** Read BPM and calculate tempo-synced delay time / grain rate
**Parameters Affected:** `delay_sync_toggle`, `delay_sync_division`, `grain_rate_sync_toggle`, `grain_rate_sync_division`

**BPM Reading:**
```cpp
if (auto* playHead = getPlayHead()) {
    if (auto posInfo = playHead->getPosition()) {
        if (posInfo->getBpm().hasValue()) {
            currentBPM = *posInfo->getBpm();
        }
    }
}
```

**Note Division to Milliseconds:**
```cpp
float noteDivisionToMs(int divisionIndex, double bpm) {
    double beatDuration = 60000.0 / bpm;  // ms per beat
    double beats = divisionMultipliers[divisionIndex];  // e.g., 1.0, 0.5, 1.5, 0.333...
    return (float)(beatDuration * beats);
}
```

**Division Multipliers:**
- 1/1: 4.0, 1/1D: 3.0, 1/1T: 8/3, 1/2: 2.0, 1/2D: 1.5, 1/2T: 4/3
- 1/4: 1.0, 1/4D: 0.75, 1/4T: 2/3, 1/8: 0.5, 1/8D: 0.375, 1/8T: 1/3
- 1/16: 0.25, 1/16D: 0.1875, 1/16T: 1/6, 1/32: 0.125, 1/32D: 0.09375, 1/32T: 1/12

**Smooth BPM Transitions:** Use `juce::SmoothedValue<float, Multiplicative>` with 50ms ramp

### 1.13 Dry/Wet Mixer

**JUCE Class:** Custom implementation (simple crossfade)
**Purpose:** Blend dry and wet signals
**Parameters Affected:** `dry_wet`

**Algorithm:**
```cpp
float mixAmount = dryWetParam * 0.01f;  // 0-100% -> 0.0-1.0
output = dry * (1.0f - mixAmount) + wet * mixAmount;
```

**Smoothing:** Use `juce::SmoothedValue<float, Linear>` with 20ms ramp

### 1.14 Parameter Smoothing

**JUCE Class:** `juce::SmoothedValue<float>`
**Purpose:** Prevent zipper noise and parameter discontinuities

**Configuration:**

| Parameter | Smoothing Type | Ramp Time | Update Rate |
|-----------|---------------|-----------|-------------|
| `dry_wet` | Linear | 20ms | Sample-accurate |
| `feedback` | Linear | 20ms | Sample-accurate |
| `delay_time` | Multiplicative | 50ms | Sample-accurate |
| `diffusion` | Linear | 30ms | Sample-accurate |
| `low_cut` | Multiplicative | 50ms | Sample-accurate |
| `high_cut` | Multiplicative | 50ms | Sample-accurate |
| `grain_size` | None | N/A | Block-rate |
| `grain_rate` | None | N/A | Block-rate |
| `stereo_spread` | None | N/A | Block-rate |
| `detune` | None | N/A | Block-rate |
| `reverse_mix` | None | N/A | Block-rate |
| `grain_mutation` | Linear | 30ms | Block-rate |
| `freq_spread` | Linear | 30ms | Block-rate |
| `freeze` | Custom | 10ms | Sample-accurate (crossfade) |

**Rationale:** Grain-spawn parameters (size, rate, spread, detune, reverse) naturally smooth via grain overlap, no need for sample-accurate smoothing

---

## 2. Processing Chain

```
Input (Stereo L/R)
      |
      v
+---------------------+
| Freeze Write Gate   |  (if frozen: writeGain = 0.0)
+---------------------+
      |
      v
+---------------------+
| Delay Buffer Write  |  (input + feedback -> buffer)
+---------------------+
      |
      |  [Grain Scheduling]
      v
+---------------------+
| Grain Engine        |  (64 grain pool, overlap-add)
| - Read from buffer  |
| - Apply windowing   |
| - Apply pitch shift |
| - Apply stereo pan  |
+---------------------+
      |
      v
+---------------------+
| Diffusion Network   |  (4-stage allpass, post-mix)
+---------------------+
      |
      v
+---------------------+
| Dry/Wet Mix         |  (crossfade input and processed)
+---------------------+
      |
      v
+---------------------+
| Safety Limiter      |  (soft tanh at +6 dBFS)
+---------------------+
      |
      v
Output (Stereo L/R)

      +-------------------- Feedback Path -------------------+
      |                                                       |
      | [Grain Mutation] -> [Soft Clipper] -> [DC Blocker]  |
      |   -> [HP Filter] -> [LP Filter] -> [Feedback Gain]  |
      |                                                       |
      +-------------------------------------------------------+
                                    |
                                    v
                            [Back to Buffer Write]
```

---

## 3. System Architecture

### 3.1 MIDI Handling

**Freeze Trigger:**
- Process MIDI messages in `processBlock()` via `juce::MidiBuffer`
- Note-on: Engage freeze (set `freezeTransition.setFrozen(true)`)
- Note-off: Disengage freeze (set `freezeTransition.setFrozen(false)`)
- Optional: Specific note mapping (e.g., C0) or sustain pedal (CC 64)

**No Other MIDI Use:** Plugin is audio-effect-only, no MIDI note processing beyond freeze

### 3.2 State Persistence

**APVTS (AudioProcessorValueTreeState):**
- All 19 parameters stored in APVTS
- Automatic XML serialization for DAW state save/load

**Custom State:**
- Freeze state (boolean): Store in processor state, restore on load
- Buffer content: NOT persisted (delay buffer clears on load)
- Mutation pass count buffer: Clears on load (starts fresh)

---

## 4. Parameter Mapping

| Parameter ID | DSP Component | Usage |
|--------------|---------------|-------|
| `delay_time` | Circular Buffer | Read offset in samples (ms → samples conversion) |
| `delay_sync_toggle` | Tempo Sync | Enable/disable tempo-synced delay time |
| `delay_sync_division` | Tempo Sync | Note division for delay time calculation |
| `grain_size` | Grain Engine | Base grain duration in samples |
| `grain_rate` | Grain Scheduler | Grain trigger frequency (Hz) |
| `grain_rate_sync_toggle` | Tempo Sync | Enable/disable tempo-synced grain rate |
| `grain_rate_sync_division` | Tempo Sync | Note division for grain rate calculation |
| `grain_shape` | Grain Windowing | Window function index (0-3) |
| `feedback` | Feedback Path | Feedback gain (0-115%) |
| `diffusion` | Diffusion Network | Allpass coefficient scaling (0-100%) |
| `stereo_spread` | Stereo Spread | Pan range scaling (0-100%) |
| `detune` | Pitch Shift | Cents offset for playback rate calculation |
| `low_cut` | Feedback Path | HP filter cutoff frequency (Hz) |
| `high_cut` | Feedback Path | LP filter cutoff frequency (Hz) |
| `reverse_mix` | Reverse Grain Mix | Probability of reverse playback (0-100%) |
| `grain_mutation` | Mutation Engine | Mutation amount scaling (0-100%) |
| `freq_spread` | Freq-Dependent Sizing | Frequency size scaling amount (0-100%) |
| `freeze` | Freeze System | Toggle buffer capture (boolean) |
| `dry_wet` | Dry/Wet Mixer | Mix ratio (0-100%) |

---

## 5. Algorithm Details

### 5.1 Circular Buffer

**Algorithm:** Ring buffer with power-of-2 sizing
**Indexing:** Bitmask modulo (`index & mask` instead of `index % size`)
**Interpolation:** 4-point Hermite (cubic)

**Hermite Formula:**
```cpp
float hermiteInterp(float ym1, float y0, float y1, float y2, float frac) {
    float c0 = y0;
    float c1 = 0.5f * (y1 - ym1);
    float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
    float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
    return c0 + frac * (c1 + frac * (c2 + frac * c3));
}
```

**Edge Cases:**
- Delay time < grain size: Grains may read ahead of write pointer (okay, reads old buffer content)
- Delay time > buffer size: Clamp to max buffer size
- Sample rate change: Reallocate buffer in `prepareToPlay()`, clear old content

### 5.2 Grain Scheduling

**Algorithm:** Fixed-rate accumulator

**Formula:**
```cpp
samplesPerGrain = sampleRate / grainRateHz;
samplesSinceLastGrain += 1.0f;
if (samplesSinceLastGrain >= samplesPerGrain) {
    samplesSinceLastGrain -= samplesPerGrain;
    spawnGrain();
}
```

**Edge Cases:**
- Grain rate > sample rate: Clamp to sample rate / 2 (Nyquist for grain triggering)
- Grain pool full: Voice steal oldest grain (highest phase value)
- Grain size > delay time: Grain reads wrap around buffer (acceptable artifact)

### 5.3 Overlap-Add Synthesis

**Algorithm:** Sum all active grain outputs

**Normalization:**
```cpp
float expectedOverlap = grainRateHz * (grainSizeSec);
float normalizationGain = 1.0f / std::sqrt(expectedOverlap);
```

**Edge Cases:**
- High overlap (>10 grains): May exceed 0 dBFS, soft clipper in feedback handles this
- Low overlap (<0.5): Gaps between grains, intentional (granular stutter effect)

### 5.4 Grain Mutation

**Algorithm:** Progressive parameter drift based on pass count

**Formula:**
```cpp
float t = std::min((float)passCount / 20.0f, 1.0f);
float m = mutationAmount;

grainSizeMultiplier = 1.0f - (0.6f * t * m);  // Down to 40%
pitchDriftCents = 50.0f * t * m * randomBipolar();
densityMultiplier = 1.0f + (3.0f * t * m);     // Up to 4x
```

**Edge Cases:**
- Pass count overflow (255): Cap effect at 255, no further mutation
- Mutation at 0%: All multipliers = 1.0, no mutation applied
- Grain size minimum: Clamp to 5ms regardless of mutation

### 5.5 Frequency-Dependent Grain Size

**Algorithm:** Energy-weighted size scaling

**Steps:**
1. Analyze input signal energy per band (low/mid/high) using RMS over ~100ms window
2. Calculate energy ratio: `highRatio = highEnergy / (lowEnergy + midEnergy + highEnergy + epsilon)`
3. Interpolate size multiplier: `sizeMultiplier = lowScale * (1.0 - highRatio) + highScale * highRatio`
4. Apply to next spawned grain: `actualSize = baseSize * sizeMultiplier`

**Edge Cases:**
- Silence: Use base grain size (no scaling)
- Full-spectrum noise: Averages to mid-band size
- Pure sine wave: Depends on frequency, tracks correctly

### 5.6 Allpass Diffusion

**Algorithm:** Cascaded Schroeder allpass filters

**Formula:**
```cpp
for (int stage = 0; stage < 4; ++stage) {
    float delayed = delayLine[stage][(writePos - delaySamples[stage]) & mask[stage]];
    float v = input - coeff * delayed;
    output = coeff * v + delayed;
    delayLine[stage][writePos & mask[stage]] = v;
    writePos++;
    input = output;  // Feed to next stage
}
```

**Edge Cases:**
- Diffusion at 0%: Bypass entire network (save CPU)
- Diffusion at 100%: Full coefficient (0.75), dense wash

---

## 6. Integration Points

### 6.1 Feature Dependencies

- **Grain Engine depends on:** Circular Buffer (read source)
- **Mutation depends on:** Feedback counter (pass count tracking)
- **Reverse Grain Mix depends on:** Grain Engine (per-grain spawn decision)
- **Diffusion depends on:** Grain Engine output (post-mix processing)
- **Stereo Spread depends on:** Grain Engine (per-grain pan assignment)
- **Detune depends on:** Grain Engine (per-grain playback rate)
- **Freeze depends on:** All components (modulates write gate)
- **Tempo Sync depends on:** AudioPlayHead (BPM source)

### 6.2 Parameter Interactions

- **Mutation affects Freeze:** Mutation > 0% enables live freeze mode (feedback writes continue)
- **Feedback > 100% requires:** Soft clipper mandatory to prevent runaway oscillation
- **Freeze + Grain Rate:** Grains continue spawning from frozen buffer
- **Delay Sync + Grain Rate Sync:** Independent tempo sync for both parameters
- **Freq Spread + Grain Size:** Freq spread scales the base grain size parameter

### 6.3 Processing Order Requirements

**Strict Sequential Order in processBlock():**

1. Read parameters (block-rate: grain size, rate, spread, etc.)
2. Update smoothed parameters (sample-accurate: dry/wet, feedback, filters)
3. Read AudioPlayHead (BPM, tempo sync calculations)
4. Process MIDI (freeze triggers)
5. FOR EACH SAMPLE:
   - a. Calculate freeze write gain (crossfade)
   - b. Read grain scheduler (should spawn grain?)
   - c. Spawn grain if needed (assign all grain-spawn parameters)
   - d. Process all active grains (read buffer, apply window, apply pitch, apply pan)
   - e. Sum grain outputs (overlap-add)
   - f. Process diffusion network
   - g. Mix dry/wet
   - h. Apply safety limiter
   - i. Calculate feedback signal (mutation → clipper → DC blocker → filters → gain)
   - j. Write to delay buffer (input * writeGain + feedback)
   - k. Advance buffer write pointer
6. Update grain phases (increment currentSample, check for grain completion)

**Critical:** Feedback write must happen AFTER grain read, otherwise single-sample delay loop

### 6.4 Thread Boundaries

**Audio Thread (real-time, non-blocking):**
- ALL DSP processing (grain engine, buffer, filters, diffusion, etc.)
- Parameter reads from APVTS (atomic)
- AudioPlayHead reads
- MIDI processing

**Message Thread (UI, non-real-time):**
- Parameter updates (via APVTS)
- UI updates (value displays, knob positions)
- State save/load

**No Background Thread:** All processing is synchronous in processBlock()

---

## 7. Implementation Risks

### 7.1 Per-Feature Risk Assessment

| Feature | Risk Level | Rationale | Mitigation |
|---------|-----------|-----------|------------|
| Grain Engine | MEDIUM | Custom implementation, scheduling complexity, overlap-add | Start with basic Hann window, test voice stealing thoroughly |
| Frequency-Dependent Sizing | HIGH | Novel approach, frequency analysis, energy tracking | Implement as Phase 4.3 (last), use simple RMS per band, extensive testing |
| Grain Mutation | MEDIUM | Feedback tracking, progressive drift, runaway prevention | Cap all mutation parameters, test with extreme settings |
| Allpass Diffusion | LOW | Well-documented algorithm, simple cascaded structure | Use proven Schroeder form, prime delay times |
| Reverse Grain Mix | LOW | Simple per-grain decision, straightforward read direction | Deterministic random for reproducibility |
| Feedback Path | MEDIUM | Self-oscillation stability, soft clipper tuning | Mandatory safety limiter, test at 100-115% feedback |
| Freeze System | LOW | Simple write gate, crossfade well-understood | Test auto mode transition (mutation 0% vs >0%) |
| Tempo Sync | LOW | JUCE API well-documented, standard note division math | Fallback BPM (120), smooth tempo changes |
| Circular Buffer | LOW | Proven ring buffer technique, power-of-2 sizing | Pre-allocate at prepareToPlay(), test wrap-around |
| Parameter Smoothing | LOW | JUCE SmoothedValue handles this | Classify params correctly (sample-accurate vs block-rate) |

### 7.2 Highest Risks

1. **Frequency-Dependent Grain Sizing:** Novel feature, complex interaction between analysis and grain spawning. May require multiple iterations to sound musical.

2. **Grain Mutation Runaway:** Density/pitch drift could spiral out of control. Need robust capping and energy compensation.

3. **Feedback Stability:** Allowing >100% feedback requires careful soft clipper tuning and safety limiting.

---

## 8. Architecture Decisions

### 8.1 Single Grain Engine vs Per-Band

**Decision:** Single grain engine with frequency-weighted size scaling

**Rationale:**
- Lower memory footprint (1 buffer vs 3)
- Simpler implementation (1 grain pool vs 3)
- Frequency analysis affects grain size, not grain isolation
- Reduces CPU cost (64 grains total vs 3x24 grains)

**Trade-off:** Less clean frequency separation, but acceptable for the "crystalline" effect goal

### 8.2 Post-Mix Diffusion vs Per-Grain

**Decision:** Post-mix diffusion (single allpass chain after grain summation)

**Rationale:**
- Much lower CPU cost (1 chain vs per-grain processing)
- Diffusion is a spatial/temporal smearing effect, not a per-grain timbral effect
- Cleaner implementation (decoupled from grain engine)

**Trade-off:** Cannot diffuse individual grains independently, but unnecessary for the design goal

### 8.3 1-Pole vs Biquad Feedback Filters

**Decision:** 1-pole (6 dB/oct) HP/LP filters in feedback path

**Rationale:**
- Musical, gentle slope mimics analog delay feedback
- Lower CPU cost (1 multiply vs 5 for biquad)
- No resonance needed (avoids self-oscillation at specific frequencies)

**Trade-off:** Less aggressive filtering than biquad, but 6 dB/oct is sufficient for feedback tone shaping

### 8.4 Random Detune vs Fixed

**Decision:** Random detune spread per grain (within ±cents range)

**Rationale:**
- Creates richer, more chaotic shimmer (chorus-like)
- Avoids static pitch shift character
- More interesting musically

**Trade-off:** Less predictable than fixed shift, but that's desirable for granular effects

### 8.5 Auto Freeze Mode vs Separate Toggle

**Decision:** Auto mode based on Mutation parameter

**Rationale:**
- Mutation = 0%: Clean freeze makes sense (no evolution needed, save CPU)
- Mutation > 0%: Live freeze makes sense (frozen material should evolve)
- Reduces parameter count (no separate "Freeze Mode" toggle)

**Trade-off:** Less explicit control, but the auto behavior aligns with musical intent

---

## 9. Special Considerations

### 9.1 Thread Safety

- **Parameter Access:** All APVTS parameter reads are atomic (JUCE guarantees this)
- **Buffer Access:** Delay buffer is audio-thread-only (no message thread access)
- **Grain Pool:** Audio-thread-only, no locking needed
- **Freeze State:** Write from message thread (parameter change), read from audio thread → use `std::atomic<bool>`

### 9.2 Performance Estimates

**CPU Load (estimated, single core):**
- Circular Buffer: ~1% (write + read with Hermite interpolation)
- Grain Engine (64 grains, 10 active average): ~5% (window calculation, overlap-add)
- Diffusion (4 stages): ~0.5%
- Feedback Path (clipper + filters): ~1%
- Frequency Analysis (RMS per band): ~1%
- **Total: ~8-10% single-core load at 48kHz**

**Worst Case (100 Hz grain rate, 64 active grains):** ~15-20% single-core load

### 9.3 Denormal Protection

**JUCE Handles This:** `juce::FloatVectorOperations::disableDenormalisedNumberSupport()` called at plugin init

**Additional Safety:**
- Epsilon in division: `energy / (totalEnergy + 1e-10f)`
- Flush-to-zero in filter state variables: Check for abs(state) < 1e-15f, set to 0.0f

### 9.4 Sample Rate Handling

**prepareToPlay() Logic:**
- Reallocate delay buffer to support new sample rate (e.g., 4 sec at 192kHz)
- Recalculate filter coefficients for new sample rate
- Update smoothing rates for new sample rate
- Scale allpass delay times for new sample rate (e.g., 113 samples at 48kHz → 226 at 96kHz)

**Supported Rates:** 44.1kHz, 48kHz, 88.2kHz, 96kHz, 176.4kHz, 192kHz

### 9.5 Latency

**Inherent Latency:**
- Grain Engine: 0 samples (grains read from buffer, no lookahead)
- Diffusion: ~43ms at 48kHz (sum of allpass delay times: 113+337+601+1009 = 2060 samples)
- Filters: 0 samples (IIR, no group delay reported)

**Reported Latency:** 2060 samples at 48kHz (diffusion network total delay)

**Optional:** Report 0 latency and accept the phase shift from diffusion (it's musical in this context)

---

## 10. Research References

### 10.1 Professional Plugins (Reference / Inspiration)

- **Output Portal:** Granular delay with reverse/forward mix, diffusion, freeze
- **Soundtoys Crystallizer:** Granular pitch delay, shimmer, reverse
- **Eventide Blackhole:** Massive diffusion, reverse delay, freeze
- **Quanta 2 (Audio Damage):** Granular delay, frequency-dependent grain size (inspiration for Freq Spread)
- **Valhalla Delay:** Diffusion network design, allpass cascades
- **Baby Audio Crystalline:** Clean futuristic UI aesthetic (UI design reference)

### 10.2 JUCE Documentation

- `juce::AudioBuffer` — Delay buffer storage
- `juce::SmoothedValue` — Parameter smoothing
- `juce::AudioPlayHead` — BPM and tempo sync
- `juce::dsp::LinkwitzRileyFilter` — Frequency-dependent sizing crossover
- `juce::dsp::IIR::Filter` — Alternative for feedback filters (if upgrading from 1-pole)
- `juce::AudioProcessorValueTreeState` — Parameter management

### 10.3 DSP Resources

- Roads, C. "Microsound" (MIT Press, 2001) — Granular synthesis theory
- Schroeder, M.R. "Natural Sounding Artificial Reverberation" (1962) — Allpass diffusion
- Linkwitz & Riley, "Active Crossover Networks" (1976) — Crossover design
- Valhalla DSP Blog: "Reverbs: Diffusion, allpass delays, and metallic artifacts" (2011)
- musicdsp.org: "4th order Linkwitz-Riley filters"
- JUCE Forum: Circular buffer design patterns

---

**End of Architecture Specification**

Generated by: Plugin Freedom System
Date: 2026-02-14
Plugin: Degrain v3.0.0
Next Step: plan.md (Implementation Plan)
