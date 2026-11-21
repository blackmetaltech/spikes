---
title: Tips and common mistakes
section: Playing
order: 16
summary: What the verdicts and playtests show players get wrong, and habits that save time.
---

# Tips and common mistakes

This page collects what trips players up, from the structure of the verdicts and from the project's playtest reports. It gives habits, not solutions.

## Habits that save time

**Read the brief twice.** The TO PASS part says what the score measures. Most failed first tries are a mismatch between what you built and what the pass rule counts.

**Open a trace before you change a weight.** Select a neuron and press **Trace**. If you can see the voltage, you know whether a cell is below threshold, just at it, or far over it. A weight changed without looking is a guess.

**Change one thing at a time.** Change one weight, press Reset, Run. If you change three things you will not know which helped.

**Type numbers.** The weight sliders are square-law: steps are fine near zero and coarse near the top. Click the number next to the slider and type a value for a precise change. The slider range comes from the level's reference circuit, with room above it.

**Use the speed and Skip.** More, then a speed of 5x or 10x, shortens a long episode. **Skip** jumps to just before the next scripted event.

**Use Check for fast tries, Run for understanding.** Check scores in a few seconds without waiting for the episode. Run shows you why.

**Look at the glow.** After a failed run, the red part of the bench is what the verdict is about. Start there.

**Pass first, then trim.** Get a working circuit with a few extra neurons. Then remove neurons one at a time for the par star. The lesion lab shows which neurons you can lose.

## The five most common mistakes

### 1. Testing on one seed only

A circuit that passes on the level's own seed can fail on others if a weight sits right at an edge. Press **Test x5** early, not only at the end. The end card says "It passes the fixed world but not every seed: make it less fragile." If it fails, move the weight toward the middle of the range that works. A **spread** of a few percent on the connection (inspector) shows fragility at once.

### 2. Building a single cell where a group is needed

A single cell sees noise as a signal. Many levels need **convergence** (a cell that needs several inputs at once) or **a group** (several cells that average out noise). The reflex says it plainly: one cell on its own cannot pass, whatever its weight. Use **neurons in group** to make ten cells at the cost of one disc.

### 3. Forgetting that sensor weight and synapse weight are separate

A sensor card has its own weight (click the card). A connection between two cells has another. The sensor weight sets how hard the task's input drives the first cell. On the first level, you control only the sensor weight. On later ones both matter.

### 4. Copying the reference

If your circuit equals the reference circuit, or the **Reference** button filled the bench, it earns no stars. The end card says so. Use **Empty bench** and build your own.

### 5. Wiring a motor channel without the right output kind

Some levels read a **modulator** (acetylcholine or noradrenaline) instead of a rate, for example the heart levels. The motor card then needs to be switched to "releases modulator". The inspector tells you: "This task reads NE from this channel: switch it to 'releases modulator'". Heed that line.

## Mistakes that look like bugs but are not

| What you see | What is going on |
|---|---|
| The cell fires at 250 to 400 Hz and nothing is hurt | A model cell with no adaptation can fire near its refractory limit. Real persistent firing is 10 to 50 Hz. The Theory wing says so on its page. |
| The verdict asks for "more strength" but the slider is at its top | The slider range is twice the largest reference weight. If the verdict still asks, the mechanism, not the weight, is missing. A verdict that asks for more strength is not shown at the slider's cap. |
| A tool is missing (inhibitory neurons, parts, analysis) | It opens at the first level that needs it. More says what opens later. The [sandbox](sandbox.html) has everything. |
| "You built a rhythm by accident!" | A toast that appears when a loop in your circuit starts to oscillate and the task has no periodic stimulus. It is a celebration, not an error. |
| Hint button reads "Hint after 2 missed runs" | Standard mode. Two different failed circuits open it. |
| A level passes on Check but the end card says not passed | Check ran the quick episode. The live run runs the full one. Run a full episode and Test x5. |
| The numbers in the game differ from the animal's | Time is compressed in the leech and Aplysia levels, and spiking cells stand in for graded ones. See [what is real](what-is-real.html). |

## Steeper levels

The playtest of the 0.5 build (a scripted novice using only the briefs and the lab's defaults) found these levels the steepest. Several were adjusted afterwards, so treat this as a warning that they reward patience and not as a fixed ranking.

| Level | Why it bites |
|---|---|
| Worm chemotaxis | The behaviour is judged in a closed loop. The worm moves in a gradient, so a weight changes where it goes, and then what it senses. |
| Half-centre | Two cells that inhibit each other need a drive and a slow variable (adaptation) to take turns. |
| Rebound after inhibition | The rebound needs a steady background drive that the brief names. |
| The gill-withdrawal reflex | The synapse changes with use. The effect of a weight depends on what came before. |
| Reach | It is a branch (a challenge), not on the main path. About 30% of generated variants pass. |
| The two bosses | They combine several earlier ideas in one long episode. Pass the parts first. |

## Diagnosis tips

- Spend the three free probes on looking, not cutting.
- A wrong guess costs more than several probes (15 points against 3).
- The symptom text says what the patient cannot do. Ask which part of the circuit does that.
- See [diagnosis cases](diagnosis-cases.html).

## Learning tips

- If a level is too hard in Standard, switch it to Guided, pass it and read the recipe afterwards. Then switch back and rebuild it from memory.
- After each pass, read the notebook entry. It is the biology behind what you just built.
- Use the lesion lab to ask "which of my neurons does nothing?" Delete those. See [the lesion lab](lesion-lab-and-analysis.html).
- Use the sandbox to try a new part before a level needs it. See [the parts bin](parts-bin.html).

## Related pages

- [The core loop](core-loop.html)
- [Stars, par and hints](stars-and-hints.html)
- [FAQ](faq.html)
