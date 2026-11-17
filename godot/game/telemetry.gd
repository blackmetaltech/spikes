class_name Telemetry
extends RefCounted
## One running experiment: a SpikeSim loaded with the editor's circuit, optionally driven by a SpikeTask, stepped in 1 ms slices from the
## frame loop under a time budget. Collects what the scopes and the circuit view draw: recent spikes, 1 ms voltage samples, flash levels,
## travelling pulses, smoothed output rates, and whole-run spike statistics.
const SLICE_MS := 1.0
static var budget_us := 9000     # at most this much wall time per frame spent stepping (SP_BUDGET_US overrides: screenshots on a software renderer)
const HISTORY_MS := 20000.0      # spikes kept for the raster, the analysis and the stats
const VOLT_KEEP := 20000
const FLASH_TAU := 0.11          # s, wall clock: how long a soma glows after a spike
const PULSE_MIN_S := 0.14        # a pulse takes delay_ms * PULSE_S_PER_MS of wall time, so a 1-5 ms synaptic delay is visible
const PULSE_S_PER_MS := 0.085
const PULSE_MAX_S := 0.55

var sim = null
var task = null
var circuit := {}
var error := ""
var failed := false              # the engine reported an error while stepping: the run was stopped
var running := true
var speed := 1.0
var achieved := 1.0              # simulated ms per wall ms, as actually delivered
var t_ms := 0.0
var view_t := 0.0                # smoothly advancing display time (ms), never ahead of t_ms
var n := 0
var kinds: Array = []            # "E"/"I" per neuron
var pop_of: PackedInt32Array = PackedInt32Array()   # neuron -> population index
var out_projs: Array = []        # population index -> [projection indices]
var sp_n := PackedInt32Array()   # recent spikes: neuron ...
var sp_t := PackedFloat64Array() # ... and time (ms)
var count := PackedInt32Array()  # whole-run spikes per neuron
var first_t := PackedFloat64Array(); var last_t := PackedFloat64Array()
var isi_sum := PackedFloat64Array(); var isi_sq := PackedFloat64Array(); var isi_n := PackedInt32Array()
var total_spikes := 0
var watched: Array = []
var volt := {}                   # neuron -> PackedFloat32Array of 1 ms samples (newest last)
var flash := PackedFloat32Array()
var pulses: Array = []           # projection index -> PackedFloat64Array of wall birth times
var rates := {}                  # output channel -> smoothed Hz (spike count in the last RATE_WIN_MS over the channel's neurons)
var out_mask := {}               # output channel -> PackedByteArray (1 = neuron belongs to it)
const RATE_WIN_MS := 120.0
var in_values := {}              # input channel -> last value we set (for the circuit view)
var body := {}
var score := {}
var finished := false
var hold := false                # freeze the instant without showing "paused" (screenshots)
var episode := 0
## v0.2: readout history for the perturbation timeline and the dashboard (sampled every HIST_STEP_MS of model time) and the modulator dials.
const HIST_STEP_MS := 50.0
var hist_on := false
var hist_t := PackedFloat32Array()          # ms
var hist := {}                               # readout name -> PackedFloat32Array aligned with hist_t
var mode := "standard"                       # v0.3 difficulty mode ("guided" | "standard" | "researcher") and challenge variant, applied to the task before reset()
var variant := ""
var audio_on := true                         # live view: spikes click, the heart beats, the lungs breathe (GameAudio); off for headless-style runs
var _last_beat := -1.0
var skip_to := -1.0                          # v0.3 pacing: model time (ms) a "skip to next event" is fast-forwarding to (< 0 = not skipping)
var lesioned := false                        # the lab runs a lesioned copy of the circuit: do not complain about its wiring
var mods := {}                               # "NE" / "ACh" / "DA" -> concentration set by the player (sandbox dials)
var _next_hist := 0.0
var _owed := 0.0
var _wall := 0.0
var task_id := ""                            # the level this run belongs to ("" = free run); the play log counts a run at every episode start
var _ep_t0 := 0                              # ticks (us) at the episode start: SP_TIMING reports the wall time of a whole run
var _dbg_frames := 0; var _dbg_step_us := 0.0
var _vc := 0
var _hold_last := 0.0                        # last model ms pushed to the scopes (decimated voltage samples at high speed)
## v0.3 pacing: the slowest stepping any run has measured so far in this session (model ms per wall ms with the whole share spent stepping). Unknown = 0.
static var throughput := 0.0
const WALL_SHARE := 0.7                      # share of a frame's wall time the stepper may use when the run is behind (a software renderer, a slow frame)
const BUDGET_MAX_US := 60000

