# MUTAFORM — design concept (v1.1, "retro handheld")

The UI is styled as a **retro handheld gadget**: a chunky plastic shell, a pixel-art screen
with a living mascot, and three big physical buttons. This follows the genre the user asked
for (handheld virtual-pet look); every asset — palette, scene, mascot, icons, logo, texts —
is original to this project and drawn in code.

## Palette (own colour grading)
| Token | Hex | Use |
|---|---|---|
| shell | `#E07A55` (hi `#F39A72`, lo `#B9573A`, deep `#8F3F2A`) | terracotta plastic body |
| bezel / ink | `#2A1C2C` | screen frame, outlines, text |
| paper | `#F3EAD9` / `#E7DCC7` / line `#CDBFA6` | panels on the screen |
| plum | `#6B4C9A` | layer tiles, waveforms |
| pink | `#FF6F91` | level bar, multipoints |
| amber | `#F7B548` | active toggles |
| mint | `#4FD1A5` | ON states, lock |
| buttons | FX lilac `#B79CE6`, EVOLVE magenta `#E8467C`, MIX sky `#7FC6E8` |
| sky bands | `#24173F` → `#F79A7A` (11 dithered bands) | twilight desert scene |

## Typography
Embedded OFL pixel fonts (offline): **Pixelify Sans** (logo, buttons) and **Silkscreen**
(all labels). Licences in `ui/fonts/OFL-*.txt`.

## Layout (600 × 800 design units, uniformly scaled, 3:4 aspect)
```
┌──────────────── MUTAFORM (embossed logo) ────────── speaker ┐
│ ┌─ bezel ───────────────────────────────────────────────┐  │
│ │ [CREATURE | ONE-SHOTS]                                │  │
│ │        ◀ ▣ PRESET NAME ▶                              │  │
│ │ [folder]                                  (planet)    │  │
│ │ [disk]           speech bubble                        │  │
│ │ [dice][undo]                                          │  │
│ │              ~ pixel mascot on desert ~               │  │
│ │ [LAYER][CHAOS]                     [GLIDE][VIBRATO]   │  │
│ │ [◀] [lock] LEVEL 42 - SCAMP [star] [▶]                │  │
│ │      ▬▬▬▬▬▬▬ progress to next stage                   │  │
│ └───────────────────────────────────────────────────────┘  │
│          (FX)        ( EVOLVE )        (MIX)               │
│              ▫▫▫▫▫ audition keys ▫▫▫▫▫                    │
└─────────────────────────────────────────────────────────────┘
```
- **Scene**: 168 × 181 pixel canvas scaled ×3 (`image-rendering: pixelated`), scanline
  overlay. Twilight sky, ringed planet, mesas, dunes, cacti, drifting spores, twinkling stars.
- **Mascot**: procedural pixel art. Stage by level: SPORE (0) → SPROUT (1) → CUB (10) →
  SCAMP (30) → YOUTH (60) → ADULT (120) → ELDER (300) → ANCIENT (1000). Shape, colour, eyes,
  spikes, antenna, horns, ears, pattern come from the current variation seeds and from sound
  analysis (bright → warm hues, dark → cool hues, plucky → spikes, moving → wobbly).
  It bounces and opens its mouth while notes play, shakes while mutating, "poofs" on change.
- **ONE-SHOTS** page: paper card with GENERATED / LIBRARY tabs. Toolbar (LOOP, MULTIPOINT,
  REVERSE, GAIN ◀▶, OCT ◀▶, export) acts on the selected layer. 5 rows: layer tile,
  waveform with draggable start (green) / end (red) / loop (amber) / multipoint (pink)
  markers, and a 2 × 5 icon grid (loop, multipoint, reverse, back-to-original, library;
  mute, solo, octave, lock, export).
- **LIBRARY**: categories RANDOM / ALL / FAVORITES / DARK / BRIGHT / PLUCKY / SMOOTH /
  MOVING / STEADY (auto-tags from sound analysis) and the list of variations with names
  and favourite stars.
- **FX** and **MIX** buttons open paper overlays on the screen (pixel knobs, faders).

## Components
Pixel knob = 16 × 16 SVG pixel grid, pointer snapped to 15° steps, value tooltip while dragging.
Buttons are bevelled paper keys (3 px ink border, inner light/dark edges, 3 px drop).
Icons are 12 × 12 pixel maps rendered to SVG data URIs (`ICONS` in `app.js`).
