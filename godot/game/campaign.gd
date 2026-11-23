class_name Campaign
extends RefCounted
## The 0.2 campaign: five chapters, each an organ on the anatomical map, each a handful of task nodes. The engine decides which tasks exist and which
## chapter each belongs to (Task::chapter()); this file only knows the chapters, the planned lineup from docs/REQ-0.2.md section C (so the map shows
## "not installed yet" ghosts where a task has not landed), and where each node sits on its organ. Unknown tasks from the engine are placed after
## the planned ones of their chapter, so new tasks appear on the map without a UI change.
##
## v0.3 (docs/DESIGN-0.3.md section 4): each chapter is a SPINE of levels in play order (micro-puzzles, tasks, a boss; Task::level_kind / on_spine) with optional
## BRANCHES hanging off a spine task (challenge variants from Task::variants(), diagnosis cases). A chapter opens when the previous chapter's spine is
## passed; the first two chapters are open from the start; branches never gate anything. A spine node opens when the spine node before it is passed; a
## branch opens when the task it hangs off is passed. Node ids: a level is its task id; a variant branch is "<task>#<variant>".
##
## Dev hooks:  SP_MOCK=1 adds synthetic tasks (game/mock_task.gd) so the body views can be looked at before the engine has them
##             SP_MOCK03=1 adds synthetic v0.3 levels (micro-puzzles, bosses, diagnosis, variants, notebook entries) and v0.3 facts on the real tasks, so the
##             map and the notebook can be looked at before the content agents have landed theirs (ORDER03 / DEFS03 / OVER03 in game/mock_task.gd)
##             SP_UNLOCK_ALL=1 opens every node and every part.

const CHAPTERS := [
	{"id": "invertebrates", "title": "Invertebrates", "organ": "worm", "blurb": "Reflexes, a swimming rhythm, a nose for salt. Small circuits, real behaviour."},
	{"id": "spinal", "title": "Spinal cord", "organ": "spine", "blurb": "Stretch, hold and reach: muscle spindles, motor neurons, Renshaw cells."},
	{"id": "heart", "title": "Heart", "organ": "heart", "blurb": "A pacemaker cell sets the beat; the autonomic nerves and the baroreflex steer it."},
	{"id": "breathing", "title": "Breathing", "organ": "lungs", "blurb": "A short chapter on purpose: one micro-puzzle, one task and a diagnosis case. A bursting pacemaker network in the brainstem drives the diaphragm, tuned by CO2; the autonomic exam reuses it."},
	{"id": "autonomic", "title": "Autonomic", "organ": "autonomic", "blurb": "Pupil, gut and the whole fight-or-flight response: two opposing nerves, one body."},
	# v0.5 Bestiary wing (docs/DESIGN-0.5.md): one chapter per model organism. Opens after the chapter I spine (WING_GATES says what each waits for), never counted by the unlock rule of the body chapters. No plate art yet (organ "bestiary").
	{"id": "worm", "title": "Bestiary: Worm", "organ": "bestiary", "wing": "bestiary", "blurb": "C. elegans: touch the head and it backs up, touch the tail and it goes. The command switch, wired from the real touch circuit."},
	{"id": "leech", "title": "Bestiary: Leech", "organ": "bestiary", "wing": "bestiary", "blurb": "The leech heartbeat: two cells that burst in turn, a half-centre with a slow cycle."},
	{"id": "aplysia", "title": "Bestiary: Sea slug", "organ": "bestiary", "wing": "bestiary", "blurb": "Aplysia: a reflex that habituates, recovers and is sensitised, all in one synapse."},
	{"id": "fly_compass", "title": "Bestiary: Fly compass", "organ": "bestiary", "wing": "bestiary", "blurb": "Drosophila: a bump of activity on a ring of compass cells holds the heading in the dark and follows the fly's turns. After Turner-Evans/Hulse cell types, not synapse-level data."},
	{"id": "fly_mb", "title": "Bestiary: Fly mushroom body", "organ": "bestiary", "wing": "bestiary", "blurb": "Drosophila: a sparse odour code, and a dopamine-gated depression that teaches the fly to avoid a punished odour. After the published cell types, not synapse-level data."},
	{"id": "crab", "title": "Bestiary: Crab", "organ": "bestiary", "wing": "bestiary", "blurb": "The crab stomatogastric ganglion: a conductance-based pacemaker kernel and two rebound followers make the triphasic pyloric rhythm, and many different parameter sets give the same pattern."},
	# v0.6 Theory wing (docs/DESIGN-THEORY.md): what a graduate comp-neuro course teaches. Opens after the chapter I spine like the Bestiary.
	{"id": "dynamics", "title": "Theory: Dynamics", "organ": "bestiary", "wing": "theory", "blurb": "The phase plane: nullclines, fixed points and what happens to the resting state as the current grows. Class I and class II firing, a cell that reports only change, and a single neuron that remembers a pulse."},
	{"id": "rates", "title": "Theory: Rate models", "organ": "bestiary", "wing": "theory", "blurb": "A population as one number: the Wilson-Cowan excitatory-inhibitory pair that oscillates, two populations that compete, and the mean field of a spiking network, so you can see when the shortcut holds."},
	{"id": "memory", "title": "Theory: Attractor memory", "organ": "bestiary", "wing": "theory", "blurb": "Hopfield's idea in spiking cells: a partial cue recalls a whole pattern, two patterns without blends, and a pattern written by spike-timing-dependent plasticity during an exposure."},
	{"id": "tuning", "title": "Theory: Tuning and decoding", "organ": "bestiary", "wing": "theory", "blurb": "Cells tuned to an orientation or a direction: a tuned cell built by convergence, tuning sharpened by inhibition, and a direction decoded from a whole population with the population vector."},
	# v0.6 Atlas wing (docs/ROADMAP-CONNECTOME.md): real wiring is given and the player reads it. Opens after the chapter I spine like the other wings.
	{"id": "a_worm", "title": "Reading wiring", "organ": "bestiary", "wing": "atlas", "blurb": "The measured touch circuit of the worm: find the shortest route from a touch on the head to the backward muscles and test it by silencing, test a pattern in the wiring against chance, and bet on a lesion before you make it."}]

