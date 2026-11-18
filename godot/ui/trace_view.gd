class_name TraceView
extends Control
## Membrane-potential chart recorder: 1 to 4 traces (1 ms samples of V_m from NEST), each a pen line in its cell's stain on its own chart-paper strip,
## with an inked stroke at every recorded spike (NEST models reset V_m at the threshold, so the stroke shows the action potential).
var tele: Telemetry = null
var window_ms := 500.0           # the window drawn now (eases toward window_target)
var auto_window := true          # follow the timescale of the watched cells; a click on the "window" caption steps through fixed windows
var window_target := 500.0
var _retarget_at := 0.0
var _pending := 0.0
var _layer: Control
const WINDOWS := [250.0, 500.0, 1000.0, 2000.0, 3000.0, 4000.0, 6000.0, 8000.0, 10000.0, 15000.0, 20000.0, 30000.0]
var _range := {}    # neuron -> Vector2(lo, hi), eased

class Layer extends Control:
	var owner_view: Control
	func _draw() -> void: owner_view.call("_draw_traces", self)

func _ready() -> void:
	clip_contents = true; custom_minimum_size = Vector2(300, 120)
	_layer = Layer.new(); _layer.owner_view = self; _layer.set_anchors_preset(Control.PRESET_FULL_RECT); _layer.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_layer)

func min_text_px() -> float: return float(Kit.fs(14)) * Kit.ppl
func attach(t: Telemetry) -> void: tele = t; _range = {}; _pending = 0.0; _retarget_at = 0.0; window_ms = 500.0; window_target = 500.0; queue_redraw()
func _process(dt: float) -> void:
	if not is_visible_in_tree(): return
	if tele != null and auto_window: _auto(dt)
	window_ms = lerpf(window_ms, window_target, clampf(dt * 8.0, 0.0, 1.0)) if absf(window_ms - window_target) > 1.0 else window_target
	queue_redraw(); _layer.queue_redraw()

## The smallest standard window that is at least `ms` and no longer than `cap` (the largest allowed when none fits).
static func nice_window(ms: float, cap: float) -> float:
	var best := float(WINDOWS[0])
	for w in WINDOWS:
		if float(w) <= cap: best = float(w)
		if float(w) >= ms and float(w) <= cap: return float(w)
	return best

## The longest quiet stretch of neuron `id` in the spikes the telemetry still holds (ms): the longest gap between two spikes, or the time since its last spike
## when that is longer (a cell that has gone slow or silent). 0 when it has not fired.
static func quiet_gap(tele: Telemetry, id: int) -> float:
	var last := -1.0; var gap := 0.0
	for j in tele.sp_t.size():
		if tele.sp_n[j] != id: continue
		var t := tele.sp_t[j]
		if last >= 0.0: gap = maxf(gap, t - last)
		last = t
	if last < 0.0: return 0.0
	return maxf(gap, tele.t_ms - last)

## A window from the task's length when the cells have not told us their timescale yet: a 30 s episode starts at a couple of seconds, a 5 s one at the default.
static func task_window(tele: Telemetry) -> float:
	var dur := tele.duration() if tele != null else 0.0
	return clampf(dur / 12.0, 500.0, 4000.0) if dur > 0.0 else 500.0

func _auto(dt: float) -> void:
	_retarget_at -= dt
	if _retarget_at > 0.0: return
	_retarget_at = 0.5
	var gap := 0.0
	for w in tele.watched: gap = maxf(gap, quiet_gap(tele, int(w)))
	var fb := task_window(tele)
	var want := maxf(gap * 2.6, fb * 0.25) if gap > 0.0 else fb
	var cap := float(Telemetry.VOLT_KEEP) * 0.5 - 100.0   # the scope only keeps that much voltage
	var nw := nice_window(want, cap)
	if nw == window_target: _pending = 0.0
	else:
		_pending += 1.0
		if _pending >= 2.0 or tele.t_ms < 600.0: window_target = nw; _pending = 0.0

func _gui_input(e: InputEvent) -> void:
	if e is InputEventMouseButton and e.pressed and e.button_index == MOUSE_BUTTON_LEFT and Rect2(size.x - 200.0, 0.0, 200.0, 26.0).has_point(e.position):
		step_window(); accept_event()

## Auto -> 500 ms -> 1 s -> 2 s -> 4 s -> 10 s -> auto.
func step_window() -> void:
	var seq := [500.0, 1000.0, 2000.0, 4000.0, 10000.0]
	if auto_window: auto_window = false; window_target = seq[0]
	else:
		var i := seq.find(window_target)
		if i < 0 or i >= seq.size() - 1: auto_window = true
		else: window_target = seq[i + 1]

## "window 2 s", with the unit that reads best.
static func window_text(ms: float, auto: bool) -> String:
	var s := ("%d ms" % int(roundf(ms))) if ms < 1000.0 else ("%s s" % (("%d" % int(roundf(ms / 1000.0))) if absf(ms / 1000.0 - roundf(ms / 1000.0)) < 0.05 else "%.1f" % (ms / 1000.0)))
	return "window %s%s" % [s, " (auto)" if auto else ""]

