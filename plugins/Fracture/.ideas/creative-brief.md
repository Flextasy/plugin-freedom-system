# Fracture — Creative Brief

## Concept
Granular delay effect plugin. Audio is captured into a delay buffer and played back as time-delayed grains. The goal is a crystalline, crunchy, sound-design-oriented character — sound fracturing into shimmering particles.

## Plugin Type
Audio Effect (Stereo In / Stereo Out)

## Signal Flow

```
Input → Delay Buffer → Grain Engine → [Diffusion] → [Stereo Spread] → [Detune] → Output
                ↑            ↓                                                |
                ↑      [Reverse Mix]                                          |
                ↑                                                             |
                └──── HP/LP Filter ← Soft Clipper ← Grain Mutation ←─────────┘
                                      (feedback path)
```

- The grain engine reads from the delay buffer at a configurable delay time offset
- Grains can be played forward, reversed, or a mix of both (Reverse Mix)
- Grain size is frequency-dependent: smaller grains for highs, larger for lows
- Feedback path includes: grain mutation → soft clipper → HP/LP filter → back to buffer
- Each feedback pass mutates grain characteristics (size, pitch, density drift)
- Freeze mode captures the buffer and loops grains indefinitely

## Parameters

### 1. Delay Time
- **Time** — Delay time (1 ms – 2000 ms in free mode)
- **Sync** — Toggle tempo sync on/off
- **Sync Division** — Note value when synced:
  - Straight: 1/1, 1/2, 1/4, 1/8, 1/16, 1/32
  - Dotted: 1/1., 1/2., 1/4., 1/8., 1/16., 1/32.
  - Triplet: 1/1T, 1/2T, 1/4T, 1/8T, 1/16T, 1/32T

### 2. Grain Size
- **Size** — Base length/duration of each grain (5 ms – 500 ms)
- Actual grain size is frequency-dependent (see Section 12)

### 3. Grain Rate
- **Rate** — How frequently new grains are triggered / scan rate (0.1 Hz – 100 Hz)
- **Rate Sync** — Toggle tempo sync on/off (same as Delay Time sync)
- **Rate Sync Division** — Note value when synced:
  - Straight: 1/1, 1/2, 1/4, 1/8, 1/16, 1/32
  - Dotted: 1/1., 1/2., 1/4., 1/8., 1/16., 1/32.
  - Triplet: 1/1T, 1/2T, 1/4T, 1/8T, 1/16T, 1/32T
- At low rates: distinct rhythmic grains
- At high rates: dense, textural, almost granular-synthesis territory

### 4. Grain Shape (Window/Envelope)
- **Shape** — Selects the amplitude envelope applied to each grain:
  - **Sine** (Hann window) — smooth, rounded
  - **Triangle** — linear fade in/out
  - **Square** — hard edges, clicky, aggressive
  - **Ramp** (saw with peak at start, decay after) — percussive, plucky attack

### 5. Feedback
- **Feedback** — Amount of output fed back into the delay buffer (0% – 120%)
- Above 100% enables self-oscillation
- Mandatory soft clipper (tanh) in feedback path to prevent runaway clipping

### 6. Diffusion
- **Diffusion** — Amount of diffusion applied to grains (0% – 100%)
- Implementation: allpass diffusion network (similar to reverb early reflections)
- At 0%: clean, precise grains
- At 100%: smeared, washy, reverb-like grain tails

### 7. Stereo Spread
- **Spread** — Random stereo panning of individual grains (0% – 100%)
- At 0%: mono grain placement
- At 100%: each grain randomly panned across the stereo field
- Creates a wide, immersive spatial image

### 8. Detune
- **Detune** — Pitch shift applied to grains (-50 to +50 cents, default 0)
- Center (0): no pitch change
- Left (negative): grains pitched down
- Right (positive): grains pitched up
- Adds shimmer, tension, or harmonic movement depending on direction

### 9. Filter (in feedback path)
- **Low Cut** — High-pass filter frequency (20 Hz – 5 kHz, default 20 Hz)
- **High Cut** — Low-pass filter frequency (200 Hz – 20 kHz, default 20 kHz)
- Filters are placed in the feedback loop — each pass progressively shapes the tone
- At default values: no filtering (full spectrum feedback)
- Useful for darkening tails (lower High Cut) or thinning out lows (raise Low Cut)

### 10. Reverse Grain Mix
- **Reverse** — Ratio of reversed grains to forward grains (0% – 100%)
- At 0%: all grains play forward (normal delay)
- At 50%: half forward, half reversed — "mirror echo" effect
- At 100%: all grains play reversed
- Per-grain decision: each triggered grain is independently assigned forward or reverse based on this ratio