func now() -> float: return _wall

## Build everything for `c` (circuit Dictionary) and `tid` ("" = free run). The task is ALWAYS created first, so a circuit the engine rejects (or one with
## nothing in it yet) still shows the task's world and brief, with a verdict that names what is missing. Returns true when the engine is running the circuit;
## on false `error` holds the engine's message ("" for a bench with no neurons yet).
func setup(c: Dictionary, tid: String, ext_ok: bool) -> bool:
	task_id = tid
	circuit = c; error = ""; finished = false; failed = false; sim = null; task = null; body = {}; score = {}
	if not ext_ok: error = "The spike-gd GDExtension is not loaded. Run scripts/build-gd.sh."; return false
	if tid != "":
		task = Campaign.make(tid)
		if task == null: error = "unknown task " + tid; return false
		hist_on = _wants_hist(tid)
		configure(task, mode, variant)
		task.reset(int(c.get("seed", 1))); body = task.body(); score = task.score()
	n = Circuits.neuron_count(c); kinds = Circuits.kind_per_neuron(c)
	rates = {}
	for ch in c.get("outputs", []): rates[str(ch.get("name", ""))] = 0.0
	_apply_issue()
	if n == 0: return false
	sim = ClassDB.instantiate("SpikeSim")
	if not sim.load_circuit(Circuits.to_json(sanitized(c))):
		error = str(sim.error()); sim = null; _apply_issue(); return false
	n = int(sim.neuron_count())
	pop_of = PackedInt32Array(); out_projs = []
	for pi in c["populations"].size():
		out_projs.append([])
		for k in Circuits.pop_n(c["populations"][pi]): pop_of.append(pi)
	for i in c["projections"].size():
		var pre := Circuits.pop_index(c, Circuits.proj_pop(c["projections"][i].get("pre", "")))
		if pre >= 0: out_projs[pre].append(i)
	pulses = []; pulses.resize(c["projections"].size())
	for i in pulses.size(): pulses[i] = PackedFloat64Array()
	watched = Circuits.default_watch(c)
	reset()
	return true

## v0.3: put a task in a difficulty mode and challenge variant (before reset()). A task object that has no such methods (mock tasks, an older extension) is left alone.
static func configure(t: Variant, mode_id: String, variant_id: String) -> void:
	if t == null: return
	if t.has_method("set_mode"): t.call("set_mode", mode_id)
	if t.has_method("set_variant"): t.call("set_variant", variant_id)

## Fast-forward to model time `t` (ms): the stepper ignores the speed and spends its whole frame budget until it gets there.
func skip_until(t: float) -> void:
	if sim == null or finished: return
	skip_to = t; running = true

## The circuit as the engine takes it: channels that select no neuron yet are left out (their input is never driven and their output reads 0).
static func sanitized(c: Dictionary) -> Dictionary:
	var out := c.duplicate(true)
	var ins := []; var outs := []
	for ch in out.get("inputs", []):
		if not (ch.get("targets", []) as Array).is_empty(): ins.append(ch)
	for ch in out.get("outputs", []):
		if not (ch.get("neurons", []) as Array).is_empty(): outs.append(ch)
	out["inputs"] = ins; out["outputs"] = outs
	return out

## What stops the task from being playable yet, in the player's words ("" when the circuit is wired from every sensor to every motor).
func wiring_issue() -> String:
	if task == null: return ""
	var miss_in := []; var miss_out := []
	for s in task.sensor_channels():
		var ok := false
		for ch in circuit.get("inputs", []):
			if str(ch.get("name", "")) == str(s) and not (ch.get("targets", []) as Array).is_empty(): ok = true
		if not ok: miss_in.append(str(s))
	for m in task.motor_channels():
		var ok := false
		for ch in circuit.get("outputs", []):
			if str(ch.get("name", "")) == str(m) and not (ch.get("neurons", []) as Array).is_empty(): ok = true
		if not ok: miss_out.append(str(m))
	if n == 0:
		return "Add a neuron, then connect %s to it and it to %s." % [_list(task.sensor_channels()), _list(task.motor_channels())]
	var parts := []
	if not miss_in.is_empty(): parts.append("connect %s to a neuron" % _list(miss_in))
	if not miss_out.is_empty(): parts.append("connect a neuron to %s" % _list(miss_out))
	if parts.is_empty(): return ""
	var t := " and ".join(parts)
	return t.substr(0, 1).to_upper() + t.substr(1) + "."

