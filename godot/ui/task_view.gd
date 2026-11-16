class_name TaskView
extends Control
## The task world: the salt plate with its gradient and the worm, the swimming lamprey, the touch-reflex limb. Bodies are vector ribbons that
## light up with the activity of the motor pools; the field is a smooth heat map with contour lines. Follows `Telemetry.body`, `Telemetry.score`.
var tele: Telemetry = null
var _tid := ""
var _view := "polyline"           # Task::view(): which generic body renderer to use for tasks without their own drawing here
var _episode := -1
var _tex: ImageTexture = null
var _contours: Array = []        # PackedVector2Array in world coordinates
var _bounds := Rect2()
var _trail := PackedVector2Array()
var _has_field := false
var _t0 := 0.0
var _hist_t := PackedFloat32Array()   # worm: distance-to-peak history
var _hist_v := PackedFloat32Array()
var _wake := PackedVector2Array()     # lamprey: where the tail has been

func _ready() -> void: clip_contents = true; custom_minimum_size = Vector2(320, 200)
func min_text_px() -> float: return float(Kit.fs(14)) * Kit.ppl

func attach(t: Telemetry) -> void:
	tele = t; _episode = -1; _tid = ""; queue_redraw()

func _process(dt: float) -> void:
	if tele == null or not is_visible_in_tree(): return
	if tele.task != null and (tele.episode != _episode or _tid != str(tele.task.id())): _rebuild()
	if tele.task != null and tele.running and not tele.finished and tele.body.has("points"):
		var pts: PackedVector2Array = tele.body["points"]
		if pts.size() > 0 and (_trail.is_empty() or _trail[_trail.size() - 1].distance_to(pts[0]) > _bounds.size.length() * 0.004):
			_trail.append(pts[0])
			if _trail.size() > 700: _trail = _trail.slice(100)
	if tele.task != null and tele.running and not tele.finished and tele.body.has("points"):
		var ro: Dictionary = tele.body.get("readouts", {}); var pts2: PackedVector2Array = tele.body["points"]
		if ro.has("dist_mm") and (_hist_t.is_empty() or tele.t_ms - _hist_t[_hist_t.size() - 1] * 1000.0 >= 400.0):
			_hist_t.append(tele.t_ms * 0.001); _hist_v.append(float(ro["dist_mm"]))
		if _tid == "lamprey" and pts2.size() > 1 and (_wake.is_empty() or _wake[_wake.size() - 1].distance_to(pts2[pts2.size() - 1]) > 0.03):
			_wake.append(pts2[pts2.size() - 1])
			if _wake.size() > 260: _wake = _wake.slice(60)
	queue_redraw()

func _rebuild() -> void:
	_episode = tele.episode; _tid = str(tele.task.id()); _view = str(Campaign.info(_tid)["view"]); _trail = PackedVector2Array(); _contours = []; _tex = null; _hist_t = PackedFloat32Array(); _hist_v = PackedFloat32Array(); _wake = PackedVector2Array()
	_bounds = tele.task.bounds()
	var n := 72; var vals := PackedFloat32Array(); vals.resize(n * n)
	var lo := 1e9; var hi := -1e9
	for j in n:
		for i in n:
			var v := float(tele.task.field(_bounds.position.x + _bounds.size.x * (float(i) + 0.5) / n, _bounds.position.y + _bounds.size.y * (float(j) + 0.5) / n))
			vals[j * n + i] = v; lo = minf(lo, v); hi = maxf(hi, v)
	_has_field = hi - lo > 1e-5
	if not _has_field: return
	var img := Image.create(n, n, false, Image.FORMAT_RGBA8)
	var cx := _bounds.get_center(); var rad := minf(_bounds.size.x, _bounds.size.y) * 0.5
	for j in n:
		for i in n:
			var c := clampf((vals[j * n + i] - lo) / (hi - lo), 0.0, 1.0)
			var wx := _bounds.position.x + _bounds.size.x * (float(i) + 0.5) / n; var wy := _bounds.position.y + _bounds.size.y * (float(j) + 0.5) / n
			var inside := Vector2(wx, wy).distance_to(cx) <= rad
			var col := _ramp(c); if not inside: col.a = 0.0
			img.set_pixel(i, n - 1 - j, col)   # world y is up
	_tex = ImageTexture.create_from_image(img)
	for lv in [0.12, 0.28, 0.44, 0.6, 0.76, 0.9]: _contours.append(_marching(vals, n, lo + (hi - lo) * lv))

