class_name Progress
extends RefCounted
## Best score, pass flag and stars per task (user://progress.cfg).
## Stars: 1 = pass, 2 = pass on 5 test seeds (the lab's TEST button), 3 = that, within the neuron budget (par_neurons).
const PATH := "user://progress.cfg"
## Neuron budget for the third star, per task (the reference circuits use 15 / 12 / 18; a hand-built reflex arc passes with 9).
const BUDGET := {"reflex": 10, "worm": 12, "lamprey": 18}
static var _best := {}
static var _loaded := false

static func _load() -> void:
	if _loaded: return
	_loaded = true
	if OS.get_environment("SP_PROGRESS") != "":   # dev hook for screenshots: "reflex:3:92,worm:2:81,lamprey:0" = id:stars:best score
		for part in OS.get_environment("SP_PROGRESS").split(","):
			var f := part.split(":")
			if f.size() >= 2: _best[f[0]] = {"value": float(f[2]) if f.size() > 2 else 80.0, "passed": int(f[1]) > 0, "stars": int(f[1])}
		return
	var c := ConfigFile.new()
	if c.load(PATH) != OK: return
	for k in c.get_section_keys("best"): _best[k] = c.get_value("best", k)

static func best(task: String) -> Dictionary:
	_load()
	var b: Dictionary = (_best.get(task, {"value": 0.0, "passed": false}) as Dictionary).duplicate()
	if not b.has("stars"): b["stars"] = 1 if bool(b.get("passed", false)) else 0
	return b

## The task's own par (Task::par_neurons) when the engine gives one, else the 0.1 table.
static func budget(task: String) -> int:
	if task == "": return 0
	var par := int(Campaign.info(task).get("par", 0))
	return par if par > 0 else int(BUDGET.get(task, 0))

## Stars earned by a result: `passed` on the normal run, `five_seeds` when all 5 test seeds passed, `neurons` the circuit's size.
static func stars_for(task: String, passed: bool, five_seeds: bool, neurons: int) -> int:
	if not passed: return 0
	if not five_seeds: return 1
	var b := budget(task)
	return 3 if (b <= 0 or neurons <= b) else 2

## The task that comes after `task` in the campaign, "" after the last one.
static func next_task(task: String) -> String: return Campaign.next_task(task)

## A task is open when its chapter is open and the one before it in the chapter has been passed (see Campaign).
static func unlocked(task: String) -> bool: return Campaign.unlocked(task)

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
