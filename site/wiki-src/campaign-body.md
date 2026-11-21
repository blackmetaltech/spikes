---
title: The body chapters
section: The campaign
order: 30
summary: Every level of the five body chapters (Invertebrates, Spinal cord, Heart, Breathing, Autonomic): what it teaches, what you need to know first, the real biology and the goal, with no solutions.
---

# The body chapters

The campaign has five body chapters. Each one is an organ on the campaign map, and each one climbs from a single cell to a circuit that drives a simulated body: a worm, a limb, a heart, lungs, an eye and a gut. This page lists every level in play order. For the Bestiary (worm, leech, sea slug, fly compass, fly mushroom body, crab) see [the Bestiary](campaign-bestiary.html). For the four Theory chapters see [the Theory wing](campaign-theory.html).

Nothing here gives away a solution. Each level has a goal in plain words and, under a "Stuck?" heading, the same nudge the game gives as its first hint: the idea, never the wiring and never a number to type in. If you want more, the in-game [hint ladder](stars-and-hints.html) has two more tiers.

## How the campaign opens up

Levels unlock in a fixed order. The rules are short.

- **The first two chapters are open from the start.** That is Invertebrates and Spinal cord.
- **A later chapter opens when the previous chapter's spine is passed, or the spine of the chapter two before it.** The spine is the main line of levels in a chapter, in play order. So Heart opens when you pass all of Spinal cord (or all of Invertebrates). Breathing opens when you pass Heart or Spinal cord. One stuck level never blocks the whole game.
- **Inside a chapter, a spine level opens when the one before it is passed.** The first level opens with its chapter.
- **A branch opens when the level it hangs off is passed.** Branches are the diagnosis cases and the challenge variants (a harder or smaller version of a level). Branches never gate anything.
- **The Bestiary and Theory wings open when the Invertebrates spine is passed** and do not count toward the body chapters' unlock rule. Inside the Bestiary some chapters wait for an idea taught elsewhere: Sea slug needs the Neuromodulation level of Heart, Crab needs the Leech chapter, and Fly mushroom body needs the Sea slug. Every level card has a **NEEDS** row that lists the ideas the level uses and that you have not met yet, with the level that teaches each ("Neuromodulation (from Neuromodulation)"). Guided mode leaves the in-wing waits open and keeps the NEEDS row.
- **Posture and Reach are branches, not part of the Spinal cord spine.** No novice bench solves them from the brief alone, so the spinal sprint no longer waits for them; the boss follows the spine levels before it.

A level that you pass in any mode counts as passed for unlocking. Switching [mode](modes.html) never locks you out of anything.

On the map a locked level says what it waits for, for example "Pass Convergence first". The parts bin and the lab tools also open as the campaign needs them: inhibitory neurons, the parts bin and noise inputs appear at the first level whose reference circuit uses them, and Analysis and Load / Save appear from the second chapter. See [the lab](how-to-read-the-lab.html).

## The kinds of level

| Kind | What it is | Size |
|---|---|---|
| **Micro-puzzle** | One concept in a fish tank: 1 to 12 cells, a 3 to 5 second episode, a small dashboard of readouts instead of a body. | Par of 1 to 12 neurons |
| **Task** | The same ideas put to work on a body or a world. Many cells, a long episode (13 to 150 seconds), a body view. | Par of 7 to 56 neurons |
| **Boss** | A chapter finale that combines every idea of the chapter in one circuit. | Par of 75 to 80 neurons |
| **Diagnosis case** | A patient circuit with one dead connection. You do not build; you probe, then name the broken connection. A branch, not part of the spine. | Not applicable |

"Par" is the neuron count for the third star. It is explained in [Stars, par and the hint ladder](stars-and-hints.html). "Spiking" means the cells are NEST neuron models that emit discrete spikes; see [what is real](what-is-real.html) for what is the simulator's own maths and what is tuned for the game.

Every task offers "challenge variants" in its More menu: a smaller circuit ("half the neurons") and one harder world. They are listed per task below. Micro-puzzles and bosses have none. A passed variant has no stars of its own.

## The five chapters at a glance

| Chapter | Body | Levels (play order) | Boss | Diagnosis cases |
|---|---|---|---|---|
| Invertebrates | A touch-sensitive animal, a crawling worm, a swimming lamprey | 7 micros, 3 tasks (Threshold to Lamprey swimmer) | none | none |
| Spinal cord | A forearm with muscle spindles, tendon organs and two muscles | 5 micros, 4 tasks | Spinal sprint | the shaky arm |
| Heart | A heart with blood pressure | 4 micros, 3 tasks | none | the racing resting pulse, faint on standing |
| Breathing | Lungs and blood CO2 | 1 micro, 1 task | none | stops breathing in sleep |
| Autonomic | An eye and a gut, then the whole body | 2 micros, 3 tasks | Autonomic exam | the small pupil, food that will not pass |

In total that is 20 micro-puzzles, 14 tasks, 2 bosses and 6 diagnosis cases.

> **Note:** The counts of "levels in a chapter" above are spine levels in the order the game plays them, which is the order of the engine's registry. The map draws the same order.

---

# Chapter I: Invertebrates

*Reflexes, a swimming rhythm, a nose for salt. Small circuits, real behaviour.*

![The Invertebrates chapter on the campaign map](img/campaign-invertebrates.png)

You start with a single cell and finish with a swimming animal. The ideas are the foundation of everything after: when does a cell fire, how do cells add up, how does inhibition subtract, and how do two cells make a rhythm.

| Level | Kind | What it teaches | Know first | The real biology | Citation |
|---|---|---|---|---|---|
| Threshold | Micro | A cell ignores weak input and fires for strong input (rheobase). | Nothing | Lapicque measured in 1907 how much current excites a nerve; the weakest steady current that works is the rheobase. | Lapicque 1907 |
| Excitation: weight and delay | Micro | A synapse has a weight (whether a spike gets through) and a delay (when). | Threshold | A chemical synapse delays a spike by about 0.5 to 4 ms; most of it is the time for calcium to trigger release. | Katz and Miledi 1965 |
| Convergence | Micro | A cell adds up its inputs and can detect coincidence. | Threshold, Excitation | One spindle afferent gives a motoneuron about 0.1 mV; it needs about 10 mV to fire, so it fires only when many arrive together. | Mendell and Henneman 1971 |
| Reflex arc | Task | Averaging many noisy inputs to tell a real touch from chatter. | Convergence | The sea slug Aplysia withdraws its gill when its siphon is touched, through many weak sensory synapses. | Castellucci and Kandel 1974 |
| Inhibition | Micro | An inhibitory cell silences its target; a sensor can only excite. | Threshold, Excitation | Lateral inhibition in the horseshoe crab eye sharpens contrast. | Hartline, Wagner and Ratliff 1956 |
| Subtraction | Micro | Excitation minus inhibition on one cell gives a difference (ON minus OFF). | Inhibition, Convergence | The worm C. elegans has two salt neurons, ASEL and ASER, that respond in opposite ways. | Suzuki et al. 2008 |
| Worm chemotaxis | Task | Using the difference signal to steer toward salt. | Subtraction (and Inhibition) | The worm steers by comparing the two salt neurons while crawling a gradient. | Suzuki et al. 2008; Thiele et al. 2009 |
| Adaptation | Micro | A cell that starts fast and then slows under a steady drive. | Threshold | Cortical pyramidal cells fire a fast burst then settle at a lower rate. | Connors and Gutnick 1990; Brette and Gerstner 2005 |
| Tonic drive | Micro | A cell held just over threshold by a bias of its own hums at rest, so a cue can say "less". | Inhibition | Purkinje cells fire at about 50 spikes a second with no input. | Raman and Bean 1999 |
| Half-centre | Micro | Two cells that inhibit each other and tire take turns. | Inhibition, Adaptation | Brown proposed in 1911 that stepping in the cat spinal cord comes from two half-centres. | Brown 1911; Grillner 2003 |
| Lamprey swimmer | Task | A left-right rhythm that swims a body forward. | Half-centre | The lamprey spinal cord makes its swimming rhythm with two pools that inhibit each other. | Grillner 2003 |

