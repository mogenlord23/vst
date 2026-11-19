# MUTAFORM — design concept (v2.0, "pop-art stage", 9:16)

The UI was laid out in Figma first (file "MUTAFORM — character & world concepts", frame
"MUTAFORM UI — 9:16", built by hand from vector shapes) and then implemented in `ui/`.

## World
A pop-art stage: horizontal black/hot-pink stripes, a yellow comic burst with orange
halftone dots, a row of coloured tiles on top. In the centre stands the mascot **POPSY**,
an original pop-art character (not modelled on any real person): tall yellow swirl quiff,
pink skin with halftone shading, cyan eyeshadow, glossy red lips, star beauty mark, black
jacket with lime collar, blue sleeves. Users can replace POPSY with their own PNG layers
(CHARACTER tile).

## Story / interaction
1. Drag a sound over the plugin → POPSY raises both arms ("GIMME GIMME!").
2. Drop it → he catches it; the sound becomes a pink **bubble-gum ribbon** (its waveform)
   held between his hands.
3. MELODY plays → he stretches the ribbon: width follows the groove, height follows the
   pitch of each note; he sings (mouth opens per note), bobs his head, blinks.
4. EVOLVE / GENOFORM → he shakes while working; a dot meter shows GENOFORM growth.

## Palette
hot pink `#FF1FA8`, lime `#B8F400`, electric blue `#1E7BFF`, orange `#FF8A00`,
yellow `#FFE600`, cyan `#2BB8FF`, red `#E8102A`, ink `#141014`, white, cream `#FFF6E6`.

## Typography
Luckiest Guy (logo, stickers, buttons; Apache 2.0) and Bangers (labels; OFL), embedded.

## Layout (450 × 800 design units, scaled uniformly, fixed 9:16)
```
 0   tiles toolbar: LOAD · LAYERS · PRESETS · SAVE · RANDOM · CHARACTER
 74  black band: MUTAFORM logo · preset pill ◀ NAME ▶
136  stage (stripes + burst): stickers EVOLVE / GENOFORM / (FEED in FX edition) left,
     NEW TUNE / UNDO right, speech bubble, POPSY + ribbon, GENOFORM meter,
     status strip (key, scale, layer, detected pitch, level)
588  knobs: KEY · SCALE · MOOD · DENSITY (glossy pop dials)
696  arcade buttons: FX (yellow) · MELODY (pink, large, toggles playback) · MIX (blue)
```
Overlays (white cards, 4 px ink borders, hard drop shadows): LAYERS (5 rows with waveform,
LOAD/LOOP/REV/M/S/OCT±/LOCK/GENO/WAV/X and history chips), FX, MIX (faders, pan, melody
options, amp envelope, output), PRESETS, SAVE, CHARACTER.