## Planned lineup per chapter: [id, title, node centre on the map (0..1, 0..1)].
const PLANNED := {
	"invertebrates": [["reflex", "Touch reflex", Vector2(0.17, 0.66)], ["worm", "Salt-seeking worm", Vector2(0.5, 0.34)], ["lamprey", "Swimming lamprey", Vector2(0.83, 0.66)]],
	"spinal": [["stretch_reflex", "Stretch reflex", Vector2(0.15, 0.3)], ["renshaw", "Renshaw inhibition", Vector2(0.38, 0.7)], ["posture", "Posture", Vector2(0.62, 0.3)], ["reach", "Reach", Vector2(0.85, 0.7)]],
	"heart": [["heart_pacemaker", "Heart pacemaker", Vector2(0.17, 0.36)], ["heart_autonomic", "Autonomic heart rate", Vector2(0.5, 0.74)], ["baroreflex", "Baroreflex", Vector2(0.83, 0.36)]],
	"breathing": [["breathing", "Breathing rhythm", Vector2(0.5, 0.5)]],
	"autonomic": [["pupil", "Pupil light reflex", Vector2(0.17, 0.34)], ["peristalsis", "Peristalsis", Vector2(0.5, 0.72)], ["fight_or_flight", "Fight or flight", Vector2(0.83, 0.34)]],
	# Bestiary placeholder layout (no plate yet): the three levels of a chapter on a gentle arc
	"worm": [["w_latch", "Holding a reversal", Vector2(0.2, 0.62)], ["w_switch", "Forward or backward", Vector2(0.5, 0.36)], ["worm_touch", "Touch, reverse or go", Vector2(0.8, 0.62)]],
	"leech": [["l_burst", "A cell that bursts", Vector2(0.2, 0.62)], ["l_rebound", "Rebound after inhibition", Vector2(0.5, 0.36)], ["leech_heart", "The heartbeat", Vector2(0.8, 0.62)]],
	"aplysia": [["a_depress", "Getting used to a touch", Vector2(0.2, 0.62)], ["a_facilitate", "A shock to the tail", Vector2(0.5, 0.36)], ["aplysia_gill", "The gill-withdrawal reflex", Vector2(0.8, 0.62)]],
	"fly_compass": [["c_ring", "A bump that stays", Vector2(0.2, 0.62)], ["c_shift", "Moving the bump", Vector2(0.5, 0.36)], ["fly_compass", "The compass in the dark", Vector2(0.8, 0.62)]],
	"fly_mb": [["k_sparse", "A sparse odour code", Vector2(0.2, 0.62)], ["k_depress", "Learning to avoid an odour", Vector2(0.5, 0.36)], ["fly_mb", "The mushroom body learns", Vector2(0.8, 0.62)]],
	"dynamics": [["d_class", "Where firing starts", Vector2(0.2, 0.62)], ["d_phasic", "Only the change counts", Vector2(0.5, 0.36)], ["d_switch", "A neuron that remembers a pulse", Vector2(0.8, 0.62)]],
	"rates": [["r_oscillate", "Two populations that make a rhythm", Vector2(0.2, 0.62)], ["r_choose", "Two populations, one winner", Vector2(0.5, 0.36)], ["r_meanfield", "The network and its mean field", Vector2(0.8, 0.62)]],
	"memory": [["h_complete", "Recalling a whole from a part", Vector2(0.2, 0.62)], ["h_two", "Two memories, no blends", Vector2(0.5, 0.36)], ["h_learn", "Writing a memory with plasticity", Vector2(0.8, 0.62)]],
	"tuning": [["t_tuned", "A cell tuned by its inputs", Vector2(0.2, 0.62)], ["t_sharpen", "Sharpening a tuning curve", Vector2(0.5, 0.36)], ["t_decode", "Reading a direction from a crowd", Vector2(0.8, 0.62)]],
	"a_worm": [["a_path", "A route on paper", Vector2(0.2, 0.62)], ["a_motif", "Over-represented, but against what?", Vector2(0.5, 0.36)], ["a_predict", "Bet on a lesion", Vector2(0.8, 0.62)], ["dx_a_worm", "The worm that does not back up", Vector2(0.64, 0.86)]],
	"crab": [["s_pacemaker", "A pacemaker kernel", Vector2(0.2, 0.62)], ["s_phase", "Phase from rebound", Vector2(0.5, 0.36)], ["crab_pyloric", "The pyloric rhythm", Vector2(0.8, 0.62)]]}

## Extra slots for engine tasks that are not in PLANNED.
const SPARE := [Vector2(0.5, 0.2), Vector2(0.5, 0.86), Vector2(0.28, 0.5), Vector2(0.72, 0.5)]

