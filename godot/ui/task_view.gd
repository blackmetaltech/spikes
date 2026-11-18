class_name TaskView
extends Control
## The task world, drawn as specimen plates: the stippled petri dish with its salt gradient and the worm, the swimming lamprey, the touch-reflex limb. Bodies
## are inked ribbons washed with the stain of the active motor pools; the salt is stippling, denser toward the peak, with faint inked contours.
## Follows `Telemetry.body`, `Telemetry.score`.
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
var _stip := PackedVector2Array()     # worm: salt stipple dots in world coordinates (denser toward the peak)

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
	_episode = tele.episode; _tid = str(tele.task.id()); _view = str(Campaign.info(_tid)["view"]); _trail = PackedVector2Array(); _contours = []; _tex = null; _hist_t = PackedFloat32Array(); _hist_v = PackedFloat32Array(); _wake = PackedVector2Array(); _stip = PackedVector2Array()
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
	var rng := RandomNumberGenerator.new(); rng.seed = 4242   # stipple: dots dropped with probability rising steeply with the salt level
	for j in n:
		for i in n:
			var c2 := clampf((vals[j * n + i] - lo) / (hi - lo), 0.0, 1.0)
			for k in 3:
				if rng.randf() < pow(c2, 1.6) * 0.75:
					var wp := Vector2(_bounds.position.x + _bounds.size.x * (float(i) + rng.randf()) / n, _bounds.position.y + _bounds.size.y * (float(j) + rng.randf()) / n)
					if wp.distance_to(cx) <= rad * 0.97: _stip.append(wp)
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
	draw_rect(Rect2(Vector2.ZERO, size), Kit.CARD)
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
	_hud(fz)

func _draw_body_view(font: Font, fz: int) -> void:
	var margin := 14.0
	var r := Rect2(Vector2(margin, 34.0), size - Vector2(margin * 2.0, 34.0 + 22.0))
	if _view == "dashboard": r = Rect2(Vector2(margin, 30.0), size - Vector2(margin * 2.0, 30.0 + 18.0))
	BodyViews.draw(_view, self, tele, r)
	_hud(fz)

func _draw_plate(font: Font, fz: int) -> void:
	var c := w2s(_bounds.get_center()); var rad := minf(_bounds.size.x, _bounds.size.y) * 0.5 * _view_s
	# the petri dish: a shadow, the glass rim as a double ink ring, the agar a shade lighter than the page
	draw_circle(c + Vector2(4, 5), rad + 6.0, Color(Kit.INK, 0.08))
	draw_circle(c, rad + 6.0, Color("#FBF7EE")); draw_circle(c, rad, Gfx.SHEET)
	# salt: verdigris stippling, denser up the gradient
	if _stip.size() > 0:
		var dots := PackedVector2Array(); dots.resize(_stip.size() * 2); var d := clampf(rad / 140.0, 1.0, 2.0)
		for i in _stip.size(): var q := w2s(_stip[i]); dots[i * 2] = q; dots[i * 2 + 1] = q + Vector2(d * 0.6, d * 0.4)
		draw_multiline(dots, Color(Kit.SENSOR, 0.75), d * 1.3, true)
	for cl in _contours:
		var seg: PackedVector2Array = cl; var lines := PackedVector2Array()
		for i in range(0, seg.size() - 1, 2): lines.append(w2s(seg[i])); lines.append(w2s(seg[i + 1]))
		if lines.size() > 1: draw_multiline(lines, Color(Kit.SENSOR, 0.22), 1.0, true)
	draw_arc(c, rad, 0, TAU, 128, Color(Kit.INK, 0.9), 2.2, true); draw_arc(c, rad + 6.0, 0, TAU, 128, Color(Kit.INK, 0.6), 1.2, true)
	draw_arc(c, rad - 4.0, PI * 1.05, PI * 1.45, 24, Color(Color.WHITE, 0.6), 2.0, true)   # a glint on the glass
	# peak marker: an inked cross and an italic call-out
	var ro: Dictionary = tele.body.get("readouts", {})
	if ro.has("peak_x"):
		var pk := w2s(Vector2(float(ro["peak_x"]), float(ro["peak_y"])))
		draw_line(pk - Vector2(8, 0), pk + Vector2(8, 0), Kit.INK, 1.2, true); draw_line(pk - Vector2(0, 8), pk + Vector2(0, 8), Kit.INK, 1.2, true)
		Gfx.callout(self, pk + Vector2(3, -3), pk + Vector2(-34, -26), "salt", fz)
	_draw_worm_charts(font, fz, c, rad)
	# trail: a dotted ink path, darker toward the present
	if _trail.size() > 2:
		var m := _trail.size(); var chunks := 8
		for k in chunks:
			var a2 := m * k / chunks; var b := mini(m, m * (k + 1) / chunks + 1)
			var sp := PackedVector2Array()
			for i in range(a2, b): sp.append(w2s(_trail[i]))
			if sp.size() > 1: Gfx.dashed(self, sp, Color(Kit.INK, 0.15 + 0.6 * float(k + 1) / chunks), 1.5, 2.5, 3.5)

