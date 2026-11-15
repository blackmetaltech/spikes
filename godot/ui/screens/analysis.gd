class_name AnalysisScreen
extends Screen
## Analysis: the 3-node motif census of the circuit (13 connected directed classes, Milo et al. 2002) with z-scores against degree-preserving
## nulls, from the C++ engine; plus spike statistics of the run so far (firing rate, ISI CV per neuron, Golomb synchrony).
var circuit := {}
var tele: Telemetry = null
var task_id := ""
var sign_idx := 0
var null_n := 100
var result := {}
var back_btn: Button
var chart: MotifChart
var stats_view: StatsView

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
	back_btn = Kit.btn("BACK", func(): app.back(), "panel", 22, Vector2(110, 0)); head.add_child(back_btn)
	var ttl := Kit.shout("Analysis", int(46 * f) if not phone else 30, Kit.TEXT); ttl.size_flags_horizontal = Control.SIZE_EXPAND_FILL; head.add_child(ttl)
	head.add_child(Kit.label("edges", 18, Kit.TEXT_SOFT))
	head.add_child(Kit.stepper([[0, "all"], [1, "excitatory"], [2, "inhibitory"]], str(sign_idx), func(i): sign_idx = int(i); compute(), 190.0 if not phone else 130.0))
	head.add_child(Kit.label("nulls", 18, Kit.TEXT_SOFT))
	head.add_child(Kit.stepper([[20, "20"], [100, "100"], [500, "500"]], str(null_n), func(i): null_n = int(i); compute(), 80.0))
	var cols := HBoxContainer.new(); cols.size_flags_vertical = Control.SIZE_EXPAND_FILL; cols.add_theme_constant_override("separation", int(16 * f)); v.add_child(cols)
	chart = MotifChart.new(); chart.size_flags_horizontal = Control.SIZE_EXPAND_FILL; chart.size_flags_stretch_ratio = 1.5; chart.size_flags_vertical = Control.SIZE_EXPAND_FILL; cols.add_child(chart)
	stats_view = StatsView.new(); stats_view.size_flags_horizontal = Control.SIZE_EXPAND_FILL; stats_view.size_flags_vertical = Control.SIZE_EXPAND_FILL; cols.add_child(stats_view)
	stats_view.tele = tele; stats_view.circuit = circuit
	compute()

func compute() -> void:
	result = {}
	if circuit.is_empty() or Circuits.neuron_count(circuit) == 0: chart.message = "No circuit yet. Build one in the lab."; chart.queue_redraw(); return
	if not ClassDB.class_exists("SpikeSim"): chart.message = "The spike-gd GDExtension is not loaded."; chart.queue_redraw(); return
	# A throwaway engine just to read the connection graph from NEST (the lab's engine is stopped while this screen is open).
	var sim = ClassDB.instantiate("SpikeSim")
	if not sim.load_circuit(Circuits.to_json(circuit)): chart.message = "The engine rejected the circuit: " + str(sim.error()); chart.queue_redraw(); return
	var txt := str(sim.motif_json(sign_idx, null_n, int(circuit.get("seed", 1))))
	sim.stop()
	var d := Circuits.parse(txt)
	if d.has("error"): chart.message = str(d["error"])
	else: result = d; chart.message = ""
	chart.result = result; chart.queue_redraw()
	if stats_view != null: stats_view.queue_redraw()

