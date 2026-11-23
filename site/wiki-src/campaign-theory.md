---
title: The Theory wing
section: The campaign
order: 32
summary: Twelve levels in four chapters that turn a computational-neuroscience course into puzzles: the phase plane, rate models and mean field, attractor memory, tuning and decoding.
---

# The Theory wing

The Theory wing asks "what does the mathematics of a neuron say?" instead of "which body does it drive?". It has four chapters of three levels: **Dynamics**, **Rate models**, **Attractor memory** and **Tuning and decoding**. Each chapter starts with two small puzzles (one idea each) and ends with a larger scored task, like every other chapter.

The wing opens when you pass the Invertebrates spine (the Touch reflex, Salt-seeking worm and Swimming lamprey levels), like the [Bestiary](campaign-bestiary.html). Before that the Theory tab is locked and says "Pass the Invertebrates spine first". Unlike the Bestiary, the four Theory chapters have no further gate between them. Once the wing is open, all four chapters are open and you can start with any. It is not part of the body chapters' unlock chain, and nothing in it gates anything else. Inside a chapter, a level opens when the one before it is passed. You can start with Theory, with the body chapters, or jump between them.

Each level card also carries **Needs** chips: concepts the level uses that another level teaches first (the Wilson-Cowan levels use negative feedback, taught in the Spinal chapter). Each is marked met once you pass the level that teaches it. The chips are advice only and never lock a level. See [the core loop](core-loop.html) for how a level is built, run and checked, and [stars, par and the hint ladder](stars-and-hints.html) for scoring.

The usual honesty contract applies. The game checks the **mechanism** you built by reading the circuit's structure, not only the numbers it produces. A shortcut that hits the target numbers fails with a verdict that names the missing part. This page gives no wiring or values. The "Stuck?" notes go no further than the first hint tier, and the goals repeat only what the level's own brief says.

## What the wing adds to the lab

The Theory levels use the same [lab](how-to-read-the-lab.html) as the rest of the game, plus a few new pieces.

| New piece | Where you meet it | What it shows |
|---|---|---|
| Phase plane | World panel of the Dynamics levels | A picture of a two-variable cell: voltage on one axis, a "recovery variable" on the other, the two nullclines, the fixed points and the live path of the cell. |
| Rate units and rate strips | World panel of the Rate models levels | Scrolling traces of the readouts of rate units on one shared scale, one trace per output channel. |
| Siegert units and diffusion projections | Parts bin and the projection inspector in `r_meanfield` | A unit that stands for a whole population, driven by the mean and spread of its input; a connection that carries the weight J and the number of inputs K of the network it stands for. |
| Pattern grid | World panel of the Attractor memory levels | The cells the level reads, laid out on a grid and lit by their own spikes. Cells that a sensor reaches are ringed and named by the sensor. |
| Tuning dial, bars and curve | World panel of the Tuning levels | A dial for the stimulus shown, a bar per cell, a single cell's tuning curve, and a decoded-direction arrow. |
| Population-vector readout | Output kind of `t_decode` | A motor channel that turns a ring of cells into one angle. |
| STA tab | Analysis screen | Spike-triggered average: what the input looked like just before the cell fired. Reachable from the More menu once the campaign has reached the chapters that open analysis. |

Each is explained in the chapters. Parts and wiring options are in [the parts bin](parts-bin.html), [synapses and modulators](synapses-and-modulators.html) and [inputs and outputs](inputs-and-outputs.html).

### Honest caveats

The wing runs on real simulator models, but several things are simplified so you can finish a puzzle in minutes. The full account is in [what is real](what-is-real.html). The points that matter here:

- **The "Wilson-Cowan" units are not the original equations.** They are NEST's threshold-linear rate neurons with a ceiling, not the sigmoid-with-refractoriness of the 1972 paper. Rates are in arbitrary units, and the drive is added after the nonlinearity, so inhibition cannot cancel it.
- **The mean-field network is well behaved by design.** The level shows the method, not where it breaks down.
- **Tuning curves are given, not learned.** The afferents have a fixed bell-shaped rate curve, Poisson spikes and evenly spread preferred angles.
- **Memory patterns are groups of cells**, not the plus-one and minus-one vectors of the 1982 model. Persistent firing is far faster than real delay activity.
- **The "one neuron switch" fires much faster than a real persistent-firing cell.** The level says so.
- **Weights, time constants, windows and pass bands are tuned for the game.** They are properties of the game, not measurements.

## Levels at a glance