## The empty sides of the plate: the two sensory cells as level bars on the left, the distance to the salt peak over time on the right.
func _draw_worm_charts(font: Font, fz: int, c: Vector2, rad: float) -> void:
	var ro: Dictionary = tele.body.get("readouts", {}); var fi := Gfx.serif_italic(); var mo := Gfx.mono()
	var left_w := c.x - rad - 24.0
	if left_w > 110.0:   # the two sensory cells as engraved bars, hatched in their stain
		var x0 := 28.0; var top := 70.0; var h := size.y - 150.0
		for k in 2:
			var hz := float(ro.get("ASEL_hz" if k == 0 else "ASER_hz", 5.0)); var col := Kit.exc() if k == 0 else Kit.inh()
			var bx := x0 + k * 62.0; var u := clampf((hz - 5.0) / 400.0, 0.0, 1.0)
			var fr := Rect2(bx, top + h * (1.0 - u), 22, h * u)
			draw_rect(Rect2(bx, top, 22, h), Color("#FBF7EE"))
			if fr.size.y > 0.5:
				draw_rect(fr, Color(col, 0.18)); Gfx.hatch(self, PackedVector2Array([fr.position, Vector2(fr.end.x, fr.position.y), fr.end, Vector2(fr.position.x, fr.end.y)]), -PI * 0.25, 3.5, col, 1.3)
			draw_rect(Rect2(bx, top, 22, h), Color(Kit.INK, 0.85), false, 1.0)
			draw_string(fi, Vector2(bx - 12, top + h + float(fz) + 6.0), "ASEL" if k == 0 else "ASER", HORIZONTAL_ALIGNMENT_CENTER, 46, fz, col)
			draw_string(mo, Vector2(bx - 12, top - 8.0), "%d" % int(roundf(hz)), HORIZONTAL_ALIGNMENT_CENTER, 46, fz, Kit.TEXT)
	var rx := c.x + rad + 36.0; var rw := size.x - rx - 18.0
	if rw > 140.0 and _hist_t.size() > 1:   # distance to the peak: a pen line on bare axes
		var top := 56.0; var h := size.y - 132.0; var dmax := maxf(_bounds.size.x * 0.5, 1.0); var dur := maxf(tele.duration() * 0.001, 1.0)
		draw_line(Vector2(rx, top), Vector2(rx, top + h), Kit.INK, 1.2); draw_line(Vector2(rx, top + h), Vector2(rx + rw, top + h), Kit.INK, 1.2)
		draw_string(fi, Vector2(rx, top - 10.0), "distance to peak, mm", HORIZONTAL_ALIGNMENT_LEFT, rw, fz, Kit.TEXT_SOFT)
		var yt := top + h * (1.0 - 1.0 / dmax)
		Gfx.dashed(self, PackedVector2Array([Vector2(rx, yt), Vector2(rx + rw, yt)]), Color(Kit.GREEN, 0.8), 1.3, 5.0, 4.0)
		draw_string(fi, Vector2(rx + rw - 110, yt - 5.0), "arrived", HORIZONTAL_ALIGNMENT_RIGHT, 106, fz, Kit.GREEN)
		var pts := PackedVector2Array()
		for i in _hist_t.size(): pts.append(Vector2(rx + rw * _hist_t[i] / dur, top + h * (1.0 - clampf(_hist_v[i] / dmax, 0.0, 1.0))))
		draw_polyline(pts, Kit.exc(), 1.8, true)
		draw_string(mo, Vector2(rx - 4, top + h + float(fz) + 6.0), "0", HORIZONTAL_ALIGNMENT_LEFT, 40, fz, Kit.TEXT_SOFT)
		draw_string(mo, Vector2(rx + rw - 80, top + h + float(fz) + 6.0), "%d s" % int(dur), HORIZONTAL_ALIGNMENT_RIGHT, 80, fz, Kit.TEXT_SOFT)

