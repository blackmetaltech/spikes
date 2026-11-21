---
title: Diagnosis cases
section: Playing
order: 12
summary: How a diagnosis case works, what counts as a probe, how it is scored and how stars are earned, with no answers given.
---

# Diagnosis cases

A diagnosis case turns the game around. In a normal level you build a circuit to do a job. In a case you are handed a working circuit that has one dead connection, and a short description of what is wrong with the "patient". Your job is to work out which connection is dead.

A symptom is something the patient does wrong. A lesion is damage to a part of the circuit. Here the lesion is one connection that has been set to zero strength. The patient is invented. The circuit is a model.

There are thirteen cases. Six belong to the body chapters and seven to the Bestiary. Each hangs off a level you must pass first.

| Case | Opens after you pass | Chapter |
|---|---|---|
| The shaky arm | Renshaw cell | Spinal cord |
| The racing resting pulse | Autonomic heart rate | Heart |
| Faint on standing | Baroreflex | Heart |
| Stops breathing in sleep | Breathing rhythm | Breathing |
| The small pupil | Pupil light reflex | Autonomic |
| Food that will not pass | Peristalsis | Autonomic |
| The worm that cannot back up | Worm: touch, reverse or go | Bestiary: worm |
| The one-sided heartbeat | Leech: the heartbeat | Bestiary: leech |
| The reflex that does not sensitise | Aplysia: the gill-withdrawal reflex | Bestiary: sea slug |
| The compass that walks off | Fly: the compass in the dark | Bestiary: fly compass |
| The fly that cannot tell odours apart | Fly: the mushroom body learns to avoid | Bestiary: fly mushroom body |
| The fly that never learns | Fly: the mushroom body learns to avoid | Bestiary: fly mushroom body |
| The follower that does not wait | Crab: the pyloric rhythm | Bestiary: crab |

A case never gates anything. See [the body chapters](campaign-body.html) and [the Bestiary](campaign-bestiary.html) for what each patient shows.

> **Note:** The cases are fictional models of a real kind of case. For example, the shaky arm is not clinical clonus. [What is real](what-is-real.html) says what each case echoes and where the model stops matching.

## What the screen looks like

The lab opens on the patient's circuit, read-only. You cannot add or move parts. The inspector holds the **case card**:

- **PATIENT SYMPTOMS**: a few sentences about what the patient does.
- **Probe buttons**: **LESION LAB** (opens or leaves the lesion lab) and a reminder that the scopes are on the right.
- **Used so far**: your probe and wrong-guess count.
- **A list of candidate connections**, each in plain words (the name of the missing wire, such as "the link from X to Y").
- **SUBMIT DIAGNOSIS**, and, after three wrong guesses, **REVEAL ANSWER**.

You can click a group or a connection of the patient. The inspector shows read-only facts: the group's model and size, or a connection's type, direction and delay. It never shows a connection's weight. The dead connection is kept at weight zero and must be found by probing.

Every candidate connection is drawn on the bench. One of them is dead.

## How to probe

A probe is any of these three actions.

| Probe | How |
|---|---|
| Open a scope | Select a group and press **Trace**. Opening a voltage trace on a group of the patient counts. |
| Cut a connection | In the lesion lab, click an axon, a group or a sensor card so it is marked cut. |
| Re-run a lesion | In the lesion lab, press **Run lesion** or **Sweep all singles**. Each lesion re-run is one probe. A sweep of every single lesion is as many probes as there are lesions. |

Each probe is counted once per thing probed. Opening the same scope again, or cutting the same connection again, is free. Opening scopes on two different groups is two probes.

What to look for:

- Watch which groups fire and which do not answer. A group that never fires although its input does, has a dead connection upstream.
- Cut a suspect in the lesion lab. A working connection changes the symptoms or the activity downstream. The dead one does not, because it was already doing nothing.
- Compare the groups at both ends of each candidate.

The raster and traces work as in any level. See [how to read the lab](how-to-read-the-lab.html).

## How a case is scored

The case is scored on what it took.

> value = 100, minus 15 for every wrong guess, minus 3 for every probe beyond the first three, never below 25.

| Used | Score |
|---|---|
| 3 probes, no wrong guess | 100 |
| 8 probes, no wrong guess | 85 |
| 3 probes, 1 wrong guess | 85 |
| 3 probes, 3 wrong guesses | 55 |
| Many wrong guesses and probes | 25 at the lowest |

A wrong guess tells you only "not that one". It does not give a reason. The reasons for every candidate appear on the result card once you solve the case or reveal the answer. This stops guessing from being a strategy.

If you press **REVEAL ANSWER** (after three wrong guesses), you see the answer and the reasons. You do not pass the case.

## Stars for a case

| Stars | Condition |
|---|---|
| 1 | Name the cause. |
| 2 | Name it with no wrong answer. |
| 3 | No wrong answer, and a score of 85 or more. That is at most 8 probes. |

Cases have no par, no Test x5 and no challenge variants. The notebook entry for a case opens when you solve it.

## After the answer

The case card shows CORRECT (or THE ANSWER), the lesion in plain words, your tally, and a verdict. It then lists **Why not the others**: one line per wrong candidate saying why it does not fit the symptoms. The notebook entry with its citation is shown below.

## Tips

- Spend your three free probes on looking, not cutting. Open scopes on the groups that the symptoms mention.
- Read the symptom text for what the patient fails to do, and then ask which part of the circuit does that thing.
- A cut that changes nothing is also a finding. Note which connection it was.
- Two candidates may look alike on the scopes. Use the lesion lab to tell them apart: one of them, when cut, changes the symptoms, and the other does not.

See also [the lesion lab and analysis tools](lesion-lab-and-analysis.html) and [tips and common mistakes](tips.html).
