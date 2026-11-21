---
title: Stars, par and the hint ladder
section: Playing
order: 11
summary: How stars are earned, what the five-world Check, par and the challenge mean, what the recipe costs, and how the three hint tiers work.
---

# Stars, par and the hint ladder

Each level can give up to three stars. This page says exactly what earns them, what par and the energy budget are, and how hints work.

## The three stars

| Stars | Condition |
|---|---|
| 1 | The level is passed. |
| 2 | **Check** passes all five worlds (Standard or Researcher). |
| 3 | Star 2, plus the level's own **challenge**: an optional goal written on the level card before you start, for example "Build it with no more than 16 neurons (par)". In Standard mode you also must never have opened **Show recipe**. |

**Guided mode earns at most two stars.** It is the mode with the recipe and free hints, so the third star is not offered there. In **Researcher** mode a pass is five worlds under the mode's physiological noise and tight bands, and it earns the **Researcher seal** as well, a mark kept apart from the stars on the level card ("sealed").

The level card shows the three conditions for your current mode, with the challenge text in full. Nothing that can block a star is left unstated.

### How Check runs the five worlds

**Check** now scores your circuit in five slightly different worlds at once: seeds 101 to 105, with the circuit reseeded as well, using the level's quick episode. The toast says "Check: 4 of 5 worlds passed" and shows the verdict of the first world that failed. A pass in one world is the first star; all five is the second. A circuit that only just works fails some of the five: make it less fragile, then check again. **Test x5** in the More menu is the same test on the full episode, and records the mean score.

A **spread** on a connection (a percentage of weight variation across the synapses of an axon) is drawn again for every world. A circuit that only works with identical weights fails here.

## Par

Par is a neuron count that each level sets, and the default challenge asks for it. It is the size a tidy circuit needs. It is close to the size of the level's reference circuit for most levels but not always the same number: the level decides it. The campaign card and the brief state it ("Pass with 16 neurons or fewer").

Par counts the neurons of your circuit, with the cells of each group added up. A group of ten counts as ten. Par does not count sensors and motors.

| Level (example) | Par |
|---|---|
| Threshold | 1 neuron |
| Excitation: weight and delay | 2 |
| Convergence | 1 |
| Reflex arc | 16 |
| Worm chemotaxis | 28 |
| Stretch reflex | 54 |
| Spinal sprint (boss) | 80 |
| Autonomic exam (boss) | 75 |

The campaign pages list the par of every level: [body](campaign-body.html), [Bestiary](campaign-bestiary.html), [Theory](campaign-theory.html). A diagnosis case has no par.

## The energy clause

A real neuron pays for every spike in the molecule ATP, the cell's energy currency. The game counts it as the **mean firing rate per neuron over a whole episode**, in spikes per second. A few levels add an energy clause to their challenge, and then the card says so: "Keep the mean rate under 40 spikes/s per neuron". The ceiling is 1.6 times the rate of the level's reference circuit, rounded.

| Level | Ceiling (spikes/s per neuron) |
|---|---|
| Reflex arc | 40 |
| Worm | 130 |
| Lamprey | 25 |
| Stretch reflex | 60 |
| Renshaw | 80 |
| Posture | 60 |
| Reach | 55 |
| Heart pacemaker | 3 |
| Autonomic heart rate | 14 |
| Baroreflex | 35 |
| Breathing | 25 |
| Pupil | 30 |
| Peristalsis | 10 |
| Fight or flight | 26 |

Levels not in the table have no energy clause. The Analysis energy tab still shows a reference ceiling of 40 for them, for information only. The end card translates the rate to a rough cost: about 1e8 to 1e9 ATP per spike (Attwell and Laughlin 2001; Lennie 2003), so read it as an order of magnitude. See [the lesion lab and analysis tools](lesion-lab-and-analysis.html).

If a third star is blocked, the end card says why: "N neurons, par is M" or "x spikes/s per neuron, ceiling y".

## The recipe

The recipe is the reference circuit's numbers written out in words. In Guided mode it is in the brief. In Standard mode it is behind **Show recipe**. Pressing it is saved for that level and caps the level at two stars. The inspector then says: "You used the recipe: this level can earn two stars, not three." Researcher mode has no recipe.

## The hint ladder

Hints are a ladder of three tiers. You climb one rung at a time.

| Tier | Name on the card | What it gives |
|---|---|---|
| 1 | THE IDEA | Names the concept. For example: this needs inhibition between the two sides. |
| 2 | THE PLACE | Points at the part of your circuit the problem is in, and outlines it on the bench. |
| 3 | A PARTIAL RECIPE | Describes the wiring in words. It still has no numbers. |

Hint text depends on what your circuit did. The same level can give a different tier 1 if your circuit was jumpy than if it was silent.

### When hints are open

| Mode | Hints |
|---|---|
| Guided | Free from the start. |
| Standard | Open after two missed runs. The game counts a miss once per different circuit, so a run and a Check of the same circuit are one miss. A toast says "Hints unlocked". Before that the Hint button reads "Hint after 2 missed runs (n so far)". |
| Researcher | None. The button is hidden. |

Open the card with the **Hint** button on the bar, or from the end card. Press **Show hint 2: the place** (and so on) to go down the ladder. Run the circuit once first, so the hint can look at what went wrong. A hint can be asked for before any run but it will say "Run the circuit once so the hint can look at what went wrong".

> **Note:** Hint use is recorded and never punished. It does not touch your stars. The campaign card shows "Hints used: n (highest tier t). Recorded, never punished."

## The verdict highlight

After a failed run a part of the bench glows red. This is not a hint. It is free in every mode, including Researcher. It marks the element the verdict is about: a population, a sensor, a motor card or a connection. If the verdict is about the whole circuit, nothing glows.

## What does not give stars

- A copy of the reference circuit.
- A challenge variant (it has a DONE chip and gives no stars).
- A diagnosis case: it has its own star rules (its third star needs a probe). See [diagnosis cases](diagnosis-cases.html).

## Related pages

- [Modes](modes.html)
- [The core loop](core-loop.html)
- [Tips and common mistakes](tips.html)
