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
##             map and the notebook can be looked at before the content agents have landed theirs (see MOCK03_* below)
##             SP_UNLOCK_ALL=1 opens every node and every part.

const CHAPTERS := [
	{"id": "invertebrates", "title": "Invertebrates", "organ": "worm", "blurb": "Reflexes, a swimming rhythm, a nose for salt. Small circuits, real behaviour."},
	{"id": "spinal", "title": "Spinal cord", "organ": "spine", "blurb": "Stretch, hold and reach: muscle spindles, motor neurons, Renshaw cells."},
	{"id": "heart", "title": "Heart", "organ": "heart", "blurb": "A pacemaker cell sets the beat; the autonomic nerves and the baroreflex steer it."},
	{"id": "breathing", "title": "Breathing", "organ": "lungs", "blurb": "A bursting pacemaker network in the brainstem drives the diaphragm, tuned by CO2."},
	{"id": "autonomic", "title": "Autonomic", "organ": "autonomic", "blurb": "Pupil, gut and the whole fight-or-flight response: two opposing nerves, one body."}]

## Planned lineup per chapter: [id, title, node centre on the map (0..1, 0..1)].
const PLANNED := {
	"invertebrates": [["reflex", "Touch reflex", Vector2(0.17, 0.66)], ["worm", "Salt-seeking worm", Vector2(0.5, 0.34)], ["lamprey", "Swimming lamprey", Vector2(0.83, 0.66)]],
	"spinal": [["stretch_reflex", "Stretch reflex", Vector2(0.15, 0.3)], ["renshaw", "Renshaw inhibition", Vector2(0.38, 0.7)], ["posture", "Posture", Vector2(0.62, 0.3)], ["reach", "Reach", Vector2(0.85, 0.7)]],
	"heart": [["heart_pacemaker", "Heart pacemaker", Vector2(0.17, 0.36)], ["heart_autonomic", "Autonomic heart rate", Vector2(0.5, 0.74)], ["baroreflex", "Baroreflex", Vector2(0.83, 0.36)]],
	"breathing": [["breathing", "Breathing rhythm", Vector2(0.5, 0.5)]],
	"autonomic": [["pupil", "Pupil light reflex", Vector2(0.17, 0.34)], ["peristalsis", "Peristalsis", Vector2(0.5, 0.72)], ["fight_or_flight", "Fight or flight", Vector2(0.83, 0.34)]]}

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
	"diagnosis": ["Diagnosis: from symptom to lesion", "Working backwards. The symptoms tell you what the circuit stopped doing; probing it with the scopes and lesions tells you where it was damaged.", "Rorden C, Karnath HO (2004) Nat Rev Neurosci 5:813, doi:10.1038/nrn1521"],
	"divergent_command": ["Divergent command", "One command cell fans out to many targets, so a single cue changes the whole body at once: the fight-or-flight response.", "Cannon WB (1915) Bodily Changes in Pain, Hunger, Fear and Rage; Janig W (2006) The Integrative Action of the Autonomic Nervous System"]}

static var pending_variant := ""   # set by the map before app.open_lab(task) for a variant branch; the lab reads it and clears it ("" = the base task)
static var _info := {}
static var _ids := []
static var _have_ids := false
static var _notes := {}
static var _nodes := {}

## Synthetic v0.3 levels (SP_MOCK03=1): the play order per chapter (real task ids stay where the engine has them) and what each synthetic level says about
## itself. Real tasks get the v0.3 facts from MOCK03_OVER until the engine supplies them. {kind, concepts_new, concepts_used, spine, parent, variants, note}.
const MOCK03_ORDER := {
	"invertebrates": ["m3_threshold", "m3_excitation", "m3_convergence", "reflex", "m3_inhibition", "m3_subtraction", "worm", "m3_adaptation", "m3_half_centre", "lamprey"],
	"spinal": ["m3_recruitment", "m3_reciprocal", "stretch_reflex", "m3_recurrent", "renshaw", "posture", "m3_timed_bursts", "reach", "m3_sprint", "m3_clonus"],
	"heart": ["m3_coupling", "m3_pacemaking", "heart_pacemaker", "m3_neuromod", "heart_autonomic", "m3_feedforward", "baroreflex", "m3_orthostatic"],
	"breathing": ["m3_rhythm_gating", "breathing"],
	"autonomic": ["m3_waves", "pupil", "peristalsis", "m3_divergent", "fight_or_flight", "m3_exam", "m3_horner"]}
