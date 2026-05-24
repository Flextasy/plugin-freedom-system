# Degrain — Implementation Plan

**Plugin:** Degrain — Granular Delay
**Type:** Audio Effect (Stereo In / Stereo Out)
**Generated:** 2026-02-14
**Stage:** 0 (Implementation Planning)

---

## 1. Complexity Calculation

### 1.1 Parameters (19 total)

**Formula:** `min(paramCount / 5, 2.0)`
**Calculation:** `19 / 5 = 3.8 → capped at 2.0`
**Score:** **2.0**

### 1.2 DSP Algorithms (14 components)

**Components:**
1. Circular Delay Buffer
2. Grain Engine (scheduling + pool)
3. Grain Windowing (4 window types)
4. Frequency-Dependent Grain Sizing
5. Grain Mutation Engine
6. Reverse Grain Mix
7. Allpass Diffusion Network (4 stages)
8. Stereo Spread
9. Detune / Pitch Shift
10. Feedback Path (clipper + DC blocker + filters)
11. Freeze System
12. Tempo Sync
13. Dry/Wet Mixer
14. Parameter Smoothing

**Complexity Weighting:**
- Simple (Buffer, Mixer, Smoothing, Tempo Sync): 4 × 0.2 = 0.8
- Medium (Grain Engine, Windowing, Reverse, Spread, Detune, Freeze): 6 × 0.5 = 3.0
- High (Diffusion, Feedback, Mutation, Freq-Dependent Sizing): 4 × 1.0 = 4.0

**Total Algorithm Score:** 0.8 + 3.0 + 4.0 = **7.8** (cap at 5.0 for calculation purposes)

### 1.3 Unique Features

**Features:**
1. **Feedback Loop:** +1.0 (circular dependency, requires careful ordering)
2. **Frequency Analysis:** +1.0 (real-time energy tracking per band)
3. **Custom Grain Engine:** +1.0 (64-grain pool, voice stealing, overlap-add)
4. **Tempo Sync:** +0.5 (AudioPlayHead, note division math)

**Score:** 1.0 + 1.0 + 1.0 + 0.5 = **3.5**

### 1.4 Total Complexity

**Formula:** `Parameters + min(Algorithms, 5.0) + Features`
**Calculation:** `2.0 + 5.0 + 3.5 = 10.5`
**Normalized (cap at 5.0):** **5.0**

**Complexity Rating:** **HIGH** (score 5.0 / 5.0)

---

## 2. Implementation Strategy

### 2.1 Strategy Selection

**Complexity Score:** 5.0 (≥ 3.0)
**Selected Strategy:** **Phase-Based Implementation**

**Rationale:**
- High complexity (multiple interdependent DSP components)
- Unique features require careful testing and iteration
- Phased approach allows for incremental testing and validation
- Reduces risk of cascading bugs (test each phase before adding next)

### 2.2 Development Phases Overview

**Phase 1-3:** Project Setup (Standard)
**Phase 4:** DSP Implementation (3 sub-phases)
**Phase 5:** GUI Implementation (3 sub-phases)
**Phase 6:** Testing & Optimization (Standard)

---

## 3. DSP Implementation Phases

### Phase 4.1: Core Processing (Foundation)

**Goal:** Get audio flowing through the plugin with basic delay and grain playback

**Components:**
1. **Circular Delay Buffer**
   - Power-of-2 sizing (1,048,576 samples per channel)
   - Write/read pointer management
   - Hermite interpolation for fractional reads
   - Test: Write input, read with delay, hear delayed signal

2. **Basic Grain Engine**
   - 64-grain pool (pre-allocated structs)
   - Fixed-rate scheduler (grain rate parameter)
   - Hann window only (simplest, smoothest)
   - Forward playback only (no reverse yet)
   - No pitch shift (playback rate = 1.0)
   - Mono grain placement (no stereo spread yet)
   - Test: Spawn grains at 10 Hz, hear granular delay

3. **Grain Windowing**
   - Implement all 4 window types (Hann, Triangle, Square, Ramp)
   - Function pointer or switch-based selection
   - Test: Switch between window types, hear character change

4. **Dry/Wet Mixer**
   - Simple crossfade: `output = dry * (1 - mix) + wet * mix`
   - SmoothedValue (Linear, 20ms)
   - Test: 0% = bypass, 100% = wet only, 50% = equal mix