### Threshold

*Micro. Par 1 neuron.*

A neuron does nothing until its input is strong enough. The smallest steady current that makes it fire is its rheobase (a word for the minimum current that works). Weaker input, however long it lasts, is ignored.

**Goal.** One sensor, "pulse", gives a small blip and later a big step, each a little different on every run. The current the cell receives is the weight of the sensor times the level of the pulse. Build one neuron that stays silent during the blip, fires at least 8 spikes in the step, and fires at no other time. The episode is 4 seconds.

**Stuck?** A neuron only fires when its input pushes the membrane past threshold. Below the rheobase nothing happens however long you wait. The knob you have is the weight of the sensor.

### Excitation: weight and delay

*Micro. Par 2 neurons.*

An excitatory synapse does two things. Its weight decides whether a spike gets through. Its delay decides when. The delay of the link adds to the time the second cell needs to reach threshold.

**Goal.** Five taps each make a first cell fire a spike or two, which you read on the channel "source". Build a second cell that repeats every one of those spikes 8 ms later, within 1.5 ms, and read it on "echo". You need echoes for at least 90 percent of the source spikes, a mean latency of 8 plus or minus 1.5 ms, and at most one echo with no source spike before it. The episode is 3 seconds.

**Stuck?** A synapse has two numbers. A lone spike through a weak synapse is a tiny nudge; the weight decides whether that is enough. The delay decides when the next cell answers.

### Convergence

*Micro. Par 1 neuron.*

A neuron adds up what converges on it and fires only when the sum is big enough. Each input is a current of its sensor's weight; the cell fires when the total goes over its rheobase.

**Goal.** Three inputs, in_a, in_b and in_c, switch on in different combinations: one alone, two together, all three together, and all three one after another. Build a neuron that fires only when all three are on at the same time (at least 3 spikes, both times it happens) and is silent for every other combination. The episode is 3.8 seconds.

**Stuck?** Each input on its own must be too weak to fire the cell, and even two must not be enough. Think about what the weight of each input has to be so that only the sum of all three crosses threshold.

### Reflex arc

*Task. Par 16 neurons. Episode 20 seconds.*

A touch receptor chatters in the background at about 150 spikes per second and fires a little faster, about 400, while something pokes the skin. A poke is therefore hard to tell from a noisy stretch of chatter.

**Goal.** Make the animal pull back within 60 ms of every poke, yet stay still when the receptor only chatters. Six pokes arrive at random times. A "withdrawal" is a burst of motor spikes: at least 3 within 25 ms, or half the pool if it has more than 6 cells. You are scored on speed, missed pokes and false withdrawals triggered by noise. The pass rule asks for no misses, no false trigger (Guided mode allows one), and a mean latency of 60 ms or less. A single cell cannot pass at any weight.

**Challenge variants.** Half the neurons (8 cells instead of 16). A poke half as strong against the chatter.

**Stuck?** A poke is only a little faster than the chatter, so one afferent cannot tell it from noise. The idea you learned in Convergence applies: a cell that needs several inputs at once.

### Inhibition

*Micro. Par 2 neurons.*

Excitation makes cells fire; inhibition makes them stop. A sensor can only excite, so a stop signal has to act through a neuron of its own that you mark inhibitory with the E / I switch.

**Goal.** A "drive" input keeps a cell firing from 0.4 to 3.4 seconds, and a "stop" input comes on for one second in the middle. Build a circuit in which "out" fires with the drive alone, goes quiet (at most 2 spikes) while stop is on, and fires again when stop ends.

**Stuck?** Inhibition is the opposite of excitation: the spikes of an inhibitory neuron lower the membrane of the cell they connect to. The stop signal must act through a neuron of its own that you set to inhibitory.

### Subtraction

*Micro. Par 2 neurons.*

Many sensory circuits compute a difference, ON minus OFF. Excitation from one source and inhibition from another meet on one cell, which reports the difference.

**Goal.** Two inputs, "plus" and "minus", take levels from 0 to 3. The output "diff" must fire only when plus is stronger than minus: not when both are 0, not when minus is on alone, not when the two are equal (3 against 3, or 2 against 2), but yes for 3 against 0 and 3 against 2. All three firing segments must be answered and all four quiet ones kept silent.

**Stuck?** Subtraction is excitation plus inhibition on one cell. The inhibition has to be about as strong as the excitation it cancels.

### Worm chemotaxis

*Task. Par 28 neurons. Episode 120 seconds.*

A worm crawls at constant speed over a plate with a hill of salt it loves, and starts out facing away from it. Chemotaxis means moving along a chemical gradient. Two sensor cells sit in its head: ASEL fires when the salt level is rising (things are getting better) and ASER fires when it is falling (worse). Two motor pools steer it: the turning speed is turn_left rate minus turn_right rate. Turning one way is enough, so you may leave the other channel unconnected.

**Goal.** Turn while things get worse, go straight while they get better. The catch: the plate is stirred, and now and then both cells chatter together for a second or so. That chatter means nothing; only the difference between the two does. Get within 10 percent of the plate radius of the peak within 45 seconds. You are scored on how fast, and how close you stay on average. A single cell listening to ASER alone is too slow and gets thrown off by the chatter.

**Challenge variants.** Half the neurons (14 instead of 28). Twice as much common salt chatter (up to 1200 Hz on both cells).

**Stuck?** A sensor that only counts spikes cannot steer. The worm needs a motor pool that turns it, and the steering signal is a difference between two cells.

### Adaptation

*Micro. Par 1 neuron.*

Hold a steady current on a plain integrate-and-fire cell and it fires at one fixed rate for ever. Real neurons slow down. This is spike-frequency adaptation: each spike leaves behind a slow current that holds the membrane back.

