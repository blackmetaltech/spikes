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
6. **Pass**, then pass on five worlds, then meet the level's challenge for the third star.
7. **Dissect** it in the lesion lab and read the notebook entry.

## 1. Read the brief

The brief is in the inspector when nothing is selected (press **Brief** on the bar to bring it back). It is a short paragraph of plain words, and two parts of it are kept in view without opening the rest.

| Part | Content |
|---|---|
| TO BUILD | The sentences that say what to make or connect. |
| TO PASS | The pass rule in plain words, with the number of seeds. |
| More... | Opens the full brief. |
| RECIPE | Guided mode prints it. Standard mode hides it behind **Show recipe**, which costs the third star. Researcher mode has none. |

The brief never gives the answer except in the recipe. The recipe holds the numbers of the reference circuit.

## 2. Build

You add neuron groups, choose their model, wire sensors to neurons and neurons to motors, and set weights and delays. See [how to read the lab](how-to-read-the-lab.html) for the controls. Your circuit is saved automatically per level, and every change can be undone (Ctrl+Z). A bench that a button replaces (Empty bench, Reference, Load, a solution slot, a paste) is kept in a short history, so you can bring it back with **Restore previous bench** in More.

Two things worth knowing:

- Every level checks the **mechanism** it teaches, not only the numbers. A circuit that happens to hit the numbers by a cheap trick fails the mechanism check. The verdict then names the missing part.
- A circuit is judged on several random worlds, so "works on one seed" and "works" are different things.

### What the scorer refuses

The scorer is built so that a score means the circuit does the job:

- **Only the level's own sensors drive it.** A circuit that brings a stimulus of its own (an input channel the level does not drive, or a recorded spike train) never passes and its score is capped at 49. The verdict says "The circuit brings a stimulus of its own ... delete that input and wire the sensors the brief names."
- **A token wire is no wire.** A connection or a sensor weight below a small floor, far under any useful weight, does not count as wiring. A sensor or motor that is not connected to any neuron is named in the verdict ("sensor 'touch' is not connected to any neuron").
- **Fakes of the mechanism fail it.** Levels check the wiring that produces the behaviour (for example that a cell really needs several inputs, or that a rhythm is made by the circuit and not by the stimulus), and a table of cheap circuits that must fail guards each level.
- **100 means passed.** A run that has not passed never reads 100, however close it is.
- **A reference copy earns nothing** (see below).

## 3. Run