static func _list(a) -> String:
	var l := []
	for x in a: l.append(str(x))
	return " and ".join(l) if l.size() <= 2 else ", ".join(l.slice(0, l.size() - 1)) + " and " + l[l.size() - 1]

## Put the wiring problem (if any) into the score's verdict, unless the engine itself failed (then that message is shown by the lab).
func _apply_issue() -> void:
	if task == null or lesioned: return
	var iss := wiring_issue()
	if iss != "": score["verdict"] = iss; score["passed"] = false

func reset(seed := -1) -> void:
	if sim == null: return
	episode += 1; _ep_t0 = Time.get_ticks_usec()
	if task_id != "": Playlog.run_started(task_id, int(circuit.get("seed", 1)))
	var circ := Circuits.to_json(sanitized(circuit))
	failed = false
	if not sim.load_circuit(circ): error = str(sim.error()); failed = true; running = false; return   # a fresh kernel: same circuit, same seed, so an episode is repeatable
	t_ms = 0.0; view_t = 0.0; _owed = 0.0; finished = false; total_spikes = 0; skip_to = -1.0; _last_beat = -1.0
	sp_n = PackedInt32Array(); sp_t = PackedFloat64Array()
	count = PackedInt32Array(); count.resize(n); first_t = PackedFloat64Array(); first_t.resize(n); last_t = PackedFloat64Array(); last_t.resize(n)
	isi_sum = PackedFloat64Array(); isi_sum.resize(n); isi_sq = PackedFloat64Array(); isi_sq.resize(n); isi_n = PackedInt32Array(); isi_n.resize(n)
	for i in n: first_t[i] = -1.0; last_t[i] = -1.0
	flash = PackedFloat32Array(); flash.resize(n)
	for p in pulses: p.clear()
	volt = {}; rates = {}
	for w in watched: sim.watch(w); volt[w] = PackedFloat32Array()
	out_mask = {}
	for ch in circuit.get("outputs", []): rates[str(ch.get("name", ""))] = 0.0   # unwired ones stay at 0
	for ch in sim.outputs():
		var m := PackedByteArray(); m.resize(n)
		for i in ch["neurons"]: m[int(i)] = 1
		out_mask[str(ch["name"])] = m; rates[str(ch["name"])] = 0.0
	if task != null: task.reset(int(circuit.get("seed", 1)) if seed < 0 else seed); body = task.body(); score = task.score(); _apply_issue()
	hist_t = PackedFloat32Array(); hist = {}; _next_hist = 0.0
	for k in mods: _push_mod(k)

## Only tasks with a judged trace or a body view beyond the generic polyline keep a history.
static func _wants_hist(task_id: String) -> bool:
	var d := Campaign.info(task_id)
	return not (d["bands"] as Array).is_empty() or str(d["view"]) != "polyline"

## Record the task's readouts at the current model time (called from the stepping loop every HIST_STEP_MS).
func _sample_hist() -> void:
	var ro: Dictionary = (task.body() as Dictionary).get("readouts", {})
	var n0 := hist_t.size(); hist_t.append(t_ms)
	for k in ro:
		var key := str(k)
		var a: PackedFloat32Array
		if hist.has(key): a = hist[key]
		else:
			a = PackedFloat32Array(); a.resize(n0); a.fill(float(ro[k]))   # a late key is back-filled with its first value
		a.append(float(ro[k])); hist[key] = a   # packed arrays are values: store the appended copy back
	for k in hist.keys():
		if not ro.has(k):
			var a2: PackedFloat32Array = hist[k]; a2.append(a2[n0 - 1] if n0 > 0 and a2.size() >= n0 else 0.0); hist[k] = a2
	_next_hist = t_ms + HIST_STEP_MS

## Neuromodulator concentration dial (sandbox): remembered, and pushed to the engine (SpikeSim.set_modulator).
static func modulators_available() -> bool:
	return ClassDB.class_exists("SpikeSim") and ClassDB.class_has_method("SpikeSim", "set_modulator")