**Goal.** Build a cell whose rate in the first 100 ms of a step is at least 3.5 times its rate in the last second, and that still keeps firing (at least 3 spikes per second). The cell the lab opens on is the adaptive exponential one; two of its parameters decide how big the adaptation jump is and how slowly it fades.

**Stuck?** Adaptation is a slow current switched on by the neuron's own spikes. A plain integrate-and-fire cell has no such current, so look at which cell model you are using.

### Tonic drive

*Micro. Par 2 neurons.*

Most cells are silent until something excites them. A cell held just over its threshold by a steady current of its own, a bias, fires all the time at a low rate, and then it can signal "less" as well as "more". Posture, pupil size and the heart rate all rest on such a bias.

**Goal.** Build a cell that hums at rest, with nothing applied to it, between about 5 and 60 spikes a second. Then a "cue" arrives that means "stop": it must almost silence the cell while it lasts, and the hum must come back when it goes. The bias is a parameter of the cell itself (its advanced parameters hold the steady current I_e). The cue is a sensor, and a sensor can only excite, so it has to reach the humming cell through a neuron of its own that you set to inhibitory.

**Stuck?** A cell fires without input only if something keeps pushing it. The default cell has no bias, so it is silent at rest: the verdict says so.

### Half-centre

*Micro. Par 2 neurons.*

Swimming, walking and breathing are rhythms, and the simplest rhythm generator is two cells that inhibit each other and take turns. That is a half-centre.

**Goal.** A steady "drive" excites both sides equally. Build a left and a right cell, read on "left" and "right". The output must switch from one side to the other at least 6 times in 4 seconds, with both sides almost never firing together and neither side winning for good. Two plain cells that inhibit each other do not alternate: the first one to fire silences the other for ever. The lab opens on the plain cell, so finding the cell that tires is part of the puzzle.

**Stuck?** A half-centre needs mutual inhibition and a way for the active side to tire so it lets go. Without the second, one side wins for ever.

### Lamprey swimmer

*Task. Par 16 neurons. Episode 10 seconds.*

A lamprey swims by alternating left and right muscle bursts. A steady descending drive from the brain switches swimming on. The body makes a travelling wave and a simple reactive-thrust model pushes it along.

**Goal.** Both sides must burst (a rhythm on one side does not count). The swim frequency must be inside 1 to 10 Hz, left and right about 180 degrees out of phase, and the animal must make forward speed. You read left_motor and right_motor.

**Challenge variants.** Half the neurons (8 instead of 16). A sprint: swim at 0.5 body lengths per second or more.

**Stuck?** A swimming rhythm needs both sides to fire in turn. If one side is silent, the other has nothing to alternate with.

---

# Chapter II: Spinal cord

*Stretch, hold and reach: muscle spindles, motor neurons, Renshaw cells.*

![The Spinal cord chapter on the campaign map](img/campaign-spinal.png)

The spinal cord runs reflexes before the brain is involved. A motoneuron (a nerve cell that makes a muscle contract) is recruited, braked and timed. The body is a forearm with a joint, two muscles, muscle spindles (stretch sensors in the muscle) and tendon organs (force sensors). Sensory fibres from the spindle are called Ia afferents. See [bodies](bodies.html) for what the limb model does.

| Level | Kind | What it teaches | Know first | The real biology | Citation |
|---|---|---|---|---|---|
| Recruitment | Micro | Different thresholds grade force smoothly. | Threshold, Excitation | Small motoneurons fire first and big ones later: the size principle. | Henneman, Somjen and Carpenter 1965 |
| Reciprocal inhibition | Micro | The antagonist is switched off when the agonist is driven. | Excitation, Inhibition | Sherrington saw that when flexors are excited their antagonists relax. | Sherrington 1906; Lloyd 1946 |
| Stretch reflex | Task | A one-synapse reflex with reciprocal inhibition. | Reciprocal inhibition | The knee-jerk is monosynaptic: Ia fibres excite the same muscle's motoneurons directly. | Lloyd 1943; Eccles, Eccles and Lundberg 1957 |
| Recurrent inhibition | Micro | A Renshaw cell trims a motoneuron's own output. | Inhibition, Excitation | Renshaw found interneurons driven by the motoneurons' axon collaterals that inhibit them back. | Renshaw 1941; Eccles, Fatt and Koketsu 1954 |
| Renshaw cell | Task | The recurrent loop calms tremor. | Recurrent inhibition | Renshaw cells release glycine and GABA onto the pool and act as a gain control. | Renshaw 1941; Hultborn, Jankowska and Lindstrom 1971 |
| Negative feedback | Micro | A sensor that opposes the drive holds a value steady. | Inhibition, Excitation | Cannon's homeostasis: the body's variables are held steady by feedback. | Cannon 1929 |
| Posture | Task | A spindle servo holds a limb up against gravity and pushes. | Negative feedback | Gamma motoneurons set the spindles' sensitivity; the reflex works as a length servo. | Merton 1953; Matthews 1972; Hulliger 1984 |
| Timed bursts | Micro | A chain of cells and delays makes a movement pattern. | Excitation, Threshold | The triphasic pattern: agonist, antagonist, agonist again. | Hallett, Shahani and Young 1975 |
| Reach | Task | A triphasic burst moves the limb to a target. | Timed bursts | Fast accurate arm movements are made by a stereotyped triphasic burst. | Hallett et al. 1975; Berardelli et al. 1996 |
| Spinal sprint | Boss | Hold, resist, reach and hold again in one circuit. | Everything above | The reflex does not know about your plans: it resists a voluntary movement. | Feldman 1986; Hallett et al. 1975; Hultborn 2006 |
| Diagnosis: the shaky arm | Diagnosis | Find the dead connection behind a tremor. | Recurrent inhibition, Negative feedback | Reduced recurrent inhibition has been proposed as one contributor to spasticity. | Williams and Baker 2009; Hultborn 2006 |

### Recruitment

*Micro. Par 3 neurons.*

A muscle is made of motor units. A small motoneuron drives a few weak fibres and needs only a little input; a big one drives many strong fibres and needs more. The nervous system grades force by recruiting units from small to large. This is the size principle.

**Goal.** A "command" steps through three efforts (light, medium, maximal) and the muscle has three motor pools. The force must follow a staircase (0.10, 0.40, 1.00 within 0.09) and be zero at rest. With identical cells every pool switches on at the same effort, so the force jumps from 0 to 1. Give the three pools different thresholds. The ADVANCED section of a group holds a bias current that lowers or raises its threshold.

**Stuck?** The three pools must have different thresholds. Look at the ADVANCED bias current of each group.

### Reciprocal inhibition

*Micro. Par 3 neurons.*

Muscles come in antagonist pairs. When the flexor contracts, the extensor has to let go, or the joint just stiffens. The spindle afferent of the stretched muscle excites its own motoneurons and, through an inhibitory interneuron, silences its antagonist's.

