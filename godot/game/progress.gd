class_name Progress
extends RefCounted
## Best score and pass flag per task (user://progress.cfg).
const PATH := "user://progress.cfg"
static var _best := {}
static var _loaded := false

static func _load() -> void:
	if _loaded: return
	_loaded = true
	var c := ConfigFile.new()
	if c.load(PATH) != OK: return
	for k in c.get_section_keys("best"): _best[k] = c.get_value("best", k)

static func best(task: String) -> Dictionary:
	_load(); return _best.get(task, {"value": 0.0, "passed": false})

static func record(task: String, value: float, passed: bool) -> void:
	_load(); var b: Dictionary = best(task)
	if value > float(b["value"]) or (passed and not bool(b["passed"])):
		_best[task] = {"value": maxf(value, float(b["value"])), "passed": passed or bool(b["passed"])}
		if Settings.no_save or OS.has_environment("SP_NOAUTOSAVE"): return
		var c := ConfigFile.new()
		for k in _best: c.set_value("best", k, _best[k])
		c.save(PATH)
