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
func _process(dt: float) -> void:
	if not is_visible_in_tree(): return
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
	if tele == null or tele.n == 0: return
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