**Goal.** Two pools hold a little tone of about 40 spikes per second. At about 1 second the flexor is stretched for 1 second. The flexor pool must speed up by at least 15 spikes per second while the extensor pool drops to a third of its resting rate or less, and the extensor must come back afterwards. The extensor has only a little drive above its threshold, so a little inhibition is enough.

**Stuck?** The extensor needs a route from the flexor's afferent that changes sign. Something between the sensor and the extensor pool has to be inhibitory.

### Stretch reflex

*Task. Par 54 neurons. Episode 13.5 seconds.*

Tap a tendon and the muscle twitches. The spindle fires Ia afferents that excite the same muscle's alpha motoneurons through a single synapse. The same fibre also feeds an inhibitory interneuron that silences the opposing muscle. First the tendons are tapped, one muscle at a time, to time the reflex. Then the forearm is pushed by four sudden loads, alternately toward extension and flexion.

**Goal.** The motor reply to a tap must come within 50 ms. The reflex must cut the deflection to well below what the muscles' own stiffness would allow. The opposing muscle must be switched off. The limb must settle without ringing (too much reflex gain oscillates, which is called clonus).

**Challenge variants.** Half the neurons (26 instead of 52). Heavy load: pushes of 3.5 N m instead of 2.5 N m.

**Stuck?** A tendon tap sends one brief volley down the Ia fibres. One synapse onto the motoneurons has to be strong enough to fire several cells at once.

### Recurrent inhibition

*Micro. Par 5 neurons.*

A motoneuron that is pushed harder should not simply fire faster and faster. Its axon sends a collateral to a small inhibitory interneuron, the Renshaw cell, which inhibits the motoneurons back. The harder they fire, the harder they are held in: negative feedback in one synapse pair.

**Goal.** A steady drive makes the motoneurons fire at about 50 spikes per second. At 1 second an extra volley adds to their input, and 1 second later it doubles. Without a brake the rate would climb by about 60 and 105 spikes per second. Keep the extra rate the kicks add under 27 and under 42 spikes per second, while still firing at least 15 spikes per second at the base drive. The Renshaw cells only fire if the loop is strong enough, and how strong depends on how many cells sum onto each other.

**Stuck?** Close a loop: the motoneuron group should feed a small inhibitory group, and that group should project back onto the motoneurons.

### Renshaw cell

*Task. Par 56 neurons. Episode 16.45 seconds.*

Add Renshaw cells to your stretch reflex. The cord is bathed in background synaptic noise that reaches both pools through shared fibres, which makes the pools fire in waves and the muscle force wobble. That wobble is tremor.

**Goal.** Hold the limb for six seconds, then take two sudden stretches. You are scored on how much your recurrent inhibition cuts the force tremor compared with the cord you would have without Renshaw cells. That control is a curve measured on the reference circuit and re-scaled for your circuit by how strongly your pools answer the common drive, which twelve short volleys on the noise input measure after the stretches. Shielding the pools from the noise cannot pass, and a cord without recurrent inhibition scores about zero. You need at least a quarter less tremor than the control while the reflex still resists the pushes and the limb does not ring. A cell that integrates a fast Poisson input fires far less often than its input and swallows the volleys, so the cells that carry the noise must repeat every spike they receive.

**Challenge variants.** Half the neurons (28 instead of 56). Strong tremor: the common drive is 1.5 times as deep (90 percent modulation).

**Stuck?** The motoneuron pools need tonic drive to fire before the feedback has anything to hold back.

### Negative feedback

*Micro. Par 4 neurons.*

To hold a value steady, measure it and push against any change. The sign matters: a signal that adds to the effort when the level is high makes things worse.

**Goal.** A motor pool "effort" lifts a level (a joint angle, a blood pressure) that must stay at 0.5 plus or minus 0.1. The drive on its own would carry it to about 1.0. At 1.1 seconds the level is pushed down by 0.3 for a second, later up by 0.3. It must stay inside the band both times. The sensor "level" reports the level. The loop needs gain: a weak link is not enough.

**Stuck?** The level sensor has to oppose the drive, so it must reach the effort cells through an inhibitory cell.

### Posture

*Task. Par 54 neurons. Episode 13.5 seconds.*

Holding an arm up is a servo (a system that measures and corrects its own error). Gravity and bumps stretch the muscles, the spindles report it and the reflex pulls back. The loop only works if the spindles are tuned: gamma motoneurons set their sensitivity, and a tonic descending drive sets how hard the antigravity muscle works.

**Goal.** The forearm is not supported. It weighs 1.5 kg and must be held 29 degrees above horizontal while six random pushes knock it either way. You are scored on the root-mean-square angle error, the worst error, the sag between pushes and the absence of ringing. Too much flexor tone is as bad as too little: a pool driven past its ceiling cannot lift the limb any further.

**Challenge variants.** Half the neurons (27 instead of 54). Strong pushes, 1.4 times as hard (1.4 to 2.5 N m).

**Stuck?** Negative feedback with conduction delays rings when its gain is too high.

### Timed bursts

*Micro. Par 12 neurons.*

A fast reach is not steered while it happens. The brain fires a stereotyped sequence of bursts, one to start, one to brake, one to hold, 100 to 200 ms apart, and the arm obeys. A pattern generator needs only a trigger and delays.

**Goal.** A brief "go" of 50 ms must release three bursts in order on b1, b2 and b3, each 100 to 220 ms after the previous one, and then silence. It happens twice, at about 0.6 and 2 seconds, and nothing may fire on its own before. The delay slider on a link reaches 250 ms. Cells stop firing when their input stops, so each burst ends by itself.

**Stuck?** Look at the links between the groups: each must be strong enough to fire the next group and carry a delay about as long as the pause you want between bursts.

### Reach

*Task. Par 55 neurons. Episode 13.3 seconds.*

Fast movements are not steered by feedback. The nervous system fires a triphasic burst: agonist, then antagonist, then agonist again. The first burst accelerates the limb, the antagonist burst brakes it, and a small final burst clamps it on target. Bigger reaches get bigger bursts.

**Goal.** A target appears with a "go" cue: go pulses for 40 ms and "target" stays on, scaled by the distance in radians, for 1.8 seconds. There are three reaches of 0.4, 0.7 and 0.5 rad. Reach the target within 700 ms of the cue, overshoot by less than 30 percent of the distance, finish within 0.15 rad, and do not creep before go. Keep the stretch reflex to hold the limb at the new angle. Real motoneurons differ in size, so a pool is recruited gradually; identical cells all start together and the creep and reach time then depend on the seed.

**Challenge variants.** Half the neurons (28 instead of 55). A quick reach: each target within 550 ms of the cue.

**Stuck?** A pattern generator must stay quiet until it is triggered: the pools should be nearly silent at rest.

### Spinal sprint (boss)

