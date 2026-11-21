---
title: Bodies
section: Reference
order: 53
summary: The bodies your circuits drive: limb, heart and circulation, lungs, eye, gut, lamprey, worm and the ring and dashboard views. What each one models, what you drive and read, and what is simplified.
---

# Bodies

A circuit alone is just spikes. In Spike each level plugs it into a **body**: a small physical model that turns your motor spikes into a movement, a pressure or a pupil, and turns the body's state back into a sensor signal. You wire neurons, the body moves, and the body's state comes back to you through the sensors. This is a **closed loop**: what the body does changes what the sensors say, which changes what the circuit does.

This page describes each body: what it models, what you drive, what you read, and what is simplified. See [inputs and outputs](inputs-and-outputs.html) for the cards that connect circuit and body, and [what is real](what-is-real.html) for how honest each model is.

## What all bodies have in common

- **No NEST in the body.** The neurons run in NEST (see [the parts bin](parts-bin.html)). The bodies are plain C++ code that takes rates or spike counts in and gives forces, pressures and sensor rates out.
- **Deterministic.** The same inputs give the same result on every machine. Where a body uses random numbers, they come from a seeded generator, not from the operating system.
- **One-millisecond slices.** The circuit and the body advance together, 1 ms at a time. Muscles and joints sub-step inside a slice for stability.
- **Textbook rest.** Each body is tuned so that, with no circuit at all, it sits near a textbook resting value: a mean arterial pressure of 93 mmHg, a pupil of a few millimetres, a normal breath.
- **Structure from the literature, numbers illustrative.** The shapes are tested against known facts (a Hill force-velocity curve, the size principle, a Windkessel pressure decay, a single-compartment lung). The coefficients are chosen so the model is plausible and fun, not fitted to a particular person.
- **Units.** Lengths are normalised to the muscle's optimal length, pressures are in mmHg (cmH2O in the lung's mechanics), volumes in ml (litres for the lung), time in ms inside a step and in seconds elsewhere.

## Overview

| Body | Used by | You drive it with | You read from it | Chapter |
|---|---|---|---|---|
| Limb: joint, muscle pair, spindles, tendon organs | Stretch reflex, Renshaw, Posture, Reach, Spinal sprint | Motor pools `ag_motor`, `ant_motor`, `gamma_motor` | Spindle and tendon-organ afferents as `dc` sensors | Spinal cord |
| Heart and circulation | Sinoatrial pacemaker, Autonomic heart rate, Baroreflex, Fight or flight, Autonomic exam | `sa_beat` spikes, vagal ACh, sympathetic NE | Baroreceptor rate, heart rate error signals | Heart |
| Lungs and CO2 | Breathing rhythm, Fight or flight, Autonomic exam | `phrenic` pool | Chemoreceptor rates (carotid body, central) | Breathing |
| Eye | Pupil light reflex, Fight or flight, Autonomic exam | `sphincter`, `dilator` pools | Retinal rate | Autonomic |
| Gut | Peristalsis | `ach`, `no`, `icc` pools of eight segments | Distension of each segment | Autonomic |
| Lamprey swimmer | Lamprey swimmer | `left_motor`, `right_motor` | Nothing: an open command | Invertebrates |
| Worm on a salt plate | Worm chemotaxis | `turn_left`, `turn_right` | `ASEL`, `ASER` salt cells | Invertebrates |
| Touch reflex limb | Reflex arc | `withdraw` pool | Touch Poisson input | Invertebrates |
| Specimens, rings, dials and dashboards | Bestiary and Theory levels, micro-puzzles | varies | varies | Bestiary, Theory |

![The limb view: a joint with its flexor and extensor muscles and spindle readouts](img/bodies-limb.png)

## The limb (Spinal cord chapter)

**What it models.** One hinge joint like an elbow, with a **flexor** (bends it) and an **extensor** (straightens it), each a muscle. A load can hang on it and a push can be applied from outside. The muscles are Hill-type muscles made of a pool of **motor units**. A motor unit is one motor neuron and the muscle fibres it controls. Units are ordered from small and slow to large and fast.

**What happens in a slice.**