func _draw_water(font: Font, fz: int) -> void:
	# the tank: paper with engraved ripple strokes fixed in the world (so the motion is visible), the arena ruled in ink
	var step := 1.0
	var x0 := floorf((_view_c.x - size.x * 0.5 / _view_s) / step) * step
	var rng := RandomNumberGenerator.new(); var waves := PackedVector2Array()
	for i in int(size.x / (step * _view_s)) + 3:
		var wx := x0 + i * step
		for j in 7:
			rng.seed = int(wx * 7.0) * 31 + j * 977
			var wy := _bounds.position.y + _bounds.size.y * rng.randf()
			var s0 := w2s(Vector2(wx + rng.randf() * step, wy)); var L := (10.0 + 14.0 * rng.randf()) * clampf(_view_s / 120.0, 0.6, 1.6)
			var prev := s0
			for k in range(1, 7):   # a short wavy engraver's stroke
				var q := s0 + Vector2(L * float(k) / 6.0, sin(float(k) * 1.2) * 1.6); waves.append(prev); waves.append(q); prev = q
	if waves.size() > 1: draw_multiline(waves, Color(Kit.INH, 0.22), 1.0, true)
	var tl := w2s(Vector2(_bounds.position.x, _bounds.end.y)); var br := w2s(Vector2(_bounds.end.x, _bounds.position.y))
	draw_rect(Rect2(tl, br - tl), Color(Kit.INK, 0.55), false, 1.2)
	var s0b := w2s(Vector2(0, _bounds.end.y)); var s1 := w2s(Vector2(0, _bounds.position.y))
	Gfx.dashed(self, PackedVector2Array([s0b, s1]), Color(Kit.INK, 0.4), 1.2, 6.0, 6.0)
	draw_string(Gfx.serif_italic(), s0b + Vector2(6, float(fz) + 28.0), "start", HORIZONTAL_ALIGNMENT_LEFT, 80, fz, Kit.TEXT_SOFT)
	# scale bar: one body length
	var ro: Dictionary = tele.body.get("readouts", {})
	var sb := Vector2(size.x - 20.0 - _view_s, size.y - 34.0)
	draw_line(sb, sb + Vector2(_view_s, 0), Kit.INK, 1.6); draw_line(sb - Vector2(0, 5), sb + Vector2(0, 5), Kit.INK, 1.4); draw_line(sb + Vector2(_view_s, -5), sb + Vector2(_view_s, 5), Kit.INK, 1.4)
	draw_string(Gfx.serif_italic(), sb + Vector2(0, -8), "1 body length", HORIZONTAL_ALIGNMENT_LEFT, 200, fz, Kit.TEXT_SOFT)
	if ro.has("speed"): draw_string(Gfx.mono(), Vector2(14, size.y - 24), "speed %.2f BL/s" % float(ro["speed"]), HORIZONTAL_ALIGNMENT_LEFT, 240, fz, Kit.TEXT)