## Concept glossary (DESIGN-0.3 section 3): id -> [title, one-paragraph definition]. Ids the engine uses that are not here fall back to the id, capitalised.
const CONCEPTS := {
	"threshold": ["Threshold and rheobase", "A neuron fires when its membrane voltage reaches threshold. Rheobase is the smallest steady current that gets it there; weaker input, however long, does nothing.", "Lapicque L (1907) J Physiol Pathol Gen 9:620; Hodgkin AL, Huxley AF (1952) J Physiol 117:500, doi:10.1113/jphysiol.1952.sp004764"],
	"excitation": ["Excitation: weight and delay", "An excitatory synapse pushes the next cell toward threshold. Its weight sets how far, its delay how late; a few milliseconds per synapse add up along a pathway.", "Katz B, Miledi R (1965) Proc R Soc Lond B 161:483, doi:10.1098/rspb.1965.0016"],
	"convergence": ["Convergence and coincidence", "Many inputs onto one cell. If each is weak, the cell fires only when several arrive within a short window: it detects coincidence.", "Mendell LM, Henneman E (1971) J Neurophysiol 34:171, doi:10.1152/jn.1971.34.1.171"],
	"inhibition": ["Inhibition", "An inhibitory synapse pushes the next cell away from threshold. It silences, gates and shapes; without it a circuit can only ever add.", "Hartline HK, Wagner HG, Ratliff F (1956) J Gen Physiol 39:651, doi:10.1085/jgp.39.5.651"],
	"subtraction": ["Subtraction", "Excitation from one source and inhibition from another give a cell that reports the difference. ON minus OFF is how a nervous system computes a gradient.", "Suzuki H et al. (2008) Nature 454:114, doi:10.1038/nature06927"],
	"adaptation": ["Adaptation", "Under a steady drive many neurons fire a burst and then slow down as a slow current builds up. The cell reports change more than level.", "Madison DV, Nicoll RA (1984) J Physiol 354:319, doi:10.1113/jphysiol.1984.sp015378; Benda J, Herz AVM (2003) Neural Comput 15:2523, doi:10.1162/089976603322385063"],
	"tonic_drive": ["Tonic drive (bias)", "A steady current of a cell's own keeps it just over threshold, so it fires all the time at a low rate. A humming cell can signal less as well as more, and a pool held near threshold answers at once. Posture, pupil size and heart rate all rest on such a bias.", "Raman IM, Bean BP (1999) J Neurosci 19:1663, doi:10.1523/JNEUROSCI.19-05-01663.1999"],
	"half_centre": ["Mutual inhibition and the half-centre", "Two groups that inhibit each other cannot both win for long. With adaptation or fatigue they alternate: a half-centre oscillator, the core of many walking and swimming rhythms.", "Brown TG (1911) Proc R Soc Lond B 84:308; Grillner S (2003) Nat Rev Neurosci 4:573, doi:10.1038/nrn1137"],
	"coupling": ["Coupling and synchrony", "Oscillators that influence each other tend to lock their phases. Coupling strength and delay decide whether they fire together or in antiphase.", "Jalife J (1984) J Physiol 356:221, doi:10.1113/jphysiol.1984.sp015461"],
	"pacemaking": ["Intrinsic pacemaking", "Some cells fire without any input: a slow inward current drifts the voltage up to threshold again and again. The sinoatrial node sets the heartbeat this way.", "DiFrancesco D (1993) Annu Rev Physiol 55:455, doi:10.1146/annurev.ph.55.030193.002323"],
	"neuromodulation": ["Neuromodulation", "A modulator such as acetylcholine or noradrenaline does not fire the next cell; it changes how that cell responds, for instance slowing a pacemaker or raising its gain.", "Loewi O (1921) Pflugers Arch 189:239; Marder E (2012) Neuron 76:1, doi:10.1016/j.neuron.2012.09.010"],
	"recruitment": ["Recruitment (the size principle)", "A motor pool is recruited from small, easily excited neurons to large ones. Force grows smoothly because units are added in a fixed order.", "Henneman E (1957) Science 126:1345, doi:10.1126/science.126.3287.1345"],
	"reciprocal_inhibition": ["Reciprocal inhibition", "When an agonist muscle is driven, its antagonist is inhibited through an interneuron, so the two do not fight each other.", "Sherrington CS (1906) The Integrative Action of the Nervous System; Pierrot-Deseilligny E, Burke D (2005) The Circuitry of the Human Spinal Cord"],
	"recurrent_inhibition": ["Recurrent inhibition", "A motor neuron excites a Renshaw cell, which inhibits that motor neuron back. This is a gain control: strong drive is trimmed, weak drive passes.", "Renshaw B (1941) J Neurophysiol 4:167; Hultborn H (2006) Prog Neurobiol 78:215, doi:10.1016/j.pneurobio.2006.04.001"],
	"negative_feedback": ["Negative feedback control", "A sensor reports the error between the value and its set-point and the circuit pushes against it. Posture and blood pressure are held this way.", "Cannon WB (1929) Physiol Rev 9:399"],
	"feedforward": ["Feed-forward plus feedback", "Feed-forward acts on a cue before the error appears; feedback corrects what is left. Together they are fast and accurate.", "Rowell LB, O'Leary DS (1990) J Appl Physiol 69:407, doi:10.1152/jappl.1990.69.2.407"],
	"timed_bursts": ["Timed bursts (pattern generator)", "A chain of cells that fire in a fixed order and for fixed durations produces a movement without any sensory input: a central pattern generator.", "Marder E, Bucher D (2001) Curr Biol 11:R986, doi:10.1016/s0960-9822(01)00581-4"],
	"rhythm_gating": ["Rhythm and sensory gating", "A rhythm generator keeps the beat, and sensory input such as CO2 tunes its rate and amplitude without having to create it.", "Smith JC et al. (1991) Science 254:726, doi:10.1126/science.1683005; Feldman JL, Del Negro CA (2006) Nat Rev Neurosci 7:232, doi:10.1038/nrn1871"],
	"travelling_waves": ["Travelling waves and polarity", "Excitation behind and inhibition ahead make activity move in one direction along a chain of segments, as in gut peristalsis.", "Bayliss WM, Starling EH (1899) J Physiol 24:99"],
	"pathway": ["Pathway", "A route through the wiring from a sense to a muscle, found on a diagram. The shortest route on paper is a hypothesis: whether it carries the response has to be tested by silencing the cells off it.", "Chalfie M et al. (1985) J Neurosci 5:956, doi:10.1523/JNEUROSCI.05-04-00956.1985"],
	"prediction": ["Prediction before the experiment", "You commit to an answer first, then the experiment is run. A wrong bet costs something, so look at the wiring before you commit, and be ready to be wrong."],
	"null_model": ["Null model", "A pattern in a wiring diagram means little until you say what chance would give. Counting a motif in random graphs with the same number of edges, the same degrees, or the same degrees and cell-type blocks can give three different answers.", "Milo R et al. (2002) Science 298:824, doi:10.1126/science.298.5594.824"],
	"diagnosis": ["Diagnosis: from symptom to lesion", "Working backwards. The symptoms tell you what the circuit stopped doing; probing it with the scopes and lesions tells you where it was damaged.", "Rorden C, Karnath HO (2004) Nat Rev Neurosci 5:813, doi:10.1038/nrn1521"],
	"persistence": ["Persistence: positive feedback holds a state", "Cells that excite each other keep each other firing after the input that started them has gone. A brief touch can start a state, a reversal for instance, that holds itself until something inhibits it.", "Chalfie M et al. (1985) J Neurosci 5:956, doi:10.1523/JNEUROSCI.05-04-00956.1985"],
	"command_switch": ["Command switch (winner takes all)", "Two command groups that inhibit each other cannot both be active. Each is driven by its own inputs, and when both are driven the stronger wins and silences the other: forward or backward, never both.", "Chalfie M et al. (1985) J Neurosci 5:956, doi:10.1523/JNEUROSCI.05-04-00956.1985"],
	"bursting": ["Bursting", "Some neurons turn a steady drive into bursts of spikes with silent gaps. A slow variable inside the cell builds up during a burst, ends it, and decays during the gap. Leech heart interneurons are bursters.", "Nadim F et al. (1995) J Comput Neurosci 2:215, doi:10.1007/BF00961435; Hill AA et al. (2001) J Comput Neurosci 10:281, doi:10.1023/a:1011216131638"],
	"rebound": ["Post-inhibitory rebound", "A cell that has been held silent by inhibition can fire when the inhibition ends, because slow currents pushed the other way during the inhibition. Rebound lets a silenced half of an oscillator come back.", "Nadim F et al. (1995) J Comput Neurosci 2:215, doi:10.1007/BF00961435; Olsen OH et al. (1995) J Comput Neurosci 2:237, doi:10.1007/BF00961436"],
	"escape_release": ["Escape or release?", "Two cells that inhibit each other can take turns in two ways. In release the active cell tires and lets go. In escape the silenced cell recovers by itself and breaks away while the other is still firing. The bursts look alike; the way the cycle responds when one cell is driven harder tells them apart: a tiring cell bursts for longer, an escaping cell comes back sooner.", "Skinner FK, Kopell N, Marder E (1994) J Comput Neurosci 1:69, doi:10.1007/BF00962719; Sharp AA, Skinner FK, Marder E (1996) J Neurophysiol 76:867, doi:10.1152/jn.1996.76.2.867"],
	"synaptic_depression": ["Short-term synaptic depression", "A synapse that is used hard releases less each time, and refills slowly. In the Aplysia gill-withdrawal reflex this makes the response shrink with repeated touch (habituation) while the sensory cells keep firing the same.", "Castellucci V et al. (1970) Science 167:1745, doi:10.1126/science.167.3926.1745"],
	"heterosynaptic_facilitation": ["Heterosynaptic facilitation", "Activity in one pathway changes the strength of a synapse in another, through a released modulator. A shock to the tail of Aplysia releases serotonin that strengthens the siphon's sensory synapses: the reflex is sensitised.", "Castellucci V et al. (1970) Science 167:1745, doi:10.1126/science.167.3926.1745; Hawkins RD et al. (2006) Biol Bull 210:174, doi:10.2307/4134556"],
	"ring_attractor": ["Ring attractor", "Cells arranged on a ring that excite their neighbours and are inhibited together can hold a single bump of activity in place. In the fly the position of the bump on the ring of E-PG cells is its heading, and it stays when the landmark goes.", "Seelig JD, Jayaraman V (2015) Nature 521:186, doi:10.1038/nature14446"],
	"path_integration": ["Path integration", "Adding up movement to keep track of position or heading. In the fly, left and right shifting cells (P-EN) push the compass bump one way or the other, so that it follows the animal's turns even in the dark.", "Turner-Evans D et al. (2017) eLife 6:e23496, doi:10.7554/eLife.23496; Green J et al. (2017) Nature 546:101, doi:10.1038/nature22343"],
	"sparse_coding": ["Sparse coding", "Each Kenyon cell of the fly listens to a few projection neurons at random and fires only when several are active together, and one giant inhibitory neuron (APL) holds the number of active cells down. An odour becomes a small subset of cells, and different odours use mostly different cells.", "Aso Y et al. (2014) eLife 3:e04577, doi:10.7554/eLife.04577; Papadopoulou M et al. (2011) Science 332:721, doi:10.1126/science.1201835"],
	"dopamine_gated_depression": ["Dopamine-gated depression", "A synapse that weakens only when its input was active and dopamine arrives afterwards. In the fly mushroom body pairing an odour with punishment depresses the synapses of that odour's Kenyon cells onto an output neuron, and leaves other odours alone.", "Hige T et al. (2015) Neuron 88:985, doi:10.1016/j.neuron.2015.11.003"],
	"pacemaker_kernel": ["Pacemaker kernel", "A bursting cell coupled to partner cells that it drags into its rhythm. In the crab stomatogastric ganglion the anterior burster AB and the pyloric dilator cells PD burst together, about once a second, and pace the whole pyloric rhythm.", "Marder E, Bucher D (2001) Curr Biol 11:R986, doi:10.1016/s0960-9822(01)00581-4"],
	"phase_lag": ["Phase from inhibition and rebound", "A follower cell that is inhibited by the pacemaker and fires by rebound when the inhibition lets go has its burst after the pacemaker's in the cycle. The delay comes from slow currents that wake up on release, and it fixes the follower's phase.", "Prinz AA et al. (2004) Nat Neurosci 7:1345, doi:10.1038/nn1352; Marder E, Bucher D (2001) Curr Biol 11:R986, doi:10.1016/s0960-9822(01)00581-4"],
	"degeneracy": ["Degeneracy", "Very different sets of membrane conductances and synaptic strengths can give the same network pattern, so animals can differ a lot underneath and still behave alike. It also means that similar-looking lesions can come from different missing parts.", "Prinz AA, Bucher D, Marder E (2004) Nat Neurosci 7:1345, doi:10.1038/nn1352"],
	"excitability_class": ["Excitability class (type I and II)", "How a neuron starts to fire as the current grows. Class I cells fire slowly at first and speed up smoothly; class II cells start at a clear rate at once. In the phase plane the resting state vanishes by colliding with a saddle (class I) or turns into an unstable spiral (class II).", "Hodgkin AL (1948) J Physiol 107:165, doi:10.1113/jphysiol.1948.sp004260; Ermentrout B (1996) Neural Comput 8:979, doi:10.1162/neco.1996.8.5.979"],
	"stable_fixed_point": ["A stable fixed point (phasic firing)", "In the phase plane the point where the nullclines cross is the resting state. If it is still stable at the new current, a cell that was kicked across threshold spirals back into it and falls silent although the current stays on: it reports change, not level.", "Izhikevich EM (2003) IEEE Trans Neural Netw 14:1569, doi:10.1109/TNN.2003.820440; Rothman JS, Manis PB (2003) J Neurophysiol 89:3097, doi:10.1152/jn.00127.2002"],
	"bistability": ["Bistability", "One cell, one current, two stable states: rest and repetitive firing, each with its own basin. A brief pulse that crosses the border between the basins flips the state and it stays flipped, so the cell stores one bit.", "Izhikevich EM (2003) IEEE Trans Neural Netw 14:1569, doi:10.1109/TNN.2003.820440; Touboul J, Brette R (2008) Biol Cybern 99:319, doi:10.1007/s00422-008-0267-4"],
	"wilson_cowan": ["The Wilson-Cowan pair (rate model)", "A population of neurons described by one number, its firing rate. An excitatory population that excites itself and an inhibitory one it drives and that inhibits it back can settle, switch between states or oscillate: excitation builds, drives the inhibition, which shuts it down and then fades.", "Wilson HR, Cowan JD (1972) Biophys J 12:1, doi:10.1016/S0006-3495(72)86068-5"],
	"rate_competition": ["Competition between populations", "Two populations that excite themselves and inhibit each other turn a small difference in input into a large difference in output: one wins, the other is silenced, and the winner holds itself up until a much stronger input for the other side overturns it.", "Wilson HR, Cowan JD (1972) Biophys J 12:1, doi:10.1016/S0006-3495(72)86068-5"],
	"mean_field": ["Mean-field theory", "For a large, sparse network of integrate-and-fire neurons the input to a cell is the sum of many weak, independent inputs, so its mean and variance fix the cell's rate (the Siegert formula) and the rates of the populations obey self-consistent equations. A unit that applies the formula stands for a whole population, and the prediction fails when the network is small, strongly coupled or synchronous.", "Amit DJ, Brunel N (1997) Cereb Cortex 7:237, doi:10.1093/cercor/7.3.237; Fourcaud N, Brunel N (2002) Neural Comput 14:2057, doi:10.1162/089976602320264015"],
	"attractor_memory": ["Attractor memory (Hopfield)", "A network with symmetric recurrent connections has stable states. If a pattern is stored in the weights, a part of it cued falls into the whole: a content-addressable memory that completes a partial cue and keeps the pattern after the cue is gone.", "Hopfield JJ (1982) Proc Natl Acad Sci USA 79:2554, doi:10.1073/pnas.79.8.2554; Amit DJ, Brunel N (1997) Cereb Cortex 7:237, doi:10.1093/cercor/7.3.237"],
	"spurious_states": ["Spurious states", "A network that stores several patterns can fall into mixtures that were never stored. Competition between the patterns, symmetric like the other weights, makes the pure patterns the states that win, so an ambiguous cue ends in one pattern and not a blend.", "Hopfield JJ (1982) Proc Natl Acad Sci USA 79:2554, doi:10.1073/pnas.79.8.2554"],
	"hebbian_storage": ["Hebbian storage by STDP", "Connections between cells that fire together are strengthened; with spike-timing-dependent plasticity a presynaptic spike just before a postsynaptic one potentiates the connection. Showing a pattern to such a network writes it into the weights.", "Bi GQ, Poo MM (1998) J Neurosci 18:10464, doi:10.1523/JNEUROSCI.18-24-10464.1998; Song S, Miller KD, Abbott LF (2000) Nat Neurosci 3:919, doi:10.1038/78829"],
	"tuning_curve": ["Tuning curve", "Many sensory neurons fire most for one value of a stimulus, an orientation or a direction, and less for values away from it. A cell that listens only to inputs that prefer one value inherits that preference: orientation selectivity from aligned inputs.", "Hubel DH, Wiesel TN (1962) J Physiol 160:106, doi:10.1113/jphysiol.1962.sp006837"],
	"lateral_inhibition_tuning": ["Sharpening tuning with inhibition", "Inhibition among cells with different preferences makes each cell's tuning narrower than that of its input: the cell with the strongest input pulls ahead and pushes its neighbours down. A cell that also excites itself holds its lead.", "Ben-Yishai R, Bar-Or RL, Sompolinsky H (1995) Proc Natl Acad Sci USA 92:3844, doi:10.1073/pnas.92.9.3844"],
	"population_vector": ["Population vector", "Each cell is broadly tuned and noisy, but the sum of the preferred directions of all the cells, each weighted by how much it fires, points in the direction of the stimulus. The population decodes what no single cell can.", "Georgopoulos AP, Schwartz AB, Kettner RE (1986) Science 233:1416, doi:10.1126/science.3749885"],
	"divergent_command": ["Divergent command", "One command cell fans out to many targets, so a single cue changes the whole body at once: the fight-or-flight response.", "Cannon WB (1915) Bodily Changes in Pain, Hunger, Fear and Rage; Janig W (2006) The Integrative Action of the Autonomic Nervous System"]}

