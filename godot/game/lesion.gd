class_name Lesion
extends RefCounted
## The lesion lab's logic. A lesion set is a Dictionary of keys -> true:
##   "proj:<i>"    cut the axon (projection i): every synapse of it gets weight 0
##   "pop:<id>"    silence the population: its outgoing and incoming synapses are cut, it is taken off the input channels and off the motor outputs
##   "in:<name>"   cut the sensor: the input channel drives nothing (deafferentation)
## The circuit Dictionary is never changed; `apply` returns a lesioned copy and `Runner` re-runs the task on it, headless, for the circuit's own seed, so the
## result is comparable with the intact run. Results make the lesion table (rows -> markdown / CSV).

static func parse_key(key: String) -> Array:
	var p := key.split(":", true, 1); return [p[0], p[1] if p.size() > 1 else ""]

## A plain name for one lesion key: "cut E_L > I_R", "silence E_L", "cut sensor touch".
static func key_label(c: Dictionary, key: String) -> String:
	var k := parse_key(key)
	match k[0]:
		"proj":
			var i := int(k[1]); var projs: Array = c.get("projections", [])
			if i >= 0 and i < projs.size(): return "cut %s > %s" % [Circuits.proj_pop(projs[i].get("pre", "")), Circuits.proj_pop(projs[i].get("post", ""))]
			return "cut axon %s" % k[1]
		"pop": return "silence %s" % k[1]
		"in": return "cut sensor %s" % k[1]
	return key

static func label(c: Dictionary, lesions: Dictionary) -> String:
	if lesions.is_empty(): return "intact (baseline)"
	var keys := lesions.keys(); keys.sort()
	return " + ".join(keys.map(func(k): return key_label(c, str(k))))

## The lesioned copy of a circuit.
static func apply(c: Dictionary, lesions: Dictionary) -> Dictionary:
	var out := c.duplicate(true)
	var projs: Array = out.get("projections", [])
	for key in lesions:
		var k := parse_key(str(key))
		match k[0]:
			"proj":
				if int(k[1]) >= 0 and int(k[1]) < projs.size(): _zero(projs[int(k[1])])
			"pop":
				var id: String = k[1]
				for pr in projs:
					if Circuits.proj_pop(pr.get("pre", "")) == id or Circuits.proj_pop(pr.get("post", "")) == id: _zero(pr)
				for ch in out.get("inputs", []):
					ch["targets"] = (ch.get("targets", []) as Array).filter(func(x): return Circuits.proj_pop(x) != id)
				for ch in out.get("outputs", []):
					ch["neurons"] = (ch.get("neurons", []) as Array).filter(func(x): return Circuits.proj_pop(x) != id)
			"in":
				for ch in out.get("inputs", []):
					if str(ch.get("name", "")) == k[1]: ch["targets"] = []
	return out

static func _zero(pr: Dictionary) -> void:
	if pr.has("edges"):
		for e in pr["edges"]:
			if e is Array and e.size() >= 3: e[2] = 0.0
	var s: Dictionary = pr.get("synapse", {}); s["weight"] = 0.0; pr["synapse"] = s

## Every single lesion worth trying: each axon, each population, each sensor that is wired to something.
static func singles(c: Dictionary) -> Array:
	var out := []
	for i in (c.get("projections", []) as Array).size(): out.append({"proj:%d" % i: true})
	for p in c.get("populations", []): out.append({"pop:" + str(p.get("id", "")): true})
	for ch in c.get("inputs", []):
		if not (ch.get("targets", []) as Array).is_empty(): out.append({"in:" + str(ch.get("name", "")): true})
	return out

# ---------- the lesion table ----------
## One row: {label, lesions (keys), score, passed, verdict, parts, rate (spikes/s)}.
static func markdown(task_title: String, circuit_name: String, seed: int, rows: Array) -> String:
	var s := "# Lesion table: %s\n\nCircuit: %s. Same seed (%d) for every row; each row is one full episode with the lesion applied.\n\n" % [task_title, circuit_name, seed]
	s += "| Lesion | Score | Result | Spikes/s | Verdict |\n|---|---:|---|---:|---|\n"
	for r in rows:
		s += "| %s | %d | %s | %.1f | %s |\n" % [str(r["label"]).replace("|", "/"), int(roundf(float(r["score"]))), "pass" if r["passed"] else "fail", float(r.get("rate", 0.0)), str(r["verdict"]).replace("|", "/").replace("\n", " ")]
	return s

