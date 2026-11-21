# Spike visual identity: "Cajal Plate"

Spike looks like a 19th-century anatomical plate: India ink and Golgi stain on aged paper. The reference mockups are `docs/identity/cajal-title.png` and `docs/identity/cajal-lab.png`. This replaces the dark navy bench the UI shell inherited from gigawatt.

## Palette
The tokens live in `godot/ui/kit.gd`.

| Role | Colour | Hex |
|---|---|---|
| Page background | paper | #EFE6D4 |
| Card | | #F8F2E6 |
| Raised card | | #FFFBF2 |
| Rules | | #CDBFA3 |
| Graph grid | | #D3DCE0 |
| Text | ink | #231C17 |
| Soft text | soft ink | #5E5246 |
| Excitatory | Golgi sienna | #B8461B |
| Inhibitory | Prussian blue | #1E4F8F |
| Sensor | verdigris | #12808F |
| Motor | madder plum | #6E2A4E |
| Highlight | gamboge | #C9971C |
| Success | viridian | #2E7D4F |
| Warning | ochre | #B7791F |
| Error | vermilion | #A3261C |

- **Colour-blind safety.** Excitatory and inhibitory stay distinct under simulated protanopia, deuteranopia and tritanopia (Machado 2009; CIEDE2000 ΔE 44–58). E/I is also coded a second way: an arrowhead for excitatory, a bar (⊣) for inhibitory. Sensors and motors are told apart from neurons by shape: labelled index-card ports versus round somata.
- **Contrast.** Sienna on paper is only about 4.5:1, so use it for strokes and fills, never for small text.

## Type
All fonts are OFL and vendored in `godot/assets/fonts`.

| Use | Font |
|---|---|
| Display | Fraunces (variable), including italic for "Fig." captions |
| Body and UI | Atkinson Hyperlegible |
| Readouts | IBM Plex Mono |

## Shapes
- Cards have an ink outline and a hard 5 px offset shadow, like letterpress.
- The primary button is ink-filled with a sienna offset. Secondary buttons are outlined.
- Ports are index-card tabs.
- Menus are numbered legends (I.–V.) with leader dots.
- Every panel is a numbered "Fig. N" with an italic caption.

## Motion
- A firing neuron spreads a watercolour wash of stain, not a glow.
- Traces draw like a pen.
- A score lands like a rubber stamp.
- Respect the Reduced motion setting.

## Screens
- **Title:** the large "Spike." wordmark with a sienna full stop, beside procedurally inked Cajal cells: a pyramidal cell in black ink, an excitatory cell in sienna, a basket interneuron in blue. A plate caption runs along the bottom.
- **Campaign:** each chapter is a hand-inked atlas page with specimen drawings and lettered call-outs.
- **Lab:**
  - The circuit sits on blue engineering graph paper.
  - Neurons are stained somata with fine inked dendrites.
  - Scopes are chart-paper strips.
  - The score is a stamp or needle gauge.

## Plates of the Bestiary and Theory wings
The body chapters draw their atlas pages live in `godot/ui/anatomy.gd`. The ten wing chapters use pre-rendered plates instead: `godot/assets/plates/<chapter id>.png`, one per chapter (worm, leech, aplysia, crab, fly_compass, fly_mb, dynamics, rates, memory, tuning).
- **Made by** `tools/art/plates.py` (pycairo, Pillow, numpy; deterministic). Rerun it after editing a drawing and commit the PNGs. The theory curves are computed from the models they show: FitzHugh-Nagumo for Dynamics, Wilson-Cowan for Rate models, von Mises tuning for Tuning and decoding.
- **Format:** 2000 × 1250 RGBA, the aspect of the map canvas. The paper is transparent, so the map draws its own sheet and engraved frame underneath. Stretch the plate over the whole canvas rect. A pin at canvas fraction (fx, fy) then sits at (fx × 2000, fy × 1250) on the plate.
- **Style:** the same as the procedural plates. Line weights and lettering are given in logical px at 1080p and scaled by 2000 / 1236. Labels are Fraunces italic (opsz 24, wght 420) on translucent paper slips. Specimens are sepia or slate washes with ink rims, hatching and stipple. Stain colours keep their roles: sienna for excitation, Prussian blue for inhibition, verdigris for sensory, madder plum for motor, gamboge for modulators.
- **Composition:** each Bestiary plate has the specimen, its species name, a scale bar, and one or two "Fig. N." insets (a circuit, a section or a trace). Each Theory plate is a set of numbered figures. All of them leave the three planned pin spots (0.2, 0.62), (0.5, 0.36) and (0.8, 0.62) readable.
- **Pins and keep-out areas:** `plates.json` lists, for each plate, the pin positions it was composed around (the three spine levels, plus a quiet spot for each diagnosis case where `BRANCH` in plates.py gives one) and keep-out rects as fractions of the plate `[x, y, w, h, weight]`. The rects cover the anatomy, the inset figures (`FIGS`) and every lettered call-out (weight 120). The map (`_sc_plate_image` in `godot/ui/screens/tasks.gd`) draws the plate at its own aspect inside the frame, takes the pins from it, and feeds the rects to its label-plate placement like `keep_rect`: the anatomy and figures at half weight, the lettering in full (and not at all on a phone, where it is too small to read).
- **Ink density:** `deepen()` in `tools/art/ink.py` pushes pale tints away from the paper and raises faint alpha before the lettering, so the plates hold up beside the procedural body plates at map size.