None of the twelve levels has challenge variants or an energy clause. Par is a neuron count. Stars follow the usual rules. One star for a pass. Two when CHECK passes all five worlds (seeds 101 to 105) in Standard or Researcher mode. Three when the level's visible challenge is also met (here: stay at or under par) and, in Standard, the recipe was not opened. Guided mode stops at two stars. A Researcher pass of all five worlds under physiological noise also earns the Researcher seal. See [stars, par and the hint ladder](stars-and-hints.html).

| Chapter | Level | Kind | Par |
|---|---|---|---|
| Dynamics | Where firing starts (`d_class`) | micro-puzzle | 1 |
| Dynamics | Only the change counts (`d_phasic`) | micro-puzzle | 1 |
| Dynamics | A neuron that remembers a pulse (`d_switch`) | task | 1 |
| Rate models | Two populations that make a rhythm (`r_oscillate`) | micro-puzzle | 2 |
| Rate models | Two populations, one winner (`r_choose`) | micro-puzzle | 2 |
| Rate models | The network and its mean field (`r_meanfield`) | task | 255 |
| Attractor memory | Recalling a whole from a part (`h_complete`) | micro-puzzle | 8 |
| Attractor memory | Two memories, no blends (`h_two`) | micro-puzzle | 8 |
| Attractor memory | Writing a memory with plasticity (`h_learn`) | task | 8 |
| Tuning and decoding | A cell tuned by its inputs (`t_tuned`) | micro-puzzle | 9 |
| Tuning and decoding | Sharpening a tuning curve (`t_sharpen`) | micro-puzzle | 16 |
| Tuning and decoding | Reading a direction from a crowd (`t_decode`) | task | 24 |

---

# Chapter 1: Dynamics

![The phase plane of a one-cell level: two nullclines, a fixed point and the path of the cell](img/theory-dynamics.png)

One neuron seen as a moving point. A cell like these has two variables: its **voltage** and a slower **recovery variable** (a stand-in for the slow currents that pull the voltage back). A picture with voltage on one axis and the recovery variable on the other is a **phase plane**. Every moment of the cell is a point on it, and the cell's life is a path.

Two curves on the plane matter. A **nullcline** is the set of points where one of the two variables is not changing at that instant. The voltage nullcline and the recovery nullcline cross at a **fixed point**: a state where nothing changes. A cell left alone at a stable fixed point stays there. That is the resting state. A steady input current moves the voltage nullcline, and the fixed point moves with it, or disappears.

In the lab the phase plane is drawn for the first cell the level's motor channel reads, using the cell's own parameters and the constant current it gets right now. Stable fixed points are filled, unstable ones hollow, saddles crosses. The reset (where a spike sends the cell) is marked, and the cell's live path runs over the picture. It is computed for a constant current, so it ignores synaptic input.

> **Note:** The phase plane works for the two-variable cells: the Izhikevich cell and the adaptive exponential cells. A level may still be scored with another model, but then it has no picture and the mechanism check says the cell needs two variables. The default cell of the Dynamics levels is the Izhikevich cell. See [the parts bin](parts-bin.html).

| Level | What it teaches | Know first | The real science | Citation |
|---|---|---|---|---|
| Where firing starts | Excitability class: how firing begins as current grows | Threshold and rheobase | Hodgkin found two kinds of axon. Class 1 axons fire at any low rate just above threshold; class 2 axons start at a clear rate. In a two-variable model the difference is how the resting state is lost. | Hodgkin 1948, J Physiol 107:165, doi:10.1113/jphysiol.1948.sp004260; Izhikevich 2003, IEEE Trans Neural Netw 14:1569, doi:10.1109/TNN.2003.820440 |
| Only the change counts | A stable fixed point that survives the input, so firing stops | Excitability class | Some auditory brainstem neurons with low-threshold potassium currents fire at the onset of a step and are then silent: they report change, not level. | Izhikevich 2003; Rothman and Manis 2003, J Neurophysiol 89:3097, doi:10.1152/jn.00127.2002 |
| A neuron that remembers a pulse | Bistability: rest and firing both stable at once | Excitability class, stable fixed point | A model cell can have a stable rest and a stable firing state side by side; a brief pulse that crosses the border flips it. Real persistent firing is a different, channel-based mechanism. | Izhikevich 2003; Touboul and Brette 2008, Biol Cybern 99:319, doi:10.1007/s00422-008-0267-4 |

## Where firing starts (`d_class`)

**The idea.** Raise the current into a neuron step by step and watch where firing begins. A **class I** cell starts slowly, from almost no spikes, and speeds up smoothly as the current grows. A **class II** cell starts at once, at a clear rate, as if a switch had been thrown.

