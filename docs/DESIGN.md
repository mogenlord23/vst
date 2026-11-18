# MUTAFORM — design concept

Original visual language: a dark "lab bench" where a sound is a living specimen. No eggs,
creatures or mascots; the brand mark is an abstract **hexagonal cell with an inner offset
ring** (a mutating nucleus), drawn as inline SVG.

## Palette
| Token | Hex | Use |
|---|---|---|
| `--bg` | `#0D0F13` | window background |
| `--panel` | `#151922` | module panels |
| `--raised` | `#1C2130` | controls, cards |
| `--line` | `#283041` | hairlines, grid |
| `--text` | `#E7EAF0` | primary text |
| `--muted` | `#8A93A6` | labels, units |
| `--accent` | `#38E0C0` | primary accent (cyan-mint): active state, waveform, knobs |
| `--mutate` | `#FF5A7A` | EVOLVE energy (coral): evolve button, chaos meter high end |
| `--marker` | `#FFB84D` | loop region / markers (amber) |
| `--violet` | `#8F7CFF` | multipoint markers, secondary data |
Gradients only on the EVOLVE button (coral → amber) and the CHAOS ring (mint → coral).

## Typography
- UI: `"Segoe UI Variable Text", "Segoe UI", system-ui, sans-serif` (always present on Windows;
  no web fonts are downloaded — the plugin works offline).
- Numbers / seeds: `"Cascadia Mono", Consolas, ui-monospace, monospace`, tabular.
- Sizes: 11px labels (uppercase, +0.08em tracking), 13px body, 15px section titles,
  22px EVOLVE label.

## Grid (design size 900 × 600, 8 px base unit)
```
┌──────────────────────────────── header 48 ────────────────────────────────┐
│ logo │ preset ◀ name ▶ save │ dice (random) │ undo redo │ (spacer) │ scale │
├────────── 168 ─────────┬──────────────── 516 ───────────────┬──── 200 ─────┤
│ LAYER RAIL             │ STAGE: drop zone / waveform        │ EVOLVE COLUMN│
│ 5 slot cards           │ markers: start/end (mint),         │ big EVOLVE   │
│ (name, mini wave,      │ loop (amber), multipoint (violet)  │ CHAOS ring   │
│  mute, level)          │ history strip below                │ seed, ◀ ▶    │
├────────────────────────┴───────── module panel 232 ─────────┴──────────────┤
│ tabs: EVOLVE · CHAOS · LAYERS · FX · MIX                                    │
└─────────────────────────────────────────────────────────────────────────────┘
```
- Gutters 8 px, panel padding 12 px, radius 10 px (panels) / 6 px (controls).
- The whole page is laid out at 900×600 and uniformly scaled with CSS `transform` to the
  editor size (editor keeps a fixed 3:2 aspect, 600×400 … 1800×1200).

## Components
- **Knob**: 44 px SVG arc (270°), mint value arc on a `--line` track, value text below;
  drag vertically (shift = fine), double-click = default, wheel = step.
- **Toggle pill**: 22 px tall, mint when on.
- **Segmented choice**: for filter type, multipoint mode, delay sync.
- **EVOLVE button**: 168 × 72, coral→amber gradient, subtle pulsing ring while rendering.
- **CHAOS ring**: large 120 px knob whose arc colour interpolates mint → coral.
- **Waveform**: min/max peaks (mirrored), mint; region outside start/end dimmed; loop region
  amber tint with crossfade ramp shown; draggable handles with labels S / E / L / L.

## Motion
150 ms ease-out on hover/active; EVOLVE press = short scale-down + ring pulse; new
variation fades the waveform in over 200 ms. No continuous animation when idle.

## Copy
Short, technical, friendly: "Drop a one-shot here", "Evolving…", "Variation 7 · seed 3FA2C1".