static func csv(rows: Array) -> String:
	var keys := []
	for r in rows:
		for k in (r.get("parts", {}) as Dictionary):
			if not keys.has(str(k)): keys.append(str(k))
	var s := "lesion,score,passed,spikes_per_s,verdict" + "".join(keys.map(func(k): return "," + _q(k))) + "\n"
	for r in rows:
		s += "%s,%.1f,%s,%.1f,%s" % [_q(str(r["label"])), float(r["score"]), "true" if r["passed"] else "false", float(r.get("rate", 0.0)), _q(str(r["verdict"]))]
		for k in keys: s += "," + (str(snappedf(float(r["parts"][k]), 0.01)) if (r["parts"] as Dictionary).has(k) and not (r["parts"][k] is String) else "")
		s += "\n"
	return s

static func _q(t: String) -> String: return "\"" + t.replace("\"", "\"\"") + "\""

## Write both exports to user://lesions/ and return the folder's real path ("" if it could not be written).
static func save(task_id: String, md: String, csv_text: String) -> String:
	var dir := "user://lesions"; DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(dir))
	var stamp := Time.get_datetime_string_from_system().replace(":", "").replace("-", "").replace("T", "-")
	var ok := true
	for pair in [[md, "md"], [csv_text, "csv"]]:
		var fh := FileAccess.open("%s/%s-%s.%s" % [dir, task_id, stamp, pair[1]], FileAccess.WRITE)
		if fh == null: ok = false
		else: fh.store_string(pair[0])
	return ProjectSettings.globalize_path(dir) if ok else ""

# ---------- the re-runner ----------
## Runs a list of jobs ({label, lesions}) one after another, a time slice per call, on its own SpikeSim and task. The engine is a process singleton, so the
## caller stops its live run first and reloads it after (the lab does, as it does for TEST x5).
class Runner extends RefCounted:
	var jobs: Array = []
	var results: Array = []
	var i := 0
	var circuit := {}
	var task_id := ""
	var seed := 1
	var failed := ""
	var _sim = null
	var _task = null
	var _t := -1.0
	var _spikes := 0

	func begin(tid: String, c: Dictionary, js: Array) -> void:
		task_id = tid; circuit = c; jobs = js; results = []; i = 0; _t = -1.0; failed = ""
		seed = int(c.get("seed", 1)); _sim = ClassDB.instantiate("SpikeSim"); _task = Campaign.make(tid)

	func done() -> bool: return i >= jobs.size() or failed != ""
	func fraction() -> float:
		if jobs.is_empty(): return 1.0
		var dur := maxf(float(_task.duration_ms()), 1.0) if _task != null else 1.0
		return (float(i) + clampf(maxf(_t, 0.0) / dur, 0.0, 1.0)) / float(jobs.size())
	func current_label() -> String: return str(jobs[i]["label"]) if i < jobs.size() else ""

	func step(budget_us := 12000) -> void:
		if done(): return
		var dur := float(_task.duration_ms())
		if _t < 0.0:
			var lc := Lesion.apply(circuit, jobs[i]["lesions"])
			if not _sim.load_circuit(Circuits.to_json(Telemetry.sanitized(lc))): failed = "the engine rejected the lesioned circuit: " + str(_sim.error()); return
			_task.reset(seed); _t = 0.0; _spikes = 0
		var t0 := Time.get_ticks_usec()
		while _t < dur and Time.get_ticks_usec() - t0 < budget_us:
			_task.step(_sim, 1.0); _t += 1.0
			var e := str(_sim.error())
			if not bool(_task.step_ok(_sim)) or (e != "" and not e.begins_with("set_input")): failed = "the run stopped: " + e; return
			if int(_t) % 200 == 0: _spikes += (_sim.drain_spikes() as PackedFloat64Array).size() / 2
		if _t < dur: return
		_spikes += (_sim.drain_spikes() as PackedFloat64Array).size() / 2
		var sc: Dictionary = _task.score()
		results.append({"label": jobs[i]["label"], "lesions": (jobs[i]["lesions"] as Dictionary).keys(), "score": float(sc.get("value", 0.0)), "passed": bool(sc.get("passed", false)),
			"verdict": str(sc.get("verdict", "")), "parts": (sc.get("parts", {}) as Dictionary).duplicate(), "rate": float(_spikes) / maxf(dur * 0.001, 0.001)})
		i += 1; _t = -1.0
		if i >= jobs.size(): _sim.stop()
