class_name AnalysisScreen
extends Screen
## Analysis, from the C++ engine (SpikeSim bindings of engine/include/spike/analysis.h). Tabs:
##   3-node    motif census of the circuit (13 connected directed classes, Milo et al. 2002), z-scores against degree-preserving nulls
##   4-node    the same for the 199 connected 4-node classes (strongest |z| listed); refuses, with a message, when the work estimate is too big
##   HRV       heart-rate variability (SDNN, RMSSD, pNN50, LF/HF) of the beats of one population (spikes merged within 150 ms are one beat)
##   PRC       phase-response curve of that population to the task's scripted perturbations (task timeline events), from the recorded spikes
##   bursts    burst detection (max ISI 170 ms, min 3 spikes) on that population
##   energy    mean spikes per second per neuron against the task's energy budget (the energy star, Progress.ENERGY_HZ) and the ATP it costs
## HRV, PRC and bursts need a recording: RECORD re-runs the circuit on the task (a throwaway engine, the lab's is stopped while this screen is open)
## for 10 s, 30 s, 60 s or the full episode and keeps every spike. Beside the tabs, spike statistics of the lab run so far (rate, ISI CV, Golomb chi).
var circuit := {}
var tele: Telemetry = null
var task_id := ""
var sign_idx := 0
var level_idx := 0   # 0 = populations (one node per population), 1 = neurons (every cell, as NEST sees it)
var null_n := 100
var result := {}
var back_btn: Button
var chart: MotifChart
var panel: SeriesPanel
var stats_view: StatsView
const TABS := [[0, "3-node"], [1, "4-node"], [2, "HRV"], [3, "PRC"], [4, "bursts"], [5, "energy"], [6, "STA"]]
const RECORDS := [[10, "10 s"], [30, "30 s"], [60, "60 s"], [0, "full"]]
const BEAT_GAP_MS := 150.0
const MOTIF4_MAX_WORK := 5.0e7
var tab := 0
var pop_idx := 0          # population whose spikes HRV / PRC / bursts read
var rec_s := 30           # recording length in s (0 = the task's whole episode)
var rec := {}             # {pairs, n, t_ms, task_id}: the last recording
var _rec_run := {}        # a recording in progress
var _head_motif: Array = []
var _head_rec: Array = []
var _rec_step: Kit.Stepper = null
const STA_MAX_S := 20     # the reverse-correlation recording is at most this long (the stimulus is kept per millisecond)

func default_focus() -> Control: return back_btn

func build() -> void:
	var f := f(); var phone := Layout.phone
	if circuit.is_empty() and app != null: circuit = app.analysis_circuit; tele = app.analysis_tele; task_id = app.analysis_task
	add_child(Kit.bench())
	var margin := MarginContainer.new(); margin.set_anchors_preset(Control.PRESET_FULL_RECT)
	var mx := int(50 * f) if not phone else 16; var my := int(26 * f) if not phone else 8
	for side in ["left", "right"]: margin.add_theme_constant_override("margin_" + side, mx)
	margin.add_theme_constant_override("margin_top", my); margin.add_theme_constant_override("margin_bottom", my); add_child(margin)
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", int(12 * f)); margin.add_child(v)
	var head := HFlowContainer.new(); head.add_theme_constant_override("h_separation", 12); head.add_theme_constant_override("v_separation", 6); v.add_child(head)
	back_btn = Kit.btn("← Back", func(): app.back(), "ghost", 22 if not phone else 18); back_btn.size_flags_vertical = Control.SIZE_SHRINK_CENTER; head.add_child(back_btn)
	var ttl := Kit.display("Analysis", int(46 * f) if not phone else 28, Kit.TEXT); ttl.size_flags_horizontal = Control.SIZE_EXPAND_FILL; ttl.size_flags_vertical = Control.SIZE_SHRINK_CENTER; head.add_child(ttl)
	head.add_child(Kit.ident(Kit.stepper(TABS, str(tab), func(i): tab = int(i); _tab_changed(), 380.0 if not phone else 260.0, 20 if not phone else 18), "StepTab"))
	var mrow := HFlowContainer.new(); mrow.add_theme_constant_override("h_separation", 12); mrow.add_theme_constant_override("v_separation", 6); v.add_child(mrow)
	_head_motif.append(Kit.italic("graph", 20, Kit.TEXT_SOFT)); mrow.add_child(_head_motif[-1])
	_head_motif.append(Kit.ident(Kit.stepper([[0, "populations"], [1, "neurons"]], str(level_idx), func(i): level_idx = int(i); compute(), 170.0 if not phone else 130.0), "StepLevel")); mrow.add_child(_head_motif[-1])
	_head_motif.append(Kit.italic("edges", 20, Kit.TEXT_SOFT)); mrow.add_child(_head_motif[-1])
	_head_motif.append(Kit.ident(Kit.stepper([[0, "all"], [1, "excitatory"], [2, "inhibitory"]], str(sign_idx), func(i): sign_idx = int(i); compute(), 190.0 if not phone else 130.0), "StepSign")); mrow.add_child(_head_motif[-1])
	_head_motif.append(Kit.italic("nulls", 20, Kit.TEXT_SOFT)); mrow.add_child(_head_motif[-1])
	_head_motif.append(Kit.ident(Kit.stepper([[20, "20"], [100, "100"], [500, "500"]], str(null_n), func(i): null_n = int(i); compute(), 80.0), "StepNull")); mrow.add_child(_head_motif[-1])
	var pops: Array = circuit.get("populations", [])
	if pops.size() > 0:
		pop_idx = _default_pop()
		_head_rec.append(Kit.italic("population", 20, Kit.TEXT_SOFT)); mrow.add_child(_head_rec[-1])
		var popts := []
		for i in pops.size(): popts.append([i, str(pops[i].get("id", i))])
		_head_rec.append(Kit.ident(Kit.stepper(popts, str(pop_idx), func(i): pop_idx = int(i); compute(), 200.0 if not phone else 150.0), "StepPop")); mrow.add_child(_head_rec[-1])
	_head_rec.append(Kit.italic("record", 20, Kit.TEXT_SOFT)); mrow.add_child(_head_rec[-1])
	_head_rec.append(Kit.ident(Kit.stepper(RECORDS, str(rec_s), func(i): rec_s = int(i), 150.0 if not phone else 120.0), "StepRec")); mrow.add_child(_head_rec[-1]); _rec_step = _head_rec[-1]
	_head_rec.append(Kit.btn("RECORD", func(): _start_record(), "green", 20, Vector2(120, 0))); mrow.add_child(_head_rec[-1])
	var cols := HBoxContainer.new(); cols.size_flags_vertical = Control.SIZE_EXPAND_FILL; cols.add_theme_constant_override("separation", int(16 * f)); v.add_child(cols)
	chart = MotifChart.new(); chart.size_flags_horizontal = Control.SIZE_EXPAND_FILL; chart.size_flags_stretch_ratio = 1.5; chart.size_flags_vertical = Control.SIZE_EXPAND_FILL; cols.add_child(chart)
	panel = SeriesPanel.new(); panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL; panel.size_flags_stretch_ratio = 1.5; panel.size_flags_vertical = Control.SIZE_EXPAND_FILL; cols.add_child(panel)
	stats_view = StatsView.new(); stats_view.size_flags_horizontal = Control.SIZE_EXPAND_FILL; stats_view.size_flags_vertical = Control.SIZE_EXPAND_FILL; cols.add_child(stats_view)
	stats_view.tele = tele; stats_view.circuit = circuit
	if OS.get_environment("SP_ATAB") != "": tab = int(OS.get_environment("SP_ATAB"))   # screenshot hooks: open on a tab, and press Record
	_tab_changed()
	if OS.get_environment("SP_AREC") != "": _start_record()

