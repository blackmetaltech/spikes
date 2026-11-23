---
title: Diagnosis cases
section: Playing
order: 12
summary: How a diagnosis case works, what counts as a probe, how it is scored and how stars are earned, with no answers given.
---

# Diagnosis cases

A diagnosis case turns the game around. In a normal level you build a circuit. In a case you get a working circuit with one dead connection and a short description of what is wrong with the "patient". Find the dead connection.

A symptom is something the patient does wrong. A lesion is damage to part of the circuit: here, one connection set to zero strength. The patient is invented. The circuit is a model.

There are thirteen cases: six in the body chapters and seven in the Bestiary. Each branches off a level and opens when you pass that level. A case is never on a chapter's spine and never gates anything.

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

See [the body chapters](campaign-body.html) and [the Bestiary](campaign-bestiary.html) for what each patient shows.

> **Note:** The cases are fictional models of a real kind of case. The shaky arm, for example, is not clinical clonus. [What is real](what-is-real.html) says what each case echoes and where the model stops matching.

## What the screen looks like

The lab opens on the patient's circuit, read-only. You cannot add or move parts. The inspector holds the **case card**:

- **PATIENT SYMPTOMS**: what the patient does, in a few sentences.
- **Probe buttons**: **LESION LAB** (opens or leaves the lesion lab) and a reminder that the scopes are on the right.
- **Used so far**: your probe and wrong-guess count.
- **A list of candidate connections**, each in plain words (such as "the link from X to Y"). The order is fixed for each case and tells you nothing: a rule shuffles it and never puts the answer first.
- **SUBMIT DIAGNOSIS**, and, after three wrong guesses, **REVEAL ANSWER**.

Click a group or connection to see read-only facts: the group's model and size, or a connection's type, direction and delay. The inspector never shows a connection's weight. You must find the dead one, at weight zero, by probing.

Every candidate connection is drawn on the bench. One of them is dead.

The case remembers your wrong guesses, probes and any reveal if you leave and come back. Closing the lab does not reset the tally. Solving the case clears it for the next visit.

## How to probe

A probe is any of these three actions.

| Probe | How |
|---|---|
| Open a scope | Select a group and press **Trace**. Opening a voltage trace on a group of the patient counts. |
| Cut a connection | In the lesion lab, click an axon, a group or a sensor card so it is marked cut. |
| Re-run a lesion | In the lesion lab, press **Run lesion** or **Sweep all singles**. Each lesion re-run is one probe. A sweep of every single lesion is as many probes as there are lesions. |

Each thing probed counts once. Opening the same scope again, or cutting the same connection again, is free. Scopes on two different groups are two probes.

What to look for:

- Watch which groups fire and which stay silent. A group that never fires although its input does has a dead connection upstream.
- Cut a suspect in the lesion lab. A working connection changes the symptoms or the downstream activity. The dead one changes nothing, because it was already doing nothing.
- Compare the groups at both ends of each candidate.

The raster and traces work as in any level. See [how to read the lab](how-to-read-the-lab.html).

## How a case is scored

The score depends on what the case took.

> value = 100, minus 15 for every wrong guess, minus 3 for every probe beyond the first three, never below 25.

| Used | Score |
|---|---|
| 3 probes, no wrong guess | 100 |
| 8 probes, no wrong guess | 85 |
| 3 probes, 1 wrong guess | 85 |
| 3 probes, 3 wrong guesses | 55 |
| Many wrong guesses and probes | 25 at the lowest |

A wrong guess says only "not that one", with no reason. The reasons for every candidate appear on the result card once you solve the case or reveal the answer. So guessing is not a strategy.

**REVEAL ANSWER** (after three wrong guesses) shows the answer and the reasons, and nothing is scored. The reveal is remembered. If you later solve the case yourself it counts, but earns one star at most.

## Stars for a case

| Stars | Condition |
|---|---|
| 1 | Name the cause. |
| 2 | Name it with no wrong answer. |
| 3 | No wrong answer, a score of 85 or more (at most 8 probes), and the right answer given **after at least one of the case's own probes**: one of the one or two probes that tell its top candidates apart (a scope on the group behind the dead link, or a cut of that link). A guess with no probe never earns the third star, however lucky. Guided mode stops at two stars. |

Cases have no par, no Test x5 and no challenge variants. The notebook entry opens when you solve the case. The campaign card lists the three conditions as for any level.

## After the answer

The case card shows CORRECT (or THE ANSWER), the lesion in plain words, your tally and a verdict. **Why not the others** follows: one line per wrong candidate saying why it does not fit the symptoms. The notebook entry with its citation sits below.

## Tips

- Spend your three free probes on looking, not cutting. Open scopes on the groups the symptoms mention.
- Read what the patient fails to do, then ask which part of the circuit does it.
- A cut that changes nothing is also a finding. Note which connection it was.
- Two candidates may look alike on the scopes. Cut each in the lesion lab: one changes the symptoms, the other does not.

See also [the lesion lab and analysis tools](lesion-lab-and-analysis.html) and [tips and common mistakes](tips.html).
