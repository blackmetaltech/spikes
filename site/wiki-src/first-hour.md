---
title: Your first hour
section: Start here
order: 2
summary: A step-by-step walk through the first chapter, Invertebrates, with what to click and what to look for.
---

# Your first hour

This page walks through the first chapter, Invertebrates, and says what to click at each step. It teaches the controls and the way of thinking. It does not give solutions.

The chapter opens with three short puzzles, then your first real task, the touch reflex:

| Order | Level | The idea |
|---|---|---|
| 1 | Threshold | A neuron ignores weak input and fires on strong input. |
| 2 | Excitation: weight and delay | A synapse has a weight (does the spike get through) and a delay (when). |
| 3 | Convergence | A neuron adds up what arrives together. |
| 4 | Reflex arc | Use all three to ignore noise and react to a real touch. |

The first three are micro-puzzles: a "fish tank" with a few neurons, a few seconds of simulated time and one concept.

## Plate 0: the guided first neuron

On a fresh save, **Play** opens an interactive tutorial, *Plate 0: the first neuron*, instead of the map. It runs in the lab on the Threshold level, in Guided mode, and teaches the controls this page uses. Everything is dimmed except the control you need next, and a banner says what to do. A step moves on only when you have done the thing: the game watches the bench and the live run, not your clicks.

| Step | What you do |
|---|---|
| 1 | Look at the bench: a sensor on the left, a motor on the right. |
| 2 | Press + Excitatory to add a neuron. |
| 3 | Drag from the sensor card to the cell. A moving dot shows the drag. |
| 4 | Drag from the cell's rim to the motor card. |
| 5 | Watch the raster and the voltage trace while the circuit runs. The cell stays silent. (This step moves on by itself after a few seconds, or press Next.) |
| 6 | Click the sensor card and raise its weight until the cell spikes. |
| 7 | Find the middle of the weight range: spikes on the big step only, so the run passes. |
| 8 | Press Check. |
| 9 | Read the verdict, and where stars come from. |
| 10 | Flip a second cell to inhibitory with the E / I switch. |
| 11 | See where hints, the brief and the recipe are. |
| 12 | Back on the map, pick Guided, Standard or Researcher. |

You can leave any step. **Do it for me** performs the step's action. **Back a step** goes back one. **Next** moves on from a step that only asks you to look (it reads **Finish** at the last one). **Skip tutorial** (or Esc, or B on a pad) leaves at once. It works with the mouse, by touch, and with the keyboard or a pad (the banner buttons and the lit control take focus). With reduced motion on, the ring around the lit control stays still and the drag is shown as a fixed arrow.

The tutorial never runs twice by itself. When it ends, a one-screen picker asks for the difficulty mode (Guided, Standard or Researcher, one line each). If you skip the tutorial without picking a mode, the next **Play** shows the picker. To run it again, open **Play** and press **Tutorial** in the campaign header. A replay starts on an empty Threshold bench (the old bench is kept in the bench history, see More, Restore previous bench) and puts your own mode back afterwards.

A short pointer shows the first time you open a level with a new control: the E / I switch (Inhibition), the group size (Reflex arc), and the Advanced parameters (Adaptation and Tonic drive). Each has a **Got it** button. On the Threshold level a pulsing ring sits on the wired sensor card until you have clicked it once.

## Before you start

Start the game, choose Play, and look at the campaign map. See [getting started](getting-started.html) if you have not installed it.

The chapters are tabs along the top. The first, Invertebrates, shows its levels as pins on a drawing of a worm. A small bare pin is a micro-puzzle. Tasks and bosses carry a label with their title and stars. Click the first pin, Threshold. A card shows the brief, what each star needs, and a Build button. The **Continue** button next to the wing selector opens the first open level you have not passed.

If you chose no mode, you get Standard. [Modes](modes.html) explains the three. For the most help, switch to Guided in the campaign header: it prints the recipe in the brief and makes hints free.

## Level 1: Threshold

**Step 1: read the brief.** Press Build. You land in the lab on an empty bench. The inspector on the lower left shows the brief: what to build ("TO BUILD") and what counts as a pass ("TO PASS"). Press **More...** inside the brief to read a long brief in full (this is not the More menu on the top bar).

The Threshold brief asks for one neuron that stays silent during a small blip of input and fires during a big step. Your knob is the weight of the sensor called `pulse`.

**Step 2: add a neuron.** Press **+ Excitatory** in the row above the circuit. A round cell appears. Its spikes push the next cell toward firing. Drag it where you like.

