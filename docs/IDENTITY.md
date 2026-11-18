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