*Boss. Par 80 neurons. Episode 40 seconds.*

The spinal motor system does four jobs with the same few neurons. It holds the arm up against gravity, stands firm when pushed, reaches to a new target, and holds the new angle under more pushes. Nothing in the world is new: the arm is the one from Posture and the reach is the one from Reach. What is new is that every part must work with the others. The reflex that holds the arm against a push also resists the reach, and the burst must not set the arm ringing.

**Goal.** Score how well the arm is held (root-mean-square and worst error against the displayed target), how well it reaches (time, overshoot, final error; three reaches of 0.30, 0.40 and 0.25 rad), no ringing after a push, and no creeping before go. Pushes arrive before, during and after the reaches.

**Stuck?** A ringing arm is a feedback loop with too much gain for its delays. Ask what damps a motor pool from within.

### Diagnosis: the shaky arm

*Diagnosis. Hangs off Renshaw cell.*

A fictional patient: a 54-year-old man, a year after a cervical spinal cord injury, says his arm trembles when he holds it out. You are given the circuit of his motor pool: the stretch reflex and its Renshaw cells, with one connection dead.

**Findings.** Postural tremor at about 10 Hz that nothing damps. The motor units fire very fast, about 48 impulses per second per cell against about 30 normally, for the same effort. The stretch reflex is present and of nearly normal strength. Both flexor and extensor pools are involved.

**What to probe.** Open the scopes on the pools and compare what each source fires with what its target does. A dead connection is told apart by activity: a source that fires into a target that stays silent.

**Stuck?** Tremor that grows when a feedback brake is missing: which cells provide the brake on a motor pool, and what drives them?

> **Note:** This case is a model of the kind, not clinical clonus. The authors say in [what is real](what-is-real.html) that isolated loss of Renshaw inhibition is not a clinical cause of clonus. See [diagnosis cases](diagnosis-cases.html) for how all six cases are scored.

---

# Chapter III: Heart

*A pacemaker cell sets the beat; the autonomic nerves and the baroreflex steer it.*

![The Heart chapter on the campaign map](img/campaign-heart.png)

Here the cells are not integrate-and-fire. The sinoatrial (SA) node is a patch of cells in the right atrium that fire on their own and set the heartbeat. You use the SA-node pacemaker cell from [the parts bin](parts-bin.html). Two nerves steer it: the vagus (parasympathetic, releases acetylcholine, ACh, which slows the heart) and the sympathetic chain (releases noradrenaline, NE, which speeds it up). Those chemicals do not fire the next cell; they change how it behaves. That is neuromodulation, covered in [synapses and modulators](synapses-and-modulators.html). The body is a heart with blood pressure ([bodies](bodies.html)).

| Level | Kind | What it teaches | Know first | The real biology | Citation |
|---|---|---|---|---|---|
| Pacemaking | Micro | A cell that fires rhythmically with no input. | Threshold | A heart removed from the body keeps beating because SA node cells fire on their own. | DiFrancesco 1993; Jose and Collison 1970 |
| Coupling | Micro | Two clocks of different speed lock together. | Pacemaking, Excitation | SA node cells differ in rate but the whole node beats at a single rate. | Jalife 1984 |
| Sinoatrial pacemaker | Task | A coupled cluster agrees on one rhythm. | Pacemaking, Coupling | Coupled pacemaker cells; the intrinsic rate is about 100 per minute. | Jose and Collison 1970; Michaels, Matyas and Jalife 1987 |
| Feed-forward and feedback | Micro | Acting on a cue before the error appears. | Negative feedback, Excitation | Central command raises breathing and heart rate within seconds of starting exercise. | Krogh and Lindhard 1913 |
| Neuromodulation | Micro | A chemical level changes how a cell behaves. | Pacemaking, Excitation | Loewi showed the vagus slows a frog heart through a released chemical. | Loewi 1921 |
| Autonomic heart rate | Task | Two nerves, one node: follow a target heart rate. | Neuromodulation, Feed-forward | The node is slowed by vagal ACh and sped up by sympathetic NE. | Jose and Collison 1970; Levy 1971 |
| Baroreflex | Task | Hold blood pressure through the brainstem. | Negative feedback, Neuromodulation | Baroreceptors in the carotid sinus and aortic arch fire faster when pressure rises. | Kirchheim 1976; Guyenet 2006 |
| Diagnosis: the racing resting pulse | Diagnosis | Find the dead connection behind a fast resting pulse. | Neuromodulation, Negative feedback | Vagal withdrawal and resting tachycardia. | Jose and Collison 1970; Robinson et al. 1966 |
| Diagnosis: faint on standing | Diagnosis | Find the dead connection behind low pressure on standing. | Negative feedback, Inhibition | Neurogenic orthostatic hypotension. | Freeman et al. 2011; Guyenet 2006 |

The order the game plays them in is: Pacemaking, Coupling, Sinoatrial pacemaker, Feed-forward and feedback, Neuromodulation, Autonomic heart rate, Baroreflex.

### Pacemaking

*Micro. Par 1 neuron.*

Most neurons are silent until excited. A pacemaker fires rhythmically all by itself from the slow currents of its own membrane.

**Goal.** There is no input at all. Build a cell and wire it to "beat" so that it beats on its own, regularly, between 50 and 140 times a minute, with beat intervals varying by no more than 15 percent. The lab opens on a plain cell that stays silent. A cell you have to push, even with a constant bias current, is not a pacemaker. The parts bin holds a cell designed for the job.

**Stuck?** Pacemaking is rhythmic firing with no input: slow inward currents bring the membrane back to threshold again and again. Most cells have none of that and need a push.

### Coupling

*Micro. Par 2 neurons.*

SA node cells do not all beat at the same speed, yet the node beats as one. Cells that influence each other lock their phases.

**Goal.** Two pacemaker cells, "a" (10 percent faster) and "b" (10 percent slower), beat on their own. Build one cell for each channel and couple them so that they lock: one-for-one beating, with the delay between them changing by no more than 12 percent of a beat over the last 3.5 seconds. Real cells couple through gap junctions (tiny electrical channels); here excitatory synapses play that part.

**Stuck?** Two oscillators of different speed lock when each pulls the other. The stronger the coupling, the more the faster one is held back and the slower one is sped up, until they share one rhythm.

### Sinoatrial pacemaker

*Task. Par 7 neurons. Episode 30 seconds. Runs the heart body.*

A slow inward "funny" current depolarises each cell until calcium channels open and it fires one beat. Each spike read out on sa_beat is a heartbeat.

**Goal.** The beat must come at 60 to 80 per minute and be regular (beat-interval variation under 5 percent). A cell alone runs at about 100 per minute; the resting vagal tone (ACh) that the task supplies slows it to the resting range. Real node cells are not identical (each gets its own size, so its own rate), so a cluster has to agree on one rhythm. Uncoupled copies drift apart and fail, and one cell alone is not a node: the level needs a coupled cluster. The tighter the synchrony, the higher the score.

