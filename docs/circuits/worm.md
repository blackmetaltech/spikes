# Reference circuit: worm chemotaxis (`circuits/worm_chemotaxis.json`)

Task `worm` (engine/src/tasks/worm.cpp): a worm crawling at 0.3 mm/s on a 10 mm-radius plate with a Gaussian salt hill
(sigma 2.5 mm), starting 5.5 mm away and facing away from the peak, must come within 1 mm of the peak within 45 s (the episode runs 120 s so the score keeps counting).
The plate is "stirred": a common chatter (up to +600 Hz, bursts about 1 s, seeded) is added to BOTH ASEL and ASER, so only
ASER minus ASEL carries information.

## Biology
ASEL and ASER, the two gustatory neurons of C. elegans, are asymmetric: ASEL is an ON cell (activated when salt
concentration rises) and ASER an OFF cell (activated when it falls) (Suzuki et al. 2008). Both are differentiators
(Thiele, Faumont & Lockery 2009). Salt chemotaxis combines two strategies: klinotaxis, steering during the head swing
(Iino & Yoshida 2009), and pirouettes, i.e. sharp reorientations triggered when things get worse
(Pierce-Shimomura, Morse & Lockery 1999). The reference only ever turns one way (TL is unused), so it implements
klinokinesis / pirouettes, not klinotaxis (no head-swing steering). Downstream, AIY promotes smooth runs and AIB (with RIA/SMB) promotes turning
(Gray, Hill & Bargmann 2005; Kunitomo et al. 2013).

## Wiring (24 neurons; 4 per class)
```
ASEL(ON)  --800--> AIY --(-1500)--> AIB --1000--> turn_right (TR)        TL = turn_left, no inputs
ASER(OFF) -------1000--------------> AIB (+ tonic 450 pA)
```
| Population | Model | Notes |
|---|---|---|
| ASEL, ASER | `iaf_psc_alpha` | the world drives each with a Poisson train (5 Hz + up to 400 Hz); the 1000 pA input weight makes each input spike produce one output spike, so the cell's rate follows d ln c/dt |
| AIY | `iaf_psc_alpha` | run-promoting interneuron, driven by the ON cell |
| AIB | `iaf_psc_alpha`, `I_e` = 450 pA | turn-promoting interneuron with tonic drive (just above the 375 pA rheobase); inhibited by AIY, excited by ASER |
| TR / TL | `iaf_psc_alpha` | motor pools; angular velocity = 1.5 rad/s x clamp((rate_TL - rate_TR) / 40 Hz, -1, 1) (see the task header) |

Inhibition is current based (no reversal potential), so strongly inhibited AIB can be driven well below -70 mV; that is a model simplification.
(C. elegans neurons are mostly graded and non-spiking; spiking `iaf_psc_alpha` cells are a game-level abstraction.)

## Why it works
* The tonic drive on AIB makes the default behaviour "keep turning" (a local search, pirouette-like).
* When salt is rising, ASEL fires, AIY fires, and AIY's strong inhibition (-1500 pA) silences AIB: the worm runs
  straight. As soon as the signal stops improving (worm points away or passes the peak), ASEL falls silent, AIB returns
  and the worm turns until it runs up the gradient again; ASER (OFF) adds a fast kick when concentration actually falls.
* Heading noise in the world (0.15 rad per sqrt(s)) stops the worm locking onto a closed orbit.
* Parameters: the four weights were set by an input-output survey (rate of every cell for ASEL/ASER = 5, 105, 405 Hz)
  and then closed-loop sweeps on 8 seeds; the reference reaches the 1 mm disc after 27-36 s on 8 seeds (limit 45 s). All populations pin V_m = -70 mV.
* **Why more than one neuron.** Turn-only-on-ASER with one cell per side takes 42-110 s and is thrown off by the chatter; best
  pool sizes still miss 45 s on some seeds. Subtracting ASEL from ASER (ASER excites turn_left, ASEL inhibits it, 2+ cells per
  pool, +-1000 pA) reaches the peak in 35-41 s on 8 of 8 seeds with 2 cells per class, 31-36 s with 4.

## What breaks it
| Lesion | Result |
|---|---|
| ASEL -> AIY x0 (no run signal): AIB turns forever | worm circles at the start, mean distance 5.2-5.6 mm, fail |
| AIY -> AIB x0 (no inhibition of turning) | same, fail |
| AIB -> TR x0 (cannot turn) | worm walks away, 10 mm, fail |
| ASER -> AIB x0 (OFF pathway removed) | slower (41-51 s vs 27-36 s), fails the 45 s bar on some seeds: the OFF cell is an accelerator |
| ASER -> AIB x0 and ASEL -> AIY x0 | no sensing, circles, fail |

## v0.3: concepts, modes, variants (item D)

* **Concepts:** none new; excitation, inhibition, subtraction used (`m_inhibition` and `m_subtraction` teach inhibition and subtraction first). Level kind `task`, on the spine.
* **Modes** (guided / standard / researcher): Common chatter x0.6 guided, x1.25 researcher; pass time 56 / 45 / 40.5 s (reference 27-35 s). The structure of the pass test never changes; the reference circuit passes every mode (researcher on seeds 1-5; `tasks_modes_test`).
* **Variants** (optional branches, never weaken the base bar): half_neurons (par 14); noise_x2 (chatter up to 1200 Hz).
* **Hints:** three tiers from `score().parts` (concept, then the place: a channel or population id, then a partial recipe).
* **Quality metric:** `mean_dist_mm`. **Notebook:** Suzuki et al. 2008 *Nature* 454:114 (the earlier entry cited it as *Neuron* 57:1, which is wrong; `m_subtraction` always had it right); Thiele, Faumont & Lockery 2009 *J Neurosci* 29:11904.
* **Default part:** `iaf_psc_alpha`. **Recipe** (`recipe()`, kept out of the brief): 8 cells in every group; ASER onto A and ASEL onto B at a sensor weight of about 1000 pA; A onto turn_left at +1000, B inhibitory onto turn_left at -1000; turn_right wired to an idle group.
