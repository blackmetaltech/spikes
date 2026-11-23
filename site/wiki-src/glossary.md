---
title: Glossary
section: Reference
order: 54
summary: Every concept and lab term the game uses, alphabetical, one plain line each.
---

# Glossary

Every term the game and this wiki use, in alphabetical order. Concept entries name the level that teaches them and match the Concepts tab of [the notebook](notebook.html). Lab terms are the words on the building screen.

## A

**ACh (acetylcholine).** A neuromodulator. In the heart levels it carries the vagus nerve's slowing signal. It is one of three global modulator levels, with NE and DA.

**Adaptation.** Under a steady push, a neuron fires a burst, then slows as a slow current builds up. It reports change more than level. Taught in the Adaptation micro-puzzle.

**Advanced.** Inspector button. It shows a cell model's parameters as sliders with units, plus the rows for how it responds to modulators.

**Afferent.** A nerve fibre that carries signals toward the centre, for instance from the skin or a muscle. In the game, a sensor stands for a bundle of afferents.

**Antagonist.** The muscle that opposes another (the agonist). See reciprocal inhibition.

**Attractor memory (Hopfield).** A network whose connections store a pattern as a stable state. Cueing part of the pattern makes the network fall into the whole one. Taught in Recalling a whole from a part.

**Axon.** The output fibre of a neuron. On the bench, a connection is drawn as an axon with an arrowhead.

## B

**Bench.** The circuit editor, where neurons and connections are drawn.

**Bench history.** The newest ten benches that Reference, Empty bench, Load, a slot, Paste circuit JSON or the tutorial replaced, kept per level. More, Restore previous bench brings back the newest.

**Bestiary.** The wing of the campaign with one chapter per model animal. See [the Bestiary](campaign-bestiary.html).

**Bias current (tonic drive).** A steady current of a cell's own (the parameter I_e in its advanced parameters). It holds the cell near or just over threshold, so it fires or is ready to fire with no input. A cell with a bias can signal "less" as well as "more". Taught in the Tonic drive micro-puzzle; used by posture, the pupil, the autonomic heart and the fly compass.

**Bistability.** One cell and one current with two stable states, rest and repetitive firing. A brief pulse can flip between them. Taught in A neuron that remembers a pulse.

**Branch.** A level that hangs off another one: a diagnosis case, a challenge variant or an optional level such as Posture or Reach. A branch never gates anything.

**Burst.** A group of spikes close together with silent gaps around it. See [the analysis tools](lesion-lab-and-analysis.html).

**Bursting.** A cell turns steady input into bursts with silent gaps. A slow variable builds up during a burst and decays during the gap. Taught in Leech: a cell that bursts.

**Bus card.** One card on the bench for a run of four or more numbered sensors or motors of one kind (cue0 to cue15 read "cue ×16"). Click it to open it into one card per channel.

## C

**Challenge.** The optional goal on a level's card. The third star asks for it: by default a neuron par, on some levels also an energy ceiling or no modulator.

**Check.** The button that scores your circuit in the background on five random worlds, with the level's quick episode. Pass all five for the second star.

**Class I and class II (excitability class).** How a cell starts to fire as the current grows. Class I fires slowly at first and speeds up smoothly. Class II starts at a clear rate at once. Taught in Where firing starts.

**Command switch (winner takes all).** Two command groups that inhibit each other, so only one is active. The stronger input wins and silences the other. Taught in Worm: forward or backward.

**Conductance.** How easily a channel lets current through. It is measured in nanosiemens (nS). Conductance-based cells (names with "cond") take synapse weights in nS. Current-based cells take weights in pA.

**Convergence.** Many inputs onto one cell. If each is weak, the cell fires only when several arrive within a short time. It detects coincidence. Taught in Convergence.

**Coupling.** Oscillators that influence each other lock their phases. Strength and delay decide whether they fire together or in antiphase. Taught in Coupling.

**CV (coefficient of variation) of the intervals, ISI CV.** The spread of the time between spikes, relative to its mean. Near 0 is regular, near 1 is random.

## D

**DA (dopamine).** A neuromodulator. In the mushroom-body levels it gates learning. One of three global modulator levels.

**Degeneracy.** Very different sets of conductances and synaptic strengths can give the same network pattern. Taught in the crab diagnosis case.

**Delay.** How long a spike takes to travel along a connection, in milliseconds. Synaptic delay is a few ms.

**Details.** The button on the score card that swaps the criteria rows for the raw score parts, each under a human label.

**Diagnosis.** Working backwards from symptoms to a lesion. See [diagnosis cases](diagnosis-cases.html).

**Divergent command.** One command cell fans out to many targets, so a single cue changes the whole body at once. Taught in Divergent command.

**Dopamine-gated depression.** A synapse that weakens only when its input was active and dopamine arrives afterwards. Taught in Fly: learning to avoid an odour.

## E

**E / I.** The excitatory or inhibitory sign of a neuron group. The button flips it and every outgoing weight changes sign.