**Run** simulates the circuit live, 1 ms at a time. The world panel shows the body. The raster, traces and gauges show what the cells do. The run lasts one episode (the level's duration: a few seconds for a micro-puzzle, tens of seconds for a task; the campaign card shows it as "N s episode"). At the end of the episode you get the end card with the score.

Use More to change the speed (0.25x to 50x), and **Skip** to jump to the next scripted event.

## 4. Check

**Check** scores the circuit headless, in the background, using the level's quick episode. It runs a separate copy of the engine, so it does not touch your live run. While it works the button reads "Checking..." and a card shows how many worlds are running and the elapsed time. A card then appears with the result: "Check: passed", "Check: not passed" or "Check: N of 5 worlds passed", the score, and the verdict sentence. A check that has not answered after 60 seconds is stopped, and a toast says so (a huge weight or current can stall a cell model). If you change the circuit while a check runs, the result is dropped.

Check runs **five worlds** (seeds 101 to 105, the circuit reseeded with each world, any weight spread redrawn). A level passed on one world earns the first star; all five passing earns the second.

| | Run | Check | Test x5 |
|---|---|---|---|
| Where | Live, on the bench | Headless, in the background | Live progress card |
| Worlds | The circuit's seed | Five: 101, 102, 103, 104, 105 | Five: 101, 102, 103, 104, 105 |
| Episode | The full episode | The quick episode | The full episode |
| Records a pass | Yes | Yes | Yes, with the five-world result |
| Counts toward the second star | No | Yes, when all five pass | Yes, when all five pass |

A seed picks the random parts of the world (when the touches arrive, how noisy the input is). A challenge variant is checked on one world, and earns no stars.

In Researcher mode a pass counts only when all five worlds pass, so a single live pass shows a "Passed once" toast and nothing is recorded until Check or **Test x5** passes all five.

## 5. Read the verdict

The verdict is one sentence about what the circuit did. It is written to name the **mechanism** that is missing, not the number to type. For example, it may say that no cell needs more than one input at a time, or that the motor pool fires on noise. Verdicts and the first three hint tiers never carry numbers. The numbers live only in the recipe.

The **score card** under the world panel reads like a stamp:

- A ring shows the score, from 0 to 100, and the word SCORE, or PASSED once the level passes. The score is a quality number. A **pass** is a separate yes or no.
- Beside it, a status chip (RUNNING, PAUSED, FINISHED: NOT PASSED, PASSED), the result of the last Check, and the stars.
- **Criteria rows**: the verdict first, then one row for each judged trace ("inside the green band" or "outside"), then each pass flag. A mark says where each stands: a tick is met, a cross is not met, a dot is still being measured.
- **Details** turns the rows into the raw score parts under human labels (hits, misses, false withdrawals, a latency, a rate), the neuron count against the par, and the spike rate. Hide details turns it back. The part the verdict is about has a red outline.

After a failed run:

- The part of the circuit the verdict is about **glows red** on the bench. It is the same element a tier-2 hint would outline.
- The **timeline** shows where a trace left its band.

Reaching a good score is not enough to pass: each level states a pass rule, and the mechanism check has to hold.

## 6. Improve, with hints

Hints exist for when you are stuck. They come in three tiers. See [stars, par and hints](stars-and-hints.html).

- Tier 1 (THE IDEA) names the idea.
- Tier 2 (THE PLACE) points at the place and outlines it on the bench.
- Tier 3 (A PARTIAL RECIPE) gives a partial recipe, still without numbers.

In Guided mode the **Hint** button is open from the start. In Standard it opens after two missed runs (a run and a Check of the same circuit count once). Researcher mode has no hints. Hint use is recorded and never punished. It does not change your stars. Opening the **recipe** in Standard mode does: it caps the level at two stars.

## 7. Stars

| Stars | You need |
|---|---|
| 1 | Pass the level. |
| 2 | **Check** (or Test x5) passes all five worlds. Standard or Researcher. |
| 3 | The same, and the level's **challenge**, written on its card (by default the neuron par; a few levels add an energy ceiling or ask for no modulator). In Standard, without opening Show recipe. |

- **Guided earns at most two stars.** Its card says "Guided stops at two stars".
- **Researcher** passes are judged under physiological noise with tight bands, and a Researcher pass also earns a **seal**, kept apart from the stars.
- **Diagnosis cases:** 1 star for naming the cause, 2 for naming it with no wrong answer, 3 for the first try after at least one probe that tells the candidates apart, with few probes. A revealed answer caps the case at one star.
- Challenge variants give no stars.

The end card says what blocked a third star, for example "20 neurons, par is 16". The stars are kept per mode and the best counts. See [stars, par and hints](stars-and-hints.html).

## 8. After the pass

- **First pass.** A FIRST PASS stamp runs, a trail lights the critical path through your circuit (sensor to motor), and a toast says a notebook entry has unlocked. See [the notebook](notebook.html).
- **End card.** The score stamp ("N of 100"), your stars and best score, what the next star needs, and buttons: Next level (the main one after a pass), Run again, Test x5, Results, Lesion lab, Analysis and Close.
- **Results.** A card compares your circuit with generated variants on neurons, energy and quality. The variants are random edits of the reference circuit, kept if they pass. They are not other players.
- **Lesion lab.** Cut parts and re-run on several seeds. See [the lesion lab](lesion-lab-and-analysis.html).
- **Analysis.** Motifs, bursts, phase response, energy, reverse correlation. From the second chapter on.
- **Reference.** Load the level's reference circuit to compare. It earns no stars and the end card says so.
- **Variants.** Challenge variants (half the neurons, double noise, and so on) are optional. They give no stars and never unlock anything.

## The reference circuit and stars

A circuit that equals the level's reference file, or that the **Reference** button filled onto the bench, earns no stars and unlocks nothing. The game compares the whole circuit: groups, models, sizes, connections, positions, the file's notes and every number to seven significant figures (a relative 0.0001 per cent). A circuit you built by hand with round numbers is never flagged. The mark is kept in the circuit file, so it survives a restart, and an edited copy of the reference stays marked. In practice: press **Empty bench** and build your own.

## What unlocks what

| Thing | Opens when |
|---|---|
| A level in a chapter | You pass the one before it on that chapter's main path (the spine). |
| A body chapter | You pass the previous chapter's main path, or the one two before it. Invertebrates and Spinal cord are open from the start. The order is Invertebrates, Spinal cord, Heart, Breathing, Autonomic. |
| Branches (diagnosis cases, variants, and optional levels such as Posture and Reach) | You pass the level they hang off. They never gate anything. |
| The Bestiary and the Theory wing | You pass the Invertebrates spine. Inside the Bestiary a chapter may also wait for something it builds on: Sea slug for Heart's Neuromodulation level, Crab for the Leech chapter, Fly mushroom body for the Sea slug chapter. Guided mode drops these in-wing waits. |
| Inhibitory neurons, noise inputs, the parts bin | The first level whose reference needs them. |
| Analysis, Load and Save (and Copy and Paste circuit JSON, Import from a passed level) | The second chapter, Spinal cord. |
| Lesions, Reference | After you pass the level (Reference also opens in Guided mode). |

The level card shows what stops a locked level ("Pass X first"). It also lists **Needs** chips in red: concepts the level uses that another level teaches and you have not passed yet. They are advice, not locks.

## Related pages

- [Modes](modes.html)
- [Stars, par and hints](stars-and-hints.html)
- [Tips and common mistakes](tips.html)
- [Diagnosis cases](diagnosis-cases.html)
