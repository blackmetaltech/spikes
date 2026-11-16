# Eye: pupil and retina

`spike::body::Eye` (`body/eye.h`).

## Pupil
Two antagonist iris muscles with first-order smooth-muscle kinetics behind a 40 ms excitation-contraction delay:
sphincter (parasympathetic, Edinger-Westphal) `tau_up/down = 130/500 ms`; dilator (sympathetic) `800/300 ms`. Diameter
`D = D_min + (D_max - D_min) * clamp(0.5 + 0.5 (a_dilator - a_sphincter))`, range 2-8 mm (Watson & Yellott 2012), so both muscles silent give a
mid-range (denervated) pupil. Inputs `sphincter`, `dilator` are 0..1 pool drives (rate / reference rate). Time constants are illustrative, chosen to
reproduce the human light reflex (Ellis 1981; Loewenfeld 1993; McDougal & Gamlin 2015): constriction faster than redilation.
Plant test: constriction starts 43 ms after a sphincter step, 63 % at 238 ms; redilation 63 % at 670 ms.

## Retina
Retinal illuminance in trolands `E = L (cd/m^2) * pupil area (mm^2)` (the standard definition). Afferent rate is Naka-Rushton (Naka & Rushton 1966) on `x = log10(1 + E)`:
`5 + 95 x^2/(x^2 + 2.3^2)` Hz after a 30 ms retinal latency. Halving the pupil area lowers the rate: this is the negative feedback the reflex circuit closes.

## Light-reflex test
The test wires a 150 ms delay (pretectum -> EW), `sphincter = clamp((rate - 20)/60)` and reciprocal dilator inhibition. A dim -> 200 cd/m^2 step gives 7.4 -> 3.1 mm,
latency 228 ms (human 200-250 ms), 90 % constriction in 0.5 s, 63 % redilation in 0.84 s; static diameter falls monotonically with luminance.
In the game the arc (and thus the latency, gain and the sympathetic side) is the player's circuit.

`readouts()`: `pupil_mm, sphincter_act, dilator_act, retinal_td, retinal_hz`; `view()`: diameter, fraction, retinal rate, log10 luminance.

## Limitations
No hippus, no consensual reflex, no near response, no accommodation; the dilator is not inhibited by light inside the module (the circuit does that).
