class_name Progress
extends RefCounted
## Best score, pass flag and stars per task (user://progress.cfg).
## Stars: 1 = pass, 2 = pass on all 5 test seeds (the lab's TEST button, which reseeds world and circuit), 3 = that, within the neuron budget
## (par_neurons) and within the energy budget (mean spikes per second per neuron, see ENERGY_HZ). The TEST records its MEAN score, not the best.
## A circuit that is a (possibly edited) copy of a task's reference circuit is "reference-derived": it earns no stars and does not unlock anything.
const PATH := "user://progress.cfg"
## Neuron budget for the third star, per task (the reference circuits use 15 / 12 / 18; a hand-built reflex arc passes with 9).
const BUDGET := {"reflex": 10, "worm": 12, "lamprey": 18}
## Energy star: mean firing rate per neuron over a whole episode (spikes / s / neuron; times 1e8 to 1e9 ATP per spike gives the cost, Telemetry.atp_text).
## The budget is 1.6 x what the task's reference circuit spends (measured with `spike-run --task <id>`: rate_hz_per_neuron), rounded; DEFAULT_ENERGY_HZ otherwise.
const ENERGY_HZ := {"reflex": 40.0, "worm": 130.0, "lamprey": 25.0, "stretch_reflex": 60.0, "renshaw": 80.0, "posture": 60.0, "reach": 55.0,
	"heart_pacemaker": 3.0, "heart_autonomic": 14.0, "baroreflex": 35.0, "breathing": 25.0, "pupil": 30.0, "peristalsis": 10.0, "fight_or_flight": 26.0}
const DEFAULT_ENERGY_HZ := 40.0
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

## The energy budget of a task in spikes per second per neuron.
static func energy_budget(task: String) -> float: return float(ENERGY_HZ.get(task, DEFAULT_ENERGY_HZ))

## Mean spikes per second per neuron of a run: `spikes` counted over `t_ms` of model time in `neurons` cells. -1 when it cannot be told.
static func energy_hz(spikes: int, neurons: int, t_ms: float) -> float:
	if neurons <= 0 or t_ms <= 0.0: return -1.0
	return float(spikes) / float(neurons) / (t_ms * 0.001)

## Stars earned by a result: `passed` on the normal run, `five_seeds` when all 5 test seeds passed, `neurons` the circuit's size, `rate_hz` the mean spikes/s/neuron
## (negative = not measured: the energy condition is then not applied). The third star needs both budgets.
static func stars_for(task: String, passed: bool, five_seeds: bool, neurons: int, rate_hz := -1.0) -> int:
	if not passed: return 0
	if not five_seeds: return 1
	var b := budget(task)
	var size_ok := b <= 0 or neurons <= b
	var energy_ok := rate_hz < 0.0 or rate_hz <= energy_budget(task)
	return 3 if (size_ok and energy_ok) else 2

## Why a third star was not earned, for the end card: "" when it was (or the run did not qualify).
static func star3_gap(task: String, neurons: int, rate_hz: float) -> String:
	var b := budget(task)
	if b > 0 and neurons > b: return "%d neurons, par is %d" % [neurons, b]
	if rate_hz >= 0.0 and rate_hz > energy_budget(task): return "%.1f spikes/s per neuron, budget %.0f" % [rate_hz, energy_budget(task)]
	return ""

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

## The lab's entry point. `flags`: passed (bool, the mean TEST run or the single run), five_seeds (bool, all 5 TEST seeds passed), neurons (int),
## rate_hz (float, mean spikes/s/neuron, optional), reference_derived (bool). Records the MEAN score of the runs it is given (the best value on file
## only rises) and returns the stars awarded. A reference-derived circuit records nothing and returns 0.
static func record_result(task: String, score_mean: float, flags: Dictionary) -> int:
	if task == "" or bool(flags.get("reference_derived", false)): return 0
	var passed := bool(flags.get("passed", false))
	var st := stars_for(task, passed, bool(flags.get("five_seeds", false)), int(flags.get("neurons", 0)), float(flags.get("rate_hz", -1.0)))
	record(task, score_mean, passed, st)
	return st
