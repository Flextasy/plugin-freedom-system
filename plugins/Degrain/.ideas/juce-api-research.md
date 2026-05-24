# JUCE API Research for Degrain — Granular Delay Plugin

**Date:** 2026-02-15
**JUCE Version:** 8.x
**Plugin Type:** Audio Effect (Granular Delay)
**UI Technology:** WebView (WebBrowserComponent)
**DSP Complexity:** High (granular synthesis, multi-tap delay, modulation)

---

## Key Findings Summary

### Critical Discoveries

1. **WebView Member Declaration Order is CRITICAL** — Relays → WebView → Attachments prevents release build crashes
2. **JUCE 8 WebSliderParameterAttachment requires 3 parameters** — Adding `nullptr` as third parameter (undoManager)
3. **DelayLine supports multiple interpolation types** — Linear (default), Lagrange3rd, Thiran for fractional delays
4. **SmoothedValue has two modes** — Linear for most params, Multiplicative for frequency/time (exponential)
5. **ES6 module imports required** — WebView HTML must use `type="module"` and `import { getSliderState }` syntax
6. **DryWetMixer provides automatic latency compensation** — Perfect for granular effects with processing delay
7. **AudioPlayHead::getPosition() returns Optional** — Must check for nullopt before accessing tempo/time signature
8. **ProcessSpec pattern is universal** — All `juce::dsp::` components use prepare(spec) + process(context)
9. **valueChangedEvent callbacks receive NO parameters** — Must call `getNormalisedValue()` inside callback
10. **NEEDS_WEB_BROWSER TRUE required in CMakeLists** — VST3 won't load without this flag

### Performance Considerations

- **ScopedNoDenormals** — Mandatory in processBlock for granular processing (many DSP operations)
- **Sample-by-sample vs block processing** — Granular effects need sample-accurate grain triggering
- **InterpolationType::Lagrange3rd** — Best quality for pitch-shifting granular delays (vs Linear)
- **SmoothedValue ramp time** — 10ms typical, longer for grain size/delay time (50-100ms)

---

## 1. WebView Integration (JUCE 8)

### WebBrowserComponent Architecture

```cpp
// PluginEditor.h
#include <juce_gui_extra/juce_gui_extra.h>

class DegrainAudioProcessorEditor : public juce::AudioProcessorEditor {
private:
    // ⚠️ CRITICAL: Member declaration order prevents release build crashes
    // Destruction happens in REVERSE order of declaration
    // Order: Relays → WebView → Attachments

    // 1️⃣ RELAYS FIRST (no dependencies)
    std::unique_ptr<juce::WebSliderRelay> delayTimeRelay;
    std::unique_ptr<juce::WebSliderRelay> grainSizeRelay;
    std::unique_ptr<juce::WebSliderRelay> feedbackRelay;
    std::unique_ptr<juce::WebToggleButtonRelay> freezeRelay;
    std::unique_ptr<juce::WebComboBoxRelay> syncModeRelay;
    // ... 14 more relays

    // 2️⃣ WEBVIEW SECOND (depends on relays via withOptionsFrom)
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // 3️⃣ ATTACHMENTS LAST (depend on both relays and parameters)
    std::unique_ptr<juce::WebSliderParameterAttachment> delayTimeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> grainSizeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> feedbackAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> freezeAttachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> syncModeAttachment;
    // ... 14 more attachments

    // Helper for resource serving
    std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String& url);
};
```

### WebView Construction Pattern

```cpp
// PluginEditor.cpp constructor
DegrainAudioProcessorEditor::DegrainAudioProcessorEditor(DegrainAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    // 1. Create relays FIRST
    delayTimeRelay = std::make_unique<juce::WebSliderRelay>("DELAY_TIME");
    grainSizeRelay = std::make_unique<juce::WebSliderRelay>("GRAIN_SIZE");
    feedbackRelay = std::make_unique<juce::WebSliderRelay>("FEEDBACK");
    freezeRelay = std::make_unique<juce::WebToggleButtonRelay>("FREEZE");
    syncModeRelay = std::make_unique<juce::WebComboBoxRelay>("SYNC_MODE");
    // ... create all 19 relays

    // 2. Create WebView with relay options chained via withOptionsFrom()
    webView = std::make_unique<juce::WebBrowserComponent>(
        juce::WebBrowserComponent::Options{}
            .withNativeIntegrationEnabled()
            .withResourceProvider([this](auto& url) { return getResource(url); })
            .withOptionsFrom(*delayTimeRelay)
            .withOptionsFrom(*grainSizeRelay)
            .withOptionsFrom(*feedbackRelay)
            .withOptionsFrom(*freezeRelay)
            .withOptionsFrom(*syncModeRelay)
            // ... chain all 19 relays
    );

    // 3. Create attachments LAST (JUCE 8 requires 3 parameters!)
    delayTimeAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *processorRef.parameters.getParameter("DELAY_TIME"), *delayTimeRelay, nullptr);
    grainSizeAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *processorRef.parameters.getParameter("GRAIN_SIZE"), *grainSizeRelay, nullptr);
    feedbackAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *processorRef.parameters.getParameter("FEEDBACK"), *feedbackRelay, nullptr);
    freezeAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *processorRef.parameters.getParameter("FREEZE"), *freezeRelay, nullptr);
    syncModeAttachment = std::make_unique<juce::WebComboBoxParameterAttachment>(
        *processorRef.parameters.getParameter("SYNC_MODE"), *syncModeRelay, nullptr);
    // ... create all 19 attachments

    addAndMakeVisible(*webView);
    webView->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());

    setSize(800, 600);
}
```

### Resource Provider Pattern