func set_modulator(name: String, conc: float) -> void:
	mods[name] = conc; _push_mod(name)
func _push_mod(name: String) -> void:
	if sim != null: sim.set_modulator(name, float(mods[name]))
## What the circuit currently sees: the task's own readout of that name, else the engine's total (dial plus release pools), else the dial.
func modulator_level(name: String) -> float:
	var ro: Dictionary = body.get("readouts", {})
	if ro.has(name): return float(ro[name])   # a task that reports its own NE / ACh / DA level
	if sim != null: return float(sim.modulator(name))
	return float(mods.get(name, 0.0))

## Network spike rate (all neurons, spikes per second of model time) and the ATP it costs: roughly 1e8 to 1e9 ATP per spike
## (Attwell and Laughlin 2001; Lennie 2003 for human cortex), so this is an order-of-magnitude figure, shown as a range.
func spike_rate_hz() -> float: return float(total_spikes) / maxf(t_ms * 0.001, 0.05)
static func atp_range(spikes_per_s: float) -> Vector2: return Vector2(spikes_per_s * 1.0e8, spikes_per_s * 1.0e9)
static func atp_text(spikes_per_s: float) -> String:
	if spikes_per_s <= 0.0: return "0 ATP/s"
	var r := atp_range(spikes_per_s)
	return "%s to %s ATP/s" % [_sci(r.x), _sci(r.y)]
static func _sci(v: float) -> String:
	var e := int(floor(log(maxf(v, 1.0)) / log(10.0))); var m := v / pow(10.0, e)
	return "%.1fe%d" % [m, e]

func set_watched(list: Array) -> void:
	watched = list.slice(0, 4)
	for w in watched:
		sim.watch(w)
		if not volt.has(w): volt[w] = PackedFloat32Array()
	for k in volt.keys():
		if not watched.has(k): volt.erase(k)

func duration() -> float: return float(task.duration_ms()) if task != null else 0.0

## Advance by `dt` seconds of wall time.
func advance(dt: float) -> void:
	if finished or not running:
		if skip_to >= 0.0 or (audio_on and running == false): GameAudio.silence_body()
		skip_to = -1.0
	if sim != null and (not running or finished or hold):   # paused: the glows, pulses and scopes hold the instant
		_collect(); view_t = t_ms; return
	_wall += dt
	for i in flash.size(): flash[i] = flash[i] * exp(-dt / FLASH_TAU) if flash[i] > 0.004 else 0.0
	if sim == null: return
	var stepped := 0.0
	if running and not finished:
		_owed = minf(_owed + dt * 1000.0 * speed, 200.0 * maxf(speed, 1.0) if budget_us < 100000 else 1.0e9)
		if skip_to > t_ms: _owed = maxf(_owed, skip_to - t_ms)
		else: skip_to = -1.0
		# v0.3 pacing: at the base budget when the run is on time; when it is behind (speed > 1, a slow frame, a software renderer) it may spend a share of the
		# frame's wall time instead of a fixed 9 ms, so the lab delivers the speed it was asked for.
		var budget := budget_us
		if speed > 1.5 or skip_to >= 0.0: budget = clampi(int(dt * 1.0e6 * WALL_SHARE), budget_us, maxi(BUDGET_MAX_US, budget_us))
		var t0 := Time.get_ticks_usec(); var k := 0; var dur := duration(); var vstride := 1 if speed <= 2.0 else clampi(int(speed * 0.5), 2, 8)
		while _owed >= SLICE_MS:
			if task != null: task.step(sim, SLICE_MS)
			else: sim.step(SLICE_MS)
			k += 1
			if k % 4 == 1 and _step_failed(): break   # the engine's error text is polled every 4th slice (two bridge calls per slice were a third of the cost)
			t_ms += SLICE_MS; _owed -= SLICE_MS; stepped += SLICE_MS
			if hist_on and task != null and t_ms >= _next_hist: _sample_hist()
			_vc += 1   # voltage samples: every slice at <= 2x, every `vstride`-th at faster speeds (the others repeat it; each read is a bridge call per watched neuron)
			if _vc >= vstride:
				_vc = 0
				for w in watched: _push_volt(w)
			else:
				for w in watched:
					var a: PackedFloat32Array = volt[w]; a.append(a[a.size() - 1] if a.size() > 0 else 0.0)
			if task != null and t_ms >= dur: finished = true; break
			if skip_to >= 0.0 and t_ms >= skip_to: skip_to = -1.0; _owed = minf(_owed, 4.0); break
			if k % 4 == 0 and Time.get_ticks_usec() - t0 > budget:
				if skip_to < 0.0: _owed = minf(_owed, 4.0)
				break
		var el := float(Time.get_ticks_usec() - t0); _dbg_frames += 1; _dbg_step_us += el
		if stepped >= 20.0 and el > 0.0: throughput = stepped * 1000.0 / el if throughput <= 0.0 else lerpf(throughput, stepped * 1000.0 / el, 0.1)
		var want := dt * 1000.0 * speed
		achieved = lerpf(achieved, speed * (stepped / want) if want > 0.0 else speed, 0.1)
	_collect()
	if finished and OS.get_environment("SP_TIMING") != "": _report_timing()  # after _collect: report the final score, not the in-progress one
	if sim != null:
		for ch in rates.keys():
			if not out_mask.has(ch): continue   # an output nothing is wired to reads 0
			var m: PackedByteArray = out_mask[ch]; var cnt := 0; var nn := 0
			for i in n: nn += m[i]
			var j := sp_t.size() - 1; var t0 := t_ms - RATE_WIN_MS
			while j >= 0 and sp_t[j] >= t0:
				if m[sp_n[j]] == 1: cnt += 1
				j -= 1
			var hz := float(cnt) / (RATE_WIN_MS * 0.001 * maxf(float(nn), 1.0))
			rates[ch] = lerpf(float(rates[ch]), hz, clampf(dt * 10.0, 0.0, 1.0))
	if task != null and stepped > 0.0:
		body = task.body(); score = task.score(); _apply_issue()
		if audio_on: _body_audio()
	if running and not finished: view_t = clampf(view_t + dt * 1000.0 * achieved_or_speed(), t_ms - 40.0, t_ms)
	else: view_t = t_ms

