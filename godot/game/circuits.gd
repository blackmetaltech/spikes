class_name Circuits
extends RefCounted
## The circuit as the editor sees it: the parsed circuit JSON (docs/CIRCUIT-FORMAT.md) kept as a Dictionary, plus helpers for the things the
## editor needs (neuron list, excitatory / inhibitory kind, defaults per NEST model, reading reference circuits, save / load).
## The Dictionary is the file: unknown keys are preserved on save. Editor-only state lives under meta.editor.

const MODELS := ["iaf_psc_alpha", "aeif_cond_alpha", "izhikevich", "hh_psc_alpha"]

## Presets per model: [name, params, note]. Parameter names and units are NEST's (mV, ms, pA, nS, pF).
## Izhikevich 2003 (IEEE Trans Neural Netw 14:1569) for izhikevich; Naud et al. 2008 (Biol Cybern 99:335) for aeif_cond_alpha.
const PRESETS := {
	"iaf_psc_alpha": [["Default", {}, "Leaky integrate-and-fire, alpha-shaped synaptic current."], ["Fast integrator", {"tau_m": 6.0}, "Short membrane time constant: coincidence detector."], ["Slow integrator", {"tau_m": 30.0}, "Long membrane time constant: sums inputs over time."]],
	"aeif_cond_alpha": [["NEST default", {}, "a 4 nS, b 80.5 pA: adapting."], ["Tonic", {"a": 2.0, "b": 0.0, "tau_w": 30.0, "V_reset": -58.0}, "Regular firing, no adaptation (Naud 2008)."], ["Adapting", {"a": 2.0, "b": 60.0, "tau_w": 500.0, "V_reset": -58.0}, "Spike-triggered adaptation slows the firing (Naud 2008 adapting class)."], ["Bursting", {"a": -0.5, "b": 7.0, "tau_w": 5.0, "V_reset": -46.0}, "Fires in bursts (Naud 2008)."]],
	"izhikevich": [["Regular spiking", {"a": 0.02, "b": 0.2, "c": -65.0, "d": 8.0}, "Cortical excitatory cell."], ["Intrinsic bursting", {"a": 0.02, "b": 0.2, "c": -55.0, "d": 4.0}, "Starts with a burst, then spikes."], ["Chattering", {"a": 0.02, "b": 0.2, "c": -50.0, "d": 2.0}, "Fast rhythmic bursts."], ["Fast spiking", {"a": 0.1, "b": 0.2, "c": -65.0, "d": 2.0}, "Cortical inhibitory interneuron."]],
	"hh_psc_alpha": [["NEST default", {}, "Hodgkin-Huxley squid axon."]]}

## Fallback synaptic weights for cards whose catalogue entry has no measured default weight (an empty unit marks the placeholder 1.0): a typical single
## connection per model, in the model's own synapse units (pA for current-based, nS for conductance-based). The catalogue is the one table; this only fills gaps.
const WEIGHT_FALLBACK := {"izhikevich": 25.0, "hh_psc_alpha": 400.0}
static var sandbox := false   # set by the lab: every part is available (free run)
const SYNAPSES := ["static_synapse"]   # plasticity (STDP, Tsodyks) is the 0.2 theme; circuits that already use it still load

## Presets of any model: the editor's table, else the catalogue cards of that model (each card carries its preset parameters), else one default.
static func presets_of(model: String) -> Array:
	if PRESETS.has(model): return PRESETS[model]
	var out := []
	for c in Catalog.cards_of(model): out.append([str(c["title"]), (c["preset"] as Dictionary).duplicate(), str(c["summary"])])
	return out if not out.is_empty() else [["Default", {}, ""]]

## Weight of a typical synapse onto `model`: the catalogue's default input weight (the measured one, in the card's unit), else the small fallback table,
## else nS for conductance models and pA for current ones.
static func weight_of(model: String) -> float:
	for c in Catalog.cards_of(model):
		if float(c["input_weight"]) > 0.0 and str(c["weight_unit"]) != "": return float(c["input_weight"])
	if WEIGHT_FALLBACK.has(model): return float(WEIGHT_FALLBACK[model])
	var w := Catalog.input_weight_of(model)
	return w if w > 0.0 and w != 1.0 else (20.0 if model.contains("cond") else 100.0)

## Largest weight the projection slider offers onto `model`: 20 typical synapses (the slider is square-law, so small weights stay easy to set), in the card's unit.
static func proj_wmax(model: String) -> float: return 20.0 * weight_of(model)

## True when the circuit has a feedback loop: a cycle of connections (a population that projects, directly or through others, back onto itself; a self-connection counts). A
## connection with weight 0 is cut and does not count. A rhythm needs one (or a cell that pulses by itself), so a loop-free circuit cannot have "built" one.
static func has_loop(c: Dictionary) -> bool:
	var adj := {}
	for pr in c.get("projections", []):
		if is_zero_approx(proj_weight(pr)) and not (pr.has("edges") and not (pr["edges"] as Array).is_empty()): continue
		var a := proj_pop(pr.get("pre", "")); var b := proj_pop(pr.get("post", ""))
		if a == b: return true
		if not adj.has(a): adj[a] = []
		(adj[a] as Array).append(b)
	var state := {}   # 0 unseen, 1 on the stack, 2 done
	var cyc := [false]   # (an array: a lambda cannot assign to a captured local)
	var visit := func(self_fn: Callable, n: String) -> void:
		state[n] = 1
		for m in adj.get(n, []):
			if cyc[0]: return
			if int(state.get(m, 0)) == 1: cyc[0] = true; return
			if int(state.get(m, 0)) == 0: self_fn.call(self_fn, m)
		state[n] = 2
	for n in adj.keys():
		if cyc[0]: break
		if int(state.get(n, 0)) == 0: visit.call(visit, n)
	return cyc[0]

## The slider range of a level: the model's own range, and at least TWICE the largest weight the level's reference circuit uses onto that model (a_depress needs 8000 pA, aplysia_gill 2500,
## w_switch and worm_touch several thousand of inhibition), so every reference weight is reachable with room above it. `tid` "" (sandbox, other levels' parts) = the model's range.
static func proj_wmax_for(model: String, tid: String) -> float:
	var m := proj_wmax(model)
	if tid != "": m = maxf(m, 2.0 * float(_ref_wmax(tid).get("proj:" + model, 0.0)))
	return m
## Largest input weight for a sensor of `kind` ("dc" / "poisson") onto `model`, likewise at least twice the reference's.
static func input_wmax_for(model: String, kind: String, tid: String) -> float:
	var m := DC_WMAX if kind == "dc" else proj_wmax(model)
	if tid != "": m = maxf(m, 2.0 * float(_ref_wmax(tid).get("in:" + kind, 0.0)))
	return m

static var _ref_wmax_cache := {}
## {"proj:<post model>": max |w|, "in:<kind>": max |w|} over task `tid`'s reference circuit (cached; {} when it has none).
static func _ref_wmax(tid: String) -> Dictionary:
	if _ref_wmax_cache.has(tid): return _ref_wmax_cache[tid]
	var out := {}
	var rc := parse(read_reference(Campaign.ref_path(tid))) if tid != "" else {}
	if not rc.is_empty():
		var model_of := {}
		for p in rc.get("populations", []): model_of[str(p.get("id", ""))] = str(p.get("model", ""))
		for pr in rc.get("projections", []):
			var k := "proj:" + str(model_of.get(proj_pop(pr.get("post", "")), ""))
			out[k] = maxf(float(out.get(k, 0.0)), absf(proj_weight(pr)))
		for ch in rc.get("inputs", []):
			var k2 := "in:" + str(ch.get("kind", "poisson"))
			out[k2] = maxf(float(out.get(k2, 0.0)), absf(float(ch.get("weight", 0.0))))
	_ref_wmax_cache[tid] = out
	return out