static func _ramp(c: float) -> Color:
	var stops := [[0.0, Color("#0a1a33", 0.0)], [0.2, Color("#10345e", 0.55)], [0.5, Color("#1b7f95", 0.75)], [0.8, Color("#5fd9c0", 0.9)], [1.0, Color("#eafff6", 1.0)]]
	for k in range(stops.size() - 1):
		if c <= stops[k + 1][0]:
			return (stops[k][1] as Color).lerp(stops[k + 1][1], (c - stops[k][0]) / (stops[k + 1][0] - stops[k][0]))
	return stops[stops.size() - 1][1]

func _marching(vals: PackedFloat32Array, n: int, lv: float) -> PackedVector2Array:
	var out := PackedVector2Array()
	var sx := _bounds.size.x / n; var sy := _bounds.size.y / n
	for j in n - 1:
		for i in n - 1:
			var v00 := vals[j * n + i]; var v10 := vals[j * n + i + 1]; var v11 := vals[(j + 1) * n + i + 1]; var v01 := vals[(j + 1) * n + i]
			var code := (1 if v00 > lv else 0) | (2 if v10 > lv else 0) | (4 if v11 > lv else 0) | (8 if v01 > lv else 0)
			if code == 0 or code == 15: continue
			var x0 := _bounds.position.x + (i + 0.5) * sx; var y0 := _bounds.position.y + (j + 0.5) * sy
			var e := {}   # edge crossing points: 0 bottom(y0), 1 right, 2 top(y1), 3 left
			e[0] = Vector2(x0 + sx * _t(v00, v10, lv), y0); e[1] = Vector2(x0 + sx, y0 + sy * _t(v10, v11, lv))
			e[2] = Vector2(x0 + sx * _t(v01, v11, lv), y0 + sy); e[3] = Vector2(x0, y0 + sy * _t(v00, v01, lv))
			var segs: Array = [[], [[3, 0]], [[0, 1]], [[3, 1]], [[1, 2]], [[3, 0], [1, 2]], [[0, 2]], [[3, 2]], [[2, 3]], [[0, 2]], [[0, 3], [1, 2]], [[1, 2]], [[1, 3]], [[0, 1]], [[3, 0]], []][code]
			for sg in segs: out.append(e[sg[0]]); out.append(e[sg[1]])
	return out

static func _t(a: float, b: float, lv: float) -> float: return clampf((lv - a) / (b - a), 0.0, 1.0) if absf(b - a) > 1e-9 else 0.5

# ---------- drawing ----------
var _view_c := Vector2.ZERO       # world point at the view centre
var _view_s := 1.0                # px per world unit
var _flip := Vector2(1, -1)

func w2s(p: Vector2) -> Vector2: return size * 0.5 + Vector2((p.x - _view_c.x) * _view_s, -(p.y - _view_c.y) * _view_s)