## Body sounds from the task's readouts: a heartbeat whenever last_beat_ms moves, the lungs from the volume fraction. They play at any speed: beats are
## decimated to one per HEART_MIN_GAP_S of wall time (at 10x only every few beats sound, never none), and the breath's flow is scaled by the speed so a fast
## run sounds like breathing, not like a buzz. Only a skip-to-event is silent.
const HEART_MIN_GAP_S := 0.32
var _last_beat_wall := -10.0
func _body_audio() -> void:
	if skip_to >= 0.0: return
	var ro: Dictionary = body.get("readouts", {})
	if ro.has("last_beat_ms"):
		var lb := float(ro["last_beat_ms"])
		if lb > _last_beat + 1.0 and lb >= 0.0 and _wall - _last_beat_wall >= (HEART_MIN_GAP_S if speed > 1.5 else 0.0):
			GameAudio.body("heart", 1.0); _last_beat_wall = _wall
		_last_beat = lb
	if ro.has("volume"): GameAudio.body("breath", clampf(float(ro["volume"]), 0.0, 1.0), maxf(speed, 1.0))

## SP_TIMING=1: print the wall time of each finished episode (and quit with SP_TIMING_QUIT), for the pacing measurements.
func _report_timing() -> void:
	var wall := float(Time.get_ticks_usec() - _ep_t0) / 1.0e6
	print("TIMING task=%s speed=%.0f model_s=%.1f wall_s=%.1f delivered=%.1fx frames=%d step_ms_per_frame=%.1f throughput=%.1fx score=%.0f passed=%s" % [task_id, speed, t_ms * 0.001, wall, t_ms * 0.001 / maxf(wall, 0.001), _dbg_frames, _dbg_step_us / 1000.0 / maxf(_dbg_frames, 1), throughput, float(score.get("value", 0.0)), str(score.get("passed", false))])
	if OS.get_environment("SP_TIMING_QUIT") != "": Engine.get_main_loop().quit()

