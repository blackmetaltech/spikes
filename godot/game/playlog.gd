class_name Playlog
extends RefCounted
## Local, opt-in play telemetry (docs/DESIGN-0.3.md section 10). Nothing leaves the device: the log is a JSON file in user://, written only while
## Settings.telemetry is on, exportable and clearable from Settings > Game. With the setting off every call here is a cheap no-op.
##
## What it keeps, per level: wall time spent in it (seconds), runs, hint tiers shown, passes, mid-level quits (left the level without passing it that
## visit), and the edits made before the first pass. Plus a capped list of raw events {t, name, task, data} for anything else.
##
## API for the lab (and anything else that plays):
##   Playlog.event(name, data := {})   record one event. data.task overrides the current level. Known names update the per-level counters:
##       "run"   (a simulation run was started or scored; data: seed, passed, value...)   "edit" (the circuit changed: add/remove/move/param)
##       "hint"  (data.tier)   "pass" (data.stars, data.mode)   "variant_pass"   "check" (instant check)   "histogram" / "diagnosis" / any other name: raw event only
##       Progress.record_hint() and Progress.record() already send "hint" and "pass", so the lab need not repeat them.
##   Playlog.level_start(task, variant := "")  / Playlog.level_end(task := "", passed := false)   the lab's open and close. The campaign map closes any open
##       level itself when it is shown, so a forgotten level_end still counts (as a quit unless Progress shows a new pass).
##   Playlog.enabled(), export_json() -> String, export_file() -> path, clear(), summary() -> Dictionary, count()
const PATH := "user://playlog.json"
const MAX_EVENTS := 4000
static var _loaded := false
static var _levels := {}      # task -> {time_s, runs, hints:{tier:n}, passes, quits, visits, edits, edits_before_pass, first_pass_runs}
static var _events := []
static var _cur := ""
static var _cur_t0 := 0
static var _cur_passed := false   # Progress said "passed" when the level opened
static var _cur_pass_seen := false
static var _dirty := false
static var _t0 := 0

static func enabled() -> bool: return Settings.telemetry

static func _load() -> void:
	if _loaded: return
	_loaded = true; _t0 = Time.get_ticks_msec()
	if Settings.no_save or OS.has_environment("SP_NOAUTOSAVE"): return
	var f := FileAccess.open(PATH, FileAccess.READ)
	if f == null: return
	var j = JSON.parse_string(f.get_as_text()); f.close()
	if j is Dictionary:
		_levels = (j.get("levels", {}) as Dictionary).duplicate(true); _events = (j.get("events", []) as Array).duplicate(true)

static func _lv(task: String) -> Dictionary:
	if not _levels.has(task):
		_levels[task] = {"time_s": 0.0, "visits": 0, "runs": 0, "hints": {}, "passes": 0, "quits": 0, "edits": 0, "edits_before_first_pass": 0, "runs_before_first_pass": 0}
	return _levels[task]

static func level_start(task: String, variant := "") -> void:
	if not enabled(): return
	_load()
	if _cur != "": level_end(_cur, false)
	if task == "": return
	_cur = task; _cur_t0 = Time.get_ticks_msec(); _cur_pass_seen = false
	_cur_passed = bool(Progress.best(task)["passed"])
	_lv(task)["visits"] = int(_lv(task)["visits"]) + 1
	_push("level_start", {"task": task, "variant": variant})

## Close the current level. `passed`: it was passed during this visit (also detected from Progress). A visit that ends without a pass is a quit.
static func level_end(task := "", passed := false) -> void:
	if _cur == "": return
	_load()
	var t := _cur; var d := _lv(t)
	d["time_s"] = snappedf(float(d["time_s"]) + float(Time.get_ticks_msec() - _cur_t0) / 1000.0, 0.1)
	var won := passed or _cur_pass_seen or (bool(Progress.best(t)["passed"]) and not _cur_passed)
	if not won: d["quits"] = int(d["quits"]) + 1
	_push("level_end", {"task": t, "passed": won})
	_cur = ""; flush()

## The campaign map calls this when it is shown: the lab has been left.
static func leave() -> void:
	if _cur != "": level_end(_cur, false)

static func event(name: String, data := {}) -> void:
	if not enabled(): return
	_load()
	var task := str(data.get("task", _cur))
	if task != "":
		var d := _lv(task); var first_pass_done := int(d["passes"]) > 0
		match name:
			"run":
				d["runs"] = int(d["runs"]) + 1
				if not first_pass_done: d["runs_before_first_pass"] = int(d["runs_before_first_pass"]) + 1
			"edit":
				d["edits"] = int(d["edits"]) + 1
				if not first_pass_done: d["edits_before_first_pass"] = int(d["edits_before_first_pass"]) + 1
			"hint":
				var h: Dictionary = d["hints"]; var k := str(int(data.get("tier", 0)))
				h[k] = int(h.get(k, 0)) + 1
			"pass":
				d["passes"] = int(d["passes"]) + 1
				if task == _cur: _cur_pass_seen = true
	_push(name, data if data.has("task") or task == "" else _with_task(data, task))

static func _with_task(data: Dictionary, task: String) -> Dictionary:
	var d := data.duplicate(); d["task"] = task; return d

static func _push(name: String, data: Dictionary) -> void:
	_events.append({"t": snappedf(float(Time.get_ticks_msec() - _t0) / 1000.0, 0.01), "name": name, "data": data})
	if _events.size() > MAX_EVENTS: _events = _events.slice(_events.size() - MAX_EVENTS)
	_dirty = true
	if _events.size() % 25 == 0: flush()

static func count() -> int:
	_load(); return _events.size()

static func summary() -> Dictionary:
	_load()
	var d := {}
	for k in _levels: d[k] = _levels[k].duplicate(true)
	if _cur != "" and d.has(_cur): d[_cur]["time_s"] = snappedf(float(d[_cur]["time_s"]) + float(Time.get_ticks_msec() - _cur_t0) / 1000.0, 0.1)
	return d

static func export_json() -> String:
	_load()
	return JSON.stringify({"format": "spike-playlog-1", "version": str(ProjectSettings.get_setting("application/config/version", "dev")), "mode": Progress.mode(),
		"enabled": enabled(), "levels": summary(), "events": _events}, "\t")

static func flush() -> void:
	if not _dirty or not enabled() or Settings.no_save or OS.has_environment("SP_NOAUTOSAVE"): return
	_dirty = false
	var f := FileAccess.open(PATH, FileAccess.WRITE)
	if f != null: f.store_string(export_json()); f.close()

## Write a dated copy the player can find: returns the absolute path ("" when it could not be written).
static func export_file() -> String:
	var name := "spike-playlog-%s.json" % Time.get_datetime_string_from_system().replace(":", "").replace("-", "")
	var dir := OS.get_system_dir(OS.SYSTEM_DIR_DOCUMENTS)
	if dir == "" or not DirAccess.dir_exists_absolute(dir): dir = ProjectSettings.globalize_path("user://")
	var path := dir.path_join(name)
	var f := FileAccess.open(path, FileAccess.WRITE)
	if f == null:
		path = ProjectSettings.globalize_path("user://").path_join(name); f = FileAccess.open(path, FileAccess.WRITE)
		if f == null: return ""
	f.store_string(export_json()); f.close(); return path

static func clear() -> void:
	_load(); _levels = {}; _events = []; _cur = ""; _dirty = false
	if not (Settings.no_save or OS.has_environment("SP_NOAUTOSAVE")) and FileAccess.file_exists(PATH): DirAccess.remove_absolute(ProjectSettings.globalize_path(PATH))
