---
title: Tips and common mistakes
section: Playing
order: 16
summary: What the verdicts and playtests show players get wrong, and habits that save time.
---

# Tips and common mistakes

This page collects what trips players up, from the structure of the verdicts and the project's playtest reports. It gives habits, not solutions.

## Habits that save time

**Read the brief twice.** TO PASS says what the score measures. Most failed first tries are a mismatch between what you built and what the pass rule counts.

**Open a trace before you change a weight.** Select a neuron and press **Trace**. You see the voltage, so you know whether a cell is below threshold, just at it, or far over it. A weight changed without looking is a guess.

**Change one thing at a time.** Change one weight, press Reset, Run. Change three things and you will not know which helped.

**Type numbers.** The weight sliders are square-law: steps are fine near zero and coarse near the top. Click the number next to the slider and type a value for a precise change (it is clamped to the slider's range). The range is generous: it reaches well past what the level needs.

**Use the speed and Skip.** More, then a speed of 5x or 10x, shortens a long episode. **Skip** jumps to just before the next scripted event.

**Use Check for fast tries, Run for understanding.** Check scores five worlds in the background without waiting for the episode. Run shows you why.

**Look at the glow.** After a failed run, the red part of the bench is what the verdict is about. Start there.

**Pass first, then trim.** Get a working circuit with a few extra neurons. Then remove neurons one at a time to meet the level's challenge for the third star (its card says what it is). The lesion lab shows which neurons you can lose.

**Keep a copy before you experiment.** Save the working circuit to a solution slot (More, Save to A) before you try something drastic. Undo goes back through your edits. More, Restore previous bench brings back a bench that a button replaced. A mode switch does not change the circuit, so you can try Guided, pass, and rebuild in Standard.

## The five most common mistakes

### 1. Testing on one seed only

A circuit that passes on one world can fail on others if a weight sits right at an edge. **Check** runs five worlds. Use it early, not only at the end, and read "Check: n of 5 worlds passed". After a failed Test x5 the end card says "It passes the fixed world but not every seed: make it less fragile". Move the weight toward the middle of the range that works. A **spread** of a few percent on the connection (inspector) shows fragility at once.

### 2. Building a single cell where a group is needed

A single cell sees noise as a signal. Many levels need **convergence** (a cell that needs several inputs at once) or **a group** (several cells that average out noise). The reflex says it plainly: one cell on its own cannot pass, whatever its weight. Use **neurons in group** to make ten cells at the cost of one disc.

### 3. Forgetting that sensor weight and synapse weight are separate

A sensor card has its own weight (click the card). A connection between two cells has another. The sensor weight sets how hard the task's input drives the first cell. On the first level you control only the sensor weight. On later ones both matter.

### 4. Copying the reference

If your circuit equals the reference circuit, or the **Reference** button filled the bench, it earns no stars. The end card says so. Use **Empty bench** and build your own.

### 5. Wiring a motor channel without the right output kind

Some levels read a **modulator** (acetylcholine or noradrenaline) instead of a rate, for example the heart levels. Switch the motor card to "releases modulator". The inspector tells you: "This task reads NE from this channel: switch it to 'releases modulator'". Heed that line.

## Mistakes that look like bugs but are not

| What you see | What is going on |
|---|---|
| The cell fires at 250 to 400 Hz and nothing is hurt | A model cell with no adaptation can fire near its refractory limit. Real persistent firing is 10 to 50 Hz. The Theory wing says so on its page. |
| The verdict asks for "more strength" but the slider is at its top | If a weight is at the end of its slider, the game replaces such a verdict with: "A weight is already at the end of its slider, so more strength is not the answer: try more cells in a group, another cell type, or a different route." The mechanism, not the weight, is missing. |
| A tool is missing (inhibitory neurons, parts, analysis) | It opens at the first level that needs it. More says what opens later. The [sandbox](sandbox.html) has everything. |
| "You built a rhythm by accident!" | A toast. It appears when a loop in your circuit starts to oscillate and the task has no periodic stimulus. It is a celebration, not an error. |
| Hint button reads "Hint after 2 missed runs" | Standard mode. Two different failed circuits open it. |
| A level passes on Check but the end card says not passed | Check ran the quick episode. The live run runs the full one. Run a full episode and Test x5 (the full episode on the same five worlds). |
| The score never rises above 49 and the verdict says the circuit brings a stimulus of its own | An input the level does not drive (a spike train, an extra generator, a free noise input) is wired in. Delete it and wire the sensors the brief names. |
| A wire seems to do nothing | A connection or sensor weight below a small floor counts as no wiring, and the verdict may say a sensor or motor is not connected. Use a weight that really moves the cell. |
| A banner says the engine rejected the circuit | A weight, current, rate or delay is outside what the engine allows, or the circuit is too big. The banner names the place and the limit; see [how to read the lab](how-to-read-the-lab.html#when-the-lab-refuses-a-circuit). |
| The numbers in the game differ from the animal's | Time is compressed in the leech and Aplysia levels, and spiking cells stand in for graded ones. See [what is real](what-is-real.html). |

## Steeper levels

A scripted novice played the 0.5 build using only the briefs and the lab's defaults. These levels were the steepest. Several were adjusted afterwards, so read this as a warning that they reward patience, not as a fixed ranking.

| Level | Why it bites |
|---|---|
| Worm chemotaxis | The behaviour is judged in a closed loop. The worm moves in a gradient, so a weight changes where it goes, and then what it senses. |
| Half-centre | Two cells that inhibit each other need a drive and a slow variable (adaptation) to take turns. |
| Rebound after inhibition | The rebound needs a steady background drive that the brief names. |
| The gill-withdrawal reflex | The synapse changes with use. The effect of a weight depends on what came before. |
| Reach | It is a branch (a challenge), not on the main path. |
| The two bosses | They combine several earlier ideas in one long episode. Pass the parts first. |

## Diagnosis tips

- Spend the three free probes on looking, not cutting.
- A wrong guess costs more than several probes (15 points against 3 each beyond the first three).
- The third star needs the right answer on the first try after a probe that tells the candidates apart (a scope opened on a group, or a cut). Probe before you answer.
- The symptom text says what the patient cannot do. Ask which part of the circuit does that.
- See [diagnosis cases](diagnosis-cases.html).

## Learning tips

- If a level is too hard in Standard, switch to Guided, pass it and read the recipe afterwards (Guided earns at most two stars). Then switch back and rebuild it from memory.
- After each pass, read the notebook entry. It is the biology behind what you just built.
- Use the lesion lab to ask "which of my neurons does nothing?" Delete those. See [the lesion lab](lesion-lab-and-analysis.html).
- Use the sandbox to try a new part before a level needs it. See [the parts bin](parts-bin.html).

## Related pages

- [The core loop](core-loop.html)
- [Stars, par and hints](stars-and-hints.html)
- [FAQ](faq.html)