## True when some connection or sensor weight of `c` sits at the end of its slider (within 3 %): more strength is not available there.
static func at_weight_cap(c: Dictionary, tid: String) -> bool:
	var model_of := {}
	for p in c.get("populations", []): model_of[str(p.get("id", ""))] = str(p.get("model", ""))
	for pr in c.get("projections", []):
		var cap := proj_wmax_for(str(model_of.get(proj_pop(pr.get("post", "")), "")), tid)
		if absf(proj_weight(pr)) >= 0.97 * cap: return true
	for ch in c.get("inputs", []):
		var tg: Array = ch.get("targets", [])
		var tm := str(model_of.get(proj_pop(tg[0]), "")) if not tg.is_empty() else ""
		if float(ch.get("weight", 0.0)) >= 0.97 * input_wmax_for(tm, str(ch.get("kind", "poisson")), tid): return true
	return false

## A verdict that asks for more strength ("strengthen", "stronger", "raise ...") is not shown while a weight is already at its slider cap: the answer is then in the structure.
static func cap_verdict(verdict: String, c: Dictionary, tid: String) -> String:
	if tid == "" or verdict == "": return verdict
	var lv := verdict.to_lower()
	if not (("strengthen" in lv) or ("stronger" in lv) or ("raise" in lv) or ("more strength" in lv)): return verdict
	if not at_weight_cap(c, tid): return verdict
	return "A weight is already at the end of its slider, so more strength is not the answer: try more cells in a group, another cell type, or a different route."
## Largest weight of a dc input: pA of current per unit of the sensor's value, whichever cell it drives (a conductance cell still takes a current).
const DC_WMAX := 2000.0
## Longest synaptic delay the slider offers (ms), square-law like the weights: 1-3 ms for a synapse, 100-250 ms to stand for a polysynaptic chain (a burst generator).
const DELAY_MAX := 250.0

## Advanced parameter rows of `model`: the editor's table, else derived from the catalogue card's parameters and ranges.
static func advanced_of(model: String) -> Array:
	return ADVANCED[model] if ADVANCED.has(model) else Catalog.advanced_of(model)

## "Advanced" neuron parameters per model, in NEST's names and units: [key, label, min, max, step, NEST default, unit]. `V_m` is the initial membrane
## potential: different starting values are what lets two identical cells break symmetry (a half-centre oscillator needs that).
const ADVANCED := {
	"iaf_psc_alpha": [["V_m", "start V", -85.0, -50.0, 0.5, -70.0, "mV"], ["tau_m", "tau m", 2.0, 60.0, 0.5, 10.0, "ms"], ["V_th", "threshold", -60.0, -40.0, 0.5, -55.0, "mV"], ["I_e", "bias", -400.0, 800.0, 5.0, 0.0, "pA"], ["tau_syn_ex", "tau syn E", 0.2, 50.0, 0.1, 2.0, "ms"], ["tau_syn_in", "tau syn I", 0.2, 50.0, 0.1, 2.0, "ms"]],
	"aeif_cond_alpha": [["V_m", "start V", -85.0, -45.0, 0.5, -70.6, "mV"], ["a", "a", -2.0, 10.0, 0.1, 4.0, "nS"], ["b", "b", 0.0, 200.0, 1.0, 80.5, "pA"], ["tau_w", "tau w", 5.0, 400.0, 1.0, 144.0, "ms"], ["I_e", "bias", -400.0, 1000.0, 5.0, 0.0, "pA"], ["tau_syn_ex", "tau syn E", 0.2, 50.0, 0.1, 0.2, "ms"], ["tau_syn_in", "tau syn I", 0.2, 50.0, 0.1, 2.0, "ms"]],
	"izhikevich": [["V_m", "start V", -85.0, -45.0, 0.5, -65.0, "mV"], ["a", "a", 0.01, 0.15, 0.005, 0.02, ""], ["b", "b", 0.1, 0.3, 0.005, 0.2, ""], ["c", "c", -70.0, -45.0, 0.5, -65.0, "mV"], ["d", "d", 0.0, 10.0, 0.5, 8.0, ""], ["I_e", "bias", -10.0, 30.0, 0.5, 0.0, "pA"]],
	"hh_psc_alpha": [["V_m", "start V", -85.0, -50.0, 0.5, -65.0, "mV"], ["I_e", "bias", -100.0, 600.0, 5.0, 0.0, "pA"], ["tau_syn_ex", "tau syn E", 0.2, 50.0, 0.1, 2.0, "ms"], ["tau_syn_in", "tau syn I", 0.2, 50.0, 0.1, 2.0, "ms"]],
	"stg_cell": [["I_e", "bias", -200.0, 200.0, 1.0, 0.0, "pA"], ["g_Na", "g Na", 0.0, 300000.0, 100.0, 62800.0, "nS"], ["g_Kd", "g Kd", 0.0, 100000.0, 100.0, 62800.0, "nS"], ["g_CaT", "g CaT", 0.0, 5000.0, 10.0, 1570.0, "nS"], ["g_CaS", "g CaS", 0.0, 10000.0, 10.0, 3768.0, "nS"], ["g_A", "g A", 0.0, 60000.0, 100.0, 31400.0, "nS"], ["g_KCa", "g KCa", 0.0, 10000.0, 10.0, 3140.0, "nS"], ["g_H", "g H", 0.0, 100.0, 0.1, 6.28, "nS"], ["g_L", "g leak", 0.0, 50.0, 0.1, 6.28, "nS"]]}

## Default weight of a sensor / noise input onto a neuron of `model`: several internal synapses (weight_of), so a lone neuron wired straight from a
## 400 Hz sensor fires at once instead of sitting silent (as the old 1.2 x 250 pA did for iaf). Dc inputs keep their own scale (pA of drive).
static func input_weight(model: String, kind: String) -> float:
	if kind == "dc": return 300.0
	return snappedf(weight_of(model) * 3.5, 0.5)

## The first-connect weight of input channel `ch` onto a cell of `model`: the weight the task declared for that sensor (kept in meta.editor.task_weights = {name: {kind, w}} by the
## lab) else the heuristic input_weight().
static func first_input_weight(c: Dictionary, ch: Dictionary, model: String) -> float:
	var tw: Dictionary = c.get("meta", {}).get("editor", {}).get("task_weights", {})
	var kind := str(ch.get("kind", "poisson")); var e: Variant = tw.get(str(ch.get("name", "")))
	if e is Dictionary and str(e.get("kind", "")) == kind and float(e.get("w", 0.0)) > 0.0: return float(e["w"])
	return input_weight(model, kind)

static func empty(name := "New circuit") -> Dictionary:
	return {"schema_version": 1, "name": name, "resolution_ms": 0.1, "seed": 1, "populations": [], "projections": [], "inputs": [], "outputs": [], "record": {"spikes": "all"}, "meta": {"author": "", "notes": ""}}

static func parse(json: String) -> Dictionary:
	if json.strip_edges() == "": return {}
	var v: Variant = JSON.parse_string(json)
	return v if v is Dictionary else {}