1. The joint's sensors report the state of the muscles. A **muscle spindle** has two kinds of afferent: **Ia** (sensitive to stretch and to how fast it changes) and **II** (sensitive to length only). A **Golgi tendon organ** reports force as **Ib**.
2. The level writes those rates to the sensor cards as `dc` inputs, after about 10 ms of conduction delay. The circuit's input weight turns one hertz into about 4 pA.
3. Your motor pools fire. Their mean rate, smoothed over 15 ms and delayed 10 ms for the efferent conduction, becomes the excitation of the muscle.
4. The muscle recruits its units from small to large (the **size principle**), produces force, and moves the joint.

**The muscle.** A muscle is a pool of 20 motor units by default. Each has a twitch, a short pulse of force that rises and falls. Twitches add up. A single twitch reaches about 28 % of a fully fused contraction (a **tetanus**), so slow units fuse at about 10 Hz and fast units at about 31 Hz. Force follows a bell-shaped length curve and a Hill hyperbola in velocity, so a muscle pulls weakest when stretched or shortened a lot and when it shortens fast. Fast units tire about fifteen times faster than slow ones.

**Recruitment.** Unit i is recruited at a threshold that rises from 2 % to 80 % of the pool's excitation. A unit fires at a rate of 8 Hz plus 50 Hz per unit of excitation above its threshold, capped at 35 to 45 Hz. A cell in your motor pool is therefore one unit of a muscle with a limited range.

> **Note:** In the spinal levels the muscle's excitation is the pool's **total** rate (cells times mean rate) divided by what 20 units can use at their peak. One motor neuron at the top rate excites one twentieth of the muscle. The top rate differs from level to level (50 Hz by default, more or less in some levels), so the same pool can drive a muscle harder in one level than in another. **Pool size is part of every spinal design.**

**The joint.** It moves between -1.0 and 1.45 radians. Zero is horizontal. Positive is flexion (the limb raised). The forearm-like defaults are a mass of 1.5 kg, an inertia of 0.07 kg m^2, 350 N of peak force per muscle and a 4 cm moment arm. Stops at the ends are a stiff spring and damper. The tests show that a limp limb sags to the lower stop, that the resting angle rises with flexor drive, that co-contraction cuts the push-induced deflection (0.21 to 0.15 rad) and that the limits hold.

**The sensors.** A spindle's Ia rate follows a power law of velocity with exponent 0.6 plus length, with half gain on release (shortening), and a **gamma** motor drive adds fusimotor gain. Defaults: 40 Hz baseline, up to 250 Hz. Spindle II is length only, up to 120 Hz. The tendon organ's Ib is about 120 Hz times a saturating function of force. The coefficients are rescaled from cat data and are illustrative. The tests only check the shapes: a ramp stretch raises Ia far more than II, a shortened antagonist's Ia falls, and Ib rises with force.

**What is simplified.** Rigid tendon (no series elasticity). No pennation. No doublet potentiation. A single hinge with constant moment arm. The motor pool does not deliver spikes unit by unit: the muscle makes its own regular unit trains from the excitation. The coefficients are game values.

**The view.** A drawn arm on a trunk, with the flexor and the extensor as muscle bellies coloured by activation, spindle readouts and a ghost target where the level has one.

## The heart and circulation (Heart chapter)

**What it models.** A closed loop of blood in two compartments: an **arterial compartment** that stores pressure and a **venous** compartment that holds the reserve. The arterial side is a two-element **Windkessel**: an elastic reservoir (compliance 1.3 ml per mmHg) that drains through a resistance (1.06 mmHg s per ml). The decay of pressure between beats has a time constant of about 1.4 s. The heart pushes a stroke volume out as a half-sine of ejection.