```cpp
std::optional<juce::WebBrowserComponent::Resource>
DegrainAudioProcessorEditor::getResource(const juce::String& url)
{
    // Helper lambda for converting BinaryData to std::vector<std::byte>
    auto makeVector = [](const char* data, int size) {
        return std::vector<std::byte>(
            reinterpret_cast<const std::byte*>(data),
            reinterpret_cast<const std::byte*>(data) + size
        );
    };

    // ⚠️ CRITICAL: Explicit URL mapping (NOT generic loop)
    // BinaryData flattens paths: js/juce/index.js → index_js
    // HTML requests original path: /js/juce/index.js

    if (url == "/" || url == "/index.html") {
        return juce::WebBrowserComponent::Resource {
            makeVector(BinaryData::index_html, BinaryData::index_htmlSize),
            juce::String("text/html")
        };
    }

    if (url == "/js/juce/index.js") {
        return juce::WebBrowserComponent::Resource {
            makeVector(BinaryData::index_js, BinaryData::index_jsSize),
            juce::String("text/javascript")
        };
    }

    if (url == "/js/juce/check_native_interop.js") {
        return juce::WebBrowserComponent::Resource {
            makeVector(BinaryData::check_native_interop_js,
                      BinaryData::check_native_interop_jsSize),
            juce::String("text/javascript")
        };
    }

    if (url == "/css/styles.css") {
        return juce::WebBrowserComponent::Resource {
            makeVector(BinaryData::styles_css, BinaryData::styles_cssSize),
            juce::String("text/css")
        };
    }

    // Add mappings for all resources...

    return std::nullopt;  // 404
}
```

### JavaScript Bridge (JUCE Frontend Module)

**CRITICAL: ES6 Module Import Pattern**

```html
<!-- index.html -->
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Degrain</title>
    <link rel="stylesheet" href="css/styles.css">
</head>
<body>
    <div id="app">
        <!-- Knobs, meters, visualizations -->
    </div>

    <!-- ⚠️ CRITICAL: type="module" required for ES6 imports -->
    <script type="module" src="js/juce/index.js"></script>
    <script type="module">
        import { getSliderState, getToggleState, getComboBoxState } from './js/juce/index.js';

        // Get parameter states
        const delayTimeState = getSliderState("DELAY_TIME");
        const freezeState = getToggleState("FREEZE");
        const syncModeState = getComboBoxState("SYNC_MODE");

        // ⚠️ CRITICAL: valueChangedEvent callback receives NO parameters
        // Must call getNormalisedValue() INSIDE callback
        delayTimeState.valueChangedEvent.addListener(() => {
            const value = delayTimeState.getNormalisedValue();  // 0.0 - 1.0
            updateKnob(delayTimeKnob, value);
        });

        freezeState.valueChangedEvent.addListener(() => {
            const value = freezeState.getValue();  // true/false
            updateToggle(freezeToggle, value);
        });

        syncModeState.valueChangedEvent.addListener(() => {
            const index = syncModeState.getChoiceIndex();  // 0-based index
            updateComboBox(syncModeDropdown, index);
        });

        // Knob drag handler (relative drag, not absolute positioning)
        let lastY = 0;
        delayTimeKnob.addEventListener('mousedown', (e) => {
            lastY = e.clientY;
            delayTimeState.sliderDragStarted();
        });

        document.addEventListener('mousemove', (e) => {
            if (!isDragging) return;

            const deltaY = lastY - e.clientY;  // Frame-delta, NOT total distance
            const currentValue = delayTimeState.getNormalisedValue();
            const newValue = Math.max(0, Math.min(1, currentValue + deltaY * 0.005));

            delayTimeState.setNormalisedValue(newValue);
            lastY = e.clientY;  // Update for next frame
        });

        document.addEventListener('mouseup', () => {
            if (isDragging) {
                delayTimeState.sliderDragEnded();
                isDragging = false;
            }
        });
    </script>
</body>
</html>
```

**Key JavaScript API Methods:**

- `getSliderState(paramId)` — Returns SliderState for continuous parameters
- `getToggleState(paramId)` — Returns ToggleState for boolean parameters
- `getComboBoxState(paramId)` — Returns ComboBoxState for choice parameters
- `getNativeFunction(name)` — Binds C++ function for custom callbacks

**SliderState Methods:**

- `getNormalisedValue()` — Returns 0.0-1.0 normalized value
- `setNormalisedValue(value)` — Sets normalized value (triggers C++ update)
- `getScaledValue()` — Returns value mapped through NormalisableRange
- `sliderDragStarted()` — Call on mousedown (enables host automation)
- `sliderDragEnded()` — Call on mouseup (finalizes automation gesture)
- `valueChangedEvent.addListener(callback)` — Callback receives NO parameters!

**ToggleState Methods:**

- `getValue()` — Returns boolean (true/false)
- `setValue(bool)` — Sets boolean value
- `valueChangedEvent.addListener(callback)` — Callback receives NO parameters!

**ComboBoxState Methods:**

- `getChoiceIndex()` — Returns 0-based index
- `setChoiceIndex(index)` — Sets choice by index
- `properties.choices` — Array of choice strings
- `valueChangedEvent.addListener(callback)` — Callback receives NO parameters!

---

## 2. AudioProcessorValueTreeState (APVTS)

### Creating Parameters