# ---------- validation (a circuit file is untrusted: Load, an autosave a crash left damaged, SP_CIRCUIT) ----------
## The neuron limit is the engine's (engine/src/circuit.cpp kMaxNeurons, handed over by SpikeTask.max_neurons()): one limit, so the game never refuses a circuit
## the engine accepts or the other way round. FALLBACK_MAX_NEURONS is used only where the extension is not loaded (the headless tests of the save layer); a test
## in engine/tests/tasks_rules_test.cpp keeps it equal to the engine's.
const FALLBACK_MAX_NEURONS := 20000
static var _max_neurons := 0
static func max_neurons() -> int:
	if _max_neurons <= 0:   # asked once: validate() calls this for every population
		_max_neurons = FALLBACK_MAX_NEURONS
		if ClassDB.class_exists("SpikeTask") and ClassDB.class_has_method("SpikeTask", "max_neurons"): _max_neurons = int(ClassDB.instantiate("SpikeTask").call("max_neurons"))
	return _max_neurons
const MAX_PROJECTIONS := 20000
const MAX_EDGES := 200000
const MAX_CHANNELS := 500
const MAX_DEPTH := 24

static func _is_num(v: Variant) -> bool: return (v is float or v is int) and is_finite(float(v))
static func _is_int_val(v: Variant) -> bool: return _is_num(v) and absf(float(v)) < 1e9 and is_equal_approx(float(v), roundf(float(v)))

## "" when the circuit is safe to hand to the editor and the engine, else the first problem in plain words. Field types, n an integer 1..max_neurons(), finite numbers,
## totals capped at max_neurons() neurons and MAX_PROJECTIONS projections. (Whether the models and parameters mean anything is the engine's job: its banner.)
static func validate(c: Variant) -> String:
	if not c is Dictionary: return "not a circuit (the file is not a JSON object)"
	var d: Dictionary = c
	for key in ["populations", "projections", "inputs", "outputs"]:
		if d.has(key) and not d[key] is Array: return "\"%s\" must be a list" % key
	for key in ["meta", "record"]:
		if d.has(key) and not d[key] is Dictionary: return "\"%s\" must be an object" % key
	for key in ["seed", "resolution_ms", "schema_version"]:
		if d.has(key) and not _is_num(d[key]): return "\"%s\" must be a finite number" % key
	if d.has("resolution_ms") and float(d["resolution_ms"]) <= 0.0: return "\"resolution_ms\" must be positive"
	if d.has("name") and not d["name"] is String: return "\"name\" must be text"
	var depth := _depth_error(d, 0)
	if depth != "": return depth
	var total := 0; var ids := {}
	for p in d.get("populations", []):
		if not p is Dictionary: return "a population is not an object"
		var pd: Dictionary = p
		var id: Variant = pd.get("id", null)
		if not id is String or (id as String) == "" or (id as String).length() > 64: return "every population needs a text id (up to 64 characters)"
		if ids.has(id): return "two populations are called \"%s\"" % id
		ids[id] = true
		if not pd.get("model", null) is String: return "population %s: \"model\" must be text" % id
		if pd.has("n") and (not _is_int_val(pd["n"]) or int(pd["n"]) < 1 or int(pd["n"]) > max_neurons()): return "population %s: \"n\" must be a whole number from 1 to %d" % [id, max_neurons()]
		total += pop_n(pd)
		if total > max_neurons(): return "more than %d neurons in all" % max_neurons()
		if pd.has("params"):
			if not pd["params"] is Dictionary: return "population %s: \"params\" must be an object" % id
			for k in pd["params"]:
				var v: Variant = pd["params"][k]
				if v is float and not is_finite(v): return "population %s: parameter %s is not a finite number" % [id, k]
		if pd.has("pos") and not pd["pos"] is Array: return "population %s: \"pos\" must be a list" % id
	if (d.get("projections", []) as Array).size() > MAX_PROJECTIONS: return "more than %d projections" % MAX_PROJECTIONS
	var edges := 0
	for pr in d.get("projections", []):
		if not pr is Dictionary: return "a projection is not an object"
		var pj: Dictionary = pr
		for end in ["pre", "post"]:
			var r: Variant = pj.get(end, null)
			if not (r is String or r is Dictionary): return "a projection's \"%s\" must be a population id" % end
		if pj.has("synapse") and not pj["synapse"] is Dictionary: return "a projection's \"synapse\" must be an object"
		if pj.has("edges"):
			if not pj["edges"] is Array: return "a projection's \"edges\" must be a list"
			for e in pj["edges"]:
				if not e is Array or (e as Array).size() < 2: return "an edge must be a list [pre, post, weight, delay]"
				for x in e:
					if x is float and not is_finite(x): return "an edge holds a number that is not finite"
			edges += (pj["edges"] as Array).size()
			if edges > MAX_EDGES: return "more than %d edges in all" % MAX_EDGES
	for key in ["inputs", "outputs"]:
		if (d.get(key, []) as Array).size() > MAX_CHANNELS: return "more than %d %s" % [MAX_CHANNELS, key]
		for ch in d.get(key, []):
			if not ch is Dictionary: return "an entry of \"%s\" is not an object" % key
			if not (ch as Dictionary).get("name", null) is String: return "every entry of \"%s\" needs a text name" % key
			var lk := "targets" if key == "inputs" else "neurons"
			if (ch as Dictionary).has(lk) and not (ch as Dictionary)[lk] is Array: return "\"%s\" of a channel must be a list" % lk
			for f in ["weight", "value", "gain", "tau_ms"]:
				if (ch as Dictionary).has(f) and not _is_num((ch as Dictionary)[f]): return "channel %s: \"%s\" must be a finite number" % [str((ch as Dictionary)["name"]), f]
	return ""

static func _depth_error(v: Variant, depth: int) -> String:
	if depth > MAX_DEPTH: return "nested too deeply"
	if v is Dictionary:
		for k in v:
			var e := _depth_error(v[k], depth + 1)
			if e != "": return e
	elif v is Array:
		for x in v:
			if x is Dictionary or x is Array:
				var e := _depth_error(x, depth + 1)
				if e != "": return e
	return ""

## Editor positions that are missing, not numbers or not finite (or absurdly far out) are replaced by the automatic layout. Returns how many were replaced.
static func repair_positions(c: Dictionary) -> int:
	var fixed := 0; var pops: Variant = c.get("populations", [])
	if not pops is Array: return 0
	for pi in (pops as Array).size():
		var p: Variant = pops[pi]
		if not p is Dictionary or not (p as Dictionary).has("pos"): continue
		var pos: Variant = p["pos"]
		if not pos is Array: p.erase("pos"); fixed += 1; continue
		var n := pop_n(p)
		for k in (pos as Array).size():
			var q: Variant = pos[k]
			var ok: bool = q is Array and (q as Array).size() >= 2 and _is_num(q[0]) and _is_num(q[1]) and absf(float(q[0])) < 1.0e6 and absf(float(q[1])) < 1.0e6
			if not ok:
				var a := auto_pos(pi, k, maxi(n, k + 1)); pos[k] = [a.x, a.y]; fixed += 1
	return fixed

## Parse and check circuit text. {circuit, error, repaired}: `error` is "" when the circuit is fit to use (circuit is then the Dictionary), else why not (circuit is {}).
static func parse_checked(json: String) -> Dictionary:
	if json.strip_edges() == "": return {"circuit": {}, "error": "the file is empty", "repaired": 0}
	var v: Variant = JSON.parse_string(json)
	if not v is Dictionary: return {"circuit": {}, "error": "not a circuit (the file is not JSON, or is cut short)", "repaired": 0}
	var bad := validate(v)
	if bad != "": return {"circuit": {}, "error": bad, "repaired": 0}
	var rep := repair_positions(v)
	return {"circuit": v, "error": "", "repaired": rep}

## Godot's JSON reads every number as a float and would write 1 as 1.0, which the engine rejects for integer fields (schema_version, n, seed ...): restore them.
const INT_KEYS := ["schema_version", "n", "seed", "indegree", "outdegree", "idx", "pos", "projection"]
static func to_json(c: Dictionary) -> String: return JSON.stringify(_norm(c, ""), "  ")

