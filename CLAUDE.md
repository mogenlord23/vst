# MUTAFORM — notes for Claude / contributors

MUTAFORM is a VST3 + Standalone **instrument** for Windows: drop a one-shot sample, play it
over MIDI across up to 5 layers, and press **EVOLVE** to render endless algorithmic
variations (no AI, deterministic per seed).

## Stack (fixed — do not change)
- C++20, JUCE 8 (pinned tag in `CMakeLists.txt`, fetched with CMake `FetchContent`).
- CMake ≥ 3.22, generator "Visual Studio 17 2022", x64 (CI). Linux/Ninja also works for dev & tests.
- Formats: VST3 + Standalone instrument (`Mutaform`), plus VST3 effect edition (`MutaformFX`, `MUTAFORM_FX=1`).
- UI: plain HTML/CSS/vanilla JS (no bundler) in `ui/`, rendered in `juce::WebBrowserComponent`
  (WebView2 on Windows, statically linked loader: `JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1`).
  UI files are embedded with `juce_add_binary_data` and served via the ResourceProvider.
  Parameters are bound with `WebSliderRelay` / `WebToggleButtonRelay` / `WebComboBoxRelay`;
  everything else goes through native functions + events.
- Parameters: `AudioProcessorValueTreeState`. Full state (samples, history recipes) in `getStateInformation`.
- Tests: Catch2 v3 (`tests/`). Validation: pluginval, strictness 10.
- CI: GitHub Actions `windows-latest` (`.github/workflows/build.yml`) → artifact `Mutaform-win64.zip`.

## Layout
```
CMakeLists.txt            top-level build (JUCE, Catch2, WebView2 SDK via FetchContent)
src/core/                 DSP + model, no GUI dependencies (unit-tested)
  SampleData.*            immutable stereo sample buffer + loading/encoding helpers
  SampleSlot.h            lock-free publish of sample buffers to the audio thread
  Interp.h                cubic (Hermite) interpolation
  SamplerEngine.*         16-voice, 5-layer sampler (realtime)
  Evolver.*               offline EVOLVE renderer (8 processes, seeded PRNG)
  FxChain.*               filter / distortion / chorus / delay / reverb + limiter
  Params.*                parameter IDs and layout
  SoundFeatures.*         sound descriptors (auto-tags), YIN pitch, tonalize, variation names
  Melody.*                rule-based melody writer (MELODY)
  Sequencer.*, LockFreeBox.h  lock-free, host-synced melody sequencer
  Genoform.*              GENOFORM resynthesis: 20-gene synth + genetic algorithm
  Presets.*, FactoryPresets.* preset JSON I/O and synthesized factory sounds
src/plugin/               AudioProcessor, editor (WebView), native-function bridge
ui/                       index.html, style.css, app.js, character.js, fonts (embedded)
tests/                    Catch2 tests
docs/                     PLAN.md, DECISIONS.md, DESIGN.md
```

## Realtime rules (strict)
Inside `processBlock` and everything it calls:
- no heap allocation / deallocation, no `std::vector::push_back`, no `String` building;
- no locks (`CriticalSection`, `std::mutex`), no waiting, no file I/O, no logging, no UI calls;
- data from other threads arrives via lock-free FIFOs or `std::atomic` pointer exchange of
  **pre-built** buffers (`SampleSlot`); retired buffers are pushed to a lock-free retire queue
  and freed on the message thread (timer), never on the audio thread;
- EVOLVE, file decoding, preset I/O and waveform analysis run on background / message threads.
Do not use `juce::Synthesiser` (it locks internally) — the sampler engine is custom.

## Build
```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```
Linux dev (needs libwebkit2gtk-4.1-dev, libgtk-3-dev, libasound2-dev, libcurl4-openssl-dev):
```bash
cmake -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux && ctest --test-dir build-linux --output-on-failure
```
Optional: `-DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE` to reuse a local JUCE checkout.

## Conventions
- Every stage: commit, push, wait for CI green (see docs/PLAN.md).
- Record non-obvious choices in docs/DECISIONS.md.
- All sounds, names, texts, graphics are original. Never copy other products' assets.