### 11. Grain Mutation (feedback evolution)
- **Mutation** — How much grain parameters mutate with each feedback pass (0% – 100%)
- At 0%: standard delay behavior, repeats are identical
- At 100%: aggressive transformation — each repeat is increasingly different
- What mutates per pass:
  - Grain size: gradually shrinks (sound fractures into finer particles)
  - Pitch: slight random drift (shimmer / detuning accumulates)
  - Density: gradually increases (more grains, more chaotic)
- The delay tail doesn't just fade — it **transforms**

### 12. Frequency-Dependent Grain Size
- **Freq Spread** — How much grain size varies by frequency (0% – 100%)
- At 0%: all grains use the same base Size value
- At 100%: full frequency-dependent scaling
  - High frequencies → smaller grains (crystalline shimmer, sharp detail)
  - Low frequencies → larger grains (warm, sustained, weighty)
- Implementation: internal crossover splits signal, grain size is scaled per band
- This is the core of the "crystalline" character — highs shatter, lows rumble

### 13. Freeze
- **Freeze** — Toggle button (on/off), automatable
- When activated: stops writing new audio to the buffer, grains play from frozen content indefinitely
- All other parameters (size, rate, shape, spread, detune, reverse, mutation) remain active on frozen material
- When deactivated: buffer resumes capturing live input
- MIDI-triggerable for performance use

### 14. Mix
- **Dry/Wet** — Blend between dry input and processed signal (0% – 100%)

## Sound Character
- **Crystalline** — frequency-dependent grain size creates glassy highs with warm lows
- **Crunchy** — square window + high feedback + grain collision artifacts
- **Evolving** — grain mutation means the delay tail transforms, not just decays
- **Dimensional** — reverse grain mix creates temporal depth, stereo spread creates spatial width
- **Sound design oriented** — a tool for creating evolving textures, glitchy rhythms, and shimmering atmospheres

## Unique Selling Points (what no competitor offers)
1. **Grain Mutation** — delay tail transforms with each feedback pass
2. **Frequency-Dependent Grain Size** — crystalline highs, warm lows from a single control
3. **Reverse Grain Mix** — per-grain forward/reverse blending, not just full-reverse mode

## Use Cases
- Sound design: creating textures, risers, impacts from simple sources
- Ambient: long delay + diffusion + spread + freeze for immersive pads
- Experimental / glitch: short grains + square shape + high feedback + mutation
- Electronic music: tempo-synced rhythmic grain patterns
- Live performance: freeze + parameter tweaking for real-time grain sculpting

## Processing Architecture Decisions

### Grain Engine
- **Single engine** for the full signal (not per-band)
- Freq Spread scales grain size by analyzing frequency content, not by splitting into separate engines
- **64 grain pool** — pre-allocated, voice stealing targets grain closest to completion
- Structural params (size, rate) apply only to new grains; playback params (pitch, pan) smooth on active grains

### Buffer
- **4-second circular buffer** (stereo), power-of-2 sizing for bitmask indexing
- Hermite (cubic) interpolation for fractional delay reads

### Diffusion
- **Post-mix** — single allpass chain applied to combined grain output (not per-grain)
- 4 cascaded Schroeder allpass stages, prime delay times

### Detune
- **Random spread** — each grain gets random pitch shift within ±detune range
- Creates richer, more chaotic shimmer vs fixed shift

### Freeze Behavior
- **Auto mode based on Mutation:**
  - Mutation = 0% → clean freeze (no writes to buffer, frozen material unchanged)
  - Mutation > 0% → live freeze (feedback continues writing, frozen material evolves)
- 10ms crossfade on freeze enter/exit

### Feedback Path
- Signal chain: output → grain mutation → soft clipper (tanh) → DC blocker → HP/LP filter → back to buffer
- 1-pole (6dB/oct) filters — musical analog-style darkening, no resonance buildup
- Max feedback capped at 115% for safe self-oscillation

## Technical Notes
- Soft clipper: `tanh()` saturation in feedback path
- Grain windowing: multiply each grain's audio by the selected envelope shape
- Stereo spread: per-grain random pan position, seeded per grain trigger (equal-power panning law)
- Diffusion: chain of 4 allpass filters, prime delay times [113, 337, 601, 1009] samples at 48kHz
- Detune: per-grain random pitch shift within ±cents range via playback rate variation
- Reverse: per-grain buffer read direction (forward or backward), envelope always plays forward
- Freq-dependent size: single engine, frequency analysis scales grain size (highs x0.35, lows x3.0)
- Grain mutation: parallel uint8_t buffer tracks feedback pass count, progressive drift of size/pitch/density
- Freeze: stop input writes (clean) or allow feedback writes (live), auto-switch based on Mutation amount
- Tempo sync: read BPM from host via `AudioPlayHead`, multiplicative SmoothedValue for BPM changes
- Filter in feedback: 1-pole HP/LP (6dB/oct)
- Parameter smoothing: sample-accurate for dry/wet, feedback, delay time, filters; block-rate for grain params
- No LFO/modulation — keep it simple, use DAW automation for movement