static var pending_variant := ""   # set by the map before app.open_lab(task) for a variant branch; the lab reads it and clears it ("" = the base task)
static var _info := {}
static var _ids := []
static var _have_ids := false
static var _notes := {}
static var _nodes := {}

## Dev-only synthetic levels (game/mock_task.gd): debug builds only (the editor, `godot --path godot`), and only with SP_MOCK / SP_MOCK03 set.
## Release exports leave the file out (export_presets.cfg exclude_filter), so production never has them.
const MOCK_PATH := "res://game/mock_task.gd"
static var _mock_script: Variant = null
static func mock_script() -> Variant:
	if not OS.is_debug_build(): return null
	if _mock_script == null and ResourceLoader.exists(MOCK_PATH): _mock_script = load(MOCK_PATH)
	return _mock_script
static func _mc(name: String) -> Variant: return (mock_script() as GDScript).get_script_constant_map().get(name)
static func mock_on() -> bool: return OS.get_environment("SP_MOCK") != "" and mock_script() != null
static func mock03() -> bool: return OS.get_environment("SP_MOCK03") != "" and mock_script() != null
## The synthetic body-system task ids (SP_MOCK); [] when mocks are off.
static func mock_ids() -> Array: return (_mc("IDS") as Array).duplicate() if mock_on() else []
static func mock_has(id: String) -> bool: return mock_on() and (_mc("DEF") as Dictionary).has(id)
## The Atlas wing's development-only level (0.6, A1): a real engine task that is NOT in the campaign. It is offered like the synthetic mock levels, in a debug build only (the
## mock file is excluded from the release export, and so are the level's circuit files), and only with SP_ATLAS set. `make` works for it always (tests, the engine), `installed`
## lists it only here.
const DEV_IDS := ["atlas_probe"]
static func atlas_dev() -> bool: return OS.get_environment("SP_ATLAS") != "" and mock_script() != null
static func is_dev(id: String) -> bool: return DEV_IDS.has(task_of(id))
## A prediction bet (Task::diagnosis().bet): a level of kind "diagnosis" whose answer is committed BEFORE the run and judged on what the run measures (a_motif, a_predict).
## Everything that treats "a diagnosis level" as a case with a stored answer asks this too.
static var _bets := {}
static func is_bet(id: String) -> bool:
	id = task_of(id)
	if _bets.has(id): return bool(_bets[id])
	var r := false
	if str(info(id).get("kind", "")) == "diagnosis":
		var t = make(id)
		if t != null and t.has_method("diagnosis"): r = bool((t.diagnosis() as Dictionary).get("bet", false))
	_bets[id] = r; return r
