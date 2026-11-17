class_name Lesion
extends RefCounted
## The lesion lab's logic. A lesion set is a Dictionary of keys -> true:
##   "proj:<i>"    cut the axon (projection i): every synapse of it gets weight 0
##   "pop:<id>"    silence the population: its outgoing and incoming synapses are cut, it is taken off the input channels and off the motor outputs
##   "in:<name>"   cut the sensor: the input channel drives nothing (deafferentation)
## The circuit Dictionary is never changed; `apply` returns a lesioned copy and `Runner` re-runs the task on it, headless, on several seeds (the task and the
## circuit are both reseeded, as the TEST does), so a row is a mean with a range and a k-of-n pass count, and the intact row says how steady the circuit is.
## Results make the lesion table (rows -> markdown / CSV), each with the expected biology from LesionHints.

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
## Seeds a lesion row runs on (the intact row too): the task is reset with `seed`, the circuit is loaded with seed 1000 * seed + its own seed.
const SEEDS := [1, 2, 3]

## "mean (lo-hi)" of a row's score, or the single score for one seed.
static func score_text(r: Dictionary) -> String:
	var m := int(roundf(float(r["score"]))); var lo := int(roundf(float(r.get("lo", r["score"])))); var hi := int(roundf(float(r.get("hi", r["score"]))))
	return "%d" % m if lo == hi else "%d (%d-%d)" % [m, lo, hi]

## "2 of 3 seeds" or "" for a one-seed row.
static func pass_text(r: Dictionary) -> String:
	var n := int(r.get("n", 1))
	return "" if n <= 1 else "%d of %d seeds" % [int(r.get("passes", 0)), n]

## True when the row's intact baseline passes on some seeds and not others (the table's other rows are then partly seed noise).
static func baseline_flaky(rows: Array) -> bool:
	for r in rows:
		if (r["lesions"] as Array).is_empty(): return int(r.get("n", 1)) > 1 and int(r.get("passes", 0)) > 0 and int(r.get("passes", 0)) < int(r.get("n", 1))
	return false

## One row: {label, lesions (keys), score (mean), lo, hi, passes, n, passed, verdict, parts (means), rate (spikes/s), name, bio}.
static func markdown(task_title: String, circuit_name: String, seed: int, rows: Array) -> String:
	var ns := int(rows[0].get("n", 1)) if not rows.is_empty() else 1
	var s := "# Lesion table: %s\n\nCircuit: %s (seed %d). Every row is %d full episode%s with the lesion applied, on seeds reseeding the world and the circuit; the score is the mean (lowest-highest).\n\n" % [task_title, circuit_name, seed, ns, "" if ns == 1 else "s"]
	if baseline_flaky(rows): s += "Note: the intact circuit itself fails on some seeds, so differences of a few points between rows are noise.\n\n"
	s += "| Lesion | Score | Seeds passed | Spikes/s | Verdict | Expected biology |\n|---|---:|---|---:|---|---|\n"
	for r in rows:
		s += "| %s | %s | %s | %.1f | %s | %s |\n" % [str(r["label"]).replace("|", "/"), score_text(r), "%d/%d" % [int(r.get("passes", 1 if r["passed"] else 0)), int(r.get("n", 1))], float(r.get("rate", 0.0)), str(r["verdict"]).replace("|", "/").replace("\n", " "), str(r.get("bio", "")).replace("|", "/")]
	return s