func _sta_msg() -> String:
	return "Press “Record”: the first cell of the population is driven by a Gaussian white-noise current for the length chosen (at most %d s; 10 s is usually plenty), and the current that came before each of its spikes is averaged, minus the average of the whole current." % STA_MAX_S

func _tab_changed() -> void:
	chart.visible = tab <= 0; panel.visible = tab > 0
	for c in _head_motif: c.visible = tab <= 1
	for c in _head_rec: c.visible = (tab >= 2 and tab <= 4) or tab == 6
	if tab == 6 and (rec_s == 0 or rec_s > STA_MAX_S) and _rec_step != null:   # the default length is within the stated maximum: the first option, 10 s
		rec_s = int(RECORDS[0][0]); _rec_step.idx = 0; _rec_step.val.text = Kit.label_case(str(RECORDS[0][1]))
	compute()

## The population the heart / breathing tasks read first: the one behind the task's first motor output (the SA node, the preBotC...), else the first.
func _default_pop() -> int:
	var outs: Array = circuit.get("outputs", [])
	for o in outs:
		var ns: Array = (o as Dictionary).get("neurons", [])
		if ns.size() > 0:
			var pi := Circuits.pop_index(circuit, str((ns[0] as Dictionary).get("pop", "")))
			if pi >= 0: return pi
	return 0

func compute() -> void:
	if tab == 1: _compute_motif4(); return
	if tab >= 2: _compute_series(); return
	result = {}
	if circuit.is_empty() or Circuits.neuron_count(circuit) == 0: chart.message = "No circuit yet. Build one in the lab."; chart.queue_redraw(); return
	if not ClassDB.class_exists("SpikeSim"): chart.message = "The spike-gd GDExtension is not loaded."; chart.queue_redraw(); return
	var txt := ""
	if level_idx == 0:
		var g := population_graph(circuit)
		if int(g["n"]) < 3: chart.message = "Fewer than 3 populations: wire at least three to count three-node motifs."; chart.result = {}; chart.queue_redraw(); return
		var sim0 = ClassDB.instantiate("SpikeSim")
		txt = str(sim0.motif_json_edges(g["edges"], int(g["n"]), sign_idx, null_n, int(circuit.get("seed", 1))))
	else:
		# A throwaway engine just to read the connection graph from NEST (the lab's engine is stopped while this screen is open).
		var sim = ClassDB.instantiate("SpikeSim")
		if not sim.load_circuit(Circuits.to_json(circuit)): chart.message = "The engine rejected the circuit: " + str(sim.error()); chart.queue_redraw(); return
		txt = str(sim.motif_json(sign_idx, null_n, int(circuit.get("seed", 1))))
		sim.stop()
	var d := Circuits.parse(txt)
	if d.has("error"): chart.message = str(d["error"])
	else: result = d; chart.message = ""
	chart.level = level_idx; chart.result = result; chart.queue_redraw()
	if stats_view != null: stats_view.queue_redraw()


# ---------- 4-node census ----------
func _compute_motif4() -> void:
	panel.kind = "m4"; panel.data = {}; panel.message = ""
	if circuit.is_empty() or Circuits.neuron_count(circuit) == 0: panel.message = "No circuit yet. Build one in the lab."; panel.queue_redraw(); return
	if not ClassDB.class_exists("SpikeSim"): panel.message = "The spike-gd GDExtension is not loaded."; panel.queue_redraw(); return
	var txt := ""
	var seed := int(circuit.get("seed", 1))
	if level_idx == 0:
		var g := population_graph(circuit)
		if int(g["n"]) < 4: panel.message = "Fewer than 4 populations: wire at least four to count four-node motifs."; panel.queue_redraw(); return
		txt = str(ClassDB.instantiate("SpikeSim").motif4_json_edges(g["edges"], int(g["n"]), sign_idx, null_n, seed, MOTIF4_MAX_WORK))
	else:
		var sim = ClassDB.instantiate("SpikeSim")
		if not sim.load_circuit(Circuits.to_json(circuit)): panel.message = "The engine rejected the circuit: " + str(sim.error()); panel.queue_redraw(); return
		txt = str(sim.motif4_json(sign_idx, null_n, seed, MOTIF4_MAX_WORK)); sim.stop()
	var d := Circuits.parse(txt)
	if d.has("error"): panel.message = str(d["error"]) + ("\nThe work guard stopped it: use populations, fewer nulls, or 'excitatory' / 'inhibitory' edges only." if str(d["error"]).find("work") >= 0 else "")
	else: panel.data = d; panel.extra = {"level": level_idx}
	panel.queue_redraw()

# ---------- recordings: HRV, PRC, bursts ----------
func _start_record() -> void:
	if not ClassDB.class_exists("SpikeSim"): panel.message = "The spike-gd GDExtension is not loaded."; panel.queue_redraw(); return
	if circuit.is_empty() or Circuits.neuron_count(circuit) == 0: panel.message = "No circuit yet. Build one in the lab."; panel.queue_redraw(); return
	var sim = ClassDB.instantiate("SpikeSim")
	var sta := tab == 6   # v0.6 reverse correlation: a seeded Gaussian white-noise current is written into the first cell of the chosen population every ms
	var run_c: Dictionary = circuit
	if sta:
		var pops0: Array = circuit.get("populations", [])
		if pop_idx >= pops0.size() or Circuits.is_rate_model(str(pops0[pop_idx].get("model", ""))): panel.message = "Reverse correlation needs a spiking cell: choose a population of spiking cells."; panel.queue_redraw(); return
		run_c = circuit.duplicate(true)
		run_c["inputs"] = (run_c.get("inputs", []) as Array) + [{"name": "sta_noise", "kind": "dc", "targets": [{"pop": str(pops0[pop_idx].get("id", "")), "idx": [0]}], "weight": 1.0, "value": 0.0}]
	if not sim.load_circuit(Circuits.to_json(Telemetry.sanitized(run_c))): panel.message = "The engine rejected the circuit: " + str(sim.error()); panel.queue_redraw(); return
	var t = Campaign.make(task_id) if task_id != "" and not sta else null
	if t != null: t.reset(int(circuit.get("seed", 1)))
	var total := float(rec_s) * 1000.0
	if t != null and (rec_s == 0 or total > float(t.duration_ms())): total = float(t.duration_ms())
	if t == null and rec_s == 0: total = 30000.0
	_rec_run = {"sim": sim, "task": t, "t": 0.0, "total": total, "pairs": PackedFloat64Array(), "sta": sta, "stim": PackedFloat64Array(), "rng": RandomNumberGenerator.new()}
	if sta: (_rec_run["rng"] as RandomNumberGenerator).seed = int(circuit.get("seed", 1)) + 7919; _rec_run["total"] = minf(total, float(STA_MAX_S) * 1000.0)
	rec = {}; _compute_series()