func _draw() -> void:
	var fz := Kit.fs(14); var font := Fonts.regular
	draw_style_box(Kit.sb(Color("#081019"), Kit.LINE, 1, 12, 0, false), Rect2(Vector2.ZERO, size))
	if tele == null or tele.task == null or (_bounds.size == Vector2.ZERO and not BodyViews.KINDS.has(_view)):
		draw_string(font, Vector2(0, size.y * 0.5), "Free run: no task selected. Pick a task to see its world here.", HORIZONTAL_ALIGNMENT_CENTER, size.x, Kit.fs(18), Kit.TEXT_SOFT); return
	if BodyViews.KINDS.has(_view) and not ["worm", "lamprey", "reflex"].has(_tid):   # a v0.2 body view: drawn from the task's readouts
		_draw_body_view(font, fz); return
	var id := _tid; var pts: PackedVector2Array = tele.body.get("points", PackedVector2Array()); var ro: Dictionary = tele.body.get("readouts", {})
	var margin := 26.0; var inner := Rect2(Vector2(margin, margin + 22.0), size - Vector2(margin * 2.0, margin * 2.0 + 30.0))
	var follow := id == "lamprey" and pts.size() > 1
	if follow:
		var bl := 0.0
		for i in range(pts.size() - 1): bl += pts[i].distance_to(pts[i + 1])
		_view_s = inner.size.x / (2.7 * maxf(bl, 0.01)); _view_c = pts[0] + Vector2(-0.9 * bl, 0)
		_view_c.x = clampf(_view_c.x, _bounds.position.x + inner.size.x * 0.5 / _view_s, _bounds.end.x - inner.size.x * 0.5 / _view_s) if _bounds.size.x * _view_s > inner.size.x else _bounds.get_center().x
		_view_c.y = _bounds.get_center().y
		_view_c = Vector2(_view_c.x, _view_c.y)
	else:
		_view_s = minf(inner.size.x / _bounds.size.x, inner.size.y / _bounds.size.y); _view_c = _bounds.get_center()
		if id == "reflex": _view_c = Vector2(_bounds.get_center().x, _bounds.get_center().y)
	# backdrop per task
	match id:
		"worm": _draw_plate(font, fz)
		"lamprey": _draw_water(font, fz); _draw_wake()
		_: _draw_bench_world(font, fz, ro)
	# body
	if pts.size() > 1:
		match id:
			"worm": _draw_worm(pts, ro)
			"lamprey": _draw_lamprey(pts, ro)
			"reflex": _draw_limb(pts, ro)
	# HUD
	var t := tele.t_ms * 0.001; var dur := tele.duration() * 0.001
	draw_string(font, Vector2(14, 22), str(tele.task.title()).to_upper(), HORIZONTAL_ALIGNMENT_LEFT, size.x * 0.6, fz, Kit.TEXT_SOFT)
	draw_string(font, Vector2(size.x - 190, 22), "t %5.1f / %.0f s" % [t, dur], HORIZONTAL_ALIGNMENT_RIGHT, 176, fz, Kit.TEXT)
	var pw := size.x - 28.0
	draw_rect(Rect2(14, size.y - 14, pw, 3), Color(Kit.LINE, 0.8))
	draw_rect(Rect2(14, size.y - 14, pw * clampf(t / maxf(dur, 0.001), 0, 1), 3), Color(Kit.exc(), 0.9))

func _draw_body_view(font: Font, fz: int) -> void:
	var margin := 14.0
	var r := Rect2(Vector2(margin, 34.0), size - Vector2(margin * 2.0, 34.0 + 22.0))
	if _view == "dashboard": r = Rect2(Vector2(margin, 30.0), size - Vector2(margin * 2.0, 30.0 + 18.0))
	BodyViews.draw(_view, self, tele, r)
	var t := tele.t_ms * 0.001; var dur := tele.duration() * 0.001
	draw_string(font, Vector2(14, 22), str(tele.task.title()).to_upper(), HORIZONTAL_ALIGNMENT_LEFT, size.x * 0.6, fz, Kit.TEXT_SOFT)
	draw_string(font, Vector2(size.x - 190, 22), "t %5.1f / %.0f s" % [t, dur], HORIZONTAL_ALIGNMENT_RIGHT, 176, fz, Kit.TEXT)
	var pw := size.x - 28.0
	draw_rect(Rect2(14, size.y - 14, pw, 3), Color(Kit.LINE, 0.8))
	draw_rect(Rect2(14, size.y - 14, pw * clampf(t / maxf(dur, 0.001), 0, 1), 3), Color(Kit.exc(), 0.9))

