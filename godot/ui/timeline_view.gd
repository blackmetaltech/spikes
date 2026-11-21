class_name TimelineView
extends Control
## The perturbation timeline under the world: the task's scripted events (a push, standing up, exercise, a light flash, CO2 rising) as labelled spans on one
## strip, and below it one plot per judged trace (Task::bands()): the tolerance band the score is judged against, the trace so far (red where it is outside
## the band), and a moving cursor at the model time. The data is Telemetry.hist (the task's readouts sampled every 50 ms of model time).
var tele: Telemetry = null
var max_bands := 3
const GUTTER_PX := 8.5      # name column width in font heights

func _ready() -> void: clip_contents = true
func min_text_px() -> float: return float(Kit.fs(14)) * Kit.ppl
func attach(t: Telemetry) -> void: tele = t; queue_redraw()
func _process(_dt: float) -> void:
	if is_visible_in_tree(): queue_redraw()

## Height this view wants for `n` band plots (the strip, the plots, the time axis).
static func wanted_height(n: int, phone: bool) -> float:
	var fz := float(Kit.fs(14))
	return fz + 14.0 + float(mini(n, 3 if not phone else 1)) * (42.0 if phone else 50.0) + fz + 14.0

func _info() -> Dictionary: return Campaign.info(str(tele.task.id())) if tele != null and tele.task != null else {}