```cpp
// PluginProcessor.h
class DegrainAudioProcessor : public juce::AudioProcessor {
public:
    juce::AudioProcessorValueTreeState parameters;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
};

// PluginProcessor.cpp
DegrainAudioProcessor::DegrainAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout
DegrainAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Float parameter with linear range
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("DRY_WET", 1),
        "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),  // min, max, step
        50.0f  // default
    ));

    // Float parameter with LOGARITHMIC skew (for time/frequency)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("DELAY_TIME", 1),
        "Delay Time",
        juce::NormalisableRange<float>(
            10.0f,    // min (ms)
            5000.0f,  // max (ms)
            0.1f,     // step
            0.3f      // skew factor (<1.0 = logarithmic, >1.0 = exponential)
        ),
        500.0f  // default
    ));

    // Float parameter with custom skew + symmetrical skew center
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("GRAIN_SIZE", 1),
        "Grain Size",
        juce::NormalisableRange<float>(
            1.0f,      // min (ms)
            200.0f,    // max (ms)
            0.1f,      // step
            0.4f,      // skew factor
            false      // symmetrical skew
        ),
        50.0f  // default
    ));

    // Boolean parameter
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("FREEZE", 1),
        "Freeze",
        false  // default
    ));

    // Choice parameter (dropdown/combo box)
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("SYNC_MODE", 1),
        "Sync Mode",
        juce::StringArray("Free", "1/16", "1/8", "1/4", "1/2", "1 Bar"),
        0  // default index
    ));

    // Integer parameter
    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("GRAIN_COUNT", 1),
        "Grain Count",
        1,    // min
        16,   // max
        4     // default
    ));

    return layout;
}
```

### NormalisableRange Skew Factor

**Skew < 1.0 (Logarithmic):**
- Use for time, frequency, delay, grain size
- More precision at low values, less at high values
- Typical values: 0.3 (strong log), 0.5 (medium log)

**Skew = 1.0 (Linear):**
- Use for dry/wet, feedback, volume (linear perception)
- Even spacing across entire range

**Skew > 1.0 (Exponential):**
- Rare, opposite of logarithmic
- More precision at high values

```cpp
// Example skew values for Degrain parameters:
// DELAY_TIME: 0.3 (strong logarithmic, responsive at low values)
// GRAIN_SIZE: 0.4 (logarithmic, precise small grains)
// FEEDBACK: 1.0 (linear)
// DRY_WET: 1.0 (linear)
// FILTER_FREQ: 0.3 (logarithmic, typical for frequency)
```

### Thread-Safe Parameter Access (Audio Thread)

```cpp
// PluginProcessor.h
class DegrainAudioProcessor : public juce::AudioProcessor {
private:
    // Cached raw parameter pointers (set in constructor, never change)
    std::atomic<float>* delayTimeParam = nullptr;
    std::atomic<float>* feedbackParam = nullptr;
    std::atomic<float>* dryWetParam = nullptr;
};

// PluginProcessor.cpp constructor
DegrainAudioProcessor::DegrainAudioProcessor()
    : parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    // Cache raw parameter pointers (ONCE in constructor)
    delayTimeParam = parameters.getRawParameterValue("DELAY_TIME");
    feedbackParam = parameters.getRawParameterValue("FEEDBACK");
    dryWetParam = parameters.getRawParameterValue("DRY_WET");
}

void DegrainAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // ✅ SAFE: Read atomics on audio thread
    const float delayTimeMs = delayTimeParam->load();
    const float feedback = feedbackParam->load();
    const float dryWet = dryWetParam->load();

    // ❌ UNSAFE: Never call getParameter() on audio thread (allocates!)
    // float delayTimeMs = *parameters.getRawParameterValue("DELAY_TIME");

    // Use SmoothedValue to avoid zipper noise
    delayTimeSmoothed.setTargetValue(delayTimeMs);
    feedbackSmoothed.setTargetValue(feedback);

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
        float smoothedDelay = delayTimeSmoothed.getNextValue();
        float smoothedFeedback = feedbackSmoothed.getNextValue();

        // Process audio...
    }
}
```

### State Save/Restore

```cpp
void DegrainAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // APVTS handles all parameter serialization automatically
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void DegrainAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName(parameters.state.getType())) {
        parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}
```

---

## 3. DSP Module Classes

### juce::dsp::DelayLine

**Template Parameters:**

```cpp
template<typename SampleType, typename InterpolationType = DelayLineInterpolationTypes::Linear>
class juce::dsp::DelayLine
```

**Available Interpolation Types:**

- `DelayLineInterpolationTypes::None` — No interpolation (integer delays only)
- `DelayLineInterpolationTypes::Linear` — Default, good quality, low CPU
- `DelayLineInterpolationTypes::Lagrange3rd` — High quality, 3rd-order polynomial (best for granular)
- `DelayLineInterpolationTypes::Thiran` — Allpass-based, maximally flat group delay

**For Degrain:** Use `Lagrange3rd` for highest quality pitch-shifting grains.

**Core Methods:**

```cpp
// Setup (call in prepareToPlay)
void prepare(const juce::dsp::ProcessSpec& spec);
void setMaximumDelayInSamples(int maxDelayInSamples);  // ⚠️ Allocates! Never on audio thread
void reset();  // Clear buffer contents

// Processing (audio thread safe)
void setDelay(SampleType newDelayInSamples);  // Set delay for subsequent operations
SampleType getDelay() const;  // Get current delay setting

void pushSample(int channel, SampleType sample);  // Write to delay line
SampleType popSample(int channel,
                     SampleType delayInSamples = -1,  // -1 = use setDelay() value
                     bool updateReadPointer = true);  // false = multi-tap

// Block processing (when delay is constant)
template<typename ProcessContext>
void process(const ProcessContext& context) noexcept;
```

**Usage Pattern for Granular Delay:**

