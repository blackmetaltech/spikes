---
title: The Bestiary
section: The campaign
order: 31
summary: Six model animals, 18 levels and 7 diagnosis cases: the worm, the leech, the sea slug, the fly compass, the fly mushroom body and the crab, with the biology and the honest limits of each.
---

# The Bestiary

The Bestiary is the second wing of the campaign. The [body chapters](campaign-body.html) build organs: a heart, lungs, a pupil. The Bestiary builds the circuits that neuroscience knows best, one model animal per chapter. Each animal is a small, well-studied nervous system, and each chapter takes one famous circuit from it.

There are six chapters.

| Chapter | Animal | The circuit | Levels | Diagnosis cases |
|---|---|---|---|---|
| Worm | *C. elegans*, a roundworm | The touch circuit: back up or go forward | 3 | 1 |
| Leech | The medicinal leech | The heartbeat timer | 3 | 1 |
| Sea slug | *Aplysia*, a sea slug | The gill-withdrawal reflex and its two kinds of learning | 3 | 1 |
| Fly compass | *Drosophila*, the fruit fly | The heading ring in the central complex | 3 | 1 |
| Fly mushroom body | *Drosophila*, the fruit fly | Smell, sparse codes and learned avoidance | 3 | 2 |
| Crab | A crab's stomach nerve ring | The pyloric rhythm | 3 | 1 |

That is 18 levels and 7 diagnosis cases. Each chapter has three levels in play order: two short puzzles that each teach one idea, then one scored task that puts the ideas together. The diagnosis cases are optional and hang off the task of their chapter.

## How the wing works

**Open from the start.** Every Bestiary chapter is open as soon as you start. Passing the Bestiary never unlocks anything in the body chapters, and the body chapters never block the Bestiary. You can start here if you like. If you have never opened the game, read [Your first hour](first-hour.html) first, because it explains the lab itself.

**Inside a chapter, levels open one after another.** The second level opens when you pass the first, and the third when you pass the second. A diagnosis case opens when you pass the task it hangs off, which is the third level of its chapter (both mushroom-body cases hang off `fly_mb`).

**Same loop as everywhere.** You build on the bench, run, press Check, read the verdict and the score, and earn stars. If you are not sure how that works, see [The core loop](core-loop.html), [Stars, par and the hint ladder](stars-and-hints.html) and [Modes](modes.html).

**Checked by mechanism.** Every level checks the mechanism it teaches through the structure of your circuit, not only through the numbers it produces. A circuit that gets the right output by a trick (a cell wired straight to the motor, say) fails with a verdict that names the missing part and never gives a number. Hints work the same way: tier 1 names the idea, tier 2 points at the place, tier 3 gives a partial recipe.

**All lab tools are open.** The lab hides some tools (inhibitory cells, the parts bin, noise inputs, analysis, load and save) until the campaign reaches a level that needs them. The Bestiary comes after the body chapters in play order, so in this wing those tools are already on offer. Several levels need a cell with a slow internal variable (a burster, a rebounder, a conductance-based cell) or a synapse that changes with use. Choosing the right part from the [parts bin](parts-bin.html) and the [synapse editor](synapses-and-modulators.html) is part of the puzzle.

**No challenge variants.** The base levels of the body chapters have optional variants (for example, half the neurons). None of the Bestiary levels has one.