The phase plane says why. As the current grows, the voltage nullcline rises and the resting state, the crossing of the nullclines, is lost. If the resting state runs into a second crossing and the two annihilate, the cell can only creep through the bottleneck that is left: class I. If the resting state turns into a **spiral** (a state the cell circles into, like water down a drain) and the current makes that spiral unstable, the cell starts firing at the frequency of that spiral: class II. In the maths this is the difference between a saddle-node and a Hopf onset. You do not need the word "saddle-node" to play. "The resting state vanishes by collision" and "the resting state spirals out" are enough.

**The goal.** The sensor `current` steps up in 21 stairs of 200 ms through the cell's own onset current. The staircase is scaled to the cell, so a big weight cannot make a slow starter look abrupt. Build one cell, read on `spikes`, that is silent at the bottom and whose firing starts at a clear rate, not from zero.

**What the pass rule asks.** The three lowest steps must be silent. Some step must fire. The busiest of the first steps that spike must show a clear burst (three spikes in the last 140 ms of a step in Standard, two in Guided, four in Researcher). The top step must still fire. The structure check also computes the class for your cell from the slopes of its two equations at rest. The class is not looked up from the cell's name, so a cell with other parameters can land on either side.

**What is simplified.** The two classes shade into each other in these models. The default regular-spiking cell sits near the point where they meet, so the level uses a game criterion: class II is decided by the resonance of the resting state just before it is lost (a damped oscillation of about 15 Hz or more). The game says so rather than pretend there is a sharp line.

**Par:** 1 neuron.

### Stuck?

How firing starts depends on how the resting state is lost: through a collision of two fixed points (a slow start from zero) or as a spiral that turns unstable (an abrupt start).

## Only the change counts (`d_phasic`)

**The idea.** Some neurons answer a step of current with one spike or a short burst, then fall silent although the current stays on. They report the change, not the level. A step of current only moves the voltage nullcline. A **tonic** cell has no stable crossing left afterwards, so it circles for as long as the current is on. A **phasic** cell still has a stable crossing. The step throws the voltage across threshold once, then the path spirals into the new resting point and stays there.

**The goal.** The sensor `current` is zero, steps on for 1.2 seconds (400 to 1600 ms), and goes off again. Build a cell that is silent before the step, fires when the step comes, and is silent again well before the step ends. The step size is the weight you give the sensor, and it moves the nullcline as much as any cell parameter does.

**What the pass rule asks.** No spike in the quiet window before the step. At least one spike in the 250 ms after the step starts. No spike in the late window (900 to 1600 ms) while the current is still on. The mechanism check also asks that the cell can settle at the held current.

**What is simplified.** A generic two-variable cell, not an auditory neuron. The real brainstem mechanism involves a particular potassium current. The cell here shows only the geometry.

**Par:** 1 neuron.

### Stuck?

A step of current only moves the voltage nullcline. Whether the cell keeps firing depends on whether the nullclines still cross at a stable point afterwards.

## A neuron that remembers a pulse (`d_switch`)

**The idea.** A neuron can have two stable states at once: quiet at rest, and firing on and on. In the phase plane the resting point is a stable crossing, and the firing is a loop around it that the path can also settle into. A border runs between the two. A brief pulse that carries the cell across the border flips the state for good. This is **bistability**: one cell, one current, two stable states, so the cell stores one bit.

**The goal.** Three sensors reach the cell. `hold` is a steady current that puts the cell near the edge of firing without starting it. `set` is a short pulse that pushes the cell up, and `reset` is a short pulse that pushes it down. Both pulses take a positive weight. The sensor itself carries the sign. Build a single cell that is quiet at first, fires on and on after `set` (long after the pulse is gone), and is quiet again and stays so after `reset`. The same sequence must work a second time. What makes firing survive after a spike is where the spike resets the cell, relative to the border between the states.

**What the pass rule asks.** Quiet before the first pulse. At least six spikes in each 400 ms "on" window (four in Guided, eight in Researcher), long after the pulse. No spike in each "off" window. The on state must stay at or below 80 spikes in 400 ms (200 Hz; 100 in Guided, 70 in Researcher). The mechanism check reads the cell's own phase plane. The behaviour decides the rest: only a real second, firing state makes the "on" windows pass.

**What is simplified.** Real persistent firing of single cells runs at 5 to 50 spikes per second and relies on channel-based plateau potentials, which are not modelled. The firing state of this kind of cell is a limit cycle born at a finite rate, about 135 spikes per second at the very lowest, so the 200 Hz cap is as low as the model allows. The level shows the mechanism, not the rate.

