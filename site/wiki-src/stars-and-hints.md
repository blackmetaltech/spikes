---
title: Stars, par and the hint ladder
section: Playing
order: 11
summary: How stars are earned, what par and the energy budget mean, what the recipe costs, and how the three hint tiers work.
---

# Stars, par and the hint ladder

Each level can give up to three stars. This page says exactly what earns them, what par and the energy budget are, and how hints work.

## The three stars

| Stars | Condition |
|---|---|
| 1 | The level is passed. |
| 2 | The level is passed on all five **Test x5** seeds. |
| 3 | The same, using no more neurons than the level's **par**, and staying inside the **energy budget**. In Standard mode you also must never have opened **Show recipe**. |

In **Researcher** mode the wording shifts: the first star is a pass on all five seeds (a pass needs Test x5 there), and the second is a five-seed pass under the mode's physiological noise.

The campaign card shows the three conditions for your current mode. The brief repeats them in one line: "Stars: 1 pass. 2 pass on 5 seeds (Test x5). 3 the same with N neurons or fewer."

### How Test x5 works

**Test x5** runs the level five times. The worlds use seeds 101, 102, 103, 104 and 105. The circuit is reseeded as well. The score you keep is the mean of the five runs. All five must pass for the second star. If only some pass, the end card says "It passes the fixed world but not every seed: make it less fragile, then test again." Fragile usually means a weight sits right on an edge.

A **spread** on a connection (a percentage of weight variation across the synapses of an axon) is drawn again for every seed. A circuit that only works with identical weights fails here.

## Par

Par is a neuron count that each level sets. It is the size a tidy circuit needs. It is close to the size of the level's reference circuit for most levels but not always the same number: the level decides it. The campaign card and the brief state it ("Pass with 16 neurons or fewer").

Par counts the neurons of your circuit, with the cells of each group added up. A group of ten counts as ten. Par does not count sensors and motors.

| Level (example) | Par |
|---|---|
| Threshold | 1 neuron |
| Excitation: weight and delay | 2 |
| Convergence | 1 |
| Reflex arc | 16 |
| Worm chemotaxis | 28 |
| Stretch reflex | 52 |
| Spinal sprint (boss) | 80 |
| Autonomic exam (boss) | 75 |

The campaign pages list the par of every level: [body](campaign-body.html), [Bestiary](campaign-bestiary.html), [Theory](campaign-theory.html). A diagnosis case has no par.

## The energy budget

A real neuron pays for every spike in the molecule ATP, the cell's energy currency. The game counts it as the **mean firing rate per neuron over a whole episode**, in spikes per second. The **energy budget** of a level is 1.6 times the rate of its reference circuit, rounded. A level without its own entry has a default of 40 spikes per second per neuron.

| Level | Budget (spikes/s per neuron) |
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
| Other levels | 40 |

The end card translates it to a rough cost: about 1e8 to 1e9 ATP per spike (Attwell and Laughlin 2001; Lennie 2003), so read it as an order of magnitude. The Analysis screen has an energy tab that shows the same number plus a bar for each population. See [the lesion lab and analysis tools](lesion-lab-and-analysis.html).

If a third star is blocked, the end card says why: "N neurons, par is M" or "x spikes/s per neuron, budget y".

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
- A diagnosis case: it has its own star rules. See [diagnosis cases](diagnosis-cases.html).

## Related pages

- [Modes](modes.html)
- [The core loop](core-loop.html)
- [Tips and common mistakes](tips.html)
