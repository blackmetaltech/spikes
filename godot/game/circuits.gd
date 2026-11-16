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
	"aeif_cond_alpha": [["NEST default", {}, "a 4 nS, b 80.5 pA: adapting."], ["Tonic", {"a": 2.0, "b": 0.0, "tau_w": 30.0, "V_reset": -58.0}, "Regular firing, no adaptation (Naud 2008)."], ["Adapting", {"a": 0.0, "b": 60.0, "tau_w": 100.0, "V_reset": -58.0}, "Spike-triggered adaptation slows the firing."], ["Bursting", {"a": -0.5, "b": 7.0, "tau_w": 5.0, "V_reset": -46.0}, "Fires in bursts (Naud 2008)."]],
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

## Advanced parameter rows of `model`: the editor's table, else derived from the catalogue card's parameters and ranges.
static func advanced_of(model: String) -> Array:
	return ADVANCED[model] if ADVANCED.has(model) else Catalog.advanced_of(model)

## "Advanced" neuron parameters per model, in NEST's names and units: [key, label, min, max, step, NEST default, unit]. `V_m` is the initial membrane
## potential: different starting values are what lets two identical cells break symmetry (a half-centre oscillator needs that).
const ADVANCED := {
	"iaf_psc_alpha": [["V_m", "start V", -85.0, -50.0, 0.5, -70.0, "mV"], ["tau_m", "tau m", 2.0, 60.0, 0.5, 10.0, "ms"], ["V_th", "threshold", -60.0, -40.0, 0.5, -55.0, "mV"], ["I_e", "bias", -400.0, 800.0, 5.0, 0.0, "pA"], ["tau_syn_ex", "tau syn E", 0.2, 20.0, 0.1, 2.0, "ms"], ["tau_syn_in", "tau syn I", 0.2, 20.0, 0.1, 2.0, "ms"]],
	"aeif_cond_alpha": [["V_m", "start V", -85.0, -45.0, 0.5, -70.6, "mV"], ["a", "a", -2.0, 10.0, 0.1, 4.0, "nS"], ["b", "b", 0.0, 200.0, 1.0, 80.5, "pA"], ["tau_w", "tau w", 5.0, 400.0, 1.0, 144.0, "ms"], ["I_e", "bias", -400.0, 1000.0, 5.0, 0.0, "pA"], ["tau_syn_ex", "tau syn E", 0.2, 20.0, 0.1, 0.2, "ms"], ["tau_syn_in", "tau syn I", 0.2, 20.0, 0.1, 2.0, "ms"]],
	"izhikevich": [["V_m", "start V", -85.0, -45.0, 0.5, -65.0, "mV"], ["a", "a", 0.01, 0.15, 0.005, 0.02, ""], ["b", "b", 0.1, 0.3, 0.005, 0.2, ""], ["c", "c", -70.0, -45.0, 0.5, -65.0, "mV"], ["d", "d", 0.5, 10.0, 0.5, 8.0, ""], ["I_e", "bias", -10.0, 30.0, 0.5, 0.0, "pA"]],
	"hh_psc_alpha": [["V_m", "start V", -85.0, -50.0, 0.5, -65.0, "mV"], ["I_e", "bias", -100.0, 600.0, 5.0, 0.0, "pA"], ["tau_syn_ex", "tau syn E", 0.2, 20.0, 0.1, 2.0, "ms"], ["tau_syn_in", "tau syn I", 0.2, 20.0, 0.1, 2.0, "ms"]]}

## Default weight of a sensor / noise input onto a neuron of `model`: several internal synapses (weight_of), so a lone neuron wired straight from a
## 400 Hz sensor fires at once instead of sitting silent (as the old 1.2 x 250 pA did for iaf). Dc inputs keep their own scale (pA of drive).
static func input_weight(model: String, kind: String) -> float:
	if kind == "dc": return 300.0
	return snappedf(weight_of(model) * 3.5, 0.5)

static func empty(name := "New circuit") -> Dictionary:
	return {"schema_version": 1, "name": name, "resolution_ms": 0.1, "seed": 1, "populations": [], "projections": [], "inputs": [], "outputs": [], "record": {"spikes": "all"}, "meta": {"author": "", "notes": ""}}

static func parse(json: String) -> Dictionary:
	if json.strip_edges() == "": return {}
	var v: Variant = JSON.parse_string(json)
	return v if v is Dictionary else {}

## Godot's JSON reads every number as a float and would write 1 as 1.0, which the engine rejects for integer fields (schema_version, n, seed ...): restore them.
const INT_KEYS := ["schema_version", "n", "seed", "indegree", "outdegree", "idx", "pos"]
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

