# Roadmap 0.8: Senses and movement

Overview and shared rules: [ROADMAP.md](ROADMAP.md). Research for five of these circuits already exists in [research/bestiary-b.md](../research/bestiary-b.md) (locust, zebrafish, barn owl, zebra finch, cat).

## Idea
The body wing gave the circuit a body to run. This wing gives it a world to sense and a skilled movement to make: an eye that sees motion, two ears that find a sound, an object on a collision course, an eye that must stay on target while the head turns, a choice between two actions, a song. Each chapter is a classic result, and the target is the shape of that result.

## What is new
- **Stimulus worlds:** a small screen (bars, gratings, an expanding disc), two ears with an adjustable time difference, a head that turns.
- **Replicate the figure:** the level shows the outline of a published curve (a tuning curve, a response against time to collision) and scores whether the circuit's own curve has that shape. The notebook gives the paper.
- **Instruments:** a receptive-field mapper (extends the spike-triggered average of 0.5 to space and time); the state-space view on motor cortex and on the eye-position integrator; sound output for the song chapter.

## Chapters: senses
| Chapter | Levels | Concept | What is scored | Sources (verified when the level lands) |
|---|---|---|---|---|
| **Retina** | `s_surround`, `s_direction`, task `s_bar` | centre-surround by lateral inhibition; direction selectivity by asymmetric, delayed inhibition | a response to a spot and not to full-field light; a response to motion one way and not the other; report the direction of a moving bar | Kuffler 1953; Barlow and Levick 1965; Briggman 2011 |
| **Seeing motion, fly** (uses Atlas wiring) | `s_correlate`, task `s_t4` | the delay-and-correlate detector; the same computation in measured T4 wiring | direction selectivity from a delay and a coincidence; assign signs and time constants on the real inputs of T4 so that it is direction selective | Hassenstein and Reichardt 1956; Takemura 2017 |
| **Where is the sound** (barn owl) | `s_coincide`, `s_delayline`, task `s_owl` | coincidence detection; axonal delay lines; a place code for time difference | a cell that fires only for coincident inputs; a row of cells tuned to different time differences; turn the head toward the sound | Jeffress 1948; Carr and Konishi 1990 |
| **Collision** (locust, zebrafish) | `s_loom`, task `s_escape` | a looming detector whose peak depends on size over speed; a fast left-or-right escape with feed-forward inhibition | the response peaks at a fixed angular size across approach speeds; escape away from the stimulus, once, on the correct side | Gabbiani 1999; Korn and Faber 2005 |

## Chapters: movement
| Chapter | Levels | Concept | What is scored | Sources (verified when the level lands) |
|---|---|---|---|---|
| **Cerebellum** | `c_filter`, `c_timing`, task `c_vor` | supervised learning at the parallel-fibre synapse gated by climbing-fibre error; learned timing | an error signal reduces the response to the paired input; a response that anticipates a stimulus at the trained interval; keep the eye on target as the head turns, and re-adapt when the gain is changed | Marr 1969; Albus 1971; Medina and Mauk 2000 |
| **Choosing an action** (basal ganglia) | `bg_gate`, `bg_stop`, task `bg_learn` | selection by disinhibition; direct and indirect pathways; dopamine as a reward-prediction error | exactly one of two competing actions is released; a stop signal cancels it; learn from reward which action to take, and re-learn on reversal | Albin 1989; Gurney 2001; Schultz 1997; Frank 2004 |
| **Holding the eye** | `o_integrate`, task `o_hold` | a line attractor that integrates velocity commands into position | eye position holds after a brief command; drift appears when the feedback is mistuned | Seung 1996; Aksay 2001 |
| **Song** (zebra finch) | `v_chain`, task `v_song` | a synfire chain that fires each cell once per song; timing by a chain, pattern by its targets | sparse, ordered bursts with stable timing; the circuit sings a given motif, heard through the game's audio | Hahnloser 2002; Long 2010 |
| **Reaching** (motor cortex) | `x_prepare`, boss `boss_reach` | preparation without movement; rotational population dynamics during the reach | activity changes during the delay and the arm stays still; the population trajectory rotates and the arm reaches the cued target | Churchland 2012; Georgopoulos 1986 |

About 24 levels. Diagnosis cases: one per chapter where a lesion has a clear signature (surround loss, one ear's delay line cut, climbing fibre silenced, dopamine depleted, integrator mistuned, a broken link in the chain). The dopamine case is described as a circuit lesion and makes no clinical claim.

## Engine and tool work (add-only)
| Need | Plan |
|---|---|
| Screen stimuli | input kind `screen`: a small grid of photoreceptor drives, with scripted stimuli (spot, bar, grating, expanding disc) |
| Sub-millisecond timing for the owl | the research note gives the problem: time differences of tens to hundreds of microseconds against NEST's resolution; options are a finer resolution for this chapter or a time-scaled world, with the scaling stated on the card |
| Head and eye world | extends `body/eye` with head turns and retinal slip as the error signal |
| Error-gated plasticity | the dopamine-gated learning of 0.5 with the climbing fibre as the gate; a test that the sign and the timing window are right |
| Reward and reversal | task schedule with a reward signal that depends on the action taken; reuses the phases of 0.7 |
| Curve-shape scoring | `analysis/`: measure a response curve over a stimulus sweep and compare its shape (order, peak position, width class) with the declared target; no numbers in the verdict |
| Sound | the syrinx body maps bursts to notes through the existing audio layer |

## Iterations
| # | Scope | Exit criteria | Status |
|---|---|---|---|
| S1 | Barn owl chapter (research exists; decides the timing question) | the place code for time difference holds on seeds 101 to 105; the scaling, if any, is stated | planned |
| S2 | Screen input, receptive-field mapper; retina and fly-motion chapters | curve-shape scoring in use; T4 level uses Atlas wiring | planned |
| S3 | Collision chapter | size-over-speed dependence measured by the engine | planned |
| S4 | Cerebellum and eye-holding chapters | re-adaptation after a gain change passes on held-out seeds | planned |
| S5 | Basal ganglia chapter | reversal learning passes on held-out seeds; the lesioned pathway fails | planned |
| S6 | Song chapter with audio | timing stability scored; the motif is audible | planned |
| S7 | Motor cortex and the boss, cases, map plate, wiki, release 0.8 | full CI; release | planned |

## Risks
| Risk | Answer |
|---|---|
| Too many chapters for one release | the wing can ship in two halves (senses, then movement) |
| Cortex and basal ganglia models are simplified far below the anatomy | each card says what is left out; the checks are on the mechanism named |
| Learning across seeds | as in 0.7: held-out-seed tests from the first level |