static func ext_ok() -> bool: return ClassDB.class_exists("SpikeTask")
## The engine's task ids whose circuits this build holds: all of them in a development build; a release export leaves the development-only levels out
## (export_presets.cfg and the package scripts do not pack their circuit files), so the smoke run of an exported build does not ask for them.
static func shipped_ids() -> Array:
	var out := []
	if not ext_ok(): return out
	for id in ClassDB.instantiate("SpikeTask").call("task_ids"):
		if DEV_IDS.has(str(id)) and mock_script() == null: continue
		out.append(str(id))
	return out
## The circuit a locked-wiring level gives the player (path relative to circuits/, Task::given), "" for every other level.
static func given_path(id: String) -> String:
	if not ext_ok(): return ""
	var t = ClassDB.instantiate("SpikeTask")
	if not t.make(task_of(id)) or not t.has_method("given_circuit"): return ""
	return str(t.call("given_circuit"))
static func unlock_all() -> bool: return OS.get_environment("SP_UNLOCK_ALL") != ""

## A task object for `id`: the engine's SpikeTask, or a synthetic one for the SP_MOCK dev ids. null when unknown. The task is put in the player's current
## difficulty mode (Task::set_mode), so everything built from it (bands, noise, seeds) follows the mode.
static func make(id: String) -> Variant:
	if mock_has(id): var m = (mock_script() as GDScript).new(); m.make(id); return m
	if not ext_ok(): return null
	var t = ClassDB.instantiate("SpikeTask")
	if not t.make(id): return null
	if t.has_method("set_mode"): t.set_mode(Progress.mode())
	return t

static func ref_path(id: String) -> String:
	if mock_has(id): return str(_mc("REF"))
	if not ext_ok(): return ""
	return str(ClassDB.instantiate("SpikeTask").call("reference_circuit_of", id))