func _draw() -> void:
	var fz := Kit.fs(14); var f := Fonts.regular; var fb := Fonts.bold
	draw_rect(Rect2(Vector2.ZERO, size), Kit.CARD)
	var info := _info()
	if info.is_empty(): return
	var dur := maxf(float(info["duration_ms"]), 1.0); var events: Array = info["timeline"]; var bands: Array = info["bands"]
	var x0 := float(fz) * GUTTER_PX; var x1 := size.x - 12.0; var w := x1 - x0
	var tnow := tele.t_ms if tele != null else 0.0
	var strip := Rect2(x0, 6.0, w, float(fz) + 8.0)
	draw_string(Gfx.serif_italic(), Vector2(10, strip.position.y + float(fz) + 2.0), "events", HORIZONTAL_ALIGNMENT_LEFT, x0 - 12.0, fz, Kit.TEXT_SOFT)
	draw_rect(strip, Gfx.SHEET); draw_rect(strip, Color(Kit.INK, 0.6), false, 1.0)
	var nb := mini(bands.size(), max_bands if not Layout.phone else 1)
	var axis_h := float(fz) + 10.0
	var plots_top := strip.end.y + 4.0; var plots_h := size.y - plots_top - axis_h - 2.0
	var ph := plots_h / float(maxi(nb, 1)) if nb > 0 else 0.0
	var span_bottom := plots_top + plots_h
	# events: spans over the strip and faintly through the plots
	var label_end := -1000.0
	var sorted_ev := events.duplicate(); sorted_ev.sort_custom(func(a, b): return float(a["t_ms"]) < float(b["t_ms"]))
	for e in sorted_ev:
		var ex0 := x0 + w * clampf(float(e["t_ms"]) / dur, 0.0, 1.0); var ex1 := x0 + w * clampf((float(e["t_ms"]) + float(e["dur_ms"])) / dur, 0.0, 1.0)
		var er := Rect2(ex0, strip.position.y, maxf(ex1 - ex0, 3.0), strip.size.y)
		draw_rect(er, Color(Kit.exc(), 0.30 if tnow >= float(e["t_ms"]) else 0.14)); draw_rect(er, Color(Kit.exc(), 0.9), false, 1.0)
		if nb > 0: draw_rect(Rect2(ex0, plots_top, maxf(ex1 - ex0, 2.0), plots_h), Color(Kit.exc(), 0.07))
		# the label sits at the start of its span and may use the room up to the next event's start (or the end of the strip); it is shortened to fit, and left
		# out when there is no room, so two labels never print over each other
		var next_x := x1
		for e2 in events:
			var nx := x0 + w * clampf(float(e2["t_ms"]) / dur, 0.0, 1.0)
			if nx > er.position.x + 1.0: next_x = minf(next_x, nx)
		var lx := er.position.x + 5.0
		var room := next_x - lx - 4.0
		var txt := _fit(fb, str(e["label"]), room, fz)
		if txt == "" or lx < label_end + 2.0: continue
		label_end = lx + fb.get_string_size(txt, HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x
		draw_string(fb, Vector2(lx, strip.position.y + float(fz) + 2.0), txt, HORIZONTAL_ALIGNMENT_LEFT, label_end - lx + 4.0, fz, Kit.TEXT)
	# one plot per judged trace
	var hist_t: PackedFloat32Array = tele.hist_t if tele != null else PackedFloat32Array()
	for k in nb:
		var b: Dictionary = bands[k]; var name := str(b["name"]); var lo := float(b["lo"]); var hi := float(b["hi"])
		var r := Rect2(x0, plots_top + ph * k + 2.0, w, ph - 4.0)
		Gfx.chart_strip(self, r, 10.0)
		var arr: PackedFloat32Array = tele.hist.get(name, PackedFloat32Array()) if tele != null else PackedFloat32Array()
		var ylo := lo - (hi - lo) * 0.7; var yhi := hi + (hi - lo) * 0.7
		for q in arr: ylo = minf(ylo, q - (hi - lo) * 0.1); yhi = maxf(yhi, q + (hi - lo) * 0.1)
		var yy := func(v: float) -> float: return r.end.y - r.size.y * (v - ylo) / maxf(yhi - ylo, 1e-6)
		var band := Rect2(r.position.x, yy.call(hi), r.size.x, yy.call(lo) - yy.call(hi))
		draw_rect(band, Color(Kit.GREEN, 0.14))
		for yv in [lo, hi]: Gfx.dashed(self, PackedVector2Array([Vector2(r.position.x, yy.call(yv)), Vector2(r.end.x, yy.call(yv))]), Color(Kit.GREEN, 0.75), 1.2, 6.0, 5.0)
		var pn := BodyViews.pretty(name); var unit := str(b["unit"]) if str(b["unit"]) != "" else str(pn[1])
		var title := str(pn[0]); var tl := _fit(f, title, x0 - 14.0, fz)
		draw_string(Gfx.serif_italic(), Vector2(10, r.position.y + float(fz) + 1.0), tl, HORIZONTAL_ALIGNMENT_LEFT, x0 - 14.0, fz, Kit.TEXT)
		if unit != "" and not title.contains(unit): draw_string(f, Vector2(10, r.position.y + float(fz) * 2.1 + 3.0), unit, HORIZONTAL_ALIGNMENT_LEFT, x0 - 14.0, fz, Kit.TEXT_SOFT)
		if arr.size() > 1 and hist_t.size() == arr.size():
			var step := maxi(1, arr.size() / maxi(int(w), 1)); var run := PackedVector2Array(); var run_out := false
			for i in range(0, arr.size(), step):
				var p := Vector2(x0 + w * float(hist_t[i]) / dur, yy.call(clampf(arr[i], ylo, yhi)))
				var out := arr[i] < lo or arr[i] > hi
				if not run.is_empty() and out != run_out:
					run.append(p); _flush(run, run_out); run = PackedVector2Array()
				run_out = out; run.append(p)
			_flush(run, run_out)
			var cur := tele.display(name) if tele != null else arr[arr.size() - 1]   # the same 500 ms mean as the gauge and the motor bar
			if is_nan(cur): cur = arr[arr.size() - 1]
			var tw := f.get_string_size(_num(cur), HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x + 10.0
			draw_rect(Rect2(Vector2(r.end.x - tw - 2.0, r.position.y + 2.0), Vector2(tw, float(fz) + 6.0)), Color(Gfx.SHEET, 0.9))
			draw_string(fb, Vector2(r.end.x - tw + 3.0, r.position.y + float(fz) + 2.0), _num(cur), HORIZONTAL_ALIGNMENT_LEFT, tw, fz, Kit.RED if (cur < lo or cur > hi) else Kit.TEXT)
		if ph > float(fz) * 3.2: draw_string(f, Vector2(r.position.x + 6.0, r.end.y - 5.0), "%s to %s" % [_num(lo), _num(hi)], HORIZONTAL_ALIGNMENT_LEFT, r.size.x * 0.5, fz, Color(Kit.GREEN, 0.8))
	# the time axis and the cursor
	var tick := _tick_s(dur * 0.001, w)
	var tt := 0.0
	while tt <= dur * 0.001 + 0.001:
		var x := x0 + w * tt * 1000.0 / dur
		draw_line(Vector2(x, strip.end.y), Vector2(x, span_bottom), Color(Kit.LINE, 0.4), 1.0)
		draw_string(f, Vector2(minf(x - 24.0, size.x - 50.0), size.y - 6.0), "%ds" % int(roundf(tt)), HORIZONTAL_ALIGNMENT_CENTER, 48.0, fz, Kit.TEXT_SOFT)
		tt += tick
	if nb > 0: draw_string(Gfx.serif_italic(), Vector2(10, size.y - 6.0), "0.5 s mean", HORIZONTAL_ALIGNMENT_LEFT, x0 - 40.0, fz, Kit.TEXT_SOFT)
	var cx := x0 + w * clampf(tnow / dur, 0.0, 1.0)
	draw_line(Vector2(cx, strip.position.y - 2.0), Vector2(cx, span_bottom), Color(Kit.exc(), 0.95), 2.0, true)
	draw_circle(Vector2(cx, strip.position.y - 1.0), 3.5, Kit.exc())

func _flush(run: PackedVector2Array, out: bool) -> void:
	if run.size() < 2: return
	var col := Kit.RED if out else Kit.TEXT
	draw_polyline(run, col, 1.6, true)

## `s` shortened with "..." until it is at most `w` px wide; "" when not even three characters fit.
static func _fit(font: Font, s: String, w: float, fz: int) -> String:
	if font.get_string_size(s, HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x <= w: return s
	var t := s
	while t.length() > 3:
		t = t.substr(0, t.length() - 1)
		if font.get_string_size(t + "...", HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x <= w: return t + "..."
	return ""

static func _tick_s(total_s: float, px: float) -> float:
	var want := px / 90.0
	for t in [1.0, 2.0, 5.0, 10.0, 20.0, 30.0, 60.0]:
		if total_s / t <= want: return t
	return 60.0

static func _num(v: float) -> String:
	if absf(v) >= 100.0 or is_equal_approx(v, roundf(v)): return "%d" % int(roundf(v))
	return "%.1f" % v
