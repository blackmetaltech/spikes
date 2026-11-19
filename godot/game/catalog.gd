class_name Catalog
extends RefCounted
## The parts bin's data: one card per neuron cell type. Three sources, merged by card id:
##   1. the engine's card list, `SpikeTask.catalog_json()` (engine/include/spike/catalog.h): title, summary, examples, source, key parameters with unit
##      and range, preset parameters, default input weight, E/I suitability, whether it is a NESTML cell;
##   2. catalog/thumbs.json, written by the engine's `spike-catalog` tool: per card id {available, fi: [{level, rate_hz}], trace: {t0_ms, dt_ms, v_mV}}.
##      `available` is false when the build cannot run the model (a NESTML cell whose module is not linked in);
##   3. the built-in cards below (the 0.1 models and the REQ-0.2 list), used only when the engine gives no card list (an older extension).
## A card is usable when the build can run it and its chapter is open (the sandbox opens everything). SP_CATALOG=<thumbs.json> reads another thumbs file (dev).
## The older, looser shapes of thumbs.json (a list of [I, rate] pairs, {"I": [], "rate": []}, a bare mV list at dt_ms) are still read by `series()`.

## Unlock chapter per card id (or model name). Unknown cards belong to the first chapter.
const UNLOCK := {"iaf_psc_alpha": "invertebrates", "aeif_cond_alpha": "invertebrates", "izhikevich": "invertebrates", "hh_psc_alpha": "invertebrates", "parrot_neuron": "invertebrates",
	"izhikevich_RS": "invertebrates", "izhikevich_FS": "invertebrates", "izhikevich_IB": "invertebrates", "izhikevich_CH": "invertebrates", "izhikevich_LTS": "spinal", "izhikevich_TC": "breathing", "izhikevich_RZ": "breathing",
	"iaf_cond_exp": "spinal", "iaf_cond_exp_sfa_rr": "spinal", "aeif_cond_exp": "spinal", "hh_cond_exp_traub": "spinal", "mat2_psc_exp": "spinal", "gif_cond_exp": "spinal", "spindle_afferent": "spinal",
	"sa_node_pacemaker": "heart", "sa_node": "heart", "pp_psc_delta": "heart", "hh_psc_alpha_gap": "heart", "hh_cond_beta_gap_traub": "heart",
	"ht_neuron": "breathing", "butera_burster": "breathing", "pbc_burster": "breathing", "icc_slow_wave": "autonomic", "stg_cell": "crab"}

## Built-in cards for an extension that has no catalog_json(): model -> {title, desc, examples, params [[label, value, unit]]}. Values are NEST's defaults.
const CARDS := {
	"iaf_psc_alpha": {"title": "Leaky integrate-and-fire", "desc": "The simplest spiking cell. The membrane voltage leaks back to rest, jumps with each synaptic current, and when it reaches threshold the cell fires and resets. Synaptic currents have an alpha-function shape.",
		"examples": ["generic point neuron", "sensory relay cell", "interneuron in a small reflex"], "params": [["tau m", "10", "ms"], ["threshold", "-55", "mV"], ["C m", "250", "pF"], ["refractory", "2", "ms"]]},
	"aeif_cond_alpha": {"title": "Adaptive exponential IF", "desc": "An integrate-and-fire cell with a sharp exponential spike upstroke and a slow adaptation current. Depending on a and b it fires tonically, adapts, or bursts (Brette and Gerstner 2005).",
		"examples": ["cortical regular-spiking cell", "adapting motor neuron", "bursting interneuron"], "params": [["a", "4", "nS"], ["b", "80.5", "pA"], ["tau w", "144", "ms"], ["Delta T", "2", "mV"]]},
	"izhikevich": {"title": "Izhikevich", "desc": "Two equations, many firing patterns. Four numbers (a, b, c, d) pick regular spiking, intrinsic bursting, chattering, fast spiking and more (Izhikevich 2003).",
		"examples": ["cortical pyramidal cell", "fast-spiking interneuron", "thalamocortical relay"], "params": [["a", "0.02", ""], ["b", "0.2", ""], ["c", "-65", "mV"], ["d", "8", ""]]},
	"hh_psc_alpha": {"title": "Hodgkin-Huxley", "desc": "The squid giant axon: voltage-gated sodium and potassium conductances shape a real action potential, with threshold, refractory period and resonance for free. Expensive but biophysical.",
		"examples": ["squid giant axon", "excitable axon membrane", "textbook action potential"], "params": [["g Na", "12000", "nS"], ["g K", "3600", "nS"], ["g L", "30", "nS"], ["E Na", "50", "mV"]]}}