```cpp
// PluginProcessor.h
class DegrainAudioProcessor : public juce::AudioProcessor {
private:
    // Use Lagrange3rd for high-quality pitch-shifting
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayLine;
};

// prepareToPlay()
void DegrainAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    // 5 seconds max delay at current sample rate
    delayLine.setMaximumDelayInSamples(static_cast<int>(5.0 * sampleRate));
    delayLine.prepare(spec);
    delayLine.reset();
}

// processBlock() - sample-by-sample for modulation
void DegrainAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
        auto* channelData = buffer.getWritePointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
            // Read delay time parameter (modulated per-sample)
            float delayTimeMs = delayTimeSmoothed.getNextValue();
            float delaySamples = (delayTimeMs / 1000.0f) * sampleRate;

            // Read delayed sample (fractional delay via Lagrange interpolation)
            float delayedSample = delayLine.popSample(channel, delaySamples);

            // Process feedback
            float inputSample = channelData[sample];
            float feedbackGain = feedbackSmoothed.getNextValue();
            float delayInput = inputSample + (delayedSample * feedbackGain);

            // Write to delay line
            delayLine.pushSample(channel, delayInput);

            // Output mix
            channelData[sample] = inputSample + delayedSample;
        }
    }
}
```

**Multi-Tap Pattern (for grain overlaps):**

```cpp
// Read multiple grains from different positions without advancing read pointer
float grain1 = delayLine.popSample(channel, delay1Samples, false);  // don't update
float grain2 = delayLine.popSample(channel, delay2Samples, false);  // don't update
float grain3 = delayLine.popSample(channel, delay3Samples, true);   // update on last
```

---

### juce::dsp::IIR::Filter

**Filter Coefficient Factory Methods:**

```cpp
// First-order filters (simple, low CPU)
static Ptr makeFirstOrderLowPass(double sampleRate, NumericType frequency);
static Ptr makeFirstOrderHighPass(double sampleRate, NumericType frequency);
static Ptr makeFirstOrderAllPass(double sampleRate, NumericType frequency);

// Second-order filters (standard biquad)
static Ptr makeLowPass(double sampleRate, NumericType frequency);
static Ptr makeLowPass(double sampleRate, NumericType frequency, NumericType Q);
static Ptr makeHighPass(double sampleRate, NumericType frequency);
static Ptr makeHighPass(double sampleRate, NumericType frequency, NumericType Q);
static Ptr makeBandPass(double sampleRate, NumericType frequency, NumericType Q);
static Ptr makeNotch(double sampleRate, NumericType frequency, NumericType Q);
static Ptr makeAllPass(double sampleRate, NumericType frequency, NumericType Q);

// Shelving filters
static Ptr makeLowShelf(double sampleRate, NumericType frequency, NumericType Q, NumericType gainFactor);
static Ptr makeHighShelf(double sampleRate, NumericType frequency, NumericType Q, NumericType gainFactor);

// Parametric EQ
static Ptr makePeakFilter(double sampleRate, NumericType frequency, NumericType Q, NumericType gainFactor);
```

**Usage in Degrain (Feedback Filtering):**

```cpp
// PluginProcessor.h
class DegrainAudioProcessor : public juce::AudioProcessor {
private:
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                   juce::dsp::IIR::Coefficients<float>> feedbackFilter;
    double currentSampleRate = 44100.0;
};

// prepareToPlay()
void DegrainAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    feedbackFilter.prepare(spec);
    updateFeedbackFilter();  // Set initial coefficients
}

void DegrainAudioProcessor::updateFeedbackFilter()
{
    // Get filter parameters
    float hpFreq = hpFreqParam->load();
    float lpFreq = lpFreqParam->load();

    // Create combined HP + LP coefficients
    // (In practice, use two separate filters or ProcessorChain)
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(
        currentSampleRate, hpFreq
    );

    *feedbackFilter.state = *coeffs;  // Update coefficients
}

// processBlock()
void DegrainAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    // Update filter if parameters changed
    if (filterParamsChanged) {
        updateFeedbackFilter();
        filterParamsChanged = false;
    }

    // Process through filter
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    feedbackFilter.process(context);
}
```

**⚠️ Thread Safety:** Updating coefficients on audio thread is generally safe for IIR filters in JUCE, but can cause brief artifacts. For critical applications, use double-buffered coefficient updates.

---

### juce::SmoothedValue

**Two Smoothing Modes:**

```cpp
// Linear smoothing (for most parameters)
juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> dryWetSmoothed;

// Multiplicative smoothing (for time/frequency - exponential curves)
juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> delayTimeSmoothed;
```

**Core Methods:**

```cpp
void reset(double sampleRate, double rampLengthInSeconds);
void setCurrentAndTargetValue(ValueType newValue);  // Jump immediately
void setTargetValue(ValueType newValue);  // Ramp to new value
ValueType getNextValue();  // Get next smoothed sample
ValueType getCurrentValue() const;
ValueType getTargetValue() const;
bool isSmoothing() const;
void skip(int numSamples);  // Skip ahead without computing
```

**Recommended Ramp Times:**

- **Fast parameters (cutoff, resonance):** 10ms
- **Medium parameters (gain, feedback):** 20-50ms
- **Slow parameters (delay time, grain size):** 50-100ms
- **Very slow (reverb size, large buffers):** 100-200ms

**Usage Pattern:**

