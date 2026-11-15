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

## Synaptic weight of a typical single connection per target model, in the model's own synapse units (pA for current-based, nS for conductance-based).
const WEIGHT := {"iaf_psc_alpha": 250.0, "aeif_cond_alpha": 20.0, "izhikevich": 25.0, "hh_psc_alpha": 400.0}
const SYNAPSES := ["static_synapse", "stdp_synapse", "tsodyks_synapse"]

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

# ---------- editing ----------
static func model_options(current: String) -> Array:
	var out := []
	for m in MODELS: out.append([m, m])
	if current != "" and not MODELS.has(current): out.append([current, current])
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
	var presets: Array = PRESETS[model]
	var pr: Array = presets[clampi(preset, 0, presets.size() - 1)]
	var pop := {"id": id, "model": model, "n": 1, "params": (pr[1] as Dictionary).duplicate(), "pos": [[at.x, at.y]], "role": "neuron"}
	c["populations"].append(pop)
	var meta: Dictionary = c.get("meta", {}); var ed: Dictionary = meta.get("editor", {}); var ei: Dictionary = ed.get("ei", {})
	ei[id] = kind; ed["ei"] = ei; meta["editor"] = ed; c["meta"] = meta
	return id

static func default_weight(c: Dictionary, pre: String, post: String) -> float:
	var pi := pop_index(c, post); var model := "iaf_psc_alpha"
	if pi >= 0: model = str(c["populations"][pi].get("model", model))
	var w: float = WEIGHT.get(model, 20.0 if model.contains("cond") else 100.0)
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
	var p: Dictionary = c["populations"][pi]; var presets: Array = PRESETS[model]
	p["model"] = model; p["params"] = (presets[clampi(preset, 0, presets.size() - 1)][1] as Dictionary).duplicate()

static func preset_of(p: Dictionary) -> int:
	var presets: Array = PRESETS.get(str(p.get("model", "")), [])
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