## The speed a level opens at, from its length and what the machine has measured it can deliver: the smallest stepper speed (1, 2, 5, 10) that finishes in about
## `want_wall_s`; when the measured throughput says a speed cannot be delivered, the speeds above it are not counted as faster. Micro-puzzles run at 1x.
static func default_speed(duration_ms: float, kind := "task", want_wall_s := 25.0) -> float:
	if kind == "micro" or duration_ms <= 12000.0: return 1.0
	var cap := 10.0 if throughput <= 0.0 else clampf(throughput * WALL_SHARE * 0.8, 1.0, 10.0)
	var best := 10.0
	for sp in [1.0, 2.0, 5.0, 10.0]:
		if duration_ms / minf(sp, cap) <= want_wall_s * 1000.0: best = sp; break
	return best

## The engine reports step failures through its error text (or `last_step_ok()` where the extension has it). A task's set_input on a channel the circuit
## does not declare is not a failure of the run: unwired sensors are simply never driven.
func _step_failed() -> bool:
	var bad := false
	if task != null: bad = not bool(task.step_ok(sim))
	elif sim.has_method("last_step_ok"): bad = not bool(sim.call("last_step_ok"))
	var e := str(sim.error())
	if e != "" and not e.begins_with("set_input"): bad = true
	if not bad: return false
	error = e if e != "" else "the simulation step failed"; failed = true; running = false
	return true

func achieved_or_speed() -> float: return minf(speed, maxf(achieved, 0.01) if speed > 1.5 else speed)

func _push_volt(w: int) -> void:
	var a: PackedFloat32Array = volt[w]; a.append(float(sim.voltage(w)))
	if a.size() > VOLT_KEEP: a = a.slice(a.size() - VOLT_KEEP / 2)
	volt[w] = a

func _collect() -> void:
	var d: PackedFloat64Array = sim.drain_spikes()
	var i := 0
	while i + 1 < d.size():
		var id := int(d[i]); var t := d[i + 1]; i += 2
		if id < 0 or id >= n: continue
		sp_n.append(id); sp_t.append(t); total_spikes += 1
		count[id] += 1; flash[id] = 1.0
		if audio_on: GameAudio.spike(pop_of[id], kinds[id] == "I")
		if first_t[id] < 0.0: first_t[id] = t
		if last_t[id] >= 0.0:
			var isi := t - last_t[id]; isi_sum[id] += isi; isi_sq[id] += isi * isi; isi_n[id] += 1
		last_t[id] = t
		for pj in out_projs[pop_of[id]]:
			var pa: PackedFloat64Array = pulses[pj]
			if pa.size() < 14: pa.append(_wall)
	var cut := t_ms - HISTORY_MS
	var j := 0
	while j < sp_t.size() and sp_t[j] < cut: j += 1
	if j > 0 and j * 2 > sp_t.size(): sp_n = sp_n.slice(j); sp_t = sp_t.slice(j)
	for pj in pulses.size():
		var pa: PackedFloat64Array = pulses[pj]
		if pa.size() > 0 and _wall - pa[0] > PULSE_MAX_S + 0.2: pulses[pj] = pa.slice(1) if pa.size() > 1 else PackedFloat64Array()

## Visual travel time (wall seconds) of a pulse along a projection with `delay_ms`.
static func pulse_seconds(delay_ms: float) -> float: return clampf(delay_ms * PULSE_S_PER_MS, PULSE_MIN_S, PULSE_MAX_S)

# ---------- statistics ----------
## Whole-run firing rate (Hz) and ISI CV of neuron i; CV is -1 with fewer than 3 spikes.
func rate_hz(i: int) -> float: return float(count[i]) / maxf(t_ms * 0.001, 0.001)
func isi_cv(i: int) -> float:
	if isi_n[i] < 2: return -1.0
	var m := isi_sum[i] / isi_n[i]; var v := isi_sq[i] / isi_n[i] - m * m
	return sqrt(maxf(v, 0.0)) / maxf(m, 1e-9)

## Spikes of the last `ms` as the (neuron, t) pair array the C++ analysis takes.
func recent_pairs(ms: float) -> PackedFloat64Array:
	var out := PackedFloat64Array(); var t0 := t_ms - ms
	for j in sp_t.size():
		if sp_t[j] >= t0: out.append(float(sp_n[j])); out.append(sp_t[j])
	return out

func sync_chi(ms := 2000.0) -> float:
	if sim == null or n < 2: return 0.0
	var t0 := maxf(t_ms - ms, 0.0)
	if t_ms - t0 < 50.0: return 0.0
	return float(SpikeSimStats.stats(recent_pairs(ms), n, t0, t_ms).get("chi", 0.0))
