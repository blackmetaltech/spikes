class_name Progress
extends RefCounted
## Best score, pass flag and stars per task (user://progress.cfg).
## Stars: 1 = pass, 2 = pass on 5 test seeds (the lab's TEST button), 3 = that, within the neuron budget.
const PATH := "user://progress.cfg"
## Neuron budget for the third star, per task (the reference circuits use 15 / 12 / 18; a hand-built reflex arc passes with 9).
const BUDGET := {"reflex": 10, "worm": 12, "lamprey": 18}
## The order tasks are offered in, easy to hard.
const ORDER := ["reflex", "worm", "lamprey"]
static var _best := {}
static var _loaded := false

static func _load() -> void:
	if _loaded: return
	_loaded = true
	var c := ConfigFile.new()
	if c.load(PATH) != OK: return
	for k in c.get_section_keys("best"): _best[k] = c.get_value("best", k)

static func best(task: String) -> Dictionary:
	_load()
	var b: Dictionary = (_best.get(task, {"value": 0.0, "passed": false}) as Dictionary).duplicate()
	if not b.has("stars"): b["stars"] = 1 if bool(b.get("passed", false)) else 0
	return b

static func budget(task: String) -> int: return int(BUDGET.get(task, 0))

## Stars earned by a result: `passed` on the normal run, `five_seeds` when all 5 test seeds passed, `neurons` the circuit's size.
static func stars_for(task: String, passed: bool, five_seeds: bool, neurons: int) -> int:
	if not passed: return 0
	if not five_seeds: return 1
	var b := budget(task)
	return 3 if (b <= 0 or neurons <= b) else 2

## The task that comes after `task` in ORDER, "" after the last one.
static func next_task(task: String) -> String:
	var i := ORDER.find(task)
	return str(ORDER[i + 1]) if i >= 0 and i + 1 < ORDER.size() else ""

## A task is open when it is the first, or the one before it has been passed.
static func unlocked(task: String) -> bool:
	var i := ORDER.find(task)
	return i <= 0 or bool(best(str(ORDER[i - 1]))["passed"])

static func record(task: String, value: float, passed: bool, stars := 0) -> void:
	if task == "": return
	_load(); var b: Dictionary = best(task)
	var ns := maxi(int(b["stars"]), stars if stars > 0 else (1 if passed else 0))
	if value > float(b["value"]) or (passed and not bool(b["passed"])) or ns > int(b["stars"]):
		_best[task] = {"value": maxf(value, float(b["value"])), "passed": passed or bool(b["passed"]), "stars": ns}
		if Settings.no_save or OS.has_environment("SP_NOAUTOSAVE"): return
		var c := ConfigFile.new()
		for k in _best: c.set_value("best", k, _best[k])
		c.save(PATH)
