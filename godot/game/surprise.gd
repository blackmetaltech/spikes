class_name Surprise
extends RefCounted
## Surprise moments (docs/DESIGN-0.3.md section 7, Schell's surprise lens): emergent behaviour the player did not ask for, noticed and celebrated.
##   oscillation  a task that is not about rhythm whose circuit nonetheless fires in a periodic burst pattern ("You built a CPG by accident!")
##   lesion       a lesion that makes the score go UP (a part that was hurting)
## Detection is pure analysis of the spike record the Telemetry already keeps, so it costs nothing beyond one pass over a few seconds of spikes.

## Tasks (and concepts) where a rhythm is the point: no surprise there.
const RHYTHM_TASKS := ["lamprey", "heart_pacemaker", "heart_autonomic", "breathing", "peristalsis", "worm"]
const RHYTHM_WORDS := ["oscill", "rhythm", "pacemaker", "cpg", "burst", "central_pattern", "half_center", "metronome", "clock"]

const BIN_MS := 10.0
const WINDOW_MS := 4000.0
const MIN_WINDOW_MS := 1800.0
const LAG_MIN_MS := 100.0
const LAG_MAX_MS := 1500.0
const ACF_MIN := 0.45            # normalised autocorrelation of the binned population rate at the best lag
const SPIKES_PER_CYCLE_MIN := 2.2   # a burst rhythm, not one neuron ticking regularly
const ON_FRACTION_MAX := 0.55    # share of bins with spikes: a rhythm has quiet gaps
const MIN_SPIKES := 30

## True when the task is allowed to oscillate (so an oscillation is no surprise).
static func is_rhythm_task(task_id: String, concepts: Array) -> bool:
	if RHYTHM_TASKS.has(task_id): return true
	for c in concepts:
		var low := str(c).to_lower()
		for w in RHYTHM_WORDS:
			if w in low: return true
	return false

## The periodicity of the spike record: {found, period_ms, acf, spikes_per_cycle, on_fraction, pop} for the most rhythmic population (or the whole network),
## over the last WINDOW_MS of model time. found is false for a short record, a silent network, a tonic cell, or an irregular one.
static func rhythm(tele: Telemetry) -> Dictionary:
	var out := {"found": false, "period_ms": 0.0, "acf": 0.0, "spikes_per_cycle": 0.0, "on_fraction": 0.0, "pop": ""}
	if tele == null or tele.sim == null or tele.n < 1: return out
	var t1 := tele.t_ms; var t0 := maxf(t1 - WINDOW_MS, 0.0)
	if t1 - t0 < MIN_WINDOW_MS: return out
	var nb := int((t1 - t0) / BIN_MS)
	# whole network first, then each population of at least two cells
	var groups := [["", -1, -1]]
	var pops: Array = tele.circuit.get("populations", [])
	for pi in pops.size():
		if Circuits.pop_n(pops[pi]) >= 2: groups.append([str(pops[pi].get("id", "")), Circuits.pop_first(tele.circuit, pi), Circuits.pop_first(tele.circuit, pi) + Circuits.pop_n(pops[pi])])
	var best_acf := 0.0
	for g in groups:
		var bins := PackedFloat32Array(); bins.resize(nb)
		var total := 0
		for j in tele.sp_t.size():
			var t := tele.sp_t[j]
			if t < t0 or t >= t1: continue
			var id := int(tele.sp_n[j])
			if int(g[1]) >= 0 and (id < int(g[1]) or id >= int(g[2])): continue
			var b := int((t - t0) / BIN_MS)
			if b >= 0 and b < nb: bins[b] += 1.0; total += 1
		if total < MIN_SPIKES: continue
		var r := _analyse(bins, total)
		if bool(r["found"]) and float(r["acf"]) > best_acf:
			best_acf = float(r["acf"]); out = r; out["pop"] = str(g[0])
	return out

static func _analyse(bins: PackedFloat32Array, total: int) -> Dictionary:
	var nb := bins.size(); var mean := float(total) / float(nb)
	var var0 := 0.0; var on := 0
	for b in bins:
		var0 += (b - mean) * (b - mean)
		if b > 0.0: on += 1
	var res := {"found": false, "period_ms": 0.0, "acf": 0.0, "spikes_per_cycle": 0.0, "on_fraction": float(on) / float(nb)}
	if var0 <= 1e-6: return res
	var lag_lo := int(LAG_MIN_MS / BIN_MS); var lag_hi := mini(int(LAG_MAX_MS / BIN_MS), nb / 2)
	var best := -1.0; var best_lag := 0
	for lag in range(lag_lo, lag_hi + 1):
		var s := 0.0
		for i in nb - lag: s += (bins[i] - mean) * (bins[i + lag] - mean)
		var a := s / var0 * float(nb) / float(nb - lag)
		if a > best: best = a; best_lag = lag
	if best_lag <= 0: return res
	var period := float(best_lag) * BIN_MS
	var cycles := maxf(float(nb) * BIN_MS / period, 1.0)
	res["period_ms"] = period; res["acf"] = best; res["spikes_per_cycle"] = float(total) / cycles
	res["found"] = best >= ACF_MIN and float(res["spikes_per_cycle"]) >= SPIKES_PER_CYCLE_MIN and float(res["on_fraction"]) <= ON_FRACTION_MAX
	return res

## The toast text for an oscillation found by `rhythm()` ({title, text}).
static func oscillation_text(r: Dictionary) -> Dictionary:
	var hz := 1000.0 / maxf(float(r["period_ms"]), 1.0)
	var where := ("in %s " % str(r["pop"])) if str(r["pop"]) != "" else ""
	return {"title": "You built a rhythm by accident!", "text": "Your circuit bursts %severy %d ms (%.1f Hz) although the task never asked for it. Feedback loops of excitation and inhibition make clocks: this is how real central pattern generators begin." % [where, int(roundf(float(r["period_ms"]))), hz]}

## The lesion row (not the intact baseline) whose score beats the baseline by at least `margin` points, the best one; {} when none does. Rows as in Lesion.Runner.
static func lesion_improves(rows: Array, margin := 4.0) -> Dictionary:
	var base := -1.0
	for r in rows:
		if (r["lesions"] as Array).is_empty(): base = float(r["score"])
	if base < 0.0: return {}
	var best := {}; var gain := margin
	for r in rows:
		if (r["lesions"] as Array).is_empty(): continue
		var g := float(r["score"]) - base
		if g >= gain: gain = g; best = r
	if best.is_empty(): return {}
	return {"label": str(best["label"]), "gain": gain, "row": best}
