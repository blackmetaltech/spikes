---
title: Modes
section: Playing
order: 10
summary: Guided, Standard and Researcher: what each changes, what each never changes, and how to switch.
---

# Modes

A mode is a difficulty setting. There are three: **Guided**, **Standard** and **Researcher**. The default is Standard.

A mode changes how much help you get and how forgiving the score is. It never changes the circuit you have to build. The same mechanism is checked in every mode.

## The three modes

| | Guided | Standard | Researcher |
|---|---|---|---|
| Recipe | Printed in the brief, free | Behind **Show recipe**. Using it caps the level at two stars. | None |
| Hints | Free, all three tiers, always | Open after two missed runs | None. The Hint button is hidden. |
| Seeds needed to pass | 1 | 1 (5 for the second star) | 5. A pass needs Test x5. |
| Noise in the stimulus | Low or none | Normal | Physiological (about twice Standard in the micro-puzzles) |
| Pass bands (the ranges the score accepts) | Wide | As written in the brief | Tight |
| Reference button | Open from the start | After you pass the level | After you pass the level |

In the micro-puzzles the engine scales the pass bands by 1.5 (Guided), 1.0 (Standard) and 0.7 (Researcher) for the first group of ten concepts (threshold to neuromodulation), and by 1.35, 1.0 and 0.8 for the later micro-puzzles (recruitment to divergent command). It sets the stimulus noise to 0, 1 and 2 times its Standard value. The bosses scale their bands by 1.25, 1.0 and 0.85. Each task adds its own changes on top. The reflex, for example, makes the poke 1.67 times stronger in Guided and allows one false withdrawal. In Researcher it makes the poke weaker (200 Hz above the chatter instead of 250 Hz), allows no false withdrawal and asks for a shorter latency.

> **Note:** The scale factors above are for the micro-puzzles and the bosses, which share code. Other levels tune their own bands per mode. The brief shows the numbers in force.

## Choosing a mode

- **In the campaign:** the header has a mode stepper. Change it before you press Build.
- **In the lab:** More, then the **Level** group, then the mode stepper. The lab reloads the level when you switch.
- **In Settings:** the Game tab, the **Mode** row.

The mode is part of your save. You can switch at any time. There is no penalty. The only trace is the label on a star: the campaign card shows the best result in each mode.

## What a mode does to your stars

Your best star count for a level is the best over all modes. Unlocking never depends on the mode. A level you passed in Guided still opens the next one.

The campaign card shows a per-mode row, for example "Guided 1 star, Standard 2 stars". The end card says which stars you earned in which mode.

## Which mode to pick

| If you... | Try |
|---|---|
| Are new to neurons, or want to see how a mechanism works before you build it | Guided. Open the recipe, build it, then read the brief again to see why it works. |
| Like puzzles and want help only when stuck | Standard. |
| Want the cleanest test of your own understanding, or you are using Spike for research or teaching | Researcher. |
| Are stuck on one level in Standard | Switch that level to Guided, pass it, switch back. |

## Practical effects

- In Researcher mode the **Test x5** button is always on the bar. A pass in this mode is the five-seed test. A single live run that scores well is not a pass.
- In Guided mode the **Reference** button on the campaign card and in More opens the level's reference circuit at once. It is a solution, so it earns no stars. See [the core loop](core-loop.html).
- The hint ladder is described on [stars, par and hints](stars-and-hints.html).

## Related pages

- [Stars, par and hints](stars-and-hints.html)
- [The core loop](core-loop.html)
- [Settings and performance](settings-and-performance.html)
