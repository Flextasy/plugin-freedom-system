# Degrain v3 UI Integration Checklist

**Plugin:** Degrain — Granular Delay
**Window:** 900×480 (not resizable)
**UI Framework:** JUCE 8 WebView
**Generated:** 2026-02-14

---

## Prerequisites

- [x] JUCE 8+ installed
- [x] CMake 3.22+ installed
- [x] C++17 or later compiler
- [ ] Platform-specific WebView dependencies installed (see below)

### Platform Dependencies

**macOS:**
No additional dependencies (WebKit is built-in)

**Windows:**
Microsoft Edge WebView2 Runtime (bundled with Windows 11+)
For Windows 10, download from: https://developer.microsoft.com/en-us/microsoft-edge/webview2/

**Linux:**
WebKit2GTK library required:
```bash
sudo apt install webkit2gtk-4.0  # Debian/Ubuntu
sudo dnf install webkit2gtk4      # Fedora
```

---

## Integration Steps

### 1. File Setup

- [ ] **Copy HTML file**
  - Source: `.ideas/mockups/v3-ui.html`
  - Destination: `Source/ui/public/index.html`
  - Create directories if needed: `mkdir -p Source/ui/public`

- [ ] **Copy C++ header template**
  - Source: `.ideas/mockups/v3-PluginEditor-TEMPLATE.h`
  - Destination: `Source/PluginEditor.h`
  - Update processor class name if different from `DegrainProcessor`

- [ ] **Copy C++ implementation template**
  - Source: `.ideas/mockups/v3-PluginEditor-TEMPLATE.cpp`
  - Destination: `Source/PluginEditor.cpp`
  - Update includes to match your project structure

### 2. CMakeLists.txt Configuration

- [ ] **Add BinaryData**
  - Copy snippet from `.ideas/mockups/v3-CMakeLists-SNIPPET.txt`
  - Add `juce_add_binary_data()` block to embed `index.html`

- [ ] **Link GUI Extra module**
  - Add `juce::juce_gui_extra` to `target_link_libraries()`

- [ ] **Enable WebView backend**
  - Add platform-specific `JUCE_WEB_BROWSER=1` definitions
  - macOS: WebKit (auto)
  - Windows: WebView2 (auto)
  - Linux: WebKit2GTK (manual package install required)

### 3. Processor Setup (PluginProcessor.h/cpp)

- [ ] **Add APVTS accessor**
  ```cpp
  juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
  ```

- [ ] **Verify all 19 parameters exist in APVTS**
  - delay_time (Float)
  - delay_sync_toggle (Bool)
  - delay_sync_division (Choice, 18 items)
  - grain_size (Float)
  - grain_rate (Float)
  - grain_rate_sync_toggle (Bool)
  - grain_rate_sync_division (Choice, 18 items)
  - grain_shape (Choice, 4 items: sine/triangle/square/ramp)
  - feedback (Float)
  - diffusion (Float)
  - stereo_spread (Float)
  - detune (Float)
  - low_cut (Float)
  - high_cut (Float)
  - reverse_mix (Float)
  - grain_mutation (Float)
  - freq_spread (Float)
  - freeze (Bool)
  - dry_wet (Float)

- [ ] **Match parameter IDs exactly** (case-sensitive)
  - HTML `data-param` attributes MUST match APVTS parameter IDs
  - Example: `data-param="delay_time"` → `apvts.getParameter("delay_time")`

### 4. Build & Test

- [ ] **Configure CMake**
  ```bash
  cmake -B build -DCMAKE_BUILD_TYPE=Release
  ```

- [ ] **Build plugin**
  ```bash
  cmake --build build --config Release
  ```

- [ ] **Verify build artifacts**
  - VST3: `build/Degrain_artefacts/Release/VST3/Degrain.vst3`
  - AU (macOS): `build/Degrain_artefacts/Release/AU/Degrain.component`
  - Standalone: `build/Degrain_artefacts/Release/Standalone/Degrain`