**Challenge variants.** Half the neurons (4 instead of 7). Cells differ twice as much in size (24 percent spread in membrane capacitance): coupling must work harder.

**Stuck?** Pacemaking: some cells fire on their own, without any input. A leaky integrate-and-fire cell stays silent.

### Feed-forward and feedback

*Micro. Par 4 neurons.*

Feedback alone is always a little late: it reacts after the error has appeared. A body that is warned can act first. That is feed-forward.

**Goal.** A level (heart rate, say) has to stay at 0.5 plus or minus 0.1 while a workload pulls it down by 0.5. The effector is slow, so plain feedback lets the level sag by about 0.3. The first workload is announced by the sensor "cue" a quarter of a second ahead; the second, at 3.5 seconds, is not. Keep the level within 0.23 of the target during the announced workload and inside the band at the end of both. Too little cue leaves the dip; too much overshoots.

**Stuck?** You need two routes into the effort cells: the level through an inhibitory cell (feedback), and the cue directly (feed-forward).

### Neuromodulation

*Micro. Par 2 neurons.*

Not every nerve works through a synapse. The vagus releases acetylcholine near the heart, and the chemical, not a spike, slows the pacemaker.

**Goal.** A "vagus" signal comes on at 1.7 seconds. Build a cell the vagus drives, wire it to the "vagal" output (its spikes release ACh) and put a pacemaker cell on "beat". Make the heart slow by at least 25 percent, but not stop. The pacemaker needs no wiring from your vagus cell: it follows the ACh level by itself. You are scored on the beat interval at the end compared with the one before the vagus came on.

**Stuck?** A neuromodulator changes how cells behave, slowly and over a whole region, instead of delivering one spike to one cell. Wire a cell that the vagus drives to the "vagal" output, which turns its spikes into ACh.

### Autonomic heart rate

*Task. Par 45 neurons. Episode 85 seconds. Runs the heart body.*

The SA node has its own rate of about 100 per minute with no autonomic input. Vagal neurons slow it; sympathetic neurons speed it up. Each branch is a two-neuron chain: a preganglionic cell in the brainstem or spinal cord drives a postganglionic ganglion cell that releases the transmitter.

**Goal.** Build both branches and the SA node and make the heart follow a target: about 65 at rest, up to 120 in exercise, and back to 65 in recovery. The three inputs are stand-ins, not real sensors: "exercise_cue" is central command, and "hr_too_low" and "hr_too_high" hand you the sign of the heart-rate error directly. Cutting the vagus should show up as resting tachycardia (about 100); cutting the sympathetic chain as a heart that cannot speed up. Too much ACh slows the node a lot, so keep the vagal drive moderate. You are scored on mean tracking error and the average rate in each phase.

**Challenge variants.** Half the neurons (23 instead of 45). Tight tracking: mean error 4 bpm, rest and recovery within 3, exercise within 6.

**Stuck?** Neuromodulation: the heart has two transmitters, ACh from the vagus and NE from the sympathetic chain, each released by its own group of cells.

### Baroreflex

*Task. Par 48 neurons. Episode 140 seconds. Dashboard view with blood pressure.*

Standing up pools blood in the legs and pressure falls; a haemorrhage (blood loss) lowers it further. Baroreceptors fire faster the more their wall is stretched, and the brainstem turns that into a counter-move within seconds. The afferents excite the nucleus of the solitary tract (NTS). The NTS drives two limbs: vagal slowing of the heart, and, through an inhibitory relay, withdrawal of the tonic drive to the sympathetic neurons. High pressure must reduce sympathetic output and raise vagal output; low pressure the reverse.

**Goal.** Include the SA node so the heart beats. Keep mean arterial pressure at 90 plus or minus 10 mmHg through standing, lying down and a blood loss of 300 ml.

**Challenge variants.** Half the neurons (24 instead of 48). Severe haemorrhage: the blood loss runs 1.15 times as fast (345 ml instead of 300).

**Stuck?** The reflex has two limbs, both started by the baroreceptor relay in the brainstem: vagal slowing of the heart, and withdrawal of sympathetic drive through an inhibitory relay.

### Diagnosis: the racing resting pulse

*Diagnosis. Hangs off Autonomic heart rate.*

A fictional patient: a 47-year-old woman has a resting pulse of about 100 per minute that nothing seems to slow. You get the circuit that controls her heart rate through the autonomic nerves (rest, exercise, recovery).

**Findings.** Regular pulse near 100 per minute at rest with almost no beat-to-beat variability. Exercise raises it to about 130, but afterwards it does not fall back below 100. An injection of atropine, which blocks the vagus, changes nothing.

**What to probe.** Which branch should be setting the resting rate, and what does the pacemaker do when that branch is silent? Compare each branch's activity with what its target does.

**Stuck?** Which branch of the autonomic system sets the resting heart rate, and what happens to the pacemaker without it?

### Diagnosis: faint on standing

*Diagnosis. Hangs off Baroreflex.*

A fictional patient: a 68-year-old man sees stars and nearly faints every time he stands. The examiner stands him up at 15 seconds, lays him down at 60 seconds and later takes blood.

**Findings.** Blood pressure is low even lying down (about 68 mmHg mean) and falls further, to the mid-50s, on standing and after blood loss. His heart and arteries are healthy, and the baroreceptors report the fall normally.

**What to probe.** The reflex still sees the fall, so follow the signal from the receptors toward the vessels and look for the point where the command stops.

**Stuck?** Low pressure with a reflex that still sees the fall: where could the command to the vessels be lost?

---

# Chapter IV: Breathing

*A bursting pacemaker network in the brainstem drives the diaphragm, tuned by CO2.*

![The Breathing chapter on the campaign map](img/campaign-breathing.png)

This is the shortest chapter: one micro-puzzle and one task. The key idea is that breathing is a rhythm that a network makes by itself and that the chemical state of the blood tunes. The body is a pair of lungs with blood CO2 ([bodies](bodies.html)). The brainstem network is the preBotzinger complex, a small cluster in the medulla whose cells burst on their own.

| Level | Kind | What it teaches | Know first | The real biology | Citation |
|---|---|---|---|---|---|
| Rhythm and gating | Micro | A bursting cell makes a rhythm and a chemical signal tunes its speed. | Pacemaking, Excitation | The preBotzinger complex contains neurons that burst rhythmically by themselves. | Smith et al. 1991; Guyenet, Stornetta and Bayliss 2010 |
| Breathing rhythm | Task | A network rhythm drives the diaphragm and CO2 closes the loop. | Rhythm and gating | Persistent-sodium bursters excite each other into a shared burst; chemoreceptors report CO2. | Smith et al. 1991; Feldman and Del Negro 2006 |
| Diagnosis: stops breathing in sleep | Diagnosis | Find the dead connection behind a silent CO2 response. | Rhythm and gating, Negative feedback | Congenital central hypoventilation: breathing fails in sleep. | Amiel et al. 2003; Weese-Mayer et al. 2010 |

