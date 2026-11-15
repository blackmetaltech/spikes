class_name Telemetry
extends RefCounted
## One running experiment: a SpikeSim loaded with the editor's circuit, optionally driven by a SpikeTask, stepped in 1 ms slices from the
## frame loop under a time budget. Collects what the scopes and the circuit view draw: recent spikes, 1 ms voltage samples, flash levels,
## travelling pulses, smoothed output rates, and whole-run spike statistics.
const SLICE_MS := 1.0
static var budget_us := 9000     # at most this much wall time per frame spent stepping (SP_BUDGET_US overrides: screenshots on a software renderer)
const HISTORY_MS := 4000.0       # spikes kept for the raster, the analysis and the stats
const VOLT_KEEP := 4000
const FLASH_TAU := 0.11          # s, wall clock: how long a soma glows after a spike
const PULSE_MIN_S := 0.14        # a pulse takes delay_ms * PULSE_S_PER_MS of wall time, so a 1-5 ms synaptic delay is visible
const PULSE_S_PER_MS := 0.085
const PULSE_MAX_S := 0.55

var sim = null
var task = null
var circuit := {}
var error := ""
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
var _owed := 0.0
var _wall := 0.0

func now() -> float: return _wall

## Build everything for `c` (circuit Dictionary) and `task_id` ("" = free run). Returns false and sets `error` if the engine rejects the circuit.
func setup(c: Dictionary, task_id: String, ext_ok: bool) -> bool:
	circuit = c; error = ""; finished = false
	if not ext_ok: error = "The spike-gd GDExtension is not loaded. Run scripts/build-gd.sh."; return false
	sim = ClassDB.instantiate("SpikeSim")
	if not sim.load_circuit(Circuits.to_json(c)): error = str(sim.error()); sim = null; return false
	n = int(sim.neuron_count()); kinds = Circuits.kind_per_neuron(c)
	pop_of = PackedInt32Array(); out_projs = []
	for pi in c["populations"].size():
		out_projs.append([])
		for k in Circuits.pop_n(c["populations"][pi]): pop_of.append(pi)
	for i in c["projections"].size():
		var pre := Circuits.pop_index(c, Circuits.proj_pop(c["projections"][i].get("pre", "")))
		if pre >= 0: out_projs[pre].append(i)
	pulses = []; pulses.resize(c["projections"].size())
	for i in pulses.size(): pulses[i] = PackedFloat64Array()
	task = null
	if task_id != "":
		task = ClassDB.instantiate("SpikeTask")
		if not task.make(task_id): task = null; error = "unknown task " + task_id; return false
	watched = Circuits.default_watch(c)
	reset()
	return true

func reset(seed := -1) -> void:
	if sim == null: return
	episode += 1
	var circ := Circuits.to_json(circuit)
	sim.load_circuit(circ)   # a fresh kernel: same circuit, same seed, so an episode is repeatable
	t_ms = 0.0; view_t = 0.0; _owed = 0.0; finished = false; total_spikes = 0
	sp_n = PackedInt32Array(); sp_t = PackedFloat64Array()
	count = PackedInt32Array(); count.resize(n); first_t = PackedFloat64Array(); first_t.resize(n); last_t = PackedFloat64Array(); last_t.resize(n)
	isi_sum = PackedFloat64Array(); isi_sum.resize(n); isi_sq = PackedFloat64Array(); isi_sq.resize(n); isi_n = PackedInt32Array(); isi_n.resize(n)
	for i in n: first_t[i] = -1.0; last_t[i] = -1.0
	flash = PackedFloat32Array(); flash.resize(n)
	for p in pulses: p.clear()
	volt = {}; rates = {}
	for w in watched: sim.watch(w); volt[w] = PackedFloat32Array()
	out_mask = {}
	for ch in sim.outputs():
		var m := PackedByteArray(); m.resize(n)
		for i in ch["neurons"]: m[int(i)] = 1
		out_mask[str(ch["name"])] = m; rates[str(ch["name"])] = 0.0
	if task != null: task.reset(int(circuit.get("seed", 1)) if seed < 0 else seed); body = task.body(); score = task.score()

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
	if sim != null and (not running or finished or hold):   # paused: the glows, pulses and scopes hold the instant
		_collect(); view_t = t_ms; return
	_wall += dt
	for i in flash.size(): flash[i] = flash[i] * exp(-dt / FLASH_TAU) if flash[i] > 0.004 else 0.0
	if sim == null: return
	var stepped := 0.0
	if running and not finished:
		_owed = minf(_owed + dt * 1000.0 * speed, 200.0 * maxf(speed, 1.0) if budget_us < 100000 else 1.0e9)
		var t0 := Time.get_ticks_usec(); var k := 0
		while _owed >= SLICE_MS:
			if task != null: task.step(sim, SLICE_MS)
			else: sim.step(SLICE_MS)
			t_ms += SLICE_MS; _owed -= SLICE_MS; stepped += SLICE_MS
			for w in watched: _push_volt(w)
			if task != null and t_ms >= duration(): finished = true; break
			k += 1
			if k % 4 == 0 and Time.get_ticks_usec() - t0 > budget_us: _owed = minf(_owed, 4.0); break
		var want := dt * 1000.0 * speed
		achieved = lerpf(achieved, speed * (stepped / want) if want > 0.0 else speed, 0.1)
	_collect()
	if sim != null:
		for ch in rates.keys():
			var m: PackedByteArray = out_mask[ch]; var cnt := 0; var nn := 0
			for i in n: nn += m[i]
			var j := sp_t.size() - 1; var t0 := t_ms - RATE_WIN_MS
			while j >= 0 and sp_t[j] >= t0:
				if m[sp_n[j]] == 1: cnt += 1
				j -= 1
			var hz := float(cnt) / (RATE_WIN_MS * 0.001 * maxf(float(nn), 1.0))
			rates[ch] = lerpf(float(rates[ch]), hz, clampf(dt * 10.0, 0.0, 1.0))
	if task != null and stepped > 0.0: body = task.body(); score = task.score()
	if running and not finished: view_t = clampf(view_t + dt * 1000.0 * achieved_or_speed(), t_ms - 40.0, t_ms)
	else: view_t = t_ms

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