static var _cards := []        # normalised card Dictionaries, in display order
static var _by_id := {}
static var _loaded := false
static var _src := ""          # the thumbs file that was read ("" when none)
static var _from_engine := false

static func _read_thumbs() -> String:
	var paths := []
	if OS.get_environment("SP_CATALOG") != "": paths.append(OS.get_environment("SP_CATALOG"))
	for base in ["res://catalog", OS.get_executable_path().get_base_dir().path_join("catalog")]:
		paths.append(String(base).path_join("thumbs.json"))
	for p in paths:
		if FileAccess.file_exists(p): _src = p; return FileAccess.get_file_as_string(p)
	return ""

## Load (once): the engine's cards, then the built-in ones when there are none, then the thumbnails and availability.
static func load_catalog() -> void:
	if _loaded: return
	_loaded = true; _cards = []; _by_id = {}; _src = ""; _from_engine = false
	var eng := ""
	if ClassDB.class_exists("SpikeTask") and ClassDB.class_has_method("SpikeTask", "catalog_json"): eng = str(ClassDB.instantiate("SpikeTask").call("catalog_json"))
	var ej: Variant = JSON.parse_string(eng) if eng != "" else null
	if ej is Dictionary and ej.get("models") is Array:
		_from_engine = true
		for m in ej["models"]:
			if m is Dictionary: _add(_norm_engine(m))
	else:
		for model in CARDS: _add(_norm_builtin(model))
	var tj: Variant = JSON.parse_string(_read_thumbs())
	var tm: Variant = null
	if tj is Dictionary: tm = (tj as Dictionary).get("models", tj)
	var thumbs := {}
	if tm is Dictionary: thumbs = tm
	elif tm is Array:
		for e in tm:
			if e is Dictionary: thumbs[str(e.get("id", e.get("model", e.get("name", ""))))] = e
	for id in thumbs:
		var t: Dictionary = thumbs[id] if thumbs[id] is Dictionary else {}
		if not _by_id.has(id):   # a thumbs entry for a card the engine list does not have: a bare card
			if _from_engine: continue
			_add({"id": id, "model": id, "title": id, "summary": "", "examples": [], "params": [], "preset": {}, "source": "", "ei": "", "ei_note": "", "input_weight": 0.0, "weight_unit": "", "nestml": false, "builtin": true})
		var c: Dictionary = _by_id[id]
		if t.has("available"): c["available"] = bool(t["available"])
		c["error"] = str(t.get("error", ""))
		c["fi"] = series_of(t, "fi"); c["vm"] = series_of(t, "vm")
		for k in ["chapter", "title", "summary", "description", "examples"]:
			if t.has(k) and not _from_engine: c[k if k != "description" else "summary"] = t[k]
	for c in _cards:
		if not c.has("available"): c["available"] = bool(Circuits.MODELS.has(c["model"]) or (_from_engine and not bool(c.get("nestml", false)) and _src == ""))
		if not c.has("fi"): c["fi"] = [PackedFloat32Array(), PackedFloat32Array()]; c["vm"] = [PackedFloat32Array(), PackedFloat32Array()]

static func _add(c: Dictionary) -> void:
	if _by_id.has(c["id"]): return
	_cards.append(c); _by_id[c["id"]] = c