### Rhythm and gating

*Micro. Par 1 neuron.*

Breathing is a rhythm made by a cell that bursts on its own, and the body's chemical state decides how fast it runs. When CO2 rises, chemoreceptors raise the drive and the bursts come faster. In this tank the rhythm is ten times faster than a breath, so a run fits in five seconds.

**Goal.** Build one bursting cell read out on "phrenic" and feed it the sensor "chemo" (the CO2 signal: level 1.0 at rest, 1.6 from about 2.5 seconds). At rest it must burst between 5 and 14 times a second (bursts of two spikes or more, not a steady train), and after the CO2 rise at least 1.4 times faster. Use the part called "Izhikevich chattering" as a single cell: a pool of identical cells would burst out of step and smear the rhythm. A bursting cell only bursts inside a window of drive, and too much drive makes it fire continuously.

**Stuck?** The sensor's weight sets the drive. A bursting cell has a window of drive in which it bursts, and speeds up inside it. Look at the weight of the "chemo" sensor.

### Breathing rhythm

*Task. Par 26 neurons. Episode 150 seconds. Runs the lungs body.*

A network of cells that burst on their own and excite each other makes a shared burst every few seconds. The bursts excite phrenic motoneurons, the phrenic nerve drives the diaphragm and the lungs fill. Blood CO2 closes the loop.

**Goal.** 12 to 20 breaths per minute at rest. When the air gets 5 percent CO2 (50 to 110 seconds) ventilation must rise at least 1.6 times and arterial CO2 must stay below 52 mmHg. It must come back afterwards.

**Challenge variants.** Half the neurons (13 instead of 26). A CO2 challenge 1.15 times as strong.

**Stuck?** Breathing comes from a network whose cells excite one another into a shared burst, and CO2 must act on that network, not only on the motoneurons.

### Diagnosis: stops breathing in sleep

*Diagnosis. Hangs off Breathing rhythm.*

A fictional patient: a newborn breathes well awake and crying but becomes blue and stops breathing when she falls asleep. The monitor shows CO2 rising without any attempt to breathe harder. The circuit is her brainstem breathing rhythm in sleep, driven by a steady background drive and a chemoreceptor signal; the examiner raises the CO2 in the air from 50 to 110 seconds.

**Findings.** Slow, shallow breathing (about 8 to 10 per minute) with a high CO2. Breathing CO2-enriched air hardly increases ventilation. The rhythm itself is regular and each breath has normal shape.

**What to probe.** The rhythm is fine; ask whether the CO2 information is reaching it.

**Stuck?** The rhythm is fine but the CO2 signal is not being used: where does the chemoreceptor information enter the circuit?

---

# Chapter V: Autonomic

*Pupil, gut and the whole fight-or-flight response: two opposing nerves, one body.*

![The Autonomic chapter on the campaign map](img/campaign-autonomic.png)

The last body chapter puts two opposing nerve systems into one body. The parasympathetic side (rest and digest) and the sympathetic side (fight or flight) pull the same organs in opposite directions. You meet the pupil (an eye view), the gut and then a whole day's control. Bodies are described in [bodies](bodies.html).

| Level | Kind | What it teaches | Know first | The real biology | Citation |
|---|---|---|---|---|---|
| Pupil light reflex | Task | A two-muscle servo with a fast and a slow nerve. | Negative feedback, Inhibition | Light closes the pupil via pretectum, Edinger-Westphal nucleus and ciliary ganglion; darkness opens it by the sympathetic chain. | McDougal and Gamlin 2015; Loewenfeld 1993 |
| Travelling wave | Micro | Excitation behind and inhibition ahead moves things along. | Excitation, Inhibition | The law of the intestine: the gut contracts above a bolus and relaxes below it. | Bayliss and Starling 1899 |
| Peristalsis | Task | A chain of segments moves a bolus down a gut. | Travelling wave | The enteric nervous system runs two interneuron chains; interstitial cells of Cajal set a slow wave. | Bayliss and Starling 1899; Furness 2012 |
| Divergent command | Micro | One command, many effectors, each with its own strength and sign. | Excitation, Inhibition | Cannon: fear makes the sympathetic nervous system act as a unit. | Cannon 1915 |
| Fight or flight | Task | One threat cue reshapes heart, vessels, pupil and breathing. | Divergent command | The hypothalamus and brainstem fan out to sympathetic ganglia while the parasympathetic side is switched off. | Cannon 1915; Dampney 1994 |
| Autonomic exam | Boss | A day in two minutes: rest, stand, exercise, fright, recovery. | Everything in the chapter | Central command resets the baroreflex during exercise. | Raven, Fadel and Ogoh 2006; Rowell and O'Leary 1990; Dampney 1994 |
| Diagnosis: the small pupil | Diagnosis | Find the dead connection behind a small pupil. | Negative feedback, Inhibition | Interruption of the sympathetic chain (Horner syndrome). | Kanagalingam and Miller 2015 |
| Diagnosis: food that will not pass | Diagnosis | Find the dead connection behind slow propulsion. | Inhibition, Travelling wave | Loss of inhibitory (nitrergic) neurons in the gut wall. | Goyal and Chaudhury 2010; Pandolfino and Gawron 2015 |

In play order: Pupil light reflex, Travelling wave, Peristalsis, Divergent command, Fight or flight, Autonomic exam.

### Pupil light reflex

*Task. Par 30 neurons. Episode 40 seconds. Runs the eye body.*

The pupil is a two-muscle servo. Light on the retina fires ganglion cells that excite the pretectum, which excites the Edinger-Westphal nucleus; its neurons relay in the ciliary ganglion to the iris sphincter and squeeze the pupil shut. That is the parasympathetic pathway, and it is fast. In the dark, and when frightened, the sympathetic chain drives the radial dilator and the pupil opens. That is slower. Light also switches the sympathetic side off.

**Goal.** Wire "retina" to the reflex arc and "threat" to the sympathetic arm, and read out "sphincter" and "dilator". A step to bright light must constrict the pupil by 2.5 mm or more within about a quarter of a second, the return to dark must open it again, and a fright cue must dilate it.

**Challenge variants.** Half the neurons (15 instead of 30). A weak retina: three retinal fibres instead of four.

**Stuck?** The pupil is a two-muscle servo: a parasympathetic pathway closes it and a sympathetic pathway opens it. Each needs its own chain of cells.

### Travelling wave

*Micro. Par 4 neurons.*

Food moves along the gut because the gut squeezes behind a bolus (a lump of food) and relaxes in front of it. Excitation runs up the tube, inhibition runs down, so the push always points the same way.