func _draw_plate(font: Font, fz: int) -> void:
	var c := w2s(_bounds.get_center()); var rad := minf(_bounds.size.x, _bounds.size.y) * 0.5 * _view_s
	Gfx.glow(self, c, rad * 1.18, Color("#1a3a5c", 0.35))
	draw_circle(c, rad, Color("#0b1626"))
	if _tex != null: draw_texture_rect(_tex, Rect2(c - Vector2(rad, rad), Vector2(rad, rad) * 2.0), false)
	for cl in _contours:
		var seg: PackedVector2Array = cl
		for i in range(0, seg.size() - 1, 2): draw_line(w2s(seg[i]), w2s(seg[i + 1]), Color(0.85, 1, 0.95, 0.20), 1.0, true)
	draw_arc(c, rad, 0, TAU, 96, Color("#2c4a73"), 2.5, true); draw_arc(c, rad + 5.0, 0, TAU, 96, Color("#2c4a73", 0.35), 1.2, true)
	# peak marker
	var ro: Dictionary = tele.body.get("readouts", {})
	if ro.has("peak_x"):
		var pk := w2s(Vector2(float(ro["peak_x"]), float(ro["peak_y"])))
		draw_arc(pk, 9.0, 0, TAU, 24, Color(1, 1, 1, 0.8), 1.4, true)
		draw_line(pk - Vector2(14, 0), pk + Vector2(14, 0), Color(1, 1, 1, 0.55), 1.2, true); draw_line(pk - Vector2(0, 14), pk + Vector2(0, 14), Color(1, 1, 1, 0.55), 1.2, true)
		draw_string(font, pk + Vector2(14, -12), "salt peak", HORIZONTAL_ALIGNMENT_LEFT, 120, fz, Color(1, 1, 1, 0.55))
	_draw_worm_charts(font, fz, c, rad)
	# trail
	if _trail.size() > 2:
		var m := _trail.size(); var chunks := 8
		for k in chunks:
			var a := m * k / chunks; var b := mini(m, m * (k + 1) / chunks + 1)
			var sp := PackedVector2Array()
			for i in range(a, b): sp.append(w2s(_trail[i]))
			if sp.size() > 1: draw_polyline(sp, Color(1, 1, 1, 0.06 + 0.36 * float(k + 1) / chunks), 1.8, true)

## The empty sides of the plate: the two sensory cells as level bars on the left, the distance to the salt peak over time on the right.
func _draw_worm_charts(font: Font, fz: int, c: Vector2, rad: float) -> void:
	var ro: Dictionary = tele.body.get("readouts", {})
	var left_w := c.x - rad - 24.0
	if left_w > 110.0:
		var x0 := 28.0; var top := 70.0; var h := size.y - 150.0
		for k in 2:
			var hz := float(ro.get("ASEL_hz" if k == 0 else "ASER_hz", 5.0)); var col := Kit.exc() if k == 0 else Kit.inh()
			var bx := x0 + k * 62.0; var u := clampf((hz - 5.0) / 400.0, 0.0, 1.0)
			draw_rect(Rect2(bx, top, 22, h), Color(Kit.LINE, 0.55))
			Gfx.glow(self, Vector2(bx + 11, top + h * (1.0 - u)), 26.0, Color(col, 0.25 * u))
			draw_rect(Rect2(bx, top + h * (1.0 - u), 22, h * u), Color(col, 0.95))
			draw_string(font, Vector2(bx - 12, top + h + float(fz) + 6.0), "ASEL" if k == 0 else "ASER", HORIZONTAL_ALIGNMENT_CENTER, 46, fz, col)
			draw_string(font, Vector2(bx - 12, top - 8.0), "%d" % int(roundf(hz)), HORIZONTAL_ALIGNMENT_CENTER, 46, fz, Kit.TEXT)
	var rx := c.x + rad + 36.0; var rw := size.x - rx - 18.0
	if rw > 140.0 and _hist_t.size() > 1:
		var top := 56.0; var h := size.y - 132.0; var dmax := maxf(_bounds.size.x * 0.5, 1.0); var dur := maxf(tele.duration() * 0.001, 1.0)
		draw_rect(Rect2(rx, top, rw, h), Color(Kit.BENCH, 0.55)); draw_rect(Rect2(rx, top, rw, h), Color(Kit.LINE, 0.9), false, 1.0)
		var yt := top + h * (1.0 - 1.0 / dmax)
		Gfx.dashed(self, PackedVector2Array([Vector2(rx, yt), Vector2(rx + rw, yt)]), Color(Kit.GREEN, 0.7), 1.4, 6.0, 5.0)
		draw_string(font, Vector2(rx + rw - 110, yt + float(fz) + 4.0), "1 mm: arrived", HORIZONTAL_ALIGNMENT_RIGHT, 106, fz, Color(Kit.GREEN, 0.9))
		var pts := PackedVector2Array()
		for i in _hist_t.size(): pts.append(Vector2(rx + rw * _hist_t[i] / dur, top + h * (1.0 - clampf(_hist_v[i] / dmax, 0.0, 1.0))))
		draw_polyline(pts, Color(Kit.exc(), 0.25), 6.0, true); draw_polyline(pts, Color(Kit.exc(), 1.0), 2.0, true)
		draw_string(font, Vector2(rx, top + h + float(fz) + 6.0), "distance to peak, mm", HORIZONTAL_ALIGNMENT_LEFT, rw, fz, Kit.TEXT_SOFT)