**Test Criteria:**
- [ ] Audio passes through at 0% wet (bypass)
- [ ] Delay buffer writes and reads correctly (no clicks, no distortion)
- [ ] Grains trigger at correct rate (10 Hz = 10 grains/sec)
- [ ] Hann window produces smooth grains (no clicks)
- [ ] All 4 window types produce audible differences
- [ ] Dry/wet mix crossfades smoothly (no zipper noise)
- [ ] Plugin loads in DAW (AU/VST3) and processes audio

**Estimated Time:** 1 week

---

### Phase 4.2: Feedback & Character (Sound Shaping)

**Goal:** Add feedback loop, spatial processing, and timbral shaping

**Components:**
1. **Feedback Path**
   - Soft clipper (tanh or fast Pade approximation)
   - DC blocker (1-pole HPF at ~5 Hz, R = 0.995)
   - HP filter (1-pole, user-controlled, 20 Hz - 5 kHz)
   - LP filter (1-pole, user-controlled, 200 Hz - 20 kHz)
   - Feedback gain (0-115%, smoothed, 20ms)
   - Test: 40% feedback → multiple repeats, 100% → sustain, 110% → self-oscillation

2. **Allpass Diffusion Network**
   - 4 cascaded Schroeder allpass stages
   - Prime delay times: [113, 337, 601, 1009] samples at 48kHz
   - Coefficient: 0.7, scaled by diffusion parameter (0-100%)
   - Bypass when diffusion < 5% (save CPU)
   - Test: 0% = clean grains, 100% = washy, reverb-like

3. **Stereo Spread**
   - Per-grain random pan position (deterministic hash)
   - Equal-power panning law (cosine/sine)
   - Spread parameter scales pan range (0% = center, 100% = full stereo)
   - Test: 0% = mono, 50% = moderate spread, 100% = wide stereo field

4. **Detune / Pitch Shift**
   - Per-grain random detune within ±cents range (deterministic hash)
   - Playback rate: `pow(2, cents/1200)`
   - Hermite interpolation for fractional reads (reuse buffer interpolation)
   - Test: 0 cents = no pitch change, +50 cents = shimmer up, -50 cents = shimmer down

**Test Criteria:**
- [ ] Feedback at 40% produces 3-5 audible repeats
- [ ] Feedback at 100% sustains indefinitely (no amplitude growth)
- [ ] Feedback at 110% self-oscillates (stable, no runaway)
- [ ] Soft clipper prevents distortion at high feedback
- [ ] HP filter removes low frequencies from feedback trail
- [ ] LP filter darkens feedback trail (analog delay character)
- [ ] Diffusion at 0% = dry grains, 100% = dense wash
- [ ] Diffusion does not produce metallic artifacts (test with impulse)
- [ ] Stereo spread at 100% creates wide, immersive field
- [ ] Mono compatibility check (no phase cancellation)
- [ ] Detune at 0 cents = no pitch change
- [ ] Detune at ±50 cents = audible shimmer/chorus effect
- [ ] No aliasing or interpolation artifacts

**Estimated Time:** 1.5 weeks

---

### Phase 4.3: Advanced Features (Unique Selling Points)

**Goal:** Implement the 3 unique features that differentiate Degrain

**Components:**
1. **Reverse Grain Mix**
   - Per-grain forward/reverse decision (deterministic hash)
   - Reverse playback: envelope forward, audio backward
   - Reverse mix parameter: 0% = all forward, 100% = all reverse
   - Test: 50% mix → equal forward/reverse, "mirror echo" effect

2. **Grain Mutation**
   - Parallel pass count buffer (uint8_t, same size as delay buffer)
   - Increment pass count when writing feedback to buffer
   - Apply progressive drift: grain size (shrinks), pitch (drifts), density (increases)
   - Mutation amount parameter (0-100%)
   - Safety: cap pass count at 255, grain size min 5ms, density max 4x
   - Energy compensation: `amplitude *= 1.0 / sqrt(densityScale)`
   - Test: 0% = identical repeats, 100% = aggressive transformation

3. **Frequency-Dependent Grain Size**
   - 3-band energy analysis (low/mid/high) using RMS over ~100ms window
   - Linkwitz-Riley crossover (4th order) at 250 Hz and 2500 Hz
   - Size scaling: low × 3.0, mid × 1.0, high × 0.35
   - Freq spread parameter (0-100%) interpolates scaling amount
   - Test: 0% = uniform grain size, 100% = highs shatter, lows sustain