**Energy budget (energy ceiling).** On some levels, the mean firing rate per neuron that the challenge allows. See [stars, par and hints](stars-and-hints.html).

**Ensemble.** A neuron group of more than twelve cells, drawn as one stained ellipse with a ring of sample cells.

**Episode.** One run of a level, from time zero to the level's duration.

**Escape or release.** Two cells that inhibit each other can take turns in two ways. In release the active cell tires and lets go. In escape the silenced cell recovers by itself and breaks away while the other is still firing. The bursts look alike. Driving one cell harder tells them apart: watch how the cycle responds. Taught in Leech: the heartbeat.

**Excitation.** An excitatory synapse pushes the next cell toward threshold. Its weight sets how far, its delay how late. Taught in Excitation: weight and delay.

**Excitatory.** Pushes other cells toward firing. Drawn in sienna.

## F

**Feed-forward and feedback.** Feed-forward acts on a cue before an error appears. Feedback corrects what is left. Together they are fast and accurate. Taught in Feed-forward and feedback.

**f-I curve.** The firing rate (f) of a cell against the current (I) it receives. Each card in the parts bin shows one.

**Fixed point.** In the phase plane, a state where nothing changes. A stable fixed point is a resting state the cell returns to.

## G

**Gap junction.** An electrical connection between two cells that passes current directly. The engine couples electrically only the models that are made for it.

**Guided.** The easiest mode. The recipe is printed, hints are free, and a level earns at most two stars. See [modes](modes.html).

## H

**Half-centre.** Two groups that inhibit each other cannot both win for long. With adaptation or fatigue they alternate. Taught in Half-centre.

**Hebbian storage (STDP).** Connections between cells that fire together are strengthened. With spike-timing-dependent plasticity a presynaptic spike just before a postsynaptic one strengthens the connection. Taught in Writing a memory with plasticity.

**HRV (heart-rate variability).** How much the time between beats varies from beat to beat.

**Hz (hertz).** Spikes per second.

## I

**Inhibition.** An inhibitory synapse pushes the next cell away from threshold. It silences, gates and shapes. Taught in Inhibition.

**Inhibitory.** Pushes other cells away from firing. Drawn in Prussian blue, with a bar at the connection's end.

**Inspector.** The panel that shows details of what you selected.

**Integrate-and-fire.** A simple neuron model: the voltage rises with input, a spike fires at threshold and the voltage resets. The default cell is `iaf_psc_alpha`.

## K

**Kenyon cells.** In the fly's mushroom body, many cells that each listen to a few inputs and fire sparsely. See Sparse coding.

## L

**Lateral inhibition (sharpening tuning).** Inhibition among cells with different preferences makes each cell's tuning narrower than its input's. Taught in Sharpening a tuning curve.

**Lesion.** Damage to a part of a circuit. In the game, a cut axon, a silenced group or a cut sensor.

**Level kind.** One of micro-puzzle, task, boss or diagnosis.

## M

**Mean field.** In a large sparse network of integrate-and-fire cells, a cell's input is the sum of many weak independent inputs, so its mean and variance fix its rate. A single unit applying the formula stands for a whole population. Taught in The network and its mean field.

**Micro-puzzle.** A short level of one to a few cells, one concept, a few seconds.

**Mode.** Guided, Standard or Researcher.

**Motif.** A small wiring pattern of a few cells, counted by the motif census.

**Motor.** An output card. Its value is the mean firing rate of the neurons you connect to it.

## N

**NE (noradrenaline).** A neuromodulator. In the heart and fight-or-flight levels it carries the sympathetic nerve's speeding-up signal. In the Aplysia levels the same channel is shown as serotonin.

**Needs.** Red chips on a level card. They name concepts the level uses and another level teaches, until you pass that level. Advice, not a lock.

**NESTML.** A language for writing neuron models that NEST compiles. The game's special cells are written in it.

**Neuromodulation.** A modulator does not fire the next cell. It changes how that cell responds, for instance slowing a pacemaker or raising its gain. Taught in Neuromodulation.

**Nullcline.** In the phase plane, the curve where one of the two variables stops changing. Fixed points are where the nullclines cross.

## P

**Pacemaker kernel.** A bursting cell coupled to partners it drags into its rhythm. Taught in Crab: a pacemaker kernel.

**Pacemaking.** Some cells fire with no input: a slow inward current drifts the voltage up to threshold again and again. Taught in Pacemaking.

**Par.** The neuron count that a level's challenge asks for by default.

**Path integration.** Adding up movement to keep track of position or heading. Taught in Fly: moving the bump.

**Persistence.** Cells that excite each other keep firing after the input that started them has gone. Taught in Worm: holding a reversal.

**Phase lag.** A follower inhibited by a pacemaker and firing by rebound bursts after the pacemaker's burst in the cycle. Taught in Crab: phase from rebound.

**Phase plane.** A picture of a cell with its voltage on one axis and a recovery variable on the other.

**Plate 0.** The interactive first-neuron tutorial that a fresh save starts with. It can be skipped and replayed from the campaign header.