- [ ] **Test in DAW**
  - Load plugin in host
  - Verify UI loads (900×480 window)
  - Test all 19 controls update parameters
  - Test parameter automation (C++ → JS updates)
  - Test preset recall

### 5. Validation

- [ ] **Visual checks**
  - Window size is exactly 900×480
  - All 5 sections visible (Delay, Grain, Character, Degrain, Mix)
  - Section colors match spec (Delay: #e85d3a, Grain: #3aaa5c, etc.)
  - Knobs render with correct arc (270° sweep, dead zone at bottom)
  - Freeze button glows green when active

- [ ] **Interaction checks**
  - Knob dragging (vertical drag, shift for fine control)
  - Knob double-click reset to default
  - Filter sliders (horizontal drag)
  - Delay sync toggle shows/hides combo box
  - Grain Rate sync toggle shows/hides combo box
  - Shape selector (4 buttons, one active)
  - Freeze button toggles on/off

- [ ] **Parameter binding checks**
  - UI changes update DSP (JS → C++)
  - DAW automation updates UI (C++ → JS)
  - Preset recall updates UI
  - All 19 parameters respond correctly

### 6. Final Cleanup

- [ ] **Remove template comments**
  - Clean up `// Generated: 2026-02-14` comments if desired

- [ ] **Update plugin version**
  - Set version to 3.0.0 in CMakeLists.txt and PluginProcessor.cpp

- [ ] **Add copyright/license headers**
  - Update file headers to match your project license

- [ ] **Test on target platforms**
  - macOS: 10.15+ (Catalina)
  - Windows: 10/11 (with WebView2 Runtime)
  - Linux: Ubuntu 20.04+ / Fedora 34+ (with webkit2gtk)

---

## Troubleshooting

### WebView fails to load (blank window)

- **Check BinaryData**: Verify `index.html` is embedded correctly
  ```cpp
  // Should see in getResource():
  BinaryData::index_html
  BinaryData::index_htmlSize
  ```
- **Check URL**: Verify base URL matches `juce::URL("https://degrain.local")`
- **Check console**: Enable JUCE debug logging to see WebView errors

### Parameters don't update

- **Check parameter IDs**: HTML `data-param` must match APVTS exactly
- **Check attachments**: Verify all 19 `ParameterAttachment` objects are created
- **Check relay order**: Relays MUST be declared before WebView in header

### UI looks wrong

- **Check HTML file**: Verify `v3-ui.html` was copied correctly (not v3-ui-test.html)
- **Check window size**: Verify `setSize(900, 480)` in constructor
- **Check fonts**: Verify fallback stack is present (Inter not loaded externally)

### Linux: WebView crashes

- **Install webkit2gtk**: `sudo apt install webkit2gtk-4.0`
- **Check version**: `pkg-config --modversion webkit2gtk-4.0` (need 2.26+)
- **Check linking**: Verify `WEBKIT2_LIBRARIES` in CMake output

---

## Next Steps

After successful integration:

1. [ ] Implement DSP (see `creative-brief.md` and `parameter-spec.md`)
2. [ ] Add preset system
3. [ ] Add MIDI learn (for Freeze button)
4. [ ] Add tooltips (optional)
5. [ ] Optimize WebView performance (disable animations if needed)
6. [ ] Test on all target platforms
7. [ ] Package for distribution

---

## Resources

- **JUCE WebView Docs**: https://docs.juce.com/master/classWebBrowserComponent.html
- **Parameter Spec**: `../parameter-spec.md`
- **Creative Brief**: `../creative-brief.md`
- **UI Mockup**: `v3-ui.html` (production HTML)
- **C++ Templates**: `v3-PluginEditor-TEMPLATE.h/cpp`

---

**Generated by:** Plugin Freedom System
**Date:** 2026-02-14
**Plugin Version:** 3.0.0