**Goal.** A short tube has three segments, seg0 (oral, toward the mouth) to seg2 (aboral, away from it). Each holds a little tone of about 40 spikes per second. A bolus presses on segment 1, then later on segment 2. While it presses on segment 1, seg0 must contract (at least 15 spikes per second above its tone) and seg2 must relax (to a third of its tone or less). While it presses on segment 2 the wave has moved on: seg1 contracts and seg0 no longer contracts. At the end every segment must be back to its tone. Get the polarity right: contracting the segment below the bolus would squeeze it back.

**Stuck?** The segment below the bolus must be relaxed by the stimulus. Look at that segment: it needs an inhibitory route.

### Peristalsis

*Task. Par 49 neurons. Episode 40 seconds. Runs the gut body.*

The gut moves food by itself. When a bolus stretches the wall, sensory neurons of the enteric nervous system (the gut's own nerve network) fire, and two interneuron chains answer: an ascending one excites motoneurons just behind the bolus, which release ACh and contract, and a descending one excites inhibitory motoneurons ahead, which release nitric oxide and VIP and relax. Interstitial cells of Cajal run a slow wave of 12 per minute that decides when the muscle can contract.

**Goal.** Stretch sensors dist0 to dist7 feed interneurons that drive eight excitatory motor pools ach0 to ach7. That alone gives half of the law of the intestine and is not enough. The bolus must reach the end of the tube within 25 seconds. You may also add your own slow-wave pacemakers on the icc outputs, which then replace the built-in clock.

**Challenge variants.** Half the neurons (25 instead of 49). Fast transit: the bolus must arrive within 15 seconds.

**Stuck?** Travelling wave: each segment's stretch must excite the muscle just behind the bolus, so the squeeze moves the food on. Then ask what the muscle ahead should be doing.

### Divergent command

*Micro. Par 5 neurons.*

When something frightening appears the body does not decide organ by organ. One command fans out to many effectors at once, each with its own strength and sign.

**Goal.** A threat cue "alarm" arrives at about 1 second for 1.2 seconds. Four effectors listen. The heart must go from silence to at least 100 spikes per second. The pupil and the sweat glands go to a gentler 15 to 80 spikes per second. All three start within 100 ms of each other. The gut, which holds a little tone of about 40 spikes per second, must fall to a third of that or less. Everything must return to rest after the cue. A sensor has one weight, so it drives every cell it touches equally; you need something in between to give each effector its own strength.

**Stuck?** The effectors need different strengths and one the opposite sign. A command group with a separate weight to each target does that.

### Fight or flight

*Task. Par 45 neurons. Episode 100 seconds. Dashboard of heart, pressure, pupil and breathing.*

A threat turns the body from rest-and-digest to fight-or-flight. The hypothalamus drives the sympathetic premotor cells of the brainstem; they excite the sympathetic ganglia, which release noradrenaline on the heart's pacemaker (faster, stronger beats), tighten the vessels, open the pupil and raise ventilation. At the same time the threat switches off the parasympathetic side: the cardiac vagus and the Edinger-Westphal nucleus. When the threat ends the parasympathetic side must take over again.

**Goal.** Wire "threat" into the circuit and read out the pacemaker (sa_beat), the two heart modulators (sympathetic and vagal: set both outputs to kind modulator), sym_vascular, dilator, sphincter and vent. For each of heart rate, pressure, pupil and ventilation: the resting value must lie in its band, there must be a clear rise during the 30 second threat, and a return to rest within 50 seconds after it.

**Challenge variants.** Half the neurons (23 instead of 45). A faint threat cue (125 Hz instead of 250 Hz): the command must be amplified.

**Stuck?** Divergent command: one cue must fan out from one command group to every effector, each through its own relay.

### Autonomic exam (boss)

*Boss. Par 75 neurons. Episode 120 seconds. Dashboard view.*

A day in the life in two minutes: lie at rest, stand up, exercise, get frightened, calm down. One brainstem circuit has to hold heart rate, blood pressure, pupil and breathing right through all of it, using only what you learned in this chapter and the Heart chapter. Standing pools blood in the legs, so the baroreceptors must raise sympathetic output and drop the vagus. Exercise brings central command ("exercise_cue") and a CO2 load: the heart must speed up, pressure rise a little and ventilation follow the chemoreceptors and the command. A threat opens the pupil and raises heart rate, pressure and breathing at once. The baroreflex that holds pressure when you stand would fight the rise in exercise and fright, so the central signals must also turn the reflex down.

**Goal.** Read out sa_beat, the two modulators, dilator, sphincter and vent. The score covers five phases (rest, stand, exercise, threat, recovery) across the four organs, 19 conditions in all. Each is a soft score and the task passes when all hold.

**Stuck?** The exam needs every part of the chapter in one circuit: the baroreflex, the central command, the defence reaction and the chemoreflex.

### Diagnosis: the small pupil

*Diagnosis. Hangs off Pupil light reflex.*

A fictional patient: a 61-year-old smoker notices that one pupil is smaller than the other, and the upper lid droops a little on that side. The examiner flashes a bright light, darkens the room, and finally startles the patient.

**Findings.** The pupil is small: in the dark it opens only to about 5 mm instead of 7, and a sudden fright adds less than it should (about 1.7 mm instead of 2.9). It still reacts briskly to a bright light (to about 2.6 mm in a quarter of a second). The light reflex itself is normal.

**What to probe.** The light reflex is fine, so compare the two iris muscles' drive in the dark and under fright.

**Stuck?** A small pupil that still reacts to light: which of the two iris muscles has lost its drive?

### Diagnosis: food that will not pass

*Diagnosis. Hangs off Peristalsis.*

A fictional patient: a 45-year-old man says swallowed food sticks and has to be washed down, and sometimes comes back up. A bolus is placed in the tube and the enteric nervous system must move it along.

**Findings.** The bolus does move, but very slowly: the muscle ahead of it tightens instead of relaxing, so the passage takes more than 30 seconds where the normal gut needs 15. The segments do sense the bolus, and the slow waves are present.

**What to probe.** Follow the activity from the stretch sensors to the motor pools, behind the bolus and ahead of it, and look for a source that fires into a silent target.

**Stuck?** Propulsion needs the gut to contract behind the bolus and relax ahead of it. Which half is failing?

---

## About the numbers on this page

Episode lengths, pass thresholds and par counts come from the game's engine. Weights, delays and pass bands are tuned in silico: they are properties of the game, not measurements. Some levels use a real figure for illustration (the touch-reflex level uses cat spindle data for its arithmetic) and say so in their notebook entry. Read [what is real](what-is-real.html) for the full list.

## Where to go next

- [The core loop](core-loop.html) explains build, run, check and the verdicts.
- [Stars, par and the hint ladder](stars-and-hints.html) explains the stars and the three hint tiers.
- [Diagnosis cases](diagnosis-cases.html) explains how the six cases here (and the Bestiary ones) are scored.
- [The notebook](notebook.html) collects the cited note for every level you pass.
- [The Bestiary](campaign-bestiary.html) and [the Theory wing](campaign-theory.html) are the other two wings.