func _process(_dt: float) -> void:
	if _rec_run.is_empty(): return
	var t0 := Time.get_ticks_msec()
	var sim = _rec_run["sim"]; var t = _rec_run["task"]
	while float(_rec_run["t"]) < float(_rec_run["total"]) and Time.get_ticks_msec() - t0 < 12:
		for _k in 20:
			if bool(_rec_run.get("sta", false)):
				var lv := _sta_level(); var z := float((_rec_run["rng"] as RandomNumberGenerator).randfn(0.0, 1.0)); var u: float = float(lv[0]) + float(lv[1]) * z
				sim.set_input("sta_noise", u); var sa: PackedFloat64Array = _rec_run["stim"]; sa.append(u); _rec_run["stim"] = sa
				sim.step(1.0)
			elif t != null: t.step(sim, 1.0)
			else: sim.step(1.0)
			_rec_run["t"] = float(_rec_run["t"]) + 1.0
		var pa: PackedFloat64Array = _rec_run["pairs"]; pa.append_array(sim.drain_spikes()); _rec_run["pairs"] = pa   # packed arrays are values: store the appended copy back
	if float(_rec_run["t"]) >= float(_rec_run["total"]):
		rec = {"pairs": _rec_run["pairs"], "n": int(sim.neuron_count()), "t_ms": float(_rec_run["t"]), "task_id": task_id, "stim": _rec_run.get("stim", PackedFloat64Array()), "sta": bool(_rec_run.get("sta", false)), "sta_pop": pop_idx}
		sim.stop(); _rec_run = {}
	if (tab >= 2 and tab <= 4) or tab == 6: _compute_series()

## Mean and standard deviation (pA, or the model's own current unit) of the white-noise current for the cell of the chosen population: about at its threshold, so that it fires irregularly.
func _sta_level() -> Array:
	var pops: Array = circuit.get("populations", []); var model := str(pops[pop_idx].get("model", "")) if pop_idx < pops.size() else ""
	var tab_ := {"izhikevich": [3.0, 2.5], "hh_psc_alpha": [0.0, 300.0], "aeif_cond_alpha": [330.0, 150.0], "aeif_cond_exp": [330.0, 150.0], "iaf_cond_exp": [280.0, 120.0]}
	return tab_.get(model, [330.0, 120.0])

## Neuron ids of the chosen population.
func _pop_neurons() -> PackedInt32Array:
	var out := PackedInt32Array()
	var pops: Array = circuit.get("populations", [])
	if pop_idx < 0 or pop_idx >= pops.size(): return out
	var first := Circuits.pop_first(circuit, pop_idx)
	for k in Circuits.pop_n(pops[pop_idx]): out.append(first + k)
	return out

## Beat times: spike times of the neurons, a spike within BEAT_GAP_MS of the previous one belongs to the same beat.
static func beat_times(pairs: PackedFloat64Array, neurons: PackedInt32Array, gap_ms: float) -> PackedFloat64Array:
	var want := {}
	for n in neurons: want[int(n)] = true
	var ts := PackedFloat64Array()
	var i := 0
	while i + 1 < pairs.size():
		if want.has(int(pairs[i])): ts.append(pairs[i + 1])
		i += 2
	ts.sort()
	var out := PackedFloat64Array(); var last := -1.0e18
	for t in ts:
		if t - last > gap_ms: out.append(t)
		last = t
	return out

func _compute_series() -> void:
	panel.kind = ["", "", "hrv", "prc", "bursts", "energy", "sta"][tab]; panel.data = {}; panel.message = ""; panel.extra = {}
	if tab == 5: _compute_energy(); return
	if not _rec_run.is_empty():
		panel.message = "Recording... %.0f of %.0f s" % [float(_rec_run["t"]) * 0.001, float(_rec_run["total"]) * 0.001]; panel.queue_redraw(); return
	if rec.is_empty() and tab == 6: panel.message = _sta_msg(); panel.queue_redraw(); return
	if rec.is_empty():
		panel.message = "Press “Record”: the circuit is run on the task for the length chosen and every spike is kept, then analysed here."
		panel.queue_redraw(); return
	if tab == 6:
		if not bool(rec.get("sta", false)): panel.message = _sta_msg(); panel.queue_redraw(); return
		var pr0: PackedFloat64Array = rec["pairs"]; var first := Circuits.pop_first(circuit, int(rec["sta_pop"])); var st := PackedFloat64Array()
		for j in range(0, pr0.size() - 1, 2):
			if int(pr0[j]) == first: st.append(pr0[j + 1])
		var d0 := Circuits.parse(str(SpikeSim.sta_json(rec["stim"], 1.0, st, 80.0, 10.0)))
		panel.extra = {"sd": _sta_level()[1], "t_ms": float(rec["t_ms"]), "spikes": st.size()}
		if d0.has("error"): panel.message = str(d0["error"])
		else: panel.data = d0
		panel.queue_redraw(); return
	var pairs: PackedFloat64Array = rec["pairs"]; var n := int(rec["n"]); var t_ms := float(rec["t_ms"]); var ns := _pop_neurons()
	var pops: Array = circuit.get("populations", [])
	panel.extra = {"pop": str(pops[pop_idx].get("id", "")) if pop_idx < pops.size() else "", "t_ms": t_ms}
	var txt := ""
	match tab:
		2:
			var beats := beat_times(pairs, ns, BEAT_GAP_MS)
			txt = str(SpikeSim.hrv_json(beats)); panel.extra["beats"] = beats
		3:
			var perturb := PackedFloat64Array()
			for e in Campaign.info(task_id).get("timeline", []): perturb.append(float((e as Dictionary).get("t_ms", 0.0)))
			panel.extra["perturb"] = perturb
			if perturb.is_empty():
				panel.message = "This task has no scripted perturbation to measure a phase response to. A PRC needs pulses at known times (tasks with a timeline: baroreflex, heart autonomic, pupil...)."; panel.queue_redraw(); return
			txt = str(SpikeSim.prc_json(pairs, n, ns, perturb, 0.0, BEAT_GAP_MS, 10))
		4:
			txt = str(SpikeSim.bursts_json(pairs, n, ns, 0.0, t_ms))
	var d := Circuits.parse(txt)
	if d.has("error"): panel.message = str(d["error"])
	else: panel.data = d
	panel.queue_redraw()

func _compute_energy() -> void:
	var spikes := 0; var n := 0; var t_ms := 0.0; var src := ""
	var per_pop := []
	if tele != null and tele.n > 0 and tele.t_ms >= 200.0:
		spikes = tele.total_spikes; n = tele.n; t_ms = tele.t_ms; src = "the lab run so far"
		var pops: Array = circuit.get("populations", [])
		for pi in pops.size():
			var first := Circuits.pop_first(circuit, pi); var k := Circuits.pop_n(pops[pi]); var c := 0
			for j in k: c += int(tele.count[first + j])
			per_pop.append([str(pops[pi].get("id", "")), Progress.energy_hz(c, k, t_ms)])
	elif not rec.is_empty():
		var pairs: PackedFloat64Array = rec["pairs"]; spikes = pairs.size() / 2; n = int(rec["n"]); t_ms = float(rec["t_ms"]); src = "the last recording"
	if n == 0 or t_ms < 200.0:
		panel.message = "Run the circuit for a few seconds in the lab (or press “Record” in another tab), then come back: the energy bill is measured from the spikes it produced."; panel.queue_redraw(); return
	var hz := Progress.energy_hz(spikes, n, t_ms)
	panel.data = {"hz": hz, "budget": Progress.energy_budget(task_id), "neurons": n, "par": Progress.budget(task_id), "t_ms": t_ms, "spikes": spikes,
		"has_task": task_id != "", "per_pop": per_pop, "src": src, "gap": Progress.star3_gap(task_id, n, hz)}
	panel.queue_redraw()

## Population-level graph: node = population (index in circuit.populations), edge = at least one projection between two different
## populations; the edge weight carries the projection's sign. Returns {n, edges: PackedFloat64Array of (pre, post, weight) triples}.
static func population_graph(c: Dictionary) -> Dictionary:
	var tr := PackedFloat64Array()
	for pr in c.get("projections", []):
		var a := Circuits.pop_index(c, Circuits.proj_pop(pr.get("pre", "")))
		var b := Circuits.pop_index(c, Circuits.proj_pop(pr.get("post", "")))
		if a < 0 or b < 0 or a == b: continue
		tr.append(float(a)); tr.append(float(b)); tr.append(Circuits.proj_weight(pr))
	return {"n": c.get("populations", []).size(), "edges": tr}

