---
title: Modes
section: Playing
order: 10
summary: Guided, Standard and Researcher: what each changes, what each never changes, and how to switch.
---

# Modes

A mode is a difficulty setting: **Guided**, **Standard** or **Researcher**. The default is Standard.

A mode changes how much help you get and how forgiving the score is. It never changes the circuit you have to build. Every mode checks the same mechanism.

## The three modes

| | Guided | Standard | Researcher |
|---|---|---|---|
| Recipe | Written out for you in the lab, free | Behind **Show recipe**. Using it caps the level at two stars. | None |
| Hints | Free, all three tiers, always | Open after two missed runs | None. The Hint button is hidden. |
| Worlds needed to pass | 1 (Check runs five worlds, and Guided stops at two stars) | 1 (all five for the second star) | 5. A pass needs all five, and earns the Researcher seal. |
| Noise in the stimulus | Low or none | Normal | Physiological (about twice Standard in the micro-puzzles) |
| Pass bands (the ranges the score accepts) | Wide | As written in the brief | Tight |
| Reference button | Open from the start | After you pass the level | After you pass the level |

In the micro-puzzles the engine scales the pass bands by 1.5 (Guided), 1.0 (Standard) and 0.7 (Researcher) for eleven (Threshold, Excitation, Convergence, Inhibition, Subtraction, Adaptation, Tonic drive, Half-centre, Pacemaking, Coupling and Neuromodulation), and by 1.35, 1.0 and 0.8 for the other nine (Recruitment, Reciprocal inhibition, Recurrent inhibition, Negative feedback, Timed bursts, Feed-forward and feedback, Rhythm and gating, Travelling wave and Divergent command). It sets stimulus noise to 0, 1 and 2 times its Standard value. The bosses scale their bands by 1.25, 1.0 and 0.85. Each task adds its own changes. The reflex makes the poke 1.67 times stronger in Guided and allows one false withdrawal. In Researcher the poke is weaker (200 Hz above the chatter instead of 250 Hz), no false withdrawal is allowed and the latency must be shorter.

> **Note:** These scale factors cover the micro-puzzles and the bosses, which share code. Other levels tune their own bands per mode. The brief shows the numbers in force.

## Choosing a mode

- **First time:** at the end of the interactive tutorial (Plate 0) a screen asks "How much help do you want?". Pick one. If you skip the tutorial and never chose, the same screen appears over the map on your first Play.
- **In the campaign:** the header has a mode stepper. Change it before you press Build.
- **In the lab:** More, then the **Level** group, then the mode stepper. The lab reloads the level when you switch.
- **In Settings:** the Game tab, the **Mode** row.

The mode is part of your save. Switch at any time, with no penalty. The only trace is the label on a star: the campaign card shows the best result in each mode.

## What a mode does to your stars

Your best star count for a level is the best over all modes. Unlocking never depends on the mode: a level passed in Guided still opens the next.

The level card on the campaign map shows a per-mode row, for example "Guided: 2 of 2 stars   Standard: 1 of 3 stars   Researcher: not yet", with ", sealed" added where you hold the Researcher seal. The three star conditions above it are worded for your selected mode.

## Which mode to pick

| If you... | Try |
|---|---|
| Are new to neurons, or want to see how a mechanism works before you build it | Guided. Open the recipe, build it, then read the brief again to see why it works. |
| Like puzzles and want help only when stuck | Standard. |
| Want the cleanest test of your own understanding, or you are using Spike for research or teaching | Researcher. |
| Are stuck on one level in Standard | Switch that level to Guided, pass it, switch back. |

## Practical effects

- In Researcher mode the **Test x5** button is always on the bar (in the other modes it appears once you have passed the level). A pass needs all five worlds, from **Test x5** or from a **Check** that passes all five. One good live run is not a pass: the game says "Passed once" and asks you to press Test x5.
- In Guided mode the **Reference** button on the campaign card and in More opens the level's reference circuit at once. It is a solution, so it earns no stars and unlocks nothing. See [the core loop](core-loop.html).
- Guided also drops the waits inside the Bestiary (a chapter that waits for a level or another chapter) but keeps the gate on the Invertebrates spine; see [the body chapters](campaign-body.html).
- The hint ladder is described on [stars, par and hints](stars-and-hints.html).

## Related pages

- [Stars, par and hints](stars-and-hints.html)
- [The core loop](core-loop.html)
- [Settings and performance](settings-and-performance.html)
