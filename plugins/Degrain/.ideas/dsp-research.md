# Degrain: Granular Delay DSP Research

Comprehensive research covering implementation strategies, algorithms, and best practices
for each DSP component in the Degrain granular delay plugin. This document informs the
architecture and implementation plan for JUCE C++.

---

## Table of Contents

1. [Grain Engine Core](#1-grain-engine-core)
2. [Frequency-Dependent Grain Size](#2-frequency-dependent-grain-size)
3. [Grain Mutation](#3-grain-mutation)
4. [Reverse Grain Mix](#4-reverse-grain-mix)
5. [Allpass Diffusion Network](#5-allpass-diffusion-network)
6. [Feedback Path Design](#6-feedback-path-design)
7. [Stereo Spread](#7-stereo-spread)
8. [Detune / Pitch Shifting](#8-detune--pitch-shifting)
9. [Freeze Mode](#9-freeze-mode)
10. [Tempo Sync](#10-tempo-sync)
11. [Parameter Smoothing](#11-parameter-smoothing)
12. [Implementation Priority](#12-implementation-priority)

---

## 1. Grain Engine Core

### Circular Delay Buffer Design

The delay buffer is the foundation of the entire plugin. It must support stereo operation
with variable length up to 2 seconds at 48kHz (96,000 samples per channel).

**Buffer sizing:**
- Maximum delay = 2 seconds at 192kHz = 384,000 samples (support high sample rates)
- Allocate as power-of-2 for fast modulo via bitmask: 524,288 samples (2^19) per channel
- Use `juce::AudioBuffer<float>` for JUCE-friendly memory management
- Alternatively, use a raw `std::vector<float>` per channel for tighter control

**Write/read pointer management:**

```cpp
class CircularBuffer {
    juce::AudioBuffer<float> buffer;
    int writePos = 0;
    int bufferSize;  // power of 2
    int bufferMask;  // bufferSize - 1

public:
    void writeSample(int channel, float sample) {
        buffer.setSample(channel, writePos & bufferMask, sample);
    }

    float readSample(int channel, float delaySamples) const {
        float readPos = (float)(writePos - (int)delaySamples);
        // Cubic interpolation for sub-sample accuracy
        int idx0 = ((int)readPos - 1) & bufferMask;
        int idx1 = ((int)readPos)     & bufferMask;
        int idx2 = ((int)readPos + 1) & bufferMask;
        int idx3 = ((int)readPos + 2) & bufferMask;
        float frac = readPos - std::floor(readPos);
        // Hermite interpolation
        float a = buffer.getSample(channel, idx0);
        float b = buffer.getSample(channel, idx1);
        float c = buffer.getSample(channel, idx2);
        float d = buffer.getSample(channel, idx3);
        return hermiteInterpolate(a, b, c, d, frac);
    }

    void advance() { writePos = (writePos + 1) & bufferMask; }
};
```

**Key considerations:**
- Power-of-2 size enables bitmask instead of modulo (avoids integer division)
- Hermite/cubic interpolation for fractional delay reads -- critical for pitch shifting
  and smooth delay time modulation
- The buffer must NOT be reallocated on the audio thread; size it at `prepareToPlay()`

### Grain Scheduling

Grains are triggered at a fixed rate between 0.1 Hz (one grain every 10 seconds) and
100 Hz (one grain every 10ms). The scheduler tracks elapsed time and spawns grains.

**Scheduling algorithm:**

```cpp
struct GrainScheduler {
    float grainRateHz = 10.0f;       // grains per second
    float samplesSinceLastGrain = 0;
    float samplesPerGrain;           // sampleRate / grainRateHz

    void prepare(double sampleRate) {
        samplesPerGrain = (float)(sampleRate / grainRateHz);
    }

    bool shouldTriggerGrain() {
        samplesSinceLastGrain += 1.0f;
        if (samplesSinceLastGrain >= samplesPerGrain) {
            samplesSinceLastGrain -= samplesPerGrain;
            return true;
        }
        return false;
    }
};
```

**Overlap management:**
- At 100 Hz with grain sizes of 50ms+, heavy overlap is expected (5+ simultaneous grains)
- Use a fixed-size grain pool (e.g., 64 grains max) to avoid dynamic allocation
- Each grain tracks: startSample, duration, currentPosition, envelope phase, parameters

### Grain Windowing / Envelopes

Four window types, all applied multiplicatively to grain output:

**Hann window (default, smoothest):**
```cpp
float hann(float phase) {  // phase: 0.0 to 1.0
    return 0.5f * (1.0f - std::cos(2.0f * M_PI * phase));
}
```

**Triangle window:**
```cpp
float triangle(float phase) {
    return (phase < 0.5f) ? (2.0f * phase) : (2.0f * (1.0f - phase));
}
```

**Square window (no fade, can cause clicks -- useful for glitch effects):**
```cpp
float square(float phase) {
    return 1.0f;
}
```
Note: A "soft square" with 1-2ms rise/fall prevents hard clicks while preserving the
character. Apply a tiny cosine taper at edges (first/last 64 samples).

**Ramp (sawtooth decay):**
```cpp
float ramp(float phase) {
    return 1.0f - phase;  // loud start, fade to silence
}
```

### Overlap-Add Synthesis

Overlap-add (OLA) is the standard technique for recombining grains:

1. Each active grain produces its windowed output sample
2. Sum all active grain outputs into the output buffer
3. The overlapping windows naturally crossfade

**Normalization concern:** With Hann windows at 50% overlap, the sum is exactly 1.0.
At other overlap ratios, the sum varies. Options:
- Accept variable amplitude (more "granular" character)
- Apply a global normalization based on expected overlap count
- Use a soft limiter on the output

**Recommended approach:** Do NOT normalize per-grain. Instead, scale the overall grain
engine output by `1.0f / sqrt(expectedOverlapCount)` as a rough energy normalization.
The soft clipper in the feedback path handles any excess.

### Voice Stealing

When the grain pool is exhausted:

1. **Oldest-first stealing:** Kill the grain closest to completion (highest phase)
2. Apply a fast fadeout (1-2ms) to the stolen grain to avoid clicks
3. Reassign the slot to the new grain

```cpp
int findStealableGrain(Grain* pool, int poolSize) {
    int bestIdx = -1;
    float bestPhase = -1.0f;
    for (int i = 0; i < poolSize; ++i) {
        if (pool[i].active && pool[i].phase > bestPhase) {
            bestPhase = pool[i].phase;
            bestIdx = i;
        }
    }
    return bestIdx;
}
```

### Real-Time Parameter Changes

When parameters change while grains are active:

- **Grain size:** New value applies only to newly spawned grains. Active grains keep
  their original duration. This prevents discontinuities.
- **Grain rate:** Takes effect at next scheduling decision. No impact on active grains.
- **Window type:** Can be changed on active grains (just changes the envelope function
  pointer), but for cleanest results, apply to new grains only.
- **Pitch/detune:** Can be smoothly interpolated on active grains since it only affects
  read position increment.

**Rule of thumb:** Parameters that affect grain structure (size, buffer position) are
"new-grain-only." Parameters that affect playback (pitch, pan, amplitude) can be
smoothly changed on active grains.

### Recommended Implementation

- Fixed pool of 64 `Grain` structs, pre-allocated at init
- Each grain stores: active flag, startReadPos, grainLengthSamples, currentSample,
  playbackRate, panL/panR, windowType enum, isReversed flag, mutationPass count
- Process all active grains per sample (not per block) for sample-accurate scheduling
- Use function pointers or a switch for window type selection

---

## 2. Frequency-Dependent Grain Size

This is a unique feature: grains spawned from different frequency bands have different
sizes. Low frequencies get larger grains (preserving bass clarity), high frequencies
get smaller grains (more textural variation).

### Internal Crossover Design

**Recommended: 4th-order Linkwitz-Riley (LR4) crossover**

Linkwitz-Riley filters are the standard for multiband audio processing because:
- Flat magnitude response at crossover (both bands sum to unity: -6dB each at crossover)
- No phase issues at crossover point (both outputs are in-phase)
- LR4 = two cascaded 2nd-order Butterworth filters per band

**3-band split** is the sweet spot (low / mid / high):
- Two crossover frequencies: e.g., 250 Hz and 2500 Hz (user-adjustable or fixed)
- Requires 2 LR4 lowpass + 2 LR4 highpass filter pairs
- Mid band = highpass of low crossover AND lowpass of high crossover

```cpp
// Using JUCE's IIR filters:
// For each crossover point, cascade two Butterworth 2nd-order sections
auto coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, crossoverFreq);
// Apply twice in series for LR4 (4th order)
```

**Alternative: JUCE's LinkwitzRileyFilter class** (available in juce_dsp module)
```cpp
juce::dsp::LinkwitzRileyFilter<float> lowCrossover;
lowCrossover.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
lowCrossover.setCutoffFrequency(250.0f);
```

### Per-Band Grain Size Scaling

Map each frequency band to a grain size multiplier:

| Band | Frequency Range | Grain Size Multiplier | Rationale |
|------|----------------|----------------------|-----------|
| Low  | < 250 Hz       | 2.0x - 4.0x         | Longer grains preserve bass phase coherence |
| Mid  | 250 - 2500 Hz  | 1.0x (reference)     | Base grain size applies directly |
| High | > 2500 Hz      | 0.25x - 0.5x        | Short grains create texture, preserve transients |

The user controls a single "Freq Size" knob (0-100%) that interpolates:
- 0%: All bands use the same grain size (no frequency dependence)
- 100%: Full multiplier spread as shown above

```cpp
float getGrainSize(float baseSize, Band band, float freqSizeAmount) {
    float multiplier = 1.0f;
    switch (band) {
        case Band::Low:  multiplier = juce::jmap(freqSizeAmount, 1.0f, 3.0f); break;
        case Band::Mid:  multiplier = 1.0f; break;
        case Band::High: multiplier = juce::jmap(freqSizeAmount, 1.0f, 0.35f); break;
    }
    return baseSize * multiplier;
}
```

### Recombination Strategy

Two approaches:

**Approach A: Separate grain engines per band (recommended)**
- Split the input into 3 bands before writing to delay buffer
- Each band has its own circular buffer and grain engine
- Sum the 3 grain engine outputs
- Pro: Clean separation, independent grain scheduling per band
- Con: 3x CPU cost for grain processing, 3x buffer memory

**Approach B: Single buffer, band-filtered read**
- Write the full signal to one buffer
- When spawning a grain, assign it a band and filter the grain output
- Pro: Less memory, simpler buffer management
- Con: Filtering per-grain is expensive; filter ringing at grain boundaries

**Recommended: Approach A (separate engines per band)**

The CPU cost is manageable because each band's grain engine can run fewer simultaneous
grains (e.g., 24 per band instead of 64 total). The sonic quality is significantly
better because grains are cleanly separated before granulation.

### Trade-offs

| Factor | Separate Engines | Single Buffer + Filter |
|--------|-----------------|----------------------|
| CPU | Higher (3x grain processing) | Lower base, but per-grain filtering adds up |
| Memory | 3x buffer size (~6MB at 192kHz) | 1x buffer size (~2MB) |
| Latency | LR4 crossover adds ~0 latency (IIR) | Same |
| Quality | Clean, no cross-band artifacts | Filter transients at grain boundaries |
| Complexity | Moderate | High (per-grain filter state management) |

### References

- Linkwitz & Riley, "Active Crossover Networks for Noncoincident Drivers" (1976)
- Fasciani & Wyse, "Spectral Granular Synthesis" (ICMC 2018) -- explores frequency-domain
  granular processing as an alternative to time-domain band splitting
- The spectral granular approach (FFT-based) is theoretically cleaner but introduces
  significant latency from FFT windowing; not recommended for a real-time delay effect

---

## 3. Grain Mutation

Grain mutation is a unique feature where grains progressively transform as they pass
through the feedback loop multiple times.

### Tracking Feedback Pass Count

Each grain (or each sample in the feedback buffer) needs a "generation counter."

**Implementation approach:**
- Maintain a parallel buffer (same size as delay buffer) storing pass count as `uint8_t`
- When writing to the delay buffer, write pass count + 1 to the parallel buffer
- When a grain reads from the buffer, it inherits the pass count from that position
- Cap at 255 passes (uint8_t max) to prevent overflow

```cpp
class MutationTracker {
    std::vector<uint8_t> passCountBuffer;  // parallel to delay buffer
    int bufferMask;

    void writePassCount(int writePos, uint8_t currentPass) {
        passCountBuffer[writePos & bufferMask] =
            std::min<uint8_t>(currentPass + 1, 255);
    }

    uint8_t readPassCount(int readPos) const {
        return passCountBuffer[readPos & bufferMask];
    }
};
```

### Progressive Parameter Drift

As pass count increases, grain parameters mutate:

| Parameter | Mutation Behavior | Musical Effect |
|-----------|------------------|----------------|
| Grain size | Shrinks (e.g., 0.95^pass) | Trails become more granular/textural |
| Pitch | Random drift (+/- range increases) | Trails become detuned/shimmery |
| Density | Increases (shorter inter-grain interval) | Trails become denser clouds |
| Pan | Wider spread | Trails spread across stereo field |
| Reverse probability | Increases | Later repeats more likely reversed |

**Mutation curves:**

```cpp
struct MutationParams {
    float grainSizeScale;    // multiplied to base grain size
    float pitchDriftCents;   // added to base detune
    float densityScale;      // multiplied to base density
    float spreadScale;       // multiplied to base spread
    float reverseProb;       // added to base reverse probability

    static MutationParams calculate(uint8_t passCount, float mutationAmount) {
        // mutationAmount: 0.0 (no mutation) to 1.0 (full mutation)
        float t = (float)passCount / 20.0f;  // normalize (20 passes = full effect)
        t = std::min(t, 1.0f);
        float m = mutationAmount;

        MutationParams p;
        p.grainSizeScale = 1.0f - (0.6f * t * m);   // shrink to 40% at full mutation
        p.pitchDriftCents = 50.0f * t * m * randomBipolar();  // +/- 50 cents max
        p.densityScale = 1.0f + (3.0f * t * m);      // up to 4x density
        p.spreadScale = std::min(1.0f, t * m * 2.0f); // spread to 100%
        p.reverseProb = 0.5f * t * m;                 // up to 50% reverse probability
        return p;
    }
};
```

### Single "Mutation Amount" Control

The mutation amount knob (0-100%) scales ALL mutation behaviors proportionally:

- 0%: No mutation. Feedback repeats are identical to the original.
- 25%: Subtle drift. Slight grain size reduction and pitch wander on later repeats.
- 50%: Moderate. Clearly evolving trails with increasing texture.
- 75%: Heavy. Dramatic transformation over 5-10 repeats.
- 100%: Maximum. Rapid transformation; trails become unrecognizable clouds quickly.

### Preventing Runaway Behavior

Key safety mechanisms:

1. **Cap pass count effect:** Mutation maxes out at ~20 passes. Beyond that, no further
   change. This prevents infinite escalation.
2. **Density limiter:** Even at max mutation, cap density at 4x the base rate to prevent
   CPU overload from spawning too many grains.
3. **Pitch drift uses smoothed random:** Use a low-frequency noise source (interpolated
   random values at ~2-5 Hz) rather than per-sample random, preventing harsh artifacts.
4. **Grain size minimum:** Never shrink below 5ms regardless of mutation, to maintain
   some tonal quality.
5. **Energy compensation:** As grain density increases with mutation, reduce per-grain
   amplitude proportionally: `amplitude *= 1.0f / sqrt(densityScale)`.

### Interaction with Feedback Path

Mutation occurs AFTER the grain engine output and BEFORE the soft clipper in the
feedback path:

```
Grain Engine Output -> Mutation Parameter Application -> Soft Clipper -> Filters -> Buffer
```

The mutation tracker reads the pass count from the buffer at grain spawn time and applies
parameter modifications to that grain's playback. The mutated output then feeds back
into the buffer with an incremented pass count.

### Recommended Implementation

- Use `uint8_t` parallel buffer for pass count tracking (minimal memory overhead)
- Apply mutation parameters at grain spawn time (not continuously during playback)
- Use deterministic pseudo-random for pitch drift (seed = pass count + grain start pos)
  to ensure reproducible behavior
- Smooth the mutation amount parameter to prevent clicks when adjusting during playback

---

## 4. Reverse Grain Mix

### Per-Grain Forward/Reverse Probability

The Reverse Mix control (0-100%) sets the probability that any given grain plays in
reverse:

```cpp
bool shouldReverse(float reverseMix, uint32_t grainSeed) {
    // Use deterministic random based on grain seed for reproducibility
    float randomVal = hashToFloat(grainSeed);  // 0.0 to 1.0
    return randomVal < reverseMix;
}
```

- 0%: All grains play forward
- 50%: Equal chance of forward/reverse (maximum rhythmic chaos)
- 100%: All grains play in reverse

### Reading Buffer Backwards

For a reversed grain, the read pointer starts at the END of the grain's window and
decrements:

```cpp
float readGrainSample(const CircularBuffer& buffer, int channel,
                       const Grain& grain, int grainSampleIndex) {
    float readPos;
    if (grain.isReversed) {
        // Start from end of grain window, read backwards
        readPos = grain.startReadPos + grain.lengthSamples - grainSampleIndex;
    } else {
        readPos = grain.startReadPos + grainSampleIndex;
    }
    // Apply playback rate for pitch shifting
    readPos = grain.startReadPos + (grain.isReversed ? -1.0f : 1.0f)
              * grainSampleIndex * grain.playbackRate;
    return buffer.readSample(channel, readPos);
}
```

### Grain Boundary Handling

When reversing, the grain window (envelope) still plays forward in time -- only the
audio content is reversed. This is critical for smooth overlap-add:

```
Forward grain:  Envelope: [fade in ... fade out]  Audio: [start ... end]
Reverse grain:  Envelope: [fade in ... fade out]  Audio: [end ... start]
```

The envelope always progresses from phase 0.0 to 1.0 regardless of audio direction.
This ensures that overlap-add still works correctly with mixed forward/reverse grains.

### Timing Alignment with Delay

Reversed grains present a timing challenge: the audio content that was at the END of
the delay window now plays first. Two approaches:

**Approach A: Preserve grain start time (recommended)**
- The grain triggers at the same time regardless of direction
- Reversed audio means the listener hears "future" buffer content first
- This creates the characteristic reverse delay effect naturally

**Approach B: Offset grain start to preserve perceived timing**
- Shift the grain start position by the grain length when reversing
- More "aligned" rhythmically but reduces the reverse effect

Recommended: Approach A. The timing displacement IS the desired effect.

### Crossfading Between Forward and Reverse

When the reverse mix parameter changes, currently active grains are not affected (they
were already assigned forward or reverse at spawn time). The transition happens naturally
as new grains spawn with the updated probability. This provides an inherently smooth
transition without needing explicit crossfading.

For an even smoother transition, implement a brief "crossfade zone" where grains near
50% mix sometimes play a blend:

```cpp
// Optional: soft reverse blend for grains near the threshold
float reverseBlend = 0.0f;
if (std::abs(randomVal - reverseMix) < 0.05f) {
    // Grain is near the threshold -- crossfade
    reverseBlend = juce::jmap(randomVal, reverseMix - 0.05f, reverseMix + 0.05f, 0.0f, 1.0f);
    // Mix forward and reverse reads
    float fwd = buffer.readForward(...);
    float rev = buffer.readReverse(...);
    output = fwd * (1.0f - reverseBlend) + rev * reverseBlend;
}
```

### Recommended Implementation

- Assign forward/reverse at grain spawn time using deterministic hash
- Envelope always plays forward; only audio read direction changes
- Use Approach A (preserve grain start time) for timing
- No explicit crossfading needed -- overlap-add handles transitions
- The reverse mix parameter itself should be smoothed to avoid abrupt probability jumps

---

## 5. Allpass Diffusion Network

### Schroeder Allpass Diffuser Design

The diffusion network smears transients into a dense wash, sitting between the grain
engine and the output (or optionally in the feedback path).

**Recommended: 4 cascaded allpass stages**

This is a good balance between density and CPU cost. Valhalla DSP's Sean Costello
notes that more stages create denser diffusion but increase metallic coloration.
4 stages with well-chosen delay times provide musical results.

**Single allpass structure:**

```cpp
class SchroederAllpass {
    std::vector<float> delayLine;
    int writePos = 0;
    int delaySamples;
    int mask;
    float coefficient = 0.7f;

public:
    void setDelay(int samples) {
        delaySamples = samples;
        int size = juce::nextPowerOfTwo(samples + 1);
        delayLine.resize(size, 0.0f);
        mask = size - 1;
    }

    float process(float input) {
        float delayed = delayLine[(writePos - delaySamples) & mask];
        float feedforward = input * (-coefficient) + delayed;
        float feedback = input + delayed * coefficient;
        // Wait -- standard Schroeder form:
        float v = input + coefficient * delayed;
        float output = -coefficient * v + delayed;
        delayLine[writePos & mask] = v;
        writePos++;
        return output;
    }
};
```

**Corrected Schroeder allpass (Moorer's optimized form):**

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

This uses only 1 multiply and 2 adds (Moorer optimization), vs 2 multiplies in the
naive form.

### Delay Time Selection

Delay times should be mutually prime to avoid resonant modes. Recommended values
(in samples at 48kHz):

| Stage | Delay (samples) | Delay (ms) | Notes |
|-------|----------------|------------|-------|
| 1     | 113            | 2.35       | Prime |
| 2     | 337            | 7.02       | Prime |
| 3     | 601            | 12.52      | Prime |
| 4     | 1009           | 21.02      | Prime |

These values are in roughly 3:1 ratio progression, which provides good density buildup
without obvious periodicity. Total delay through the diffusion network: ~43ms.

**Scaling for sample rate:** Multiply all delay times by `sampleRate / 48000.0` and
round to nearest prime (or at least nearest odd number).

**A "Size" control** can scale all delay times proportionally:
```cpp
int scaledDelay = (int)(baseDelay * sizeParam);  // sizeParam: 0.1 to 2.0
```

### Coefficient Selection

- **Typical range:** 0.5 to 0.8
- **0.5:** Minimal diffusion, preserves transient clarity
- **0.7:** Good all-round value, used by many classic reverb algorithms
- **0.8+:** Dense but risks metallic coloration on impulsive sources
- **Above 0.9:** Usable only with modulated delay times or for special effects

All stages can use the same coefficient for simplicity, or use slightly different
values (e.g., [0.75, 0.7, 0.65, 0.6]) to reduce correlation.

### Scaling Diffusion Amount (0-100%)

**Recommended: Coefficient interpolation + dry/wet mix**

```cpp
float processDiffusion(float input, float amount) {
    // Scale coefficients from 0 to max
    float coeff = amount * 0.75f;  // 0% = 0.0 (bypass), 100% = 0.75

    // However, at very low coefficients the allpass still processes
    // (just doesn't diffuse much). For true bypass at 0%, use wet/dry:
    float wet = allpassCascade.process(input, coeff);
    return input + amount * (wet - input);  // crossfade dry->wet
}
```

**Better approach:** Below ~5% diffusion amount, bypass the network entirely to save
CPU. Between 5-100%, interpolate the coefficient linearly.

### CPU Efficiency

- Each allpass stage: 1 multiply, 2 adds, 1 delay line read, 1 delay line write
- 4 stages = 4 multiplies + 8 adds per sample per channel
- This is extremely cheap -- comparable to a single biquad filter
- Total memory for delay lines: ~2060 samples * 4 bytes = ~8KB per channel

### Avoiding Metallic Artifacts

From Valhalla DSP research:
1. Keep coefficients below 0.8 for general use
2. Use mutually prime delay times (already addressed above)
3. Optional: Add subtle modulation (0.1-0.5 samples LFO) to delay times to break up
   periodic patterns. This adds slight chorus but eliminates metallic ringing.
4. Consider reducing coefficients on later stages (e.g., 0.75, 0.72, 0.68, 0.65)

### Recommended Implementation

- 4 cascaded Schroeder allpass stages using Moorer's optimized single-multiply form
- Delay times: [113, 337, 601, 1009] samples at 48kHz, scaled for sample rate
- Base coefficient: 0.7, controlled by diffusion amount parameter
- Dry/wet crossfade for diffusion amount control
- Optional: subtle delay time modulation for artifact reduction
- Place the diffusion network AFTER the grain engine, BEFORE the output mix

---

## 6. Feedback Path Design

### Signal Flow

```
Grain Engine Output
       |
       v
  [Diffusion Network] (optional placement here or post-output)
       |
       v
  [Soft Clipper (tanh saturation)]
       |
       v
  [DC Blocker]
       |
       v
  [Highpass Filter]
       |
       v
  [Lowpass Filter]
       |
       v
  [Feedback Gain]
       |
       v
  Write back to Delay Buffer (with pass count increment)
```

### Soft Clipper: tanh() Saturation

The soft clipper prevents runaway feedback while adding musical saturation character.

**Implementation:**

```cpp
float softClip(float input, float drive) {
    // drive: 1.0 (clean) to 5.0 (heavy saturation)
    return std::tanh(input * drive) / std::tanh(drive);
    // Normalization by 1/tanh(drive) keeps output level consistent
}
```

**Gain staging:**
- Input to the clipper should be roughly in the -1.0 to +1.0 range
- Apply makeup gain after clipping to compensate for level reduction
- The `/ tanh(drive)` normalization ensures output peaks at +/-1.0 regardless of drive

**Fast tanh approximation (Pade approximant):**

```cpp
float fastTanh(float x) {
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
```

This is accurate to within 0.001 for |x| < 3 and avoids the cost of `std::tanh`.

**DC buildup prevention:**
- tanh() is odd-symmetric, so it does not introduce DC offset on its own
- However, asymmetric input signals CAN create DC through the nonlinearity
- Always follow the clipper with a DC blocker (see below)

### DC Blocking

A simple 1-pole highpass filter at ~5 Hz removes DC offset:

```cpp
class DCBlocker {
    float x1 = 0.0f, y1 = 0.0f;
    float R = 0.995f;  // pole close to z=1; ~5 Hz at 48kHz

public:
    void setCoefficient(double sampleRate) {
        R = 1.0f - (2.0f * M_PI * 5.0f / (float)sampleRate);
    }

    float process(float input) {
        float output = input - x1 + R * y1;
        x1 = input;
        y1 = output;
        return output;
    }
};
```

### Self-Oscillation at >100% Feedback

Allowing feedback > 1.0 (up to ~1.1 or 1.15) enables self-oscillation:

- The soft clipper prevents amplitude from growing beyond +/-1.0
- The signal stabilizes at a saturated, harmonically rich oscillation
- The HP/LP filters shape the tone of the self-oscillation

**Safety considerations:**
- Hard-limit the feedback parameter to 1.15 maximum in the UI
- The tanh clipper naturally limits amplitude -- it cannot diverge to infinity
- However, energy CAN build up in frequency ranges not caught by the filters
- Add a safety brickwall limiter at the final output (set to +6 dBFS)

### HP/LP Filter Design

**Recommended: 1-pole (6dB/oct) for both HP and LP in the feedback path**

Rationale:
- Gentle slope mimics analog feedback path behavior
- Lower CPU cost than biquad (this runs every sample in the feedback path)
- Musical darkening/thinning effect on each feedback pass
- No resonance needed in the feedback path (resonance can cause self-oscillation
  at specific frequencies, which is rarely desired)

```cpp
class OnePoleFilter {
    float z1 = 0.0f;
    float a0, b1;

public:
    void setLowpass(float cutoffHz, float sampleRate) {
        float w = 2.0f * M_PI * cutoffHz / sampleRate;
        float cos_w = std::cos(w);
        b1 = -std::exp(-w);  // simplified for 1-pole
        a0 = 1.0f + b1;

        // Even simpler (RC filter emulation):
        float dt = 1.0f / sampleRate;
        float rc = 1.0f / (2.0f * M_PI * cutoffHz);
        float alpha = dt / (rc + dt);
        a0 = alpha;
        b1 = 1.0f - alpha;
    }

    float processLowpass(float input) {
        z1 = input * a0 + z1 * b1;
        return z1;
    }

    void setHighpass(float cutoffHz, float sampleRate) {
        setLowpass(cutoffHz, sampleRate);
    }

    float processHighpass(float input) {
        float lp = processLowpass(input);
        return input - lp;  // HP = input - LP
    }
};
```

**Alternative: 2-pole biquad (12dB/oct) for more aggressive filtering.**
Use JUCE's `juce::dsp::IIR::Filter<float>` with coefficients from
`juce::dsp::IIR::Coefficients<float>::makeLowPass()`. This adds resonance control
but at 2x the cost per sample.

**Recommended ranges:**
- Highpass: 20 Hz to 2000 Hz (default: 80 Hz -- removes mud on feedback trails)
- Lowpass: 500 Hz to 20000 Hz (default: 8000 Hz -- classic analog delay darkening)

### Safety Limiter

As a final safety net at the plugin output:

```cpp
// Soft brickwall at +6 dBFS (approximately 2.0 linear)
float safetyLimit(float sample) {
    const float ceiling = 2.0f;
    return std::tanh(sample / ceiling) * ceiling;
}
```

This should be transparent during normal use and only engage during extreme feedback
or self-oscillation scenarios.

### Recommended Implementation

- Order: soft clipper -> DC blocker -> HP filter -> LP filter -> feedback gain
- Use fast tanh approximation for the soft clipper
- 1-pole filters for HP and LP (6dB/oct, no resonance)
- DC blocker with R = 0.995 (~5 Hz cutoff)
- Allow feedback up to 115% for self-oscillation
- Safety limiter at output: soft tanh at +6 dBFS

---

## 7. Stereo Spread

### Per-Grain Random Panning

Each grain is assigned a random pan position at spawn time. The spread parameter
controls how far from center the panning can go.

**Deterministic random for reproducibility:**

```cpp
float grainPanPosition(uint32_t grainSeed, float spreadAmount) {
    // Hash the seed to get a deterministic "random" value
    uint32_t hash = grainSeed * 2654435761u;  // Knuth multiplicative hash
    float randomPan = (float)(hash & 0xFFFF) / 65535.0f;  // 0.0 to 1.0
    randomPan = randomPan * 2.0f - 1.0f;  // -1.0 to 1.0

    // Scale by spread amount (0 = center, 1 = full random)
    return randomPan * spreadAmount;
}
```

**Seed construction:** Use `grainIndex + writePosition + sampleCounter` as the seed.
This ensures deterministic but non-repeating pan positions.

### Equal-Power Panning Law

The constant-power panning law uses sine/cosine to maintain perceived loudness:

```cpp
void equalPowerPan(float panPosition, float& gainL, float& gainR) {
    // panPosition: -1.0 (hard left) to +1.0 (hard right)
    float angle = (panPosition + 1.0f) * 0.25f * M_PI;  // 0 to pi/2
    gainL = std::cos(angle);
    gainR = std::sin(angle);
}
```

This gives -3dB at center, which is the standard for most audio applications.

**Fast approximation (accurate within 0.5dB):**

```cpp
void fastPan(float pan, float& gainL, float& gainR) {
    // pan: 0.0 (left) to 1.0 (right)
    float p = (pan + 1.0f) * 0.5f;  // normalize to 0-1
    gainL = std::sqrt(1.0f - p);
    gainR = std::sqrt(p);
}
```

### Spread Amount Interpolation

```cpp
void applySpread(float basePan, float spreadAmount, float grainRandomPan,
                 float& gainL, float& gainR) {
    // Interpolate between center (0) and random pan position
    float finalPan = grainRandomPan * spreadAmount;
    equalPowerPan(finalPan, gainL, gainR);
}
```

- 0%: All grains at center (mono output)
- 50%: Grains panned within +/-0.5 of center
- 100%: Full -1.0 to +1.0 random panning

### Correlation Control

At extreme spread settings, random panning can cause phase cancellation when the
output is summed to mono. Mitigation strategies:

1. **Mid/side correlation check:** Monitor the correlation coefficient between L and R.
   If it drops below -0.3, reduce the spread slightly.
2. **Paired grains:** For every grain panned left, create a complementary grain panned
   right at the same time. This maintains stereo balance at the cost of doubling grain
   count.
3. **Spread limiting for mono compatibility:** When spread > 80%, blend a small amount
   of the opposite channel into each side (subtle stereo width reduction).

**Recommended: Approach 1 is safest but most complex. For simplicity, start with
uncorrelated random panning and add mono-compatibility limiting later if needed.**

### Recommended Implementation

- Assign pan at grain spawn time using deterministic hash
- Equal-power (cosine) panning law: `gainL = cos(angle)`, `gainR = sin(angle)`
- Spread parameter linearly scales the random pan offset
- Store `panL` and `panR` gains in the Grain struct
- Apply pan gains when summing grain output to stereo output buffer

---

## 8. Detune / Pitch Shifting

### Per-Grain Pitch Shift via Playback Rate

Pitch shifting in granular synthesis is achieved by changing the playback rate of each
grain. A higher rate = higher pitch, shorter grain duration.

```cpp
float playbackRate = std::pow(2.0f, detuneCents / 1200.0f);
// +50 cents: rate = 1.02930
// -50 cents: rate = 0.97153
```

**Range:** -50 to +50 cents (approximately +/- 1 semitone)

### Resampling Interpolation Methods

When the playback rate is not 1.0, we read from fractional buffer positions.
The interpolation method determines quality:

**Linear interpolation (cheapest, adequate for small detune):**

```cpp
float linearInterp(float* buffer, int mask, float position) {
    int idx0 = (int)position & mask;
    int idx1 = (idx0 + 1) & mask;
    float frac = position - std::floor(position);
    return buffer[idx0] + frac * (buffer[idx1] - buffer[idx0]);
}
```

- Pro: Very fast, 1 multiply + 1 add
- Con: Low-pass filtering effect (attenuates high frequencies), slight aliasing
- Adequate for detune within +/- 20 cents

**Cubic (Hermite) interpolation (recommended):**

```cpp
float hermiteInterp(float* buffer, int mask, float position) {
    int idx0 = ((int)position - 1) & mask;
    int idx1 = ((int)position)     & mask;
    int idx2 = ((int)position + 1) & mask;
    int idx3 = ((int)position + 2) & mask;
    float frac = position - std::floor(position);

    float a = buffer[idx1];
    float b = 0.5f * (buffer[idx2] - buffer[idx0]);
    float c = buffer[idx0] - 2.5f * buffer[idx1] + 2.0f * buffer[idx2] - 0.5f * buffer[idx3];
    float d = 0.5f * (buffer[idx3] - buffer[idx0]) + 1.5f * (buffer[idx1] - buffer[idx2]);
    return a + frac * (b + frac * (c + frac * d));
}
```

- Pro: Much better frequency response, minimal aliasing in +/- 50 cent range
- Con: 4 buffer reads, ~10 operations per sample
- Recommended for this use case

**Windowed sinc (highest quality, expensive):**
- Uses 4-8 point windowed sinc kernel per sample
- Theoretically ideal reconstruction
- Overkill for +/- 50 cents detune range
- Only consider if detune range is expanded beyond +/- 1 semitone

### Effect on Grain Duration

When playback rate != 1.0, the grain's effective duration changes:

```
effectiveDuration = grainDuration / playbackRate
```

At +50 cents (rate ~1.03), a 50ms grain plays in ~48.6ms. At -50 cents (rate ~0.97),
it plays in ~51.5ms. This is a small enough difference to ignore for scheduling
purposes in the +/- 50 cent range.

**For larger pitch shifts** (if the range is expanded later), the scheduler should
compensate: use the effective duration for overlap calculations.

### Aliasing Prevention

At +50 cents, the Nyquist frequency shifts down by ~3%. This means content near
Nyquist COULD alias, but in practice:

- The grain window itself acts as a lowpass filter (Hann window has good sidelobe
  suppression)
- At +/- 50 cents, aliasing is negligible with cubic interpolation
- For larger ranges: apply a gentle lowpass filter before upward pitch shifting

### Random Detune Per Grain

For a "chorus" or "shimmer" effect, randomize detune per grain:

```cpp
float grainDetune(float detuneAmountCents, uint32_t grainSeed) {
    float random = hashToBipolar(grainSeed);  // -1.0 to 1.0
    return random * detuneAmountCents;
}
```

This creates a cloud of slightly detuned grains -- a hallmark of lush granular effects.

### Recommended Implementation

- Hermite (cubic) interpolation for fractional buffer reads
- Playback rate = `pow(2, cents/1200)`, computed once per grain at spawn
- Random bipolar detune per grain using deterministic hash
- Ignore duration compensation for +/- 50 cents (< 3% change)
- Store playback rate in Grain struct, use it for read position increment

---

## 9. Freeze Mode

### Buffer Capture

When freeze is engaged:
1. Stop writing new input to the delay buffer
2. Continue reading from the buffer (grains still play)
3. All processing (diffusion, feedback, mutation, etc.) remains active

```cpp
void processBlock(juce::AudioBuffer<float>& buffer) {
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        // Read grains from delay buffer (always)
        float grainOutput = grainEngine.process(delayBuffer);

        // Write to delay buffer (only if not frozen)
        if (!isFrozen) {
            delayBuffer.writeSample(0, inputL + feedbackL);
            delayBuffer.writeSample(1, inputR + feedbackR);
        }

        // Optionally: still write feedback to buffer even when frozen
        // This allows the frozen material to evolve via feedback processing
        if (isFrozen && feedbackInFreezeMode) {
            delayBuffer.writeSample(0, feedbackL);
            delayBuffer.writeSample(1, feedbackR);
        }

        delayBuffer.advance();
    }
}
```

**Design choice:** Should feedback write to the buffer during freeze?
- **Yes (recommended):** Frozen material evolves over time through the feedback path.
  Mutation, filtering, and saturation gradually transform the frozen sound. This is
  more interesting musically.
- **No:** Pure capture. The buffer content is static. Grains always read the same
  material. Useful for a "sample and hold" effect.
- **Expose as option:** Let the user choose via a "Freeze Feedback" toggle.

### Smooth Transition (Crossfade)

Abruptly stopping/starting buffer writes can cause clicks. Implement a crossfade:

```cpp
class FreezeTransition {
    float freezeGain = 1.0f;  // 1.0 = writing, 0.0 = frozen
    float targetGain = 1.0f;
    float fadeRate;  // e.g., 1.0 / (0.01 * sampleRate) for 10ms fade

public:
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

// In processBlock:
float writeGain = freezeTransition.getWriteGain();
delayBuffer.writeSample(0, (inputL + feedbackL) * writeGain);
```

This crossfades the input write level over ~10ms, preventing clicks.

### MIDI Trigger for Freeze

```cpp
void processMidiMessage(const juce::MidiMessage& msg) {
    if (msg.isNoteOn()) {
        freezeTransition.setFrozen(true);
    } else if (msg.isNoteOff()) {
        freezeTransition.setFrozen(false);
    }
}
```

**MIDI learn:** Allow any MIDI CC or note to toggle freeze. Use JUCE's
`MidiMessageCollector` and process MIDI in `processBlock` alongside audio.

**Specific note mapping:** Common approach is to use a specific MIDI note (e.g., C0)
or a sustain pedal (CC 64) for freeze toggle.

### Grain Scheduling on Frozen Buffer

When frozen, the grain engine continues spawning grains. The grains read from the
static (or slowly evolving) buffer content. This creates a looping granular texture.

**Scan position:** Add a "Position" parameter (0-100%) that controls WHERE in the
frozen buffer grains read from. This lets the user scrub through the frozen material.

```cpp
float readPosition;
if (isFrozen) {
    // Grain start position = user-controlled position in buffer
    readPosition = frozenBufferStart + positionParam * frozenBufferLength;
} else {
    // Normal: grains read from delay time offset
    readPosition = writePos - delayTimeSamples;
}
```

### Recommended Implementation

- Freeze stops input writes, optionally allows feedback writes
- 10ms crossfade on freeze engage/disengage
- MIDI note-on/off and CC support for freeze toggle
- Position parameter active during freeze for buffer scrubbing
- All other parameters (grain size, density, mutation, etc.) remain active

---

## 10. Tempo Sync

### Reading BPM from AudioPlayHead

In JUCE 7+, the API uses `getPosition()` returning an `Optional<PositionInfo>`:

```cpp
void processBlock(juce::AudioBuffer<float>& buffer,
                  juce::MidiBuffer& midi) {
    if (auto* playHead = getPlayHead()) {
        if (auto posInfo = playHead->getPosition()) {
            if (posInfo->getBpm().hasValue()) {
                currentBPM = *posInfo->getBpm();
            }
            if (posInfo->getIsPlaying())  {
                isHostPlaying = true;
            }
        }
    }
}
```

**Important:** Only call `getPlayHead()` from `processBlock()`. Cache the BPM value
for use elsewhere. Use `std::atomic<double>` if accessing from the UI thread.

### Note Division to Milliseconds

```cpp
enum class NoteDivision {
    Whole,        // 4 beats
    HalfDotted,   // 3 beats
    Half,         // 2 beats
    QuarterDotted, // 1.5 beats
    QuarterTriplet, // 2/3 beat
    Quarter,      // 1 beat
    EighthDotted, // 0.75 beats
    Eighth,       // 0.5 beats
    EighthTriplet, // 1/3 beat
    Sixteenth,    // 0.25 beats
    SixteenthTriplet, // 1/6 beat
    ThirtySecond  // 0.125 beats
};

float noteDivisionToMs(NoteDivision div, double bpm) {
    double beatDuration = 60000.0 / bpm;  // ms per beat
    double beats;
    switch (div) {
        case NoteDivision::Whole:            beats = 4.0;    break;
        case NoteDivision::HalfDotted:       beats = 3.0;    break;
        case NoteDivision::Half:             beats = 2.0;    break;
        case NoteDivision::QuarterDotted:    beats = 1.5;    break;
        case NoteDivision::Quarter:          beats = 1.0;    break;
        case NoteDivision::QuarterTriplet:   beats = 2.0/3.0; break;
        case NoteDivision::EighthDotted:     beats = 0.75;   break;
        case NoteDivision::Eighth:           beats = 0.5;    break;
        case NoteDivision::EighthTriplet:    beats = 1.0/3.0; break;
        case NoteDivision::Sixteenth:        beats = 0.25;   break;
        case NoteDivision::SixteenthTriplet: beats = 1.0/6.0; break;
        case NoteDivision::ThirtySecond:     beats = 0.125;  break;
    }
    return (float)(beatDuration * beats);
}
```

### Smooth BPM Transitions

When BPM changes (tempo automation, live tempo changes):

- **Delay time:** Smooth the target delay time over ~50ms to avoid clicks from
  sudden read position jumps
- **Grain rate:** Can change immediately (next grain triggers at new interval)
- **Use SmoothedValue** for the delay time derived from tempo

```cpp
juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> delayTimeSmoothed;

void updateTempoSync() {
    float targetMs = noteDivisionToMs(currentDivision, currentBPM);
    float targetSamples = targetMs * 0.001f * sampleRate;
    delayTimeSmoothed.setTargetValue(targetSamples);
}
```

### Independent Sync for Delay Time and Grain Rate

Both delay time and grain rate can be tempo-synced independently:

```cpp
struct TempoSyncState {
    bool delaySyncEnabled = false;
    NoteDivision delayDivision = NoteDivision::Quarter;

    bool grainRateSyncEnabled = false;
    NoteDivision grainRateDivision = NoteDivision::Sixteenth;
};
```

When sync is enabled, the manual time/rate knob is overridden by the tempo-derived
value. When sync is disabled, the manual knob controls directly.

### Recommended Implementation

- Read BPM from `AudioPlayHead::getPosition()` every processBlock call
- Cache BPM as `std::atomic<double>` for UI access
- Convert note divisions using the lookup table above
- Smooth delay time changes over 50ms (SmoothedValue with multiplicative type)
- Independent sync toggles for delay time and grain rate
- Fallback BPM (120.0) when host does not provide tempo info

---

## 11. Parameter Smoothing

### SmoothedValue Configuration

JUCE provides `juce::SmoothedValue<float>` with two smoothing types:

- **Linear:** Constant-rate ramp. Good for amplitude, pan, mix levels.
- **Multiplicative:** Exponential ramp. Good for frequency, time, and rate parameters.

```cpp
// Linear smoothing for mix/amplitude parameters
juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> dryWetMix;
dryWetMix.reset(sampleRate, 0.02);  // 20ms ramp time

// Multiplicative smoothing for frequency parameters
juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> filterCutoff;
filterCutoff.reset(sampleRate, 0.05);  // 50ms ramp time
```

### Parameter Classification

| Parameter | Smoothing Type | Rate | Ramp Time | Notes |
|-----------|---------------|------|-----------|-------|
| Dry/Wet Mix | Linear | Sample | 20ms | Avoid zipper noise |
| Feedback | Linear | Sample | 20ms | Smooth to prevent pops |
| Delay Time | Multiplicative | Sample | 50ms | Longer to avoid pitch artifacts |
| Grain Rate | None | Block | N/A | Takes effect at next grain spawn |
| Grain Size | None | Block | N/A | Takes effect at next grain spawn |
| Diffusion Amount | Linear | Sample | 30ms | Coefficient interpolation |
| Filter Cutoffs | Multiplicative | Sample | 50ms | Prevents coefficient jumps |
| Spread | None | Block | N/A | Per-grain at spawn time |
| Detune | None | Block | N/A | Per-grain at spawn time |
| Reverse Mix | None | Block | N/A | Per-grain probability at spawn |
| Mutation Amount | Linear | Block | 30ms | Smooth to prevent jumps |
| Freeze | Custom | Sample | 10ms | Crossfade (see Freeze section) |

**Key insight:** Parameters that only affect NEW grains (size, rate, spread, detune,
reverse) do not need sample-rate smoothing. They naturally transition as old grains
fade out and new grains fade in. This saves significant CPU.

### Sample-Accurate vs Block-Rate

**Sample-accurate (must smooth per sample):**
- Dry/wet mix
- Feedback amount
- Delay time
- Filter cutoffs
- Diffusion amount/coefficients

These parameters directly affect every audio sample in the signal path. Changing them
abruptly causes audible artifacts (clicks, zipper noise).

**Block-rate (smooth once per processBlock call):**
- Grain rate, grain size, spread, detune, reverse mix, mutation
- These are "event" parameters that only matter when a grain spawns

```cpp
void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    // Update block-rate parameters once
    float currentGrainSize = grainSizeParam.load();
    float currentGrainRate = grainRateParam.load();
    float currentSpread = spreadParam.load();

    // Process sample-accurate parameters per sample
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        float currentMix = dryWetSmoothed.getNextValue();
        float currentFeedback = feedbackSmoothed.getNextValue();
        // ... process audio with smoothed values
    }
}
```

### Ramp Time Selection

| Context | Recommended Ramp | Rationale |
|---------|-----------------|-----------|
| Mix/amplitude knobs | 10-20ms | Fast enough to feel responsive, slow enough to prevent clicks |
| Filter frequency | 30-50ms | Prevents coefficient jumps that cause ringing |
| Delay time | 50-100ms | Longer ramp prevents pitch artifacts from read pointer jumping |
| Freeze on/off | 10ms | Quick but click-free |

### Avoiding Zipper Noise

1. **Always use SmoothedValue** for parameters that affect the signal path directly.
2. **Never read raw parameter values** on the audio thread for continuous parameters.
3. **Use `getNextValue()`** in the per-sample loop, not `getCurrentValue()`.
4. **Check `isSmoothing()`** to skip smoothing overhead when target is reached.
5. **Multiplicative smoothing for log-scale parameters** (frequency, time) -- linear
   smoothing on a log-scale parameter sounds uneven (too slow at low values, too fast
   at high values).

### Recommended Implementation

- Use `juce::SmoothedValue<float, Linear>` for mix, feedback, amplitude
- Use `juce::SmoothedValue<float, Multiplicative>` for frequency, time, rate
- Block-rate update for grain-spawn parameters (no smoothing needed)
- Ramp times: 20ms (mix), 50ms (filter/time), 10ms (freeze)
- Always call `getNextValue()` in the per-sample loop

---

## 12. Implementation Priority

Components ordered by dependency -- implement in this sequence:

### Phase 1: Foundation (Week 1)
1. **Circular Delay Buffer** -- Everything depends on this. Stereo, power-of-2,
   Hermite interpolation.
2. **Parameter Smoothing** -- Needed by all components from the start.
3. **Basic Feedback Path** -- Feedback gain, soft clipper, DC blocker. Enables
   hearing the delay effect immediately.

### Phase 2: Grain Engine (Week 2)
4. **Grain Scheduler + Pool** -- Fixed-rate triggering, grain pool management,
   voice stealing.
5. **Grain Windowing** -- Hann, Triangle, Square, Ramp envelopes.
6. **Overlap-Add Output** -- Summing active grains to stereo output.
7. **Detune / Pitch Shifting** -- Per-grain playback rate with Hermite interpolation.

### Phase 3: Feedback Processing (Week 3)
8. **HP/LP Filters in Feedback** -- 1-pole filters for feedback path tone shaping.
9. **Stereo Spread** -- Per-grain panning with equal-power law.
10. **Allpass Diffusion Network** -- 4-stage cascaded Schroeder allpass.

### Phase 4: Unique Features (Week 4)
11. **Reverse Grain Mix** -- Per-grain forward/reverse probability.
12. **Grain Mutation** -- Pass count tracking, progressive parameter drift.
13. **Frequency-Dependent Grain Size** -- LR4 crossover, per-band grain engines.

### Phase 5: Integration (Week 5)
14. **Freeze Mode** -- Buffer capture, crossfade, MIDI trigger.
15. **Tempo Sync** -- AudioPlayHead BPM reading, note division conversion.
16. **Final Tuning** -- Safety limiter, gain staging, CPU optimization.

### Rationale

- Phase 1 gets audio flowing through the plugin immediately (basic delay)
- Phase 2 adds the core granular character
- Phase 3 shapes the sound quality and spatial image
- Phase 4 adds the differentiating features (can be developed in parallel)
- Phase 5 handles integration features and polish

---

## References

### Academic / Technical
- Schroeder, M.R. "Natural Sounding Artificial Reverberation" (1962, AES)
- Roads, C. "Microsound" (MIT Press, 2001) -- Comprehensive granular synthesis reference
- Bencina, R. "Implementing Real-Time Granular Synthesis" (Audio Anecdotes III, 2006)
- Linkwitz, S. & Riley, R. "Active Crossover Networks for Noncoincident Drivers" (1976, AES)
- Smith, J.O. "Physical Audio Signal Processing" (CCRMA, Stanford) -- Allpass filter theory
- Fasciani, S. & Wyse, L. "Spectral Granular Synthesis" (ICMC 2018)

### Implementation Resources
- Valhalla DSP Blog: "Reverbs: Diffusion, allpass delays, and metallic artifacts" (2011)
  https://valhalladsp.com/2011/01/21/reverbs-diffusion-allpass-delays-and-metallic-artifacts/
- Valhalla DSP Blog: "ValhallaDelay: The Diffusion Section" (2019)
  https://valhalladsp.com/2019/06/13/valhalladelay-the-diffusion-section/
- musicdsp.org: "4th order Linkwitz-Riley filters"
  https://www.musicdsp.org/en/latest/Filters/266-4th-order-linkwitz-riley-filters.html
- musicdsp.org: "Variable-hardness clipping function"
  https://www.musicdsp.org/en/latest/Effects/104-variable-hardness-clipping-function.html
- JUCE Documentation: SmoothedValue, AudioPlayHead, dsp::IIR, dsp::LinkwitzRileyFilter
  https://docs.juce.com/

### JUCE-Specific
- JUCE Forum: Circular buffer design patterns
  https://forum.juce.com/t/whats-the-simplest-way-to-make-a-circular-buffer-using-juce/33194
- JUCE Tutorial: DSP Delay Line
  https://juce.com/tutorials/tutorial_dsp_delay_line/
- JUCE Forum: SmoothedValue best practices
  https://forum.juce.com/t/how-do-i-smooth-changes-to-an-audioparameterfloats-value/34334