static func csv(rows: Array) -> String:
	var keys := []
	for r in rows:
		for k in (r.get("parts", {}) as Dictionary):
			if not keys.has(str(k)): keys.append(str(k))
	var s := "lesion,score_mean,score_min,score_max,seeds_passed,seeds,passed,spikes_per_s,verdict,expected_biology" + "".join(keys.map(func(k): return "," + _q(k))) + "\n"
	for r in rows:
		s += "%s,%.1f,%.1f,%.1f,%d,%d,%s,%.1f,%s,%s" % [_q(str(r["label"])), float(r["score"]), float(r.get("lo", r["score"])), float(r.get("hi", r["score"])), int(r.get("passes", 1 if r["passed"] else 0)), int(r.get("n", 1)), "true" if r["passed"] else "false", float(r.get("rate", 0.0)), _q(str(r["verdict"])), _q(str(r.get("bio", "")))]
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
## Runs a list of jobs ({label, lesions}) one after another, each on every seed of `seeds`, a time slice per call, on its own SpikeSim and task. The engine is a
## process singleton, so the caller stops its live run first and reloads it after (the lab does, as it does for TEST x5).
class Runner extends RefCounted:
	var jobs: Array = []
	var results: Array = []
	var seeds: Array = []
	var i := 0            # job being run
	var si := 0           # seed (index into `seeds`) being run
	var circuit := {}
	var task_id := ""
	var seed := 1
	var failed := ""
	var _sim = null
	var _task = null
	var mode := "standard"     # v0.3: difficulty mode and challenge variant of the task being re-run (set before begin())
	var variant := ""
	var _t := -1.0
	var _spikes := 0
	var _runs: Array = []   # the finished runs of the current job

	func begin(tid: String, c: Dictionary, js: Array, sds: Array = []) -> void:
		task_id = tid; circuit = c; jobs = js; results = []; i = 0; si = 0; _t = -1.0; failed = ""; _runs = []
		seeds = sds if not sds.is_empty() else Lesion.SEEDS.duplicate()
		seed = int(c.get("seed", 1)); _sim = ClassDB.instantiate("SpikeSim"); _task = Campaign.make(tid); Telemetry.configure(_task, mode, variant)

	func done() -> bool: return i >= jobs.size() or failed != ""
	func fraction() -> float:
		if jobs.is_empty(): return 1.0
		var dur := maxf(float(_task.duration_ms()), 1.0) if _task != null else 1.0
		var per := float(maxi(seeds.size(), 1))
		return (float(i) + (float(si) + clampf(maxf(_t, 0.0) / dur, 0.0, 1.0)) / per) / float(jobs.size())
	func current_label() -> String: return str(jobs[i]["label"]) if i < jobs.size() else ""
	func current_seed() -> int: return int(seeds[si]) if si < seeds.size() else 0

	func step(budget_us := 12000) -> void:
		if done(): return
		var dur := float(_task.duration_ms())
		if _t < 0.0:
			var lc := Lesion.apply(circuit, jobs[i]["lesions"])
			lc["seed"] = 1000 * int(seeds[si]) + seed   # the circuit is reseeded per run, as the engine's own tests and TEST x5 do
			if not _sim.load_circuit(Circuits.to_json(Telemetry.sanitized(lc))): failed = "the engine rejected the lesioned circuit: " + str(_sim.error()); return
			_task.reset(int(seeds[si])); _t = 0.0; _spikes = 0
		var t0 := Time.get_ticks_usec()
		while _t < dur and Time.get_ticks_usec() - t0 < budget_us:
			_task.step(_sim, 1.0); _t += 1.0
			var e := str(_sim.error())
			if not bool(_task.step_ok(_sim)) or (e != "" and not e.begins_with("set_input")): failed = "the run stopped: " + e; return
			if int(_t) % 200 == 0: _spikes += (_sim.drain_spikes() as PackedFloat64Array).size() / 2
		if _t < dur: return
		_spikes += (_sim.drain_spikes() as PackedFloat64Array).size() / 2
		var sc: Dictionary = _task.score()
		_runs.append({"score": float(sc.get("value", 0.0)), "passed": bool(sc.get("passed", false)), "verdict": str(sc.get("verdict", "")), "parts": (sc.get("parts", {}) as Dictionary).duplicate(), "rate": float(_spikes) / maxf(dur * 0.001, 0.001)})
		_t = -1.0; si += 1
		if si < seeds.size(): return
		results.append(_aggregate(jobs[i]))
		_runs = []; si = 0; i += 1
		if i >= jobs.size(): _sim.stop()

	## One row from the runs of a job: mean score with range, passes, the verdict of the worst seed, mean parts and rate, and the expected biology.
	func _aggregate(job: Dictionary) -> Dictionary:
		var n := _runs.size(); var tot := 0.0; var lo := 1.0e9; var hi := -1.0e9; var passes := 0; var rate := 0.0; var worst: Dictionary = _runs[0]
		var parts := {}; var counts := {}
		for r in _runs:
			var sc := float(r["score"]); tot += sc; lo = minf(lo, sc); hi = maxf(hi, sc); rate += float(r["rate"])
			if bool(r["passed"]): passes += 1
			if sc < float(worst["score"]): worst = r
			for k in (r["parts"] as Dictionary):
				if r["parts"][k] is String: continue
				parts[k] = float(parts.get(k, 0.0)) + float(r["parts"][k]); counts[k] = int(counts.get(k, 0)) + 1
		for k in parts: parts[k] = float(parts[k]) / float(counts[k])
		var hint := LesionHints.find(task_id, circuit, job["lesions"])
		return {"label": job["label"], "lesions": (job["lesions"] as Dictionary).keys(), "score": tot / float(n), "lo": lo, "hi": hi, "passes": passes, "n": n, "passed": passes * 2 > n,
			"verdict": str(worst["verdict"]), "parts": parts, "rate": rate / float(n), "name": str(hint["name"]), "bio": str(hint["bio"]), "exact": bool(hint["exact"])}