4. **Freeze System**
   - Freeze write gate (crossfade, 10ms)
   - Auto mode: Mutation = 0% → clean freeze, Mutation > 0% → live freeze
   - MIDI note-on/off trigger (optional)
   - Test: Freeze ON → grains loop frozen buffer, all parameters still active

5. **Tempo Sync**
   - Read BPM from AudioPlayHead
   - Convert note division to milliseconds (18 options: 1/1, 1/1D, 1/1T, ..., 1/32T)
   - Independent sync for delay time and grain rate
   - Smooth tempo changes (SmoothedValue, Multiplicative, 50ms)
   - Fallback BPM: 120.0
   - Test: Sync to 120 BPM, 1/4 delay → 500ms, 1/8 grain rate → 4 Hz

**Test Criteria:**
- [ ] Reverse mix at 0% = all forward (normal delay)
- [ ] Reverse mix at 100% = all reverse (reverse delay)
- [ ] Reverse mix at 50% = equal mix (mirror echo effect)
- [ ] Mutation at 0% = identical repeats (standard delay)
- [ ] Mutation at 100% = dramatic transformation (shrinking grains, pitch drift)
- [ ] Mutation does not cause runaway behavior (test 10+ feedback passes)
- [ ] Pass count buffer tracks correctly (verify with logging)
- [ ] Freq spread at 0% = uniform grain size across spectrum
- [ ] Freq spread at 100% = highs have small grains, lows have large grains
- [ ] Frequency analysis responds correctly to sine waves at 100 Hz, 1 kHz, 5 kHz
- [ ] Freeze ON stops input writes, grains continue playing
- [ ] Freeze with Mutation = 0% → clean freeze (no feedback writes)
- [ ] Freeze with Mutation > 0% → live freeze (feedback writes continue)
- [ ] Freeze crossfade is smooth (no clicks on engage/disengage)
- [ ] Tempo sync at 120 BPM, 1/4 division → 500ms delay time
- [ ] Tempo sync responds to BPM changes (test with automation)
- [ ] Delay sync and grain rate sync operate independently

**Estimated Time:** 2 weeks

---

## 4. GUI Implementation Phases

### Phase 5.1: Layout & Basic Controls

**Goal:** Set up WebView and render the 5-section layout with basic controls

**Tasks:**
1. **WebView Integration**
   - Load HTML file from binary resource
   - Set up bidirectional communication (C++ ↔ JavaScript)
   - Test: HTML loads, displays placeholder text

2. **HTML/CSS Layout**
   - Header bar: Title ("DEGRAIN"), subtitle ("GRANULAR DELAY"), Freeze button, Dry/Wet knob
   - Body: 5 sections (Delay, Grain, Character, Degrain, Mix)
   - Panel backgrounds: `#1a1a26`, borders `rgba(255,255,255,0.06)`, radius 12px
   - Section headers: Colored accent per section (Delay: `#e85d3a`, Grain: `#3aaa5c`, etc.)
   - Test: Layout renders correctly, sections visible

3. **Rotary Knobs (HTML/CSS/JS)**
   - SVG-based rotary knob with arc fill
   - Drag interaction (up = increase, down = decrease)
   - Shift-drag for fine control
   - Double-click to reset to default
   - Hover shows value label
   - Test: Knob rotates smoothly, value updates

**Test Criteria:**
- [ ] HTML loads in WebView (no errors)
- [ ] 5 sections render with correct colors and labels
- [ ] Rotary knobs render (SVG arcs visible)
- [ ] Knobs respond to mouse drag (rotation visible)
- [ ] Window size is 900×480 (correct dimensions)

**Estimated Time:** 1 week

---

### Phase 5.2: Parameter Binding (Full 2-Way Communication)

**Goal:** Connect all 19 parameters between HTML and JUCE

**Tasks:**
1. **C++ → HTML (Parameter Updates)**
   - Send parameter values to HTML on change
   - Update knob positions, toggle states, combo box selections
   - Test: Change parameter in DAW, HTML updates

2. **HTML → C++ (User Interactions)**
   - Send parameter changes from HTML to JUCE
   - Update APVTS parameter values
   - Test: Drag knob in HTML, parameter changes in DAW