func _draw_wake() -> void:
	if _wake.size() < 3: return
	var m := _wake.size(); var chunks := 10
	for k in chunks:
		var a := m * k / chunks; var b := mini(m, m * (k + 1) / chunks + 1); var sp := PackedVector2Array()
		for i in range(a, b): sp.append(w2s(_wake[i]))
		if sp.size() > 1: Gfx.dashed(self, sp, Color(Kit.INK, 0.03 + 0.18 * float(k + 1) / chunks), 1.0, 2.0, 5.0)

func _draw_bench_world(font: Font, fz: int, ro: Dictionary) -> void:
	# the reflex rig: faint ruled lines like a lab notebook page
	var a := w2s(Vector2(_bounds.position.x, _bounds.position.y)); var b := w2s(Vector2(_bounds.end.x, _bounds.end.y))
	for k in 10:
		var y := lerpf(a.y, b.y, float(k) / 9.0); draw_line(Vector2(size.x * 0.06, y), Vector2(size.x * 0.94, y), Color(Kit.GRID, 0.9), 1.0)

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
	# an ink-drawn nematode: dark outline, sepia body, a pale gut line, faint annuli
	Gfx.tube_rim(self, sp, hwf, Gfx.BODY.lightened(0.35), Kit.INK, 1.4)
	Gfx.tube(self, sp, func(u): return W * _width_u(u, "worm") * 0.45, Color(Gfx.BODY.darkened(0.15), 0.6))
	for k in range(3, sp.size() - 2, 3):
		var t := (sp[k + 1] - sp[k - 1]).normalized(); var n := t.orthogonal(); var w: float = hwf.call(float(k) / float(sp.size() - 1))
		draw_line(sp[k] + n * w * 0.85, sp[k] - n * w * 0.85, Color(Kit.INK, 0.25), 1.0, true)
	# head with the two chemosensory cells, washed with their stain when they fire
	var h := sp[0]; var dir := (sp[0] - sp[mini(6, sp.size() - 1)]).normalized(); var nrm := dir.orthogonal()
	var l := clampf((float(ro.get("ASEL_hz", 5.0)) - 5.0) / 150.0, 0.0, 1.0); var r := clampf((float(ro.get("ASER_hz", 5.0)) - 5.0) / 150.0, 0.0, 1.0)
	var pl := h + dir * W * 0.9 - nrm * W * 0.55; var pr := h + dir * W * 0.9 + nrm * W * 0.55   # left of heading = -orthogonal (screen y down)
	Gfx.wash(self, pl, W * (1.0 + 1.6 * l), Kit.exc(), 0.15 + 0.5 * l, 3); Gfx.wash(self, pr, W * (1.0 + 1.6 * r), Kit.inh(), 0.15 + 0.5 * r, 4)
	Gfx.soma(self, pl, W * 0.36, Kit.exc(), 11, l, 0.0, false); Gfx.soma(self, pr, W * 0.36, Kit.inh(), 12, r, 0.0, false)
	var fz := Kit.fs(14)
	draw_string(Gfx.mono(), Vector2(14, size.y - 24), "worm ×%.0f   ASEL %d Hz   ASER %d Hz   dist %.1f mm" % [mag, int(float(ro.get("ASEL_hz", 0))), int(float(ro.get("ASER_hz", 0))), float(ro.get("dist_mm", 0))], HORIZONTAL_ALIGNMENT_LEFT, size.x - 28, fz, Kit.TEXT_SOFT)

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
	# tail fin: a pale fin membrane with fine inked rays
	var tip := sp[sp.size() - 1]; var tdir := (sp[sp.size() - 1] - sp[maxi(sp.size() - 5, 0)]).normalized(); var tn := tdir.orthogonal()
	var fin := PackedVector2Array([tip - tdir * W * 0.4, tip + tdir * W * 1.5 + tn * W * 1.6, tip + tdir * W * 1.0, tip + tdir * W * 1.5 - tn * W * 1.6])
	Gfx.ink_poly(self, fin, Color(Gfx.FISH, 0.18), Color(Kit.INK, 0.7), 1.0)
	for k in 5: draw_line(tip, tip + tdir * W * 1.3 + tn * W * (float(k) - 2.0) * 0.7, Color(Kit.INK, 0.3), 0.8, true)
	# the body: ink outline, slate specimen fill
	Gfx.tube_rim(self, sp, hw, Gfx.FISH.lightened(0.45), Kit.INK, 1.5)
	# muscle activation: the contracting flank is washed with its pool's stain (left motor pool: left side of travel)
	var rl := clampf(float(ro.get("rate_left", 0.0)) / 40.0, 0.0, 1.0); var rr := clampf(float(ro.get("rate_right", 0.0)) / 40.0, 0.0, 1.0)
	Gfx.tube(self, sp, func(u): return W * _width_u(u, "fish") * 0.95, Color(Kit.exc(), 0.08 + 0.62 * rl), -1.0)
	Gfx.tube(self, sp, func(u): return W * _width_u(u, "fish") * 0.95, Color(Kit.inh(), 0.08 + 0.62 * rr), 1.0)
	# myomere chevrons, finely inked
	for k in range(1, 22):
		var i := int(float(k) / 22.0 * (sp.size() - 1)); var u := float(i) / float(sp.size() - 1)
		var t := sp[mini(i + 1, sp.size() - 1)] - sp[maxi(i - 1, 0)]; var n := t.orthogonal().normalized(); var w := W * _width_u(u, "fish")
		if w > 1.5: draw_polyline(PackedVector2Array([sp[i] + n * w * 0.9, sp[i] + t.normalized() * w * 0.35, sp[i] - n * w * 0.9]), Color(Kit.INK, 0.32), 1.0, true)
	draw_polyline(sp, Color(Kit.INK, 0.25), 1.0, true)   # the notochord
	# head: eyes and the seven gill pores
	var h := sp[0]; var dir := (sp[0] - sp[mini(5, sp.size() - 1)]).normalized(); var nr := dir.orthogonal()
	for s in [-1.0, 1.0]:
		draw_circle(h + dir * W * 0.2 + nr * s * W * 0.5, W * 0.2, Kit.INK); draw_circle(h + dir * W * 0.26 + nr * s * W * 0.46, W * 0.07, Gfx.SHEET)
		for g in 7: draw_circle(sp[mini(6 + g * 2, sp.size() - 1)] + nr * s * W * 0.55, W * 0.07, Color(Kit.INK, 0.75))