## 13 motif classes as a diverging z-score chart with their glyphs.
class MotifChart extends Control:
	var result := {}
	var message := ""
	func _ready() -> void: clip_contents = true; custom_minimum_size = Vector2(380, 300)
	func min_text_px() -> float: return float(Kit.fs(14)) * Kit.ppl
	func _draw() -> void:
		var fz := Kit.fs(14); var f := Fonts.regular; var fb := Fonts.bold
		draw_style_box(Kit.sb(Kit.PANEL, Kit.LINE, 1, 14, 0, false), Rect2(Vector2.ZERO, size))
		draw_string(fb, Vector2(16, 28), "THREE-NODE MOTIFS", HORIZONTAL_ALIGNMENT_LEFT, 300, fz + 2, Kit.exc())
		draw_string(f, Vector2(16 + 230, 28), "count, and z-score against degree-preserving rewiring", HORIZONTAL_ALIGNMENT_LEFT, size.x - 260, fz, Kit.TEXT_SOFT)
		if message != "" or not result.has("motifs"):
			draw_string(f, Vector2(0, size.y * 0.5), message if message != "" else "computing...", HORIZONTAL_ALIGNMENT_CENTER, size.x, Kit.fs(20), Kit.TEXT_SOFT); return
		var ms: Array = result["motifs"]; var top := 46.0; var rh := (size.y - top - 16.0) / 13.0
		var gx := 16.0; var gw := minf(rh * 1.5, 56.0)
		var name_w := float(fz) * 12.5; var cnt_w := float(fz) * 3.4
		var bx := gx + gw + name_w + cnt_w + 14.0; var bw := size.x - bx - 16.0
		var zmax := 8.0
		var mid := bx + bw * 0.5
		draw_line(Vector2(mid, top), Vector2(mid, size.y - 12.0), Color(Kit.LINE, 1.0), 1.5)
		for zz in [-4, -2, 2, 4]:
			var x := mid + bw * 0.5 * float(zz) / zmax; draw_line(Vector2(x, top), Vector2(x, size.y - 12.0), Color(Kit.LINE, 0.4), 1.0)
		draw_string(f, Vector2(mid - 12, top - 4), "z=0", HORIZONTAL_ALIGNMENT_CENTER, 24, fz, Kit.TEXT_SOFT)
		var maxc := 1.0
		for m in ms: maxc = maxf(maxc, float(m["count"]))
		for i in ms.size():
			var m: Dictionary = ms[i]; var y := top + rh * i; var cy := y + rh * 0.5
			if i % 2 == 0: draw_rect(Rect2(8, y, size.x - 16, rh), Color(1, 1, 1, 0.025))
			_glyph(Vector2(gx + gw * 0.5, cy), minf(rh * 0.42, 14.0), int(m["milo_id"]))
			draw_string(f, Vector2(gx + gw + 8, cy + float(fz) * 0.35), _pretty(str(m["name"])), HORIZONTAL_ALIGNMENT_LEFT, name_w, fz, Kit.TEXT)
			var cnt := int(m["count"]); draw_string(fb, Vector2(gx + gw + name_w + 8, cy + float(fz) * 0.35), str(cnt), HORIZONTAL_ALIGNMENT_RIGHT, cnt_w, fz, Kit.TEXT if cnt > 0 else Kit.TEXT_SOFT)
			var zv = m.get("z", 0.0); var z := float(zv) if zv != null else 0.0
			if is_inf(z) or is_nan(z): z = zmax if (zv == null and cnt > 0) else 0.0
			z = clampf(z, -zmax, zmax); var x1 := mid + bw * 0.5 * z / zmax; var col := Kit.exc() if z > 0.0 else Kit.inh()
			if absf(z) > 0.02:
				Gfx.glow(self, Vector2(x1, cy), rh * 0.9, Color(col, 0.12 if absf(z) > 2.0 else 0.0))
				draw_rect(Rect2(minf(mid, x1), cy - rh * 0.27, absf(x1 - mid), rh * 0.54), Color(col, 0.95 if absf(z) >= 2.0 else 0.45))
			var zs := ("%+.1f" % z) if cnt > 0 or absf(z) > 0.0 else "-"
			zs += "*" if absf(z) >= 2.0 else ""
			if absf(x1 - mid) > 70.0:   # a long bar carries its label inside, at the far end
				draw_string(fb, Vector2(x1 - 74.0 if z > 0.0 else x1 + 6.0, cy + float(fz) * 0.35), zs, HORIZONTAL_ALIGNMENT_RIGHT if z > 0.0 else HORIZONTAL_ALIGNMENT_LEFT, 68, fz, Kit.BENCH)
			else:
				draw_string(f, Vector2(x1 + (6.0 if z >= 0 else -6.0 - 48.0), cy + float(fz) * 0.35), zs, HORIZONTAL_ALIGNMENT_LEFT if z >= 0 else HORIZONTAL_ALIGNMENT_RIGHT, 48, fz, Kit.TEXT_SOFT)
		draw_string(f, Vector2(size.x - 16 - 260, size.y - 4), "* |z| >= 2     nulls: %d" % int(result.get("null_samples", 0)), HORIZONTAL_ALIGNMENT_RIGHT, 260, fz, Kit.TEXT_SOFT)

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

## Whole-run spike statistics.
class StatsView extends Control:
	var tele: Telemetry = null
	var circuit := {}
	func _ready() -> void: clip_contents = true; custom_minimum_size = Vector2(300, 300)
	func min_text_px() -> float: return float(Kit.fs(14)) * Kit.ppl
	func _draw() -> void:
		var fz := Kit.fs(14); var f := Fonts.regular; var fb := Fonts.bold
		draw_style_box(Kit.sb(Kit.PANEL, Kit.LINE, 1, 14, 0, false), Rect2(Vector2.ZERO, size))
		draw_string(fb, Vector2(16, 28), "SPIKE STATISTICS", HORIZONTAL_ALIGNMENT_LEFT, 300, fz + 2, Kit.inh())
		if tele == null or tele.n == 0 or tele.t_ms < 200.0:
			draw_multiline_string(f, Vector2(16, 64), "Run the circuit for a few seconds in the lab, then come back: rates, ISI variability and synchrony are measured from the spikes it produced.", HORIZONTAL_ALIGNMENT_LEFT, size.x - 32, Kit.fs(18), -1, Kit.TEXT_SOFT); return
		var chi := tele.sync_chi(); var tot := tele.total_spikes
		var mean_rate := float(tot) / maxf(float(tele.n), 1.0) / maxf(tele.t_ms * 0.001, 0.001)
		var y := 56.0
		for kv in [["simulated", "%.1f s" % (tele.t_ms * 0.001)], ["spikes", str(tot)], ["mean rate", "%.1f Hz" % mean_rate], ["synchrony (Golomb chi)", "%.2f" % chi]]:
			draw_string(f, Vector2(16, y), kv[0], HORIZONTAL_ALIGNMENT_LEFT, size.x * 0.6, Kit.fs(17), Kit.TEXT_SOFT); draw_string(fb, Vector2(size.x - 160, y), kv[1], HORIZONTAL_ALIGNMENT_RIGHT, 144, Kit.fs(17), Kit.TEXT); y += 28.0
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
		draw_rect(r, Color(Kit.BENCH, 0.6)); draw_rect(r, Color(Kit.LINE, 0.9), false, 1.0)
		var pts := PackedVector2Array([Vector2(r.position.x, r.end.y)])
		for i in bins: pts.append(Vector2(r.position.x + r.size.x * (float(i) + 0.5) / bins, r.end.y - r.size.y * h[i] / mx))
		pts.append(Vector2(r.end.x, r.end.y))
		draw_colored_polygon(pts, Color(Kit.exc(), 0.22)); draw_polyline(pts.slice(1, pts.size() - 1), Kit.exc(), 1.8, true)
		draw_string(f, Vector2(r.position.x + 6, r.position.y + float(fz) + 2.0), "%d Hz" % int(roundf(mx)), HORIZONTAL_ALIGNMENT_LEFT, 80, fz, Kit.TEXT_SOFT)