func _draw_water(font: Font, fz: int) -> void:
	var r := Rect2(Vector2.ZERO, size)
	draw_rect(Rect2(0, 0, size.x, size.y), Color("#0a1a2c"))
	# depth gradient: lighter toward the surface
	for k in 14:
		var y0 := size.y * float(k) / 14.0
		draw_rect(Rect2(0, y0, size.x, size.y / 14.0 + 1.0), Color(0.12, 0.30, 0.46, 0.10 * (1.0 - float(k) / 14.0)))
	# drifting reference marks: every half body length, fixed in the world so the motion is visible
	var step := 1.0
	var x0 := floorf((_view_c.x - size.x * 0.5 / _view_s) / step) * step
	var t := tele.now() if tele != null else 0.0
	var rng := RandomNumberGenerator.new()
	for i in int(size.x / (step * _view_s)) + 3:
		var wx := x0 + i * step
		for j in 6:
			rng.seed = int(wx * 7.0) * 31 + j * 977
			var wy := _bounds.position.y + _bounds.size.y * rng.randf()
			var s := w2s(Vector2(wx + rng.randf() * step, wy))
			draw_circle(s, 1.4 + rng.randf() * 1.6, Color(0.6, 0.85, 1.0, 0.10 + 0.12 * rng.randf()))
	# arena edges and start line
	var tl := w2s(Vector2(_bounds.position.x, _bounds.end.y)); var br := w2s(Vector2(_bounds.end.x, _bounds.position.y))
	draw_rect(Rect2(tl, br - tl), Color("#2c4a73", 0.6), false, 1.6)
	var s0 := w2s(Vector2(0, _bounds.end.y)); var s1 := w2s(Vector2(0, _bounds.position.y))
	Gfx.dashed(self, PackedVector2Array([s0, s1]), Color(1, 1, 1, 0.16), 1.2, 6.0, 8.0)
	draw_string(font, s0 + Vector2(6, float(fz) + 28.0), "start", HORIZONTAL_ALIGNMENT_LEFT, 80, fz, Color(1, 1, 1, 0.4))
	# scale bar: one body length
	var ro: Dictionary = tele.body.get("readouts", {})
	var sb := Vector2(size.x - 20.0 - _view_s, size.y - 34.0)
	draw_line(sb, sb + Vector2(_view_s, 0), Color(1, 1, 1, 0.55), 2.0); draw_line(sb - Vector2(0, 4), sb + Vector2(0, 4), Color(1, 1, 1, 0.55), 2.0); draw_line(sb + Vector2(_view_s, -4), sb + Vector2(_view_s, 4), Color(1, 1, 1, 0.55), 2.0)
	draw_string(font, sb + Vector2(0, -8), "1 body length", HORIZONTAL_ALIGNMENT_LEFT, 200, fz, Color(1, 1, 1, 0.6))
	if ro.has("speed"): draw_string(font, Vector2(14, size.y - 24), "speed %.2f BL/s" % float(ro["speed"]), HORIZONTAL_ALIGNMENT_LEFT, 240, fz, Kit.TEXT)