## Grid spacing (ms) for a window: about 5 to 10 lines.
static func grid_step(ms: float) -> float:
	for st in [10.0, 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0]:
		if ms / float(st) <= 10.0: return float(st)
	return 5000.0

static func tick_text(t_ms: float, step: float) -> String:
	return "%d s" % int(roundf(t_ms * 0.001)) if step >= 1000.0 else "%.1f s" % (t_ms * 0.001)

func _lanes() -> Array:
	var out := []
	if tele == null: return out
	var k := tele.watched.size(); if k == 0: return out
	var top := 26.0; var gap := 8.0; var fz := float(Kit.fs(14))
	k = clampi(int((size.y - top - fz - 14.0 + gap) / (46.0 + gap)), 1, k)   # lanes stay at least 46 px tall: fewer lanes in a short panel
	var h := (size.y - top - fz - 14.0 - gap * float(k - 1)) / float(k)
	var x0 := fz * 3.4; var w := size.x - x0 - 12.0
	for i in k: out.append({"n": tele.watched[i], "rect": Rect2(x0, top + (h + gap) * i, w, h)})
	return out

func _kind(i: int) -> String: return str(tele.kinds[i]) if tele != null and i < tele.kinds.size() else "E"

func _draw() -> void:
	var fz := Kit.fs(14); var f := Fonts.regular
	draw_rect(Rect2(Vector2.ZERO, size), Kit.CARD)
	draw_string(Gfx.serif_italic(), Vector2(14, 20), "Membrane potential", HORIZONTAL_ALIGNMENT_LEFT, 240, Kit.fs(16), Kit.TEXT)
	if tele == null or tele.sim == null: return
	var vt := tele.view_t; var t_left := vt - window_ms
	var lanes := _lanes()
	if lanes.is_empty():
		draw_string(f, Vector2(0, size.y * 0.55), "Select a neuron and press TRACE to watch its voltage.", HORIZONTAL_ALIGNMENT_CENTER, size.x, fz, Kit.TEXT_SOFT); return
	for ln in lanes:
		var r: Rect2 = ln["rect"]; var nid: int = ln["n"]
		var col: Color = Kit.inh() if _kind(nid) == "I" else Kit.exc()
		Gfx.chart_strip(self, r, 10.0)
		var rg := _lane_range(nid); var lo := rg.x; var hi := rg.y
		# graticule: horizontal every 10/20 mV, vertical every 100 ms
		var step := 10.0 if hi - lo <= 60.0 else 20.0; var mv := ceilf(lo / step) * step; var last_lab_y := -1000.0
		while mv <= hi:
			var y := r.end.y - r.size.y * (mv - lo) / (hi - lo)
			draw_line(Vector2(r.position.x - 4.0, y), Vector2(r.position.x, y), Color(Kit.INK, 0.6), 1.0)
			if absf(y - last_lab_y) >= float(fz) * 1.25 and y > r.position.y + float(fz) * 0.5 and y < r.end.y - float(fz) * 0.4: last_lab_y = y; draw_string(Gfx.mono(), Vector2(4, y + float(fz) * 0.35), "%d" % int(mv), HORIZONTAL_ALIGNMENT_LEFT, r.position.x - 8, fz, Kit.TEXT_SOFT)
			mv += step
		var gs := grid_step(window_ms); var g0 := ceilf(t_left / gs) * gs; var tt := g0
		while tt <= vt:
			var x := r.position.x + r.size.x * (tt - t_left) / window_ms; draw_line(Vector2(x, r.end.y), Vector2(x, r.end.y - 4.0), Color(Kit.INK, 0.5), 1.0); tt += gs
	var gs2 := grid_step(window_ms); var lab_every := maxi(1, int(ceilf(64.0 / maxf(lanes[0]["rect"].size.x * gs2 / window_ms, 1.0)))); var tt2 := ceilf(t_left / gs2) * gs2
	var bot: Rect2 = lanes[lanes.size() - 1]["rect"]
	while tt2 <= vt:
		var xx := bot.position.x + bot.size.x * (tt2 - t_left) / window_ms
		if int(roundf(tt2 / gs2)) % lab_every == 0: draw_string(Gfx.mono(), Vector2(xx - 30, bot.end.y + float(fz) + 3.0), tick_text(tt2, gs2), HORIZONTAL_ALIGNMENT_CENTER, 60, fz, Kit.TEXT_SOFT)
		tt2 += gs2
	# axes caption
	draw_string(Gfx.mono(), Vector2(size.x - 230, 20), window_text(window_ms, auto_window), HORIZONTAL_ALIGNMENT_RIGHT, 216, fz, Kit.TEXT_SOFT)