**What you drive.** Each spike of the `sa_beat` pool is a request for a beat. The heart merges requests closer than 250 ms into one, so a cluster of pacemaker cells that agrees makes one beat per cycle and a cluster that disagrees makes extra, irregular beats. Vagal ACh and sympathetic NE act on the pacemaker cells through the modulation table (see [synapses and modulators](synapses-and-modulators.html#neuromodulation)). The sympathetic drive also changes the heart's contractility and, in some levels, the vessels' resistance.

**What it reports.** Heart rate, mean arterial pressure, systolic and diastolic pressure, stroke volume, cardiac output and central venous pressure. At rest in the tests: mean pressure 93 mmHg (112/76), heart rate 70 beats per minute, stroke volume 72 ml, cardiac output 5.0 L per minute.

**Stroke volume.** It depends on how full the heart is (Frank-Starling), on contractility, and on how long the heart has to fill. Faster beats leave less time to fill, so stroke volume falls from 71 to 47 ml as the rate rises from 70 to 120 per minute, while output and pressure still rise.

**Challenges.** Standing pools up to 220 ml of blood in the legs (time constant 6 s), so venous return and pressure fall. Without a reflex the mean pressure falls from 93 to 73 mmHg in the test. Haemorrhage removes volume from the venous side: 300 ml takes pressure from 93 to 63 mmHg with no reflex, a deliberately severe, game-readable fall.

**The baroreceptor.** The pressure sensor is a sigmoid of pressure plus 0.04 s times its rate of change, centred on 100 mmHg with a width of 20 mmHg and a top rate of 100 Hz: nearly silent below about 50 mmHg and nearly saturated above about 150. At equal pressure the rising limb fires more than the falling limb. The level feeds the one-second average rate to the sensor. It is the whole nerve's rate, not a single fibre.

**The intrinsic rate.** If nothing drives the heart, the module's built-in pacemaker beats at 105 plus 80 times the sympathetic level minus 85 times the vagal level, in beats per minute. A resting tone of 0.2 sympathetic and 0.6 vagal gives 70 beats per minute. In the game the pacemaker is a circuit of cells, not this formula: the formula is the fallback for tests.

**What is simplified.** No valves, chambers or afterload dependence. No nonlinear compliance. No respiratory pump. A lumped venous reservoir. In the heart-rate task the "too low" and "too high" error channels are **stand-ins**: the task hands the circuit the sign of its own error in place of baroreceptor afferents, and says so in the brief.

**The view.** An anatomical four-chamber heart that contracts on the real beat times (the atria, then the ventricles after a delay), a flashing sinoatrial node, nerves that light with ACh and NE, numbers for rate, pressure and stroke volume, and an ECG drawn from the beat times. In the baroreflex, fight-or-flight and autonomic-exam levels the main view is a dashboard of gauges instead.

## Lungs and carbon dioxide (Breathing chapter)

**What it models.** A single-compartment lung (Otis, 1956): airway resistance 3 cmH2O s per litre and compliance 0.1 L per cmH2O, so the lung's time constant is 0.3 s. The resting lung volume is 2.4 L. The diaphragm pushes with up to 20 cmH2O, smoothed over 80 ms. Expiration is passive (an exponential return); there is no active expiration.

**What you drive.** The mean rate of the `phrenic` motor pool, divided by a reference rate, is the inspiratory drive from 0 to 1. At rest a drive of about 0.25 for 1.5 s every 5 s gives a tidal volume of 0.50 L, 12 breaths per minute and 5.9 L per minute of ventilation. A drive of 1 gives about 2 L.

**Gas exchange.** Two gas pools, arterial and lung (4 L) and tissue and brain (15 L), exchange carbon dioxide. At steady state the arterial pressure of CO2 follows the alveolar gas equation: with a CO2 production of 0.2 L per minute and an alveolar ventilation of 4.2 L per minute, it sits at 41 mmHg, and tissue at 46. Dead space is 0.15 L of each breath, so alveolar ventilation is `(tidal volume - 0.15 L) times rate`. In the game, gas exchange runs several times faster than real physiology so a challenge fits in a minute or two. Steady states are unchanged. Apnoea raises CO2 by about 11 mmHg per minute (real values are 3 to 6).

**The challenge.** In the breathing level, 5 % CO2 is added to the inspired air from 50 to 110 s. Without a reflex the arterial CO2 climbs toward 71 mmHg.

**Chemoreceptors.** Two afferents come back to you. The **peripheral** one (carotid body) is fast and reads arterial CO2: about 0.7 plus 0.45 times (CO2 minus 35) hertz, about 3.5 Hz at rest. The **central** one (brainstem) is slow and reads tissue CO2: 1.2 times (tissue CO2 minus 38) hertz, about 9.6 Hz at rest. The level feeds their sum, scaled, to the `chemo` sensor as a graded current, a smooth stand-in for the population code.

**What is simplified.** No oxygen, pH or buffering (hypoxia only sensitises the peripheral receptor). No airway reflexes, no stretch receptors (the Hering-Breuer reflex), no active expiration. The chemoreflex gain is a game value.

**The view.** Two lungs with their bronchial tree inflating with volume, the diaphragm dome flattening as it contracts, the phrenic nerves lit by the drive, numbers for breath rate and CO2, and a volume trace.

## The eye (Autonomic chapter)

**What it models.** The **pupil**: two antagonist iris muscles, each with a first-order smooth-muscle response behind a 40 ms delay. The **sphincter** (parasympathetic) constricts, with time constants of 130 ms up and 500 ms down. The **dilator** (sympathetic) widens, with 800 ms up and 300 ms down. The diameter is 2 mm plus 6 mm times a function that is 0.5 when both muscles are silent, so a denervated pupil is mid-range. So constriction is faster than redilation, as in people.

**What you drive.** The `sphincter` pool (the ciliary ganglion, parasympathetic) and the `dilator` pool (the superior cervical ganglion, sympathetic). The mean rate of each pool divided by 40 Hz is that muscle's drive from 0 to 1. Both outputs must come from separate pools: one pool read by both is one muscle, not two.

**What it reports.** The **retina**. The illuminance on the retina is the luminance times the pupil's area (in trolands). The rate is a Naka-Rushton function of the log of that: about 5 to 100 Hz after a 30 ms latency. A smaller pupil lowers the signal. This is the negative feedback the light reflex closes. In the level, four fibres carry the rate as Poisson spikes.

**The light schedule.** Dim light (1 cd per m^2) until 4 s, bright (200) from 4 s, dim again from 11 s, medium (30) from 20 s. A fright cue runs from 30 to 34 s.

**Test results.** A step from dim to bright in the module's own reflex test gives a pupil of 7.4 mm falling to 3.1 mm with a latency of 228 ms (people: 200 to 250 ms). In the game the arc, and so the latency and the gain, is your circuit.

**What is simplified.** No hippus (the small pupil oscillations), no consensual reflex, no near response, no accommodation. The dilator is not inhibited by light inside the module: your circuit must do that.

**The view.** An iris with radial dilator fibres (amber, sympathetic) and a circular sphincter at the pupil margin (cyan, parasympathetic), light rays and a glow at the left.

## The gut (Autonomic chapter)

**What it models.** A tube of smooth-muscle **segments** and a **bolus**, a lump of food that sits in the tube. In the peristalsis level there are 8 segments, from segment 0 (the mouth end, **oral**) to segment 7 (the **aboral** end). The bolus is 1.2 segments long and starts at position 1.0. It must reach 7.0.

**The law of the intestine.** Bayliss and Starling (1899): when the gut wall is stretched, it contracts on the oral side and relaxes on the aboral side. That pushes the bolus on. In the model a bolus only moves by a **pressure difference** between its two sides. A uniform field pushes nothing. So uniform contraction does not move it, and neither does uniform relaxation. Only a **gradient** does. The tests show a wave moving aboral at one segment per second carries the bolus 4.8 segments in 5 s, uniform contraction moves it 0.000, and a reversed wave moves it the other way.

**What you drive.** Three pools per segment:

- `ach0` to `ach7`: excitatory motor neurons (ACh and substance P). Their mean rate divided by 45 Hz is the excitation of the segment, smoothed over 300 ms. Required.
- `no0` to `no7`: inhibitory motor neurons (NO and VIP). Their rate divided by 50 Hz relaxes the segment. Optional, but without them there is no relaxation ahead of the bolus, which makes a simpler, slower gut that the level does not accept as the mechanism.
- `icc0` to `icc7`: the pacemaker cells (the interstitial cells of Cajal). Optional. If you declare none, the gut runs its own built-in slow wave of 12 per minute. If you declare them, **they are the clock**: silence them and nothing contracts.

**What it reports.** The `dist0` to `dist7` sensors: the distension of segment i is a Poisson rate of 500 Hz times how much the bolus overlaps that segment.

**Contraction.** A segment contracts when its membrane drive, excitation minus inhibition plus a slow-wave term, passes a threshold of 0.6. Contraction rises with a time constant of 500 ms and falls with 800 ms. Luminal pressure is 10 cmH2O times contraction. Slow waves alone stay below threshold (maximum contraction 0.04): they gate and time the contractions, they do not make them. With a small ACh drive (0.35) they gate phasic contractions that travel aboral.

**What is simplified.** A one-dimensional plug-flow bolus with static friction (0.3 cmH2O). No mixing, no segmentation motility, no viscosity. The module has 12 segments by default; the level uses 8. The ICC cell is a phenomenological oscillator.

**The view.** A bowel segment as a tube with eight circular bands that constrict behind the bolus and relax ahead of it, coloured by ACh (amber) and NO (cyan).

## The lamprey swimmer (Invertebrates chapter)

**What it models.** A swimming body of 10 segments, head to tail. It is the one body here that is explicitly a **toy**.

**What you drive.** The `left_motor` and `right_motor` pools. The muscle command is the difference of their rates, divided by 40 Hz, smoothed over 40 ms and clamped to the range -1 to 1.

**What happens.** Segment i bends by 0.12 rad times the command, delayed by 36 ms per segment. That is a **travelling wave** along the body, at about 2.8 body lengths per second. The bend of the tail produces thrust through a simple reactive model, in the spirit of Lighthill but explicitly not his formula, and drag slows the animal. Steering comes from a left-right bias: the heading changes by 2 per second times the mean bend.

**The sensor.** There is a `drive` sensor that stands for the brainstem's swim command at 100 %. No signal returns from the body, the command is open-loop.

**What the level measures.** Between 2.5 and 10 s, the rhythm and speed. See [campaign-body](campaign-body.html) for the goal.

**What is simplified.** Everything about the water. The neural rhythm is real, a half-centre oscillator (Grillner, Brown). The hydrodynamics and the segment delay are game values.

**The view.** The lamprey swimming across a water plate with a wake.

## The worm on a salt plate (Invertebrates chapter)

**What it models.** A *C. elegans* worm crawling on a disc-shaped plate, 10 mm in radius, with a Gaussian hill of salt whose peak is somewhere within 3 mm of the centre. The worm moves forward at a constant 0.3 mm per second. It starts about 5.5 mm from the peak and facing away from it. Heading noise makes it wander a little. A head sensor sits 0.3 mm ahead of the centre of the body.

**What you read.** Two salt cells at the head. **ASEL** (the "ON" cell) fires when the salt concentration rises. **ASER** (the "OFF" cell) fires when it falls. Both are Poisson inputs with a 5 Hz background and up to 400 Hz in response to change. The sensor reads the *relative* change (the log of the concentration), as the real cells do. The plate is stirred, so from time to time both cells fire extra spikes together for about a second. That chatter carries no information.

**What you drive.** The `turn_left` and `turn_right` pools. The turning rate is 1.5 rad per second times the difference of their rates divided by 40 Hz, clamped to the range -1 to 1.

**What the level measures.** Whether the head gets within 1 mm of the peak within 45 s, in a run of 120 s.

**What is simplified.** A constant speed and a point head. The sensory cells here spike while the real ones are graded. Time is the real time.

**The view.** A stippled petri dish with a salt gradient and the worm drawn as an inked ribbon, two bars for the salt cells and the distance to the peak over time.

## The touch reflex limb (Invertebrates chapter)

The **reflex arc** level has no physical body model. The world is six pokes in a 20 s episode, each 200 ms long, and the withdraw pool's bursts are counted in a 25 ms window. The view draws a limb that pulls back when a burst is declared. It is a visualisation of the score, not a simulation of a limb.

## Specimen views (the Bestiary)

Every Bestiary level, and its diagnosis case, shows its animal as a living specimen. The still engraving is cut from the same drawing as the chapter's plate on the campaign map. What moves on it is drawn live from the running circuit only: the motor channels' rates, the spikes of their cells, the sensors' values and the level's own stimulus schedule. Nothing moves that the simulation does not move. Each view also keeps the level's judged quantities as compact gauges, and works in the phone layout (a short view drops the captions).

| Chapter | What the specimen shows |
|---|---|
| Worm | *C. elegans* crawling forward or backing up with the motor output, with the touch cells lit where it is touched |
| Leech | The two heart tubes beating with their side, and the heart interneurons' bursts as a pen record with the period and phase |
| Sea slug | The gill withdrawing in proportion to the motor cell's firing, with the siphon touch and the tail shock marked and the withdrawal at every touch |
| Fly compass | The ellipsoid-body ring as 16 wedges washed by their cells' firing, the bump as a needle, the landmark while the lights are on, and a fly turning with its true heading. The numbers beside it give the decoded heading, the true heading and the error |
| Fly mushroom body | The Kenyon cells as a field of points (which fire for odour A and which for odour B), the output cells and the dopamine cells |
| Crab | The stomatogastric ganglion magnified with its cells lit, and the triphasic rhythm as a pen record with phase marks |

## Rings, dials and dashboards (Theory levels and the rest)

The remaining levels have no body in the sense above. They have a **scripted world** and a view built for its idea:

| View | Levels | What it draws |
|---|---|---|
| **Dashboard** (default) | Micro-puzzles, baroreflex, fight or flight, autonomic exam | Gauges for each judged trace (the arc is the tolerance band, the needle is the value, green inside and red outside, with a sparkline of the last 20 s), tiles for other readouts, and a banner naming the scripted event that is on now |
| **Compass** | Fly compass chapter (a specimen view, see above) | A ring of 16 wedges lit by the compass cells' firing, a needle for the bump (its length is how sharp the bump is), a mark on the rim for the true heading and another for the landmark while the lights are on. Angles run clockwise from the top |
| **Phase plane** | Dynamics chapter | The two nullclines of the first cell the motor channel reads, its fixed points (filled stable, hollow unstable, a cross for a saddle), the reset, and a live trajectory. It is computed by the engine from the cell's own parameters and the current it gets now |
| **Rates** | Rate models chapter | Scrolling traces of the motor channels on one shared scale |
| **Memory** | Attractor memory chapter | Arrays of the cells the motor channels read, each filling as it fires, so a recalled pattern fills in. The stored pattern is framed by a dashed outline and a cell that a sensor reaches carries a tick |
| **Tuning** | Tuning chapter | A dial for the stimulus angle, bars for each cell's response and a single cell's tuning curve |

**The fly compass world.** A landmark sets the heading, then the lights go out and the fly turns. The 16-cell ring must form a bump at the landmark, hold it in the dark, and follow the turns by adding up the angular velocity. The landmark is 300 Hz on cue sensors, one per ring cell. The turn is two `dc` sensors: a tonic level of 40 units on both, and while turning the side of the turn gets 0.25 units per degree per second more and the other side as much less. Very slow turns, below about 60 degrees per second, fall inside the dead zone of a discrete ring and are not part of the world. The full episode is 9.2 seconds, Check runs the first 5 seconds. (The first two puzzles of the chapter use their own, simpler turn sensors.)

**The other scripted worlds.** The Aplysia chapter has a gill withdrawal, the mushroom body chapter has odour patterns and a punishment, the crab chapter has a pyloric rhythm. They are described in [campaign-bestiary](campaign-bestiary.html). The Theory worlds are in [campaign-theory](campaign-theory.html).

## Honest limits

- The bodies are **illustrative**: structure from the literature, coefficients chosen for plausibility. No body reproduces a particular person or animal.
- The efferent coupling (pool rate to muscle drive) and the afferent gains (organ state to sensor rate) are tuned in the game, not measured.
- Task worlds are built as puzzles of a few seconds (the Bestiary and micro-puzzles) up to a few minutes (the longest body levels). They follow the cited experiments in kind, not in detail.
- Several cells in the Bestiary spike where real ones signal in a graded way. See [what is real](what-is-real.html).

## Related pages

- [Inputs and outputs](inputs-and-outputs.html): the cards between your circuit and these bodies.
- [How to read the lab](how-to-read-the-lab.html): where the body view sits on screen.
- [The body chapters](campaign-body.html): every level that uses a body.
- [Glossary](glossary.html): plain-language definitions of the concepts used here.