**Par:** 1 neuron.

### Stuck?

The same current can leave a neuron with two stable states: rest and repetitive firing. A pulse that crosses the border between them flips the state, and it stays flipped.

---

# Chapter 2: Rate models

![Two rate units: excitatory and inhibitory rates swinging against each other on a shared scale](img/theory-rates.png)

A whole population of neurons becomes **one number: its firing rate**. This is a drastic and old simplification. It trades spikes for something you can analyse with a pencil.

A **rate unit** has a time constant (how fast it follows its input), a gain, a threshold and a ceiling. A connection between rate units is a weight. A negative weight is inhibition. The game's units are NEST's threshold-linear rate neurons. Their rate follows `X' = (-X + drive + clip(sum of weight x rate)) / tau`: the drive is added after the nonlinearity. So a rate can read a little above the ceiling, and inhibition cannot cancel the drive.

Rate readouts appear as scrolling traces on one shared scale. A rate unit's readout is in arbitrary units. A spiking pool's readout is in spikes per second. See [inputs and outputs](inputs-and-outputs.html) for the `rate_drive` input and `rate_value` output kinds and [synapses and modulators](synapses-and-modulators.html) for rate connections.

| Level | What it teaches | Know first | The real science | Citation |
|---|---|---|---|---|
| Two populations that make a rhythm | The excitatory-inhibitory pair that oscillates | Negative feedback | Wilson and Cowan wrote equations for coupled excitatory and inhibitory populations. A pair can sit steady, jump between rates, or oscillate, and the oscillation speeds up with stronger stimulus. | Wilson and Cowan 1972, Biophys J 12:1, doi:10.1016/S0006-3495(72)86068-5 |
| Two populations, one winner | Competition between populations that holds its winner | Command switch, the Wilson-Cowan pair | Populations that excite themselves and inhibit each other turn a small input difference into a large output difference. The same wiring has been proposed for decisions, attention and working memory. | Wilson and Cowan 1972 |
| The network and its mean field | Predicting a spiking network's rate from the mean and spread of its input | The Wilson-Cowan pair, inhibition | A population of integrate-and-fire cells with many weak inputs has a rate fixed by the mean and variance of its input. Solving the self-consistent equations predicts the network. | Amit and Brunel 1997, Cereb Cortex 7:237, doi:10.1093/cercor/7.3.237; Hahne et al. 2017, Front Neuroinform 11:34, doi:10.3389/fninf.2017.00034 |

## Two populations that make a rhythm (`r_oscillate`)

**The idea.** The **Wilson-Cowan pair** is an excitatory population that excites itself and an inhibitory population that it excites and that inhibits it back. Excitation builds and drives the inhibition. The inhibition shuts the excitation down. Then the inhibition fades and excitation builds again. Two numbers make an oscillation.

**The goal.** The sensor `drive` is zero, then switches on. Build two rate units: `e` reads the excitatory one and `i` the inhibitory one. The excitatory rate must stay quiet before the drive, then swing up and down again and again. The inhibitory rate must follow it a little later in each cycle.

Two things must be right. The excitation must be able to run away by itself (its self-connection must be strong enough compared with how fast the unit follows its input). And the loop through the inhibitory unit must be strong enough to catch it but not so strong that it holds the excitation down for good. The verdict tells you which side of that narrow band you are on. A rate stuck near its ceiling means too little inhibition. A rate pinned just above the drive level means too much. A steady value in between means the excitation does not run away strongly enough.

**What the pass rule asks.** `e` is quiet before the drive. After it, `e` swings by a clear amount, for at least four cycles (three in Guided) in the last stretch of the run. One cycle lasts between 40 and 400 ms. `i` follows `e` by between 5 and 70 percent of a cycle, so the inhibition causes the turn-around and is not a copy. The structure check also reads the circuit: `e` and `i` must be different units.

**What is simplified.** The units are threshold-linear, not the original sigmoid, so the band in which oscillation exists is narrow.

**Par:** 2 units.

### Stuck?

Excitation that excites itself, plus inhibition that it drives and that inhibits it back, makes a rhythm: it overshoots, is shut down, recovers and overshoots again.

## Two populations, one winner (`r_choose`)

**The idea.** Two populations compete for the same job, say two possible answers to an ambiguous stimulus. Each is driven by its own evidence (`a_in`, `b_in`), each excites itself and each inhibits the other. A small advantage in the drive should become a large advantage in the output: one wins and the other is silenced. The winner then holds itself up, so the choice outlasts the evidence.

