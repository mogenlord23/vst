# MUTAFORM — development plan

Each stage ends with: commit → push → GitHub Actions green (build + tests + pluginval).

| # | Stage | Content |
|---|-------|---------|
| 0 | Docs | CLAUDE.md, PLAN.md, DECISIONS.md, DESIGN.md |
| 1 | Skeleton | CMake + JUCE + empty instrument + WebView UI with one button + CI (VST3, Standalone, tests, pluginval, `Mutaform-win64.zip`) |
| 2 | Sampler, 1 layer | sample loading, 16 voices, ADSR, cubic resampling, velocity, root C3 |
| 3 | 5 layers | start/end, loop + crossfade, reverse, multipoint, mute, gain, pan, pitch |
| 4 | EVOLVE + CHAOS + undo | background renderer, 8 processes, seed, 32-step history |
| 5 | FX + MIX | filter, distortion, chorus, delay (tempo sync), reverb, master, limiter |
| 6 | Full UI | tabs EVOLVE/CHAOS/LAYERS/FX/MIX, drop zone, waveform with markers, scaling |
| 7 | Presets | JSON + audio in %APPDATA%/Mutaform/Presets, 5 synthesized factory presets, randomizer |
| 8 | Polish | README, docs, final verification |

## Status
- [x] 0 Docs
- [x] 1 Skeleton
- [x] 2 Sampler
- [x] 3 Layers
- [x] 4 Evolve
- [x] 5 FX/Mix
- [x] 6 UI
- [x] 7 Presets
- [ ] 8 Polish