static func _norm(v: Variant, key: String) -> Variant:
	if v is Dictionary:
		var out := {}
		for k in v: out[k] = _norm(v[k], str(k))
		return out
	if v is Array:
		var out := []
		if key == "edges":
			for e in v:
				var ee := []
				for i in (e as Array).size(): ee.append(int(e[i]) if i < 2 else e[i])
				out.append(ee)
			return out
		for x in v: out.append(_norm(x, key))
		return out
	if v is float and key in INT_KEYS and is_equal_approx(v, roundf(v)): return int(v)
	return v

## Read a reference circuit shipped inside the project (res://circuits, a path relative to it; the pck carries it in exports).
## The folder next to the executable is a fallback for a loose copy.
static func read_reference(rel: String) -> String:
	for base in ["res://circuits", OS.get_executable_path().get_base_dir().path_join("circuits")]:
		var p := String(base).path_join(rel)
		if FileAccess.file_exists(p): return FileAccess.get_file_as_string(p)
	return ""

static func user_dir() -> String:
	var d := "user://circuits"; DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(d)); return d

# ---------- bench history: a replaced bench is never lost ----------
const HISTORY_KEEP := 10
static func history_dir(task: String) -> String: return user_dir().path_join(task if task != "" else "free").path_join("history")

## Files of the history, newest first.
static func history_list(task: String) -> Array:
	var d := DirAccess.open(history_dir(task)); var out := []
	if d == null: return out
	for f in d.get_files():
		if f.begins_with("bench-") and f.ends_with(".json"): out.append(f)
	out.sort(); out.reverse(); return out

## Keep `c` (a bench about to be replaced by Reference, Empty bench or Load) in circuits/<task>/history, newest HISTORY_KEEP files. A bench with no neurons, or
## the same as the newest entry, is not kept. Returns true when a file was written.
static func history_push(task: String, c: Dictionary) -> bool:
	if not SaveIO.enabled() or neuron_count(c) == 0: return false
	var text := to_json(c); var list := history_list(task)
	if not list.is_empty() and FileAccess.get_file_as_string(history_dir(task).path_join(str(list[0]))) == text: return false
	# order by a sequence number, not by the clock alone: max(unix ms, newest + 1) so two pushes in one millisecond (or a clock step back) still sort in push order.
	# "bench-s<13 digits>.json" sorts after the older "bench-<date>T<time>-<ms>.json" names ('s' > digits), so existing histories keep their order.
	var seq := int(Time.get_unix_time_from_system() * 1000.0)
	if not list.is_empty() and str(list[0]).begins_with("bench-s"): seq = maxi(seq, int(str(list[0]).substr(7, 13)) + 1)
	var name := "bench-s%013d.json" % seq
	if not SaveIO.write_text(history_dir(task).path_join(name), text, -1.0): return false
	list = history_list(task)
	for i in range(HISTORY_KEEP, list.size()): DirAccess.remove_absolute(SaveIO.g(history_dir(task).path_join(str(list[i]))))
	return true

## Same, from a file on disk (the saved autosave about to be shadowed by a Reference bench).
static func history_push_file(task: String, path: String) -> bool:
	if not FileAccess.file_exists(path): return false
	var r := parse_checked(FileAccess.get_file_as_string(path))
	return history_push(task, r["circuit"]) if str(r["error"]) == "" else false

## The newest kept bench as a checked circuit ({} when there is none).
static func history_latest(task: String) -> Dictionary:
	var list := history_list(task)
	if list.is_empty(): return {}
	var r := parse_checked(FileAccess.get_file_as_string(history_dir(task).path_join(str(list[0]))))
	return r["circuit"] if str(r["error"]) == "" else {}

## Remove the newest history entry (it has been restored).
static func history_pop(task: String) -> void:
	var list := history_list(task)
	if not list.is_empty(): DirAccess.remove_absolute(SaveIO.g(history_dir(task).path_join(str(list[0]))))

# ---------- neurons ----------
static func pop_n(p: Dictionary) -> int: return maxi(1, int(p.get("n", 1)))

## The sizes the "neurons in group" stepper offers. Every group size a reference circuit uses is in it (a ring of sixteen, the 200 Kenyon cells), so a player can build
## what the level asks for; godot/tests/test_editor.gd checks that against godot/circuits. A group whose size is not in the list (loaded from a file, pasted) keeps its size
## and the stepper shows it between its neighbours (see group_size_steps).
const GROUP_SIZES := [1, 2, 3, 4, 5, 6, 8, 10, 12, 14, 16, 20, 24, 30, 40, 50, 60, 80, 100, 150, 200, 300, 400]
## [[value, label], ...] for Kit.stepper: GROUP_SIZES (up to the engine's neuron limit) plus the group's own size when it is not one of them.
static func group_size_steps(current: int) -> Array:
	var sizes := []
	for s in GROUP_SIZES:
		if int(s) <= max_neurons(): sizes.append(int(s))
	if current >= 1 and current <= max_neurons() and not sizes.has(current): sizes.append(current); sizes.sort()
	return sizes.map(func(s): return [s, str(s)])

## Flat list of circuit neurons in file order: {pop, k (index in pop), pi (population index), pos (editor coordinates)}.
static func neurons(c: Dictionary) -> Array:
	var out := []; var pops: Array = c.get("populations", [])
	for pi in pops.size():
		var p: Dictionary = pops[pi]; var n := pop_n(p); var pos: Array = p.get("pos", [])
		for k in n:
			var xy := Vector2.ZERO
			if k < pos.size() and pos[k] is Array and pos[k].size() >= 2: xy = Vector2(float(pos[k][0]), float(pos[k][1]))
			else: xy = auto_pos(pi, k, n)
			out.append({"pop": str(p.get("id", "")), "k": k, "pi": pi, "pos": xy})
	return out

static func auto_pos(pi: int, k: int, n: int) -> Vector2:
	return Vector2(160.0 + 190.0 * pi + (k % 5) * 34.0, 220.0 + (k / 5) * 34.0)

static func pop_index(c: Dictionary, id: String) -> int:
	var pops: Array = c.get("populations", [])
	for i in pops.size():
		if str(pops[i].get("id", "")) == id: return i
	return -1

## First global neuron index of a population.
static func pop_first(c: Dictionary, pi: int) -> int:
	var s := 0; var pops: Array = c.get("populations", [])
	for i in mini(pi, pops.size()): s += pop_n(pops[i])
	return s

static func neuron_count(c: Dictionary) -> int:
	var s := 0
	for p in c.get("populations", []): s += pop_n(p)
	return s

static func ensure_pos(c: Dictionary) -> void:
	var pops: Array = c.get("populations", [])
	for pi in pops.size():
		var p: Dictionary = pops[pi]; var n := pop_n(p); var pos: Array = p.get("pos", [])
		if pos.size() >= n: continue
		for k in range(pos.size(), n):
			var a := auto_pos(pi, k, n); pos.append([a.x, a.y])
		p["pos"] = pos

# ---------- projections ----------
static func proj_pop(ref: Variant) -> String:
	if ref is Dictionary: return str(ref.get("pop", ""))
	return str(ref)

static func proj_weight(pr: Dictionary) -> float:
	var e: Array = pr.get("edges", [])
	if not e.is_empty() and e[0] is Array and e[0].size() >= 3: return float(e[0][2])
	return float(pr.get("synapse", {}).get("weight", 1.0))

## A diagnosis patient's circuit carries meta.dx_types: for every projection, "E" or "I", the type it has in the healthy circuit. The dead connection has weight 0, so its weight must never
## give it away (neither its sign, nor its thickness, nor a printed number): a patient's axon is drawn and described by that type only.
static func has_dx_types(c: Dictionary) -> bool: return (c.get("meta", {}) as Dictionary).has("dx_types")