**The goal.** Build two rate units, `a` and `b`. A gets a little more drive than B, then the two are equal, then B gets much more, then the two are equal again. A small difference must be amplified. The choice must be held when the inputs are equal. A much stronger input for the other side must overturn it. The new winner must be held in turn. The memory is in the loop, so it works whichever side won last. A circuit that simply gives one input more gain holds only its favourite and fails the last phase.

**What the pass rule asks.** At the end of each phase the leading unit must beat the other by a clear margin: A wins phase one, A still leads in phase two with equal inputs, B leads in phase three, and B still leads in phase four. The structure check also reads the circuit, so a circuit that only biases one input fails.

**What is simplified.** Rates are in arbitrary units and the units are threshold-linear. The "winner-take-all" reading is the textbook one for decision and attention models.

**Par:** 2 units.

### Stuck?

Self-excitation plus mutual inhibition between two populations turns a small difference in drive into a large difference in rate, and holds it.

## The network and its mean field (`r_meanfield`)

**The idea.** A large, sparse network of excitatory and inhibitory integrate-and-fire cells, driven by a lot of independent Poisson input, fires **asynchronously** (the cells are not in step). Every cell sees the sum of many small inputs, so the total input is nearly Gaussian. Its mean grows with `J K r` and its variance with `J^2 K r`, where J is the size of one input in millivolts, K is how many inputs each cell has, and r is the rate of the cells that send them. The **Siegert formula** gives the rate of an integrate-and-fire cell driven by such an input. A rate unit that applies it, a **siegert unit**, can stand for a whole population. The network's rates then follow from a few self-consistent equations, with no spikes needed. That is **mean-field theory**.

**The goal.** Build both: the spiking network (excitatory and inhibitory cells, random sparse connections, the Poisson drive `ext`) and its prediction (a siegert unit for each population and one for the drive, with projections that carry the same J and K, driven by `ext_mf` at the same rate). `net` reads the mean rate of the excitatory cells and `mf` the prediction. At three drive levels the two must agree. Where they do not, the two descriptions differ in J, K, the sign of the inhibition, or the drive.

In the projection inspector, a "mean-field projection" takes J and K as two separate numbers. NEST itself only has the derived drift and diffusion factors. The game computes them for you.

**What the pass rule asks.** At the highest drive both the network and the prediction fire (at least 5 Hz). The network's rate rises at each drive level. Each prediction projection carries the J and K of the spiking projection it stands for (matching rates with a made-up K is rejected). At all three levels the two rates differ by at most 15 percent of the larger plus 1.5 Hz (25 percent in Guided, 10 percent in Researcher).

**What is simplified.** The level does not probe where the mean field fails. Its network is large enough for the prediction to hold. Drive levels and measurement windows are game values. Par counts all the spiking cells plus the units of the prediction.

**Par:** 255 cells and units (the reference has 253; 2 spare).

### Stuck?

Many weak, independent inputs make a cell's input Gaussian. Its mean and variance, summed over the inputs, are all the cell's rate depends on. A siegert unit computes that rate.

---

# Chapter 3: Attractor memory

![A grid of eight cells, half of them ringed as the cue, lighting up as a stored pattern is recalled](img/theory-memory.png)

An **attractor** is a state a system falls into and stays in. **Hopfield's idea** (1982): a memory can be an attractor of a network. If cells excite each other **symmetrically**, meaning the weight from A to B equals the weight from B to A, the network has stable states, and a pattern stored in the weights is one of them. Show the network part of the pattern and it falls into the whole. This is a **content-addressable** memory: you recall by content, not by address.

In these levels a pattern is a **group of cells**, not a vector of plus and minus ones. The lab draws the cells the level reads on a grid, each lit by its own spikes. Cells a sensor reaches are ringed in amber and named by the sensor: the cue is the part that is touched, and the pattern is the whole.

Symmetry is checked on the actual list of connections between cells, one by one. A directed random graph is not called symmetric. An outer-product wiring inside one population is accepted too.

| Level | What it teaches | Know first | The real science | Citation |
|---|---|---|---|---|
| Recalling a whole from a part | An attractor completes a partial cue and holds it | Persistence, excitation | Hopfield showed that symmetric networks have stable states that can be made to be stored patterns. In spiking networks the same states are persistent, selective activity, a model of delay activity in cortex. | Hopfield 1982, Proc Natl Acad Sci USA 79:2554, doi:10.1073/pnas.79.8.2554; Amit and Brunel 1997, doi:10.1093/cercor/7.3.237 |
| Two memories, no blends | Inhibition between patterns avoids spurious mixtures | Attractor memory, command switch | A network storing several patterns can fall into mixtures nobody stored. Competition between the groups makes the pure patterns win. | Hopfield 1982 |
| Writing a memory with plasticity | Spike-timing-dependent plasticity as Hebbian storage | Attractor memory | A presynaptic spike just before a postsynaptic one strengthens the connection; the reverse order weakens it. Showing a pattern writes it into the weights. | Hopfield 1982; Bi and Poo 1998, J Neurosci 18:10464, doi:10.1523/JNEUROSCI.18-24-10464.1998; Song, Miller and Abbott 2000, Nat Neurosci 3:919, doi:10.1038/78829 |

