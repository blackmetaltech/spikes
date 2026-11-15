# Reference circuit: worm chemotaxis (`circuits/worm_chemotaxis.json`)

Task `worm` (engine/src/tasks/worm.cpp): a worm crawling at 0.3 mm/s on a 10 mm-radius plate with a Gaussian salt hill
(sigma 2.5 mm), starting 5.5 mm away and facing away from the peak, must come within 1 mm of the peak inside 120 s.

## Biology
ASEL and ASER, the two gustatory neurons of C. elegans, are asymmetric: ASEL is an ON cell (activated when salt
concentration rises) and ASER an OFF cell (activated when it falls) (Suzuki et al. 2008). Both are differentiators
(Thiele, Faumont & Lockery 2009). Salt chemotaxis combines two strategies: klinotaxis, steering during the head swing
(Iino & Yoshida 2009), and pirouettes, i.e. sharp reorientations triggered when things get worse
(Pierce-Shimomura, Morse & Lockery 1999). Downstream, AIY promotes smooth runs and AIB (with RIA/SMB) promotes turning
(Gray, Hill & Bargmann 2005; Kunitomo et al. 2013).

## Wiring (12 neurons; 2 per class)
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

(C. elegans neurons are mostly graded and non-spiking; spiking `iaf_psc_alpha` cells are a game-level abstraction.)

## Why it works
* The tonic drive on AIB makes the default behaviour "keep turning" (a local search, pirouette-like).
* When salt is rising, ASEL fires, AIY fires, and AIY's strong inhibition (-1500 pA) silences AIB: the worm runs
  straight. As soon as the signal stops improving (worm points away or passes the peak), ASEL falls silent, AIB returns
  and the worm turns until it runs up the gradient again; ASER (OFF) adds a fast kick when concentration actually falls.
* Heading noise in the world (0.15 rad per sqrt(s)) stops the worm locking onto a closed orbit.
* Parameters: the four weights were set by an input-output survey (rate of every cell for ASEL/ASER = 5, 105, 405 Hz)
  and then closed-loop sweeps on 8 seeds; the reference reaches the 1 mm disc after 29-35 s (limit 120 s).

## What breaks it
| Lesion | Result |
|---|---|
| ASEL -> AIY x0 (no run signal): AIB turns forever | worm circles at the start, mean distance 5.2-5.6 mm, fail |
| AIY -> AIB x0 (no inhibition of turning) | same, fail |
| AIB -> TR x0 (cannot turn) | worm walks away, 10 mm, fail |
| ASER -> AIB x0 (OFF pathway removed) | still passes, but slower (41-43 s vs 30-35 s): the OFF cell is an accelerator |
