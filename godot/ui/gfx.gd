class_name Gfx
extends RefCounted
## Drawing helpers shared by the lab views: soft glows, bezier axons, dashes, arrow and bar heads, tapered body ribbons. All vector (no sprites).

const VIOLET := Color("#b69cff")      # motor / effector channels
const BODY := Color("#7ee0c3")        # worm
const FISH := Color("#9fd0ff")        # lamprey
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

static func glow(ci: CanvasItem, p: Vector2, radius: float, col: Color) -> void:
	if col.a <= 0.003 or radius <= 0.5: return
	ci.draw_texture_rect(glow_tex(), Rect2(p - Vector2(radius, radius), Vector2(radius, radius) * 2.0), false, col)

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