## Recalling a whole from a part (`h_complete`)

**The idea.** If a group of cells excite each other symmetrically, each weight equal to the weight back, the group has a stable state in which all of them fire. A few cells nudged into that state pull the rest after them, and the pattern stays after the cue is gone.

**The goal.** Build a pattern of several cells that is silent by itself. `memory` reads the whole pattern. `cue` reaches only part of it, for a moment (a 200 Hz Poisson burst from 300 to 600 ms). After the cue the whole pattern must fire, including the cells the cue never touched, and keep firing. You can build the group from two groups (the cued cells and the rest) that excite each other and themselves.

**What the pass rule asks.** At most 2 spikes in the pattern before the cue. In the recall window (900 to 1700 ms, long after the cue) at least 80 percent of the pattern's cells fire at 5 Hz or more on average, and at least 80 percent of the cells the cue never reached fire. The structure check also reads the circuit: the connections must be roughly symmetric, and the cue must reach some but not all of the pattern.

**What is simplified.** The sustained rate is far above real delay activity (there is no balanced inhibition). The pattern is a group of cells, not a plus-and-minus vector.

**Par:** 8 cells.

### Stuck?

A group of cells that excite each other symmetrically has a stable state in which all of them fire. A part of the group, nudged into it, pulls the rest along.

## Two memories, no blends (`h_two`)

**The idea.** Store two patterns in one network. Each pattern is a group of cells that excite each other. If nothing separated the groups, the network could fall into a blend of both: a **spurious state**, a memory nobody stored. Inhibition between the patterns, as symmetric as the excitation inside them, makes each pattern a state the network can fall into on its own, and a blend not one.

**The goal.** `cue_a` calls up pattern A and `cue_b` pattern B. `a` and `b` read each pattern's cells. After `cue_a` only A must be lit. After `cue_b` only B, overwriting A. When both cues come at once the network must pick exactly one pattern, not both. A pattern's cells must stay lit after their cue is gone.

**What the pass rule asks.** Quiet before the first cue. After `cue_a` at least 80 percent of A's cells fire and B stays silent. After `cue_b` the same for B. After both cues together exactly one pattern is lit, the other silent. The structure check also reads the circuit's connection list, including whether the connections are symmetric.

**What is simplified.** The two patterns are disjoint groups, not overlapping vectors. The inhibition between them is built in, not the result of an outer-product rule.

**Par:** 8 cells.

### Stuck?

With symmetric excitation inside each pattern and symmetric inhibition between them, the pure patterns are the states the network falls into. A blend

## Writing a memory with plasticity (`h_learn`)

**The idea.** So far the pattern was built in. In a brain, experience writes it. **Spike-timing-dependent plasticity (STDP)** makes a connection grow when its presynaptic cell fires just before its postsynaptic one, and shrink in the other order. Cells that fire together wire together.

**The goal.** Make the connections among a group of cells plastic and start them weak. First, `cue` reaches part of the group and recalls nothing. Then `show` drives the whole pattern for a while, so its cells fire together and the connections among them strengthen. Then the same `cue` must recall the whole pattern, and it must stay lit after the cue. The connections must really have grown, and must start symmetric. Too strong at the start and the cue completes the pattern before it was learned. Too weak or too slow and nothing is learned. The synapse has a learning rate, an asymmetry between strengthening and weakening, and a ceiling for the weight. These are in the projection's inspector: see [synapses and modulators](synapses-and-modulators.html).

**What the pass rule asks.** Before the exposure the cue completes nothing (at most 3 spikes in the cells it never reached). The mean weight of the plastic connections has grown at least 1.5 times. After the exposure at least 80 percent of the cells fire at 5 Hz or more, including at least 80 percent of the cells the cue never reached. The structure check also reads the circuit (plastic, symmetric at the start). The behaviour decides whether the exposure is strong or long enough.

**What is simplified.** One second of exposure stands for minutes of learning. The plastic synapse is NEST's `stdp_synapse`, which is excitatory only, so a bigger network's inhibition cannot be learned.

