class_name Gfx
extends RefCounted
## Drawing helpers shared by the lab views, in the "Cajal Plate" style (docs/IDENTITY.md): India ink on aged paper. Watercolour stain washes (never additive
## glows), stained somata with seeded inked dendrites, engineering graph paper, stippling and hatching for shade, index-card tabs, lettered call-outs, the
## rubber stamp; plus the bezier axons, dashes, arrow and bar heads and tapered body ribbons. All vector (no sprites), crisp at any resolution.

const VIOLET := Kit.MOTOR             # motor / effector channels (madder plum; name kept for compatibility)
const BODY := Color("#8A6A4F")        # worm: a sepia-washed specimen
const FISH := Color("#6F7F86")        # lamprey: a slate-grey specimen
const INK := Kit.INK
const SHEET := Color("#F6F0E2")       # the paper inside a plate / scope (a touch lighter than the page)
const SHADE := Color("#E6DCC6")       # paper in shadow
static var _glow: Texture2D = null

## A radial falloff texture (white, alpha = soft bell), modulated by draw colour. One texture, any size, any colour.
static func glow_tex() -> Texture2D:
	if _glow != null: return _glow
	var n := 128; var img := Image.create(n, n, false, Image.FORMAT_RGBA8); var c := (n - 1) * 0.5
	for y in n:
		for x in n:
			var r := Vector2(x - c, y - c).length() / c
			var a := clampf(1.0 - r, 0.0, 1.0); a = a * a * (3.0 - 2.0 * a)   # smoothstep tail
			a = pow(a, 1.6)
			img.set_pixel(x, y, Color(1, 1, 1, a))
	var t := ImageTexture.create_from_image(img); _glow = t; return t

## A soft watercolour wash (it used to be a glow): on paper there is no light, only pigment. A pure white "glow" is dropped (it would erase the ink).
static func glow(ci: CanvasItem, p: Vector2, radius: float, col: Color) -> void:
	if col.a <= 0.003 or radius <= 0.5: return
	if col.r > 0.9 and col.g > 0.9 and col.b > 0.9: return
	wash(ci, p, radius * 0.62, col, col.a * 0.7, int(p.x * 0.37 + p.y * 0.11))

static func bez(a: Vector2, c1: Vector2, c2: Vector2, b: Vector2, u: float) -> Vector2:
	var v := 1.0 - u
	return a * (v * v * v) + c1 * (3.0 * v * v * u) + c2 * (3.0 * v * u * u) + b * (u * u * u)

static func bez_tan(a: Vector2, c1: Vector2, c2: Vector2, b: Vector2, u: float) -> Vector2:
	var v := 1.0 - u
	return ((c1 - a) * (3.0 * v * v) + (c2 - c1) * (6.0 * v * u) + (b - c2) * (3.0 * u * u))

static func bez_points(a: Vector2, c1: Vector2, c2: Vector2, b: Vector2, n := 24) -> PackedVector2Array:
	var out := PackedVector2Array()
	for i in n + 1: out.append(bez(a, c1, c2, b, float(i) / float(n)))
	return out

## Dashed polyline; `phase` (px) slides the dashes along it.
static func dashed(ci: CanvasItem, pts: PackedVector2Array, col: Color, w: float, dash := 9.0, gap := 7.0, phase := 0.0) -> void:
	var period := dash + gap; var carry := fposmod(-phase, period)   # distance into the pattern at the start
	for i in range(pts.size() - 1):
		var a := pts[i]; var b := pts[i + 1]; var seg := a.distance_to(b)
		if seg < 0.01: continue
		var d := (b - a) / seg; var s := 0.0
		while s < seg:
			var inside := carry < dash
			var run := minf((dash - carry) if inside else (period - carry), seg - s)
			if inside: ci.draw_line(a + d * s, a + d * (s + run), col, w, true)
			s += run; carry += run
			if carry >= period - 0.001: carry = 0.0

static func arrow_head(ci: CanvasItem, tip: Vector2, dir: Vector2, col: Color, size: float) -> void:
	var d := dir.normalized(); var n := d.orthogonal()
	ci.draw_colored_polygon(PackedVector2Array([tip, tip - d * size + n * size * 0.55, tip - d * size * 0.7, tip - d * size - n * size * 0.55]), col)

