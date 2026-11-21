---
title: Your first hour
section: Start here
order: 2
summary: A step-by-step walk through the first chapter, Invertebrates, with what to click and what to look for.
---

# Your first hour

This page walks through the first chapter of the campaign, Invertebrates, with what to click at each step. It teaches the controls and the way of thinking. It does not give the solutions, so you still have something to work out.

The chapter opens with three short puzzles, then your first real task, the touch reflex:

| Order | Level | The idea |
|---|---|---|
| 1 | Threshold | A neuron ignores weak input and fires on strong input. |
| 2 | Excitation: weight and delay | A synapse has a weight (does the spike get through) and a delay (when). |
| 3 | Convergence | A neuron adds up what arrives together. |
| 4 | Reflex arc | Use all three to ignore noise and react to a real touch. |

The first three are micro-puzzles. A micro-puzzle is a "fish tank": one to four neurons, a few seconds of simulated time, one concept. Plan on about fifteen minutes for each of the first three and a little longer for the reflex.

## Before you start

Start the game, choose Play, and look at the campaign map. See [getting started](getting-started.html) if you have not installed it yet.

The map shows the chapters as tabs along the top. The first tab, Invertebrates, shows its levels as pins on a drawing of a worm. A small pin is a micro-puzzle. A large pin is a task. Click the first pin, Threshold. A card shows the brief, the number of stars (none yet), and a Build button.

Leave the difficulty mode at its default, Standard, for now. [Modes](modes.html) explains the three choices. If you want the most help, switch to Guided in the campaign header: it prints the recipe in the brief and makes hints free.

## Level 1: Threshold

**Step 1: read the brief.** Press Build. You land in the lab: an empty bench. The inspector on the lower left shows the brief. It says what to build ("TO BUILD") and what counts as a pass ("TO PASS"). Press More to read the rest of a long brief.

For Threshold the brief asks for one neuron that stays silent during a small blip of input and fires during a big step. The knob you control is the weight of the sensor called `pulse`.

**Step 2: add a neuron.** Press **+ Excitatory** in the row above the circuit. A round cell appears. Excitatory means its spikes push the next cell toward firing. Drag the cell where you like.

**Step 3: wire the sensor to the neuron.** On the left edge there is a card labelled `pulse`. That is the sensor. Its colour chip says SENSOR. On the right edge is a card labelled `spike`, the motor output (a motor output is something the circuit does to the world; here it is read off as the cell's firing rate). Wire in two moves:

- Drag from the right edge of the `pulse` card onto your neuron.
- Drag from the rim of your neuron onto the `spike` card.

You can also press **Connect**, then click the source and then the target. The rim of a cell is the outer ring; dragging from the middle moves the cell instead.

**Step 4: run it.** Press **Run**. The raster (the strip of tick marks, one row per neuron) fills with spikes. The traces show the cell's voltage. The `spike` gauge shows the firing rate in hertz. The timeline under the world labels the "small blip" and the "big step".

**Step 5: change the weight and watch.** Click the `pulse` card. The inspector shows its **weight** in pA, a slider, and a box you can click to type a number. Change it, press Reset, press Run. Ask: at this weight, does the cell fire during the blip? During the step? You are looking for a weight where the answer is no and yes.

**Step 6: Check.** Press **Check**. The game scores your circuit in the background in a second or two and shows a card with pass or not passed, a score from 0 to 100, and a sentence: the verdict. If the circuit did not pass, a part of the circuit glows red. That is the part the verdict is about.

**Stuck?** The idea: every cell has a smallest steady current that makes it fire (its rheobase). Below it, waiting longer changes nothing. Your weight times the level of the sensor is the current the cell receives.

When you pass, a "FIRST PASS" stamp appears, a notebook toast tells you a new finding was unlocked, and the **End card** shows your score and stars. Read the end card: it says what the next star needs.

## Level 2: Excitation: weight and delay

This time two sensors and motors matter. A `tap` input makes a first cell fire (`source`). You build a second cell that repeats every spike of the first, a fixed time later, and read it on `echo`.

New things to try:

- **Select a connection.** Click the line between two neurons. The inspector shows its weight, its delay in milliseconds and its type. The arrowhead means excitatory. A bar end means inhibitory.
- **Two controls, two jobs.** The weight decides whether a spike gets through. The delay decides when the next cell answers. The brief says both.
- **The sliders are square-law.** Small steps near zero, large steps near the top, so you can reach both fractions of a picoamp and thousands. Click the number and type to jump to an exact value.

The scoring bands are stated in the brief, so there is no guesswork about what "close enough" means. The score card on the right shows which parts of the score you are meeting.

**Stuck?** A synapse has two numbers. One decides whether a single spike is enough. The other decides when the answer comes. The second cell needs a moment to reach its own threshold, and that time adds to the delay you set.

## Level 3: Convergence

Three inputs, `in_a`, `in_b` and `in_c`, switch on in different combinations. You build one neuron that fires only when all three are on together.

New things to try:

- Wire each of the three sensors to the same neuron. Each sensor has its own weight (click its card).
- Use the **Trace** button in the neuron's inspector. It opens a voltage trace for that cell. The trace shows how close the cell gets to its threshold when one, two, or three inputs are on. Seeing the voltage creep up is the point of the level.

**Stuck?** A neuron adds what converges on it and fires when the sum crosses its threshold. Make each input too weak to do it alone, and two together still not enough.

## Level 4: Reflex arc

Your first task. A touch receptor chatters at about 150 spikes per second all the time, and fires a little faster, about 400 per second, during a poke. You wire it so the animal pulls back within 60 ms of every poke, yet stays still on chatter alone. Six pokes arrive at random times in 20 seconds. A "withdrawal" is a burst of motor spikes: at least 3 within 25 ms, or half the pool if it has more than 6 cells.

What is new:

- **Groups of neurons.** Select a neuron and change **neurons in group** (1, 2, 3, 4, 6, 8, 10, 12 or 20). A group is drawn as one disc. A sensor wired to a group of ten gives ten receptor cells.
- **Noise rejection.** One cell listening to one afferent cannot pass at any weight. The brief says why: noise spikes bunch up by chance. Averaging many independent inputs separates a real poke from the chatter.
- **Test x5.** After a pass, the bar gains a **Test x5** button. It re-runs the episode on five other random worlds. If all five pass you earn the second star. See [stars, par and hints](stars-and-hints.html).
- **Par.** The third star asks for the same result with few enough neurons (the par). The reflex's par is 16 neurons.

**Stuck?** Convergence again. If the problem is noise, a motor cell that needs several inputs at once is steadier than one that needs a single input.

## What to do next

You have used every basic control. Now:

1. Open the **Notebook** from the campaign map and read the entries you unlocked. See [the notebook](notebook.html).
2. Try the other levels of the chapter. Inhibition and Subtraction come next, then the salt-seeking worm, then Adaptation and the Half-centre, and the lamprey that swims.
3. After a pass, open **More** and try **Lesions**. Cut a connection and see what the circuit loses. See [the lesion lab](lesion-lab-and-analysis.html).
4. Read [how to read the lab](how-to-read-the-lab.html) for every panel, and [the core loop](core-loop.html) for the cycle you will repeat for the rest of the game.

> **Note:** Some tools stay hidden at first on purpose. Inhibitory neurons, noise inputs, the parts bin, analysis and load/save appear when the campaign reaches a level that needs them. The More menu says what is still to come. The [sandbox](sandbox.html) has everything unlocked.

![The lab on the Threshold level, with the circuit, scopes, and the brief](img/first-hour-lab.png)
