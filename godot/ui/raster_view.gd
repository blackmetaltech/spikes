class_name RasterView
extends Control
## Scrolling spike raster, drawn as a chart-paper strip: one row per neuron grouped by population, each spike a pen tick in sienna (excitatory) or Prussian blue
## (inhibitory). Scrolls smoothly on `Telemetry.view_t` (continuous), with a ruled time grid and an inked "now" edge.
var tele: Telemetry = null
var window_ms := 500.0           # the window drawn now (eases toward window_target)
var auto_window := true          # follow the timescale of the cells; a click on the "window" caption steps through fixed windows
var window_target := 500.0
var _retarget_at := 0.0
var _pending := 0.0
var _layer: Control

class Layer extends Control:
	var owner_view: Control
	func _draw() -> void: owner_view.call("_draw_spikes", self)

func _ready() -> void:
	clip_contents = true; custom_minimum_size = Vector2(300, 150)
	_layer = Layer.new(); _layer.owner_view = self; _layer.set_anchors_preset(Control.PRESET_FULL_RECT); _layer.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_layer)

func min_text_px() -> float: return float(Kit.fs(14)) * Kit.ppl
func attach(t: Telemetry) -> void: tele = t; _pending = 0.0; _retarget_at = 0.0; window_ms = 500.0; window_target = 500.0; queue_redraw()
## v0.6: a circuit made only of rate units has no spikes: the raster becomes a heat strip, one row per unit, colour = rate (sienna up, blue below zero), 3 s of model time.
var _rh := {}                    # neuron -> PackedFloat32Array of rate samples (every 4 ms of model time)
var _rt := PackedFloat32Array()
var _rep := -1
const RATE_WIN_MS := 3000.0
func _rate_only() -> bool:
	if tele == null or tele.sim == null or tele.n == 0: return false
	for p in tele.circuit.get("populations", []):
		if not Circuits.is_rate_model(str(p.get("model", ""))): return false
	return true
func _sample_rates() -> void:
	if _rep != tele.episode: _rep = tele.episode; _rh = {}; _rt = PackedFloat32Array()
	if not _rt.is_empty() and tele.t_ms - _rt[_rt.size() - 1] < 4.0: return
	_rt.append(tele.t_ms)
	for i in tele.n:
		var a: PackedFloat32Array = _rh.get(i, PackedFloat32Array()); var v := float(tele.sim.neuron_state(i, "rate")); a.append(v if is_finite(v) else 0.0); _rh[i] = a
	if _rt.size() > 1500:
		_rt = _rt.slice(_rt.size() - 1500)
		for i in _rh: _rh[i] = (_rh[i] as PackedFloat32Array).slice((_rh[i] as PackedFloat32Array).size() - 1500)

func _draw_rate_strip(pl: Rect2) -> void:
	var fz := Kit.fs(14); var n := tele.n; var row := pl.size.y / float(n); var t1 := tele.t_ms; var t0 := t1 - RATE_WIN_MS
	var vmax := 1.0
	for i in _rh: for v in (_rh[i] as PackedFloat32Array): vmax = maxf(vmax, absf(v))
	var first := 0; var pops: Array = tele.circuit.get("populations", [])
	for pi in pops.size():
		var cnt := Circuits.pop_n(pops[pi])
		draw_string(Gfx.serif_italic(), Vector2(8, pl.position.y + row * (float(first) + float(cnt) * 0.5) + float(fz) * 0.35), str(pops[pi].get("id", "")), HORIZONTAL_ALIGNMENT_LEFT, pl.position.x - 12, fz, Kit.TEXT)
		first += cnt
	for i in n:
		var a: PackedFloat32Array = _rh.get(i, PackedFloat32Array())
		for j in _rt.size():
			if j >= a.size() or _rt[j] < t0: continue
			var x0 := pl.position.x + pl.size.x * (_rt[j] - t0) / RATE_WIN_MS; var x1 := pl.position.x + pl.size.x * ((_rt[j + 1] if j + 1 < _rt.size() else t1) - t0) / RATE_WIN_MS
			var k := clampf(absf(a[j]) / vmax, 0.0, 1.0); var col := Kit.exc() if a[j] >= 0.0 else Kit.inh()
			draw_rect(Rect2(x0, pl.position.y + row * float(i) + 1.0, maxf(x1 - x0, 1.5), row - 2.0), Color(col, 0.08 + 0.9 * k))
	draw_string(Gfx.mono(), Vector2(size.x - 300, 20), "rate, last 3 s, full colour = %.1f" % vmax, HORIZONTAL_ALIGNMENT_RIGHT, 286, fz, Kit.TEXT_SOFT)