static func bar_head(ci: CanvasItem, tip: Vector2, dir: Vector2, col: Color, size: float, w: float) -> void:
	var d := dir.normalized(); var n := d.orthogonal()
	ci.draw_line(tip - n * size * 0.7, tip + n * size * 0.7, col, w * 1.6, true)

## ENGRAVED (docs/IDENTITY.md, the Atlas wing): given, locked wiring is cut into the plate, not inked on it. An inked axon is one solid, saturated stroke with a filled head; an
## engraved one is a quieter pair of hairlines with the paper showing between them (the groove), toned toward ink so it recedes, and an open head (a V for excitation, a bar for
## inhibition: the second code of docs/IDENTITY.md stays). `col` is the Golgi sienna or Prussian blue of the sign. Width follows `w`, never the weight's colour saturation.
const ENGRAVE_ALPHA := 0.62
static func engraved_col(col: Color) -> Color: return Color(col.lerp(INK, 0.42), ENGRAVE_ALPHA)

static func engraved(ci: CanvasItem, pts: PackedVector2Array, col: Color, w: float) -> void:
	if pts.size() < 2: return
	var c := engraved_col(col); var off := maxf(1.1, w * 0.45); var line := maxf(0.8, w * 0.28)
	var left := PackedVector2Array(); var right := PackedVector2Array()
	for i in pts.size():
		var d := (pts[mini(i + 1, pts.size() - 1)] - pts[maxi(i - 1, 0)]); if d.length() < 0.001: d = Vector2.RIGHT
		var n := d.normalized().orthogonal()
		left.append(pts[i] + n * off); right.append(pts[i] - n * off)
	ci.draw_polyline(pts, Color(SHEET, 0.55), off * 2.0 + line, true)   # the groove: paper between the two cuts
	ci.draw_polyline(left, c, line, true); ci.draw_polyline(right, c, line, true)

## The open head of an engraved axon: a V (excitation) or a bar (inhibition) in the same hairline.
static func engraved_head(ci: CanvasItem, tip: Vector2, dir: Vector2, col: Color, size: float, inhibitory: bool) -> void:
	var d := dir.normalized(); var n := d.orthogonal(); var c := engraved_col(col)
	if inhibitory:
		ci.draw_line(tip - n * size * 0.7, tip + n * size * 0.7, c, 1.8, true)
		ci.draw_line(tip - d * 3.0 - n * size * 0.7, tip - d * 3.0 + n * size * 0.7, c, 1.0, true)
	else:
		var a := tip - d * size + n * size * 0.55; var b := tip - d * size - n * size * 0.55
		ci.draw_polyline(PackedVector2Array([a, tip, b]), c, 1.6, true)

## A population's engraved frame: a double rule round it (outer hairline, inner hairline a few px in), over the stain. `outline` is a closed polyline of the group's outline.
static func engraved_frame(ci: CanvasItem, outline: PackedVector2Array, inner: PackedVector2Array) -> void:
	ci.draw_polyline(outline, Color(INK, 0.5), 1.0, true)
	if inner.size() > 1: ci.draw_polyline(inner, Color(INK, 0.3), 0.8, true)