static func _norm_engine(m: Dictionary) -> Dictionary:
	var params := []
	for p in m.get("params", []):
		if p is Dictionary: params.append({"key": str(p.get("name", "")), "label": str(p.get("name", "")).replace("_", " "), "value": p.get("default", 0.0), "unit": str(p.get("unit", "")), "desc": str(p.get("desc", "")), "lo": float(p.get("min", 0.0)), "hi": float(p.get("max", 1.0))})
	var preset: Variant = m.get("preset", {})
	if preset is String: preset = JSON.parse_string(preset)
	return {"id": str(m.get("id", m.get("model", ""))), "model": str(m.get("model", m.get("id", ""))), "title": str(m.get("title", "")), "family": str(m.get("family", "")), "summary": str(m.get("summary", "")),
		"examples": Array(m.get("examples", [])), "source": str(m.get("source", "")), "params": params, "preset": preset if preset is Dictionary else {}, "input_weight": float(m.get("input_weight", 0.0)),
		"weight_unit": str(m.get("weight_unit", "")), "ei": str(m.get("ei", "")), "ei_note": str(m.get("ei_note", "")), "nestml": bool(m.get("nestml", false)), "gap": bool(m.get("gap_capable", false)), "drive": m.get("drive", {})}

static func _norm_builtin(model: String) -> Dictionary:
	var b: Dictionary = CARDS[model]; var params := []
	for q in b.get("params", []): params.append({"key": "", "label": str(q[0]), "value": q[1], "unit": str(q[2]), "desc": "", "lo": 0.0, "hi": 0.0})
	var presets: Array = Circuits.PRESETS.get(model, [])
	return {"id": model, "model": model, "title": str(b["title"]), "family": "", "summary": str(b["desc"]), "examples": b.get("examples", []), "source": "", "params": params,
		"preset": (presets[0][1] as Dictionary).duplicate() if not presets.is_empty() else {}, "input_weight": 0.0, "weight_unit": "", "ei": "", "ei_note": "", "nestml": false, "builtin": true}

static func reload() -> void: _loaded = false; load_catalog()
static func has_thumbs() -> bool: load_catalog(); return _src != ""
static func source() -> String: load_catalog(); return _src
static func from_engine() -> bool: load_catalog(); return _from_engine

static func cards() -> Array:
	load_catalog()
	var rank := {}
	for i in _cards.size(): rank[_cards[i]["id"]] = Campaign.chapter_index(chapter_of(_cards[i]["id"])) * 1000 + i
	var out := _cards.duplicate(); out.sort_custom(func(a, b): return rank[a["id"]] < rank[b["id"]])
	return out

static func card(id: String) -> Dictionary: load_catalog(); return _by_id.get(id, {})
static func has_card(id: String) -> bool: load_catalog(); return _by_id.has(id)
## Cards whose model is `model`, in display order (the presets of that model).
static func cards_of(model: String) -> Array: return cards().filter(func(c): return c["model"] == model)

static func chapter_of(id: String) -> String:
	var c := card(id)
	if c.has("chapter") and str(c["chapter"]) != "": return str(c["chapter"])
	return str(UNLOCK.get(id, UNLOCK.get(str(c.get("model", "")), "invertebrates")))

static func available(id: String) -> bool: return bool(card(id).get("available", false))
static func unlocked(id: String, sandbox := false) -> bool:
	if sandbox or Campaign.unlock_all(): return true
	return Campaign.chapters_unlocked().has(chapter_of(id))
static func usable(id: String, sandbox := false) -> bool: return available(id) and unlocked(id, sandbox)

## Models (NEST / NESTML names) with at least one usable card, for the inspector's model stepper.
static func usable_models(sandbox := false) -> Array:
	var out := []
	for c in cards():
		if usable(c["id"], sandbox) and not out.has(c["model"]): out.append(c["model"])
	return out

static func title(id: String) -> String: return str(card(id).get("title", id))
static func description(id: String) -> String:
	var s := str(card(id).get("summary", ""))
	return s if s != "" else "No description yet."
static func examples(id: String) -> Array: return (card(id).get("examples", []) as Array).map(func(x): return str(x))

## Key parameters as [[label, value text, unit]].
static func params(id: String) -> Array:
	var out := []
	for p in card(id).get("params", []): out.append([str(p["label"]), _num(p["value"]), str(p["unit"])])
	return out