const MOCK03_DEFS := {
	"m3_threshold": {"title": "One cell, one step", "kind": "micro", "new": ["threshold"]},
	"m3_excitation": {"title": "Relay with a delay", "kind": "micro", "new": ["excitation"], "used": ["threshold"]},
	"m3_convergence": {"title": "Three of five", "kind": "micro", "new": ["convergence"], "used": ["threshold", "excitation"]},
	"m3_inhibition": {"title": "Silence a neighbour", "kind": "micro", "new": ["inhibition"], "used": ["excitation"]},
	"m3_subtraction": {"title": "A minus B", "kind": "micro", "new": ["subtraction"], "used": ["excitation", "inhibition"]},
	"m3_adaptation": {"title": "Burst, then slow", "kind": "micro", "new": ["adaptation"], "used": ["threshold"]},
	"m3_half_centre": {"title": "Flip-flop", "kind": "micro", "new": ["half_centre"], "used": ["inhibition", "adaptation"]},
	"m3_recruitment": {"title": "Graded force", "kind": "micro", "new": ["recruitment"], "used": ["threshold", "convergence"]},
	"m3_reciprocal": {"title": "Antagonist relaxes", "kind": "micro", "new": ["reciprocal_inhibition"], "used": ["inhibition"]},
	"m3_recurrent": {"title": "Trim the gain", "kind": "micro", "new": ["recurrent_inhibition"], "used": ["inhibition", "recruitment"]},
	"m3_timed_bursts": {"title": "Three bursts in order", "kind": "micro", "new": ["timed_bursts"], "used": ["excitation", "adaptation"]},
	"m3_sprint": {"title": "Spinal sprint", "kind": "boss", "used": ["recruitment", "reciprocal_inhibition", "timed_bursts"], "note": ["Why spinal cats can walk", "Cats whose spinal cord was cut from the brain can still walk on a treadmill: the stepping rhythm is made by circuits inside the cord, and sensory feedback only tunes it.", "Brown TG (1911) Proc R Soc Lond B 84:308-319; Grillner S (1985) Science 228:143-149"]},
	"m3_clonus": {"title": "The jerking ankle", "kind": "diagnosis", "spine": false, "parent": "stretch_reflex", "used": ["recurrent_inhibition"], "sym": "Rhythmic 5 Hz jerks of the ankle when the foot is pushed up and held."},
	"m3_coupling": {"title": "Two clocks", "kind": "micro", "new": ["coupling"], "used": ["inhibition"]},
	"m3_pacemaking": {"title": "Who ticks alone?", "kind": "micro", "new": ["pacemaking"], "used": ["threshold"]},
	"m3_neuromod": {"title": "Slow the clock", "kind": "micro", "new": ["neuromodulation"], "used": ["pacemaking"]},
	"m3_feedforward": {"title": "See it coming", "kind": "micro", "new": ["feedforward"], "used": ["negative_feedback"]},
	"m3_orthostatic": {"title": "Dizzy on standing", "kind": "diagnosis", "spine": false, "parent": "baroreflex", "used": ["negative_feedback"], "sym": "Blood pressure falls and the heart rate does not rise when the patient stands up."},
	"m3_rhythm_gating": {"title": "Breathe faster", "kind": "micro", "new": ["rhythm_gating"], "used": ["adaptation"]},
	"m3_waves": {"title": "Pass it along", "kind": "micro", "new": ["travelling_waves"], "used": ["excitation", "inhibition"]},
	"m3_divergent": {"title": "One cue, many targets", "kind": "micro", "new": ["divergent_command"], "used": ["excitation"]},
	"m3_exam": {"title": "Autonomic exam", "kind": "boss", "used": ["neuromodulation", "negative_feedback", "divergent_command"]},
	"m3_horner": {"title": "One small pupil", "kind": "diagnosis", "spine": false, "parent": "pupil", "used": ["divergent_command"], "sym": "A drooping lid and a small pupil on the left side only."}}
const MOCK03_OVER := {
	"reflex": {"new": ["convergence"], "used": ["threshold", "excitation"], "variants": {"half_neurons": "With half the neurons"},
		"note": ["The knee-jerk is a two-neuron arc", "The tendon-tap reflex has just one synapse between the muscle spindle afferent and the motor neuron, which is why its delay is so short.", "Sherrington CS (1906) The Integrative Action of the Nervous System"]},
	"worm": {"new": ["subtraction"], "used": ["inhibition"], "variants": {"noise_x2": "At double the noise"},
		"note": ["Two salt neurons steer a worm", "In C. elegans the ASEL and ASER neurons respond to salt going up and down, and their difference steers the animal up a gradient.", "Suzuki H et al. (2008) Nature 454:114-117, doi:10.1038/nature06927"]},
	"lamprey": {"new": ["half_centre"], "used": ["inhibition", "adaptation"], "variants": {"izhikevich_only": "Only Izhikevich cells"},
		"note": ["A swimming rhythm in a piece of cord", "An isolated lamprey spinal cord bathed in NMDA produces fictive swimming: alternating left-right bursts with a head-to-tail delay.", "Wallen P, Williams TL (1984) J Physiol 347:225-239, doi:10.1113/jphysiol.1984.sp015063"]},
	"stretch_reflex": {"new": ["reciprocal_inhibition"], "used": ["recruitment"], "variants": {"vagus_cut": "Descending drive cut"}},
	"renshaw": {"new": ["recurrent_inhibition"], "used": ["inhibition"]},
	"heart_pacemaker": {"new": ["pacemaking"], "used": ["coupling"]},
	"baroreflex": {"new": ["negative_feedback"], "used": ["neuromodulation"], "variants": {"vagus_cut": "With the vagus cut"}}}

