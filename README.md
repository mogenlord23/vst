# MUTAFORM

**MUTAFORM** is a pop-art melody machine for Windows (VST3 + Standalone, plus a VST3 effect
edition). Drag any sound into the hands of the mascot **POPSY** — he catches it, stretches
it like bubble gum, and sings it back to you as **melodies that are always in key**.
No AI: everything is algorithmic and reproducible.

<p><img src="docs/screenshot.png" width="320" alt="MUTAFORM user interface"></p>

*(Rendered from the Linux dev build; Windows uses the same embedded web UI.)*

## Install (Windows 10/11, x64)

1. Download the latest `Mutaform-win64` artifact from GitHub Actions
   (*Actions → Build → latest green run → Artifacts*). It contains:
   ```
   Mutaform.vst3/       ← instrument (put it on an instrument / MIDI track)
   Mutaform FX.vst3/    ← effect edition (put it on an audio track, press FEED)
   Mutaform.exe         ← standalone app
   ```
2. Copy the `.vst3` folders to `C:\Program Files\Common Files\VST3\` and rescan in your DAW.

The UI uses the Microsoft **WebView2 Runtime** (preinstalled on Windows 10 21H2+ / 11).

## How it works

1. **Feed POPSY.** Drag a WAV / AIFF / FLAC / MP3 onto the plugin — he raises his arms,
   catches it and holds it as a pink ribbon (or use the LOAD tile). In **Mutaform FX** press
   **FEED**, play audio on the track, press **STOP**: the captured audio becomes the sound.
2. Press **MELODY**. A melody is written for the sound and plays in sync with your DAW
   (or with its own clock when the transport is stopped). POPSY stretches the ribbon with the
   groove and lifts it with the pitch, and sings along.
3. Shape it with **KEY**, **SCALE** (major, minor, dorian, pentatonics, harmonic minor,
   lydian, phrygian), **MOOD** (chill → wild) and **DENSITY**. **NEW TUNE** writes a new
   melody, the **RANDOM** tile rolls a new sound *and* a new melody.
4. **EVOLVE** mutates the sound itself (8 offline processes), **UNDO** steps back.
   **GENOFORM** grows a synth patch that imitates your sound and adds it as a new variation.
5. Play it yourself too: MIDI keyboard, or computer keys `A W S E D F T G Y H U J K…`
   (`Z`/`X` octave, `Space` = MELODY on/off).

## Technologies

- **STRETCHTONE** — YIN pitch detection on every sound and variation; each layer is
  auto-tuned so its fundamental plays exactly on C3, which keeps every melody in key.
  Unpitched material (noise, claps, speech) is *tonalized* through tuned comb resonators.
  Hermite resampling, glide, vibrato.
- **MELODY** — a rule-based composer: chord progressions per mode, motif rhythms with a
  strong-beat bias, chord tones on strong beats, stepwise contour with bounded leaps,
  motif repetition over the changing chords, cadence on the tonic. 1/2/4/8 bars, seedable.
- **GENOFORM** — resynthesis: a 20-gene synth voice (morphing oscillator, FM, sub, noise,
  resonant filter with envelope, pitch envelope, ADSR, drive) evolved by a genetic algorithm
  (tournament selection, crossover, annealed mutation, elitism) until its spectral envelope
  (32 bands × 8 slices) and loudness contour match your sound. Runs locally, in the
  background.
- **EVOLVE** — seeded chains of 2–5 processes: pitch shift, grain shuffle, envelope filter,
  saturation, time-stretch, ring mod, reverse reverb swell, transient shaping.

## Features

- 5 layers (LAYERS tile): load, loop, reverse, mute, solo, octave, lock, GENOFORM, export
  WAV, clear, variation history chips.
- FX: filter LP/HP/BP, drive, chorus, tempo-synced delay, reverb, EVOLVE process pool, CHAOS.
- MIX: layer faders + pan, melody bars/octave/glide/vibrato, auto-tune, tonalize, amp ADSR,
  velocity, master, limiter (FX edition: DRY level).
- Presets in `%APPDATA%\Mutaform\Presets` (5 factory presets synthesised in code), RANDOM.
- **Your own character**: CHARACTER tile → choose a folder with transparent PNG layers of the
  same size: `base.png` (required), `eyes_closed.png`, `mouth_open.png`, `mouth_wide.png`,
  `arm_left.png`, `arm_right.png`. They are copied to `%APPDATA%\Mutaform\Character` and
  animated like POPSY (arms pivot at 28 % / 72 % width, 76 % height).
- Full recall of samples, variations, melody seed and parameters in the DAW project.
- Resizable 9:16 window (338 × 600 … 900 × 1600).

## Build locally

Windows 10/11 x64, Visual Studio 2022 (Desktop C++), CMake ≥ 3.22, Git, internet on first
configure (JUCE 8, Catch2 and the WebView2 SDK are fetched by CMake).

```bat
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
build\Release\MutaformTests.exe
```
Outputs: `build\Mutaform_artefacts\Release\VST3\Mutaform.vst3`,
`build\MutaformFX_artefacts\Release\VST3\Mutaform FX.vst3`,
`build\Mutaform_artefacts\Release\Standalone\Mutaform.exe`.
With only VS 2026 installed: `-G "Visual Studio 18 2026" -A x64 -T v143` (what CI uses).

CI (`.github/workflows/build.yml`, `windows-latest`) builds everything, runs the Catch2 tests
and pluginval (strictness 10) on both plugins, and uploads the `Mutaform-win64` artifact.

## Known limitations

- Windows x64 only (VST3/Standalone); not code-signed.
- "Always beautiful" is a musical goal, not a guarantee: melodies are always in key and
  phrase-shaped, but taste varies — press NEW TUNE until you like it. Very noisy or very long
  sources work best with TONALIZE on.
- GENOFORM imitates timbre and envelope, not exact sounds; it takes a few seconds and one job
  runs at a time.
- FEED captures up to 12 s and needs the host to deliver audio to the effect's input.
- Samples ≤ 30 s, EVOLVE output ≤ 20 s; all variations stay in RAM (≤ 32 per layer).
- The ribbon/arm animation of custom character packs assumes the documented pivot points.

## Credits

Code, UI, mascot POPSY, texts and factory sounds are original to this project.
Fonts: Luckiest Guy (Apache 2.0) and Bangers (SIL OFL 1.1), see `ui/fonts/`.
Built with [JUCE 8](https://juce.com) and [Catch2](https://github.com/catchorg/Catch2).