**Population.** A group of neurons of the same model, drawn as one disc. In the file format it is a "population".

**Population vector.** Each cell is broadly tuned and noisy, but the sum of the preferred directions weighted by firing points to the stimulus. Taught in Reading a direction from a crowd.

**PRC (phase-response curve).** How much a push at each moment of a cycle moves the next beat.

**Projection.** The file-format name for a connection between two populations.

## R

**Raster.** The scrolling chart of spikes, one row per neuron.

**Rate unit.** A model that has a firing rate instead of spikes. Used in the Theory wing.

**Rebound (post-inhibitory).** A cell held silent by inhibition can fire when the inhibition ends, because slow currents pushed the other way. Taught in Leech: rebound after inhibition.

**Recipe.** The reference circuit's numbers in words.

**Reciprocal inhibition.** When an agonist muscle is driven, its antagonist is inhibited through an interneuron. Taught in Reciprocal inhibition.

**Recruitment (size principle).** A motor pool is recruited from small, easily excited neurons to large ones. Force grows smoothly. Taught in Recruitment.

**Recurrent inhibition.** A motor neuron excites a Renshaw cell, which inhibits it back. A gain control. Taught in Recurrent inhibition.

**Reference circuit.** The level's own solution. It earns no stars.

**Researcher.** The hardest mode. No recipe, no hints. A pass needs all five worlds under physiological noise and tight bands, and earns the seal.

**Rheobase.** The smallest steady current that makes a cell fire. Taught in Threshold.

**Ring attractor.** Cells on a ring that excite neighbours and inhibit together hold a single bump of activity in place. Taught in Fly: a bump that stays.

## S

**Score stamp.** The ring on the score card: the score from 0 to 100, and PASSED once the level passes.

**Seal (Researcher seal).** A mark for passing all five worlds in Researcher mode. It is kept apart from the stars.

**Seed.** A number that picks the random parts of a run. The same seed gives the same run.

**Sensor.** An input card that the level drives.

**Size principle.** See recruitment.

**Solution slot.** One of three saved circuits per level (A, B, C), kept across restarts. More, Save to A and Load A.

**Sparse coding.** Each Kenyon cell listens to a few inputs and fires only when several are active. A giant inhibitory neuron keeps the number of active cells down. An odour becomes a small subset of cells. Taught in Fly: a sparse odour code.

**Spike.** The short electrical pulse a neuron fires.

**Spike-triggered average (STA).** The average of the input just before each spike.

**Spine (campaign).** The main path of a chapter. Levels on the spine open in order.

**Spread.** The percentage by which each synapse's weight varies around the value you set.

**Spurious states.** A network that stores several patterns can fall into mixtures never stored. Taught in Two memories, no blends.

**Stable fixed point (phasic firing).** If the resting state is still stable at the new current, a kicked cell falls back and falls silent although the current stays on. It reports change. Taught in Only the change counts.

**Standard.** The default mode. The recipe is behind a button that costs the third star. Hints open after two missed runs.

**Subtraction.** Excitation from one source and inhibition from another give a cell that reports the difference. Taught in Subtraction.

**Synapse.** The junction where one neuron's spike acts on the next.

**Synaptic depression.** A synapse that is used hard releases less each time and refills slowly. Taught in Aplysia: getting used to a touch.

## T

**Test x5.** The button that runs the full episode live on the same five worlds as Check. Pass all five for the second star.

**Threshold.** A neuron fires when its voltage reaches threshold. Taught in Threshold.

**Timed bursts (pattern generator).** A chain of cells firing in a fixed order for fixed durations makes a movement with no sensory input. Taught in Timed bursts.

**Travelling wave.** Excitation behind and inhibition ahead make activity move in one direction along a chain of segments, as in gut peristalsis. Taught in Travelling wave.

**Tsodyks synapse.** The depressing and facilitating synapse model (`tsodyks2_synapse`) used in the Aplysia levels.

**Tuning curve.** Many sensory neurons fire most for one value of a stimulus and less away from it. Taught in A cell tuned by its inputs.

## V

**Variant.** A challenge version of a level: half the neurons, double noise, a heavier load. It gives no stars and never gates anything.

**Verdict.** The sentence that says what your circuit did and which part of the mechanism is missing.

## W

**Weight.** The strength of a connection. In pA for current-based cells, in nS for conductance-based ones.

**Wilson-Cowan pair.** A population described by one number, its rate. An excitatory population and an inhibitory one it drives can settle, switch or oscillate. Taught in Two populations that make a rhythm.

**Wing.** One of the three groups of chapters on the campaign map: Body, Bestiary and Theory.

## Units used in the game

| Unit | Meaning |
|---|---|
| mV | Millivolt, the voltage of a cell. |
| ms | Millisecond, a thousandth of a second. |
| pA | Picoampere, a current. Weights of current-based synapses. |
| nS | Nanosiemens, a conductance. |
| Hz | Hertz, events per second. |
| ATP | The cell's energy currency. |