func _process(dt: float) -> void:
	if not is_visible_in_tree(): return
	if _rate_only(): _sample_rates()
	if tele != null and auto_window: _auto(dt)
	window_ms = lerpf(window_ms, window_target, clampf(dt * 8.0, 0.0, 1.0)) if absf(window_ms - window_target) > 1.0 else window_target
	queue_redraw(); _layer.queue_redraw()

## The window from the cells' own timescale: the median over the neurons that have fired of their longest quiet stretch, about 3 cycles wide (with a floor from the
## task's length until the cells have fired). Never wider than the spikes the telemetry keeps.
func _auto(dt: float) -> void:
	_retarget_at -= dt
	if _retarget_at > 0.0: return
	_retarget_at = 0.5
	var gaps := []
	var seen := {}
	for j in tele.sp_n.size(): seen[int(tele.sp_n[j])] = true
	for id in seen.keys(): gaps.append(TraceView.quiet_gap(tele, int(id)))
	var fb := TraceView.task_window(tele); var want := fb
	if not gaps.is_empty():
		gaps.sort(); var med: float = gaps[gaps.size() / 2]
		want = maxf(med * 3.0, fb * 0.25)
	var nw := TraceView.nice_window(want, Telemetry.HISTORY_MS - 100.0)
	if nw == window_target: _pending = 0.0
	else:
		_pending += 1.0
		if _pending >= 2.0 or tele.t_ms < 600.0: window_target = nw; _pending = 0.0

func _gui_input(e: InputEvent) -> void:
	if e is InputEventMouseButton and e.pressed and e.button_index == MOUSE_BUTTON_LEFT and Rect2(size.x - 280.0, 0.0, 280.0, 26.0).has_point(e.position):
		step_window(); accept_event()

## Auto -> 500 ms -> 1 s -> 2 s -> 4 s -> auto.
func step_window() -> void:
	var seq := [500.0, 1000.0, 2000.0, 4000.0]
	if auto_window: auto_window = false; window_target = seq[0]
	else:
		var i := seq.find(window_target)
		if i < 0 or i >= seq.size() - 1: auto_window = true
		else: window_target = seq[i + 1]

func _plot() -> Rect2:
	var fz := float(Kit.fs(14)); return Rect2(Vector2(fz * 4.0, 26.0), Vector2(size.x - fz * 4.0 - 12.0, size.y - 26.0 - fz - 12.0))

