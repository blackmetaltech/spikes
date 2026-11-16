class_name TraceView
extends Control
## Oscilloscope: 1 to 4 membrane-potential traces (1 ms samples of V_m from NEST), each in its own lane with a graticule, a glowing phosphor line,
## and a bright spike marker at every recorded spike (NEST models reset V_m at the threshold, so the marker shows the action potential).
var tele: Telemetry = null
var window_ms := 500.0
var _layer: Control
var _range := {}    # neuron -> Vector2(lo, hi), eased

class Layer extends Control:
	var owner_view: Control
	func _draw() -> void: owner_view.call("_draw_traces", self)

func _ready() -> void:
	clip_contents = true; custom_minimum_size = Vector2(300, 120)
	_layer = Layer.new(); _layer.owner_view = self; _layer.set_anchors_preset(Control.PRESET_FULL_RECT); _layer.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var m := CanvasItemMaterial.new(); m.blend_mode = CanvasItemMaterial.BLEND_MODE_ADD; _layer.material = m; add_child(_layer)

func min_text_px() -> float: return float(Kit.fs(14)) * Kit.ppl
func attach(t: Telemetry) -> void: tele = t; _range = {}; queue_redraw()
func _process(_dt: float) -> void:
	if is_visible_in_tree(): queue_redraw(); _layer.queue_redraw()

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
	draw_style_box(Kit.sb(Kit.WELL, Kit.LINE, 1, 12, 0, false), Rect2(Vector2.ZERO, size))
	draw_string(f, Vector2(14, 20), "MEMBRANE POTENTIAL", HORIZONTAL_ALIGNMENT_LEFT, 240, fz, Kit.TEXT_SOFT)
	if tele == null or tele.sim == null: return
	var vt := tele.view_t; var t_left := vt - window_ms
	var lanes := _lanes()
	if lanes.is_empty():
		draw_string(f, Vector2(0, size.y * 0.55), "Select a neuron and press TRACE to watch its voltage.", HORIZONTAL_ALIGNMENT_CENTER, size.x, fz, Kit.TEXT_SOFT); return
	for ln in lanes:
		var r: Rect2 = ln["rect"]; var nid: int = ln["n"]
		var col: Color = Kit.inh() if _kind(nid) == "I" else Kit.exc()
		draw_rect(r, Color(Kit.BENCH, 0.55)); 
		var rg := _lane_range(nid); var lo := rg.x; var hi := rg.y
		# graticule: horizontal every 10/20 mV, vertical every 100 ms
		var step := 10.0 if hi - lo <= 60.0 else 20.0; var mv := ceilf(lo / step) * step; var last_lab_y := -1000.0
		while mv <= hi:
			var y := r.end.y - r.size.y * (mv - lo) / (hi - lo)
			draw_line(Vector2(r.position.x, y), Vector2(r.end.x, y), Color(Kit.LINE, 0.55 if int(roundf(mv / step)) % 2 == 0 else 0.3), 1.0)
			if absf(y - last_lab_y) >= float(fz) * 1.25 and y > r.position.y + float(fz) * 0.5 and y < r.end.y - float(fz) * 0.4: last_lab_y = y; draw_string(f, Vector2(4, y + float(fz) * 0.35), "%d" % int(mv), HORIZONTAL_ALIGNMENT_LEFT, r.position.x - 8, fz, Kit.TEXT_SOFT)
			mv += step
		var g0 := ceilf(t_left / 100.0) * 100.0; var tt := g0
		while tt <= vt:
			var x := r.position.x + r.size.x * (tt - t_left) / window_ms; draw_line(Vector2(x, r.position.y), Vector2(x, r.end.y), Color(Kit.LINE, 0.40), 1.0); tt += 100.0
		draw_rect(r, Color(Kit.LINE, 0.9), false, 1.0)
		var pop_i: int = tele.pop_of[nid] if nid < tele.pop_of.size() else 0
		var label := "%s  %s" % [str(tele.circuit["populations"][pop_i].get("id", "")), str(tele.circuit["populations"][pop_i].get("model", ""))]
		if tele.circuit["populations"][pop_i].get("n", 1) > 1: label += "  [%d]" % (nid - Circuits.pop_first(tele.circuit, pop_i))
		var lw := minf(f.get_string_size(label, HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x + 12.0, r.size.x * 0.7)
		draw_rect(Rect2(r.position + Vector2(2, 2), Vector2(lw, float(fz) + 6.0)), Color(Kit.BENCH, 0.78))
		draw_string(f, r.position + Vector2(8, float(fz) + 3.0), label, HORIZONTAL_ALIGNMENT_LEFT, r.size.x * 0.7, fz, col)
		var a: PackedFloat32Array = tele.volt.get(nid, PackedFloat32Array())
		if a.size() > 0:
			draw_rect(Rect2(Vector2(r.end.x - 96, r.position.y + 2), Vector2(94, float(fz) + 6.0)), Color(Kit.BENCH, 0.78))
			draw_string(f, Vector2(r.end.x - 120, r.position.y + float(fz) + 3.0), ("%.1f mV" % a[a.size() - 1]) if is_finite(a[a.size() - 1]) else "no membrane", HORIZONTAL_ALIGNMENT_RIGHT, 112, fz, Kit.TEXT)
	var tt2 := ceilf(t_left / 100.0) * 100.0
	var bot: Rect2 = lanes[lanes.size() - 1]["rect"]
	while tt2 <= vt:
		var xx := bot.position.x + bot.size.x * (tt2 - t_left) / window_ms
		draw_string(f, Vector2(xx - 30, bot.end.y + float(fz) + 3.0), "%.1f s" % (tt2 * 0.001), HORIZONTAL_ALIGNMENT_CENTER, 60, fz, Kit.TEXT_SOFT); tt2 += 100.0
	# axes caption
	draw_string(f, Vector2(size.x - 190, 20), "window %d ms" % int(window_ms), HORIZONTAL_ALIGNMENT_RIGHT, 176, fz, Kit.TEXT_SOFT)

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
			layer.draw_polyline(pts, Color(col, 0.12), 7.0, true); layer.draw_polyline(pts, Color(col, 0.28), 3.4, true); layer.draw_polyline(pts, Color(col.lightened(0.35), 0.95), 1.6, true)
		# spike markers: a vertical needle from the trace to the top of the lane
		for s in tele.sp_t.size():
			if tele.sp_n[s] != nid: continue
			var t := tele.sp_t[s]; if t < t_left or t > vt: continue
			var x := r.position.x + r.size.x * (t - t_left) / window_ms
			layer.draw_line(Vector2(x, r.end.y - r.size.y * (clampf(a[maxi(a.size() - 1 - int(tl - t), 0)] if int(tl - t) < a.size() else lo, lo, hi) - lo) / (hi - lo)), Vector2(x, r.position.y + 3.0), Color(col.lightened(0.5), 0.9), 1.8, true)
			Gfx.glow(layer, Vector2(x, r.position.y + 4.0), 9.0, Color(col, 0.35 * (1.0 - clampf((vt - t) / 160.0, 0.0, 0.8))))
		# the beam head
		if pts.size() > 0: Gfx.glow(layer, pts[pts.size() - 1], 11.0, Color(col, 0.55)); layer.draw_circle(pts[pts.size() - 1], 2.4, Color(1, 1, 1, 0.9))

func _clip_x(pts: PackedVector2Array, r: Rect2) -> PackedVector2Array:
	var out := PackedVector2Array()
	for p in pts:
		if p.x >= r.position.x - 0.5 and p.x <= r.end.x + 0.5: out.append(p)
	return out