## Forget cached task info (a rebuilt extension may know more tasks, a changed mode changes the bands).
static func refresh() -> void: _info = {}; _ids = []; _have_ids = false; _notes = {}; _nodes = {}

## Every installed task id (the engine's, plus the mock ones under SP_MOCK / SP_MOCK03), in the engine's registry order (= play order).
static func installed() -> Array:
	if _have_ids: return _ids
	var out := []
	if ext_ok():
		for id in ClassDB.instantiate("SpikeTask").call("task_ids"):
			if DEV_IDS.has(str(id)) and not atlas_dev(): continue   # the Atlas probe is a development level
			out.append(str(id))
	if mock03():
		var real := out.duplicate(); out = []
		for c in CHAPTERS:
			for id in (_mc("ORDER03") as Dictionary).get(c["id"], []):
				if str(id).begins_with("m3_") or real.has(id): out.append(id)
		for id in real:
			if not out.has(id): out.append(id)
	out.append_array(mock_ids())
	_ids = out; _have_ids = true; return out

static func is_installed(id: String) -> bool: return installed().has(task_of(id))

## "task#variant" -> "task".
static func task_of(id: String) -> String: return id.get_slice("#", 0)
static func variant_of(id: String) -> String: return id.get_slice("#", 1) if "#" in id else ""

## Static facts of a task, cached: title, brief, chapter, view, par, duration, channels, timeline, bands, modulators, and the v0.3 ones: kind (micro | task |
## boss | diagnosis), spine (bool), concepts_new / concepts_used, variants {id: text}, parent (the spine task a branch hangs off, or ""), symptoms.
static func info(id: String) -> Dictionary:
	id = task_of(id)
	if _info.has(id): return _info[id]
	var t = make(id)
	var d := {"id": id, "ok": t != null, "title": id.capitalize(), "brief": "", "chapter": "invertebrates", "view": "polyline", "par": 0, "duration_ms": 0.0,
		"sensors": [], "motors": [], "timeline": [], "bands": [], "modulators": [], "kind": "task", "spine": true, "concepts_new": [], "concepts_used": [], "variants": {},
		"parent": "", "symptoms": "", "synthetic": false}
	if t != null:
		d["title"] = str(t.title()); d["brief"] = str(t.brief()); d["duration_ms"] = float(t.duration_ms())
		d["sensors"] = Array(t.sensor_channels()); d["motors"] = Array(t.motor_channels())
		var ch := str(t.chapter()); var vw := str(t.view())
		d["chapter"] = ch if ch != "" else "invertebrates"; d["view"] = vw if vw != "" else "polyline"; d["par"] = int(t.par_neurons())
		d["timeline"] = Array(t.timeline()); d["bands"] = Array(t.bands()); d["modulators"] = Array(t.modulators())
		if t.has_method("level_kind"):   # v0.3 bindings; older extensions leave the defaults
			var k := str(t.level_kind()); d["kind"] = k if k in ["micro", "task", "boss", "diagnosis"] else "task"
			d["spine"] = bool(t.on_spine()) and d["kind"] != "diagnosis"
			d["concepts_new"] = Array(t.concepts_new()); d["concepts_used"] = Array(t.concepts_used())
			if t.has_method("parent"): d["parent"] = str(t.parent())   # declared by the engine (Task::parent); "" on a spine level
			d["variants"] = (t.variants() as Dictionary).duplicate()
			if d["kind"] == "diagnosis": d["symptoms"] = str((t.diagnosis() as Dictionary).get("symptoms", ""))
	elif id.begins_with("m3_") and mock03() and (_mc("DEFS03") as Dictionary).has(id):
		var m: Dictionary = _mc("DEFS03")[id]
		d["title"] = str(m["title"]); d["synthetic"] = true; d["duration_ms"] = 4000.0; d["brief"] = "Synthetic level for looking at the map (SP_MOCK03). Nothing to build."
		for c in CHAPTERS:
			if ((_mc("ORDER03") as Dictionary)[c["id"]] as Array).has(id): d["chapter"] = c["id"]
	if mock03():
		var m: Dictionary = (_mc("DEFS03") as Dictionary).get(id, (_mc("OVER03") as Dictionary).get(id, {}))
		if not m.is_empty():
			if m.has("kind"): d["kind"] = m["kind"]
			if m.has("spine"): d["spine"] = m["spine"]
			if (d["concepts_new"] as Array).is_empty(): d["concepts_new"] = m.get("new", [])
			if (d["concepts_used"] as Array).is_empty(): d["concepts_used"] = m.get("used", [])
			if (d["variants"] as Dictionary).is_empty(): d["variants"] = m.get("variants", {})
			if m.has("parent"): d["parent"] = m["parent"]
			if m.has("sym"): d["symptoms"] = m["sym"]
	_info[id] = d; return d

## The lab-notebook entry of a level, {title, text, citation}; empty title = the level has none (yet).
static func notebook(id: String) -> Dictionary:
	id = task_of(id)
	if _notes.has(id): return _notes[id]
	var n := {"title": "", "text": "", "citation": ""}
	var t = make(id)
	if t != null and t.has_method("notebook"):
		var d: Dictionary = t.notebook()
		n = {"title": str(d.get("title", "")), "text": str(d.get("text", "")), "citation": str(d.get("citation", ""))}
	if n["title"] == "" and mock03():
		var m: Dictionary = (_mc("DEFS03") as Dictionary).get(id, (_mc("OVER03") as Dictionary).get(id, {}))
		if m.has("note"): n = {"title": m["note"][0], "text": m["note"][1], "citation": m["note"][2]}
	_notes[id] = n; return n

## Ids the content uses for the same concept (British and American spellings, longer names) map to one glossary entry.
const CONCEPT_ALIAS := {"travelling_wave": "travelling_waves", "traveling_wave": "travelling_waves", "traveling_waves": "travelling_waves", "feedforward_feedback": "feedforward", "feed_forward": "feedforward",
	"half_center": "half_centre", "halfcentre": "half_centre"}
static func concept_id(c: String) -> String: return str(CONCEPT_ALIAS.get(c, c))

static func concept_title(c: String) -> String:
	c = concept_id(c)
	return str(CONCEPTS[c][0]) if CONCEPTS.has(c) else c.replace("_", " ").capitalize()
static func concept_blurb(c: String) -> String:
	c = concept_id(c); return str(CONCEPTS[c][1]) if CONCEPTS.has(c) else ""
static func concept_citation(c: String) -> String:
	c = concept_id(c); return str(CONCEPTS[c][2]) if CONCEPTS.has(c) and (CONCEPTS[c] as Array).size() > 2 else ""
## Short name for a chip: the title up to the first colon or parenthesis.
static func concept_short(c: String) -> String:
	var t := concept_title(c)
	for sep in [":", " ("]:
		if sep in t: t = t.get_slice(sep, 0)
	return t