```cpp
// PluginProcessor.h
class DegrainAudioProcessor : public juce::AudioProcessor {
private:
    // Linear smoothing for dry/wet, feedback
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> dryWetSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> feedbackSmoothed;

    // Multiplicative smoothing for delay time (exponential feels more natural)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> delayTimeSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> grainSizeSmoothed;
};

// prepareToPlay()
void DegrainAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Initialize with sample rate and ramp time
    dryWetSmoothed.reset(sampleRate, 0.02);      // 20ms ramp
    feedbackSmoothed.reset(sampleRate, 0.05);    // 50ms ramp
    delayTimeSmoothed.reset(sampleRate, 0.1);    // 100ms ramp (slow, avoids pitch jumps)
    grainSizeSmoothed.reset(sampleRate, 0.05);   // 50ms ramp

    // Set initial values
    dryWetSmoothed.setCurrentAndTargetValue(50.0f);
    feedbackSmoothed.setCurrentAndTargetValue(0.0f);
    delayTimeSmoothed.setCurrentAndTargetValue(500.0f);
    grainSizeSmoothed.setCurrentAndTargetValue(50.0f);
}

// processBlock()
void DegrainAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    // Update target values from parameters
    delayTimeSmoothed.setTargetValue(delayTimeParam->load());
    feedbackSmoothed.setTargetValue(feedbackParam->load() / 100.0f);  // 0-1 range
    dryWetSmoothed.setTargetValue(dryWetParam->load() / 100.0f);

    // Per-sample processing
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
        float delayMs = delayTimeSmoothed.getNextValue();
        float feedback = feedbackSmoothed.getNextValue();
        float dryWet = dryWetSmoothed.getNextValue();

        // Use smoothed values in DSP...
    }
}
```

---

### juce::dsp::DryWetMixer

**Purpose:** Latency-compensated mixing of dry (unprocessed) and wet (processed) signals.

**Core Methods:**

```cpp
void prepare(const juce::dsp::ProcessSpec& spec);
void reset();
void setWetMixProportion(SampleType newWetMixProportion);  // 0.0 = dry, 1.0 = wet
void setWetLatency(SampleType wetLatencySamples);  // Delay compensation
void pushDrySamples(const AudioBlock<const SampleType>& dryBlock);
void mixWetSamples(AudioBlock<SampleType>& wetBlock);  // Modifies wetBlock in-place
```

**How Latency Compensation Works:**

1. Call `pushDrySamples()` with input before processing
2. Process wet signal (may have latency from FFT, convolution, etc.)
3. Call `mixWetSamples()` with processed output
4. DryWetMixer automatically delays dry signal to match wet latency
5. Output is perfectly phase-aligned dry + wet mix

**Usage Pattern (if Degrain has processing latency):**

```cpp
// PluginProcessor.h
class DegrainAudioProcessor : public juce::AudioProcessor {
private:
    juce::dsp::DryWetMixer<float> dryWetMixer;
    static constexpr int grainProcessingLatency = 128;  // Example latency
};

// prepareToPlay()
void DegrainAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    dryWetMixer.prepare(spec);
    dryWetMixer.setWetLatency(grainProcessingLatency);
    dryWetMixer.reset();
}

// processBlock()
void DegrainAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    juce::dsp::AudioBlock<float> block(buffer);

    // 1. Push dry samples BEFORE processing
    dryWetMixer.pushDrySamples(block);

    // 2. Process wet signal (modifies buffer in-place)
    processGranularDelay(buffer);

    // 3. Mix dry + wet with automatic latency compensation
    float dryWet = dryWetParam->load() / 100.0f;
    dryWetMixer.setWetMixProportion(dryWet);
    dryWetMixer.mixWetSamples(block);  // Modifies block in-place
}
```

**Note:** If Degrain has zero latency (pure delay line processing), DryWetMixer is optional — can manually mix:

```cpp
// Manual dry/wet mix (zero latency)
float dryWet = dryWetParam->load() / 100.0f;
channelData[sample] = (inputSample * (1.0f - dryWet)) + (wetSample * dryWet);
```

---

### juce::dsp::ProcessSpec

**Structure Members:**

```cpp
struct ProcessSpec {
    double sampleRate;           // Sample rate for processing
    juce::uint32 maximumBlockSize;  // Max samples per processBlock call
    juce::uint32 numChannels;       // Number of channels to process
};
```

**Universal Pattern for ALL juce::dsp Components:**

```cpp
void DegrainAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Create ProcessSpec once
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    // Pass to ALL DSP components
    delayLine.prepare(spec);
    feedbackFilter.prepare(spec);
    dryWetMixer.prepare(spec);
    // ... all other juce::dsp components

    // Also reset all components
    delayLine.reset();
    feedbackFilter.reset();
    dryWetMixer.reset();
}
```

---

## 4. Audio Thread Patterns

### processBlock() Implementation

```cpp
void DegrainAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    // 1. Disable denormals (CRITICAL for granular processing - many multiplies)
    juce::ScopedNoDenormals noDenormals;

    // 2. Clear any extra channels
    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // 3. Check for MIDI (freeze trigger)
    processMIDI(midiMessages);

    // 4. Get tempo sync info
    updateTempoSync();

    // 5. Update smoothed parameters
    delayTimeSmoothed.setTargetValue(delayTimeParam->load());
    feedbackSmoothed.setTargetValue(feedbackParam->load() / 100.0f);

    // 6. Sample-by-sample processing (for modulation)
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
        auto* channelData = buffer.getWritePointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
            // Get smoothed parameter values
            float delayMs = delayTimeSmoothed.getNextValue();
            float feedback = feedbackSmoothed.getNextValue();

            // DSP processing...
            float inputSample = channelData[sample];
            float delayedSample = processGrain(channel, inputSample, delayMs);

            channelData[sample] = inputSample + delayedSample;
        }
    }
}
```

### Reading MIDI (Freeze Trigger)

```cpp
void DegrainAudioProcessor::processMIDI(juce::MidiBuffer& midiMessages)
{
    for (const auto metadata : midiMessages) {
        const auto msg = metadata.getMessage();

        if (msg.isNoteOn()) {
            int noteNumber = msg.getNoteNumber();
            int velocity = msg.getVelocity();
            int samplePosition = metadata.samplePosition;

            // Trigger freeze on any MIDI note
            freezeTriggered = true;
            freezeTriggerSample = samplePosition;

            // Or map specific notes to functions
            if (noteNumber == 60) {  // Middle C
                // Trigger specific behavior
            }
        }
        else if (msg.isNoteOff()) {
            int noteNumber = msg.getNoteNumber();
            freezeTriggered = false;
        }
        else if (msg.isController()) {
            int ccNumber = msg.getControllerNumber();
            int ccValue = msg.getControllerValue();

            // Map CC to parameters (alternative to automation)
            if (ccNumber == 1) {  // Mod wheel
                // Modulate parameter
            }
        }
    }
}
```