func _lane_range(nid: int) -> Vector2:
	var a: PackedFloat32Array = tele.volt.get(nid, PackedFloat32Array())
	var lo := 1e9; var hi := -1e9; var cnt := mini(a.size(), int(window_ms))
	for j in range(a.size() - cnt, a.size()):
		var v := a[j]; if is_nan(v): continue
		lo = minf(lo, v); hi = maxf(hi, v)
	if lo > hi: lo = -75.0; hi = -50.0
	# leave headroom for the spike marker, and a stable minimum span
	hi = maxf(hi, lo + 12.0) + 8.0; lo = lo - 4.0
	if hi - lo < 36.0: var m := (hi + lo) * 0.5; lo = m - 18.0; hi = m + 18.0
	lo = floorf(lo / 5.0) * 5.0; hi = ceilf(hi / 5.0) * 5.0
	var cur: Vector2 = _range.get(nid, Vector2(lo, hi)); cur = cur.lerp(Vector2(lo, hi), 0.06); _range[nid] = cur
	return cur

func _draw_traces(layer: Control) -> void:
	if tele == null or tele.sim == null: return
	var vt := tele.view_t; var t_left := vt - window_ms
	for ln in _lanes():
		var r: Rect2 = ln["rect"]; var nid: int = ln["n"]; var col: Color = Kit.inh() if _kind(nid) == "I" else Kit.exc()
		var rg: Vector2 = _range.get(nid, Vector2(-80, -40)); var lo := rg.x; var hi := rg.y
		var a: PackedFloat32Array = tele.volt.get(nid, PackedFloat32Array()); if a.size() < 2: continue
		var pts := PackedVector2Array(); var tl := tele.t_ms
		var j0 := maxi(0, a.size() - 1 - int(window_ms + 40.0))
		var px := -1e9
		for j in range(j0, a.size()):
			var t := tl - float(a.size() - 1 - j); var x := r.position.x + r.size.x * (t - t_left) / window_ms
			if x < r.position.x - 2.0 or x > r.end.x + 2.0: continue
			if x - px < 0.6 and j < a.size() - 1: continue
			px = x; pts.append(Vector2(x, r.end.y - r.size.y * (clampf(a[j], lo, hi) - lo) / (hi - lo)))
		pts = _clip_x(pts, r)
		if pts.size() > 1:
			layer.draw_polyline(pts, Color(col, 0.95), 1.5, true)
		# spike markers: a vertical needle from the trace to the top of the lane
		for s in tele.sp_t.size():
			if tele.sp_n[s] != nid: continue
			var t := tele.sp_t[s]; if t < t_left or t > vt: continue
			var x := r.position.x + r.size.x * (t - t_left) / window_ms
			layer.draw_line(Vector2(x, r.end.y - r.size.y * (clampf(a[maxi(a.size() - 1 - int(tl - t), 0)] if int(tl - t) < a.size() else lo, lo, hi) - lo) / (hi - lo)), Vector2(x, r.position.y + 3.0), Color(col, 0.95), 1.5, true)
		_lane_label(layer, r, nid, col)
		# the pen: a small nib of ink where the trace is being written
		if pts.size() > 0: layer.draw_circle(pts[pts.size() - 1], 3.0, Color(Gfx.SHEET, 0.9)); layer.draw_circle(pts[pts.size() - 1], 2.0, Kit.INK)

func _clip_x(pts: PackedVector2Array, r: Rect2) -> PackedVector2Array:
	var out := PackedVector2Array()
	for p in pts:
		if p.x >= r.position.x - 0.5 and p.x <= r.end.x + 0.5: out.append(p)
	return out

## The lane's name (population, model, index) and its present voltage, on paper chips over the chart (drawn after the trace so the pen never covers them).
func _lane_label(ci: CanvasItem, r: Rect2, nid: int, col: Color) -> void:
	var fz := Kit.fs(14); var fi := Gfx.serif_italic()
	var pop_i: int = tele.pop_of[nid] if nid < tele.pop_of.size() else 0
	var label := "%s  %s" % [str(tele.circuit["populations"][pop_i].get("id", "")), str(tele.circuit["populations"][pop_i].get("model", ""))]
	if tele.circuit["populations"][pop_i].get("n", 1) > 1: label += "  [%d]" % (nid - Circuits.pop_first(tele.circuit, pop_i))
	var lw := minf(fi.get_string_size(label, HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x + 12.0, r.size.x * 0.7)
	ci.draw_rect(Rect2(r.position + Vector2(2, 2), Vector2(lw, float(fz) + 6.0)), Color(Gfx.SHEET, 0.92))
	ci.draw_string(fi, r.position + Vector2(8, float(fz) + 3.0), label, HORIZONTAL_ALIGNMENT_LEFT, r.size.x * 0.7, fz, col.darkened(0.1))
	var a: PackedFloat32Array = tele.volt.get(nid, PackedFloat32Array())
	if a.size() > 0:
		ci.draw_rect(Rect2(Vector2(r.end.x - 96, r.position.y + 2), Vector2(94, float(fz) + 6.0)), Color(Gfx.SHEET, 0.92))
		ci.draw_string(Gfx.mono(), Vector2(r.end.x - 120, r.position.y + float(fz) + 3.0), ("%.1f mV" % a[a.size() - 1]) if is_finite(a[a.size() - 1]) else "no membrane", HORIZONTAL_ALIGNMENT_RIGHT, 112, fz, Kit.TEXT)