## The weight to DRAW and DESCRIBE projection `i` with: its own weight, except in a patient's circuit, where it is a nominal weight of the healthy type (the post cell's usual weight, signed).
static func proj_weight_shown(c: Dictionary, i: int) -> float:
	var projs: Array = c.get("projections", [])
	if i < 0 or i >= projs.size(): return 0.0
	var pr: Dictionary = projs[i]
	var t: Array = (c.get("meta", {}) as Dictionary).get("dx_types", [])
	if t.is_empty() or i >= t.size(): return proj_weight(pr)
	var pi := pop_index(c, proj_pop(pr.get("post", ""))); var nominal := weight_of(str(c["populations"][pi].get("model", ""))) if pi >= 0 else 1.0
	return -nominal if str(t[i]) == "I" else nominal

static func proj_delay(pr: Dictionary) -> float:
	var e: Array = pr.get("edges", [])
	if not e.is_empty() and e[0] is Array and e[0].size() >= 4: return float(e[0][3])
	return float(pr.get("synapse", {}).get("delay", 1.0))

static func proj_model(pr: Dictionary) -> String: return str(pr.get("synapse", {}).get("synapse_model", "static_synapse"))

## Per-synapse weight spread of a projection (`weight_cv`, relative, seeded by the circuit seed): 0 removes the key.
static func set_proj_spread(pr: Dictionary, cv: float) -> void:
	if cv <= 0.0: pr.erase("weight_cv")
	else: pr["weight_cv"] = snappedf(cv, 0.01)

static func set_proj_weight(pr: Dictionary, w: float) -> void:
	var old := proj_weight(pr)
	if pr.has("edges"):
		for e in pr["edges"]:
			if e is Array and e.size() >= 3: e[2] = w if is_zero_approx(old) else float(e[2]) * w / old
	var s: Dictionary = pr.get("synapse", {}); s["weight"] = w; pr["synapse"] = s

static func set_proj_delay(pr: Dictionary, d: float) -> void:
	if pr.has("edges"):
		for e in pr["edges"]:
			if e is Array and e.size() >= 4: e[3] = d
	var s: Dictionary = pr.get("synapse", {}); s["delay"] = d; pr["synapse"] = s

## "E" or "I" for each population: the sign of its outgoing weights (majority), or the editor's own note for a population with none yet.
static func kinds(c: Dictionary) -> Dictionary:
	var sum := {}
	for pr in c.get("projections", []):
		var pre := proj_pop(pr.get("pre", ""))
		sum[pre] = float(sum.get(pre, 0.0)) + signf(proj_weight(pr))
	var note: Dictionary = c.get("meta", {}).get("editor", {}).get("ei", {})
	var out := {}
	for p in c.get("populations", []):
		var id := str(p.get("id", ""))
		if sum.has(id) and not is_zero_approx(sum[id]): out[id] = "I" if sum[id] < 0.0 else "E"
		else: out[id] = str(note.get(id, "E"))
	return out

static func kind_per_neuron(c: Dictionary) -> Array:
	var kd := kinds(c); var out := []
	for p in c.get("populations", []):
		for k in pop_n(p): out.append(kd[str(p.get("id", ""))])
	return out

# ---------- channels (inputs / outputs of a task) ----------
const MODULATORS := ["NE", "ACh", "DA"]
const MOD_GAIN := 0.05      # concentration units per spike (docs/CIRCUIT-FORMAT.md, "modulator" outputs)
const MOD_TAU := 1000.0     # ms

## The channel kinds a task expects, so an empty bench starts with the right devices: {"inputs": {name: kind}, "outputs": {name: {kind, modulator, gain, tau_ms}}}.
## From the task's own `channel_kinds()` when the extension has it (a Dictionary or JSON, {inputs, outputs} or flat name -> kind, or a list of {name, kind, ...}),
## else from the channels of the task's reference circuit (circuits/<ref>.json). Channels neither source names are absent: the caller falls back to rate / poisson.
static func task_channel_kinds(tid: String, task: Variant = null) -> Dictionary:
	var out := {"inputs": {}, "outputs": {}, "weights": {}}   # "weights": name -> the task's own default input weight (ChannelKind::weight), only where it declares one
	if tid == "": return out
	if task != null and task.has_method("channel_kinds"):
		var v: Variant = task.call("channel_kinds")
		if v is String: v = JSON.parse_string(v)
		_absorb_kinds(out, v, task)
	var ref := parse(read_reference(Campaign.ref_path(tid)))
	if not ref.is_empty():
		for ch in ref.get("inputs", []):
			if not (out["inputs"] as Dictionary).has(str(ch.get("name", ""))): out["inputs"][str(ch.get("name", ""))] = str(ch.get("kind", "poisson"))
		for ch in ref.get("outputs", []):
			if not (out["outputs"] as Dictionary).has(str(ch.get("name", ""))): out["outputs"][str(ch.get("name", ""))] = _out_kind(ch, false)
	return out

## Kind of one output channel. `keep_tuning` keeps the channel's own gain and tau (the task's say); a reference circuit's tuning is not copied: the empty bench
## starts at the documented defaults and the player tunes the release.
static func _out_kind(ch: Dictionary, keep_tuning := true) -> Dictionary:
	var k := str(ch.get("kind", "rate"))
	if k == "population_vector": return {"kind": "population_vector"}
	if k == "rate_value": return {"kind": "rate_value"}
	if k != "modulator": return {"kind": "rate"}
	return {"kind": "modulator", "modulator": str(ch.get("modulator", "NE")), "gain": float(ch.get("gain", MOD_GAIN)) if keep_tuning else MOD_GAIN, "tau_ms": float(ch.get("tau_ms", MOD_TAU)) if keep_tuning else MOD_TAU}

static func _absorb_kinds(out: Dictionary, v: Variant, task: Variant) -> void:
	var sensors := []
	if task != null: sensors = Array(task.sensor_channels())
	if v is Dictionary:
		if v.has("inputs") or v.has("outputs"):
			for side in ["inputs", "outputs"]: _absorb_side(out, side, v.get(side))
		else:
			for name in v:
				var side := "inputs" if sensors.has(str(name)) else "outputs"
				_absorb_one(out, side, str(name), v[name])
	elif v is Array:
		for e in v:
			if e is Dictionary:
				var name := str(e.get("name", "")); var side := "inputs" if (e.get("side", "") == "input" or sensors.has(name)) else "outputs"
				_absorb_one(out, side, name, e)

static func _absorb_side(out: Dictionary, side: String, v: Variant) -> void:
	if v is Dictionary:
		for name in v: _absorb_one(out, side, str(name), v[name])
	elif v is Array:
		for e in v:
			if e is Dictionary: _absorb_one(out, side, str(e.get("name", "")), e)

static func _absorb_one(out: Dictionary, side: String, name: String, v: Variant) -> void:
	if name == "": return
	if side == "inputs":
		out["inputs"][name] = str(v.get("kind", "poisson")) if v is Dictionary else str(v)
		if v is Dictionary and float(v.get("weight", 0.0)) > 0.0: out["weights"][name] = float(v["weight"])
	else: out["outputs"][name] = _out_kind(v if v is Dictionary else {"kind": str(v)})