**MidiBuffer Iteration (JUCE 8 style):**

```cpp
// Modern range-based for loop
for (const auto metadata : midiMessages) {
    const auto msg = metadata.getMessage();
    const int samplePosition = metadata.samplePosition;

    // Process MIDI message...
}

// Or iterator-based
for (auto it = midiMessages.begin(); it != midiMessages.end(); ++it) {
    const auto metadata = *it;
    // ...
}
```

### AudioPlayHead Tempo Sync

```cpp
// PluginProcessor.h
class DegrainAudioProcessor : public juce::AudioProcessor {
private:
    double currentBPM = 120.0;
    int timeSignatureNumerator = 4;
    int timeSignatureDenominator = 4;
    bool isPlaying = false;
};

void DegrainAudioProcessor::updateTempoSync()
{
    // JUCE 7+ pattern: getPosition() returns Optional<PositionInfo>
    if (auto* playHead = getPlayHead()) {
        if (auto positionInfo = playHead->getPosition()) {
            // Get BPM (returns Optional<double>)
            if (auto bpm = positionInfo->getBpm()) {
                currentBPM = *bpm;
            }

            // Get time signature (returns Optional<TimeSignature>)
            if (auto timeSig = positionInfo->getTimeSignature()) {
                timeSignatureNumerator = timeSig->numerator;
                timeSignatureDenominator = timeSig->denominator;
            }

            // Get playback state (returns Optional<bool>)
            if (auto playing = positionInfo->getIsPlaying()) {
                isPlaying = *playing;
            }

            // Other available data:
            // - getTimeInSamples() — Position in samples
            // - getTimeInSeconds() — Position in seconds
            // - getPpqPosition() — Position in quarter notes
            // - getIsRecording() — Recording status
            // - getIsLooping() — Loop status
            // - getPpqLoopStart() / getPpqLoopEnd() — Loop boundaries
        }
    }
}

// Convert tempo-synced note value to milliseconds
float DegrainAudioProcessor::tempoSyncToMs(int noteValue)
{
    // noteValue: 0 = 1/16, 1 = 1/8, 2 = 1/4, 3 = 1/2, 4 = 1 bar
    const float noteDurations[] = { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f };  // in quarter notes
    float quarterNotes = noteDurations[noteValue];

    // BPM = quarter notes per minute
    // Duration (seconds) = (60 / BPM) * quarterNotes
    float durationSeconds = (60.0f / currentBPM) * quarterNotes;
    return durationSeconds * 1000.0f;  // Convert to ms
}
```

### Buffer Management

```cpp
// Read-only access
const float* inputData = buffer.getReadPointer(channel);
float sample = inputData[sampleIndex];

// Write access
float* outputData = buffer.getWritePointer(channel);
outputData[sampleIndex] = newValue;

// Clear channel
buffer.clear(channel, startSample, numSamples);

// Clear entire buffer
buffer.clear();

// Apply gain
buffer.applyGain(channel, startSample, numSamples, gain);

// Add from another buffer
buffer.addFrom(destChannel, destStartSample,
               sourceBuffer, sourceChannel, sourceStartSample, numSamples);

// Copy from another buffer
buffer.copyFrom(destChannel, destStartSample,
                sourceBuffer, sourceChannel, sourceStartSample, numSamples);
```

---

## 5. Build System (CMakeLists.txt)

### Complete WebView Plugin Configuration

```cmake
cmake_minimum_required(VERSION 3.22)
project(Degrain VERSION 1.0.0)

# Add JUCE (assumes JUCE is a subdirectory or CPM dependency)
add_subdirectory(JUCE)

# Create binary data for WebView resources
juce_add_binary_data(Degrain_UIResources
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/css/styles.css
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
        # Add all other HTML/CSS/JS files
)

# Create plugin target
juce_add_plugin(Degrain
    COMPANY_NAME "YourCompany"
    PLUGIN_MANUFACTURER_CODE Manu
    PLUGIN_CODE Frac
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Degrain"

    # ⚠️ CRITICAL: Required for WebView (especially VST3)
    NEEDS_WEB_BROWSER TRUE

    # Plugin metadata
    IS_SYNTH FALSE                    # Effect, not instrument
    NEEDS_MIDI_INPUT TRUE             # For freeze trigger
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE

    # Binary settings
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_CODE Frac

    # Source files
    SOURCES
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
)

# Link JUCE modules
target_link_libraries(Degrain
    PRIVATE
        Degrain_UIResources          # Link binary data
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp                # For DelayLine, IIR, SmoothedValue
        juce::juce_gui_basics
        juce::juce_gui_extra          # For WebBrowserComponent
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ⚠️ CRITICAL: Generate JuceHeader.h (JUCE 8 requirement)
# Must come AFTER target_link_libraries, BEFORE target_compile_definitions
juce_generate_juce_header(Degrain)

# Compile definitions
target_compile_definitions(Degrain
    PUBLIC
        JUCE_WEB_BROWSER=1             # Enable WebView
        JUCE_USE_CURL=0                # Disable CURL (not needed)
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0

        # DSP optimizations
        JUCE_DSP_USE_INTEL_MKL=0
        JUCE_DSP_USE_SHARED_FFTW=0
)

# C++ standard
target_compile_features(Degrain PRIVATE cxx_std_17)
```

### Platform-Specific WebView Notes