3. **Sync Toggles & Combo Boxes**
   - Delay Sync toggle shows/hides division combo box
   - Grain Rate Sync toggle shows/hides division combo box
   - Test: Toggle ON → combo box appears, Toggle OFF → combo box hides

4. **Grain Shape Selector**
   - 4-button segmented selector (Sine, Triangle, Square, Ramp)
   - Active button highlighted with accent color
   - Test: Click button → grain shape parameter updates

5. **Freeze Button**
   - Toggle button with green glow when active
   - Active background: `#2a8a4a`, glow: `0 0 16px rgba(58, 170, 92, 0.4)`
   - Test: Click → freeze parameter toggles, button glows

6. **Horizontal Sliders (Low Cut, High Cut)**
   - Low Cut: fills left-to-right
   - High Cut: fills right-to-left
   - Test: Drag slider → parameter updates, fill direction correct

**Test Criteria:**
- [ ] All 19 parameters update HTML when changed in DAW
- [ ] All 19 parameters update APVTS when changed in HTML
- [ ] Sync toggles show/hide combo boxes correctly
- [ ] Grain shape selector updates parameter on click
- [ ] Freeze button toggles and glows when active
- [ ] Low Cut slider fills left-to-right
- [ ] High Cut slider fills right-to-left
- [ ] No lag or stuttering during parameter changes

**Estimated Time:** 1.5 weeks

---

### Phase 5.3: Polish & Visual Feedback

**Goal:** Add value formatting, visual feedback, and final polish

**Tasks:**
1. **Value Formatting**
   - Delay Time: "250.0 ms" or "1/4" (if synced)
   - Grain Size: "80.0 ms"
   - Grain Rate: "10.0 Hz" or "1/8" (if synced)
   - Feedback: "40 %" with red color above 100% (warning)
   - Diffusion, Spread, Reverse, Mutation, Freq Spread: "50 %"
   - Detune: "+12 ct" or "-12 ct" (bipolar)
   - Low Cut, High Cut: "200 Hz" or "2.5 kHz" (auto-format)
   - Dry/Wet: "50 %" (always visible)

2. **Hover States**
   - Knobs: Glow on hover (`0 0 10px <accent>33`)
   - Active drag: Stronger glow (`0 0 14px <accent>44`)
   - Test: Hover over knob → glow appears

3. **Feedback Warning**
   - When feedback > 100%, knob arc turns red (`#e83030`)
   - Value label also turns red
   - Test: Set feedback to 110% → knob and value turn red

4. **Freeze Glow Animation**
   - When freeze is ON, button glows with pulsing animation (optional)
   - Subtle pulse: opacity 0.8 → 1.0 → 0.8 over 2 seconds
   - Test: Freeze ON → button glows, pulse visible

5. **Final Visual Polish**
   - Font: Inter (fallback to system sans-serif)
   - Letter spacing: Section headers (2.5px), param labels (1px)
   - Smooth transitions: All color/glow changes use CSS transitions (200ms)

**Test Criteria:**
- [ ] All parameters display correct units and formatting
- [ ] Hover states appear on knobs (glow visible)
- [ ] Feedback > 100% turns knob and value red
- [ ] Freeze button glows when active
- [ ] Font renders correctly (Inter or fallback)
- [ ] All transitions are smooth (no abrupt changes)
- [ ] UI feels responsive and polished

**Estimated Time:** 1 week

---

## 5. Phase Summary & Timeline

| Phase | Description | Components | Duration | Cumulative |
|-------|-------------|------------|----------|------------|
| 1 | Project Setup | CMake, JUCE, folder structure | 0.5 weeks | 0.5 weeks |
| 2 | Parameter Definition | APVTS, 19 parameters | 0.5 weeks | 1 week |
| 3 | Audio Scaffold | Empty processBlock, bypass | 0.5 weeks | 1.5 weeks |
| **4.1** | **Core Processing** | Buffer, grain engine, windowing, mixer | **1 week** | **2.5 weeks** |
| **4.2** | **Feedback & Character** | Feedback path, diffusion, spread, detune | **1.5 weeks** | **4 weeks** |
| **4.3** | **Advanced Features** | Reverse, mutation, freq sizing, freeze, sync | **2 weeks** | **6 weeks** |
| **5.1** | **Layout & Basic Controls** | WebView, HTML/CSS, rotary knobs | **1 week** | **7 weeks** |
| **5.2** | **Parameter Binding** | 2-way C++/HTML, sync toggles, selectors | **1.5 weeks** | **8.5 weeks** |
| **5.3** | **Polish** | Value formatting, hover states, warnings | **1 week** | **9.5 weeks** |
| 6 | Testing & Optimization | Bug fixes, CPU optimization, validation | 1 week | 10.5 weeks |