## Make output channel `ch` a rate readout or a modulator release (docs/CIRCUIT-FORMAT.md "Output kind modulator"); the unused keys are removed.
static func set_output_kind(ch: Dictionary, kind: String, modulator := "NE", gain := MOD_GAIN, tau_ms := MOD_TAU) -> void:
	if kind == "modulator":
		ch["kind"] = "modulator"; ch["modulator"] = modulator; ch["gain"] = gain; ch["tau_ms"] = tau_ms
	elif kind == "population_vector":   # v0.5 ring readout: the bump angle over the cells in the order listed
		ch["kind"] = "population_vector"
		for k in ["modulator", "gain", "tau_ms"]: ch.erase(k)
	elif kind == "rate_value":   # v0.6: the mean rate of rate units (population rates, no spikes)
		ch["kind"] = "rate_value"
		for k in ["modulator", "gain", "tau_ms"]: ch.erase(k)
	else:
		ch["kind"] = "rate"
		for k in ["modulator", "gain", "tau_ms"]: ch.erase(k)

## The numbered family a channel belongs to: "ach3" -> {stem "ach", index 3, names [ach0 .. ach7] in index order}. Empty when it has no trailing number or
## fewer than 3 channels share its stem. Used by the segment wiring: a group of n neurons dropped on one member of a family of n connects neuron k to member k.
static func channel_family(c: Dictionary, key: String, name: String) -> Dictionary:
	var re := RegEx.new(); re.compile("^(.*?)(\\d+)$")
	var m := re.search(name)
	if m == null: return {}
	var stem := m.get_string(1); var names := {}
	for ch in c.get(key, []):
		var mm := re.search(str(ch.get("name", "")))
		if mm != null and mm.get_string(1) == stem: names[int(mm.get_string(2))] = str(ch.get("name", ""))
	if names.size() < 3: return {}
	var idx := names.keys(); idx.sort()
	return {"stem": stem, "index": int(m.get_string(2)), "names": idx.map(func(i): return names[i])}

## Wire population `pop` (n neurons) to every member of the channel family of `name`, neuron k to the k-th member (inputs: sensor k -> neuron k). Returns how many
## channels were connected (0 when the family size and n do not match).
static func wire_family(c: Dictionary, key: String, name: String, pop: String) -> int:
	var fam := channel_family(c, key, name)
	var pi := pop_index(c, pop)
	if fam.is_empty() or pi < 0: return 0
	var n := pop_n(c["populations"][pi]); var names: Array = fam["names"]
	if n != names.size(): return 0
	var model := str(c["populations"][pi].get("model", "iaf_psc_alpha")); var done := 0
	for k in n:
		for ch in c.get(key, []):
			if str(ch.get("name", "")) != str(names[k]): continue
			var field := "neurons" if key == "outputs" else "targets"
			var tg: Array = ch.get(field, [])
			if key == "inputs" and tg.is_empty(): ch["weight"] = first_input_weight(c, ch, model)
			if not tg.any(func(x): return proj_pop(x) == pop and x is Dictionary and (x.get("idx", []) as Array).has(k)): tg.append({"pop": pop, "idx": [k]})
			ch[field] = tg; done += 1
	return done

# ---------- segment wiring and modulation rows ----------
## "all" (every pre neuron to every post neuron) or "segment" (neuron k to neuron k + offset: one_to_one or explicit edges).
static func proj_wiring(pr: Dictionary) -> String:
	if str(pr.get("rule", "")) == "fixed_indegree": return "random"
	return "segment" if (str(pr.get("rule", "")) == "one_to_one" or pr.has("edges")) else "all"

static func segment_offset(pr: Dictionary) -> int:
	var e: Array = pr.get("edges", [])
	if not e.is_empty() and e[0] is Array and e[0].size() >= 2: return int(e[0][1]) - int(e[0][0])
	return 0

## Rewire projection `pr` of circuit `c` index-wise with an offset (post = pre + offset), or back to all-to-all. Weight and delay are kept.
static func set_wiring(c: Dictionary, pr: Dictionary, wiring: String, offset := 0, wrap := false) -> void:
	var w := proj_weight(pr); var d := proj_delay(pr)
	var syn: Dictionary = (pr.get("synapse", {}) as Dictionary).duplicate(); syn["weight"] = w; syn["delay"] = d
	var keep_k := int(pr.get("indegree", 4))
	pr.erase("edges"); pr.erase("rule"); pr.erase("indegree")
	if wiring == "random": pr["rule"] = "fixed_indegree"; pr["indegree"] = keep_k; pr["synapse"] = syn; return   # v0.5 mushroom body: each cell of the target listens to k random cells of the source
	if wiring != "segment": pr["rule"] = "all_to_all"; pr["synapse"] = syn; return
	var pa := pop_index(c, proj_pop(pr.get("pre", ""))); var pb := pop_index(c, proj_pop(pr.get("post", "")))
	var na := pop_n(c["populations"][pa]) if pa >= 0 else 1; var nb := pop_n(c["populations"][pb]) if pb >= 0 else 1
	if offset == 0 and na == nb: pr["rule"] = "one_to_one"; pr["synapse"] = syn; return
	var edges := []
	for k in na:
		if wrap and nb > 0: edges.append([k, posmod(k + offset, nb), w, d])   # a ring (fly compass): the last cell is next to the first
		elif k + offset >= 0 and k + offset < nb: edges.append([k, k + offset, w, d])
	pr["edges"] = edges; pr["synapse"] = {"synapse_model": str(syn.get("synapse_model", "static_synapse"))}

## Indices into c.modulation of the rows that act on population `pop`.
static func mod_rows(c: Dictionary, pop: String) -> Array:
	var out := []; var rows: Array = c.get("modulation", [])
	for i in rows.size():
		if str(rows[i].get("pop", "")) == pop: out.append(i)
	return out

static func add_mod_row(c: Dictionary, pop: String, modulator: String, param: String, gain := 1.0) -> void:
	var rows: Array = c.get("modulation", []); rows.append({"modulator": modulator, "pop": pop, "param": param, "gain": gain}); c["modulation"] = rows

static func remove_mod_row(c: Dictionary, i: int) -> void:
	var rows: Array = c.get("modulation", [])
	if i >= 0 and i < rows.size(): rows.remove_at(i)
	if rows.is_empty(): c.erase("modulation")
	else: c["modulation"] = rows

## Numeric parameters a modulation row can target on `model`: the NESTML cells' ach / ne inputs first (docs/CIRCUIT-FORMAT.md), then the editor's advanced parameters.
static func mod_params(model: String) -> Array:
	var out := []
	for a in advanced_of(model):
		if str(a[0]) != "V_m": out.append(str(a[0]))
	for k in ["ach", "ne"]:
		if Catalog.cards_of(model).any(func(cd): return (cd["params"] as Array).any(func(q): return str(q["key"]) == k)) and not out.has(k): out.insert(0, k)
	return out

# ---------- editing ----------
static func model_options(current: String) -> Array:
	var out := []
	for m in Catalog.usable_models(sandbox): out.append([m, m])
	if current != "" and out.filter(func(o): return o[0] == current).is_empty(): out.append([current, current])
	return out

static func unique_id(c: Dictionary, base: String) -> String:
	var n := 1
	while true:
		var id := "%s%d" % [base, n]
		if pop_index(c, id) < 0 and not _channel_exists(c, id): return id
		n += 1
	return base

static func _channel_exists(c: Dictionary, id: String) -> bool:
	for key in ["inputs", "outputs"]:
		for ch in c.get(key, []):
			if str(ch.get("name", "")) == id: return true
	return false

static func add_neuron(c: Dictionary, model: String, preset: int, at: Vector2, kind := "E") -> String:
	var id := unique_id(c, "N")
	var presets: Array = presets_of(model)
	var pr: Array = presets[clampi(preset, 0, presets.size() - 1)]
	var pop := {"id": id, "model": model, "n": 1, "params": (pr[1] as Dictionary).duplicate(), "pos": [[at.x, at.y]], "role": "neuron"}
	c["populations"].append(pop)
	var meta: Dictionary = c.get("meta", {}); var ed: Dictionary = meta.get("editor", {}); var ei: Dictionary = ed.get("ei", {})
	ei[id] = kind; ed["ei"] = ei; meta["editor"] = ed; c["meta"] = meta
	return id