## 13 motif classes as a diverging z-score chart with their glyphs.
class MotifChart extends Control:
	var result := {}
	var message := ""
	var level := 0
	func _ready() -> void: clip_contents = true; custom_minimum_size = Vector2(380, 300)
	func min_text_px() -> float: return float(Kit.fs(14)) * Kit.ppl
	func _draw() -> void:
		var fz := Kit.fs(16 if size.x > 600.0 else 14); var f := Fonts.regular; var fb := Fonts.bold   # the table reads at 16 px (it was 11-14)
		Kit.draw_plate(self, Rect2(Vector2.ZERO, size - Vector2(Kit.SHADOW, Kit.SHADOW)))
		Kit.draw_fig(self, Vector2(18, 32), 1, "Three-node motifs" + (": count, and z-score against degree-preserving rewiring" if size.x > 900.0 else ""), size.x - 44, "", 19)
		if message != "" or not result.has("motifs"):
			draw_string(f, Vector2(0, size.y * 0.5), message if message != "" else "computing...", HORIZONTAL_ALIGNMENT_CENTER, size.x, Kit.fs(20), Kit.TEXT_SOFT); return
		var ms: Array = result["motifs"]; var top := 46.0 + float(fz) + 6.0   # a header row for the axis label, below the caption
		var degenerate := bool(result.get("degenerate", false)); var acc := float(result.get("swap_acceptance", 0.0))
		var foot_txt := _footer(degenerate, acc)
		var fw := size.x - 32.0; var foot_h := f.get_multiline_string_size(foot_txt, HORIZONTAL_ALIGNMENT_LEFT, fw, fz).y + 8.0
		var rh := (size.y - top - foot_h - 6.0) / 13.0
		var gx := 16.0; var gw := minf(rh * 1.5, 56.0)
		var name_w := float(fz) * 12.5; var cnt_w := float(fz) * 3.4
		var bx := gx + gw + name_w + cnt_w + 14.0; var bw := size.x - bx - 16.0
		var zmax := 8.0
		var mid := bx + bw * 0.5
		draw_line(Vector2(mid, top), Vector2(mid, top + rh * 13.0), Color(Kit.LINE, 1.0), 1.5)
		for zz in [-4, -2, 2, 4]:
			var x := mid + bw * 0.5 * float(zz) / zmax; draw_line(Vector2(x, top), Vector2(x, top + rh * 13.0), Color(Kit.LINE, 0.4), 1.0)
		draw_string(Gfx.serif_italic(), Vector2(mid - 40, top - 6), "z = 0", HORIZONTAL_ALIGNMENT_CENTER, 80, fz, Kit.TEXT_SOFT)
		var maxc := 1.0
		for m in ms: maxc = maxf(maxc, float(m["count"]))
		for i in ms.size():
			var m: Dictionary = ms[i]; var y := top + rh * i; var cy := y + rh * 0.5
			if i % 2 == 0: draw_rect(Rect2(8, y, size.x - 24, rh), Color(Kit.LINE, 0.22))
			_glyph(Vector2(gx + gw * 0.5, cy), minf(rh * 0.42, 14.0), int(m["milo_id"]))
			draw_string(f, Vector2(gx + gw + 8, cy + float(fz) * 0.35), _pretty(str(m["name"])), HORIZONTAL_ALIGNMENT_LEFT, name_w, fz, Kit.TEXT)
			var cnt := int(m["count"]); draw_string(fb, Vector2(gx + gw + name_w + 8, cy + float(fz) * 0.35), str(cnt), HORIZONTAL_ALIGNMENT_RIGHT, cnt_w, fz, Kit.TEXT if cnt > 0 else Kit.TEXT_SOFT)
			var zv = m.get("z", 0.0); var defined := bool(m.get("z_defined", zv != null)) and zv != null
			var sig := bool(m.get("significant", false))
			if not defined:   # sd = 0: the count is fixed by the degree sequence, so z is undefined (not infinite): no bar, greyed
				draw_string(f, Vector2(mid - 60.0, cy + float(fz) * 0.35), "n/a (fixed by degrees)" if bw > 300.0 else "n/a", HORIZONTAL_ALIGNMENT_CENTER, 120.0 if bw > 300.0 else 60.0, fz, Color(Kit.TEXT_SOFT, 0.6))
				continue
			var z := clampf(float(zv), -zmax, zmax); var x1 := mid + bw * 0.5 * z / zmax; var col := Kit.exc() if z > 0.0 else Color("#4F5C66")   # under-represented: slate (blue means inhibition)
			if absf(z) > 0.02:
				Gfx.glow(self, Vector2(x1, cy), rh * 0.9, Color(col, 0.12 if sig else 0.0))
				draw_rect(Rect2(minf(mid, x1), cy - rh * 0.27, absf(x1 - mid), rh * 0.54), Color(col, 0.95 if sig else 0.4))
			var zs := ("%+.1f" % float(zv)) if cnt > 0 or absf(z) > 0.0 else "-"
			zs += "*" if sig else ""
			if absf(x1 - mid) > 70.0:   # a long bar carries its label inside, at the far end
				draw_string(fb, Vector2(x1 - 74.0 if z > 0.0 else x1 + 6.0, cy + float(fz) * 0.35), zs, HORIZONTAL_ALIGNMENT_RIGHT if z > 0.0 else HORIZONTAL_ALIGNMENT_LEFT, 68, fz, Kit.BENCH)
			else:
				draw_string(f, Vector2(x1 + (6.0 if z >= 0 else -6.0 - 48.0), cy + float(fz) * 0.35), zs, HORIZONTAL_ALIGNMENT_LEFT if z >= 0 else HORIZONTAL_ALIGNMENT_RIGHT, 48, fz, Kit.TEXT_SOFT)
		draw_multiline_string(f, Vector2(16, top + rh * 13.0 + float(fz) + 4.0), foot_txt, HORIZONTAL_ALIGNMENT_LEFT, fw, fz, -1, Kit.RED if degenerate else Kit.TEXT_SOFT)

	func _footer(degenerate: bool, acc: float) -> String:
		var g := "populations" if level == 0 else "neurons"
		if size.x < 600.0:   # phone: one short line (plus a short warning), the full legend does not fit
			var c := "%s: %d nodes, %d edges, swaps %.0f%%  * = |z|>2  n/a = fixed" % [g, int(result.get("nodes", 0)), int(result.get("edges", 0)), acc * 100.0]
			return c + ("\nWARNING: null too weak (<20%% swaps), no stars" if degenerate else "")
		var t := "%s graph: %d nodes, %d edges. Nulls: %d, swaps accepted %.0f%% (about %.1f per edge)." % [g, int(result.get("nodes", 0)), int(result.get("edges", 0)), int(result.get("null_samples", 0)), acc * 100.0, float(result.get("effective_swaps_per_edge", 0.0))]
		if degenerate: t += "\nWARNING: under 20%% of swaps were possible, so this graph is too small or dense to randomise and the nulls barely differ from the data: z-scores are not trustworthy, nothing is starred."
		t += "\n* = |z| > 2 and null sd >= 0.5.  n/a = count fixed by the degree sequence.  'all' edges ignores sign (Milo-style nulls that keep mutual pairs are planned)."
		return t

	static func _pretty(n: String) -> String: return n.replace("_", " ")

	## The motif's 3-node diagram from its Milo id (a 9-bit row-major adjacency matrix, MSB = a00).
	func _glyph(c: Vector2, r: float, milo: int) -> void:
		var pos := [c + Vector2(0, -r), c + Vector2(-r * 0.95, r * 0.7), c + Vector2(r * 0.95, r * 0.7)]
		for i in 3:
			for j in 3:
				if i != j and (milo >> (8 - (3 * i + j))) & 1 == 1:
					var a: Vector2 = pos[i]; var b: Vector2 = pos[j]; var d := (b - a).normalized(); var n := d.orthogonal() * (r * 0.16 if (milo >> (8 - (3 * j + i))) & 1 == 1 else 0.0)
					var a2 := a + d * r * 0.28 + n; var b2 := b - d * r * 0.28 + n
					draw_line(a2, b2, Color(Kit.TEXT_SOFT, 0.95), 1.3, true); Gfx.arrow_head(self, b2, d, Kit.TEXT_SOFT, r * 0.34)
		for p in pos: draw_circle(p, r * 0.22, Kit.TEXT)

