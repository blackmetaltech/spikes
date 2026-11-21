---
title: The core loop
section: Start here
order: 4
summary: Build, run, check, read the verdict, improve, earn stars. How a level is scored and what the game tells you after each attempt.
---

# The core loop

Every level in Spike is the same cycle.

1. **Read** the brief: what to build and what counts as a pass.
2. **Build** a circuit on the bench.
3. **Run** it and watch the scopes and the body.
4. **Check** it for a score and a verdict.
5. **Improve** the circuit using the verdict (and a hint if you need one).
6. **Pass**, then **test** it on five seeds, then trim it for the third star.
7. **Dissect** it in the lesion lab and read the notebook entry.

## 1. Read the brief

The brief is in the inspector when nothing is selected. It has three parts.

| Part | Content |
|---|---|
| TO BUILD | The sensors you have, the motors you must drive, and the concept the level teaches. |
| TO PASS | The pass rule in plain words, with the number of seeds. |
| RECIPE | Guided mode prints it. Standard mode hides it behind **Show recipe**. Researcher mode has none. |

The brief never gives the answer except in the recipe. The recipe holds the numbers of the reference circuit.

## 2. Build

You add neuron groups, choose their model, wire sensors to neurons and neurons to motors, and set weights and delays. See [how to read the lab](how-to-read-the-lab.html) for the controls. Your circuit is saved automatically per level.

Two things worth knowing:

- Every level checks the **mechanism** it teaches, not only the numbers. A circuit that happens to hit the numbers by a cheap trick fails the mechanism check. The verdict then names the missing part.
- A circuit is judged on several random worlds, so "works on one seed" and "works" are different things.

## 3. Run

**Run** simulates the circuit live, 1 ms at a time. The world panel shows the body. The raster, traces and gauges show what the cells do. The run lasts one episode (the level's duration: a few seconds for a micro-puzzle, tens of seconds for a task; the campaign card shows it as "N s episode"). At the end of the episode you get the end card with the score.

Use More to change the speed (0.25x to 50x), and **Skip** to jump to the next scripted event.

## 4. Check

**Check** scores the circuit headless, in the background, on the level's own seed, using the level's quick episode. It runs a separate copy of the engine, so it does not touch your live run. It takes from under a second (the worm, the lamprey) to about five seconds (the two bosses). A card appears with the result: "Check: passed" or "Check: not passed", the score, and the verdict sentence.

| | Run | Check | Test x5 |
|---|---|---|---|
| Where | Live, on the bench | Headless, in the background | Live progress card |
| Seeds | The circuit's seed | The circuit's seed | Five: 101, 102, 103, 104, 105 |
| Episode | The full episode | The quick episode | The full episode |
| Records a pass | Yes | Yes (a passing Check counts as a pass) | Yes, with the five-seed result |
| Counts toward the second star | No | No | Yes |

A seed picks the random parts of the world (when the touches arrive, how noisy the input is). In Test x5 the circuit is reseeded as well as the world, and any weight spread you set is redrawn for every seed.

## 5. Read the verdict

The verdict is one sentence about what the circuit did. It is written to name the **mechanism** that is missing, not the number to type. For example, it may say that no cell needs more than one input at a time, or that the motor pool fires on noise. Verdicts and the first three hint tiers never carry numbers. The numbers live only in the recipe.

After a failed run:

- The **score parts** (chips on the score card) show what the score is made of: hits, misses, false triggers, a latency, a rate.
- The part of the circuit the verdict is about **glows red** on the bench. It is the same element a tier-2 hint would outline.
- The **timeline** shows where a trace left its band.

The **score** runs from 0 to 100. It is a quality number. A **pass** is a separate yes or no. Reaching a good score is not enough: each level states a pass rule, and the mechanism check has to hold.

## 6. Improve, with hints

Hints exist for when you are stuck. They come in three tiers. See [stars, par and hints](stars-and-hints.html).

- Tier 1 names the idea.
- Tier 2 points at the place and outlines it on the bench.
- Tier 3 gives a partial recipe.

Hint use is recorded and never punished. It does not change your stars. Opening the **recipe** in Standard mode does: it caps the level at two stars.

## 7. Stars

| Stars | You need |
|---|---|
| 1 | Pass the level. |
| 2 | **Check** passes all five worlds (Standard or Researcher). |
| 3 | The same, and the level's **challenge**, written on its card (by default the neuron par; a few levels add an energy ceiling). |

Guided earns at most two stars; a Researcher pass also earns a seal. The end card says what blocked a third star: for example "20 neurons, par is 16". See [stars, par and hints](stars-and-hints.html).

## 8. After the pass

- **First pass.** A FIRST PASS stamp runs, a trail lights the critical path through your circuit (sensor to motor), and a toast says a notebook entry has unlocked. See [the notebook](notebook.html).
- **Results.** A card compares your circuit with generated variants on neurons, energy and quality. The variants are random edits of the reference circuit, kept if they pass. They are not other players.
- **Lesion lab.** Cut parts and re-run on several seeds. See [the lesion lab](lesion-lab-and-analysis.html).
- **Analysis.** Motifs, bursts, phase response, energy, reverse correlation. From the second chapter on.
- **Reference.** Load the level's reference circuit to compare. It earns no stars and the end card says so.
- **Variants.** Challenge variants (half the neurons, double noise, and so on) are optional. They give no stars and never unlock anything.

## The reference circuit and stars

A circuit that equals the level's reference file, or that the **Reference** button filled onto the bench, earns no stars and unlocks nothing. A circuit you built by hand with round numbers is never flagged. If you edit the reference a little, it still counts as a copy only when every number is within a relative 0.0001 per cent of it. In practice: press **Empty bench** and build your own.

## What unlocks what

| Thing | Opens when |
|---|---|
| A level in a chapter | You pass the one before it in that chapter. |
| A chapter | You pass the previous chapter's main path, or the one two before it. The first two chapters are open from the start. |
| Branches (diagnosis cases, variants) | You pass the level they hang off. They never gate anything. |
| The Bestiary and the Theory wing | You pass the Invertebrates spine. Inside the Bestiary a chapter may also wait for something it builds on: Sea slug for Heart's Neuromodulation level, Crab for the Leech chapter, Fly mushroom body for the Sea slug. Guided mode drops these in-wing waits. |
| Inhibitory neurons, noise inputs, the parts bin | The first level whose reference needs them. |
| Analysis, Load and Save | The second chapter, Spinal cord. |
| Lesions, Reference | After you pass the level (Reference also opens in Guided mode). |

## Related pages

- [Modes](modes.html)
- [Stars, par and hints](stars-and-hints.html)
- [Tips and common mistakes](tips.html)
- [Diagnosis cases](diagnosis-cases.html)