func _draw() -> void:
	var fz := Kit.fs(14); var f := Fonts.regular; var pl := _plot()
	draw_rect(Rect2(Vector2.ZERO, size), Kit.CARD)
	Gfx.chart_strip(self, pl, 12.0)
	if _rate_only():
		draw_string(Gfx.serif_italic(), Vector2(14, 20), "Population rates", HORIZONTAL_ALIGNMENT_LEFT, 200, Kit.fs(16), Kit.TEXT)
		_draw_rate_strip(pl); return
	draw_string(Gfx.serif_italic(), Vector2(14, 20), "Spike raster", HORIZONTAL_ALIGNMENT_LEFT, 200, Kit.fs(16), Kit.TEXT)
	var vt := tele.view_t if tele != null else 0.0
	# 100 ms grid, scrolling with time
	var gs := TraceView.grid_step(window_ms); var lab_every := maxi(1, int(ceilf(64.0 / maxf(pl.size.x * gs / window_ms, 1.0))))
	var t_left := vt - window_ms; var g0 := ceilf(t_left / gs) * gs; var t := g0
	while t <= vt:
		var x := pl.position.x + pl.size.x * (t - t_left) / window_ms
		draw_line(Vector2(x, pl.end.y), Vector2(x, pl.end.y + 4.0), Color(Kit.INK, 0.7), 1.0)
		if int(roundf(t / gs)) % lab_every == 0: draw_string(Gfx.mono(), Vector2(x - 30, pl.end.y + float(fz) + 3.0), TraceView.tick_text(t, gs), HORIZONTAL_ALIGNMENT_CENTER, 60, fz, Kit.TEXT_SOFT)
		t += gs
	var wt := TraceView.window_text(window_ms, auto_window)
	draw_string(Gfx.mono(), Vector2(size.x - 300, 20), wt + (", model time" if size.x > 560 else ""), HORIZONTAL_ALIGNMENT_RIGHT, 286, fz, Kit.TEXT_SOFT)
	if tele == null or tele.n == 0: return
	# population groups
	var n := tele.n; var row := pl.size.y / float(n); var c := tele.circuit; var first := 0
	var pops: Array = c.get("populations", [])
	for pi in pops.size():
		var cnt := Circuits.pop_n(pops[pi]); var yc := pl.position.y + row * (float(first) + float(cnt) * 0.5)
		if pi > 0: draw_line(Vector2(pl.position.x - 6, pl.position.y + row * first), Vector2(pl.end.x, pl.position.y + row * first), Color(Kit.INK, 0.28), 1.0)
		if cnt * row >= float(fz) * 0.9 or n <= 16: draw_string(Gfx.serif_italic(), Vector2(8, yc + float(fz) * 0.35), str(pops[pi].get("id", "")), HORIZONTAL_ALIGNMENT_LEFT, pl.position.x - 12, fz, Kit.TEXT)
		first += cnt

func _draw_spikes(layer: Control) -> void:
	if tele == null or tele.n == 0 or _rate_only(): return
	var pl := _plot(); var n := tele.n; var row := pl.size.y / float(n); var vt := tele.view_t; var t_left := vt - window_ms
	var th := minf(row * 0.78, 20.0); var w := 1.7 if window_ms / maxf(pl.size.x, 1.0) < 2.5 else 1.1
	var ec := Kit.exc(); var ic := Kit.inh()
	var lines_e := PackedVector2Array(); var lines_i := PackedVector2Array(); var hot := []
	for j in tele.sp_t.size():
		var t := tele.sp_t[j]
		if t < t_left or t > vt + 1.0: continue
		var id := tele.sp_n[j]; var x := pl.position.x + pl.size.x * (t - t_left) / window_ms; var y := pl.position.y + row * (float(id) + 0.5)
		var inh: bool = tele.kinds[id] == "I"
		(lines_i if inh else lines_e).append(Vector2(x, y - th * 0.5)); (lines_i if inh else lines_e).append(Vector2(x, y + th * 0.5))
		if vt - t < 90.0: hot.append([Vector2(x, y), inh, 1.0 - (vt - t) / 90.0])
	if lines_e.size() > 0: layer.draw_multiline(lines_e, Color(ec, 0.95), w, true)
	if lines_i.size() > 0: layer.draw_multiline(lines_i, Color(ic, 0.95), w, true)
	for h in hot: layer.draw_line(h[0] - Vector2(0, th * 0.5), h[0] + Vector2(0, th * 0.5), Color(Kit.INK, 0.5 * h[2]), w + 0.6, true)   # wet ink: the newest ticks are darker
	# the "now" edge: the pen's ruled line
	layer.draw_line(Vector2(pl.end.x - 1.0, pl.position.y), Vector2(pl.end.x - 1.0, pl.end.y), Color(Kit.INK, 0.55), 1.2)