**Step 3: wire the sensor to the neuron.** The card labelled `pulse` on the left edge is the sensor. Its colour chip says SENSOR. The card labelled `spike` on the right edge is the motor output (something the circuit does to the world; here it is read off as the cell's firing rate). Wire in two moves:

- Drag from the right edge of the `pulse` card onto your neuron.
- Drag from the rim of your neuron onto the `spike` card.

Or press **Connect**, then click the source, then the target. The rim is the outer ring. Dragging from the middle moves the cell.

**Step 4: run it.** The lab runs as soon as it opens (the button then reads Pause). Press **Run** to start again after a Reset or a pause. The raster (tick marks, one row per neuron) fills with spikes. The traces show the cell's voltage. The `spike` gauge shows the firing rate in hertz. The timeline under the world labels the "small blip" and the "big step".

**Step 5: change the weight and watch.** Click the `pulse` card. The inspector shows its **weight** in pA, with a slider and a box where you can type a number. Change it, press Reset, press Run. Ask whether the cell fires during the blip, and during the step. You want a weight where the answers are no and yes.

**Step 6: Check.** Press **Check**. The game scores your circuit in the background on five random worlds. A card shows the result (passed, not passed, or how many of the five worlds passed), a score from 0 to 100, and a verdict sentence. If the circuit did not pass, the part the verdict is about glows red.

**Stuck?** Every cell has a smallest steady current that makes it fire (its rheobase). Below it, waiting longer changes nothing. Your weight times the sensor level is the current the cell receives.

When you pass, a "FIRST PASS" stamp appears, a notebook toast announces a new finding, and the **End card** shows your score and stars. It says what the next star needs. A pass on one world is one star. Passing Check on all five worlds is the second.

## Level 2: Excitation: weight and delay

A `tap` input makes a first cell fire (`source`). You build a second cell that repeats every spike of the first, a fixed time later, and read it on `echo`.

New things to try:

- **Select a connection.** Click the line between two neurons. The inspector shows its weight, its delay in milliseconds and its type. An arrowhead means excitatory. A bar end means inhibitory.
- **Two controls, two jobs.** The weight decides whether a spike gets through. The delay decides when the next cell answers.
- **The sliders are square-law.** Steps are small near zero and large near the top, so you reach both fractions of a picoamp and thousands. Click the number and type for an exact value.

The brief states the scoring bands, so "close enough" is not a guess. The score card on the right lists the pass criteria with a tick, a cross or a dot for each. Details shows the numbers behind them.

**Stuck?** A synapse has two numbers. One decides whether a single spike is enough. The other decides when the answer comes. The second cell needs a moment to reach its own threshold, and that time adds to the delay you set.

## Level 3: Convergence

Three inputs, `in_a`, `in_b` and `in_c`, switch on in different combinations. Build one neuron that fires only when all three are on together.

New things to try:

- Wire all three sensors to the same neuron. Each sensor has its own weight (click its card).
- Press **Trace** in the neuron's inspector to open a voltage trace for that cell. It shows how close the cell gets to threshold with one, two or three inputs on. Watching the voltage creep up is the point of the level.

**Stuck?** A neuron adds what converges on it and fires when the sum crosses its threshold. Make each input too weak alone, and two together still too weak.

## Level 4: Reflex arc

Your first task. A touch receptor chatters at about 150 spikes per second all the time and fires faster, about 400 per second, during a poke. Wire it so the animal pulls back within 60 ms of every poke, yet stays still on chatter alone. Six pokes arrive at random times in 20 seconds. A "withdrawal" is a burst of motor spikes: at least 3 within 25 ms, or half the pool if it has more than 6 cells.

What is new:

- **Groups of neurons.** Select a neuron and change **neurons in group** (1, 2, 3, 4, 5, 6, 8, 10, 12, 14, 16, 20 and larger sizes). A group is drawn as one disc. A sensor wired to a group of ten gives ten receptor cells.
- **Noise rejection.** One cell listening to one afferent cannot pass at any weight, because noise spikes bunch up by chance. Averaging many independent inputs separates a real poke from the chatter.
- **Five worlds.** Check already runs five random worlds, and passing all five earns the second star. After a pass, the bar gains a **Test x5** button, which does the same on the full episode and shows its progress. See [stars, par and hints](stars-and-hints.html).
- **Challenge.** The third star asks for the same result plus the level's challenge, written on its card. For the reflex that is few enough neurons (the par) and a ceiling on the firing rate. Guided mode stops at two stars, so the third needs Standard or Researcher.

**Stuck?** Convergence again. Against noise, a motor cell that needs several inputs at once is steadier than one that needs a single input.

## What to do next

You have now used every basic control. Next:

1. Open the **Notebook** from the campaign map and read the entries you unlocked. See [the notebook](notebook.html).
2. Play the rest of the chapter: Inhibition and Subtraction, then the salt-seeking worm, then Adaptation, Tonic drive and the Half-centre, and the lamprey that swims. The wings (Bestiary and Theory) open when you pass the chapter.
3. After a pass, open **More** and try **Lesions**. Cut a connection and see what the circuit loses. See [the lesion lab](lesion-lab-and-analysis.html).
4. Read [how to read the lab](how-to-read-the-lab.html) for every panel, and [the core loop](core-loop.html) for the cycle you will repeat for the rest of the game.

> **Note:** Some tools stay hidden on purpose. Inhibitory neurons, the parts bin, analysis and load/save appear when the campaign reaches a level that needs them. The More menu says what is still to come. The [sandbox](sandbox.html) has everything unlocked.

![The lab on the Threshold level, with the circuit, scopes, and the brief](img/first-hour-lab.png)