static func mock03() -> bool: return OS.get_environment("SP_MOCK03") != ""
static func ext_ok() -> bool: return ClassDB.class_exists("SpikeTask")
static func unlock_all() -> bool: return OS.get_environment("SP_UNLOCK_ALL") != ""

## A task object for `id`: the engine's SpikeTask, or a synthetic one for the SP_MOCK dev ids. null when unknown. The task is put in the player's current
## difficulty mode (Task::set_mode), so everything built from it (bands, noise, seeds) follows the mode.
static func make(id: String) -> Variant:
	if MockTask.has(id): var m := MockTask.new(); m.make(id); return m
	if not ext_ok(): return null
	var t = ClassDB.instantiate("SpikeTask")
	if not t.make(id): return null
	if t.has_method("set_mode"): t.set_mode(Progress.mode())
	return t

static func ref_path(id: String) -> String:
	if MockTask.has(id): return MockTask.REF
	if not ext_ok(): return ""
	return str(ClassDB.instantiate("SpikeTask").call("reference_circuit_of", id))

## Forget cached task info (a rebuilt extension may know more tasks, a changed mode changes the bands).
static func refresh() -> void: _info = {}; _ids = []; _have_ids = false; _notes = {}; _nodes = {}

## Every installed task id (the engine's, plus the mock ones under SP_MOCK / SP_MOCK03), in the engine's registry order (= play order).
static func installed() -> Array:
	if _have_ids: return _ids
	var out := []
	if ext_ok():
		for id in ClassDB.instantiate("SpikeTask").call("task_ids"): out.append(str(id))
	if mock03():
		var real := out.duplicate(); out = []
		for c in CHAPTERS:
			for id in MOCK03_ORDER.get(c["id"], []):
				if str(id).begins_with("m3_") or real.has(id): out.append(id)
		for id in real:
			if not out.has(id): out.append(id)
	if OS.get_environment("SP_MOCK") != "": out.append_array(MockTask.IDS)
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
			d["variants"] = (t.variants() as Dictionary).duplicate()
			if d["kind"] == "diagnosis": d["symptoms"] = str((t.diagnosis() as Dictionary).get("symptoms", ""))
	elif id.begins_with("m3_") and MOCK03_DEFS.has(id):
		var m: Dictionary = MOCK03_DEFS[id]
		d["title"] = str(m["title"]); d["synthetic"] = true; d["duration_ms"] = 4000.0; d["brief"] = "Synthetic level for looking at the map (SP_MOCK03). Nothing to build."
		for c in CHAPTERS:
			if (MOCK03_ORDER[c["id"]] as Array).has(id): d["chapter"] = c["id"]
	if mock03():
		var m: Dictionary = MOCK03_DEFS.get(id, MOCK03_OVER.get(id, {}))
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
		var m: Dictionary = MOCK03_DEFS.get(id, MOCK03_OVER.get(id, {}))
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
			"parent": "", "concepts_new": d["concepts_new"], "concepts_used": d["concepts_used"], "synthetic": bool(d["synthetic"])}
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

## The spine task a non-spine level hangs off: its declared parent, else the spine task of its chapter that shares most of its concepts, else the last spine
## task that comes before it in the play order, else the first spine task.
const DX_PARENT := {"dx_clonus": "renshaw", "dx_vagal": "heart_autonomic", "dx_orthostatic": "baroreflex", "dx_apnoea": "breathing", "dx_horner": "pupil", "dx_achalasia": "peristalsis"}   # a diagnosis case hangs off the task whose circuit it is (DxSpec.inner)
static func _parent_of(id: String, in_ch: Array) -> String:
	if DX_PARENT.has(id) and in_ch.has(DX_PARENT[id]): return str(DX_PARENT[id])
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
		if installed_count(str(c["id"])) > 0: out.append(str(c["id"]))
	return out

## A chapter opens when the spine of the previous chapter (that has levels) is passed, OR the spine of the chapter two before it is passed, so one stuck task
## never blocks the game. The first two chapters are open from the start. A task that sets on_spine false is a branch and never gates.
static func chapter_open(ch: String) -> bool:
	if unlock_all(): return true
	var pl := _playable_chapters(); var i := pl.find(ch)
	if i < 2: return true   # (also true for a chapter with no levels yet)
	return spine_passed(str(pl[i - 1])) or spine_passed(str(pl[i - 2]))

## The chapter whose spine opens `ch` (the previous one with levels; "" for the first two).
static func gate_chapter(ch: String) -> String:
	var pl := _playable_chapters(); var i := pl.find(ch)
	return str(pl[i - 1]) if i >= 2 else ""

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

## What gates a locked node, for the card: "Pass <level> first" or "Pass the <chapter> spine first".
static func lock_reason(id: String) -> String:
	var t := task_of(id); var ch := chapter_of(t)
	if not chapter_open(ch):
		var g := gate_chapter(ch)
		if g == "": return ""
		var pl := _playable_chapters(); var gi := pl.find(ch)
		return "Pass the %s spine (or the %s spine) first" % [str(chapter_def(g)["title"]), str(chapter_def(str(pl[gi - 2]))["title"])]
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