## One panel for the 4-node table, HRV, PRC, bursts and energy: draws `data` (the engine's JSON, parsed) by `kind`.
class SeriesPanel extends Control:
	var kind := ""
	var data := {}
	var extra := {}
	var message := ""
	func _ready() -> void: clip_contents = true; custom_minimum_size = Vector2(380, 300)
	func min_text_px() -> float: return float(Kit.fs(14)) * Kit.ppl
	func _title() -> String:
		return {"m4": "Four-node motifs", "hrv": "Heart-rate variability", "prc": "Phase-response curve", "bursts": "Bursts", "energy": "Energy", "sta": "Spike-triggered average"}.get(kind, "")
	func _num(v: Variant, fmt := "%.1f") -> String: return "-" if v == null else fmt % float(v)
	func _draw() -> void:
		var fz := Kit.fs(16 if size.x > 600.0 else 14); var f := Fonts.regular; var fb := Fonts.bold   # tables at 16 px on a desktop (they were 11-14)
		Kit.draw_plate(self, Rect2(Vector2.ZERO, size - Vector2(Kit.SHADOW, Kit.SHADOW)))
		Kit.draw_fig(self, Vector2(18, 32), 1, _title(), size.x - 44, "", 19)
		if message != "" or data.is_empty():
			draw_multiline_string(f, Vector2(16, 64), message if message != "" else "computing...", HORIZONTAL_ALIGNMENT_LEFT, size.x - 32, Kit.fs(18), -1, Kit.TEXT_SOFT); return
		match kind:
			"m4": _draw_m4(f, fb, fz)
			"hrv": _draw_hrv(f, fb, fz)
			"prc": _draw_prc(f, fb, fz)
			"bursts": _draw_bursts(f, fb, fz)
			"energy": _draw_energy(f, fb, fz)
			"sta": _draw_sta(f, fb, fz)
	func _rows(rows: Array, y0: float, f: Font, fb: Font) -> float:
		var y := y0
		for kv in rows:
			draw_string(f, Vector2(16, y), str(kv[0]), HORIZONTAL_ALIGNMENT_LEFT, size.x * 0.62, Kit.fs(17), Kit.TEXT_SOFT)
			draw_string(Fonts.mono_med, Vector2(size.x - 176, y), str(kv[1]), HORIZONTAL_ALIGNMENT_RIGHT, 160, Kit.fs(17), Kit.TEXT); y += 28.0
		return y
	func _note(txt: String, y: float, f: Font, fz: int, col := Kit.TEXT_SOFT) -> void:
		draw_multiline_string(f, Vector2(16, y), txt, HORIZONTAL_ALIGNMENT_LEFT, size.x - 32, fz, -1, col)
	func _draw_m4(f: Font, fb: Font, fz: int) -> void:
		var cl: Array = (data.get("classes", []) as Array).filter(func(c): return bool(c.get("z_defined", false)) and int(c.get("count", 0)) > 0)
		cl.sort_custom(func(a, b): return absf(float(a["z"])) > absf(float(b["z"])))
		var g := "populations" if int(extra.get("level", 0)) == 0 else "neurons"
		draw_string(f, Vector2(16, 52), "%s graph: %d nodes, %d edges, %d nulls, swaps accepted %.0f%%. %d of 199 classes present; the ten strongest:" % [g, int(data.get("nodes", 0)), int(data.get("edges", 0)), int(data.get("null_samples", 0)), float(data.get("swap_acceptance", 0.0)) * 100.0, (data.get("classes", []) as Array).size()], HORIZONTAL_ALIGNMENT_LEFT, size.x - 32, fz, Kit.TEXT_SOFT)
		var y := 84.0; var cols := [16.0, size.x * 0.22, size.x * 0.40, size.x * 0.58, size.x * 0.76]
		for i in 5: draw_string(f, Vector2(cols[i], y), ["class", "edges / mutual", "count", "null mean", "z"][i], HORIZONTAL_ALIGNMENT_LEFT, size.x * 0.2, fz, Kit.TEXT_SOFT)
		y += 8.0
		for c in cl.slice(0, 10):
			y += 26.0; var sig := bool(c.get("significant", false))
			var z := float(c["z"]); var col: Color = (Kit.exc() if z > 0.0 else Color("#4F5C66")) if sig else Kit.TEXT
			draw_string(f, Vector2(cols[0], y), "#%d (id %d)" % [int(c["index"]), int(c["id"])], HORIZONTAL_ALIGNMENT_LEFT, size.x * 0.2, fz, Kit.TEXT)
			draw_string(f, Vector2(cols[1], y), "%d / %d" % [int(c["edges"]), int(c["mutual_pairs"])], HORIZONTAL_ALIGNMENT_LEFT, size.x * 0.17, fz, Kit.TEXT)
			draw_string(fb, Vector2(cols[2], y), str(int(c["count"])), HORIZONTAL_ALIGNMENT_LEFT, size.x * 0.17, fz, Kit.TEXT)
			draw_string(f, Vector2(cols[3], y), _num(c.get("null_mean"), "%.1f"), HORIZONTAL_ALIGNMENT_LEFT, size.x * 0.17, fz, Kit.TEXT_SOFT)
			draw_string(fb, Vector2(cols[4], y), ("%+.1f" % z) + ("*" if sig else ""), HORIZONTAL_ALIGNMENT_LEFT, size.x * 0.2, fz, col)
		if cl.is_empty(): draw_string(f, Vector2(16, y + 30.0), "No four-node class has a defined z-score here (counts fixed by the degrees).", HORIZONTAL_ALIGNMENT_LEFT, size.x - 32, fz, Kit.TEXT_SOFT)
		_note("* = |z| > 2 and null sd >= 0.5.  Class numbers follow the engine's 199 connected directed 4-node classes (analysis.h).%s" % ("\nWARNING: under 20% of swaps were possible; the nulls barely differ from the data." if bool(data.get("degenerate", false)) else ""), size.y - 40.0, f, fz)
	func _draw_hrv(f: Font, fb: Font, fz: int) -> void:
		var d := data
		var y := _rows([["population", str(extra.get("pop", ""))], ["beats / RR intervals", "%d / %d" % [int(d.get("n_beats", 0)), int(d.get("n_rr", 0))]],
			["mean heart rate", _num(d.get("mean_hr_bpm")) + " /min"], ["mean RR", _num(d.get("mean_rr_ms")) + " ms"], ["SDNN", _num(d.get("sdnn_ms")) + " ms"],
			["RMSSD", _num(d.get("rmssd_ms")) + " ms"], ["pNN50", _num(d.get("pnn50_pct")) + " %"],
			["LF power (0.04-0.15 Hz)", _num(d.get("lf_ms2")) + " ms2"], ["HF power (0.15-0.4 Hz)", _num(d.get("hf_ms2")) + " ms2"], ["LF / HF", _num(d.get("lf_hf"), "%.2f")]], 60.0, f, fb)
		var beats: PackedFloat64Array = extra.get("beats", PackedFloat64Array())
		if int(d.get("n_beats", 0)) < 3:
			_note("Fewer than three beats in %.0f s: record longer, or pick the population that makes the beat." % (float(extra.get("t_ms", 0.0)) * 0.001), y + 6.0, f, fz, Kit.RED); return
		if not bool(d.get("spectrum_ok", false)): _note("Fewer than 9 beats: no spectrum (LF, HF). Record longer.", y, f, fz, Kit.TEXT_SOFT); y += 28.0
		# tachogram: RR interval against beat number
		var r := Rect2(16, y + 24.0, size.x - 32.0, maxf(size.y - y - 70.0, 40.0)); Kit.draw_graph_paper(self, r, 24.0); draw_rect(r, Kit.TEXT, false, 1.5)
		draw_string(f, Vector2(16, y + 16.0), "RR interval per beat (ms)", HORIZONTAL_ALIGNMENT_LEFT, size.x - 32, fz, Kit.TEXT_SOFT)
		var rr := PackedFloat64Array()
		for i in range(1, beats.size()): rr.append(beats[i] - beats[i - 1])
		var lo := 1.0e18; var hi := -1.0e18
		for v in rr: lo = minf(lo, v); hi = maxf(hi, v)
		if rr.size() > 1:
			hi = maxf(hi, lo + 20.0)
			var pts := PackedVector2Array()
			for i in rr.size(): pts.append(Vector2(r.position.x + r.size.x * float(i) / float(rr.size() - 1), r.end.y - 4.0 - (r.size.y - 8.0) * (rr[i] - lo) / (hi - lo)))
			draw_polyline(pts, Kit.exc(), 1.8, true)
			draw_string(f, Vector2(r.position.x + 6, r.position.y + float(fz) + 2.0), "%.0f" % hi, HORIZONTAL_ALIGNMENT_LEFT, 80, fz, Kit.TEXT_SOFT)
			draw_string(f, Vector2(r.position.x + 6, r.end.y - 6.0), "%.0f" % lo, HORIZONTAL_ALIGNMENT_LEFT, 80, fz, Kit.TEXT_SOFT)
		_note("Beats = spikes of the population, those within 150 ms count once. Time domain after the Task Force (1996); LF/HF by Lomb-Scargle.", size.y - 18.0 - float(fz), f, fz)
	func _draw_prc(f: Font, fb: Font, fz: int) -> void:
		var d := data; var r := Rect2(56, 96, size.x - 80.0, maxf(size.y - 190.0, 80.0))
		draw_string(f, Vector2(16, 54), "%d perturbations kept of %d (%d fell in unclean cycles), period %s ms, %d clean cycles" % [int(d.get("n_perturbations", 0)), int(d.get("n_perturbations", 0)) + int(d.get("n_rejected", 0)), int(d.get("n_rejected", 0)), _num(d.get("period_ms"), "%.0f"), int(d.get("n_clean_cycles", 0))], HORIZONTAL_ALIGNMENT_LEFT, size.x - 32, fz, Kit.TEXT_SOFT)
		var pts: Array = d.get("points", [])
		if pts.is_empty(): draw_string(f, Vector2(16, 90), "No perturbation fell into a clean cycle: the beat is irregular, or the pulses are too few or too close together.", HORIZONTAL_ALIGNMENT_LEFT, size.x - 32, fz, Kit.RED); return
		var smax := 0.1
		for p in pts: smax = maxf(smax, absf(float(p["shift"])))
		smax = ceilf(smax * 10.0) / 10.0
		Kit.draw_graph_paper(self, r, 24.0); draw_rect(r, Kit.TEXT, false, 1.5)
		var y0 := r.position.y + r.size.y * 0.5; draw_line(Vector2(r.position.x, y0), Vector2(r.end.x, y0), Color(Kit.LINE, 0.9), 1.0)
		draw_string(f, Vector2(8, r.position.y + float(fz)), "+%.1f" % smax, HORIZONTAL_ALIGNMENT_LEFT, 44, fz, Kit.TEXT_SOFT); draw_string(f, Vector2(8, y0 + 5.0), "0", HORIZONTAL_ALIGNMENT_LEFT, 44, fz, Kit.TEXT_SOFT); draw_string(f, Vector2(8, r.end.y), "-%.1f" % smax, HORIZONTAL_ALIGNMENT_LEFT, 44, fz, Kit.TEXT_SOFT)
		for p in pts:
			var x := r.position.x + r.size.x * float(p["phase"]); var yy := y0 - r.size.y * 0.5 * float(p["shift"]) / smax
			draw_circle(Vector2(x, yy), 3.5, Color(Kit.inh(), 0.7))
		var line := PackedVector2Array()
		for b in d.get("bins", []):
			if b.get("mean_shift") != null and int(b.get("count", 0)) > 0: line.append(Vector2(r.position.x + r.size.x * float(b["phase"]), y0 - r.size.y * 0.5 * float(b["mean_shift"]) / smax))
		if line.size() > 1: draw_polyline(line, Kit.exc(), 2.4, true)
		draw_string(f, Vector2(r.position.x, r.end.y + float(fz) + 6.0), "phase of the perturbation in the cycle (0 = beat, 1 = next beat)", HORIZONTAL_ALIGNMENT_LEFT, r.size.x, fz, Kit.TEXT_SOFT)
		_note("shift (cycles): positive = the next beat came early (phase advance). Dots: single perturbations; line: mean per phase bin.", r.end.y + float(fz) * 2.0 + 14.0, f, fz)
	func _draw_bursts(f: Font, fb: Font, fz: int) -> void:
		var d := data; var s: Dictionary = d.get("summary", {})
		var y := _rows([["population", str(extra.get("pop", ""))], ["bursts", str(int(d.get("n_bursts", 0)))], ["spikes in bursts", "%d of %d (%s %%)" % [int(s.get("spikes_in_bursts", 0)), int(s.get("n_spikes", 0)), _num(float(s.get("fraction_in_bursts", 0.0)) * 100.0, "%.0f")]],
			["mean burst duration", _num(s.get("mean_duration_ms"), "%.0f") + " ms"], ["spikes per burst", _num(s.get("mean_spikes_per_burst"))], ["burst period", _num(s.get("mean_period_ms"), "%.0f") + " ms"],
			["intra-burst ISI", _num(s.get("mean_intra_burst_isi_ms")) + " ms"]], 60.0, f, fb)
		var r := Rect2(16, y + 24.0, size.x - 32.0, 50.0); var tt := maxf(float(extra.get("t_ms", 1.0)), 1.0)
		draw_string(f, Vector2(16, y + 16.0), "bursts over the recording", HORIZONTAL_ALIGNMENT_LEFT, size.x - 32, fz, Kit.TEXT_SOFT)
		Kit.draw_graph_paper(self, r, 24.0); draw_rect(r, Kit.TEXT, false, 1.5)
		for b in d.get("bursts", []):
			var x0 := r.position.x + r.size.x * float(b["onset_ms"]) / tt; var x1 := r.position.x + r.size.x * float(b["offset_ms"]) / tt
			draw_rect(Rect2(x0, r.position.y + 6.0, maxf(x1 - x0, 2.0), r.size.y - 12.0), Color(Kit.exc(), 0.85))
		_note("A burst starts at an ISI under 170 ms, ends at one over 170 ms, has at least 3 spikes and 10 ms, and is 200 ms or more from the last one.", r.end.y + 20.0, f, fz)
	## Reverse correlation (Chichilnisky 2001): the mean of the white-noise stimulus before each spike. For a cell that is linear before its threshold it is its filter, up to a scale.
	func _draw_sta(f: Font, fb: Font, fz: int) -> void:
		var d := data; var m: Array = d.get("mean", []); var n := int(d.get("n", 0))
		if m.is_empty() or n < 5: draw_string(f, Vector2(16, 70), "Too few spikes (%d): the cell fired hardly at all under this noise." % n, HORIZONTAL_ALIGNMENT_LEFT, size.x - 32, fz, Kit.TEXT_SOFT); return
		var sd := float(extra.get("sd", 1.0)); var P := Rect2(100, 84, size.x - 124.0, size.y - 184.0)
		draw_rect(P, Color(Gfx.SHEET, 0.9)); draw_rect(P, Kit.INK, false, 1.2)
		var se := 2.0 * float(d.get("stim_sd", sd)) / sqrt(float(n))   # what an average of n spikes wanders by chance (two standard errors): a lobe inside it is not a filter
		var lo := 0.0; var hi := 0.0
		for v in m: lo = minf(lo, float(v)); hi = maxf(hi, float(v))
		lo = minf(lo, -maxf(0.15 * sd, 1.5 * se)); hi = maxf(hi, maxf(0.15 * sd, 1.5 * se))
		var rng := maxf(hi - lo, 1e-9)
		var yz := P.end.y - (0.0 - lo) / rng * P.size.y
		draw_rect(Rect2(P.position.x, yz - se / rng * P.size.y, P.size.x, 2.0 * se / rng * P.size.y), Color(Kit.INK, 0.07))
		draw_line(Vector2(P.position.x, yz), Vector2(P.end.x, yz), Color(Kit.INK, 0.6), 1.2)
		var unit := "pA"
		var step := pow(10.0, floorf(log(rng / 3.0) / log(10.0))); var mult := 1.0
		for cand in [1.0, 2.0, 5.0, 10.0]:
			if rng / (step * cand) <= 6.0: mult = cand; break
		step *= mult
		var tv := ceilf(lo / step) * step
		while tv <= hi + 1e-9:   # y ticks in the stimulus' own unit, with the zero line labelled
			var yy := P.end.y - (tv - lo) / rng * P.size.y
			draw_line(Vector2(P.position.x - 5.0, yy), Vector2(P.position.x, yy), Kit.INK, 1.0)
			draw_string(f, Vector2(P.position.x - 92.0, yy + float(fz) * 0.35), ("%d" % int(roundf(tv))) if step >= 1.0 else ("%.1f" % tv), HORIZONTAL_ALIGNMENT_RIGHT, 82.0, fz, Kit.TEXT if absf(tv) < 1e-9 else Kit.TEXT_SOFT)
			tv += step
		draw_string(f, Vector2(P.position.x - 92.0, P.position.y - 8.0), "stimulus minus its mean (%s)" % unit, HORIZONTAL_ALIGNMENT_LEFT, 360.0, fz, Kit.TEXT_SOFT)
		var before := float(d.get("before_ms", 80.0)); var span := float(m.size() - 1); var xs := P.position.x + P.size.x * before / span; draw_line(Vector2(xs, P.position.y), Vector2(xs, P.end.y), Color(Kit.AMBER, 0.8), 1.4)
		for ms in range(-int(before), 11, 20):   # x ticks: milliseconds before (-) or after (+) the spike
			var xx := P.position.x + P.size.x * (before + float(ms)) / span
			draw_line(Vector2(xx, P.end.y), Vector2(xx, P.end.y + 5.0), Kit.INK, 1.0)
			draw_string(f, Vector2(xx - 24.0, P.end.y + float(fz) + 6.0), "%d" % ms, HORIZONTAL_ALIGNMENT_CENTER, 48.0, fz, Kit.TEXT_SOFT)
		draw_string(f, Vector2(P.position.x, P.end.y + float(fz) * 2.0 + 12.0), "ms before the spike (0: the spike). Grey band: what chance alone gives with this many spikes.", HORIZONTAL_ALIGNMENT_LEFT, P.size.x, fz, Kit.TEXT_SOFT)
		var q := PackedVector2Array()
		for k in m.size(): q.append(Vector2(P.position.x + P.size.x * float(k) / span, P.end.y - (float(m[k]) - lo) / rng * P.size.y))
		draw_polyline(q, Kit.exc(), 2.4, true)
		draw_string(f, Vector2(16, 56), "mean stimulus before a spike, from %d spikes in %.0f s (noise sd %s)" % [n, float(extra.get("t_ms", 0.0)) * 0.001, String.num(sd, 2)], HORIZONTAL_ALIGNMENT_LEFT, size.x - 32, fz, Kit.TEXT_SOFT)
		_note("The average of a Gaussian white-noise stimulus around the spikes is the cell's linear filter (Chichilnisky 2001): a bump just before the spike means the cell integrates the recent past.", size.y - 18.0 - float(fz), f, fz)

	func _draw_energy(f: Font, fb: Font, fz: int) -> void:
		var d := data; var hz := float(d["hz"]); var bud := float(d["budget"]); var ok := hz <= bud
		var atp := Telemetry.atp_text(hz * float(d["neurons"]))
		var y := _rows([["measured over", "%.0f s, %s" % [float(d["t_ms"]) * 0.001, str(d.get("src", ""))]], ["neurons / spikes", "%d / %d" % [int(d["neurons"]), int(d["spikes"])]],
			["mean rate per neuron", "%.1f spikes/s" % hz], ["energy budget" if bool(d["has_task"]) else "reference budget", "%.0f spikes/s" % bud], ["ATP (network)", atp]], 60.0, f, fb)
		var r := Rect2(16, y + 6.0, size.x - 32.0, 22.0); var mx := maxf(hz, bud) * 1.15
		draw_rect(r, Color(Kit.LINE, 0.5)); draw_rect(Rect2(r.position, Vector2(r.size.x * clampf(hz / mx, 0.0, 1.0), r.size.y)), Color(Kit.exc() if ok else Kit.RED, 0.9))
		var bx := r.position.x + r.size.x * bud / mx; draw_line(Vector2(bx, r.position.y - 4.0), Vector2(bx, r.end.y + 4.0), Kit.TEXT, 2.0)
		y = r.end.y + 34.0
		if bool(d["has_task"]):
			var gap := str(d.get("gap", ""))
			draw_string(fb, Vector2(16, y), "Energy: within budget." if ok else "Energy: over budget, no third star.", HORIZONTAL_ALIGNMENT_LEFT, size.x - 32, Kit.fs(17), Kit.exc() if ok else Kit.RED); y += 28.0
			if gap != "": draw_string(f, Vector2(16, y), "Third star blocked by: " + gap, HORIZONTAL_ALIGNMENT_LEFT, size.x - 32, fz, Kit.TEXT_SOFT); y += 24.0
		var pp: Array = d.get("per_pop", [])
		if pp.size() > 0 and y < size.y - 70.0:
			y += 6.0; draw_string(f, Vector2(16, y), "per population (spikes/s per neuron)", HORIZONTAL_ALIGNMENT_LEFT, size.x - 32, fz, Kit.TEXT_SOFT)
			var pmx := 1.0
			for q in pp: pmx = maxf(pmx, float(q[1]))
			for q in pp:
				y += 22.0
				if y > size.y - 40.0: break
				draw_string(f, Vector2(16, y), str(q[0]), HORIZONTAL_ALIGNMENT_LEFT, 110, fz, Kit.TEXT)
				var bw := size.x - 32.0 - 190.0
				draw_rect(Rect2(130, y - 11.0, bw, 9.0), Color(Kit.LINE, 0.5)); draw_rect(Rect2(130, y - 11.0, bw * clampf(float(q[1]) / pmx, 0.0, 1.0), 9.0), Color(Kit.exc(), 0.9))
				draw_string(f, Vector2(size.x - 66.0, y), "%.1f" % float(q[1]), HORIZONTAL_ALIGNMENT_RIGHT, 50, fz, Kit.TEXT)
		_note("Order of magnitude: 1e8 to 1e9 ATP per spike (Attwell and Laughlin 2001; Lennie 2003). The budget is 1.6 x the reference circuit's rate.", size.y - 18.0 - float(fz), f, fz)