## Every concept a level introduces somewhere in the campaign: {concept: {level, used_by: [level ids]}}, in play order.
static func concept_index() -> Dictionary:
	var out := {}
	for id in installed():
		for c0 in info(id)["concepts_new"]:
			var c := concept_id(str(c0))
			if not out.has(c): out[c] = {"level": id, "used_by": []}
	for id in installed():
		for c0 in info(id)["concepts_used"]:
			var c := concept_id(str(c0))
			if out.has(c) and out[c]["level"] != id and not (out[c]["used_by"] as Array).has(id): (out[c]["used_by"] as Array).append(id)
	return out

static func chapter_index(ch: String) -> int:
	for i in CHAPTERS.size():
		if CHAPTERS[i]["id"] == ch: return i
	return 0

static func chapter_def(ch: String) -> Dictionary: return CHAPTERS[chapter_index(ch)]

## Result helpers that understand variant ids ("task#variant": passed = the variant is on the task's done list).
static func passed(id: String) -> bool:
	if "#" in id: return Progress.variants_done(task_of(id)).has(variant_of(id))
	return bool(Progress.best(id)["passed"])
static func stars(id: String) -> int:
	if "#" in id: return 1 if passed(id) else 0
	return int(Progress.best(id)["stars"])

## Nodes of a chapter: spine nodes first, in play order, then the branches ({id, task, variant, title, installed, pos, kind, spine, parent, concepts_new,
## concepts_used, synthetic}). The engine's registry order is the single source of truth for the order; PLANNED supplies the map positions of the 0.2 tasks
## (Vector2(-1, -1) = not placed: the map interpolates along the spine) and the ghost nodes for tasks that have not landed. Planned ghosts come last on the
## spine. A branch's `parent` is the spine task it hangs off: a variant hangs off its task; a diagnosis level off the task Task::concepts / the order say.
static func nodes(ch: String) -> Array:
	if _nodes.has(ch): return _nodes[ch]
	var out := _build_nodes(ch); _nodes[ch] = out; return out

static func _build_nodes(ch: String) -> Array:
	var spine := []; var branches := []; var pos := {}; var title := {}
	for p in PLANNED.get(ch, []): pos[p[0]] = p[2]; title[p[0]] = p[1]
	var in_ch := []
	for id in installed():
		if str(info(id)["chapter"]) == ch: in_ch.append(id)
	for id in in_ch:
		var d := info(id)
		var n := {"id": id, "task": id, "variant": "", "title": str(d["title"]), "installed": true, "pos": pos.get(id, Vector2(-1, -1)), "kind": str(d["kind"]), "spine": bool(d["spine"]),
			"parent": "", "concepts_new": d["concepts_new"], "concepts_used": d["concepts_used"], "synthetic": bool(d["synthetic"]), "needs": needs(id).map(func(x): return {"concept": x["concept"], "level": x["level"]})}
		if bool(d["spine"]): spine.append(n)
		else: n["parent"] = _parent_of(id, in_ch); branches.append(n)
	for n in spine:
		var vs: Dictionary = info(str(n["id"]))["variants"]
		for v in vs:
			branches.append({"id": "%s#%s" % [n["id"], v], "task": n["id"], "variant": str(v), "title": str(vs[v]), "installed": true, "pos": Vector2(-1, -1), "kind": "variant", "spine": false,
				"parent": n["id"], "concepts_new": [], "concepts_used": info(str(n["id"]))["concepts_new"], "synthetic": bool(n["synthetic"])})
	for p in PLANNED.get(ch, []):
		if not is_installed(p[0]): spine.append({"id": p[0], "task": p[0], "variant": "", "title": p[1], "installed": false, "pos": p[2], "kind": "task", "spine": true, "parent": "",
			"concepts_new": [], "concepts_used": [], "synthetic": false})
	return spine + branches

## The spine task a non-spine level hangs off: the parent the engine declares (Task::parent: a diagnosis case hangs off the task whose circuit it is, Posture and Reach
## off the stretch reflex; engine/tests/tasks_rules_test.cpp requires every non-spine level to declare one). The guess below (the spine task of its chapter that shares
## most of its concepts, else the last spine task before it, else the first) is only for levels that declare none: the synthetic SP_MOCK03 ones.
static func _parent_of(id: String, in_ch: Array) -> String:
	var d := info(id)
	if str(d["parent"]) != "" and in_ch.has(d["parent"]): return str(d["parent"])
	var mine: Array = (d["concepts_used"] as Array) + (d["concepts_new"] as Array)
	var best := ""; var score := 0; var before := ""; var seen := false
	for o in in_ch:
		if o == id: seen = true; continue
		var od := info(o)
		if not bool(od["spine"]): continue
		if not seen: before = o
		var sc := 0
		for c in (od["concepts_new"] as Array):
			if mine.has(c): sc += 2
		for c in (od["concepts_used"] as Array):
			if mine.has(c): sc += 1
		if sc > score: score = sc; best = o
	if best != "": return best
	if before != "": return before
	for o in in_ch:
		if bool(info(o)["spine"]): return o
	return ""

static func spine_nodes(ch: String) -> Array:
	return nodes(ch).filter(func(n): return bool(n["spine"]))

## Installed spine task ids of a chapter in play order.
static func spine_ids(ch: String) -> Array:
	var out := []
	for n in spine_nodes(ch):
		if n["installed"]: out.append(n["id"])
	return out

## Installed task ids in play order across all chapters (spine only: what "next level" means).
static func order() -> Array:
	var out := []
	for c in CHAPTERS: out.append_array(spine_ids(c["id"]))
	return out

static func chapter_of(id: String) -> String: return str(info(id)["chapter"])

## Spine levels of a chapter passed / installed.
static func passed_count(ch: String) -> int:
	var k := 0
	for id in spine_ids(ch):
		if passed(id): k += 1
	return k

static func installed_count(ch: String) -> int: return spine_ids(ch).size()

static func spine_passed(ch: String) -> bool:
	var ids := spine_ids(ch)
	return not ids.is_empty() and passed_count(ch) == ids.size()

## Stars earned / available over the chapter's levels that are tasks of their own (spine and diagnosis; variants have no stars).
static func star_total(ch: String) -> Vector2i:
	var got := 0; var all := 0
	for n in nodes(ch):
		if n["installed"] and str(n["kind"]) != "variant": got += int(Progress.best(str(n["id"]))["stars"]); all += 3
	return Vector2i(got, all)

## Chapters that have installed levels, in order (the ones the unlock rule counts).
static func _playable_chapters() -> Array:
	var out := []
	for c in CHAPTERS:
		if is_wing(str(c["id"])): continue   # the Bestiary, Theory and Atlas wings are outside the body chapters' unlock chain
		if installed_count(str(c["id"])) > 0: out.append(str(c["id"]))
	return out