## A neuron from a parts-bin card: its model and preset parameters.
static func add_card(c: Dictionary, card: Dictionary, at: Vector2, kind := "E") -> String:
	var id := unique_id(c, "N")
	var pop := {"id": id, "model": str(card["model"]), "n": 1, "params": (card.get("preset", {}) as Dictionary).duplicate(), "pos": [[at.x, at.y]], "role": "neuron"}
	c["populations"].append(pop)
	var meta: Dictionary = c.get("meta", {}); var ed: Dictionary = meta.get("editor", {}); var ei: Dictionary = ed.get("ei", {})
	ei[id] = kind; ed["ei"] = ei; meta["editor"] = ed; c["meta"] = meta
	return id

## A free background input: a Poisson source (rate in `value`) that the player wires to any neurons. Returns its name.
static func add_noise(c: Dictionary) -> String:
	var name := unique_id(c, "noise")
	c["inputs"].append({"name": name, "kind": "poisson", "targets": [], "weight": input_weight("iaf_psc_alpha", "poisson") * 0.5, "value": 30.0})
	return name

## v0.6: NEST rate models and the siegert mean-field unit (engine/src/circuit.cpp is_rate_model): population rates, no spikes.
static func is_rate_model(model: String) -> bool: return model == "siegert_neuron" or model.contains("rate")

static func default_weight(c: Dictionary, pre: String, post: String) -> float:
	var pi := pop_index(c, post); var model := "iaf_psc_alpha"
	if pi >= 0: model = str(c["populations"][pi].get("model", model))
	if model == "siegert_neuron": return -0.1 if str(kinds(c).get(pre, "E")) == "I" else 0.1   # J in mV
	if is_rate_model(model): return -1.0 if str(kinds(c).get(pre, "E")) == "I" else 1.0         # gain of one unit onto another
	var w: float = weight_of(model)
	return -w if str(kinds(c).get(pre, "E")) == "I" else w

static func add_projection(c: Dictionary, pre: String, post: String) -> int:
	for i in c["projections"].size():   # one projection per ordered pair in the editor
		var p: Dictionary = c["projections"][i]
		if proj_pop(p.get("pre", "")) == pre and proj_pop(p.get("post", "")) == post: return i
	var a := pop_index(c, pre); var b := pop_index(c, post)
	var ma := str(c["populations"][a].get("model", "")) if a >= 0 else ""; var mb := str(c["populations"][b].get("model", "")) if b >= 0 else ""
	if is_rate_model(ma) and is_rate_model(mb):   # rate units are joined by rate connections (siegert units by a mean-field projection of weight J and K inputs per cell), which have no delay
		var syn := {"synapse_model": "diffusion_connection", "weight": default_weight(c, pre, post), "K": 1.0} if ma == "siegert_neuron" and mb == "siegert_neuron" else {"synapse_model": "rate_connection_instantaneous", "weight": default_weight(c, pre, post)}
		var pr := {"pre": pre, "post": post, "rule": "all_to_all", "synapse": syn}
		if pre == post: pr["allow_autapses"] = true
		c["projections"].append(pr)
		return c["projections"].size() - 1
	c["projections"].append({"pre": pre, "post": post, "rule": "all_to_all", "synapse": {"synapse_model": "static_synapse", "weight": default_weight(c, pre, post), "delay": 2.0}})
	return c["projections"].size() - 1

static func remove_population(c: Dictionary, id: String) -> void:
	var pi := pop_index(c, id)
	if pi >= 0: c["populations"].remove_at(pi)
	var keep := []
	for p in c["projections"]:
		if proj_pop(p.get("pre", "")) != id and proj_pop(p.get("post", "")) != id: keep.append(p)
	c["projections"] = keep
	for ch in c.get("inputs", []):
		var t := []
		for x in ch.get("targets", []):
			if proj_pop(x) != id: t.append(x)
		ch["targets"] = t
	for ch in c.get("outputs", []):
		var t := []
		for x in ch.get("neurons", []):
			if proj_pop(x) != id: t.append(x)
		ch["neurons"] = t
	var rec: Dictionary = c.get("record", {})
	if rec.get("voltage") is Array:
		var vv := []
		for x in rec["voltage"]:
			if proj_pop(x) != id: vv.append(x)
		rec["voltage"] = vv

static func set_model(c: Dictionary, id: String, model: String, preset: int) -> void:
	var pi := pop_index(c, id); if pi < 0: return
	var p: Dictionary = c["populations"][pi]; var presets: Array = presets_of(model)
	p["model"] = model; p["params"] = (presets[clampi(preset, 0, presets.size() - 1)][1] as Dictionary).duplicate()

static func preset_of(p: Dictionary) -> int:
	var presets: Array = presets_of(str(p.get("model", "")))
	for i in presets.size():
		if (presets[i][1] as Dictionary) == p.get("params", {}): return i
	return -1

static func set_n(c: Dictionary, id: String, n: int) -> void:
	var pi := pop_index(c, id); if pi < 0: return
	var p: Dictionary = c["populations"][pi]; p["n"] = n
	var pos: Array = p.get("pos", [])
	while pos.size() > n: pos.pop_back()
	p["pos"] = pos; ensure_pos(c)

## Every neuron the circuit's voltage list names (global indices), else the first neuron of each population (at most 4).
static func default_watch(c: Dictionary) -> Array:
	var out := []
	var rate_pop := func(pi: int) -> bool: return is_rate_model(str(c["populations"][pi].get("model", "")))   # rate units have no membrane voltage to trace
	for v in c.get("record", {}).get("voltage", []):
		var pi := pop_index(c, proj_pop(v)); if pi < 0: continue
		var idx: Array = v.get("idx", [0]) if v is Dictionary else [0]
		for k in idx: out.append(pop_first(c, pi) + int(k))
	if out.is_empty():
		for pi in c.get("populations", []).size():
			if not rate_pop.call(pi): out.append(pop_first(c, pi))
	return out.slice(0, 4)

# ---------- v0.3: the critical path, candidate lesions, the guilty element ----------
## Whole-run spikes of each population: {pop id: count} from a Telemetry's per-neuron counts.
static func pop_activity(c: Dictionary, count: PackedInt32Array) -> Dictionary:
	var out := {}; var pops: Array = c.get("populations", [])
	for pi in pops.size():
		var s := 0; var f := pop_first(c, pi)
		for k in pop_n(pops[pi]):
			if f + k < count.size(): s += count[f + k]
		out[str(pops[pi].get("id", ""))] = s
	return out