**macOS:** Uses WebKit (built-in, no dependencies)
**Windows:** Uses WebView2 (requires Edge runtime, usually pre-installed on Windows 10+)
**Linux:** Uses WebKitGTK (may need system package: `libwebkit2gtk-4.0-dev`)

---

## 6. Thread Safety

### What is SAFE on Audio Thread

✅ **ALLOWED:**
- Reading atomic parameters (`std::atomic<float>*` from `getRawParameterValue()`)
- Calling `getNextValue()` on SmoothedValue
- Processing audio through DSP components (DelayLine, Filter, etc.)
- Reading pre-allocated buffers
- Simple arithmetic, conditionals, loops
- Calling `prepare()` is NOT safe (allocates), only in `prepareToPlay()`

❌ **FORBIDDEN (causes glitches, crashes, priority inversion):**
- Memory allocation (`new`, `malloc`, `std::vector::push_back`, etc.)
- Locking mutexes (blocks, unpredictable timing)
- File I/O (slow, blocking)
- Calling `setMaximumDelayInSamples()` (allocates internally)
- String operations (can allocate)
- Console output (`std::cout`, `DBG()` — allocates)
- Calling `parameters.getParameter()` (allocates, use cached pointers instead)

### Lock-Free Communication Patterns

**Pattern 1: Atomic Parameter Access (APVTS Built-in)**

```cpp
// UI Thread → Audio Thread (APVTS handles this automatically)
// User moves slider → APVTS updates atomic → Audio thread reads

// Audio Thread
std::atomic<float>* delayTimeParam = parameters.getRawParameterValue("DELAY_TIME");
float value = delayTimeParam->load();  // ✅ Lock-free atomic read
```

**Pattern 2: Atomic Flags**

```cpp
// PluginProcessor.h
class DegrainAudioProcessor : public juce::AudioProcessor {
private:
    std::atomic<bool> freezeActive { false };
    std::atomic<bool> filterNeedsUpdate { false };
};

// UI Thread (or MIDI input)
freezeActive.store(true);

// Audio Thread
if (freezeActive.load()) {
    // Freeze grain buffer
}
```

**Pattern 3: Lock-Free FIFO (for events)**

```cpp
// For complex messages (not simple atomics)
#include <juce_audio_basics/juce_audio_basics.h>

juce::AbstractFifo eventQueue { 64 };  // 64 events max
struct Event { int type; float value; };
std::array<Event, 64> eventBuffer;

// UI Thread → Audio Thread
void pushEvent(int type, float value) {
    int start1, size1, start2, size2;
    eventQueue.prepareToWrite(1, start1, size1, start2, size2);

    if (size1 > 0) {
        eventBuffer[start1] = { type, value };
        eventQueue.finishedWrite(1);
    }
}

// Audio Thread
void processEvents() {
    int start1, size1, start2, size2;
    eventQueue.prepareToRead(eventQueue.getNumReady(), start1, size1, start2, size2);

    for (int i = 0; i < size1; ++i) {
        auto& event = eventBuffer[start1 + i];
        // Process event...
    }

    eventQueue.finishedRead(size1);
}
```

### SmoothedValue as Thread Bridge

SmoothedValue is **implicitly thread-safe** for this use case:

```cpp
// UI Thread (or parameter callback)
delayTimeSmoothed.setTargetValue(newValue);  // ✅ Safe

// Audio Thread
float value = delayTimeSmoothed.getNextValue();  // ✅ Safe
```

**Why it's safe:** SmoothedValue stores target internally, `getNextValue()` does pure math (no shared state).

---

## Gotchas & Pitfalls

### 1. WebView Member Declaration Order

**CRITICAL:** Relays → WebView → Attachments. Reverse order = release build crash.

**Why:** C++ destroys members in reverse order of declaration. Attachments reference relays, so relays must outlive attachments. WebView holds weak references to relays, so relays must outlive WebView.

### 2. JUCE 8 WebSliderParameterAttachment Third Parameter

**WRONG:**
```cpp
attachment = std::make_unique<juce::WebSliderParameterAttachment>(
    *param, *relay);  // ❌ JUCE 7 signature, silently fails in JUCE 8
```

**CORRECT:**
```cpp
attachment = std::make_unique<juce::WebSliderParameterAttachment>(
    *param, *relay, nullptr);  // ✅ JUCE 8 requires undoManager (nullptr = no undo)
```

### 3. valueChangedEvent Callback Has NO Parameters

**WRONG:**
```javascript
delayState.valueChangedEvent.addListener((value) => {
    updateKnob(value);  // ❌ value is UNDEFINED!
});
```

**CORRECT:**
```javascript
delayState.valueChangedEvent.addListener(() => {
    const value = delayState.getNormalisedValue();  // ✅ Read from state
    updateKnob(value);
});
```

### 4. ES6 Module Import Required

**WRONG:**
```html
<script src="js/juce/index.js"></script>
<script>
    const state = window.__JUCE__.backend.getSliderState("GAIN");  // ❌ Undefined
</script>
```

**CORRECT:**
```html
<script type="module" src="js/juce/index.js"></script>
<script type="module">
    import { getSliderState } from './js/juce/index.js';
    const state = getSliderState("GAIN");  // ✅ Works
</script>
```

### 5. DelayLine setMaximumDelayInSamples Allocates

**WRONG:**
```cpp
void processBlock(...) {
    delayLine.setMaximumDelayInSamples(newSize);  // ❌ ALLOCATES on audio thread!
}
```

**CORRECT:**
```cpp
void prepareToPlay(double sampleRate, int samplesPerBlock) {
    delayLine.setMaximumDelayInSamples(maxSize);  // ✅ Only in prepareToPlay
}
```

### 6. Knob Drag Must Be Relative, Not Absolute