**Total Estimated Time:** 10.5 weeks (~2.5 months)

---

## 6. Implementation Notes

### 6.1 Thread Safety

**Critical Points:**
- Delay buffer: Audio thread only, no locking needed
- Grain pool: Audio thread only, no locking needed
- APVTS parameters: Atomic reads (JUCE guarantees this)
- Freeze state: Use `std::atomic<bool>` for cross-thread access
- Pass count buffer: Audio thread only, no locking needed

**No Mutexes Needed:** All audio processing is single-threaded (audio callback thread)

### 6.2 Performance Estimates

**Target:** <15% single-core CPU at 48kHz, <30% at 192kHz

**Optimization Opportunities:**
- SIMD for buffer operations (use `juce::FloatVectorOperations`)
- Fast tanh approximation (Pade approximant) for soft clipper
- Bypass diffusion network when diffusion < 5%
- Block-rate parameter updates for grain-spawn parameters (no per-sample smoothing needed)

**Profiling:** Use Xcode Instruments (macOS) or Very Sleepy (Windows) to identify bottlenecks

### 6.3 Latency Reporting

**Total Latency:** 2060 samples (diffusion network delay times sum)

**JUCE Implementation:**
```cpp
void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    // Calculate total diffusion delay
    int totalDelay = 113 + 337 + 601 + 1009;  // 2060 samples at 48kHz
    setLatencySamples(totalDelay);
}
```

**Alternative:** Report 0 latency (diffusion phase shift is musical, not a problem)

### 6.4 Denormal Protection

**JUCE Auto-Handles:** `juce::FloatVectorOperations::disableDenormalisedNumberSupport()` called at init

**Additional Safety:**
- Epsilon in division: `1.0f / (x + 1e-10f)`
- Flush-to-zero in filter states: `if (abs(state) < 1e-15f) state = 0.0f;`

### 6.5 Known Challenges

**Challenge 1: Frequency-Dependent Grain Size**
- **Risk:** Frequency analysis may not respond fast enough (grains spawn too quickly)
- **Mitigation:** Use longer analysis window (~100ms RMS), smooth size changes per grain
- **Fallback:** If too CPU-intensive, simplify to 2-band split (low/high only)

**Challenge 2: Grain Mutation Runaway**
- **Risk:** Density multiplier could cause CPU overload (too many grains)
- **Mitigation:** Hard cap density at 4x, reduce grain amplitude proportionally
- **Testing:** Stress test with 115% feedback + 100% mutation for 1 minute

**Challenge 3: Feedback Stability at >100%**
- **Risk:** Self-oscillation could produce harsh artifacts or instability
- **Mitigation:** Mandatory soft clipper, safety limiter at output, test thoroughly
- **Fallback:** Cap feedback at 100% if instability persists

---

## 7. Test Criteria Per Phase

### Phase 4.1 Test Criteria
- [ ] Audio passes through at 0% wet (bypass)
- [ ] Delay buffer writes and reads correctly (no clicks, no distortion)
- [ ] Grains trigger at correct rate (10 Hz = 10 grains/sec)
- [ ] Hann window produces smooth grains (no clicks)
- [ ] All 4 window types produce audible differences
- [ ] Dry/wet mix crossfades smoothly (no zipper noise)
- [ ] Plugin loads in DAW (AU/VST3) and processes audio

### Phase 4.2 Test Criteria
- [ ] Feedback at 40% produces 3-5 audible repeats
- [ ] Feedback at 100% sustains indefinitely (no amplitude growth)
- [ ] Feedback at 110% self-oscillates (stable, no runaway)
- [ ] Soft clipper prevents distortion at high feedback
- [ ] HP filter removes low frequencies from feedback trail
- [ ] LP filter darkens feedback trail (analog delay character)
- [ ] Diffusion at 0% = dry grains, 100% = dense wash
- [ ] Diffusion does not produce metallic artifacts (test with impulse)
- [ ] Stereo spread at 100% creates wide, immersive field
- [ ] Mono compatibility check (no phase cancellation)
- [ ] Detune at 0 cents = no pitch change
- [ ] Detune at ±50 cents = audible shimmer/chorus effect
- [ ] No aliasing or interpolation artifacts