**Par:** 8 cells.

### Stuck?

Spike-timing-dependent plasticity strengthens connections between cells that fire together, so showing a pattern writes it into the weights.

---

# Chapter 4: Tuning and decoding

![A dial of orientations, a ring of cells with a bar each, and the decoded direction as an arrow](img/theory-tuning.png)

Many sensory neurons answer best to one value of a stimulus: an orientation, a direction of movement, a pitch. A plot of response against the stimulus value is the cell's **tuning curve**, and the value at its peak is the cell's **preferred** value.

An input of the kind `tuned_poisson` drives the afferent cells of these levels. The task sets a stimulus angle, and each cell the input reaches fires with Poisson spikes at a rate that depends on how far its preferred angle is from the stimulus. The curve is bell-shaped, a **von Mises** function: the circular cousin of the Gaussian bell, with a base rate, a peak rate and a width. The preferred angles are spread evenly in the order the cells were made: cell `k` of `N` prefers `180 k / N` degrees for an orientation (which repeats every 180 degrees) or `360 k / N` degrees for a direction.

In the lab a dial shows the stimulus, a bar per cell shows the response, and a single cell's tuning curve fills in as the orientations come round. For `t_decode` the decoded direction appears as an arrow. See [inputs and outputs](inputs-and-outputs.html) for the `tuned_poisson` input and the population-vector output.

| Level | What it teaches | Know first | The real science | Citation |
|---|---|---|---|---|
| A cell tuned by its inputs | Tuning from convergence | Convergence | Hubel and Wiesel found cat cortical cells that fire for a bar at one orientation and proposed that they receive input from cells whose receptive fields lie along a line. Whether real tuning is feed-forward, recurrent or both is debated. | Hubel and Wiesel 1962, J Physiol 160:106, doi:10.1113/jphysiol.1962.sp006837 |
| Sharpening a tuning curve | Narrowing tuning with inhibition among cells | Tuning curve, inhibition | In recurrent models, inhibition among cells of different preferences makes the response tuning much sharper than the input's. Evidence for and against has accumulated for decades. | Ben-Yishai, Bar-Or and Sompolinsky 1995, Proc Natl Acad Sci USA 92:3844, doi:10.1073/pnas.92.9.3844 |
| Reading a direction from a crowd | The population vector | Tuning curve | Georgopoulos and colleagues found monkey motor-cortex neurons broadly tuned to arm-movement direction. One cell predicts the direction badly; the weighted sum of preferred directions points the right way. | Georgopoulos, Schwartz and Kettner 1986, Science 233:1416, doi:10.1126/science.3749885 |

## A cell tuned by its inputs (`t_tuned`)

**The idea.** A cell that listens to the right inputs inherits a preference. This is the Hubel-Wiesel picture of an orientation-selective cell built from aligned inputs.

**The goal.** The sensor `stim` is the orientation of a bar, from 0 to 180 degrees, shown for a few hundred milliseconds in turn (eight trials of 300 ms: 0, 45, 90, 135, then the same again). Create a population of afferent cells (parrot cells, which just repeat the spikes of their input) and connect `stim` to it. Then build one more cell, `resp`, that listens to some afferents and not others, so it responds to 90 degrees much more than to 0, and more than to 45 and 135. Which afferents it listens to, and how strongly, is the whole puzzle. A cell that weighs them all alike has no preference. The afferents can be separate populations, taken in the order you create them.

**What the pass rule asks.** At 90 degrees the cell fires at 12 Hz or more (8 in Guided). Its rate at 90 is at least three times its rate at 0, and at least 1.4 times its rate at 45 and at 135, so it is not just "anything near 90". The structure check also reads the circuit: weights decide the preference, not the number of connections.

**What is simplified.** The tuning curves are given: a game-chosen width, peak and base, Poisson spikes, evenly spaced preferred angles. Real afferents are neither evenly spaced nor Poisson. The level shows that convergence can work, not that the cortex does it this way.

**Par:** 9 cells.

### Stuck?

A cell inherits the preference of the inputs it listens to: connect

## Sharpening a tuning curve (`t_sharpen`)

**The idea.** Afferents with broad tuning curves feed a population of cells, one cell to one afferent, and the cells inherit the broad tuning. Inhibition among the cells can narrow it: the cell that prefers the stimulus pulls ahead and pushes its neighbours down.