func _draw_wake() -> void:
	if _wake.size() < 3: return
	var m := _wake.size(); var chunks := 10
	for k in chunks:
		var a := m * k / chunks; var b := mini(m, m * (k + 1) / chunks + 1); var sp := PackedVector2Array()
		for i in range(a, b): sp.append(w2s(_wake[i]))
		if sp.size() > 1: draw_polyline(sp, Color(0.7, 0.9, 1.0, 0.03 + 0.2 * float(k + 1) / chunks), 1.6, true)

func _draw_bench_world(font: Font, fz: int, ro: Dictionary) -> void:
	# the reflex rig: a skin line and the labelled stimulus
	var a := w2s(Vector2(_bounds.position.x, _bounds.position.y)); var b := w2s(Vector2(_bounds.end.x, _bounds.end.y))
	for k in 10:
		var y := lerpf(a.y, b.y, float(k) / 9.0); draw_line(Vector2(size.x * 0.06, y), Vector2(size.x * 0.94, y), Color(Kit.LINE, 0.18), 1.0)

func _width_u(u: float, kind: String) -> float:
	match kind:
		"worm": return (0.3 + 0.7 * sqrt(clampf(sin(PI * clampf(u * 0.9 + 0.06, 0.0, 1.0)), 0.0, 1.0))) * (1.0 - 0.55 * u * u)
		"fish": return (0.55 + 0.45 * sin(PI * clampf(pow(u, 0.7) * 0.95, 0.0, 1.0))) * (1.0 - 0.72 * pow(u, 1.5)) if u > 0.04 else 0.5
	return 1.0

func _screen_pts(pts: PackedVector2Array, mag := 1.0) -> PackedVector2Array:
	var out := PackedVector2Array(); var h := w2s(pts[0])
	for p in pts: out.append(h + (w2s(p) - h) * mag)
	return out

func _draw_worm(pts: PackedVector2Array, ro: Dictionary) -> void:
	var sp0 := _screen_pts(pts); var len := 0.0
	for i in range(sp0.size() - 1): len += sp0[i].distance_to(sp0[i + 1])
	var target := minf(size.x, size.y) * 0.17; var mag := clampf(target / maxf(len, 1.0), 1.0, 6.0)
	var sp := Gfx.cr_spline(_screen_pts(pts, mag), 5); var W := minf(size.x, size.y) * 0.022
	var hwf := func(u): return W * _width_u(u, "worm")
	draw_polyline(sp, Color(Gfx.BODY, 0.10), W * 3.4, true)
	Gfx.tube_rim(self, sp, hwf, Color("#123436"), Color(Gfx.BODY, 0.95), 1.6)
	Gfx.tube(self, sp, func(u): return W * _width_u(u, "worm") * 0.8, Color("#123436").lerp(Gfx.BODY, 0.2))
	draw_polyline(sp, Color(Gfx.BODY, 0.38), W * 0.55, true)
	# head with the two chemosensory cells
	var h := sp[0]; var dir := (sp[0] - sp[mini(6, sp.size() - 1)]).normalized(); var nrm := dir.orthogonal()
	var l := clampf((float(ro.get("ASEL_hz", 5.0)) - 5.0) / 150.0, 0.0, 1.0); var r := clampf((float(ro.get("ASER_hz", 5.0)) - 5.0) / 150.0, 0.0, 1.0)
	var pl := h + dir * W * 0.9 - nrm * W * 0.55; var pr := h + dir * W * 0.9 + nrm * W * 0.55   # left of heading = -orthogonal (screen y down)
	Gfx.glow(self, pl, W * (2.2 + 3.0 * l), Color(Kit.exc(), 0.2 + 0.7 * l)); Gfx.glow(self, pr, W * (2.2 + 3.0 * r), Color(Kit.inh(), 0.2 + 0.7 * r))
	draw_circle(pl, W * 0.34, Kit.exc().lerp(Color.WHITE, l * 0.6)); draw_circle(pr, W * 0.34, Kit.inh().lerp(Color.WHITE, r * 0.6))
	var f := Fonts.regular; var fz := Kit.fs(14)
	draw_string(f, Vector2(14, size.y - 24), "worm magnified ×%.0f   ASEL %d Hz   ASER %d Hz   dist %.1f mm" % [mag, int(float(ro.get("ASEL_hz", 0))), int(float(ro.get("ASER_hz", 0))), float(ro.get("dist_mm", 0))], HORIZONTAL_ALIGNMENT_LEFT, size.x - 28, fz, Kit.TEXT_SOFT)