### Phase 4.3 Test Criteria
- [ ] Reverse mix at 0% = all forward (normal delay)
- [ ] Reverse mix at 100% = all reverse (reverse delay)
- [ ] Reverse mix at 50% = equal mix (mirror echo effect)
- [ ] Mutation at 0% = identical repeats (standard delay)
- [ ] Mutation at 100% = dramatic transformation (shrinking grains, pitch drift)
- [ ] Mutation does not cause runaway behavior (test 10+ feedback passes)
- [ ] Pass count buffer tracks correctly (verify with logging)
- [ ] Freq spread at 0% = uniform grain size across spectrum
- [ ] Freq spread at 100% = highs have small grains, lows have large grains
- [ ] Frequency analysis responds correctly to sine waves at 100 Hz, 1 kHz, 5 kHz
- [ ] Freeze ON stops input writes, grains continue playing
- [ ] Freeze with Mutation = 0% → clean freeze (no feedback writes)
- [ ] Freeze with Mutation > 0% → live freeze (feedback writes continue)
- [ ] Freeze crossfade is smooth (no clicks on engage/disengage)
- [ ] Tempo sync at 120 BPM, 1/4 division → 500ms delay time
- [ ] Tempo sync responds to BPM changes (test with automation)
- [ ] Delay sync and grain rate sync operate independently

### Phase 5.1 Test Criteria
- [ ] HTML loads in WebView (no errors)
- [ ] 5 sections render with correct colors and labels
- [ ] Rotary knobs render (SVG arcs visible)
- [ ] Knobs respond to mouse drag (rotation visible)
- [ ] Window size is 900×480 (correct dimensions)

### Phase 5.2 Test Criteria
- [ ] All 19 parameters update HTML when changed in DAW
- [ ] All 19 parameters update APVTS when changed in HTML
- [ ] Sync toggles show/hide combo boxes correctly
- [ ] Grain shape selector updates parameter on click
- [ ] Freeze button toggles and glows when active
- [ ] Low Cut slider fills left-to-right
- [ ] High Cut slider fills right-to-left
- [ ] No lag or stuttering during parameter changes

### Phase 5.3 Test Criteria
- [ ] All parameters display correct units and formatting
- [ ] Hover states appear on knobs (glow visible)
- [ ] Feedback > 100% turns knob and value red
- [ ] Freeze button glows when active
- [ ] Font renders correctly (Inter or fallback)
- [ ] All transitions are smooth (no abrupt changes)
- [ ] UI feels responsive and polished

---

## 8. Success Metrics

**DSP Quality:**
- [ ] Grains are smooth (no clicks or pops at Hann window)
- [ ] Feedback is stable at 110% (self-oscillates without runaway)
- [ ] Diffusion adds depth without metallic artifacts
- [ ] Frequency-dependent sizing creates audible "crystalline" character
- [ ] Grain mutation produces evolving delay tails (not just fading)
- [ ] Reverse grain mix creates convincing "mirror echo" effect

**UI Quality:**
- [ ] WebView loads instantly (<500ms)
- [ ] Parameter changes respond immediately (<50ms perceived latency)
- [ ] Visuals match mockup (Clean Futurism aesthetic)
- [ ] All controls are intuitive and discoverable

**Performance:**
- [ ] CPU usage <15% single-core at 48kHz (average case)
- [ ] CPU usage <30% single-core at 192kHz (worst case)
- [ ] No audio dropouts or glitches under normal use
- [ ] Plugin loads and saves state correctly in all DAWs

**Compatibility:**
- [ ] Builds and runs on macOS (AU, VST3)
- [ ] Builds and runs on Windows (VST3)
- [ ] Supports sample rates: 44.1, 48, 88.2, 96, 176.4, 192 kHz
- [ ] Passes AU validation (macOS)
- [ ] Passes VST3 validation (pluginval)

---

**End of Implementation Plan**

Generated by: Plugin Freedom System
Date: 2026-02-14
Plugin: Degrain v3.0.0
Next Step: Begin Phase 1 (Project Setup) via plugin-workflow skill