## Read a reference circuit shipped in the repo's circuits/ dir (a path relative to it). Looks next to the project (dev, ../circuits) and in res://circuits (exports).
static func read_reference(rel: String) -> String:
	for base in [ProjectSettings.globalize_path("res://").path_join("../circuits"), ProjectSettings.globalize_path("res://circuits"), OS.get_executable_path().get_base_dir().path_join("circuits")]:
		var p := String(base).path_join(rel)
		if FileAccess.file_exists(p): return FileAccess.get_file_as_string(p)
	return ""

static func user_dir() -> String:
	var d := "user://circuits"; DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(d)); return d

# ---------- neurons ----------
static func pop_n(p: Dictionary) -> int: return maxi(1, int(p.get("n", 1)))

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

static func proj_delay(pr: Dictionary) -> float:
	var e: Array = pr.get("edges", [])
	if not e.is_empty() and e[0] is Array and e[0].size() >= 4: return float(e[0][3])
	return float(pr.get("synapse", {}).get("delay", 1.0))

static func proj_model(pr: Dictionary) -> String: return str(pr.get("synapse", {}).get("synapse_model", "static_synapse"))

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
	var out := {"inputs": {}, "outputs": {}}
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
	if side == "inputs": out["inputs"][name] = str(v.get("kind", "poisson")) if v is Dictionary else str(v)
	else: out["outputs"][name] = _out_kind(v if v is Dictionary else {"kind": str(v)})

## Make output channel `ch` a rate readout or a modulator release (docs/CIRCUIT-FORMAT.md "Output kind modulator"); the unused keys are removed.
static func set_output_kind(ch: Dictionary, kind: String, modulator := "NE", gain := MOD_GAIN, tau_ms := MOD_TAU) -> void:
	if kind == "modulator":
		ch["kind"] = "modulator"; ch["modulator"] = modulator; ch["gain"] = gain; ch["tau_ms"] = tau_ms
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
			if key == "inputs" and tg.is_empty(): ch["weight"] = input_weight(model, str(ch.get("kind", "poisson")))
			if not tg.any(func(x): return proj_pop(x) == pop and x is Dictionary and (x.get("idx", []) as Array).has(k)): tg.append({"pop": pop, "idx": [k]})
			ch[field] = tg; done += 1
	return done

# ---------- segment wiring and modulation rows ----------
## "all" (every pre neuron to every post neuron) or "segment" (neuron k to neuron k + offset: one_to_one or explicit edges).
static func proj_wiring(pr: Dictionary) -> String:
	return "segment" if (str(pr.get("rule", "")) == "one_to_one" or pr.has("edges")) else "all"

static func segment_offset(pr: Dictionary) -> int:
	var e: Array = pr.get("edges", [])
	if not e.is_empty() and e[0] is Array and e[0].size() >= 2: return int(e[0][1]) - int(e[0][0])
	return 0

## Rewire projection `pr` of circuit `c` index-wise with an offset (post = pre + offset), or back to all-to-all. Weight and delay are kept.
static func set_wiring(c: Dictionary, pr: Dictionary, wiring: String, offset := 0) -> void:
	var w := proj_weight(pr); var d := proj_delay(pr)
	var syn: Dictionary = (pr.get("synapse", {}) as Dictionary).duplicate(); syn["weight"] = w; syn["delay"] = d
	pr.erase("edges"); pr.erase("rule")
	if wiring != "segment": pr["rule"] = "all_to_all"; pr["synapse"] = syn; return
	var pa := pop_index(c, proj_pop(pr.get("pre", ""))); var pb := pop_index(c, proj_pop(pr.get("post", "")))
	var na := pop_n(c["populations"][pa]) if pa >= 0 else 1; var nb := pop_n(c["populations"][pb]) if pb >= 0 else 1
	if offset == 0 and na == nb: pr["rule"] = "one_to_one"; pr["synapse"] = syn; return
	var edges := []
	for k in na:
		if k + offset >= 0 and k + offset < nb: edges.append([k, k + offset, w, d])
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

static func default_weight(c: Dictionary, pre: String, post: String) -> float:
	var pi := pop_index(c, post); var model := "iaf_psc_alpha"
	if pi >= 0: model = str(c["populations"][pi].get("model", model))
	var w: float = weight_of(model)
	return -w if str(kinds(c).get(pre, "E")) == "I" else w

static func add_projection(c: Dictionary, pre: String, post: String) -> int:
	for i in c["projections"].size():   # one projection per ordered pair in the editor
		var p: Dictionary = c["projections"][i]
		if proj_pop(p.get("pre", "")) == pre and proj_pop(p.get("post", "")) == post: return i
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
	for v in c.get("record", {}).get("voltage", []):
		var pi := pop_index(c, proj_pop(v)); if pi < 0: continue
		var idx: Array = v.get("idx", [0]) if v is Dictionary else [0]
		for k in idx: out.append(pop_first(c, pi) + int(k))
	if out.is_empty():
		for pi in c.get("populations", []).size():
			out.append(pop_first(c, pi))
	return out.slice(0, 4)