## Whole-run spike statistics.
class StatsView extends Control:
	var tele: Telemetry = null
	var circuit := {}
	func _ready() -> void: clip_contents = true; custom_minimum_size = Vector2(300, 300)
	func min_text_px() -> float: return float(Kit.fs(14)) * Kit.ppl
	func _draw() -> void:
		var fz := Kit.fs(16 if size.x > 600.0 else 14); var f := Fonts.regular; var fb := Fonts.bold   # tables at 16 px on a desktop (they were 11-14)
		Kit.draw_plate(self, Rect2(Vector2.ZERO, size - Vector2(Kit.SHADOW, Kit.SHADOW)))
		Kit.draw_fig(self, Vector2(18, 32), 2, "Spike statistics", size.x - 44, "", 19)
		if tele == null or tele.n == 0 or tele.t_ms < 200.0:
			draw_multiline_string(f, Vector2(16, 64), "Run the circuit for a few seconds in the lab, then come back: rates, ISI variability and synchrony are measured from the spikes it produced.", HORIZONTAL_ALIGNMENT_LEFT, size.x - 32, Kit.fs(18), -1, Kit.TEXT_SOFT); return
		var chi := tele.sync_chi(); var tot := tele.total_spikes
		var mean_rate := float(tot) / maxf(float(tele.n), 1.0) / maxf(tele.t_ms * 0.001, 0.001)
		var y := 56.0
		for kv in [["simulated", "%.1f s" % (tele.t_ms * 0.001)], ["spikes", str(tot)], ["mean rate", "%.1f Hz" % mean_rate], ["synchrony (Golomb chi)", "%.2f" % chi]]:
			draw_string(f, Vector2(16, y), kv[0], HORIZONTAL_ALIGNMENT_LEFT, size.x * 0.6, Kit.fs(17), Kit.TEXT_SOFT); draw_string(Fonts.mono_med, Vector2(size.x - 160, y), kv[1], HORIZONTAL_ALIGNMENT_RIGHT, 144, Kit.fs(17), Kit.TEXT); y += 28.0
		y += 8.0; draw_line(Vector2(16, y), Vector2(size.x - 16, y), Kit.LINE, 1.0); y += 24.0
		draw_string(f, Vector2(16, y), "neuron", HORIZONTAL_ALIGNMENT_LEFT, 120, fz, Kit.TEXT_SOFT); draw_string(f, Vector2(size.x - 190, y), "rate Hz", HORIZONTAL_ALIGNMENT_RIGHT, 80, fz, Kit.TEXT_SOFT); draw_string(f, Vector2(size.x - 90, y), "ISI CV", HORIZONTAL_ALIGNMENT_RIGHT, 74, fz, Kit.TEXT_SOFT); y += 8.0
		var psth_h := 150.0
		var show := mini(tele.n, int((size.y - y - psth_h - 24.0) / 26.0)); var mx := 1.0
		for i in tele.n: mx = maxf(mx, tele.rate_hz(i))
		var pops: Array = circuit.get("populations", [])
		for i in show:
			var yy := y + i * 26.0 + 18.0; var pi := tele.pop_of[i]; var col: Color = Kit.inh() if tele.kinds[i] == "I" else Kit.exc()
			var lab := str(pops[pi].get("id", "")) + ("[%d]" % (i - Circuits.pop_first(circuit, pi)) if Circuits.pop_n(pops[pi]) > 1 else "")
			draw_string(f, Vector2(16, yy), lab, HORIZONTAL_ALIGNMENT_LEFT, 100, fz, col)
			var bx := 16.0 + 100.0; var bw := size.x - bx - 200.0; var r := tele.rate_hz(i)
			draw_rect(Rect2(bx, yy - 11.0, bw, 10.0), Color(Kit.LINE, 0.5)); draw_rect(Rect2(bx, yy - 11.0, bw * clampf(r / mx, 0, 1), 10.0), Color(col, 0.9))
			draw_string(f, Vector2(size.x - 190, yy), "%.1f" % r, HORIZONTAL_ALIGNMENT_RIGHT, 80, fz, Kit.TEXT)
			var cv := tele.isi_cv(i); draw_string(f, Vector2(size.x - 90, yy), "%.2f" % cv if cv >= 0.0 else "-", HORIZONTAL_ALIGNMENT_RIGHT, 74, fz, Kit.TEXT)
		var ptop := size.y - psth_h - 8.0
		if show < tele.n: draw_string(f, Vector2(size.x - 220, ptop - 8.0), "+ %d more neurons" % (tele.n - show), HORIZONTAL_ALIGNMENT_RIGHT, 204, fz, Kit.TEXT_SOFT)
		_psth(f, fz, Rect2(16, ptop + 22.0, size.x - 32.0, psth_h - 40.0), ptop)

	## Population PSTH of the last 4 s (mean rate per neuron, 40 ms bins).
	func _psth(f: Font, fz: int, r: Rect2, ptop: float) -> void:
		draw_string(f, Vector2(16, ptop + 4.0), "population rate, last 4 s, 40 ms bins", HORIZONTAL_ALIGNMENT_LEFT, size.x - 32, fz, Kit.TEXT_SOFT)
		var span := minf(4000.0, tele.t_ms); var bins := 100; var bw := span / bins; var h := PackedFloat32Array(); h.resize(bins)
		var t0 := tele.t_ms - span
		for j in tele.sp_t.size():
			var t := tele.sp_t[j]
			if t >= t0: h[mini(int((t - t0) / bw), bins - 1)] += 1.0
		var mx := 1.0
		for i in bins: h[i] = h[i] / (bw * 0.001 * maxf(float(tele.n), 1.0)); mx = maxf(mx, h[i])
		Kit.draw_graph_paper(self, r, 24.0); draw_rect(r, Kit.TEXT, false, 1.5)
		var pts := PackedVector2Array([Vector2(r.position.x, r.end.y)])
		for i in bins: pts.append(Vector2(r.position.x + r.size.x * (float(i) + 0.5) / bins, r.end.y - r.size.y * h[i] / mx))
		pts.append(Vector2(r.end.x, r.end.y))
		draw_colored_polygon(pts, Color(Kit.exc(), 0.22)); draw_polyline(pts.slice(1, pts.size() - 1), Kit.exc(), 1.8, true)
		draw_string(f, Vector2(r.position.x + 6, r.position.y + float(fz) + 2.0), "%d Hz" % int(roundf(mx)), HORIZONTAL_ALIGNMENT_LEFT, 80, fz, Kit.TEXT_SOFT)
