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