**Stars.** One star for a pass. Two stars for a pass on all five test seeds (the Test x5 button). Three stars for the same within par (a neuron budget) and within an energy budget. Par is listed for every level below. In Standard mode, opening the recipe caps the level at two stars. Diagnosis cases are scored differently; see [the diagnosis section](#diagnosis-cases-in-this-wing) below.

## What is real in this wing

The Bestiary is the part of the game that looks most like real neuroscience, so it is also where honesty matters most. This is the short version. The full table is in [What is real](what-is-real.html).

- **The cells are spiking model cells.** The worm and the leech are the biggest approximation. Most neurons of the worm's touch circuit have no sodium action potentials; they pass signals as smooth voltage changes. The leech heart interneurons inhibit each other partly through graded, voltage-dependent transmitter release. NEST, the simulator inside the game, has no graded synapse. So these levels use spiking cells and spike-triggered synapses everywhere. The sea slug's sensory and motor neurons do spike, so that chapter has no such problem.
- **Time is compressed.** The leech cycle is about two seconds so that a check takes a second; the real cycle is much slower. The sea slug habituates in seconds, where a real animal needs minutes. The compass and the crab run in real time.
- **The worm wiring is real; the signs are not.** The touch circuit in `worm_touch` is wired from published synapse counts (Cook et al. 2019). The data have no sign, so every chemical synapse is assumed excitatory, and the mutual inhibition between the forward and backward commands is added by hand.
- **The fly circuits follow cell types, not synapses.** The compass and the mushroom body are models after the published cell types (Turner-Evans, Hulse and others), not synapse-level data. Cell counts, weights and time scales are game values.
- **The crab cell is a re-fit, not a published model.** The `stg_cell` is written in NESTML. Its current types follow the stomatogastric literature, but the kinetic forms were written from memory of the papers and the conductances are game values. It is not validated against recordings and it is not the published parameter set.
- **Every weight is tuned.** Weights, delays, pass bands and world timings are game values, chosen until the reference circuit passes.
- **The diagnosis patients are fictional.** Each is a model circuit with one dead connection. See [Diagnosis cases](diagnosis-cases.html).

Each level lists below what it simplifies. The notebook entry you earn on a first pass says it again, with citations.

## Chapter: Worm

*C. elegans* is a one-millimetre roundworm. Its whole nervous system has been mapped cell by cell, and its touch response was worked out by killing single cells with a laser. Touch its head and it backs up. Touch its tail and it goes forward. The chapter builds that decision: a memory that holds a reversal, a switch that picks one direction, and then the real circuit.

![The worm chapter plate: the worm, its touch cells and its command cells](img/bestiary-worm.png)

| Level | Kind | What it teaches | Know first | The real biology | Citation |
|---|---|---|---|---|---|
| Worm: holding a reversal (`w_latch`) | Puzzle | Persistence: positive feedback holds a state | Excitation, inhibition | A brief head touch makes the worm back up, and the backing continues after the touch. The backward command cells keep each other active. | Chalfie et al. 1985, J Neurosci 5:956 |
| Worm: forward or backward (`w_switch`) | Puzzle | The command switch: two commands that silence each other | Inhibition | Forward and backward crawling run on two sets of command interneurons. The exclusion between them is a property of how they antagonise each other, not visible in a wiring diagram. | Chalfie et al. 1985, J Neurosci 5:956 |
| Worm: touch, reverse or go (`worm_touch`) | Task | Both ideas together, in the real wiring | Persistence, command switch, excitation, inhibition | Six touch cells, five pairs of interneurons and 69 motor neurons. Head touch reverses through AVD, tail touch goes forward through PVC. | Chalfie et al. 1985; Cook et al. 2019 |
| Diagnosis: the worm that cannot back up (`dx_worm`) | Case | Finding a dead connection by watching who fires | Command switch, persistence | Chalfie's method: kill a cell, see which response goes. | Chalfie et al. 1985 |

### Worm: holding a reversal

**The idea.** A memory in a circuit is positive feedback: cells that excite each other keep each other firing after the input that started them is gone. The worm's backward command cells do this, so a brief touch starts a reversal that outlasts it.

**The goal.** You have two sensors, `touch` and `stop`, and one motor, `reverse`. A touch must switch `reverse` on. It must stay on after the touch ends. The `stop` sensor must switch it off again. Before the touch and after the stop the motor must be quiet. The episode is 4 seconds long. A cell that only relays the touch stops the moment the touch stops, so the level fails it. A sensor can only excite, so `stop` needs a cell that inhibits.

**What is simplified.** In the animal the persistence also uses gap junctions (direct electrical links between cells) and neuromodulators. Here the memory is recurrent excitation only, which is one of several real mechanisms.

**Variants.** None. **Par:** 3 neurons.

#### Stuck?

A memory in a circuit is positive feedback: cells that excite each other keep each other firing after the input that started them is gone.

### Worm: forward or backward

**The idea.** A worm cannot crawl forward and backward at once. A choice between two behaviours needs competition: each command must be able to silence the other, so that only one is ever active.

**The goal.** Two sensors push two command cells: `push_rev` pushes the one behind the motor `reverse`, and `push_fwd` the one behind `forward`. Pushed alone, each motor must answer while the other stays quiet. Pushed together, with one push stronger than the other, only the stronger command may win. The loser must be silenced, not just smaller. Plain cells cannot do this, because each simply follows its own push. The episode is 5 seconds.

**What is simplified.** All weights are game values. The worm's real commands are cells that signal largely with graded potentials; here they are spiking cells.

**Variants.** None. **Par:** 2 neurons.

#### Stuck?

A choice between two behaviours needs competition: each command must be able to silence the other, so that only one is ever active.

### Worm: touch, reverse or go

**The idea.** This is the worm's real touch circuit, drawn from the published wiring, with both of the previous ideas inside it: each command keeps itself going once the touch is over, and the two commands silence each other.

**The goal.** Two sensors (`touch_head`, `touch_tail`) and two motors (`reverse`, `forward`). Between them sit the command cells: a backward command and a forward command, each driven by its own touch cells, each holding its state after the touch has ended, and each silencing the other. After a head touch the right motor must be firing in the window around the touch and the wrong one nearly silent. That must also hold when both ends are touched at once (the stronger touch wins) and while the worm is still in the other state from the touch before.

The real cells are named in the brief: three kinds of touch cell (ALM and AVM in the head half, PLM in the tail), the interneurons AVD, AVA (backward) and PVC, AVB (forward), and the A-type and B-type motor neurons.

**What is simplified.**

- The reference circuit is wired from Cook et al. 2019 synapse counts, read through the OpenWorm ConnectomeToolbox. Weight is count times a fixed scale.
- The data carry no sign. Every chemical synapse is assumed excitatory. Gap junctions are modelled as excitatory links.
- The mutual inhibition between the backward and forward commands is added by hand. It is not a connection in the data.
- Strong connections in the data that are not on the classic touch pathway are left out.
- Spiking cells stand in for cells that mostly signal with graded potentials.

**Variants.** None. **Par:** 17 neurons.

#### Stuck?

Two behaviours that exclude each other need two command groups that inhibit each other, and each command has to keep itself going once the touch is over.

## Chapter: Leech

The medicinal leech heart is driven by a timing network in a few ganglia of the nerve cord. Its core is a pair of heart interneurons (HN cells) that inhibit each other and fire in alternating bursts. The chapter builds this oscillator from its parts: a cell that bursts, a cell that rebounds, and then the pair.

![The leech chapter plate: the leech, its nerve cord and its two heart tubes](img/bestiary-leech.png)

| Level | Kind | What it teaches | Know first | The real biology | Citation |
|---|---|---|---|---|---|
| Leech: a cell that bursts (`l_burst`) | Puzzle | Bursting: a slow variable ends each burst | Adaptation | The HN cells fire bursts with silent gaps, even with steady input. A slow current builds up during the burst and ends it. | Nadim et al. 1995; Hill et al. 2001 |
| Leech: rebound after inhibition (`l_rebound`) | Puzzle | Post-inhibitory rebound | Inhibition, adaptation | A silenced HN cell fires strongly just after release, helped by a hyperpolarisation-activated current and a low-threshold calcium current. | Nadim et al. 1995; Olsen et al. 1995 |
| Leech: the heartbeat (`leech_heart`) | Task | A half-centre with adapting cells | Half-centre, bursting, rebound, adaptation, inhibition | Two HN cells inhibit each other and each inhibits the heart motor neuron of its side, so the two sides beat in turn. | Hill et al. 2001 |
| Diagnosis: the one-sided heartbeat (`dx_leech`) | Case | Finding which inhibition is missing | Half-centre, inhibition | A model of the loss of one inhibitory link between the two HN cells. | Hill et al. 2001, 2002 |

### Leech: a cell that bursts

**The idea.** Bursting comes from a slow variable inside the cell. It builds up during a burst, ends it, and decays during the gap.

**The goal.** Drive one cell with the steady sensor `drive` and read it on `burst`. It must be silent before the drive, and then fire in clear bursts: nearly all of its spikes inside bursts, with real silent gaps between them. An ordinary integrate-and-fire cell fires one spike at a time at a steady rate, because its only reset is the spike itself. You must choose a different cell. Look in the [parts bin](parts-bin.html). The episode is 4 seconds.

**What is simplified.** The level uses a generic bursting cell, not the leech cell. The real cell's currents (including the hyperpolarisation-activated current and the low-threshold calcium current) are not modelled. The real cells burst much more slowly.

**Variants.** None. **Par:** 1 neuron.

#### Stuck?

Bursting comes from a slow variable inside the cell: it builds up during a burst, ends it, and decays during the gap.

### Leech: rebound after inhibition

**The idea.** In a half-centre, one side is held silent by the other. When the other lets go, the silenced cell does not just return to rest. It fires. A rebound needs a slow variable in the cell that the inhibition pushes past its resting value, so that the cell overshoots when the inhibition ends.

**The goal.** The sensor `inhibit` drives an inhibitory cell, which silences a target cell that you read on `rebound`. The target must stay quiet before the inhibition, quiet during it and quiet between the two pulses, and it must fire right after each pulse ends. Most cells cannot do this: an integrate-and-fire cell inhibited for a while simply returns to rest. The target also needs a steady drive of its own, a bias that holds it just below firing. A cell far from threshold has nothing to rebound to. The episode is 4 seconds.

**What is simplified.** The rebound here comes from sub-threshold adaptation in a generic cell. The leech cell's actual currents are not modelled. The modelling papers distinguish a "release" mode (the active cell tires) from an "escape" mode (the silenced cell escapes while the other is still active); the level is agnostic about which you build.

**Variants.** None. **Par:** 2 neurons.

#### Stuck?

A rebound needs a slow variable in the cell that the inhibition drives past its resting value, so that the cell overshoots when the inhibition ends.

### Leech: the heartbeat

**The idea.** A half-centre needs two things: mutual inhibition, so that the side that fires silences the other, and a slow process that wears the active side out, so that the other can take over.

**The goal.** Build two heart interneurons and two heart motor neurons, and read the motors on `left` and `right`. A steady `drive` excites all cells equally, so the rhythm has to come from your circuit. The two sides must alternate: clean turns, almost never firing together, neither side winning for good. Each side must fire in bursts, and a full cycle must take a few seconds. Two plain interneurons that inhibit each other do not alternate. The one that fires first silences the other for ever. The episode is 14 seconds (a shorter 9-second run is used by Check).

**What is simplified.**

- Time is compressed so that a check takes seconds; the real cycle is much slower.
- The real inhibition is partly graded. Here it is spike-triggered.
- Only a single segmental oscillator is built. The coordination between segments is not modelled.
- Weights, adaptation constants and the period band are game values.

**Variants.** None. **Par:** 4 neurons.

#### Stuck?

A half-centre needs mutual inhibition, so the side that fires silences the other, and a slow process that wears the active side out so the other can take over.

## Chapter: Sea slug

*Aplysia* is a large sea slug that Eric Kandel's group used to find the first cellular basis of learning. Touch its siphon and it pulls in its gill. The chapter splits the reflex into its two learning effects, then puts them on the same synapse.

![The sea slug chapter plate: Aplysia, its siphon and its gill](img/bestiary-aplysia.png)

| Level | Kind | What it teaches | Know first | The real biology | Citation |
|---|---|---|---|---|---|
| Aplysia: getting used to a touch (`a_depress`) | Puzzle | Short-term synaptic depression | Excitation | Repeated touch weakens the sensory-to-motor synapse. The sensory cells keep firing the same. The reflex recovers after rest. | Castellucci et al. 1970; Byrne et al. 1978 |
| Aplysia: a shock to the tail (`a_facilitate`) | Puzzle | Heterosynaptic facilitation | Neuromodulation, excitation | A tail shock excites interneurons that release serotonin onto the sensory terminals, which then release more transmitter. | Castellucci et al. 1970; Hawkins et al. 2006 |
| Aplysia: the gill-withdrawal reflex (`aplysia_gill`) | Task | Habituation, recovery and sensitisation on one synapse | Synaptic depression, heterosynaptic facilitation, neuromodulation | One synapse stores two opposite changes with different causes. | Castellucci et al. 1970; Hawkins et al. 2006 |
| Diagnosis: the reflex that does not sensitise (`dx_aplysia`) | Case | Finding a broken facilitating pathway | Synaptic depression, heterosynaptic facilitation | A line of slugs that habituate normally but never sensitise. | Castellucci et al. 1970; Hawkins et al. 2006 |

### Aplysia: getting used to a touch

**The idea.** A synapse can change its strength on a time scale of seconds. A depressing synapse releases less each time it is used and recovers slowly. This is habituation: the withdrawal shrinks although the sensory cell keeps firing the same.

**The goal.** The sensor `touch` is pressed five times. The first withdrawal on `gill` must be a clear one, and the fifth clearly weaker, with nothing happening between the touches. A normal static synapse gives every touch the same answer. A depressing synapse runs out of transmitter when it is used hard and refills slowly, so each touch finds it a little emptier. Build a sensory cell, a motor neuron and that kind of synapse between them. You choose the synapse type in the inspector when you select the connection. See [Synapses and modulators](synapses-and-modulators.html). The episode is 3.2 seconds.

**What is simplified.** Time is compressed (seconds instead of minutes). Depression is the generic Tsodyks-Markram form, not a fit to *Aplysia* data. The weights needed are large compared with the default; a weak synapse gives no reflex to habituate.

**Variants.** None. **Par:** 2 neurons.

#### Stuck?

A synapse can change its strength on a time scale of seconds: a depressing synapse releases less each time it is used and recovers slowly.

### Aplysia: a shock to the tail

**The idea.** One pathway can change how strongly another one transmits. A modulator released by the first acts on the synapse of the second. In the slug, a tail shock makes a facilitating interneuron release serotonin, and serotonin strengthens the synapse of the siphon's sensory cell onto the motor neuron.

**The goal.** `touch` drives a sensory cell, which drives the motor neuron behind `gill`. `shock` drives a facilitating interneuron whose spikes release the modulator on the `serotonin` channel. The released modulator must act on the sensory synapse: select that connection and let the modulator change it. Two touches before the shock give a modest withdrawal. Two after it must give clearly more, with nothing happening between the touches. The episode is 3.8 seconds.

**What is simplified.** Serotonin is the engine's norepinephrine (NE) channel. The cAMP cascade is replaced by a direct change of the synapse. Real short-term facilitation uses cAMP and protein kinase A, and repeated, spaced shocks recruit slower phases that need gene expression; none of that is here.

**Variants.** None. **Par:** 4 neurons.

#### Stuck?

One pathway can change how strongly another one transmits: a modulator released by the first acts on the synapse of the second.

### Aplysia: the gill-withdrawal reflex

**The idea.** One synapse, two opposite memories. The synapse weakens with use (habituation) and is strengthened by the tail pathway (sensitisation). The same facilitation can restore a habituated response, which is called dishabituation.

**The goal.** Build the reflex arc and both kinds of learning. `touch` drives a sensory cell that drives the motor neuron behind `gill`. Habituation lives in that synapse, which must lose strength with use and recover with rest. `shock` drives a facilitating interneuron that releases serotonin on the `serotonin` channel, and the serotonin must act on the same synapse and strengthen it. Checked in one run:

- The reflex must be there.
- It must be smaller on the fourth touch than on the first.
- It must be back after the rest.
- It must be clearly larger after the shock than just before it.
- The gill must be still when nothing touches it.

The tail shock may also make the gill move by itself. The episode is 7.1 seconds.

**What is simplified.** The real slug needs many more touches and minutes; this level compresses time. The synapse changes are the generic depression of the previous level and the direct facilitation of the one before.

**Variants.** None. **Par:** 9 neurons.

#### Stuck?

One synapse can store two opposite changes: it depresses with use (habituation) and is strengthened by a modulator released by another pathway (sensitisation).

## Chapter: Fly compass

A fly keeps track of the direction it faces with a ring of cells in the central complex of its brain. At any moment one wedge of the ring is active: a bump whose position is the heading. The bump stays when the lights go out and it moves when the fly turns. The chapter builds the ring, then the shifting cells that move the bump, then scores the whole compass in the dark.

![The fly compass chapter plate: a ring of cells with a bump of activity](img/bestiary-fly-compass.png)

| Level | Kind | What it teaches | Know first | The real biology | Citation |
|---|---|---|---|---|---|
| Fly: a bump that stays (`c_ring`) | Puzzle | Ring attractor | Excitation, inhibition, persistence | E-PG cells tile a ring and one wedge is active. The bump holds for many seconds in the dark. | Seelig and Jayaraman 2015, Nature 521:186 |
| Fly: moving the bump (`c_shift`) | Puzzle | Path integration | Ring attractor, inhibition, persistence | P-EN cells get input from the bump and from a turning signal; left and right subtypes shift the bump opposite ways. | Turner-Evans et al. 2017; Green et al. 2017 |
| Fly: the compass in the dark (`fly_compass`) | Task | Holding a heading and following turns | Ring attractor, path integration, inhibition, persistence | Silencing the shifting cells breaks tracking in the dark. | Turner-Evans et al. 2017; Green et al. 2017 |
| Diagnosis: the compass that walks off (`dx_compass`) | Case | A one-way tracking error | Ring attractor, path integration | A compass that follows turns one way and loses the other. | Turner-Evans et al. 2017; Green et al. 2017 |

### Fly: a bump that stays

**The idea.** A bump on a ring is held by two opposite things: cells that excite their neighbours, and an inhibition that every cell shares. Excitation alone spreads round the whole ring. The shared inhibition makes the bump compete with itself, so one bump wins.

**The goal.** Build a ring of sixteen cells and read `heading` as a population vector over them, in ring order (the readout turns the pattern of activity into one direction). The sensors `cue0` to `cue15` are the landmark seen at each of sixteen bearings, one sensor for each ring cell. One is on for a moment and then goes off. While it is on there must be a single bump at the landmark: a few neighbouring cells firing, the rest quiet. One and a half seconds after it has gone, the bump must still be there, in the same place. The brief spells out how default cells fall short, and the Inspector has a note on numbered families of sensors; use "Wire all to" once instead of sixteen drags. The episode is 3 seconds. You will also use the [parts bin](parts-bin.html) and the synapse parameters.

**What is simplified.** The ring is a ring-attractor model after the published cell types, not synapse-level wiring. Excitation between ring cells is a direct stand-in for what is largely indirect in the animal. The 16-wedge ring, the weights, the slow excitatory synapse and the bias are game values. Time is the real one.

**Variants.** None. **Par:** 24 neurons.

#### Stuck?

A bump on a ring is held by two opposite things: cells that excite their neighbours, and an inhibition that every cell shares.

### Fly: moving the bump

**The idea.** Two opposite pushes cancel. Give one of them a little more drive and the bump moves. The angular velocity is the difference between the two sides.

**The goal.** The ring from the previous level gets two groups of shifting cells, one for each direction. Each group listens to the compass cells at the bump and to its own turn sensor, and excites the compass cells one step to its own side. The sensors `turn_left` and `turn_right` both carry a steady level, equal while the fly is still. During a turn the sensor of that side carries more and the other less. The bump must form at the landmark, stay while the fly is still, move right during a turn to the right, and move back during a turn to the left. A shifting group that listened only to the turn sensor would push everywhere on the ring; it must also need the bump. Each unit of a turn sensor is only a small current, so its weight is small next to the ring's own weights. The episode is 4.4 seconds.

**What is simplified.** The turn signal is a pair of steady inputs rather than noisy spikes, so the gain does not vary from seed to seed. Tracking is good only for turns of about 90 to 170 degrees per second. A discrete ring of 16 wedges has a dead zone for slow turns.

**Variants.** None. **Par:** 60 neurons.

#### Stuck?

Two opposite pushes cancel. Give one of them a little more drive and the bump moves; the angular velocity is the difference between the two sides.

### Fly: the compass in the dark

**The idea.** Holding a heading needs a ring attractor. Following turns needs two shifters, one for each direction, driven by the angular velocity and each pushing the bump one step to its own side. Together they integrate the fly's turns, which is called angular path integration.

**The goal.** A landmark sets the bump, then the lights go out and the fly turns. The bump must hold its place when the fly is still and move round the ring as fast as the fly turns. You build the ring from the previous levels (16 compass cells, global inhibition) and the two shifting groups. You are scored on the heading error in the dark: how far the bump is from the true heading, after holding still and after each turn. A ring alone cannot move, and shifters on one side cannot turn both ways. The first two levels of the chapter give you the parts.

**What is simplified.** Same as the two puzzles. Heading is scored with a population vector of the ring cells.

**Variants.** None. **Par:** 60 neurons.

#### Stuck?

Holding a heading needs a ring attractor; following turns needs two shifters, one for each direction, driven by the angular velocity and each pushing the bump one step to its own side.

## Chapter: Fly mushroom body

The mushroom body is the fly's centre for learned smell. An odour arrives as a pattern on projection neurons. Thousands of Kenyon cells turn it into a sparse code, a handful of active cells out of about two thousand. Output neurons read that code and drive approach or avoidance. Punishment makes dopamine neurons fire, and dopamine weakens only the synapses that were just active. The chapter builds the sparse code, then the learning rule, then the whole loop.

![The fly mushroom body chapter plate: projection neurons, Kenyon cells, the giant inhibitory neuron and an output neuron](img/bestiary-fly-mb.png)

| Level | Kind | What it teaches | Know first | The real biology | Citation |
|---|---|---|---|---|---|
| Fly: a sparse odour code (`k_sparse`) | Puzzle | Sparse coding | Excitation, inhibition, negative feedback | Each Kenyon cell listens to a few projection neurons at random, and one giant inhibitory neuron (APL) keeps the active number small. | Aso et al. 2014, eLife 3:e04577; Papadopoulou et al. 2011 |
| Fly: learning to avoid an odour (`k_depress`) | Puzzle | Dopamine-gated depression | Excitation, heterosynaptic facilitation | Odour then dopamine depresses the active Kenyon cells' synapses onto an output neuron, odour by odour. | Hige et al. 2015, Neuron 88:985 |
| Fly: the mushroom body learns to avoid (`fly_mb`) | Task | The whole learning loop | Sparse coding, dopamine-gated depression, negative feedback, inhibition | Odours are sparse patterns; punishment-triggered dopamine weakens the avoidance output for that odour only. | Aso et al. 2014 (two papers); Hige et al. 2015 |
| Diagnosis: the fly that cannot tell odours apart (`dx_mb_apl`) | Case | Dense codes and overlap | Sparse coding, dopamine-gated depression | A fly whose codes are too dense and overlap. | Hige et al. 2015; Papadopoulou et al. 2011 |
| Diagnosis: the fly that never learns (`dx_mb_dan`) | Case | A missing teaching signal | Sparse coding, dopamine-gated depression | A fly that feels the shock but never learns the odour. | Hige et al. 2015; Aso et al. 2014 |

### Fly: a sparse odour code

**The idea.** Sparse codes come from two things. Each cell needs several of its few random inputs to fire. And one inhibitory cell that every active cell excites inhibits them all, so a stronger odour does not simply recruit more of them. Sparse codes make memories specific: two odours share few cells.

**The goal.** The sensors `pn0` to `pn19` are twenty glomeruli; an odour is the pattern of six of them that are active. Build twenty projection neurons driven by the sensors (one sensor each), a large group of two hundred Kenyon cells (so the code does not depend on the luck of one random wiring) and the giant inhibitory loop, and read `kc` from the Kenyon cells. Use random sparse wiring: each Kenyon cell listens to just four randomly chosen projection neurons. Odour A at a weak level must make a few Kenyon cells fire. Odour B must make mostly other cells fire. Odour A three times stronger must still make only a few fire, not many more. Without the inhibitory loop a strong odour recruits far too many cells. The episode is 2.2 seconds.

The inspector has a "wiring" option on an axon (random sparse wiring) for this.

**What is simplified.**

- 200 Kenyon cells, not about 2000. Four inputs per cell and six-glomerulus odours are game values.
- The odour patterns are made up.
- The APL is a spiking group. In the locust analogue it is a nonspiking neuron.
- It is a model after the published cell types, not synapse-level data.

**Variants.** None. **Par:** 250 neurons (the reference circuit has 224 cells).

#### Stuck?

Sparse codes come from two things: each cell needs several of its few random inputs to fire, and one inhibitory cell that every active cell excites inhibits them all.

### Fly: learning to avoid an odour

**The idea.** Learning here is a three-factor rule. A synapse changes only if its input was active, its output cell was active, and dopamine arrives. Dopamine does not strengthen or weaken anything on its own. So only the synapses of the punished odour's Kenyon cells lose strength.

**The goal.** The sensors `odor_a` and `odor_b` drive two groups of Kenyon cells, one for each odour. `punish` drives the dopamine neurons. Read `avoid` from the avoidance neurons. Both odours must make the avoidance cells fire before training. Then odour A is paired with punishment twice. Afterwards the avoidance cells must answer odour A much less than before, and odour B as much as before. Use a dopamine-gated synapse from each group of Kenyon cells onto the avoidance cells, with the dopamine neurons as its source. The synapse rule also looks at the avoidance cells' own spikes, so the odours have to make them fire during the pairing. The synapse types are in the [inspector](synapses-and-modulators.html).

**What is simplified.** The rule is NEST's dopamine-gated STDP with signs set so the synapse depresses. The real rule needs no postsynaptic spike. There is one compartment and one avoidance output; valence is not modelled beyond that. A pairing of 0.4 s stands for a training session.

**Variants.** None. **Par:** 6 neurons.

#### Stuck?

Learning here is a three-factor rule: a synapse changes only if its input was active, the output cell was active, and dopamine arrives.

### Fly: the mushroom body learns to avoid

**The idea.** Put the two halves together. The sparse code makes the memory specific to the punished odour. The dopamine rule writes the memory. Without the giant inhibitory neuron the code is dense and the odours overlap, so punishing one hurts the other. Without dopamine nothing is learned.

**The goal.** Twenty projection neurons driven by `pn0` to `pn19`, two hundred Kenyon cells with random sparse wiring, the APL loop, a small group of avoidance output neurons read on `avoid`, and the Kenyon cells read on `kc`. The sensor `punish` drives a punishment cell, which drives the dopamine neurons, which are the dopamine source of the Kenyon-cell-to-output synapse. You are scored on the protocol: both odours make the avoidance cells fire; odour A is paired with punishment twice; afterwards the avoidance cells must answer odour A much less than before and odour B almost as before. Along the way the Kenyon-cell code must be sparse and the two odours' codes must mostly differ.

**What is simplified.** See the two puzzles.

**Variants.** None. **Par:** 20 neurons, as the engine states it. The reference circuit has 231 cells, so par here is not simply a cell count; read the end card for how your design is judged.

#### Stuck?

Three parts: a sparse odour code (random few-input wiring plus feedback inhibition), a punishment pathway to dopamine neurons, and a synapse from the odour code onto the output neurons that dopamine can weaken.

## Chapter: Crab

In a crab's stomach a small nerve ring, the stomatogastric ganglion, drives the filtering of food. Its pyloric rhythm repeats about once a second. A pacemaker kernel bursts by itself, and two kinds of follower are silenced by it and fire when it lets go. The order of the bursts is PD, then LP, then PY. Famously, very different sets of cell conductances give the same pattern. The chapter builds the kernel, the rebound that sets a follower's phase, and the rhythm.

![The crab chapter plate: the stomatogastric ganglion with its pacemaker kernel and followers](img/bestiary-crab.png)

| Level | Kind | What it teaches | Know first | The real biology | Citation |
|---|---|---|---|---|---|
| Crab: a pacemaker kernel (`s_pacemaker`) | Puzzle | A burster that drags its partners into its rhythm | Bursting, excitation, coupling | The AB neuron is electrically coupled to two PD neurons and they burst as one unit. | Marder and Bucher 2001; Prinz et al. 2004 |
| Crab: phase from rebound (`s_phase`) | Puzzle | A follower's place in the cycle comes from rebound | Rebound, inhibition, bursting | LP and PY are inhibited by the kernel and fire when it lets go, after a delay set by slow currents. | Marder and Bucher 2001; Prinz et al. 2004 |
| Crab: the pyloric rhythm (`crab_pyloric`) | Task | The triphasic rhythm and degeneracy | Pacemaker kernel, rebound, inhibition, bursting | Different animals reach the same rhythm with widely different parameters. | Prinz et al. 2004, Nat Neurosci 7:1345 |
| Diagnosis: the follower that does not wait (`dx_crab`) | Case | A rhythm that has lost its order | Pacemaker kernel, rebound, degeneracy | A second animal, whose cells have different conductances, with one dead synapse. | Prinz et al. 2004; Marder and Bucher 2001 |

All three levels use a conductance-based cell, `stg_cell`, which is the default model for the chapter. A new `stg_cell` has only the spike currents (sodium and delayed rectifier), so it is silent. The conductances you add make it burst or rebound. In this chapter units matter: conductances are in nS, a bias is in pA, a connection's weight is a synaptic conductance in nS (negative inhibits), and the modulator's weight is a current in pA per unit of modulator.

### Crab: a pacemaker kernel

**The idea.** A kernel is a bursting cell plus partners that it drags into its rhythm. Slow currents make a cell burst. Coupling makes cells burst together.

**The goal.** The sensor `modulator` is the neuromodulator bath that the pacemaker needs. Build the burster AB and two PD cells, couple them both ways, and read `ab` and `pd`. The kernel must burst regularly with a cycle of about a second, each burst must fill a sensible part of the cycle, and the PD cells must burst together with AB, cycle after cycle. A plain integrate-and-fire cell does not burst. Without the coupling the PD cells would burst on their own schedule, out of step with AB. The episode is 8 seconds.

**What is simplified.** The engine couples cells electrically only for tonic cell models, so here the coupling is approximated by strong, fast excitation both ways between AB and PD. The cell is a NESTML re-fit, with kinetic forms written from memory of the published ones and game conductances.

**Variants.** None. **Par:** 3 neurons.

#### Stuck?

A kernel is a bursting cell plus partners that it drags into its rhythm. Slow currents make a cell burst; coupling makes cells burst together.

### Crab: phase from rebound

**The idea.** Inhibition removes the follower from the pacemaker's phase. Currents that wake up when it is released bring it back after a delay. The delay is the phase.

**The goal.** The sensor `pd` stands for the pacemaker's inhibition: a burst of inhibitory input of a third of a second at the start of each second. Build the follower as a single conductance-based cell, with the currents that make a rebound and without the calcium-activated potassium current that would make it a pacemaker itself. Connect `pd` to it as an inhibitory input (a negative weight) and read `lp`. The cell must stay silent while it is inhibited and fire a burst after the inhibition ends, with its first spike somewhere between the middle of the cycle and the last quarter of it. An integrate-and-fire cell just resumes its steady firing the moment the inhibition stops, or never fires at all. The episode is 6.5 seconds.

**What is simplified.** The graded glutamatergic synapses of the real network are spike-triggered inhibitory conductances here. See the notes at the top of this chapter on the cell.

**Variants.** None. **Par:** 1 neuron.

#### Stuck?

Inhibition removes the follower from the pacemaker's phase; currents that wake up when it is released bring it back after a delay: the delay is the phase.

### Crab: the pyloric rhythm

**The idea.** The pattern comes from a pacemaker that bursts by itself and followers that are silenced by it and rebound when it stops. A second inhibition, LP onto PY, puts PY last. Many different sets of conductances give the same rhythm; that is degeneracy. The game gives you one set to find.

**The goal.** Build a pacemaker kernel of AB and PD, an LP cell and PY cells, and read `pd`, `lp` and `py`. The sensor `modulator` is the bath the pacemaker needs. The brief gives the published three-cell scheme: AB and PD inhibit LP and PY; LP inhibits PD and PY; PY inhibits LP. The parameters are yours to find. You are scored on the rhythm: cycles of about a second, regular, with the bursts in the order PD, LP, PY, and with sensible burst lengths. The episode is 11 seconds (8 for Check).

**What is simplified.**

- The cell is not the published parameter set; the kinetic forms were written from memory.
- Cell counts (one AB, two PD, one LP, two PY), conductances and synaptic strengths are game values.
- The reference ships in two hand-picked parameter sets (found by random search; both pass), which is the degeneracy the chapter is about. The diagnosis case uses the second set.
- In the networks tried, the cycle is moved most cleanly by AB's H current and the modulator's drive, while the calcium-activated potassium current mostly sets the burst length. The rhythm is fragile to many other single-parameter changes. That is measured in the game, not published.

**Variants.** None. **Par:** 6 neurons.

#### Stuck?

The pattern comes from a pacemaker that bursts by itself and followers that are silenced by it and rebound when it stops; a second inhibition puts the last cell after the middle one.

## Diagnosis cases in this wing

Each chapter ends with a diagnosis case, and the mushroom body has two. The patient is a working circuit with exactly one connection dead. You cannot edit the circuit. You can run it, open scopes, trace single cells, and use the lesion lab to cut other connections and see what changes. Then you pick the dead connection from a short list of candidates and submit.

The rules are the same as in the body chapters. For the full method see [Diagnosis cases](diagnosis-cases.html) and [The lesion lab and analysis tools](lesion-lab-and-analysis.html).

- The case card gives the findings in plain language. The vignette and symptoms below are what you are told.
- A wrong pick says only "not that one". The reasons come after the case is solved or revealed.
- A connection's type is shown but never its weight. The dead connection is kept at weight 0, so you find it by watching a cell that fires into a silent target, not by looking for a missing arrow.
- The score is 100, minus 15 for every wrong guess, minus 3 for every probe beyond the first three (a scope opened, a cut tried, a lesion re-run), never below 25.
- Stars: one for finding the cause. Two for no wrong guess. Three for no wrong guess and a score of 85 or more.
- After three wrong guesses the Reveal answer button appears. Revealing the answer scores nothing.
- Hints are available in the same ladder as for levels.

### Diagnosis: the worm that cannot back up

**Opens after:** Worm: touch, reverse or go. **Candidates:** 6.

**The case.** A *C. elegans* from a mutant strain crawls and feeds normally, but does not back away when its head is touched with an eyelash. A touch on the tail makes it run forward as usual. A touch on the head makes it stop dead and nothing else: it does not back away, and it does not crawl forward either, but lies still for as long as the touch lasts. Muscle recordings show no backward movement at all.

**What to probe.** Follow the chain from touch cell to command interneuron to motor neuron during a head touch, and ask which cell is active while the one after it is silent. This case does what Chalfie and colleagues did with a laser: remove one link and see which response goes.

#### Stuck?

A reversal needs a chain of cells: touch cell, command interneuron, motor neuron. Look for the cell that is active while the cell after it stays silent.

### Diagnosis: the one-sided heartbeat

**Opens after:** Leech: the heartbeat. **Candidates:** 4.

**The case.** A medicinal leech has been dissected so the heartbeat timing network and its two heart motor neurons can be recorded. The two sides of the heart no longer take turns: the left heart tube is driven steadily and the right one is almost still. Recordings from the two motor neurons show no bursts that give way to the other side.

**What to probe.** Compare the two heart interneurons: one is strong, one is held down. In a half-centre, both sides must be able to silence each other. Ask which side can no longer do so.

#### Stuck?

In a half-centre both sides must be able to silence each other. If one side cannot, which side wins, and what happens to the other?

### Diagnosis: the reflex that does not sensitise

**Opens after:** Aplysia: the gill-withdrawal reflex. **Candidates:** 4.

**The case.** A sea slug from a breeding line is tested for the two simple kinds of learning of its gill-withdrawal reflex. The reflex is normal: a siphon touch gives a clear withdrawal, repeated touches make it smaller, and after a rest it is back. A shock to the tail still makes the gill twitch by itself. But the touch after a tail shock is no stronger than the touch before it.

**What to probe.** Sensitisation needs a pathway that releases a modulator when the tail is shocked. Watch the cells of that pathway during the shock.

#### Stuck?

Sensitisation needs a pathway that releases a modulator when the tail is shocked. Which cell must fire for that, and does it?

### Diagnosis: the compass that walks off

**Opens after:** Fly: the compass in the dark. **Candidates:** 5.

**The case.** A fruit fly from a mutant line is shown a landmark, then the lights go out and it is made to turn left and right in the dark. With the fly still, the bump holds its place, so the ring is fine. A turn to the right is followed correctly. A turn to the left is hardly registered, so after a right turn and a left turn the compass reports a heading that is off in one direction, and every further back-and-forth adds to the error.

**What to probe.** Only one direction of turn fails. Ask which part of the circuit is used only for that direction, and watch it during that turn.

#### Stuck?

Only one direction of turn fails. Which part of the circuit is used only for that direction?

### Diagnosis: the fly that cannot tell odours apart

**Opens after:** Fly: the mushroom body learns to avoid. **Candidates:** 5.

**The case.** A fruit fly from a mutant line is trained to avoid one odour. Before training it answers both odours, but far more strongly than a normal fly does. After training it no longer answers the punished odour, so the punishment pathway works and the learning happens. In the Kenyon cells, for each odour about a third of them fire, where a normal fly uses a small handful, and about a quarter of the cells that answer one odour answer the other too (a normal fly shares almost none).

**What to probe.** A memory that spreads to the wrong odour means the codes of the two odours overlap. Count how many Kenyon cells fire for one odour.

> **Note:** At the size of this model, the overlap between odours shows in the cell counts, not in a measurable loss of specificity in the behaviour. Do not expect the second odour to lose its answer visibly.

#### Stuck?

A memory that spreads to the wrong odour means the codes of the two odours overlap. What keeps a code sparse?

### Diagnosis: the fly that never learns

**Opens after:** Fly: the mushroom body learns to avoid. **Candidates:** 5.

**The case.** A fruit fly from a mutant line is trained to avoid one odour with the usual pairings of odour and punishment. Before training it answers both odours, and the Kenyon-cell code is sparse and different for the two. After training nothing has changed: it answers the punished odour exactly as before. It does react to the shock itself, so it feels the punishment.

**What to probe.** Learning needs a teaching signal. Watch the cells that carry it while the punishment cell fires.

#### Stuck?

Learning needs a teaching signal. Which cells carry it, and do they fire when the fly is punished?

### Diagnosis: the follower that does not wait

**Opens after:** Crab: the pyloric rhythm. **Candidates:** 5.

**The case.** A crab stomatogastric ganglion from a second animal, whose cells have different conductances from the textbook ones, is recorded. The rhythm from the motor nerves is regular, about one cycle a second, but the pattern is no longer triphasic. The last burst of the cycle begins early, at the same time as the middle one, instead of after it, and lasts about as long as the whole quiet half-cycle. It is still held off during the first burst of the cycle.

**What to probe.** The network can compensate for losing some of its inhibition, so the symptom does not point at one cell directly. Watch the last follower while the middle one fires and compare with what the pacemaker's burst does to it.

> **Note:** This patient is built on the second parameter set of the crab task. Do not assume the same conductances as in your own solution.

#### Stuck?

The last follower has several sources of inhibition, and the network can compensate for losing some of them. Which one puts it after the middle cell?

## Where to go next

- [The body chapters](campaign-body.html): the five organ chapters. They are separate from this wing.
- [The Theory wing](campaign-theory.html): what a computational neuroscience course teaches, in 12 levels.
- [Parts bin](parts-bin.html) and [Synapses and modulators](synapses-and-modulators.html): the cells and synapses these levels ask for.
- [Inputs and outputs](inputs-and-outputs.html): the population-vector readout the compass uses and the modulator channels the sea slug and crab use.
- [What is real](what-is-real.html): the honest account of every simplification in this wing.
- [Glossary](glossary.html): the concept terms used here (half-centre, rebound, bursting, ring attractor, sparse coding, degeneracy and so on).
- [The notebook](notebook.html): each first pass adds a cited entry.