func _side_poly(sp: PackedVector2Array, hw: Callable, side: float) -> PackedVector2Array:
	var edge := PackedVector2Array(); var n := sp.size()
	for i in n:
		var t := sp[mini(i + 1, n - 1)] - sp[maxi(i - 1, 0)]; var nrm := t.orthogonal().normalized() if t.length() > 0.001 else Vector2.UP
		edge.append(sp[i] + nrm * side * float(hw.call(float(i) / float(maxi(n - 1, 1)))))
	var back := sp.duplicate(); back.reverse(); edge.append_array(back); return edge

func _draw_lamprey(pts: PackedVector2Array, ro: Dictionary) -> void:
	var sp := Gfx.cr_spline(_screen_pts(pts), 5)
	var bl := 0.0
	for i in range(pts.size() - 1): bl += pts[i].distance_to(pts[i + 1])
	var W := bl * _view_s * 0.058
	var hw := func(u): return W * _width_u(u, "fish")
	# tail fin and dorsal fin
	var tip := sp[sp.size() - 1]; var tdir := (sp[sp.size() - 1] - sp[maxi(sp.size() - 5, 0)]).normalized(); var tn := tdir.orthogonal()
	draw_colored_polygon(PackedVector2Array([tip - tdir * W * 0.4, tip + tdir * W * 1.5 + tn * W * 1.6, tip + tdir * W * 1.0, tip + tdir * W * 1.5 - tn * W * 1.6]), Color(Gfx.FISH, 0.20))
	draw_polyline(sp, Color(Gfx.FISH, 0.10), W * 4.0, true)
	Gfx.tube_rim(self, sp, hw, Color("#0f2236"), Color(Gfx.FISH, 0.95), 1.5)
	Gfx.tube(self, sp, func(u): return W * _width_u(u, "fish") * 0.86, Color("#0f2236").lerp(Gfx.FISH, 0.2))
	# muscle activation: the flank that is contracting glows (left motor pool: left side of travel)
	var rl := clampf(float(ro.get("rate_left", 0.0)) / 40.0, 0.0, 1.0); var rr := clampf(float(ro.get("rate_right", 0.0)) / 40.0, 0.0, 1.0)
	Gfx.tube(self, sp, func(u): return W * _width_u(u, "fish") * 0.95, Color("#17314a").lerp(Kit.exc(), 0.10 + 0.78 * rl), -1.0)
	Gfx.tube(self, sp, func(u): return W * _width_u(u, "fish") * 0.95, Color("#17314a").lerp(Kit.inh(), 0.10 + 0.78 * rr), 1.0)
	# myomere chevrons
	for k in range(1, 22):
		var i := int(float(k) / 22.0 * (sp.size() - 1)); var u := float(i) / float(sp.size() - 1)
		var t := sp[mini(i + 1, sp.size() - 1)] - sp[maxi(i - 1, 0)]; var n := t.orthogonal().normalized(); var w := W * _width_u(u, "fish")
		if w > 1.5: draw_polyline(PackedVector2Array([sp[i] + n * w * 0.9, sp[i] + t.normalized() * w * 0.35, sp[i] - n * w * 0.9]), Color(1, 1, 1, 0.10), 1.0, true)
	draw_polyline(sp, Color(Gfx.FISH, 0.35), maxf(W * 0.18, 1.0), true)
	# head: eye and gill pores
	var h := sp[0]; var dir := (sp[0] - sp[mini(5, sp.size() - 1)]).normalized(); var nr := dir.orthogonal()
	for s in [-1.0, 1.0]:
		draw_circle(h + dir * W * 0.2 + nr * s * W * 0.5, W * 0.22, Color("#06101c")); draw_circle(h + dir * W * 0.2 + nr * s * W * 0.5, W * 0.1, Color(1, 1, 1, 0.9))
		for g in 5: draw_circle(sp[mini(6 + g * 3, sp.size() - 1)] + nr * s * W * 0.55, W * 0.09, Color(1, 1, 1, 0.5))

