class_name Progress
extends RefCounted
## Best score, pass flag and stars per task (user://progress.cfg).
## Stars: 1 = pass, 2 = pass on all 5 test seeds (the lab's TEST button, which reseeds world and circuit), 3 = that, within the neuron budget
## (par_neurons) and within the energy budget (mean spikes per second per neuron, see ENERGY_HZ). The TEST records its MEAN score, not the best.
## A circuit that is a (possibly edited) copy of a task's reference circuit is "reference-derived": it earns no stars and does not unlock anything.
##
## v0.3 (docs/DESIGN-0.3.md section 5): the save also holds the difficulty mode (guided | standard | researcher, switchable at any time) and, per task,
## the result in each mode, the hint tiers used and the challenge variants done. The top-level best/passed/stars of a task are the best over all modes, so
## unlocking never depends on the mode; star labels and the per-mode row on the campaign card show the mode.
##   Progress.mode() / set_mode(m)                       the current difficulty mode (saved)
##   Progress.best(task)                                 {value, passed, stars, modes:{mode:{value,passed,stars}}, hints:{tier:count}, variants:[ids]}
##   Progress.best_in(task, mode)                        the result in one mode
##   Progress.record(task, value, passed, stars, mode)   mode defaults to the current one; record_result() passes it through
##   Progress.record_hint(task, tier)                    the lab calls it when a hint tier is shown (recorded, never punished)
##   Progress.record_variant(task, variant)              the lab calls it when a challenge variant is passed
##   Progress.hints_used(task) -> {max_tier, count}      Progress.variants_done(task) -> Array
const MODES := ["guided", "standard", "researcher"]
const MODE_NAMES := {"guided": "Guided", "standard": "Standard", "researcher": "Researcher"}
const MODE_BLURB := {
	"guided": "The recipe is visible, hints are free, one seed passes, noise is low and the pass bands are wide.",
	"standard": "The recipe is behind a button, hints open after two failed runs, one seed passes (five for the second star).",
	"researcher": "No recipe, no hints. Five seeds must pass, physiological noise, tight bands."}
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
static var _mode := "standard"

static func _load() -> void:
	if _loaded: return
	_loaded = true
	if OS.get_environment("SP_PROGRESS") != "":   # dev hook for screenshots: "reflex:3:92,worm:2:81,lamprey:0" = id:stars:best score
		for part in OS.get_environment("SP_PROGRESS").split(","):
			var f := part.split(":")
			if f.size() >= 2: _best[f[0]] = {"value": float(f[2]) if f.size() > 2 else 80.0, "passed": int(f[1]) > 0, "stars": int(f[1])}
		if OS.get_environment("SP_MODE") in MODES: _mode = OS.get_environment("SP_MODE")
		for k in _best: _best[k]["modes"] = {_mode: {"value": _best[k]["value"], "passed": _best[k]["passed"], "stars": _best[k]["stars"]}}
		return
	var c := ConfigFile.new()
	if c.load(PATH) != OK: return
	for k in c.get_section_keys("best"): _best[k] = c.get_value("best", k)
	var m := str(c.get_value("meta", "mode", "standard")); _mode = m if m in MODES else "standard"

static func best(task: String) -> Dictionary:
	_load()
	var b: Dictionary = (_best.get(task, {"value": 0.0, "passed": false}) as Dictionary).duplicate(true)
	if not b.has("stars"): b["stars"] = 1 if bool(b.get("passed", false)) else 0
	if not b.has("modes"): b["modes"] = {}
	if not b.has("hints"): b["hints"] = {}
	if not b.has("variants"): b["variants"] = []
	return b

static func mode() -> String:
	_load(); return _mode
static func set_mode(m: String) -> void:
	_load()
	if not m in MODES or m == _mode: return
	_mode = m; Campaign.refresh(); _save()   # tasks scale their bands with the mode, so cached task facts are stale
static func mode_name(m := "") -> String: return str(MODE_NAMES.get(m if m != "" else mode(), "Standard"))

## The result in one mode: {value, passed, stars}.
static func best_in(task: String, m: String) -> Dictionary:
	var r: Dictionary = (best(task)["modes"] as Dictionary).get(m, {})
	return {"value": float(r.get("value", 0.0)), "passed": bool(r.get("passed", false)), "stars": int(r.get("stars", 0))}

static func hints_used(task: String) -> Dictionary:
	var h: Dictionary = best(task)["hints"]; var mx := 0; var n := 0
	for k in h: mx = maxi(mx, int(k)); n += int(h[k])
	return {"max_tier": mx, "count": n}

static func variants_done(task: String) -> Array: return best(task)["variants"]

static func record_hint(task: String, tier: int) -> void:
	if task == "" or tier < 1: return
	_load(); var b := best(task); var h: Dictionary = b["hints"]; h[str(tier)] = int(h.get(str(tier), 0)) + 1
	_best[task] = b; _save(); Playlog.event("hint", {"task": task, "tier": tier})

static func record_variant(task: String, variant: String) -> void:
	if task == "" or variant == "": return
	_load(); var b := best(task)
	if not (b["variants"] as Array).has(variant): (b["variants"] as Array).append(variant)
	_best[task] = b; _save(); Playlog.event("variant_pass", {"task": task, "variant": variant})

static func _save() -> void:
	if Settings.no_save or OS.has_environment("SP_NOAUTOSAVE") or OS.get_environment("SP_PROGRESS") != "": return
	var c := ConfigFile.new()
	for k in _best: c.set_value("best", k, _best[k])
	c.set_value("meta", "mode", _mode)
	c.save(PATH)

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

static func record(task: String, value: float, passed: bool, stars := 0, in_mode := "") -> void:
	if task == "": return
	_load(); var b: Dictionary = best(task); var m := in_mode if in_mode in MODES else _mode
	var st := stars if stars > 0 else (1 if passed else 0)
	var ns := maxi(int(b["stars"]), st)
	var first_pass := passed and not bool(b["passed"])
	var pm: Dictionary = b["modes"]; var cur: Dictionary = pm.get(m, {"value": 0.0, "passed": false, "stars": 0})
	var changed := false
	if value > float(cur["value"]) or (passed and not bool(cur["passed"])) or st > int(cur["stars"]):
		pm[m] = {"value": maxf(value, float(cur["value"])), "passed": passed or bool(cur["passed"]), "stars": maxi(int(cur["stars"]), st)}; changed = true
	if value > float(b["value"]) or first_pass or ns > int(b["stars"]): changed = true
	if not changed: return
	b["value"] = maxf(value, float(b["value"])); b["passed"] = passed or bool(b["passed"]); b["stars"] = ns; b["modes"] = pm
	_best[task] = b; _save()
	if passed: Playlog.event("pass", {"task": task, "stars": st, "mode": m, "first": first_pass})

## The lab's entry point. `flags`: passed (bool, the mean TEST run or the single run), five_seeds (bool, all 5 TEST seeds passed), neurons (int),
## rate_hz (float, mean spikes/s/neuron, optional), reference_derived (bool). Records the MEAN score of the runs it is given (the best value on file
## only rises) and returns the stars awarded. A reference-derived circuit records nothing and returns 0.
static func record_result(task: String, score_mean: float, flags: Dictionary) -> int:
	if task == "" or bool(flags.get("reference_derived", false)): return 0
	var passed := bool(flags.get("passed", false))
	var st := stars_for(task, passed, bool(flags.get("five_seeds", false)), int(flags.get("neurons", 0)), float(flags.get("rate_hz", -1.0)))
	record(task, score_mean, passed, st, str(flags.get("mode", "")))
	return st