static func _num(v: Variant) -> String:
	if v is float or v is int:
		var f := float(v)
		return str(int(roundf(f))) if absf(f) >= 100.0 or is_equal_approx(f, roundf(f)) else str(snappedf(f, 0.01))
	return str(v)

## The default input weight of the first card of `model` (0 when the catalogue has none), in the card's own unit.
static func input_weight_of(model: String) -> float:
	for c in cards_of(model):
		if float(c["input_weight"]) > 0.0: return float(c["input_weight"])
	return 0.0

static func unit_of(model: String) -> String:
	for c in cards_of(model):
		if str(c["weight_unit"]) != "": return str(c["weight_unit"])
	return "nS" if model.contains("cond") else "pA"

## "Advanced" parameter rows from the first card of `model`: [key, label, min, max, step, default, unit], as Circuits.ADVANCED has them.
static func advanced_of(model: String) -> Array:
	var out := []
	for c in cards_of(model):
		for p in c["params"]:
			if str(p["key"]) == "" or float(p["hi"]) <= float(p["lo"]): continue
			out.append([str(p["key"]), str(p["label"]), float(p["lo"]), float(p["hi"]), (float(p["hi"]) - float(p["lo"])) / 200.0, float(p["value"]) if (p["value"] is float or p["value"] is int) else 0.0, str(p["unit"])])
		break
	return out

# ---------- series ----------
## [xs, ys] of the thumbs entry `t`: kind "fi" (input level -> rate) or "vm" (time ms -> mV).
static func series_of(t: Dictionary, kind: String) -> Array:
	var keys: Array = ["fi", "f_i", "fI", "fi_curve", "f-i"] if kind == "fi" else ["trace", "vm", "v_m", "voltage", "vtrace"]
	var raw: Variant = null
	for k in keys:
		if t.has(k): raw = t[k]; break
	return _series(raw, kind)

## Same, for a card id (empty arrays when the card has no thumbnail).
static func series(id: String, kind: String) -> Array:
	var c := card(id)
	var s: Variant = c.get("fi" if kind == "fi" else "vm", null)
	return s if s is Array and (s as Array).size() == 2 else [PackedFloat32Array(), PackedFloat32Array()]

static func _series(raw: Variant, kind: String) -> Array:
	var xs := PackedFloat32Array(); var ys := PackedFloat32Array()
	if raw is Dictionary:
		var d: Dictionary = raw
		var xk := ["level", "I", "i", "i_pA", "I_pA", "current", "x", "t", "t_ms"] if kind == "fi" else ["t", "t_ms", "x", "time"]
		var yk := ["rate_hz", "rate", "f", "f_hz", "hz", "y"] if kind == "fi" else ["v_mV", "v", "V", "v_m", "V_m", "y"]
		var xa: Variant = null; var ya: Variant = null
		for k in xk:
			if d.get(k) is Array: xa = d[k]; break
		for k in yk:
			if d.get(k) is Array: ya = d[k]; break
		if ya != null and xa == null:   # a bare list of mV at dt_ms, starting at t0_ms
			xa = []; var dt := float(d.get("dt_ms", 1.0)); var t0 := float(d.get("t0_ms", 0.0))
			for i in (ya as Array).size(): (xa as Array).append(t0 + float(i) * dt)
		if xa != null and ya != null:
			for i in mini((xa as Array).size(), (ya as Array).size()):
				xs.append(float(xa[i])); ys.append(float(ya[i]))
	elif raw is Array:
		for i in (raw as Array).size():
			var p: Variant = raw[i]
			if p is Array and p.size() >= 2: xs.append(float(p[0])); ys.append(float(p[1]))
			elif p is Dictionary: xs.append(float(p.get("level", p.get("I", p.get("i", p.get("t", p.get("x", i))))))); ys.append(float(p.get("rate_hz", p.get("rate", p.get("f", p.get("v", p.get("y", 0.0)))))))
			elif p is float or p is int: xs.append(float(i)); ys.append(float(p))
	return [xs, ys]