static func cr_spline(pts: PackedVector2Array, per := 6) -> PackedVector2Array:
	if pts.size() < 3: return pts
	var out := PackedVector2Array()
	for i in range(pts.size() - 1):
		var p0 := pts[maxi(i - 1, 0)]; var p1 := pts[i]; var p2 := pts[i + 1]; var p3 := pts[mini(i + 2, pts.size() - 1)]
		for k in per:
			var t := float(k) / float(per); var t2 := t * t; var t3 := t2 * t
			out.append(0.5 * ((2.0 * p1) + (-p0 + p2) * t + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2 + (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3))
	out.append(pts[pts.size() - 1]); return out

## Closed outline polygon of a ribbon along `pts` (head first) whose half-width at fraction u in 0..1 is width(u).
static func ribbon(pts: PackedVector2Array, half_w: Callable) -> PackedVector2Array:
	var left := PackedVector2Array(); var right := PackedVector2Array(); var n := pts.size()
	for i in n:
		var t := (pts[mini(i + 1, n - 1)] - pts[maxi(i - 1, 0)])
		var nrm := t.orthogonal().normalized() if t.length() > 0.0001 else Vector2.UP
		var w: float = half_w.call(float(i) / float(maxi(n - 1, 1)))
		left.append(pts[i] + nrm * w); right.append(pts[i] - nrm * w)
	right.reverse(); left.append_array(right); return left

static func rounded_rect(ci: CanvasItem, r: Rect2, fill: Color, border: Color, radius: float, bw := 1.0) -> void:
	var sb := StyleBoxFlat.new(); sb.bg_color = fill; sb.border_color = border; sb.set_border_width_all(int(ceil(bw))); sb.set_corner_radius_all(int(radius)); sb.anti_aliasing = true
	ci.draw_style_box(sb, r)

## A tapered tube along `pts` (head first): `half_w` gives the half-width at fraction u in 0..1. Drawn as overlapping segments and round joints, so it never
## fails to triangulate at tight bends (unlike a ribbon polygon). `side` = 0 both flanks, -1 / +1 one flank only (a half-width strip on that side).
static func tube(ci: CanvasItem, pts: PackedVector2Array, half_w: Callable, col: Color, side := 0.0) -> void:
	var n := pts.size()
	for i in n - 1:
		var u := (float(i) + 0.5) / float(maxi(n - 1, 1)); var w: float = half_w.call(u)
		if side == 0.0:
			ci.draw_line(pts[i], pts[i + 1], col, w * 2.0, true)
			ci.draw_circle(pts[i], w, col)
		else:
			var t := pts[mini(i + 1, n - 1)] - pts[maxi(i - 1, 0)]; var nrm := t.orthogonal().normalized() if t.length() > 0.001 else Vector2.UP
			var t2 := pts[mini(i + 2, n - 1)] - pts[i]; var nrm2 := t2.orthogonal().normalized() if t2.length() > 0.001 else nrm
			ci.draw_line(pts[i] + nrm * side * w * 0.5, pts[i + 1] + nrm2 * side * w * 0.5, col, w, true)
	if side == 0.0: ci.draw_circle(pts[n - 1], float(half_w.call(1.0)), col)

## A tube with a bright rim: the rim colour first (wider), then the fill colour.
static func tube_rim(ci: CanvasItem, pts: PackedVector2Array, half_w: Callable, fill: Color, rim: Color, rim_w := 2.0) -> void:
	tube(ci, pts, func(u): return float(half_w.call(u)) + rim_w, rim)
	tube(ci, pts, half_w, Color(fill.r, fill.g, fill.b, 1.0))


# ---------- the Cajal Plate kit ----------
static var _shape_cache := {}
static var _den_cache := {}
static var _pts_cache := {}

## A deterministic 0..1 hash of two ints.
static func h01(a: int, b: int) -> float:
	var x := (a * 374761393 + b * 668265263) & 0x7fffffff
	x = ((x ^ (x >> 13)) * 1274126177) & 0x7fffffff
	return float(x & 0xffff) / 65535.0

## A unit-radius irregular blob outline (a watercolour edge), seeded; cached.
static func blob(seed: int, n := 40, rough := 1.0) -> PackedVector2Array:
	var key := "%d|%d|%.2f" % [seed & 63, n, rough]
	if _shape_cache.has(key): return _shape_cache[key]
	var out := PackedVector2Array(); var s1 := h01(seed, 1) * TAU; var s2 := h01(seed, 2) * TAU; var s3 := h01(seed, 3) * TAU
	for i in n:
		var a := TAU * float(i) / float(n)
		var r := 1.0 + rough * (0.075 * sin(3.0 * a + s1) + 0.05 * sin(5.0 * a + s2) + 0.03 * sin(9.0 * a + s3))
		out.append(Vector2(cos(a), sin(a)) * r)
	_shape_cache[key] = out; return out

static func _xf(shape: PackedVector2Array, c: Vector2, r: float, rot := 0.0) -> PackedVector2Array:
	var out := PackedVector2Array(); out.resize(shape.size()); var cs := cos(rot); var sn := sin(rot)
	for i in shape.size():
		var q := shape[i]; out[i] = c + Vector2(q.x * cs - q.y * sn, q.x * sn + q.y * cs) * r
	return out

## A watercolour wash: three stacked irregular pools of pigment with a darker pooled rim, like a stain soaked into paper. `a` is the overall strength.
static func wash(ci: CanvasItem, c: Vector2, r: float, col: Color, a := 0.3, seed := 0) -> void:
	if a <= 0.004 or r < 1.0: return
	for k in 3:
		var poly := _xf(blob(seed + k * 7), c + Vector2(h01(seed, k + 9) - 0.5, h01(seed, k + 19) - 0.5) * r * 0.16, r * (1.0 - 0.2 * k), h01(seed, k) * TAU)
		ci.draw_colored_polygon(poly, Color(col, a * (0.30 if k == 0 else 0.26)))
	var rim := _xf(blob(seed), c, r, h01(seed, 0) * TAU); rim.append(rim[0])
	ci.draw_polyline(rim, Color(col, a * 0.42), maxf(1.0, r * 0.035), true)

## Inked dendrite sprigs around a soma, seeded: `n` primary branches that fork twice, tapering. Unit geometry is cached per seed.
static func _den(seed: int, n: int) -> Array:
	var key := "%d|%d" % [seed & 255, n]
	if _den_cache.has(key): return _den_cache[key]
	var lv := [[], [], []]   # plain Arrays (a PackedVector2Array in an Array is copied on access)
	var rng := RandomNumberGenerator.new(); rng.seed = seed * 7919 + 13
	var a0 := rng.randf() * TAU
	for i in n:
		var a := a0 + TAU * (float(i) + rng.randf_range(-0.3, 0.3)) / float(n)
		var p := Vector2(cos(a), sin(a)) * 0.92; var len := rng.randf_range(0.55, 0.95)
		_branch(lv, rng, p, a, len, 0)
	var out := [PackedVector2Array(lv[0]), PackedVector2Array(lv[1]), PackedVector2Array(lv[2])]
	_den_cache[key] = out; return out

static func _branch(lv: Array, rng: RandomNumberGenerator, p: Vector2, a: float, len: float, depth: int) -> void:
	var segs := 3; var q := p; var ang := a
	for k in segs:
		ang += rng.randf_range(-0.35, 0.35); var nq := q + Vector2(cos(ang), sin(ang)) * len / float(segs)
		(lv[depth] as Array).append(q); (lv[depth] as Array).append(nq); q = nq
		if depth < 2 and k == 1 and rng.randf() < 0.85:
			_branch(lv, rng, q, ang + rng.randf_range(0.45, 0.9) * (1.0 if rng.randf() < 0.5 else -1.0), len * 0.62, depth + 1)
	if depth < 2:
		_branch(lv, rng, q, ang + rng.randf_range(-0.5, 0.5), len * 0.5, depth + 1)

static func dendrites(ci: CanvasItem, c: Vector2, r: float, col: Color, seed := 0, n := 7, reach := 1.0) -> void:
	var lv := _den(seed, n); var ws := [1.5, 1.05, 0.75]
	for d in 3:
		var src: PackedVector2Array = lv[d]; if src.is_empty(): continue
		var pts := PackedVector2Array(); pts.resize(src.size())
		for i in src.size(): pts[i] = c + src[i] * r * (1.0 + (reach - 1.0) * (src[i].length() - 0.9))
		ci.draw_multiline(pts, Color(col, col.a * (1.0 - 0.18 * d)), maxf(0.7, float(ws[d]) * clampf(r / 22.0, 0.7, 1.4)), true)

## A Golgi-stained soma: inked dendrite sprigs, a stained body in `fill` with a lighter crown and darker belly, and a fine ink outline.
## `dark` (0..1) deepens the stain (a spike); `fade` greys it toward the paper (silenced, empty).
static func soma(ci: CanvasItem, c: Vector2, r: float, fill: Color, seed := 0, dark := 0.0, fade := 0.0, sprigs := true) -> void:
	if sprigs: dendrites(ci, c, r, Color(INK, 0.62 * (1.0 - fade)), seed, 7)
	var body := fill.darkened(0.22 * dark).lerp(SHEET, fade)
	ci.draw_circle(c, r, body)
	ci.draw_circle(c + Vector2(0.10, 0.14) * r, r * 0.86, Color(body.darkened(0.14), 0.4))
	ci.draw_circle(c - Vector2(0.18, 0.22) * r, r * 0.52, Color(body.lightened(0.16), 0.75))
	ci.draw_circle(c - Vector2(0.32, 0.36) * r, r * 0.16, Color(SHEET, 0.35 * (1.0 - fade)))
	ci.draw_arc(c, r, 0, TAU, maxi(24, int(r * 1.6)), Color(INK, 0.88 * (1.0 - 0.5 * fade)), clampf(r * 0.07, 1.1, 2.2), true)

## The firing stain: a wash of `col` that spreads and fades as `f` (1 at the spike, decaying to 0) falls. Reduced motion: no spread, only the fade.
static func spike_stain(ci: CanvasItem, c: Vector2, r: float, col: Color, f: float, seed := 0) -> void:
	if f <= 0.02: return
	var spread := (1.0 - f) if Kit.motion() else 0.5
	wash(ci, c, r * (1.35 + 1.25 * spread), col, 0.55 * f, seed)

## Engineering graph paper over `r`: fine minor lines every `cell` px, a heavier major line every `major` cells. `origin` anchors the grid (it pans with the view).
static func graph_paper(ci: CanvasItem, r: Rect2, cell := 24.0, fill: Color = SHEET, origin := Vector2.ZERO, major := 5, line: Color = Kit.GRID) -> void:
	if fill.a > 0.0: ci.draw_rect(r, fill)
	while cell < 9.0: cell *= float(major); major = 1
	var minor := PackedVector2Array(); var heavy := PackedVector2Array()
	var i0 := int(floor((r.position.x - origin.x) / cell)); var i1 := int(ceil((r.end.x - origin.x) / cell))
	for i in range(i0, i1 + 1):
		var x := origin.x + float(i) * cell
		if x < r.position.x or x > r.end.x: continue
		var arr := heavy if (major > 1 and posmod(i, major) == 0) else minor
		arr.append(Vector2(x, r.position.y)); arr.append(Vector2(x, r.end.y))
	var j0 := int(floor((r.position.y - origin.y) / cell)); var j1 := int(ceil((r.end.y - origin.y) / cell))
	for j in range(j0, j1 + 1):
		var y := origin.y + float(j) * cell
		if y < r.position.y or y > r.end.y: continue
		var arr2 := heavy if (major > 1 and posmod(j, major) == 0) else minor
		arr2.append(Vector2(r.position.x, y)); arr2.append(Vector2(r.end.x, y))
	if minor.size() > 0: ci.draw_multiline(minor, Color(line, 0.75), 1.0)
	if heavy.size() > 0: ci.draw_multiline(heavy, line.darkened(0.12), 1.3)

## Chart-paper strip (a scope lane): sheet, faint grid, ink border.
static func chart_strip(ci: CanvasItem, r: Rect2, cell := 12.0) -> void:
	graph_paper(ci, r, cell, SHEET, r.position, 5, Color(Kit.GRID, 0.85))
	ci.draw_rect(r, Color(INK, 0.55), false, 1.0)

## An ink-ruled plate frame: thin double rule (outer heavy, inner hairline) like an engraved plate.
static func plate_frame(ci: CanvasItem, r: Rect2, fill: Color = SHEET) -> void:
	if fill.a > 0.0: ci.draw_rect(r, fill)
	ci.draw_rect(r, Color(INK, 0.85), false, 1.4)
	ci.draw_rect(r.grow(-4.0), Color(INK, 0.35), false, 1.0)

## Seeded points: `n` uniformly in a unit disc (cached), with a density bias `bias` (> 1 pulls toward the centre).
static func _disc_pts(seed: int, n: int, bias: float) -> PackedVector2Array:
	var key := "d%d|%d|%.2f" % [seed, n, bias]
	if _pts_cache.has(key): return _pts_cache[key]
	var rng := RandomNumberGenerator.new(); rng.seed = seed * 31 + 7; var out := PackedVector2Array()
	for i in n:
		var a := rng.randf() * TAU; var rr := pow(rng.randf(), 0.5 * bias)
		out.append(Vector2(cos(a), sin(a)) * rr)
	_pts_cache[key] = out; return out

## Stippling in a disc: tiny ink dots (drawn as one multiline batch of short dashes), denser toward the centre when bias > 1.
static func stipple_disc(ci: CanvasItem, c: Vector2, r: float, n: int, col: Color, seed := 0, bias := 1.0, dot := 1.6) -> void:
	var src := _disc_pts(seed, n, bias); var pts := PackedVector2Array(); pts.resize(src.size() * 2)
	for i in src.size(): var q := c + src[i] * r; pts[i * 2] = q; pts[i * 2 + 1] = q + Vector2(dot * 0.5, dot * 0.35)
	ci.draw_multiline(pts, col, dot, true)

## Stippling inside a polygon (screen space): `n` seeded dots, cached per polygon bounding box size.
static func stipple_poly(ci: CanvasItem, poly: PackedVector2Array, n: int, col: Color, seed := 0, dot := 1.4) -> void:
	if poly.size() < 3: return
	var lo := poly[0]; var hi := poly[0]
	for q in poly: lo = lo.min(q); hi = hi.max(q)
	var rng := RandomNumberGenerator.new(); rng.seed = seed * 131 + 3
	var pts := PackedVector2Array(); var tries := 0
	while pts.size() < n * 2 and tries < n * 6:
		tries += 1
		var q := Vector2(rng.randf_range(lo.x, hi.x), rng.randf_range(lo.y, hi.y))
		if Geometry2D.is_point_in_polygon(q, poly): pts.append(q); pts.append(q + Vector2(dot * 0.5, dot * 0.35))
	if pts.size() > 1: ci.draw_multiline(pts, col, dot, true)

## Hatching: parallel ink lines at `angle` every `gap` px, clipped to `poly` (engraver's shade).
static func hatch(ci: CanvasItem, poly: PackedVector2Array, angle: float, gap: float, col: Color, w := 1.0) -> void:
	if poly.size() < 3 or gap < 1.5: return
	var c := Vector2.ZERO; var lo := poly[0]; var hi := poly[0]
	for q in poly: c += q; lo = lo.min(q); hi = hi.max(q)
	c /= float(poly.size()); var rad := (hi - lo).length() * 0.5 + 2.0
	var d := Vector2(cos(angle), sin(angle)); var nrm := d.orthogonal(); var segs := PackedVector2Array()
	var k := -rad
	while k <= rad:
		var a := c + nrm * k - d * rad; var b := c + nrm * k + d * rad
		for piece in Geometry2D.intersect_polyline_with_polygon(PackedVector2Array([a, b]), poly):
			var pp: PackedVector2Array = piece
			if pp.size() >= 2: segs.append(pp[0]); segs.append(pp[pp.size() - 1])
		k += gap
	if segs.size() > 1: ci.draw_multiline(segs, col, w, true)

## A closed ink outline with an optional wash fill.
static func ink_poly(ci: CanvasItem, poly: PackedVector2Array, fill: Color, line: Color = INK, w := 1.6) -> void:
	if poly.size() < 3: return
	if fill.a > 0.0: ci.draw_colored_polygon(poly, fill)
	var cl := poly.duplicate(); cl.append(poly[0]); ci.draw_polyline(cl, line, w, true)

## An ellipse as a polygon.
static func ellipse(c: Vector2, rx: float, ry: float, n := 48, rot := 0.0) -> PackedVector2Array:
	var out := PackedVector2Array(); var cs := cos(rot); var sn := sin(rot)
	for i in n:
		var a := TAU * float(i) / float(n); var q := Vector2(cos(a) * rx, sin(a) * ry)
		out.append(c + Vector2(q.x * cs - q.y * sn, q.x * sn + q.y * cs))
	return out

## An ink bead (a pulse on an axon): a dark dot with a paper halo so it reads on any line.
static func bead(ci: CanvasItem, p: Vector2, r := 3.4, col: Color = INK) -> void:
	ci.draw_circle(p, r + 1.6, Color(SHEET, 0.9)); ci.draw_circle(p, r, col)

## An index-card tab (sensor / motor port): a card with a small tab on its top edge, ink-coloured border, kicker in small caps, the name in ink.
## `act` (0..1) fills the activity dots; returns nothing. `left_tab` puts the tab at the left.
static func index_card(ci: CanvasItem, r: Rect2, col: Color, kicker: String, name: String, act: float, font: Font, bold: Font, fz: int, sel := false) -> void:
	var tw := minf(r.size.x * 0.34, 46.0); var th := minf(7.0, r.size.y * 0.16)
	var shape := PackedVector2Array([r.position, r.position + Vector2(10.0, -th), r.position + Vector2(10.0 + tw, -th), r.position + Vector2(18.0 + tw, 0.0), Vector2(r.end.x, r.position.y), r.end, Vector2(r.position.x, r.end.y)])
	ci.draw_colored_polygon(PackedVector2Array([r.position + Vector2(4, 4), Vector2(r.end.x + 4, r.position.y + 4), r.end + Vector2(4, 4), Vector2(r.position.x + 4, r.end.y + 4)]), Color(INK, 0.10))
	ink_poly(ci, shape, Color("#FBF7EE"), col if not sel else INK, 2.2 if sel else 1.6)
	var kz := maxi(fz - 1, Kit.fs(12)); var y1 := r.position.y + float(kz) + 4.0
	var ks := spaced(kicker); var ndots := 5
	if font.get_string_size(ks, HORIZONTAL_ALIGNMENT_LEFT, -1, kz).x + 70.0 > r.size.x: ks = kicker   # a narrow card (phone): plain caps, fewer dots
	if font.get_string_size(ks, HORIZONTAL_ALIGNMENT_LEFT, -1, kz).x + 70.0 > r.size.x: ndots = 3
	if font.get_string_size(ks, HORIZONTAL_ALIGNMENT_LEFT, -1, kz).x > r.size.x - 18.0 - 8.0 * float(ndots): ndots = 2   # a card the editor narrowed to fit a small bench: the word first, then the dots
	if font.get_string_size(ks, HORIZONTAL_ALIGNMENT_LEFT, -1, kz).x > r.size.x - 18.0 - 8.0 * float(ndots): ndots = 0
	ci.draw_string(font, Vector2(r.position.x + 10.0, y1), ks, HORIZONTAL_ALIGNMENT_LEFT, r.size.x - 18.0 - 8.0 * ndots, kz, col)
	var nz := fz   # the name shrinks (to the font floor) before it is cut: "right_motor" on a narrowed card
	while nz > Kit.fs(12) and bold.get_string_size(name, HORIZONTAL_ALIGNMENT_LEFT, -1, nz).x > r.size.x - 12.0: nz -= 1
	ci.draw_string(bold, Vector2(r.position.x + 8.0, r.end.y - 8.0), name, HORIZONTAL_ALIGNMENT_LEFT, r.size.x - 12.0, nz, INK)
	for k in ndots:
		var lit := float(k) < act * float(ndots) - 0.01
		var dp := Vector2(r.end.x - 11.0 - float(ndots - 1 - k) * 8.0, r.position.y + float(kz) * 0.5 + 4.0)
		if lit: ci.draw_circle(dp, 3.0, col)
		else: ci.draw_arc(dp, 2.6, 0, TAU, 12, Color(col, 0.55), 1.0, true)

## Letter-spaced small caps for kickers ("SENSOR" -> "S E N S O R" is too wide; this uses thin spaces).
static func spaced(t: String) -> String:
	var out := ""
	for i in t.length(): out += t[i] + ("\u2009" if i < t.length() - 1 else "")
	return out

## A lettered call-out: a fine ink leader from `at` (a small ring there) to the italic letter at `lab`.
static func callout(ci: CanvasItem, at: Vector2, lab: Vector2, letter: String, fz: int, col: Color = INK) -> void:
	var f := serif_italic(); var sz := f.get_string_size(letter, HORIZONTAL_ALIGNMENT_LEFT, -1, fz)
	var dir := (at - lab); var end := lab + dir.normalized() * (sz.x * 0.5 + 5.0) if dir.length() > sz.x else lab
	ci.draw_line(end, at, Color(col, 0.8), 1.0, true); ci.draw_circle(at, 2.2, col)
	ci.draw_string(f, lab + Vector2(-sz.x * 0.5, fz * 0.35), letter, HORIZONTAL_ALIGNMENT_LEFT, -1, fz, col)

## A rubber stamp: a slightly rotated double ring in `col`, an arched word over a big number and a small line under it; drawn at partial ink
## (stamps never print solid). `k` (0..1) is the landing: it starts large and settles (reduced motion: lands at once).
static func stamp(ci: CanvasItem, c: Vector2, r: float, top: String, big: String, sub: String, col: Color, rot := -0.21, k := 1.0) -> void:
	var s := 1.0 + 0.5 * (1.0 - k) * (1.0 - k) if Kit.motion() else 1.0
	var a := clampf(k * 1.4, 0.0, 1.0) if Kit.motion() else 1.0
	ci.draw_set_transform(c, rot, Vector2(s, s))
	var ink := Color(col, 0.9 * a)
	ci.draw_arc(Vector2.ZERO, r, 0, TAU, 72, ink, maxf(2.0, r * 0.045), true)
	ci.draw_arc(Vector2.ZERO, r * 0.86, 0, TAU, 72, Color(ink, ink.a * 0.8), maxf(1.0, r * 0.02), true)
	var f := serif(); var fb := serif_bold(); var fi := serif_italic()
	var tz := maxi(int(r * 0.22), Kit.fs(12))
	# the arched word across the top
	var t2 := top; var n := t2.length(); var span := minf(PI * 0.9, float(n) * float(tz) * 0.78 / (r * 0.68))
	for i in n:
		var ang := -PI * 0.5 - span * 0.5 + span * (float(i) + 0.5) / float(n)
		var p := Vector2(cos(ang), sin(ang)) * r * 0.62
		ci.draw_set_transform(c + (p * s).rotated(rot), rot + ang + PI * 0.5, Vector2(s, s))
		var cw := fb.get_string_size(t2[i], HORIZONTAL_ALIGNMENT_LEFT, -1, tz).x
		ci.draw_string(fb, Vector2(-cw * 0.5, tz * 0.35), t2[i], HORIZONTAL_ALIGNMENT_LEFT, -1, tz, ink)
	ci.draw_set_transform(c, rot, Vector2(s, s))
	var bz := maxi(int(r * 0.66), Kit.fs(14)); var bw := fb.get_string_size(big, HORIZONTAL_ALIGNMENT_LEFT, -1, bz).x
	ci.draw_string(fb, Vector2(-bw * 0.5, r * 0.3), big, HORIZONTAL_ALIGNMENT_LEFT, -1, bz, ink)
	if sub != "":
		var sz := maxi(int(r * 0.2), Kit.fs(12)); var sw := fi.get_string_size(sub, HORIZONTAL_ALIGNMENT_LEFT, -1, sz).x
		ci.draw_string(fi, Vector2(-sw * 0.5, r * 0.66), sub, HORIZONTAL_ALIGNMENT_LEFT, -1, sz, ink)
	# worn ink: a few paper specks over the impression
	for i in 26:
		var q := Vector2(h01(i, 5) - 0.5, h01(i, 6) - 0.5) * r * 1.9
		if q.length() < r * 1.02: ci.draw_circle(q, 0.6 + 1.3 * h01(i, 7), Color(SHEET, 0.85))
	ci.draw_set_transform(Vector2.ZERO, 0.0, Vector2.ONE)

# ---------- type ----------
## The identity's faces (game/fonts.gd): display serif (Fraunces), its bold, its italic for captions and specimen letters, and the mono for readouts.
static func serif() -> Font: return Fonts.display_med
static func serif_bold() -> Font: return Fonts.display_heavy
static func serif_italic() -> Font: return Fonts.italic
static func mono() -> Font: return Fonts.mono