## The route a signal takes from a sensor to a motor channel, as the list ["in:<name>", <pop id>, ..., "out:<name>"]: among the simple paths over the circuit's
## projections the one whose populations all fired and whose total activity is highest (shorter wins a tie); with no activity at all, the shortest path.
## [] when no sensor reaches a motor. `sensors` / `motors` limit the channels (the task's own; empty = every channel).
static func critical_path(c: Dictionary, activity: Dictionary, sensors: Array = [], motors: Array = []) -> Array:
	var adj := {}
	for pr in c.get("projections", []):
		var a := proj_pop(pr.get("pre", "")); var b := proj_pop(pr.get("post", ""))
		if a == "" or b == "" or pop_index(c, a) < 0 or pop_index(c, b) < 0: continue
		if not adj.has(a): adj[a] = []
		if not (adj[a] as Array).has(b): (adj[a] as Array).append(b)
	var sinks := {}   # pop -> out channel
	for ch in c.get("outputs", []):
		var nm := str(ch.get("name", ""))
		if not motors.is_empty() and not motors.has(nm): continue
		for x in ch.get("neurons", []): sinks[proj_pop(x)] = nm if not sinks.has(proj_pop(x)) else sinks[proj_pop(x)]
	var best := []; var best_score := -1.0e9
	for ch in c.get("inputs", []):
		var nm := str(ch.get("name", ""))
		if not sensors.is_empty() and not sensors.has(nm): continue
		for tg in ch.get("targets", []):
			var start := proj_pop(tg)
			var stack := [[start]]
			while not stack.is_empty():
				var path: Array = stack.pop_back()
				var last: String = path[path.size() - 1]
				if sinks.has(last):
					var sc := 0.0; var all_on := true
					for p in path:
						var act := float(activity.get(p, 0))
						if act <= 0.0: all_on = false
						sc += log(1.0 + act)
					sc = sc * (1.0 if all_on else 0.1) - 0.4 * path.size() + (50.0 if all_on else 0.0)
					if sc > best_score: best_score = sc; best = ["in:" + nm] + path + ["out:" + str(sinks[last])]
				if path.size() >= 8: continue
				for nx in adj.get(last, []):
					if not path.has(nx): stack.append(path + [nx])
	return best

## Index of the first projection from population `pre` to `post`, -1 when there is none.
static func proj_between(c: Dictionary, pre: String, post: String) -> int:
	var projs: Array = c.get("projections", [])
	for i in projs.size():
		if proj_pop(projs[i].get("pre", "")) == pre and proj_pop(projs[i].get("post", "")) == post: return i
	return -1

## The population a diagnosis candidate is about, for the editor to outline: the target of the first connection of "pre>post[+pre>post...]" (the wire is missing
## from the patient's circuit, so its target is where the lost drive shows), or the population of "pop:ID".
static func candidate_focus(cand: String) -> String:
	if cand.begins_with("pop:"): return cand.substr(4)
	var first := cand.split("+")[0].split(">")
	return first[1].strip_edges() if first.size() == 2 else ""

## A candidate in the player's words: "the pathway E_L to I_R" / "the cells of E_L"; compound candidates ("A>B+C>D") join their parts with "and".
static func candidate_text(cand: String) -> String:
	if "+" in cand:
		var parts := []
		for c in cand.split("+"):
			parts.append(candidate_text(c).trim_prefix("the pathway "))
		return "the pathways " + " and ".join(parts)
	if cand.begins_with("pop:"): return "the cells of %s" % cand.substr(4)
	if cand.begins_with("in:"): return "the sensory input %s" % cand.substr(3)
	var p := cand.split(">")
	if p.size() == 2: return "the pathway %s to %s" % [p[0].strip_edges(), p[1].strip_edges()]
	return cand

# ---------- structural fingerprint (the reference lock) ----------
## A fingerprint of what a circuit IS, independent of its seed and of the ids it gives its groups: every population as model, size, parameters and editor
## positions, every projection as (pre population, post population, sign and weight, delay, synapse, rule), every channel with what it is wired to, the
## modulation rows, and the file's own author and notes (the reference files carry "spike" as author; a bench the player builds does not).
## The rule is deliberately narrow, so that it is honest: a circuit is "reference-derived" only when it equals the reference EXACTLY (same populations,
## models, sizes, projections, positions and provenance, and every number within a relative 1e-6: seven significant figures), or when the REFERENCE
## button filled the bench this session (see LabScreen._is_reference). A round number a player types (450 pA) on a circuit they built never matches,
## because their circuit is not the reference file. Nothing about the lock is stored in the circuit file.
static func fingerprint(c: Dictionary) -> String:
	var sig := {}
	for p in c.get("populations", []):
		sig[str(p.get("id", ""))] = "%s*%d{%s}@%s" % [str(p.get("model", "")), pop_n(p), _dict_sig(p.get("params", {})), _pos_sig(p.get("pos", []))]
	var parts := PackedStringArray()
	var meta: Dictionary = c.get("meta", {})
	parts.append("A %s|%s" % [str(meta.get("author", "")), str(meta.get("notes", ""))])
	for id in sig: parts.append("P " + str(sig[id]))
	for pr in c.get("projections", []):
		var a := str(sig.get(proj_pop(pr.get("pre", "")), "?")); var b := str(sig.get(proj_pop(pr.get("post", "")), "?"))
		parts.append("J %s>%s w%s d%s cv%s %s %s %s" % [a, b, _sig3(proj_weight(pr)), _sig3(proj_delay(pr)), _sig3(float(pr.get("weight_cv", 0.0))), proj_model(pr), proj_wiring(pr), str(segment_offset(pr))])
	for key in ["inputs", "outputs"]:
		for ch in c.get(key, []):
			var t := PackedStringArray()
			for x in ch.get("targets" if key == "inputs" else "neurons", []):
				var ps := str(sig.get(proj_pop(x), "?"))
				if x is Dictionary and (x as Dictionary).has("idx"): ps += "#" + str(x["idx"])
				t.append(ps)
			t.sort()
			if key == "inputs": parts.append("I %s %s w%s [%s]" % [str(ch.get("name", "")), str(ch.get("kind", "")), _sig3(float(ch.get("weight", 0.0))), ",".join(t)])
			else: parts.append("O %s %s %s g%s t%s [%s]" % [str(ch.get("name", "")), str(ch.get("kind", "rate")), str(ch.get("modulator", "")), _sig3(float(ch.get("gain", 0.0))), _sig3(float(ch.get("tau_ms", 0.0))), ",".join(t)])
	for row in c.get("modulation", []):
		if row is Dictionary: parts.append("M %s %s %s %s" % [str(sig.get(proj_pop(row.get("pop", "")), "?")), str(row.get("modulator", "")), str(row.get("param", "")), _dict_sig(row)])
	parts.sort()
	return "\n".join(parts).md5_text()

## Seven significant figures (relative 1e-6 and a little better), as text (0 stays "0").
static func _sig3(x: float) -> String:
	if is_zero_approx(x): return "0"
	var e := floori(log(absf(x)) / log(10.0)) - 6
	return str(snappedf(x, pow(10.0, e)))

static func _pos_sig(pos: Variant) -> String:
	if not pos is Array: return ""
	var out := PackedStringArray()
	for q in pos:
		if q is Array and q.size() >= 2: out.append("%d,%d" % [roundi(float(q[0])), roundi(float(q[1]))])
	return ";".join(out)

static func _dict_sig(d: Variant) -> String:
	if not d is Dictionary: return ""
	var keys := (d as Dictionary).keys(); keys.sort()
	var out := PackedStringArray()
	for k in keys:
		var v: Variant = d[k]
		out.append("%s=%s" % [str(k), _sig3(float(v)) if (v is float or v is int) else str(v)])
	return ",".join(out)

## True when `c` is the reference circuit of task `tid` (an exact copy of it; see fingerprint).
static func is_reference_of(c: Dictionary, tid: String) -> bool:
	if tid == "" or neuron_count(c) == 0: return false
	var ref := reference_print(tid)
	return ref != "" and fingerprint(c) == ref

static var _ref_prints := {}
## The fingerprint of task `tid`'s reference circuit ("" when it is not installed), cached.
static func reference_print(tid: String) -> String:
	if _ref_prints.has(tid): return _ref_prints[tid]
	var rc := parse(read_reference(Campaign.ref_path(tid)))
	var fp := fingerprint(rc) if not rc.is_empty() else ""
	_ref_prints[tid] = fp; return fp
