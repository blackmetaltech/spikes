class_name LabUnlocks
extends RefCounted
## Progressive disclosure in the lab (docs/REQ-0.4.md "UI honesty", REVIEW-HARSH U1): which tools a level shows. Level 1 shows the essentials only
## (Run / Pause, Reset, Check, Brief, the add-neuron button and Connect); everything else lives in the "More" menu, and some of it only once the campaign
## gets to the level that needs it. Unlocks are data-driven: a tool opens at the first level (in play order) whose reference circuit uses it, or at the
## chapter that teaches it, and stays open once the player has got that far. The sandbox and SP_UNLOCK_ALL / SP_ALLTOOLS show everything.
##   LabUnlocks.has(task, feature)    feature: inhibitory | parts | analysis | loadsave (+ Noise is the sandbox's alone, see LabLayout.fill_palette)
##   LabUnlocks.locked_text(task)     one line for the More menu: what opens later

const FEATURE_TEXT := {"inhibitory": "inhibitory neurons", "parts": "the parts bin", "analysis": "analysis", "loadsave": "load and save"}
## Chapters whose levels are about reading wiring: the Analysis screen is open in them whatever the campaign has reached (Analysis is their instrument).
const INSTRUMENT_CHAPTERS := ["a_worm", "a_compass"]
static var _first := {}   # feature -> index in Campaign.order() of the first level that needs it

static func all_open(task_id: String) -> bool:
	return task_id == "" or Campaign.unlock_all() or OS.get_environment("SP_ALLTOOLS") != ""

## How far into the campaign the player is for the purpose of tools: the further of this level and the level after the furthest one passed.
static func reach(task_id: String) -> int:
	var o := Campaign.order(); var far := -1
	for i in o.size():
		if Campaign.passed(str(o[i])): far = i
	var here := o.find(task_id)
	if here < 0:   # a branch (variant, diagnosis case): it opens after the task it hangs off, so it is at least that far
		var parent := ""
		for n in Campaign.nodes(Campaign.chapter_of(task_id)):
			if str(n["id"]) == task_id: parent = str(n["parent"])
		here = o.find(parent) if parent != "" else o.size()
	return maxi(here, far + 1)

static func has(task_id: String, feature: String) -> bool:
	if all_open(task_id): return true
	if feature == "analysis" and INSTRUMENT_CHAPTERS.has(Campaign.chapter_of(Campaign.task_of(task_id))): return true   # the Atlas wing's instruments: the matrix, the path finder, the null bench
	return reach(task_id) >= first_level(feature)

## Index of the first level (play order) that needs `feature`; a large number when none does.
static func first_level(feature: String) -> int:
	if _first.is_empty(): _scan()
	return int(_first.get(feature, 1 << 20))

static func _scan() -> void:
	var o := Campaign.order()
	for i in o.size():
		var id := str(o[i]); var info := Campaign.info(id)
		var rc := Circuits.parse(Circuits.read_reference(Campaign.ref_path(id)))
		var neg := false; var other_model := false
		for pr in rc.get("projections", []):
			if Circuits.proj_weight(pr) < 0.0: neg = true
		for p in rc.get("populations", []):
			if str(p.get("model", "iaf_psc_alpha")) != "iaf_psc_alpha": other_model = true
		if neg and not _first.has("inhibitory"): _first["inhibitory"] = i
		if other_model and not _first.has("parts"): _first["parts"] = i
		if str(info.get("chapter", "")) != "invertebrates":
			if not _first.has("analysis"): _first["analysis"] = i
			if not _first.has("loadsave"): _first["loadsave"] = i

## What the More menu says about the tools that are not open yet ("" when everything is).
static func locked_text(task_id: String) -> String:
	if all_open(task_id): return ""
	var later := []
	for f in ["inhibitory", "parts", "analysis", "loadsave"]:
		if not has(task_id, f): later.append(FEATURE_TEXT[f])
	if later.is_empty(): return ""
	return "More tools open as the campaign needs them: " + ", ".join(later) + "."

static func refresh() -> void: _first = {}