**WRONG:**
```javascript
let startY = 0;
knob.addEventListener('mousedown', e => startY = e.clientY);
document.addEventListener('mousemove', e => {
    const deltaY = startY - e.clientY;  // ❌ Total distance from start
    rotation = initialRotation + deltaY;  // Knob jumps to cursor
});
```

**CORRECT:**
```javascript
let lastY = 0;
knob.addEventListener('mousedown', e => lastY = e.clientY);
document.addEventListener('mousemove', e => {
    const deltaY = lastY - e.clientY;  // ✅ Distance since last frame
    rotation += deltaY * sensitivity;  // Incremental update
    lastY = e.clientY;  // Update for next frame
});
```

### 7. NEEDS_WEB_BROWSER Required for VST3

**WRONG:**
```cmake
juce_add_plugin(MyPlugin
    FORMATS VST3 AU
    # Missing NEEDS_WEB_BROWSER
)
```

VST3 builds but won't appear in DAW scanner.

**CORRECT:**
```cmake
juce_add_plugin(MyPlugin
    FORMATS VST3 AU
    NEEDS_WEB_BROWSER TRUE  # ✅ Required for VST3 WebView
)
```

### 8. check_native_interop.js Required

**WRONG:**
```cmake
juce_add_binary_data(MyPlugin_UIResources
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/juce/index.js
        # Missing check_native_interop.js
)
```

**CORRECT:**
```cmake
juce_add_binary_data(MyPlugin_UIResources
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js  # ✅ Required
)
```

### 9. NormalisableRange Skew for Time/Frequency

**WRONG:**
```cpp
// Linear range for delay time (poor UX — no precision at low values)
juce::NormalisableRange<float>(10.0f, 5000.0f, 0.1f)
```

**CORRECT:**
```cpp
// Logarithmic range for delay time (better UX — precise at low values)
juce::NormalisableRange<float>(10.0f, 5000.0f, 0.1f, 0.3f)
//                                                      ^^^^ skew < 1.0 = logarithmic
```

### 10. ScopedNoDenormals in processBlock

**WRONG:**
```cpp
void processBlock(...) {
    // No denormal protection → CPU spikes on silence
}
```

**CORRECT:**
```cpp
void processBlock(...) {
    juce::ScopedNoDenormals noDenormals;  // ✅ Prevents denormal slowdown
    // ... processing
}
```

---

## References & Resources

### Official JUCE Documentation

- [JUCE SmoothedValue Reference](https://docs.juce.com/master/classSmoothedValue.html)
- [JUCE DelayLine Tutorial](https://juce.com/tutorials/tutorial_dsp_delay_line/)
- [JUCE dsp::DelayLine API](https://docs.juce.com/master/classdsp_1_1DelayLine.html)
- [JUCE IIR::Coefficients API](https://docs.juce.com/master/structdsp_1_1IIR_1_1Coefficients.html)
- [JUCE AudioPlayHead API](https://docs.juce.com/master/classAudioPlayHead.html)
- [JUCE AudioProcessorValueTreeState API](https://docs.juce.com/master/classAudioProcessorValueTreeState.html)
- [JUCE ProcessSpec API](https://docs.juce.com/master/structdsp_1_1ProcessSpec.html)
- [JUCE DryWetMixer API](https://docs.juce.com/master/classdsp_1_1DryWetMixer.html)
- [JUCE ScopedNoDenormals API](https://docs.juce.com/master/classScopedNoDenormals.html)
- [JUCE MidiBuffer API](https://docs.juce.com/master/classMidiBuffer.html)

### Community Resources

- [JUCE Forum: SmoothedValue Best Practices](https://forum.juce.com/t/how-do-i-smooth-changes-to-an-audioparameterfloats-value/34334)
- [JUCE Forum: Circular Buffer Patterns](https://forum.juce.com/t/whats-the-simplest-way-to-make-a-circular-buffer-using-juce/33194)
- [KVR Audio: Parameter Smoothing Discussion](https://www.kvraudio.com/forum/viewtopic.php?t=439148)

### GitHub Examples

- [Pamplejuce JUCE Template](https://github.com/sudara/pamplejuce) — Modern JUCE CMake template
- [CHOC JavaScript Integration](https://github.com/Tracktion/choc) — WebView utilities and JS bindings

### Internal Project References

- `/Users/flextasy/plugin-freedom-system/troubleshooting/patterns/juce8-critical-patterns.md` — 22 critical patterns
- `/Users/flextasy/plugin-freedom-system/plugins/TapeAge/Source/PluginEditor.h` — Working WebView example
- `/Users/flextasy/plugin-freedom-system/plugins/TapeAge/Source/ui/public/js/juce/index.js` — JUCE JS module

---

## Version-Specific Notes

### JUCE 7 vs JUCE 8 Differences

| Feature | JUCE 7 | JUCE 8 |
|---------|--------|--------|
| WebSliderParameterAttachment | 2 params (param, relay) | 3 params (param, relay, undoManager) |
| AudioPlayHead API | getCurrentPosition() | getPosition() (returns Optional) |
| JuceHeader.h | Pre-built | Must call juce_generate_juce_header() |
| ES6 modules | Partial support | Full support (type="module" required) |
| WebBrowserComponent | Basic | Enhanced with relay system |

### Platform Differences

| Platform | WebView Backend | Installation Notes |
|----------|----------------|-------------------|
| macOS | WebKit (built-in) | No dependencies |
| Windows | WebView2 (Edge) | Usually pre-installed (Windows 10+) |
| Linux | WebKitGTK | May need: `libwebkit2gtk-4.0-dev` |

---

**Last Updated:** 2026-02-15
**Next Steps:** Use this research to implement Degrain plugin following the architecture plan.