func _draw_limb(pts: PackedVector2Array, ro: Dictionary) -> void:
	var base := w2s(pts[0]); var tip := w2s(pts[pts.size() - 1]); var mid := w2s(pts[1])
	var touching := float(ro.get("touching", 0.0)) > 0.5; var wd := float(ro.get("withdrawn", 0.0))
	var W := minf(size.x, size.y) * 0.075
	# the body wall the limb grows from: an engraved, hatched block
	var wall := Rect2(Vector2(base.x - W * 2.6, size.y * 0.16), Vector2(W * 2.6, size.y * 0.68))
	var wp := PackedVector2Array([wall.position, Vector2(wall.end.x, wall.position.y), wall.end, Vector2(wall.position.x, wall.end.y)])
	draw_colored_polygon(wp, Gfx.SHADE); Gfx.hatch(self, wp, PI * 0.25, 7.0, Color(Kit.INK, 0.35), 1.0); Gfx.ink_poly(self, wp, Color(0, 0, 0, 0), Kit.INK, 1.5)
	var sp := Gfx.cr_spline(PackedVector2Array([base, mid, tip]), 10)
	Gfx.tube_rim(self, sp, func(u): return W * (1.0 - 0.45 * u), Color("#E9D2B8").lerp(Kit.exc().lightened(0.4), 0.5 * wd), Kit.INK, 1.6)
	# shading: stipple along the lower flank
	var shade := PackedVector2Array()
	for i in sp.size():
		var t := sp[mini(i + 1, sp.size() - 1)] - sp[maxi(i - 1, 0)]; var n := t.orthogonal().normalized(); var u := float(i) / float(sp.size() - 1)
		shade.append(sp[i] + n * W * (1.0 - 0.45 * u) * 0.92)
	for i in range(sp.size() - 1, -1, -1):
		var t2 := sp[mini(i + 1, sp.size() - 1)] - sp[maxi(i - 1, 0)]; var n2 := t2.orthogonal().normalized(); var u2 := float(i) / float(sp.size() - 1)
		shade.append(sp[i] + n2 * W * (1.0 - 0.45 * u2) * 0.35)
	Gfx.stipple_poly(self, shade, 160, Color(Kit.INK, 0.5), 7, 1.3)
	draw_arc(tip, W * 0.42, 0, TAU, 24, Color(Kit.INK, 0.5), 1.0, true)
	if touching: Gfx.wash(self, tip + Vector2(W * 0.5, 0), W * 1.6, Kit.RED, 0.35, 9)
	# the poke: an inked probe on a rod
	var rest_x := w2s(Vector2(1.32, 0)).x; var probe_x := (tip.x + W * 0.9) if touching else rest_x
	var py := base.y
	draw_line(Vector2(size.x - 20, py), Vector2(probe_x + W * 0.5, py), Color(Kit.INK, 0.9 if touching else 0.5), 2.4, true)
	draw_circle(Vector2(probe_x, py), W * 0.5, Gfx.SHEET); draw_arc(Vector2(probe_x, py), W * 0.5, 0, TAU, 24, Kit.INK, 1.6, true)
	if touching:
		for k in 3: draw_arc(Vector2(probe_x, py), W * (0.9 + 0.5 * k), PI * 0.75, PI * 1.25, 8, Color(Kit.INK, 0.7 - 0.2 * k), 1.4, true)
	draw_string(Gfx.mono(), Vector2(14, size.y - 24), "poke %s   afferent %d Hz   bursts %d" % ["ON" if touching else "off", int(float(ro.get("touch_rate_hz", 0.0))), int(float(ro.get("bursts", 0.0)))], HORIZONTAL_ALIGNMENT_LEFT, size.x - 28, Kit.fs(14), Kit.TEXT_SOFT)

## The plate's caption (task title in italic), the model clock in mono, and the run's progress as an inked rule along the bottom.
func _hud(fz: int) -> void:
	var t := tele.t_ms * 0.001; var dur := tele.duration() * 0.001
	draw_string(Gfx.serif_italic(), Vector2(14, 22), str(tele.task.title()), HORIZONTAL_ALIGNMENT_LEFT, size.x * 0.6, Kit.fs(16), Kit.TEXT)
	draw_string(Gfx.mono(), Vector2(size.x - 230, 22), "t %5.1f / %.0f s" % [t, dur], HORIZONTAL_ALIGNMENT_RIGHT, 216, fz, Kit.TEXT_SOFT)
	var pw := size.x - 28.0
	draw_line(Vector2(14, size.y - 12), Vector2(14 + pw, size.y - 12), Color(Kit.INK, 0.35), 1.0)
	draw_line(Vector2(14, size.y - 12), Vector2(14 + pw * clampf(t / maxf(dur, 0.001), 0, 1), size.y - 12), Kit.exc(), 2.5)
