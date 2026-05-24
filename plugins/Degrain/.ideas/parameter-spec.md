# Degrain — Parameter Specification

**Plugin:** Degrain — Granular Delay
**Type:** Audio Effect (Stereo In / Stereo Out)
**Version:** 3.0.0
**Generated:** 2026-02-14
**Total Parameters:** 19 (12 Float, 4 Choice, 3 Bool)

---

## Parameter Groups

1. **Delay** — 3 parameters (1 Float, 1 Bool, 1 Choice)
2. **Grain** — 5 parameters (2 Float, 1 Bool, 1 Choice, 1 Choice)
3. **Character** — 6 parameters (6 Float)
4. **Degrain** — 3 parameters (3 Float)
5. **Global** — 2 parameters (1 Bool, 1 Float)

---

## 1. Delay Parameters

### 1.1 Delay Time
- **ID:** `delay_time`
- **Type:** Float
- **Range:** 1.0 to 2000.0 ms
- **Default:** 250.0 ms
- **Skew:** 0.3 (logarithmic)
- **UI Control:** Rotary knob (48px, orange #e85d3a)
- **DSP Usage:** Delay buffer read offset. When tempo sync is OFF, this value directly controls the delay time. When tempo sync is ON, this parameter is ignored in favor of the synced division value.

### 1.2 Delay Sync Toggle
- **ID:** `delay_sync_toggle`
- **Type:** Bool
- **Range:** 0 (OFF) to 1 (ON)
- **Default:** 0 (OFF)
- **UI Control:** Toggle button ("SYNC")
- **DSP Usage:** Enables tempo-synced delay time. When ON, shows sync division combo box and calculates delay time from host BPM and selected note division. When OFF, uses free-running delay time parameter.

### 1.3 Delay Sync Division
- **ID:** `delay_sync_division`
- **Type:** Choice (18 options)
- **Range:** 0 to 17 (index)
- **Default:** 6 (1/4)
- **Options:**
  - 0: 1/1 (whole note)
  - 1: 1/1D (dotted whole)
  - 2: 1/1T (triplet whole)
  - 3: 1/2 (half note)
  - 4: 1/2D (dotted half)
  - 5: 1/2T (triplet half)
  - 6: 1/4 (quarter note)
  - 7: 1/4D (dotted quarter)
  - 8: 1/4T (triplet quarter)
  - 9: 1/8 (eighth note)
  - 10: 1/8D (dotted eighth)
  - 11: 1/8T (triplet eighth)
  - 12: 1/16 (sixteenth)
  - 13: 1/16D (dotted sixteenth)
  - 14: 1/16T (triplet sixteenth)
  - 15: 1/32 (thirty-second)
  - 16: 1/32D (dotted thirty-second)
  - 17: 1/32T (triplet thirty-second)
- **UI Control:** Combo box (dropdown)
- **DSP Usage:** Note division for tempo-synced delay time. Active only when `delay_sync_toggle` is ON. Calculate delay time as: `(60.0 / BPM) * 4.0 * division_multiplier` where division_multiplier accounts for straight/dotted/triplet.

---

## 2. Grain Parameters

### 2.1 Grain Size
- **ID:** `grain_size`
- **Type:** Float
- **Range:** 5.0 to 500.0 ms
- **Default:** 80.0 ms
- **Skew:** 0.3 (logarithmic)
- **UI Control:** Rotary knob (48px, green #3aaa5c)
- **DSP Usage:** Base grain duration. Each grain is windowed (Hann/Triangle/Square/Ramp) to this length. Actual grain size is frequency-dependent when `freq_spread` > 0 (highs get smaller grains, lows get larger grains).

### 2.2 Grain Rate
- **ID:** `grain_rate`
- **Type:** Float
- **Range:** 0.1 to 100.0 Hz
- **Default:** 10.0 Hz
- **Skew:** 0.3 (logarithmic)
- **UI Control:** Rotary knob (48px, green #3aaa5c)
- **DSP Usage:** Grain trigger frequency (grains per second). When tempo sync is OFF, this value directly controls the grain rate. When tempo sync is ON, this parameter is ignored in favor of the synced division value. Low rates (0.1-5 Hz) create rhythmic grains, high rates (20-100 Hz) create dense textures approaching granular synthesis.

### 2.3 Grain Rate Sync Toggle
- **ID:** `grain_rate_sync_toggle`
- **Type:** Bool
- **Range:** 0 (OFF) to 1 (ON)
- **Default:** 0 (OFF)
- **UI Control:** Toggle button ("SYNC")
- **DSP Usage:** Enables tempo-synced grain rate. When ON, shows sync division combo box and calculates grain rate from host BPM and selected note division. When OFF, uses free-running grain rate parameter.

### 2.4 Grain Rate Sync Division
- **ID:** `grain_rate_sync_division`
- **Type:** Choice (18 options)
- **Range:** 0 to 17 (index)
- **Default:** 9 (1/8)
- **Options:** Same as Delay Sync Division (1/1, 1/1D, 1/1T, ..., 1/32T)
- **UI Control:** Combo box (dropdown)
- **DSP Usage:** Note division for tempo-synced grain rate. Active only when `grain_rate_sync_toggle` is ON. Calculate grain rate (Hz) from division and BPM.

### 2.5 Grain Shape
- **ID:** `grain_shape`
- **Type:** Choice (4 options)
- **Range:** 0 to 3 (index)
- **Default:** 0 (Sine/Hann)
- **Options:**
  - 0: Sine (Hann window) — smooth, rounded
  - 1: Triangle — linear fade in/out
  - 2: Square — hard edges, clicky, aggressive
  - 3: Ramp — percussive, plucky attack
- **UI Control:** Segmented selector (4 buttons)
- **DSP Usage:** Amplitude envelope applied to each grain. Multiply grain audio by selected window function. Sine/Hann is smoothest (no clicks), Square is most aggressive (audible grain boundaries), Triangle is in-between, Ramp emphasizes attack.

---

## 3. Character Parameters

### 3.1 Feedback
- **ID:** `feedback`
- **Type:** Float
- **Range:** 0.0 to 120.0 %
- **Default:** 40.0 %
- **Skew:** linear
- **UI Control:** Rotary knob (48px, blue #3a8ed6), warning color above 100% (red #e83030)
- **DSP Usage:** Amount of output fed back into delay buffer. 0% = no feedback (single echo), 100% = infinite sustain (critical feedback), >100% = self-oscillation (requires soft clipper). Feedback path includes: grain mutation → tanh soft clipper → HP/LP filter → back to buffer. The soft clipper prevents runaway clipping above 100%.

### 3.2 Diffusion
- **ID:** `diffusion`
- **Type:** Float
- **Range:** 0.0 to 100.0 %
- **Default:** 30.0 %
- **Skew:** linear
- **UI Control:** Rotary knob (48px, blue #3a8ed6)
- **DSP Usage:** Amount of allpass diffusion applied to grains. 0% = clean, precise grains. 100% = smeared, washy, reverb-like grain tails. Implementation: chain of 4-8 allpass filters. Adds spatial complexity and thickness to the grain cloud.

### 3.3 Stereo Spread
- **ID:** `stereo_spread`
- **Type:** Float
- **Range:** 0.0 to 100.0 %
- **Default:** 50.0 %
- **Skew:** linear
- **UI Control:** Rotary knob (48px, blue #3a8ed6)
- **DSP Usage:** Random stereo panning of individual grains. 0% = mono grain placement (center). 100% = each grain randomly panned across full stereo field. Per-grain random pan position is seeded at grain trigger time. Creates wide, immersive spatial image.

### 3.4 Detune
- **ID:** `detune`
- **Type:** Float
- **Range:** -50.0 to +50.0 cents
- **Default:** 0.0 cents
- **Skew:** linear
- **UI Control:** Rotary knob (48px, blue #3a8ed6), bipolar (center notch at 0)
- **DSP Usage:** Pitch shift applied to all grains. 0 = no pitch change. Negative = grains pitched down. Positive = grains pitched up. Implementation: per-grain pitch shift via playback rate variation (sample rate conversion). Adds shimmer, tension, or harmonic movement.

### 3.5 Low Cut
- **ID:** `low_cut`
- **Type:** Float
- **Range:** 20.0 to 5000.0 Hz
- **Default:** 20.0 Hz
- **Skew:** 0.25 (logarithmic)
- **UI Control:** Horizontal slider (fills left-to-right, blue #3a8ed6)
- **DSP Usage:** High-pass filter frequency in feedback path. At default (20 Hz), no filtering (full spectrum feedback). Raising this value progressively removes low frequencies from each feedback pass, thinning out the delay tail. Implementation: simple 1-pole or biquad HP filter.

### 3.6 High Cut
- **ID:** `high_cut`
- **Type:** Float
- **Range:** 200.0 to 20000.0 Hz
- **Default:** 20000.0 Hz
- **Skew:** 0.25 (logarithmic)
- **UI Control:** Horizontal slider (fills right-to-left, blue #3a8ed6)
- **DSP Usage:** Low-pass filter frequency in feedback path. At default (20 kHz), no filtering (full spectrum feedback). Lowering this value progressively removes high frequencies from each feedback pass, darkening the delay tail. Implementation: simple 1-pole or biquad LP filter.

---

## 4. Degrain Parameters

### 4.1 Reverse Mix
- **ID:** `reverse_mix`
- **Type:** Float
- **Range:** 0.0 to 100.0 %
- **Default:** 0.0 %
- **Skew:** linear
- **UI Control:** Rotary knob (48px, gold #e8a832)
- **DSP Usage:** Ratio of reversed grains to forward grains. 0% = all grains play forward (normal delay). 50% = half forward, half reversed ("mirror echo" effect). 100% = all grains play reversed. Per-grain decision: each triggered grain is independently assigned forward or reverse playback direction based on this ratio. Creates temporal depth and reverse-delay effects.

### 4.2 Grain Mutation
- **ID:** `grain_mutation`
- **Type:** Float
- **Range:** 0.0 to 100.0 %
- **Default:** 0.0 %
- **Skew:** linear
- **UI Control:** Rotary knob (48px, gold #e8a832)
- **DSP Usage:** How much grain parameters mutate with each feedback pass. 0% = standard delay behavior (repeats are identical). 100% = aggressive transformation (each repeat is increasingly different). What mutates per pass: (1) Grain size gradually shrinks (sound degrains into finer particles), (2) Pitch slightly drifts randomly (shimmer/detuning accumulates), (3) Density gradually increases (more grains, more chaotic). The delay tail doesn't just fade — it transforms. Implementation: track feedback pass count, apply progressive parameter drift.

### 4.3 Frequency-Dependent Spread
- **ID:** `freq_spread`
- **Type:** Float
- **Range:** 0.0 to 100.0 %
- **Default:** 0.0 %
- **Skew:** linear
- **UI Control:** Rotary knob (48px, gold #e8a832)
- **DSP Usage:** How much grain size varies by frequency. 0% = all grains use the same base `grain_size` value. 100% = full frequency-dependent scaling (highs → smaller grains, lows → larger grains). This is the core of the "crystalline" character: highs shatter into glassy particles, lows remain warm and sustained. Implementation: internal 2-3 band crossover splits signal, grain size is scaled per band (e.g., low band: grain_size × 1.5, mid: grain_size × 1.0, high: grain_size × 0.5).

---

## 5. Global Parameters

### 5.1 Freeze
- **ID:** `freeze`
- **Type:** Bool
- **Range:** 0 (OFF) to 1 (ON)
- **Default:** 0 (OFF)
- **UI Control:** Toggle button (72×28px, green glow when active)
- **DSP Usage:** Stops writing new audio to the delay buffer, grains continue reading from frozen content indefinitely. All other parameters (size, rate, shape, spread, detune, reverse, mutation) remain active on frozen material. When deactivated, buffer resumes capturing live input. Useful for creating infinite grain clouds, freeze-and-process effects, and live performance.

### 5.2 Dry/Wet
- **ID:** `dry_wet`
- **Type:** Float
- **Range:** 0.0 to 100.0 %
- **Default:** 50.0 %
- **Skew:** linear
- **UI Control:** Rotary knob (56px large, purple #b55dba, always shows value)
- **DSP Usage:** Blend between dry input and processed signal. 0% = 100% dry (bypass). 100% = 100% wet (fully processed). 50% = equal mix. Standard crossfade implementation: `output = (dry × (1 - mix)) + (wet × mix)`.

---

## Parameter Summary Table

| ID | Type | Range | Default | Skew | UI Control | Section |
|----|------|-------|---------|------|------------|---------|
| delay_time | Float | 1–2000 ms | 250 ms | 0.3 | Rotary knob | Delay |
| delay_sync_toggle | Bool | 0–1 | 0 | — | Toggle button | Delay |
| delay_sync_division | Choice | 0–17 | 6 | — | Combo box | Delay |
| grain_size | Float | 5–500 ms | 80 ms | 0.3 | Rotary knob | Grain |
| grain_rate | Float | 0.1–100 Hz | 10 Hz | 0.3 | Rotary knob | Grain |
| grain_rate_sync_toggle | Bool | 0–1 | 0 | — | Toggle button | Grain |
| grain_rate_sync_division | Choice | 0–17 | 9 | — | Combo box | Grain |
| grain_shape | Choice | 0–3 | 0 | — | Segmented (4 btns) | Grain |
| feedback | Float | 0–120 % | 40 % | linear | Rotary knob | Character |
| diffusion | Float | 0–100 % | 30 % | linear | Rotary knob | Character |
| stereo_spread | Float | 0–100 % | 50 % | linear | Rotary knob | Character |
| detune | Float | -50–+50 ct | 0 ct | linear | Rotary knob (bipolar) | Character |
| low_cut | Float | 20–5000 Hz | 20 Hz | 0.25 | Horizontal slider | Character |
| high_cut | Float | 200–20000 Hz | 20000 Hz | 0.25 | Horizontal slider | Character |
| reverse_mix | Float | 0–100 % | 0 % | linear | Rotary knob | Degrain |
| grain_mutation | Float | 0–100 % | 0 % | linear | Rotary knob | Degrain |
| freq_spread | Float | 0–100 % | 0 % | linear | Rotary knob | Degrain |
| freeze | Bool | 0–1 | 0 | — | Toggle button | Global |
| dry_wet | Float | 0–100 % | 50 % | linear | Rotary knob (large) | Global |

---

## JUCE Parameter Creation Example

```cpp
// Example: Creating parameters in AudioProcessorValueTreeState

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // DELAY SECTION
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID {"delay_time", 1},
        "Delay Time",
        juce::NormalisableRange<float> (1.0f, 2000.0f, 0.01f, 0.3f),
        250.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1) + " ms"; }
    ));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID {"delay_sync_toggle", 1},
        "Delay Sync",
        false
    ));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID {"delay_sync_division", 1},
        "Delay Sync Division",
        juce::StringArray {"1/1", "1/1D", "1/1T", "1/2", "1/2D", "1/2T",
                           "1/4", "1/4D", "1/4T", "1/8", "1/8D", "1/8T",
                           "1/16", "1/16D", "1/16T", "1/32", "1/32D", "1/32T"},
        6  // Default: 1/4
    ));

    // GRAIN SECTION
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID {"grain_size", 1},
        "Grain Size",
        juce::NormalisableRange<float> (5.0f, 500.0f, 0.01f, 0.3f),
        80.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1) + " ms"; }
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID {"grain_rate", 1},
        "Grain Rate",
        juce::NormalisableRange<float> (0.1f, 100.0f, 0.01f, 0.3f),
        10.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1) + " Hz"; }
    ));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID {"grain_rate_sync_toggle", 1},
        "Grain Rate Sync",
        false
    ));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID {"grain_rate_sync_division", 1},
        "Grain Rate Sync Division",
        juce::StringArray {"1/1", "1/1D", "1/1T", "1/2", "1/2D", "1/2T",
                           "1/4", "1/4D", "1/4T", "1/8", "1/8D", "1/8T",
                           "1/16", "1/16D", "1/16T", "1/32", "1/32D", "1/32T"},
        9  // Default: 1/8
    ));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID {"grain_shape", 1},
        "Grain Shape",
        juce::StringArray {"Sine", "Triangle", "Square", "Ramp"},
        0  // Default: Sine
    ));

    // CHARACTER SECTION
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID {"feedback", 1},
        "Feedback",
        juce::NormalisableRange<float> (0.0f, 120.0f, 0.1f),
        40.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1) + " %"; }
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID {"diffusion", 1},
        "Diffusion",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        30.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1) + " %"; }
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID {"stereo_spread", 1},
        "Stereo Spread",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        50.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1) + " %"; }
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID {"detune", 1},
        "Detune",
        juce::NormalisableRange<float> (-50.0f, 50.0f, 0.1f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1) + " ct"; }
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID {"low_cut", 1},
        "Low Cut",
        juce::NormalisableRange<float> (20.0f, 5000.0f, 0.01f, 0.25f),
        20.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) {
            return value < 1000.0f ? juce::String (value, 0) + " Hz"
                                   : juce::String (value / 1000.0f, 2) + " kHz";
        }
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID {"high_cut", 1},
        "High Cut",
        juce::NormalisableRange<float> (200.0f, 20000.0f, 0.01f, 0.25f),
        20000.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) {
            return value < 1000.0f ? juce::String (value, 0) + " Hz"
                                   : juce::String (value / 1000.0f, 2) + " kHz";
        }
    ));

    // DEGRAIN SECTION
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID {"reverse_mix", 1},
        "Reverse Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1) + " %"; }
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID {"grain_mutation", 1},
        "Grain Mutation",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1) + " %"; }
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID {"freq_spread", 1},
        "Frequency-Dependent Spread",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1) + " %"; }
    ));

    // GLOBAL
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID {"freeze", 1},
        "Freeze",
        false
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID {"dry_wet", 1},
        "Dry/Wet",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        50.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [] (float value, int) { return juce::String (value, 1) + " %"; }
    ));

    return layout;
}
```

---

## Notes

- **Skew factors**: Logarithmic skew approximations: delay_time (0.3), grain_size (0.3), grain_rate (0.3), low_cut (0.25), high_cut (0.25). All others are linear.
- **Bipolar parameters**: Only `detune` is bipolar (center notch at 0).
- **Warning thresholds**: `feedback` shows red color above 100%.
- **Tempo sync**: When sync toggles are ON, the corresponding time/rate parameters are overridden by sync division calculations.
- **Parameter IDs**: MUST match exactly between APVTS and HTML `data-param` attributes (case-sensitive).

---

**Generated by:** Plugin Freedom System
**Date:** 2026-02-14
**Plugin:** Degrain v3.0.0
