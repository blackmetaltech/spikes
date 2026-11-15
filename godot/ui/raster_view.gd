class_name RasterView
extends Control
## Scrolling spike raster: one row per neuron grouped by population, ticks coloured amber (excitatory) / cyan (inhibitory), drawn additively so
## dense firing glows. Scrolls smoothly on `Telemetry.view_t` (continuous), with a 100 ms grid and a bright "now" edge.
var tele: Telemetry = null
var window_ms := 500.0
var _layer: Control

class Layer extends Control:
	var owner_view: Control
	func _draw() -> void: owner_view.call("_draw_spikes", self)

func _ready() -> void:
	clip_contents = true; custom_minimum_size = Vector2(300, 150)
	_layer = Layer.new(); _layer.owner_view = self; _layer.set_anchors_preset(Control.PRESET_FULL_RECT); _layer.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var m := CanvasItemMaterial.new(); m.blend_mode = CanvasItemMaterial.BLEND_MODE_ADD; _layer.material = m; add_child(_layer)

func min_text_px() -> float: return float(Kit.fs(14)) * Kit.ppl
func attach(t: Telemetry) -> void: tele = t; queue_redraw()
func _process(_dt: float) -> void:
	if is_visible_in_tree(): queue_redraw(); _layer.queue_redraw()

func _plot() -> Rect2:
	var fz := float(Kit.fs(14)); return Rect2(Vector2(fz * 3.4, 26.0), Vector2(size.x - fz * 3.4 - 12.0, size.y - 26.0 - fz - 12.0))

func _draw() -> void:
	var fz := Kit.fs(14); var f := Fonts.regular; var pl := _plot()
	draw_style_box(Kit.sb(Kit.WELL, Kit.LINE, 1, 12, 0, false), Rect2(Vector2.ZERO, size))
	draw_string(f, Vector2(14, 20), "SPIKE RASTER", HORIZONTAL_ALIGNMENT_LEFT, 200, fz, Kit.TEXT_SOFT)
	var vt := tele.view_t if tele != null else 0.0
	# 100 ms grid, scrolling with time
	var t_left := vt - window_ms; var g0 := ceilf(t_left / 100.0) * 100.0; var t := g0
	while t <= vt:
		var x := pl.position.x + pl.size.x * (t - t_left) / window_ms
		draw_line(Vector2(x, pl.position.y), Vector2(x, pl.end.y), Color(Kit.LINE, 0.55 if int(roundf(t / 100.0)) % 5 != 0 else 0.9), 1.0)
		var lab := "%.1f s" % (t * 0.001)
		draw_string(f, Vector2(x - 30, pl.end.y + float(fz) + 3.0), lab, HORIZONTAL_ALIGNMENT_CENTER, 60, fz, Kit.TEXT_SOFT)
		t += 100.0
	draw_string(f, Vector2(size.x - 260, 20), "%d ms window, model time" % int(window_ms) if size.x > 520 else "%d ms" % int(window_ms), HORIZONTAL_ALIGNMENT_RIGHT, 246, fz, Kit.TEXT_SOFT)
	if tele == null or tele.n == 0: return
	# population groups
	var n := tele.n; var row := pl.size.y / float(n); var c := tele.circuit; var first := 0
	var pops: Array = c.get("populations", [])
	for pi in pops.size():
		var cnt := Circuits.pop_n(pops[pi]); var yc := pl.position.y + row * (float(first) + float(cnt) * 0.5)
		if pi > 0: draw_line(Vector2(pl.position.x - 6, pl.position.y + row * first), Vector2(pl.end.x, pl.position.y + row * first), Color(Kit.LINE, 0.9), 1.0)
		if cnt * row >= float(fz) * 0.9 or n <= 16: draw_string(f, Vector2(6, yc + float(fz) * 0.35), str(pops[pi].get("id", "")), HORIZONTAL_ALIGNMENT_LEFT, pl.position.x - 10, fz, Kit.TEXT_SOFT)
		first += cnt
	draw_rect(pl, Color(Kit.LINE, 0.8), false, 1.0)

func _draw_spikes(layer: Control) -> void:
	if tele == null or tele.n == 0: return
	var pl := _plot(); var n := tele.n; var row := pl.size.y / float(n); var vt := tele.view_t; var t_left := vt - window_ms
	var th := minf(row * 0.78, 20.0); var w := 2.2
	var ec := Kit.exc(); var ic := Kit.inh()
	var lines_e := PackedVector2Array(); var lines_i := PackedVector2Array(); var hot := []
	for j in tele.sp_t.size():
		var t := tele.sp_t[j]
		if t < t_left or t > vt + 1.0: continue
		var id := tele.sp_n[j]; var x := pl.position.x + pl.size.x * (t - t_left) / window_ms; var y := pl.position.y + row * (float(id) + 0.5)
		var inh: bool = tele.kinds[id] == "I"
		(lines_i if inh else lines_e).append(Vector2(x, y - th * 0.5)); (lines_i if inh else lines_e).append(Vector2(x, y + th * 0.5))
		if vt - t < 90.0: hot.append([Vector2(x, y), inh, 1.0 - (vt - t) / 90.0])
	if lines_e.size() > 0: layer.draw_multiline(lines_e, Color(ec, 0.80), w, true)
	if lines_i.size() > 0: layer.draw_multiline(lines_i, Color(ic, 0.80), w, true)
	for h in hot: Gfx.glow(layer, h[0], 10.0 + 5.0 * h[2], Color(ic if h[1] else ec, 0.32 * h[2]))
	# the "now" edge
	for k in 10: layer.draw_line(Vector2(pl.end.x - k * 2.0, pl.position.y), Vector2(pl.end.x - k * 2.0, pl.end.y), Color(1, 1, 1, 0.07 * (1.0 - float(k) / 10.0)), 2.0)
