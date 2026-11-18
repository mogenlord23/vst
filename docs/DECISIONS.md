# Decisions log

Choices made autonomously during development. Newest at the bottom.

1. **Name: MUTAFORM.** The brief left the name as a placeholder. "Mutaform" = *mutation* +
   *form*: one sound mutating into endless forms. Short, pronounceable, not tied to any
   existing product's branding. Binary/target name `Mutaform`, artifact `Mutaform-win64.zip`,
   manufacturer code `Mtfm`, plugin code `Mtf1`, company "Mutaform Audio".
2. **JUCE version: 8.0.17-security** — the newest JUCE 8 tag at the time of writing
   (JUCE 9 exists, but the brief requires JUCE 8).
3. **Catch2 v3.16.1** via FetchContent.
4. **WebView2 SDK** (`Microsoft.Web.WebView2` 1.0.3485.44 NuGet package) is downloaded by
   CMake FetchContent on Windows and handed to JUCE via `JUCE_WEBVIEW2_PACKAGE_LOCATION`,
   so a local build needs no manual NuGet step.
5. **Root note C3 = MIDI note 60** (the "C3 = 60" convention used by most DAWs such as
   Ableton/FL/Cubase).
6. **No `juce::Synthesiser`**: it takes a `CriticalSection` in `renderNextBlock`. A custom
   lock-free voice manager is used instead.
7. **Interpolation: 4-point cubic Hermite.** Good quality/CPU trade-off for realtime pitch
   shifting of 5 layers × 16 voices; offline EVOLVE pitch shift uses the same kernel.
8. **Sample hand-off to the audio thread**: a heap-allocated `shared_ptr` *holder* is
   published through an atomic pointer; the audio thread swaps it in at block start and pushes
   the old holder to a lock-free retire FIFO; the message thread deletes retired holders.
   The audio thread never touches reference counts or the allocator.
9. **Drag & drop**: WebView2 swallows OS drops, so the page reads the dropped file with
   `FileReader`, sends it as base64 to the native function `loadSampleData`, and C++ decodes
   it from memory (WAV/AIFF/FLAC/MP3 via JUCE formats). The JUCE `FileDragAndDropTarget`
   path is kept as a fallback, and a "Load…" button opens a native file chooser.
10. **Max sample length 30 s** (one-shot instrument). Longer files are truncated.
    EVOLVE output is capped at 20 s.
11. **CI generator fallback.** `windows-latest` no longer ships Visual Studio 2022. The
    workflow uses the "Visual Studio 17 2022" generator when VS 2022 is installed, otherwise
    the "Visual Studio 18 2026" generator with the VS 2022 **v143** toolset (`-T v143`) if that
    component is present. Locally, the documented command remains `-G "Visual Studio 17 2022"`.
12. **EVOLVE chain**: number of processes = 2 + ⌊3·chaos + U(0,1)⌋ clamped to 2..5 (chaos 0 → 2,
    chaos 100 % → 5); per-step strength = 0.08 + chaos·U(0.55, 1). Processes are drawn without
    repetition from the enabled set (CHAOS tab toggles). Output is DC-removed, normalised to
    −1 dBFS, trailing silence trimmed, 5 ms fade-out, max 20 s.
13. **PRNG**: xoshiro256** seeded through SplitMix64 (own code, so results never depend on the
    standard library implementation). Seeds are 64-bit, shown as hex in the UI.
14. **History**: up to 32 entries per layer (entry 0 = original, never dropped). When the cap
    is reached the oldest variation is removed and the next one becomes a *root* (keeps its own
    audio). All entries keep their audio in memory during a session.
15. **State size**: the plugin state stores audio only for the original, root entries and the
    current variation (raw float32 + gzip, lossless). Other history steps are stored as
    recipes (seed, chaos, process mask) and re-rendered deterministically in the background
    when the user navigates to them.
16. **Limiter**: own instant-attack peak limiter (80 ms release) + hard clip at the ceiling,
    instead of `juce::dsp::Limiter`, whose fixed −10 dB first stage and make-up gain colour the
    sound.
17. **FX bypass/mix** are smoothed (30 ms) by blending a wet buffer, so toggling never clicks.
    The delay is a stereo line with mild cross-feed and damped feedback; tempo sync reads the
    host BPM from the play head (120 BPM fallback).
18. **Relays are generated** from the APVTS parameter list (bool → toggle relay, choice →
    combo relay, everything else → slider relay), named by parameter ID, so the UI and the
    processor never drift apart.
19. **UI scaling**: the page is laid out at 900×600 and scaled with a CSS transform; the
    editor is resizable with a fixed 3:2 aspect (600×400 … 1800×1200).
20. **On-screen + computer keyboard** (A…L row = C3…, Z/X octave, Space = EVOLVE) feed notes
    through a lock-free FIFO into the engine, so the Standalone is playable without MIDI gear.
21. **Factory presets** are generated at load time by code in `FactoryPresets.cpp` (FM bell,
    filtered noise, pitch-enveloped kick, sub, formant vowel pads, Karplus-Strong pluck,
    inharmonic plate) — not shipped as files. User presets are folders
    `%APPDATA%\Mutaform\Presets\<name>\` with `preset.json` + `layerN.wav` (32-bit float).
22. **Randomizer** touches layer level/pan/pitch/region/loop/reverse, ADSR, CHAOS and all FX,
    but never the samples, mute/solo, master level or limiter.
23. Stages were committed in pairs (2+3, 4+5, 6+7) where code was shared between them; each
    pushed commit was verified green in CI before the next push.
24. **v1.1 UI redesign (user request)**: switched to a retro-handheld / virtual-pet style
    (pixel screen, mascot, physical round buttons) as requested, with an original palette
    (terracotta shell, twilight desert), original procedural mascot, scene, icons, names and
    texts. No third-party images or logos are used.
25. **Levels**: level = Σ over layers of Σ over current history chain of (1 + round(9·chaos)).
    Group EVOLVE results are recorded as *generations*; ◀ ▶ on the level bar walk them.
26. **LAYER box** chooses what EVOLVE mutates: ALL (unlocked, loaded layers) or one layer.
    **Lock** excludes layers from group EVOLVE and from the dice.
27. **Dice** = randomise parameters + 1–3 chained mutations on every unlocked layer;
    one-step undo restores parameters and history positions.
28. **GLIDE** (portamento from the last played note, 0–2 s) and **VIBRATO** (0–0.6 st,
    5.6 Hz, 150 ms delayed onset) added to the sampler.
29. **Auto-tags** DARK/BRIGHT (energy-weighted zero-crossing rate), PLUCKY/SMOOTH (early peak
    vs. body level), MOVING/STEADY (frame-to-frame level/brightness flux); variation names are
    deterministic two-word names derived from the seed.
30. Editor is now 600 × 800 (3:4), resizable 450 × 600 … 1200 × 1600.
