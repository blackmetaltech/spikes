class_name Campaign
extends RefCounted
## The 0.2 campaign: five chapters, each an organ on the anatomical map, each a handful of task nodes. The engine decides which tasks exist and which
## chapter each belongs to (Task::chapter()); this file only knows the chapters, the planned lineup from docs/REQ-0.2.md section C (so the map shows
## "not installed yet" ghosts where a task has not landed), and where each node sits on its organ. Unknown tasks from the engine are placed after
## the planned ones of their chapter, so new tasks appear on the map without a UI change.
##
## Dev hooks:  SP_MOCK=1 adds synthetic tasks (game/mock_task.gd) so the body views can be looked at before the engine has them
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

static var _info := {}
static var _ids := []
static var _have_ids := false

static func ext_ok() -> bool: return ClassDB.class_exists("SpikeTask")
static func unlock_all() -> bool: return OS.get_environment("SP_UNLOCK_ALL") != ""

## A task object for `id`: the engine's SpikeTask, or a synthetic one for the SP_MOCK dev ids. null when unknown.
static func make(id: String) -> Variant:
	if MockTask.has(id): var m := MockTask.new(); m.make(id); return m
	if not ext_ok(): return null
	var t = ClassDB.instantiate("SpikeTask")
	return t if t.make(id) else null

static func ref_path(id: String) -> String:
	if MockTask.has(id): return MockTask.REF
	if not ext_ok(): return ""
	return str(ClassDB.instantiate("SpikeTask").call("reference_circuit_of", id))

## Forget cached task info (a rebuilt extension may know more tasks).
static func refresh() -> void: _info = {}; _ids = []; _have_ids = false

## Every installed task id (the engine's, plus the mock ones under SP_MOCK).
static func installed() -> Array:
	if _have_ids: return _ids
	var out := []
	if ext_ok():
		for id in ClassDB.instantiate("SpikeTask").call("task_ids"): out.append(str(id))
	if OS.get_environment("SP_MOCK") != "": out.append_array(MockTask.IDS)
	_ids = out; _have_ids = true; return out

static func is_installed(id: String) -> bool: return installed().has(id)

## Static facts of a task, cached: title, brief, chapter, view, par, duration, channels, timeline, bands, modulators.
static func info(id: String) -> Dictionary:
	if _info.has(id): return _info[id]
	var t = make(id)
	var d := {"id": id, "ok": t != null, "title": id.capitalize(), "brief": "", "chapter": "invertebrates", "view": "polyline", "par": 0, "duration_ms": 0.0,
		"sensors": [], "motors": [], "timeline": [], "bands": [], "modulators": []}
	if t != null:
		d["title"] = str(t.title()); d["brief"] = str(t.brief()); d["duration_ms"] = float(t.duration_ms())
		d["sensors"] = Array(t.sensor_channels()); d["motors"] = Array(t.motor_channels())
		var ch := str(t.chapter()); var vw := str(t.view())
		d["chapter"] = ch if ch != "" else "invertebrates"; d["view"] = vw if vw != "" else "polyline"; d["par"] = int(t.par_neurons())
		d["timeline"] = Array(t.timeline()); d["bands"] = Array(t.bands()); d["modulators"] = Array(t.modulators())
	_info[id] = d; return d

static func chapter_index(ch: String) -> int:
	for i in CHAPTERS.size():
		if CHAPTERS[i]["id"] == ch: return i
	return 0

static func chapter_def(ch: String) -> Dictionary: return CHAPTERS[chapter_index(ch)]

## Nodes of a chapter in play order: [{id, title, installed, pos}]. The engine's registry order decides the order of installed tasks (so there is one
## source of truth); PLANNED only supplies the map positions and the ghost nodes for tasks that have not landed. Planned ghosts come last.
static func nodes(ch: String) -> Array:
	var out := []; var pos := {}; var title := {}
	for p in PLANNED.get(ch, []): pos[p[0]] = p[2]; title[p[0]] = p[1]
	var spare := 0
	for id in installed():
		if str(info(id)["chapter"]) != ch: continue
		var at: Vector2
		if pos.has(id): at = pos[id]
		else: at = SPARE[spare % SPARE.size()]; spare += 1
		out.append({"id": id, "title": str(info(id)["title"]), "installed": true, "pos": at})
	for p in PLANNED.get(ch, []):
		if not is_installed(p[0]): out.append({"id": p[0], "title": p[1], "installed": false, "pos": p[2]})
	return out

## Installed task ids in play order across all chapters.
static func order() -> Array:
	var out := []
	for c in CHAPTERS:
		for n in nodes(c["id"]):
			if n["installed"]: out.append(n["id"])
	return out

static func chapter_of(id: String) -> String: return str(info(id)["chapter"])

static func passed_count(ch: String) -> int:
	var k := 0
	for n in nodes(ch):
		if n["installed"] and bool(Progress.best(n["id"])["passed"]): k += 1
	return k

static func installed_count(ch: String) -> int:
	var k := 0
	for n in nodes(ch):
		if n["installed"]: k += 1
	return k

static func star_total(ch: String) -> Vector2i:
	var got := 0; var all := 0
	for n in nodes(ch):
		if n["installed"]: got += int(Progress.best(n["id"])["stars"]); all += 3
	return Vector2i(got, all)

## A chapter opens when the previous chapter that has tasks has half of them (rounded up) passed. The first chapter is always open.
static func chapter_open(ch: String) -> bool:
	if unlock_all(): return true
	var i := chapter_index(ch)
	while i > 0:
		i -= 1
		var pc: String = CHAPTERS[i]["id"]; var n := installed_count(pc)
		if n == 0: continue
		return chapter_open(pc) and passed_count(pc) >= ceili(float(n) / 2.0)
	return true

## A node opens when the installed node before it in its chapter is passed; the chapter's first node opens with the chapter.
static func unlocked(id: String) -> bool:
	if unlock_all(): return true
	var ch := chapter_of(id)
	if not chapter_open(ch): return false
	var prev := ""
	for n in nodes(ch):
		if n["id"] == id: break
		if n["installed"]: prev = n["id"]
	return prev == "" or bool(Progress.best(prev)["passed"])

## What gates a locked node, for the card: "Pass <task> first" or "Reach <chapter> first".
static func lock_reason(id: String) -> String:
	var ch := chapter_of(id)
	if not chapter_open(ch):
		var i := chapter_index(ch)
		return "Pass half of %s first" % str(CHAPTERS[maxi(i - 1, 0)]["title"])
	var prev := ""
	for n in nodes(ch):
		if n["id"] == id: break
		if n["installed"]: prev = n["title"]
	return "Pass %s first" % prev if prev != "" else ""

## Next installed task after `id` in play order, "" after the last.
static func next_task(id: String) -> String:
	var o := order(); var i := o.find(id)
	return str(o[i + 1]) if i >= 0 and i + 1 < o.size() else ""

## The chapters whose parts are available: every chapter up to and including the furthest open one.
static func chapters_unlocked() -> Array:
	var out := []
	for c in CHAPTERS:
		if chapter_open(c["id"]): out.append(c["id"])
	return out