**The goal.** `stim` shows an orientation from 0 to 180 degrees. Make eight afferent cells and connect `stim` to them. Make eight cells that each listen to their own afferent, using the "neuron k to k" segment wiring (neuron k of one group to neuron k of the next). `resp` reads the eight cells. The cell that prefers the orientation shown must fire well, and the cells that prefer 45 degrees away must fire at only a small fraction of that. Without inhibition among the cells the tuning is as broad as the afferents'. Self-excitation holds a cell's lead against the others. It is optional, and how strong each is decides how much it sharpens.

**What the pass rule asks.** The cells fire at 10 Hz or more at their preferred orientation (7 in Guided), and the side rate, the cells 45 degrees away, is at most 58 percent of the peak rate (62 in Guided, 52 in Researcher). A ratio of two small spike counts is noisy, so the episode is long: 24 trials of 300 ms, read over 48 trial responses. The structure check also reads the circuit.

**What is simplified.** Whether inhibition sharpens real cortical tuning is debated. The level shows the mechanism working. Self-excitation is optional, as the brief says.

**Par:** 16 cells.

### Stuck?

Inhibition among cells with different preferences narrows the tuning of each: the cell with the strongest input wins and pushes the others down.

## Reading a direction from a crowd (`t_decode`)

**The idea.** No single cell tells the direction of a stimulus: each is broadly tuned and noisy. A crowd does. Weight every cell's preferred direction by how much it fires and add them up. The **population vector** points where the stimulus is.

**The goal.** The sensor `stim` shows a direction from 0 to 360 degrees, a different one in each of six 400 ms trials. Make twelve afferent cells and connect `stim` to them. Cell k of twelve prefers 30 k degrees. Then make a population of cells that listen to the afferents, and let `heading` read it as a ring. `heading` takes the cells in listed order and puts cell k of N at 360 k / N degrees, with cell 0 at the top, so list the cells in the order of their preferred directions. The decoded direction must be near the shown one, trial after trial. If the cells are listed in the wrong order, or each listens to the wrong afferent, the arrow points elsewhere. The ring readout is described in [inputs and outputs](inputs-and-outputs.html).

**What the pass rule asks.** The readout fires (at least 10 spikes per trial on average). The mean absolute error between decoded and shown direction over the six trials is at most 25 degrees (35 in Guided, 18 in Researcher). No single trial is more than 60 degrees off (80 in Guided). The shown directions are random, a different draw for each seed, so a lucky guess does not repeat. The structure check also reads the circuit.

**What is simplified.** The population vector is one linear decoder, not the best one: for Poisson cells with bell-shaped tuning a maximum-likelihood readout is more accurate. The tuning curves are given.

**Par:** 24 cells.

### Stuck?

One cell is broadly tuned and noisy. The sum of all the cells' preferred directions, weighted by their firing, points at the stimulus.

---

# The spike-triggered average tab

The wing adds a seventh tab to the Analysis screen, **STA**. It belongs with tuning: instead of asking which stimulus a cell prefers, it asks which input pattern makes a cell fire. This is **reverse correlation**.

Choose a population and press **Record**. A Gaussian white-noise current (random, with no pattern, seeded from the circuit's seed so a run repeats) drives the population's first cell for the length you choose, at most 20 seconds. 10 seconds is usually plenty. Each time the cell fires, the program takes the stretch of current before the spike. The average of all those stretches, minus the average of the whole current, is the **spike-triggered average**, drawn as a deviation from the mean input in picoamperes.

Reading it: a bump just before time zero means the cell responds to a brief rise in current. A bump followed by a dip means it responds to change, like the phasic cell of [Only the change counts](#only-the-change-counts-d_phasic). A grey band shows what chance alone gives with that many spikes, so a lobe inside the band is not a filter. If the cell fires fewer than five spikes under the noise, the tab says "too few spikes" instead of drawing noise. The mean and spread of the noise current come from a per-model table chosen near each cell's threshold. For the other analysis tabs see [the lesion lab and analysis tools](lesion-lab-and-analysis.html).

---

# Related pages

- [The lab](how-to-read-the-lab.html): where the new views sit.
- [The core loop](core-loop.html) and [stars, par and the hint ladder](stars-and-hints.html): build, check, verdicts and the stars.
- [The parts bin](parts-bin.html): the Izhikevich and adaptive cells, the rate units and the siegert unit.
- [Inputs and outputs](inputs-and-outputs.html): `rate_drive`, `rate_value`, `tuned_poisson` and the population-vector readout.
- [Glossary](glossary.html): every term used above, one line each.
- [What is real](what-is-real.html): the honest account of what is NEST, tuned and illustrative.
- [The Bestiary](campaign-bestiary.html) and [the body chapters](campaign-body.html): the other two wings.