## True for the chapters of the Bestiary wing (v0.5): open after the chapter I spine, never gating a body chapter.
static func is_bestiary(ch: String) -> bool: return str(chapter_def(ch).get("wing", "")) == "bestiary"
## v0.6: the Theory wing (dynamics, rates, memory, tuning): open after the chapter I spine as well.
static func is_theory(ch: String) -> bool: return str(chapter_def(ch).get("wing", "")) == "theory"
## v0.6: the Atlas wing (reading real wiring): open after the chapter I spine as well.
static func is_atlas(ch: String) -> bool: return str(chapter_def(ch).get("wing", "")) == "atlas"
static func is_wing(ch: String) -> bool: return is_bestiary(ch) or is_theory(ch) or is_atlas(ch)
## "body", "bestiary", "theory" or "atlas": the row of the campaign map the chapter's tab sits in.
static func wing_of(ch: String) -> String: return "theory" if is_theory(ch) else ("atlas" if is_atlas(ch) else ("bestiary" if is_bestiary(ch) else "body"))

## The Bestiary and Theory wings open once the chapter I spine (Invertebrates) is passed, and inside the wings a chapter may wait for a level or a chapter it
## builds on (WING_GATES). Guided mode drops the in-wing prerequisites (it keeps the chapter I gate): a Guided player may wander, and the "Needs" chips on
## the level card still say what the level uses. The body chapters: a chapter opens when the spine of the previous chapter (that has levels) is passed, OR the
## spine of the chapter two before it is passed, so one stuck task never blocks the game; the first two are open from the start. A task that sets on_spine
## false is a branch and never gates.
const FIRST_CHAPTER := "invertebrates"
## chapter -> {level: a level that must be passed, or chapter: a chapter whose spine must be passed, title: what the card says}
const WING_GATES := {
	"aplysia": {"level": "m_neuromodulation", "title": "Heart: Neuromodulation"},
	"crab": {"chapter": "leech", "title": "Bestiary: Leech"},
	"fly_mb": {"chapter": "aplysia", "title": "Bestiary: Sea slug"}}

static func chapter_open(ch: String) -> bool:
	if unlock_all(): return true
	if is_wing(ch):
		if not spine_passed(FIRST_CHAPTER): return false
		return wing_gate_met(ch) or Progress.mode() == "guided"
	var pl := _playable_chapters(); var i := pl.find(ch)
	if i < 2: return true   # (also true for a chapter with no levels yet)
	return spine_passed(str(pl[i - 1])) or spine_passed(str(pl[i - 2]))

## The in-wing prerequisite of a wing chapter is met (true for chapters without one).
static func wing_gate_met(ch: String) -> bool:
	if not WING_GATES.has(ch): return true
	var g: Dictionary = WING_GATES[ch]
	if g.has("level"): return passed(str(g["level"]))
	return spine_passed(str(g["chapter"]))

## The chapter whose spine opens `ch` (the previous one with levels; "" for the first two; a wing: Invertebrates).
static func gate_chapter(ch: String) -> String:
	if is_wing(ch): return FIRST_CHAPTER
	var pl := _playable_chapters(); var i := pl.find(ch)
	return str(pl[i - 1]) if i >= 2 else ""

## What a node needs, for the "Needs" chips of the level card (the node dict carries the static part as "needs": [{concept, level}]): [{concept, title, level, level_title, met}] for every concept the level
## uses (Task::concepts_used) that another level teaches (the first level that introduces it). `met` is true when that level is passed. Data only: nothing here gates.
static func needs(id: String) -> Array:
	var out := []; var idx := concept_index(); id = task_of(id)
	for c0 in info(id)["concepts_used"]:
		var c := concept_id(str(c0))
		if not idx.has(c): continue
		var lvl := str(idx[c]["level"])
		if lvl == id: continue
		out.append({"concept": c, "title": concept_short(c), "level": lvl, "level_title": str(info(lvl)["title"]), "met": passed(lvl)})
	return out

## A spine node opens when the spine node before it is passed (the chapter's first opens with the chapter); a branch when the task it hangs off is passed.
static func unlocked(id: String) -> bool:
	if unlock_all(): return true
	var t := task_of(id); var ch := chapter_of(t)
	if not chapter_open(ch): return false
	var nd := {}
	for n in nodes(ch):
		if n["id"] == id: nd = n
	if nd.is_empty(): return false
	if not bool(nd["spine"]): return passed(str(nd["parent"])) if str(nd["parent"]) != "" else true
	var prev := ""
	for n in spine_nodes(ch):
		if n["id"] == id: break
		if n["installed"]: prev = str(n["id"])
	return prev == "" or passed(prev)

## Why a chapter is closed ("" when it is open): the sentence of the locked-chapter banner and of the level card.
static func chapter_lock_reason(ch: String) -> String:
	if chapter_open(ch): return ""
	if is_wing(ch):
		if not spine_passed(FIRST_CHAPTER): return "Pass the %s spine first" % str(chapter_def(FIRST_CHAPTER)["title"])
		var g: Dictionary = WING_GATES.get(ch, {})
		if g.has("level"): return "Pass %s (%s) first" % [str(info(str(g["level"]))["title"]), str(g["title"])]
		return "Pass the %s spine first" % str(g.get("title", ""))
	var g2 := gate_chapter(ch)
	if g2 == "": return ""
	var pl := _playable_chapters(); var gi := pl.find(ch)
	return "Pass the %s spine (or the %s spine) first" % [str(chapter_def(g2)["title"]), str(chapter_def(str(pl[gi - 2]))["title"])]

## What gates a locked node, for the card: "Pass <level> first" or "Pass the <chapter> spine first".
static func lock_reason(id: String) -> String:
	var t := task_of(id); var ch := chapter_of(t)
	if not chapter_open(ch): return chapter_lock_reason(ch)
	var nd := {}
	for n in nodes(ch):
		if n["id"] == id: nd = n
	if nd.is_empty(): return ""
	if not bool(nd["spine"]): return "Pass %s first" % str(info(str(nd["parent"]))["title"]) if str(nd["parent"]) != "" else ""
	var prev := ""
	for n in spine_nodes(ch):
		if n["id"] == id: break
		if n["installed"]: prev = n["title"]
	return "Pass %s first" % prev if prev != "" else ""

## Next installed spine level after `id` in play order, "" after the last (and for branches).
static func next_task(id: String) -> String:
	var o := order(); var i := o.find(id)
	return str(o[i + 1]) if i >= 0 and i + 1 < o.size() else ""

## The chapters whose parts are available: every chapter that is open.
static func chapters_unlocked() -> Array:
	var out := []
	for c in CHAPTERS:
		if chapter_open(c["id"]): out.append(c["id"])
	return out