func _draw_limb(pts: PackedVector2Array, ro: Dictionary) -> void:
	var base := w2s(pts[0]); var tip := w2s(pts[pts.size() - 1]); var mid := w2s(pts[1])
	var touching := float(ro.get("touching", 0.0)) > 0.5; var wd := float(ro.get("withdrawn", 0.0))
	var W := minf(size.x, size.y) * 0.075
	# the body wall the limb grows from
	var wall := Rect2(Vector2(base.x - W * 2.6, size.y * 0.16), Vector2(W * 2.6, size.y * 0.68))
	Gfx.rounded_rect(self, wall, Color("#122338"), Color(Kit.LINE, 0.9), W * 0.6, 1.5)
	var sp := Gfx.cr_spline(PackedVector2Array([base, mid, tip]), 10)
	Gfx.glow(self, tip, W * 2.6, Color(Gfx.BODY, 0.06 + 0.4 * wd))
	Gfx.tube_rim(self, sp, func(u): return W * (1.0 - 0.45 * u), Color("#10282b"), Color(Gfx.BODY, 0.95), 2.0)
	Gfx.tube(self, sp, func(u): return W * (1.0 - 0.45 * u) * 0.85, Color("#10282b").lerp(Gfx.BODY, 0.25 + 0.4 * wd))
	draw_circle(tip, W * 0.42, Color(Gfx.BODY, 0.55))
	if touching: Gfx.glow(self, tip + Vector2(W * 0.5, 0), W * 3.0, Color(1, 1, 1, 0.35))
	# the poke
	var rest_x := w2s(Vector2(1.32, 0)).x; var probe_x := (tip.x + W * 0.9) if touching else rest_x
	var py := base.y
	draw_line(Vector2(size.x - 20, py), Vector2(probe_x + W * 0.5, py), Color(1, 1, 1, 0.55 if touching else 0.22), 3.0, true)
	draw_circle(Vector2(probe_x, py), W * 0.5, Color(1, 1, 1, 0.9 if touching else 0.3))
	if touching:
		for k in 3: draw_arc(Vector2(probe_x, py), W * (0.9 + 0.5 * k), PI * 0.75, PI * 1.25, 8, Color(1, 1, 1, 0.5 - 0.15 * k), 2.0, true)
	draw_string(Fonts.regular, Vector2(14, size.y - 24), "poke %s   afferent %d Hz   bursts %d" % ["ON" if touching else "off", int(float(ro.get("touch_rate_hz", 0.0))), int(float(ro.get("bursts", 0.0)))], HORIZONTAL_ALIGNMENT_LEFT, size.x - 28, Kit.fs(14), Kit.TEXT_SOFT)
