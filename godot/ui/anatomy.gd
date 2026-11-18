class_name Anatomy
extends RefCounted
## Vector anatomy shared by the campaign maps and the body views: a four-chamber heart with its conduction system, lungs with a bronchial tree, a medulla
## section, a spinal column and cord section, an arm with muscles and spindles, an eye, a bowel, the autonomic body, a worm and a lamprey. Everything is
## drawn from polygons, splines and tubes in a unit frame and scaled to the rect it is given, so it stays crisp at any resolution. Each function returns the
## pixel positions of its landmarks (SA node, preBötC, dorsal horn ...) so the map can pin tasks to them.

const OXY := Color("#B23A2E")        # oxygenated blood
const DEOXY := Color("#3D5E9E")      # deoxygenated blood
const WALL := Color("#A2493F")       # myocardium
const BONE := Color("#5E4E3E")
const TISSUE := Color("#EEDFCB")
const GRAY := Color("#6E6458")
const NUC := Color("#6E4E7E")

static func symp() -> Color: return Kit.exc()
static func para() -> Color: return Kit.inh()

# ---------------------------------------------------------------- geometry helpers
static func sc(arr: Array, o: Vector2, s: float) -> PackedVector2Array:
	var out := PackedVector2Array()
	for p in arr: out.append(o + (p as Vector2) * s)
	return out

static func closed_spline(pts: PackedVector2Array, per := 5) -> PackedVector2Array:
	var n := pts.size(); var out := PackedVector2Array()
	for i in n:
		var p0 := pts[(i - 1 + n) % n]; var p1 := pts[i]; var p2 := pts[(i + 1) % n]; var p3 := pts[(i + 2) % n]
		for k in per:
			var t := float(k) / float(per); var t2 := t * t; var t3 := t2 * t
			out.append(0.5 * ((2.0 * p1) + (-p0 + p2) * t + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2 + (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3))
	return out

static func scaled_about(pts: PackedVector2Array, c: Vector2, k: float) -> PackedVector2Array:
	var out := PackedVector2Array()
	for p in pts: out.append(c + (p - c) * k)
	return out

static func centroid(pts: PackedVector2Array) -> Vector2:
	var c := Vector2.ZERO
	for p in pts: c += p
	return c / float(maxi(pts.size(), 1))

static func length(pts: PackedVector2Array) -> float:
	var l := 0.0
	for i in pts.size() - 1: l += pts[i].distance_to(pts[i + 1])
	return l

static func along(pts: PackedVector2Array, u: float) -> Vector2:
	if pts.size() < 2: return pts[0] if pts.size() == 1 else Vector2.ZERO
	var d := clampf(u, 0.0, 1.0) * length(pts)
	for i in pts.size() - 1:
		var seg := pts[i].distance_to(pts[i + 1])
		if d <= seg or i == pts.size() - 2: return pts[i].lerp(pts[i + 1], clampf(d / maxf(seg, 1e-6), 0.0, 1.0))
		d -= seg
	return pts[pts.size() - 1]

static func smooth(pts: Array, o: Vector2, s: float, per := 6) -> PackedVector2Array: return Gfx.cr_spline(sc(pts, o, s), per)

static func bez2(a: Vector2, c: Vector2, b: Vector2, n := 20) -> PackedVector2Array:
	var out := PackedVector2Array()
	for i in n + 1:
		var u := float(i) / float(n); var v := 1.0 - u
		out.append(a * (v * v) + c * (2.0 * v * u) + b * (u * u))
	return out

static func shape(ci: CanvasItem, poly: PackedVector2Array, fill: Color, rim: Color, rim_w := 2.0, halo := 0.0) -> void:
	if fill.a > 0.0 and poly.size() >= 3: ci.draw_colored_polygon(poly, fill)
	if poly.size() < 2: return
	var cl := poly.duplicate(); cl.append(poly[0])
	ci.draw_polyline(cl, rim, rim_w, true)

## A tube with a rim, constant half width in px.
static func pipe(ci: CanvasItem, pts: PackedVector2Array, hw: float, fill: Color, rim: Color, rim_w := 1.6) -> void:
	Gfx.tube_rim(ci, pts, func(_u): return hw, fill, rim, rim_w)

## A nerve or fibre: an inked line in its stain (heavier when active) with ink beads running along it in proportion to `act` (0..1).
static func nerve(ci: CanvasItem, pts: PackedVector2Array, col: Color, w: float, act := 0.3, t := 0.0, alpha := 1.0, pulses := 3) -> void:
	if pts.size() < 2: return
	ci.draw_polyline(pts, Color(col, (0.55 + 0.4 * act) * alpha), maxf(w * (0.8 + 0.4 * act), 1.0), true)
	if act > 0.03 and Kit.motion():
		for k in pulses:
			var u := fposmod(t * (0.22 + 0.5 * act) + float(k) / float(pulses), 1.0); var p := along(pts, u)
			ci.draw_circle(p, maxf(w * 1.3, 2.4) + 1.2, Color(Gfx.SHEET, 0.85 * alpha)); ci.draw_circle(p, maxf(w * 1.3, 2.4), Color(col.darkened(0.35), 0.95 * alpha))

static func text(ci: CanvasItem, s: String, p: Vector2, col: Color, size := 14, align := HORIZONTAL_ALIGNMENT_LEFT, w := -1.0, bold := false) -> void:
	ci.draw_string(Fonts.bold if bold else Gfx.serif_italic(), p, s, align, w, Kit.fs(size), col)

## Text on a small paper slip, anchored at its left (or centre / right) at baseline p.
static func tag(ci: CanvasItem, s: String, p: Vector2, col: Color, size := 14, align := HORIZONTAL_ALIGNMENT_LEFT, bold := false, plate := 0.78) -> Rect2:
	var f := Fonts.bold if bold else Gfx.serif_italic(); var fz := Kit.fs(size)
	var w := f.get_string_size(s, HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x; var h := float(fz)
	var x := p.x if align == HORIZONTAL_ALIGNMENT_LEFT else (p.x - w * 0.5 if align == HORIZONTAL_ALIGNMENT_CENTER else p.x - w)
	var r := Rect2(x - 4.0, p.y - h * 0.92, w + 8.0, h * 1.25)
	if plate > 0.0: Gfx.rounded_rect(ci, r, Color(Gfx.SHEET, plate), Color(0, 0, 0, 0), 3.0, 0.0)
	ci.draw_string(f, Vector2(x, p.y), s, HORIZONTAL_ALIGNMENT_LEFT, w + 4.0, fz, col)
	return r

## A leader line from a landmark to a label, with a small dot on the landmark.
static func leader(ci: CanvasItem, from: Vector2, to: Vector2, col: Color) -> void:
	ci.draw_line(from, to, Color(col, 0.55), 1.2, true); ci.draw_circle(from, 2.6, col)

# ---------------------------------------------------------------- heart
const H_RA := [Vector2(0.17, 0.30), Vector2(0.24, 0.215), Vector2(0.35, 0.19), Vector2(0.44, 0.235), Vector2(0.465, 0.34), Vector2(0.45, 0.44), Vector2(0.33, 0.465), Vector2(0.21, 0.43)]
const H_LA := [Vector2(0.535, 0.34), Vector2(0.56, 0.235), Vector2(0.66, 0.19), Vector2(0.77, 0.21), Vector2(0.84, 0.31), Vector2(0.83, 0.42), Vector2(0.72, 0.465), Vector2(0.56, 0.44)]
const H_RV := [Vector2(0.14, 0.525), Vector2(0.30, 0.505), Vector2(0.465, 0.51), Vector2(0.48, 0.66), Vector2(0.49, 0.80), Vector2(0.50, 0.945), Vector2(0.36, 0.865), Vector2(0.22, 0.75), Vector2(0.145, 0.63)]
const H_LV := [Vector2(0.52, 0.51), Vector2(0.70, 0.505), Vector2(0.845, 0.545), Vector2(0.905, 0.645), Vector2(0.88, 0.775), Vector2(0.76, 0.885), Vector2(0.60, 0.94), Vector2(0.505, 0.955), Vector2(0.515, 0.80), Vector2(0.52, 0.66)]
const H_SVC := [Vector2(0.25, -0.16), Vector2(0.265, 0.03), Vector2(0.30, 0.225)]
const H_IVC := [Vector2(0.07, 1.0), Vector2(0.06, 0.72), Vector2(0.09, 0.52), Vector2(0.20, 0.40)]
const H_AORTA := [Vector2(0.505, 0.55), Vector2(0.505, 0.26), Vector2(0.53, 0.10), Vector2(0.62, 0.02), Vector2(0.74, 0.03), Vector2(0.85, 0.10), Vector2(0.94, 0.17), Vector2(1.02, 0.17)]
const H_PV1 := [Vector2(1.02, 0.27), Vector2(0.92, 0.285), Vector2(0.82, 0.30)]
const H_PV2 := [Vector2(1.02, 0.395), Vector2(0.92, 0.385), Vector2(0.82, 0.375)]
const H_CAROTID := [Vector2(0.61, 0.03), Vector2(0.585, -0.07), Vector2(0.60, -0.16)]
const H_INTER := [Vector2(0.30, 0.22), Vector2(0.345, 0.30), Vector2(0.40, 0.375), Vector2(0.455, 0.455)]
const H_BACH := [Vector2(0.36, 0.26), Vector2(0.50, 0.265), Vector2(0.66, 0.27)]
const H_HIS := [Vector2(0.455, 0.455), Vector2(0.50, 0.50), Vector2(0.50, 0.62)]
const H_BR := [Vector2(0.50, 0.62), Vector2(0.44, 0.74), Vector2(0.36, 0.835), Vector2(0.28, 0.865)]
const H_BL := [Vector2(0.50, 0.62), Vector2(0.60, 0.73), Vector2(0.70, 0.82), Vector2(0.79, 0.845)]
const HEART_H := 1.16        # frame height in units (the carotid stub reaches y = -0.16)

## Time-course of a heartbeat from the milliseconds since the last beat: atrial then ventricular contraction after the AV delay.
static func heart_state(tms: float) -> Dictionary:
	var a := sin(PI * clampf(tms / 120.0, 0.0, 1.0)) if tms >= 0.0 and tms < 120.0 else 0.0
	var v := sin(PI * clampf((tms - 150.0) / 300.0, 0.0, 1.0)) if tms >= 150.0 and tms < 450.0 else 0.0
	return {"a": a, "v": v, "tms": tms}

static func heart_frame(r: Rect2) -> Array:
	var s := minf(r.size.x, r.size.y / HEART_H)
	return [Vector2(r.get_center().x - s * 0.5, r.get_center().y - s * HEART_H * 0.5 + 0.16 * s), s]

## The anatomical heart in front view: RA, RV, LA, LV, vena cavae, aorta with its arch and a carotid stub, pulmonary veins, valves and the conduction system.
## st: tms (ms since the last beat, -1 = at rest), a / v (override of the contraction), alpha, labels, drive (autonomic: symp, vagus 0..1; omit for none), t (seconds).
static func heart(ci: CanvasItem, r: Rect2, st: Dictionary) -> Dictionary:
	var fr := heart_frame(r); var o: Vector2 = fr[0]; var s: float = fr[1]
	var al := float(st.get("alpha", 1.0)); var tms := float(st.get("tms", -1.0)); var t := float(st.get("t", 0.0))
	var hs := heart_state(tms) if tms >= 0.0 else {"a": 0.0, "v": 0.0}
	var a := float(st.get("a", hs["a"])); var v := float(st.get("v", hs["v"]))
	var labels := bool(st.get("labels", false)); var fz := Kit.fs(14)
	var P := func(x: float, y: float) -> Vector2: return o + Vector2(x, y) * s
	# vessels (behind the chambers)
	var vox := Color(OXY, 0.85 * al); var vde := Color(DEOXY, 0.85 * al)
	var ao := smooth(H_AORTA, o, s); var hw_ao := s * 0.035 * (1.0 + 0.10 * v)
	pipe(ci, ao, hw_ao, Color("#EDC9C0", al), vox, 1.8)
	pipe(ci, smooth(H_CAROTID, o, s), s * 0.019, Color("#EDC9C0", al), vox, 1.4)
	pipe(ci, smooth(H_PV1, o, s), s * 0.021, Color("#EDC9C0", al), vox, 1.4); pipe(ci, smooth(H_PV2, o, s), s * 0.021, Color("#EDC9C0", al), vox, 1.4)
	pipe(ci, smooth(H_SVC, o, s), s * 0.031, Color("#CDD5E6", al), vde, 1.8)
	pipe(ci, smooth(H_IVC, o, s), s * 0.031, Color("#CDD5E6", al), vde, 1.8)
	# chambers: myocardial wall first (a wide stroke), then the blood pool
	var chambers := [["RA", H_RA, a, false, 0.022], ["LA", H_LA, a, true, 0.022], ["RV", H_RV, v, false, 0.040], ["LV", H_LV, v, true, 0.055]]
	var polys := []
	for ch in chambers:
		var k := 1.0 - 0.10 * float(ch[2]); var base := closed_spline(sc(ch[1], o, s), 5)
		var p := scaled_about(base, centroid(base), k); polys.append(p)
		var cl := p.duplicate(); cl.append(p[0])
		var wcol := WALL.lerp(Color("#7E2E2A"), 0.45 * float(ch[2]))
		ci.draw_polyline(cl, Color(wcol, al), s * (float(ch[4]) + 0.012 * float(ch[2])) * 2.0, true)
	for i in chambers.size():
		var ch: Array = chambers[i]; var ox: bool = ch[3]; var act := float(ch[2]); var p: PackedVector2Array = polys[i]
		var base_col := Color("#F2D6CE") if ox else Color("#D8DEEC"); var hot := Color("#D9786A") if ox else Color("#8FA4D2")
		ci.draw_colored_polygon(p, Color(base_col.lerp(hot, 0.55 * act), al))
		Gfx.hatch(ci, p, PI * 0.25, maxf(s * 0.02, 5.0), Color(Kit.INK, 0.09 * al), 1.0)
		var cl2 := p.duplicate(); cl2.append(p[0])
		ci.draw_polyline(cl2, Color(OXY if ox else DEOXY, (0.55 + 0.4 * act) * al), 1.6, true)
		var cc := centroid(p)
		Gfx.glow(ci, cc, s * 0.22, Color(OXY if ox else DEOXY, (0.05 + 0.20 * act) * al))
	# papillary hint: the septum line and the valves in the AV groove
	var spt := P.call(0.5, 0.52) as Vector2; var spb := P.call(0.5, 0.94) as Vector2
	ci.draw_line(spt, spb, Color(WALL.lightened(0.18), 0.9 * al), s * 0.016, true)
	var closed_valve := v > 0.12
	for vi in 2:
		var cx := 0.325 if vi == 0 else 0.675; var hwid := 0.125
		var y0 := 0.482; var op := 0.0 if closed_valve else 1.0
		var vcol := Color(BONE, 0.85 * al)
		var l1 := P.call(cx - hwid, y0) as Vector2; var l2 := P.call(cx + hwid, y0) as Vector2
		var t1 := P.call(cx - hwid * 0.12 * (1.0 - op) - hwid * 0.5 * op, y0 + 0.045 + 0.02 * op) as Vector2
		var t2 := P.call(cx + hwid * 0.12 * (1.0 - op) + hwid * 0.5 * op, y0 + 0.045 + 0.02 * op) as Vector2
		ci.draw_polyline(PackedVector2Array([l1, t1]), vcol, maxf(s * 0.008, 1.4), true); ci.draw_polyline(PackedVector2Array([l2, t2]), vcol, maxf(s * 0.008, 1.4), true)
	# conduction system
	var inter := smooth(H_INTER, o, s, 5); var bach := smooth(H_BACH, o, s, 5); var his := smooth(H_HIS, o, s, 5); var br := smooth(H_BR, o, s, 5); var bl := smooth(H_BL, o, s, 5)
	var ccol := Color("#C9971C")
	for pth in [inter, bach, his, br, bl]: ci.draw_polyline(pth, Color(ccol, 0.30 * al), maxf(s * 0.006, 1.2), true)
	for k in 3:   # Purkinje twigs
		var base2 := along(br, 0.45 + 0.25 * float(k)); ci.draw_line(base2, base2 + Vector2(-0.03, 0.045) * s, Color(ccol, 0.22 * al), 1.0, true)
		var base3 := along(bl, 0.45 + 0.25 * float(k)); ci.draw_line(base3, base3 + Vector2(0.03, 0.045) * s, Color(ccol, 0.22 * al), 1.0, true)
	var sa := P.call(0.298, 0.222) as Vector2; var av := P.call(0.455, 0.455) as Vector2
	var sa_glow := exp(-maxf(tms, 0.0) / 80.0) if tms >= 0.0 else 0.0
	var av_glow := clampf(1.0 - absf(tms - 140.0) / 60.0, 0.0, 1.0) if tms >= 0.0 else 0.0
	if tms >= 0.0:
		if tms < 120.0: _dot(ci, along(inter, tms / 120.0), s, ccol, al); _dot(ci, along(bach, tms / 100.0), s, ccol, al * 0.8)
		if tms >= 140.0 and tms < 175.0: _dot(ci, along(his, (tms - 140.0) / 35.0), s, ccol, al)
		if tms >= 175.0 and tms < 235.0:
			_dot(ci, along(br, (tms - 175.0) / 60.0), s, ccol, al); _dot(ci, along(bl, (tms - 175.0) / 60.0), s, ccol, al)
	ci.draw_circle(sa, s * 0.017, Color(ccol.lerp(Kit.RED, sa_glow), al)); Gfx.glow(ci, sa, s * (0.05 + 0.10 * sa_glow), Color(ccol, (0.25 + 0.55 * sa_glow) * al))
	ci.draw_circle(av, s * 0.014, Color(ccol.lerp(Kit.RED, av_glow), al)); Gfx.glow(ci, av, s * (0.04 + 0.07 * av_glow), Color(ccol, (0.18 + 0.5 * av_glow) * al))
	# the autonomic supply of the nodes and the muscle, when asked for
	var an := {"vagus_in": P.call(0.0, 0.03), "symp_in": P.call(-0.04, 0.20), "symp_in2": P.call(1.04, 0.52)}
	if st.has("symp") or st.has("vagus"):
		var sy := float(st.get("symp", 0.0)); var vg := float(st.get("vagus", 0.0))
		var vp := Gfx.cr_spline(PackedVector2Array([an["vagus_in"], P.call(0.10, 0.09), P.call(0.19, 0.15), P.call(0.26, 0.20), sa]), 6)
		nerve(ci, vp, para(), maxf(s * 0.008, 1.6), vg, t, al)
		var sp1 := Gfx.cr_spline(PackedVector2Array([an["symp_in"], P.call(0.08, 0.27), P.call(0.2, 0.26), sa]), 6)
		var sp2 := Gfx.cr_spline(PackedVector2Array([an["symp_in"], P.call(-0.02, 0.42), P.call(0.09, 0.60), P.call(0.22, 0.74)]), 6)
		var sp3 := Gfx.cr_spline(PackedVector2Array([an["symp_in2"], P.call(0.97, 0.62), P.call(0.90, 0.74), P.call(0.80, 0.83)]), 6)
		for pth in ([sp1, sp2, sp3] if bool(st.get("symp2", false)) else [sp1, sp2]): nerve(ci, pth, symp(), maxf(s * 0.008, 1.6), sy, t, al)
	if labels:
		var col := Color(Kit.TEXT_SOFT, 0.95 * al)
		for q in [["RA", 0.33, 0.35], ["LA", 0.69, 0.33], ["RV", 0.30, 0.70], ["LV", 0.70, 0.72]]:
			text(ci, str(q[0]), P.call(float(q[1]), float(q[2])) + Vector2(-float(fz) * 0.6, float(fz) * 0.35), col, 14, HORIZONTAL_ALIGNMENT_LEFT, -1.0, true)
	return {"sa": sa, "av": av, "his": P.call(0.5, 0.58), "apex": P.call(0.52, 0.93), "svc_top": P.call(0.25, -0.16), "arch": P.call(0.70, 0.025), "carotid": P.call(0.585, -0.075), "carotid_top": P.call(0.60, -0.16),
		"ra": P.call(0.33, 0.33), "la": P.call(0.69, 0.33), "rv": P.call(0.32, 0.70), "lv": P.call(0.70, 0.72), "vent_base": P.call(0.20, 0.74), "origin": o, "scale": s,
		"vagus_in": an["vagus_in"], "symp_in": an["symp_in"], "symp_in2": an["symp_in2"], "aorta_exit": P.call(1.02, 0.17), "ivc_bottom": P.call(0.07, 1.0)}

static func _dot(ci: CanvasItem, p: Vector2, s: float, col: Color, al: float) -> void:
	Gfx.glow(ci, p, s * 0.07, Color(col, 0.6 * al)); ci.draw_circle(p, maxf(s * 0.011, 2.0), Color(col.lightened(0.5), al))

# ---------------------------------------------------------------- lungs
const L_R := [Vector2(0.455, 0.24), Vector2(0.43, 0.14), Vector2(0.34, 0.115), Vector2(0.25, 0.13), Vector2(0.165, 0.19), Vector2(0.085, 0.32), Vector2(0.052, 0.50), Vector2(0.065, 0.69), Vector2(0.10, 0.82)]
const L_L := [Vector2(0.545, 0.24), Vector2(0.57, 0.14), Vector2(0.66, 0.115), Vector2(0.75, 0.13), Vector2(0.835, 0.19), Vector2(0.915, 0.32), Vector2(0.95, 0.50), Vector2(0.935, 0.69), Vector2(0.90, 0.82)]

static func _dome(x: float, vol: float) -> float:
	var u := (x - 0.5) / 0.5
	return 0.985 + 0.02 * vol - (0.135 - 0.06 * vol) * (1.0 - u * u)

## Frame of the lungs: x 0..1, y 0 (top of the trachea) .. 1.07 (below the diaphragm); returns [origin, scale].
static func lungs_frame(r: Rect2) -> Array:
	var s := minf(r.size.x, r.size.y / 1.08)
	return [Vector2(r.get_center().x - s * 0.5, r.get_center().y - s * 0.54), s]

static func _lung_pts(side_pts: Array, left: bool, vol: float) -> PackedVector2Array:
	var out := PackedVector2Array(); var kx := 0.90 + 0.12 * vol; var ky := 0.95 + 0.08 * vol
	var pts := []
	for p in side_pts: pts.append(Vector2(0.5 + (p.x - 0.5) * kx, 0.115 + (p.y - 0.115) * ky))
	# medial border from the apex down: the cardiac notch bites into the left lung
	var med: Array = []
	if left: med = [Vector2(0.56, 0.38), Vector2(0.575, 0.50), Vector2(0.60, 0.58), Vector2(0.585, 0.68), Vector2(0.575, 0.78)]
	else: med = [Vector2(0.455, 0.38), Vector2(0.455, 0.54), Vector2(0.46, 0.68), Vector2(0.465, 0.78)]
	var all := []
	# order: lateral base -> up the lateral side -> apex -> down the medial border -> medial base -> along the dome back to the lateral base
	var rev: Array = pts.duplicate(); rev.reverse()
	for p in rev: all.append(p)
	for p in med: all.append(Vector2(0.5 + (p.x - 0.5) * kx, 0.115 + (p.y - 0.115) * ky))
	var x_med := (0.575 if left else 0.465); var x_lat := (0.90 if left else 0.10)
	var xm := 0.5 + (x_med - 0.5) * kx; var xl := 0.5 + (x_lat - 0.5) * kx
	for k in 7:
		var x := lerpf(xm, xl, float(k) / 6.0); all.append(Vector2(x, _dome(x, vol) - 0.012))
	for p in all: out.append(p)
	return out

static func _bronchi(ci: CanvasItem, p: Vector2, ang: float, ln: float, w: float, depth: int, left: bool, mp: Callable, s: float, col: Color, tips: Array) -> void:
	var q := p + Vector2(sin(ang), cos(ang)) * ln
	ci.draw_line(mp.call(p), mp.call(q), col, maxf(w * s, 1.0), true)
	if depth <= 0: tips.append(mp.call(q)); return
	var spread := 0.62 - 0.05 * float(5 - depth)
	var sgn := -1.0 if not left else 1.0
	_bronchi(ci, q, ang + sgn * spread * 1.15, ln * 0.74, w * 0.72, depth - 1, left, mp, s, col, tips)
	_bronchi(ci, q, ang - sgn * spread * 0.85, ln * 0.70, w * 0.72, depth - 1, left, mp, s, col, tips)

## Two lungs (right with three lobes, left with two and a cardiac notch), the trachea with its rings, the bronchial tree that inflates with `vol` (0..1),
## and the diaphragm dome below. st: vol, alpha, t, labels, diaphragm (default true), tree (default true).
static func lungs(ci: CanvasItem, r: Rect2, st: Dictionary) -> Dictionary:
	var fr := lungs_frame(r); var o: Vector2 = fr[0]; var s: float = fr[1]
	var al := float(st.get("alpha", 1.0)); var vol := clampf(float(st.get("vol", 0.4)), 0.0, 1.0)
	var P := func(p: Vector2) -> Vector2: return o + p * s
	var kx := 0.90 + 0.12 * vol; var ky := 0.95 + 0.08 * vol
	var mp := func(p: Vector2) -> Vector2: return o + Vector2(0.5 + (p.x - 0.5) * kx, 0.115 + (p.y - 0.115) * ky) * s
	var cy := Color(Kit.CYAN)
	# the lungs
	var outl := {}
	for side in 2:
		var left := side == 1
		var poly := closed_spline(_px(_lung_pts(L_L if left else L_R, left, vol), o, s), 4)
		outl[side] = poly
		ci.draw_colored_polygon(poly, Color("#F3E2DC", al).lerp(Color("#E2A497", al), vol * 0.85))
		Gfx.hatch(ci, poly, -PI * 0.3 if left else PI * 0.3, maxf(s * 0.016, 4.5), Color(Kit.INK, 0.10 * al), 1.0)
		Gfx.glow(ci, centroid(poly), s * 0.42, Color(cy, (0.03 + 0.10 * vol) * al))
	# fissures: oblique and horizontal on the right, oblique on the left
	var fis := [[[Vector2(0.056, 0.60), Vector2(0.24, 0.53), Vector2(0.46, 0.70)]], [[Vector2(0.06, 0.45), Vector2(0.25, 0.43), Vector2(0.455, 0.40)]], [[Vector2(0.944, 0.60), Vector2(0.76, 0.53), Vector2(0.575, 0.66)]]]
	for f in fis:
		var line := PackedVector2Array()
		for p in f[0]: line.append(mp.call(p))
		ci.draw_polyline(Gfx.cr_spline(line, 6), Color(cy, 0.30 * al), 1.4, true)
	for side in 2:
		var poly: PackedVector2Array = outl[side]; var cl := poly.duplicate(); cl.append(poly[0])
		ci.draw_polyline(cl, Color(cy, 0.16 * al), 7.0, true); ci.draw_polyline(cl, Color(cy, (0.70 + 0.2 * vol) * al), 2.0, true)
	# trachea with cartilage rings, carina, bronchial tree
	var tr_top := P.call(Vector2(0.5, 0.0)) as Vector2; var carina := P.call(Vector2(0.5, 0.31)) as Vector2
	pipe(ci, PackedVector2Array([tr_top, carina]), s * 0.026, Color("#EFE3D3", al), Color(cy, 0.85 * al), 1.6)
	for k in 9:
		var y := lerpf(0.02, 0.29, float(k) / 8.0); ci.draw_line(P.call(Vector2(0.488, y)), P.call(Vector2(0.512, y)), Color(cy, 0.35 * al), 1.0, true)
	var tips := []
	var bcol := Color(cy.lightened(0.15), 0.78 * al)
	if bool(st.get("tree", true)):
		for side in 2:
			var left := side == 1
			var ang := (0.62 if left else -0.48)
			_bronchi(ci, Vector2(0.5, 0.31), ang, 0.12, 0.020, 5, left, mp, s, bcol, tips)
		for tp in tips: Gfx.glow(ci, tp, s * (0.018 + 0.016 * vol), Color(cy, (0.10 + 0.35 * vol) * al))   # alveoli fill as the lungs inflate
	if bool(st.get("diaphragm", true)):
		var dome := PackedVector2Array()
		for k in 41:
			var x := lerpf(0.0, 1.0, float(k) / 40.0); dome.append(o + Vector2(x, _dome(x, vol)) * s)
		var low := PackedVector2Array()
		for k in 41:
			var x := lerpf(1.0, 0.0, float(k) / 40.0); low.append(o + Vector2(x, _dome(x, vol) + 0.032) * s)
		var band := dome.duplicate(); band.append_array(low)
		ci.draw_colored_polygon(band, Color(Gfx.VIOLET.darkened(0.55), 0.95 * al))
		ci.draw_polyline(dome, Color(Gfx.VIOLET, 0.95 * al), 2.4, true)
		for k in range(2, 40, 4): ci.draw_line(dome[k], low[40 - k], Color(Gfx.VIOLET, 0.35 * al), 1.0, true)   # muscle fibres
	if st.has("phrenic"):   # the phrenic nerves, from C3-C5 beside the trachea down to the two domes, lit by the drive
		var dr := float(st["phrenic"]); var t := float(st.get("t", 0.0))
		for sd in [-1.0, 1.0]:
			var xd := 0.22 if sd < 0.0 else 0.78
			var pn := Gfx.cr_spline(PackedVector2Array([P.call(Vector2(0.5 + 0.04 * sd, 0.03)), P.call(Vector2(0.5 + 0.052 * sd, 0.2)), P.call(Vector2(0.5 + 0.058 * sd, 0.45)), P.call(Vector2(0.5 + 0.12 * sd, 0.74)), P.call(Vector2(xd, _dome(xd, vol) - 0.01))]), 8)
			nerve(ci, pn, Gfx.VIOLET, maxf(s * 0.006, 1.6), dr, t, al, 2)
	var dtop := P.call(Vector2(0.5, _dome(0.5, vol))) as Vector2
	return {"carina": carina, "trachea_top": tr_top, "dome": dtop, "dome_left": P.call(Vector2(0.78, _dome(0.78, vol))), "dome_right": P.call(Vector2(0.22, _dome(0.22, vol))), "origin": o, "scale": s,
		"rlung": P.call(Vector2(0.25, 0.40)), "llung": P.call(Vector2(0.75, 0.40)), "hilum_r": P.call(Vector2(0.40, 0.38)), "hilum_l": P.call(Vector2(0.60, 0.38))}

static func _px(a: PackedVector2Array, o: Vector2, s: float) -> PackedVector2Array:
	var out := PackedVector2Array()
	for p in a: out.append(o + p * s)
	return out

# ---------------------------------------------------------------- medulla (brainstem section)
## A transverse section of the medulla with the respiratory and autonomic nuclei. Centre c, half width w. st: hi (nucleus ids to light), act (0..1 bursting), alpha, labels.
## Nuclei ids: prebotc, rtn, nts, dmv, amb.
static func medulla(ci: CanvasItem, c: Vector2, w: float, st: Dictionary) -> Dictionary:
	var al := float(st.get("alpha", 1.0)); var hi: Array = st.get("hi", []); var act := float(st.get("act", 0.0)); var t := float(st.get("t", 0.0))
	var P := func(x: float, y: float) -> Vector2: return c + Vector2(x, y) * w
	var outline := [Vector2(-0.30, -0.62), Vector2(-0.62, -0.50), Vector2(-0.88, -0.14), Vector2(-0.86, 0.26), Vector2(-0.64, 0.58), Vector2(-0.30, 0.66), Vector2(-0.14, 0.78), Vector2(0.0, 0.70), Vector2(0.14, 0.78), Vector2(0.30, 0.66), Vector2(0.64, 0.58), Vector2(0.86, 0.26), Vector2(0.88, -0.14), Vector2(0.62, -0.50), Vector2(0.30, -0.62), Vector2(0.0, -0.54)]
	var poly := closed_spline(sc(outline, c, w), 5)
	shape(ci, poly, Color("#F0E5DC", al), Color(GRAY, 0.75 * al), 2.0, 6.0)
	# fourth ventricle floor (dorsal) and the central canal
	ci.draw_circle(P.call(0.0, -0.18), w * 0.045, Color(Kit.BENCH, al)); ci.draw_arc(P.call(0.0, -0.18), w * 0.045, 0, TAU, 12, Color(GRAY, 0.6 * al), 1.2, true)
	# pyramids (ventral) and inferior olives
	for sd in [-1.0, 1.0]:
		var pyr := closed_spline(sc([Vector2(0.04 * sd, 0.30), Vector2(0.17 * sd, 0.28), Vector2(0.22 * sd, 0.50), Vector2(0.14 * sd, 0.66), Vector2(0.04 * sd, 0.62)], c, w), 4)
		shape(ci, pyr, Color("#E7DCD2", al), Color(GRAY, 0.35 * al), 1.2)
		var ol := closed_spline(sc([Vector2(0.38 * sd, 0.12), Vector2(0.52 * sd, 0.04), Vector2(0.60 * sd, 0.22), Vector2(0.50 * sd, 0.38), Vector2(0.38 * sd, 0.30)], c, w), 4)
		shape(ci, ol, Color("#ECE1E3", al), Color(GRAY, 0.35 * al), 1.2)
	var nuclei := {"nts": [Vector2(0.34, -0.20), 0.085, Kit.GREEN], "dmv": [Vector2(0.14, -0.30), 0.06, Kit.GREEN], "amb": [Vector2(0.50, -0.02), 0.07, para()],
		"prebotc": [Vector2(0.58, 0.14), 0.10, symp()], "rtn": [Vector2(0.34, 0.50), 0.075, para()]}
	var out := {}
	for k in nuclei:
		var nd: Array = nuclei[k]; var on := hi.has(k); var col: Color = nd[2]
		for sd in [-1.0, 1.0]:
			var p := P.call(float(nd[0].x) * sd, float(nd[0].y)) as Vector2; var rr: float = float(nd[1]) * w
			Gfx.glow(ci, p, rr * (3.4 if on else 2.0), Color(col, ((0.30 + 0.5 * act) if on else 0.10) * al))
			ci.draw_circle(p, rr, Color(col.darkened(0.55), al)); ci.draw_arc(p, rr, 0, TAU, 20, Color(col, (0.95 if on else 0.55) * al), 1.8 if on else 1.2, true)
			if on:
				for q in 5: ci.draw_circle(p + Vector2(cos(float(q) * 2.4 + 0.4), sin(float(q) * 2.4)) * rr * 0.45, rr * 0.16, Color(col.lightened(0.4), 0.9 * al))
		out[k] = P.call(float(nd[0].x) * -1.0, float(nd[0].y))   # the image-left (patient right) copy for pins
		out[k + "_r"] = P.call(float(nd[0].x), float(nd[0].y))
	if bool(st.get("labels", false)):
		var lab := {"nts": "NTS", "dmv": "DMV", "amb": "amb", "prebotc": "preBötC", "rtn": "RTN"}
		for k in lab:
			if hi.has(k):
				var q: Vector2 = out[k]
				ci.draw_line(q + Vector2(-w * 0.12, 0), q + Vector2(-w * 0.30, -w * 0.20), Color(NUC, 0.5 * al), 1.2, true)
				tag(ci, str(lab[k]), q + Vector2(-w * 0.32, -w * 0.18), Color(Kit.TEXT, al), 14, HORIZONTAL_ALIGNMENT_RIGHT, true, 0.7 * al)
	out["origin"] = c; out["scale"] = w
	return out

# ---------------------------------------------------------------- spinal column and cord section
## A lateral spinal column of height h with its top at `top` (x is the column's front edge line). Cervical, thoracic and lumbar vertebrae with discs and spinous
## processes, the sacrum, and the cord in the canal. Returns the y positions of the levels (cord segment anchors).
static func spine(ci: CanvasItem, top: Vector2, h: float, st: Dictionary) -> Dictionary:
	var al := float(st.get("alpha", 1.0)); var t := float(st.get("t", 0.0)); var n := 24
	var pitch := h / 26.5; var bodies := []
	var sway := h * 0.06
	for i in n:
		var u := float(i) / float(n - 1)
		var y := top.y + pitch * (float(i) + 0.5)
		var x := top.x + sway * (sin(u * TAU * 1.0 + 0.2) * 0.9)
		var bw := h * (0.040 + 0.040 * pow(u, 0.9)) * (1.0 if i >= 7 else 0.82)
		bodies.append([Vector2(x, y), bw, pitch * 0.74])
	var canal := PackedVector2Array()
	for b in bodies: canal.append((b[0] as Vector2) + Vector2(float(b[1]) * 0.62 + pitch * 0.55, 0.0))
	canal = Gfx.cr_spline(canal, 4)
	pipe(ci, canal, pitch * 0.34, Color("#EFE5D3", al), Color(BONE, 0.30 * al), 1.4)
	nerve(ci, canal, symp(), pitch * 0.12, float(st.get("act", 0.3)), t, al, 4)
	for i in n:
		var b: Array = bodies[i]; var p: Vector2 = b[0]; var bw: float = b[1]; var bh: float = b[2]
		var sx := p.x + bw * 0.5 + pitch * 0.15
		var ulen := pitch * (0.8 if i < 7 else (1.35 if i < 19 else 1.0))
		var spin := PackedVector2Array([Vector2(sx - pitch * 0.1, p.y - bh * 0.30), Vector2(sx + pitch * 0.45, p.y - bh * 0.2), Vector2(sx + ulen * 0.85, p.y + bh * 0.55 + ulen * 0.22), Vector2(sx + ulen * 0.55, p.y + bh * 0.62 + ulen * 0.22), Vector2(sx - pitch * 0.1, p.y + bh * 0.25)])
		ci.draw_colored_polygon(spin, Color("#E9DDC4", al)); ci.draw_polyline(PackedVector2Array([spin[0], spin[1], spin[2], spin[3], spin[4]]), Color(BONE, 0.6 * al), 1.4, true)
		Gfx.rounded_rect(ci, Rect2(p.x - bw * 0.5, p.y - bh * 0.5, bw, bh), Color("#ECE0C6", al), Color(BONE, 0.75 * al), bh * 0.28, 1.4)
		if i < n - 1: Gfx.rounded_rect(ci, Rect2(p.x - bw * 0.42, p.y + bh * 0.5 + 1.0, bw * 0.84, pitch * 0.2), Color(Kit.CYAN, 0.28 * al), Color(0, 0, 0, 0), pitch * 0.1, 0.0)
	var last: Vector2 = bodies[n - 1][0]
	var sac := PackedVector2Array([last + Vector2(-h * 0.05, pitch * 0.5), last + Vector2(h * 0.045, pitch * 0.5), last + Vector2(h * 0.075, pitch * 2.6), last + Vector2(h * 0.015, pitch * 3.2), last + Vector2(-h * 0.02, pitch * 2.2)])
	if top.y + pitch * (float(n) + 3.2) <= top.y + h + pitch * 0.6:
		shape(ci, closed_spline(sac, 4), Color("#ECE0C6", al), Color(BONE, 0.7 * al), 1.4)
	var levels := {}
	for k in [["C5", 4], ["T6", 12], ["L2", 18], ["L4", 21]]: levels[k[0]] = (bodies[int(k[1])][0] as Vector2)
	levels["canal"] = canal; levels["pitch"] = pitch
	return levels

## A transverse section of the spinal cord (one segment) with the reflex arc: dorsal root and ganglion (spindle afferent), dorsal and ventral horns, motor neurons,
## Renshaw cell, ventral root. Centre c, half width w. st: t, act (0..1 how busy), alpha, labels.
static func cord_section(ci: CanvasItem, c: Vector2, w: float, st: Dictionary) -> Dictionary:
	var al := float(st.get("alpha", 1.0)); var act := float(st.get("act", 0.35)); var t := float(st.get("t", 0.0)); var hi: Array = st.get("hi", [])
	var P := func(x: float, y: float) -> Vector2: return c + Vector2(x, y) * w
	var white := [Vector2(-0.90, -0.05), Vector2(-0.80, -0.55), Vector2(-0.40, -0.84), Vector2(0.0, -0.80), Vector2(0.40, -0.84), Vector2(0.80, -0.55), Vector2(0.90, -0.05), Vector2(0.80, 0.50), Vector2(0.42, 0.80), Vector2(0.0, 0.84), Vector2(-0.42, 0.80), Vector2(-0.80, 0.50)]
	var wp := closed_spline(sc(white, c, w), 6)
	Gfx.glow(ci, c, w * 1.5, Color(GRAY, 0.06 * al))
	shape(ci, wp, Color("#F3EBDF", al), Color(GRAY, 0.8 * al), 2.2, 8.0)
	for sd in [-1.0, 1.0]:   # tracts hinted as faint arcs in the white matter
		ci.draw_arc(c + Vector2(sd * w * 0.0, 0.0), w * 0.74, (-0.95 if sd > 0 else PI + 0.45), (-0.45 if sd > 0 else PI + 0.95), 12, Color(GRAY, 0.12 * al), w * 0.10, true)
	ci.draw_line(P.call(0.0, -0.80), P.call(0.0, -0.40), Color(GRAY, 0.5 * al), 1.4, true)   # posterior median septum
	ci.draw_line(P.call(0.0, 0.84), P.call(0.0, 0.52), Color(Kit.BENCH, al), w * 0.03, true)   # anterior median fissure
	# the grey matter: a butterfly, dorsal horns slim and long, ventral horns broad
	var gray := [Vector2(0.0, -0.20), Vector2(-0.08, -0.42), Vector2(-0.20, -0.74), Vector2(-0.34, -0.74), Vector2(-0.38, -0.48), Vector2(-0.50, -0.22), Vector2(-0.64, -0.14), Vector2(-0.64, 0.14), Vector2(-0.60, 0.40), Vector2(-0.44, 0.58), Vector2(-0.24, 0.52), Vector2(-0.16, 0.30), Vector2(-0.08, 0.20),
		Vector2(0.0, 0.20), Vector2(0.08, 0.20), Vector2(0.16, 0.30), Vector2(0.24, 0.52), Vector2(0.44, 0.58), Vector2(0.60, 0.40), Vector2(0.64, 0.14), Vector2(0.64, -0.14), Vector2(0.50, -0.22), Vector2(0.38, -0.48), Vector2(0.34, -0.74), Vector2(0.20, -0.74), Vector2(0.08, -0.42)]
	var gp := closed_spline(sc(gray, c, w), 5)
	shape(ci, gp, Color("#E4D1CB", al), Color(GRAY, 0.55 * al), 1.6)
	Gfx.stipple_poly(ci, gp, 220, Color(Kit.INK, 0.28 * al), 3, 1.2)   # grey matter: stippled, as in a Nissl plate
	ci.draw_circle(P.call(0.0, 0.0), w * 0.035, Color(Kit.BENCH, al)); ci.draw_arc(P.call(0.0, 0.0), w * 0.035, 0, TAU, 12, Color(GRAY, 0.7 * al), 1.2, true)   # central canal
	# the reflex arc on the right-hand side (image right): dorsal root in, ventral root out
	var dr := Gfx.cr_spline(PackedVector2Array([P.call(0.28, -0.74), P.call(0.48, -0.98), P.call(0.86, -1.10), P.call(1.25, -1.12)]), 8)
	var vr := Gfx.cr_spline(PackedVector2Array([P.call(0.40, 0.44), P.call(0.62, 0.66), P.call(0.95, 0.80), P.call(1.28, 0.82)]), 8)
	var mn_p := P.call(0.44, 0.30) as Vector2; var ia_syn := P.call(0.34, -0.50) as Vector2
	var ren := P.call(0.14, 0.40) as Vector2
	var afferent := PackedVector2Array(); for i in range(dr.size() - 1, -1, -1): afferent.append(dr[i])
	afferent.append(P.call(0.34, -0.50)); afferent.append(P.call(0.38, -0.05)); afferent.append(mn_p + Vector2(0.0, -w * 0.02))
	var efferent := PackedVector2Array([mn_p]); efferent.append(P.call(0.46, 0.46)); for i in range(2, vr.size()): efferent.append(vr[i])
	nerve(ci, afferent, Kit.GREEN, maxf(w * 0.018, 1.8), 0.6 * act + 0.2, t, al)
	nerve(ci, efferent, Gfx.VIOLET, maxf(w * 0.022, 2.0), 0.6 * act + 0.2, t * 1.2, al)
	# dorsal root ganglion on the root
	var drg := along(dr, 0.55); ci.draw_circle(drg, w * 0.065, Color("#D3E6E2", al)); ci.draw_arc(drg, w * 0.065, 0, TAU, 14, Color(Kit.GREEN, 0.9 * al), 1.6, true)
	# motor neuron (large, multipolar) and Renshaw cell
	var mn_on := hi.has("mn")
	Gfx.glow(ci, mn_p, w * (0.34 if mn_on else 0.22), Color(Gfx.VIOLET, (0.35 if mn_on else 0.18) * al))
	var soma := PackedVector2Array()
	for i in 14:
		var a := TAU * float(i) / 14.0; var rr := w * (0.075 if i % 2 == 0 else 0.052); soma.append(mn_p + Vector2(cos(a), sin(a)) * rr)
	ci.draw_colored_polygon(soma, Color(Gfx.VIOLET.darkened(0.15), al)); ci.draw_circle(mn_p, w * 0.028, Color(Gfx.VIOLET.darkened(0.6), al))
	for k in 5:   # dendrites
		var a := -0.9 + 0.5 * float(k); ci.draw_line(mn_p, mn_p + Vector2(cos(a) * -1.0, sin(a)) * w * 0.20, Color(Gfx.VIOLET, 0.55 * al), 1.3, true)
	var ren_on := hi.has("renshaw")
	Gfx.glow(ci, ren, w * (0.26 if ren_on else 0.14), Color(para(), (0.45 if ren_on else 0.22) * al))
	ci.draw_circle(ren, w * 0.042, Color(para().darkened(0.2), al)); ci.draw_arc(ren, w * 0.042, 0, TAU, 12, Color(para(), al), 1.4, true)
	var back := PackedVector2Array([mn_p + Vector2(-w * 0.03, w * 0.07), P.call(0.30, 0.50), ren + Vector2(w * 0.02, w * 0.04)])   # axon collateral to the Renshaw cell, which inhibits the motor neuron back
	ci.draw_polyline(Gfx.cr_spline(back, 4), Color(Gfx.VIOLET, 0.65 * al), 1.4, true)
	var inh_p := PackedVector2Array([ren + Vector2(-w * 0.02, 0.0), P.call(0.20, 0.18), mn_p + Vector2(-w * 0.07, -w * 0.01)])
	ci.draw_polyline(Gfx.cr_spline(inh_p, 4), Color(para(), 0.8 * al), 1.4, true); Gfx.bar_head(ci, inh_p[inh_p.size() - 1], (inh_p[inh_p.size() - 1] - inh_p[inh_p.size() - 2]), Color(para(), al), w * 0.07, 1.4)
	# interneuron on the other side (the antagonist's inhibition), faint
	if bool(st.get("labels", false)):
		var f := Kit.fs(14)
		tag(ci, "dorsal horn", P.call(-0.74, -0.58) + Vector2(0, 0), Color(Kit.TEXT_SOFT, al), 14, HORIZONTAL_ALIGNMENT_LEFT, false, 0.0)
		tag(ci, "ventral horn", P.call(-0.74, 0.50), Color(Kit.TEXT_SOFT, al), 14, HORIZONTAL_ALIGNMENT_LEFT, false, 0.0)
		tag(ci, "Ia", P.call(0.98, -1.06), Color(Kit.GREEN, al), 14, HORIZONTAL_ALIGNMENT_LEFT, true, 0.0)
	return {"dorsal_horn": ia_syn, "mn": mn_p, "renshaw": ren, "drg": drg, "dorsal_root_end": dr[dr.size() - 1], "ventral_root_end": vr[vr.size() - 1], "origin": c, "scale": w,
		"ventral_horn": P.call(-0.40, 0.30), "afferent": afferent, "efferent": efferent}

# ---------------------------------------------------------------- the arm: bones, muscles, spindles
## An arm in the plane: shoulder, elbow, hand in px (the forearm rotates). Biceps-like agonist on the side the hand turns toward, antagonist behind, each a
## fusiform belly that bends around the elbow and swells with activation, with a muscle spindle and its Ia afferent. st: ag, ant (0..1), ia_ag, ia_ant (0..1),
## t, alpha, labels.
static func limb(ci: CanvasItem, sh: Vector2, el: Vector2, hd: Vector2, st: Dictionary) -> Dictionary:
	var al := float(st.get("alpha", 1.0)); var t := float(st.get("t", 0.0)); var u := maxf(el.distance_to(hd), 20.0)
	var d := (hd - el) / u; var dh := (el - sh).normalized(); var lh := sh.distance_to(el)
	var nf := Vector2(d.y, -d.x); var nhh := Vector2(dh.y, -dh.x)
	var uw := u * 0.115
	var skin := Color(Gfx.BODY, 0.55 * al)
	# soft tissue silhouette
	Gfx.tube_rim(ci, PackedVector2Array([sh, el]), func(q): return uw * (1.55 - 0.25 * q), Color(TISSUE, 0.85 * al), Color(skin, 0.35), 1.6)
	Gfx.tube_rim(ci, PackedVector2Array([el, el.lerp(hd, 0.5), hd]), func(q): return uw * (1.35 - 0.50 * q), Color(TISSUE, 0.85 * al), Color(skin, 0.35), 1.6)
	# bones
	Gfx.tube_rim(ci, PackedVector2Array([sh, el]), func(q): return uw * (0.34 + 0.06 * absf(q - 0.5)), Color("#EDE2C8", al), Color(BONE, 0.85 * al), 1.4)
	Gfx.tube_rim(ci, PackedVector2Array([el, hd]), func(q): return uw * (0.30 - 0.12 * q), Color("#EDE2C8", al), Color(BONE, 0.85 * al), 1.4)
	var out := {}
	for k in 2:
		var ag := k == 0
		var act := clampf(float(st.get("ag" if ag else "ant", 0.2)), 0.0, 1.0); var ia := clampf(float(st.get("ia_ag" if ag else "ia_ant", 0.0)), 0.0, 1.0)
		var sd := 1.0 if ag else -1.0
		var col := Kit.exc() if ag else Kit.inh()
		var o_pt := sh + dh * lh * (0.28 if ag else 0.22) + nhh * sd * uw * (1.1 if ag else 1.0)
		var i_pt := el + d * u * (0.20 if ag else 0.06) + nf * sd * uw * (1.0 if ag else 0.55)
		var bow := (nhh + nf).normalized() * sd
		var ctrl := (o_pt + i_pt) * 0.5 + bow * uw * (2.1 if ag else 2.6)
		var path := bez2(o_pt, ctrl, i_pt, 22)
		var wmax := uw * (0.70 + 0.95 * act)
		var left := PackedVector2Array(); var right := PackedVector2Array()
		for i in path.size():
			var q := float(i) / float(path.size() - 1)
			var tg := (path[mini(i + 1, path.size() - 1)] - path[maxi(i - 1, 0)]).normalized(); var nn := Vector2(tg.y, -tg.x)
			var hw := wmax * (0.10 + 0.90 * pow(sin(PI * clampf(q * 0.86 + 0.07, 0.0, 1.0)), 0.75))
			left.append(path[i] + nn * hw); right.append(path[i] - nn * hw)
		right.reverse(); left.append_array(right)
		var fill := Color("#F1DDD2").lerp(col.darkened(0.35), 0.35 + 0.65 * act)
		Gfx.glow(ci, path[path.size() / 2], u * 0.5, Color(col, 0.28 * act * al))
		ci.draw_colored_polygon(left, Color(fill, 0.96 * al))
		var cl := left.duplicate(); cl.append(left[0])
		ci.draw_polyline(cl, Color(col, (0.55 + 0.45 * act) * al), 1.8, true)
		for fi in 3:   # muscle fibres
			var off := (float(fi) - 1.0) * 0.5; var fb := PackedVector2Array()
			for i in range(2, path.size() - 2):
				var q := float(i) / float(path.size() - 1); var tg := (path[i + 1] - path[i - 1]).normalized(); var nn := Vector2(tg.y, -tg.x)
				var hw := wmax * (0.10 + 0.90 * pow(sin(PI * clampf(q * 0.86 + 0.07, 0.0, 1.0)), 0.75))
				fb.append(path[i] + nn * hw * off * 0.75)
			ci.draw_polyline(fb, Color(col.lightened(0.3), (0.12 + 0.35 * act) * al), 1.0, true)
		ci.draw_polyline(PackedVector2Array([o_pt, path[0]]), Color(col, 0.8 * al), 2.0, true); ci.draw_polyline(PackedVector2Array([path[path.size() - 1], i_pt]), Color(col, 0.8 * al), 2.0, true)
		# the spindle sits in the belly, with its Ia afferent running away from the limb
		var mid := path[path.size() / 2]; var tgm := (path[path.size() / 2 + 1] - path[path.size() / 2 - 1]).normalized(); var nm := Vector2(tgm.y, -tgm.x)
		var sl := uw * 1.7
		var sp_a := mid - tgm * sl * 0.5; var sp_b := mid + tgm * sl * 0.5
		Gfx.glow(ci, mid, uw * (1.6 + 2.2 * ia), Color(Kit.GREEN, (0.10 + 0.55 * ia) * al))
		ci.draw_line(sp_a, sp_b, Color(Kit.GREEN, 0.95 * al), uw * 0.44, true); ci.draw_line(sp_a, sp_b, Color("#E6F0EC", al), uw * 0.20, true)
		for ri in 3: var rp := mid + tgm * sl * (float(ri) - 1.0) * 0.28; ci.draw_line(rp + nm * uw * 0.30, rp - nm * uw * 0.30, Color(Kit.GREEN.lightened(0.4), al), 1.2, true)
		var away := nm * sd
		var aff := Gfx.cr_spline(PackedVector2Array([mid + nm * uw * 0.25, mid + away * uw * 2.2, mid + away * uw * 3.2 + (-dh) * uw * 1.2, mid + away * uw * 3.8 + (-dh) * uw * 3.2]), 6)
		nerve(ci, aff, Kit.GREEN, maxf(uw * 0.16, 1.4), ia, t, al, 2)
		out["ag" if ag else "ant"] = mid; out["ag_aff" if ag else "ant_aff"] = aff[aff.size() - 1]; out["ag_path" if ag else "ant_path"] = path
	# the joint and the hand
	ci.draw_circle(sh, uw * 0.85, Color("#EDE2C8", al)); ci.draw_arc(sh, uw * 0.85, 0, TAU, 20, Color(BONE, 0.85 * al), 1.6, true)
	ci.draw_circle(el, uw * 0.78, Color("#EDE2C8", al)); ci.draw_arc(el, uw * 0.78, 0, TAU, 20, Color(BONE, 0.9 * al), 1.8, true); ci.draw_circle(el, uw * 0.18, Color(Gfx.BODY, al))
	var hand := PackedVector2Array([hd - nf * uw * 0.7, hd + nf * uw * 0.7, hd + nf * uw * 0.55 + d * uw * 1.5, hd - nf * uw * 0.55 + d * uw * 1.3])
	shape(ci, closed_spline(hand, 4), Color("#EDE2C8", al), Color(BONE, 0.85 * al), 1.4)
	out["shoulder"] = sh; out["elbow"] = el; out["hand"] = hd; out["uw"] = uw; out["u"] = u; out["d"] = d; out["n"] = nf
	return out

# ---------------------------------------------------------------- the eye
## An eye: sclera, iris with radial fibres (the dilator, amber, lights with `dilator`), the sphincter ring at the pupil margin (cyan, thickens with `sphincter`),
## the pupil of `pupil_mm` and a catch-light. c = centre, ew = half width of the lid opening.
static func eye(ci: CanvasItem, c: Vector2, ew: float, st: Dictionary) -> Dictionary:
	var al := float(st.get("alpha", 1.0)); var eh := ew * 0.62
	var pmm := float(st.get("pupil_mm", 4.0)); var sph := clampf(float(st.get("sphincter", 0.4)), 0.0, 1.0); var dil := clampf(float(st.get("dilator", 0.2)), 0.0, 1.0)
	var lid := PackedVector2Array()
	for i in 33:
		var uu := float(i) / 32.0; lid.append(c + Vector2((uu - 0.5) * 2.0 * ew, -sin(uu * PI) * eh * (1.0 - 0.10 * (uu - 0.5) * 2.0)))
	for i in 33:
		var uu := 1.0 - float(i) / 32.0; lid.append(c + Vector2((uu - 0.5) * 2.0 * ew, sin(uu * PI) * eh * 0.85))
	ci.draw_colored_polygon(lid, Color("#FBF7EE", al))
	Gfx.glow(ci, c, ew * 1.4, Color(Gfx.FISH, 0.05 * al))
	var ir := eh * 0.86; var pr := ir * clampf(pmm / 12.0, 0.1, 0.85)
	# iris body
	ci.draw_circle(c, ir, Color("#8AA3BA", al)); ci.draw_circle(c, ir * 0.9, Color("#B4C6D4", al))
	# radial fibres: the dilator pupillae, drawn from the pupil margin to the iris periphery
	var dcol := symp()
	for i in 72:
		var a := TAU * float(i) / 72.0; var dirv := Vector2(cos(a), sin(a)); var j := 0.5 + 0.5 * sin(float(i) * 7.3)
		ci.draw_line(c + dirv * pr * 1.12, c + dirv * ir * (0.78 + 0.12 * j), Color(Color("#3E5E7E").lerp(dcol, dil * 0.85), (0.18 + 0.20 * j + 0.40 * dil) * al), 1.0 + 0.9 * dil, true)
	ci.draw_arc(c, ir * 0.52, 0, TAU, 40, Color(Kit.INK, 0.25 * al), 1.2, true)   # collarette
	# sphincter: a ring round the pupil margin
	var sw := ir * (0.06 + 0.10 * sph)
	ci.draw_arc(c, pr + sw * 0.5, 0, TAU, 48, Color(para().lerp(Color.WHITE, 0.15), (0.35 + 0.6 * sph) * al), sw, true)
	ci.draw_circle(c, pr, Color("#231C17", al))
	ci.draw_arc(c, ir, 0, TAU, 48, Color(Kit.INK, 0.85 * al), 1.6, true)
	ci.draw_circle(c + Vector2(-pr * 0.35, -pr * 0.40), maxf(pr * 0.17, 1.6), Color(Gfx.SHEET, 0.9 * al))
	var lc := lid.duplicate(); lc.append(lid[0])
	ci.draw_polyline(lc, Color(Gfx.FISH, 0.9 * al), 3.0, true)
	ci.draw_arc(c + Vector2(0, -eh * 0.55), ew * 1.0, PI * 1.12, PI * 1.88, 24, Color(Gfx.FISH, 0.35 * al), 2.0, true)   # brow line
	return {"iris": c, "ir": ir, "pr": pr, "sphincter": c + Vector2(pr + sw, 0).rotated(-0.7), "dilator": c + Vector2(ir * 0.7, 0).rotated(2.3), "pupil": c}

# ---------------------------------------------------------------- the gut
## A segment of bowel as a tube: eight circular-muscle segments that constrict with seg0..seg7 (0..1), the bolus in the lumen, and the wall that distends ahead of it.
## st: seg (Array of 8), bolus (0..1), ex / inh (Arrays of 8, 0..1: ACh / NO drive per segment), t, labels.
static func gut(ci: CanvasItem, r: Rect2, st: Dictionary) -> Dictionary:
	var al := float(st.get("alpha", 1.0)); var n := 8
	var seg: Array = st.get("seg", []); var ex: Array = st.get("ex", []); var inh: Array = st.get("inh", [])
	var bolus := clampf(float(st.get("bolus", 0.0)), 0.0, 1.0)
	var R := minf(r.size.y * 0.24, r.size.x / float(n) * 0.78)
	var x0 := r.position.x + R * 0.9; var x1 := r.end.x - R * 0.9; var cy := r.get_center().y
	var sway := R * 0.35
	var N := 96; var top := PackedVector2Array(); var bot := PackedVector2Array(); var mid := PackedVector2Array(); var radii := []
	var bx := x0 + (x1 - x0) * bolus
	for i in N + 1:
		var q := float(i) / float(N); var x := lerpf(x0, x1, q)
		var si := q * float(n) - 0.5; var a := 0.0
		var i0 := clampi(int(floor(si)), 0, n - 1); var i1 := clampi(i0 + 1, 0, n - 1); var f := clampf(si - floor(si), 0.0, 1.0)
		var g := func(k: int) -> float: return clampf(float(seg[k]), 0.0, 1.0) if k < seg.size() else 0.0
		a = lerpf(float(g.call(i0)), float(g.call(i1)), f * f * (3.0 - 2.0 * f))
		var dist := exp(-pow((x - bx) / (R * 1.3), 2.0)) * (0.30 if bolus > 0.001 and bolus < 0.999 else 0.0)
		var rad := R * (1.0 - 0.62 * a + dist)
		var y := cy + sin(q * TAU * 0.9) * sway
		mid.append(Vector2(x, y)); top.append(Vector2(x, y - rad)); bot.append(Vector2(x, y + rad)); radii.append(rad)
	# the wall: outer rim, lumen
	var outer := PackedVector2Array(); var inner := PackedVector2Array()
	for p in top: outer.append(p + Vector2(0, -R * 0.20))
	for i in range(bot.size() - 1, -1, -1): outer.append(bot[i] + Vector2(0, R * 0.20))
	ci.draw_colored_polygon(outer, Color("#EBCDB7", al))
	Gfx.hatch(ci, outer, PI * 0.3, maxf(r.size.y * 0.025, 5.0), Color(Kit.INK, 0.10 * al), 1.0)   # engraver's shade on the muscle wall
	var oc := outer.duplicate(); oc.append(outer[0]); ci.draw_polyline(oc, Color(Kit.exc(), 0.55 * al), 2.0, true)
	var lumen := top.duplicate()
	for i in range(bot.size() - 1, -1, -1): lumen.append(bot[i])
	ci.draw_colored_polygon(lumen, Color("#F6EEDF", al))
	var lc := lumen.duplicate(); lc.append(lumen[0]); ci.draw_polyline(lc, Color(Kit.exc(), 0.30 * al), 1.2, true)
	# circular muscle bands, coloured by ACh (amber) and NO (cyan)
	for i in n:
		var q := (float(i) + 0.5) / float(n); var j := int(q * float(N)); var a := clampf(float(seg[i]), 0.0, 1.0) if i < seg.size() else 0.0
		var e := clampf(float(ex[i]), 0.0, 1.0) if i < ex.size() else a
		var h := clampf(float(inh[i]), 0.0, 1.0) if i < inh.size() else 0.0
		var rad: float = radii[j]; var p: Vector2 = mid[j]
		var bw := (x1 - x0) / float(n) * 0.30
		for b in 3:
			var xo := (float(b) - 1.0) * bw * 0.9
			var jj := clampi(int((q + xo / (x1 - x0)) * float(N)), 0, N); var pp: Vector2 = mid[jj]; var rr: float = radii[jj] + R * 0.14
			var col := Color(Kit.exc(), 0.25 + 0.7 * e).lerp(Color(Kit.CYAN, 0.45 + 0.4 * h), clampf(h - e, 0.0, 1.0))
			ci.draw_line(pp + Vector2(0, -rr), pp + Vector2(0, rr), Color(col, col.a * al), 1.6 + 2.6 * e, true)
		if bool(st.get("labels", true)): text(ci, "%d" % (i + 1), Vector2(p.x - 6.0, cy + R * 1.45 + float(Kit.fs(14)) + sway), Color(Kit.TEXT_SOFT, al), 14)
	# longitudinal muscle: a faint line along each wall
	ci.draw_polyline(top, Color(Kit.exc(), 0.35 * al), 1.2, true); ci.draw_polyline(bot, Color(Kit.exc(), 0.35 * al), 1.2, true)
	# the bolus, deformed by the squeeze
	var bj := clampi(int(bolus * float(N)), 0, N); var bp: Vector2 = mid[bj]
	var brad: float = minf(float(radii[bj]) * 0.78, R * 0.8)
	Gfx.glow(ci, bp, R * 1.5, Color(1, 1, 1, 0.16 * al))
	var el := PackedVector2Array()
	for i in 28:
		var a := TAU * float(i) / 28.0; el.append(bp + Vector2(cos(a) * R * 1.05, sin(a) * brad))
	ci.draw_colored_polygon(el, Color("#E2C48E", 0.93 * al)); var elc := el.duplicate(); elc.append(el[0]); ci.draw_polyline(elc, Color(Kit.INK, 0.9 * al), 1.4, true)
	return {"mid": mid, "R": R, "bolus": bp, "x0": x0, "x1": x1, "cy": cy}

# ---------------------------------------------------------------- the autonomic body
## A front view of head, neck and trunk with the CNS, both divisions of the autonomic system, and the organs (eyes, lungs, heart, stomach, bowel, adrenal).
## Frame: the body fits the rect's height; centreline at the rect centre. st: sy (sympathetic activity 0..1), pa (parasympathetic 0..1), t, alpha, beat (0..1 phase), pupil (0..1),
## labels, hi (organs to light). Returns the pin anchors.
static func body(ci: CanvasItem, r: Rect2, st: Dictionary) -> Dictionary:
	var al := float(st.get("alpha", 1.0)); var t := float(st.get("t", 0.0)); var sy := float(st.get("sy", 0.4)); var pa := float(st.get("pa", 0.5))
	var s := r.size.y; var cx := r.get_center().x; var top := r.position.y
	var P := func(x: float, y: float) -> Vector2: return Vector2(cx + x * s, top + y * s)
	var sk := Color(GRAY, 0.5 * al)
	# silhouette: head, neck, trunk
	var head := closed_spline(PackedVector2Array([P.call(0.0, 0.0) as Vector2, P.call(0.062, 0.012), P.call(0.085, 0.07), P.call(0.07, 0.14), P.call(0.04, 0.178), P.call(-0.04, 0.178), P.call(-0.07, 0.14), P.call(-0.085, 0.07), P.call(-0.062, 0.012)]), 5)
	var trunk := closed_spline(PackedVector2Array([P.call(-0.03, 0.17) as Vector2, P.call(-0.07, 0.19), P.call(-0.20, 0.215), P.call(-0.255, 0.26), P.call(-0.245, 0.40), P.call(-0.20, 0.52), P.call(-0.17, 0.62), P.call(-0.19, 0.70), P.call(-0.12, 0.745), P.call(0.12, 0.745), P.call(0.19, 0.70), P.call(0.17, 0.62), P.call(0.20, 0.52), P.call(0.245, 0.40), P.call(0.255, 0.26), P.call(0.20, 0.215), P.call(0.07, 0.19), P.call(0.03, 0.17)]), 4)
	shape(ci, trunk, Color("#F4EBDB", 0.9 * al), sk, 2.0, 6.0); shape(ci, head, Color("#F4EBDB", 0.9 * al), sk, 2.0, 6.0)
	# the CNS: brainstem and cord
	var cord := PackedVector2Array([P.call(0.0, 0.12) as Vector2, P.call(0.0, 0.20), P.call(0.0, 0.42), P.call(0.0, 0.56), P.call(0.0, 0.62)])
	var brain := closed_spline(PackedVector2Array([P.call(-0.045, 0.045) as Vector2, P.call(0.0, 0.025), P.call(0.045, 0.045), P.call(0.04, 0.10), P.call(0.016, 0.145), P.call(-0.016, 0.145), P.call(-0.04, 0.10)]), 5)
	shape(ci, brain, Color("#ECE1E3", al), Color(NUC, 0.75 * al), 1.6, 4.0)
	ci.draw_polyline(cord, Color(NUC, 0.20 * al), s * 0.016, true); ci.draw_polyline(cord, Color(NUC, 0.6 * al), s * 0.007, true)
	var out := {}
	# lungs and heart (behind the nerves)
	lungs(ci, Rect2(cx - s * 0.185, top + s * 0.205, s * 0.37, s * 0.27), {"vol": 0.35 + 0.1 * sin(t * 1.4), "alpha": 0.6 * al, "diaphragm": false, "tree": true})
	var hrect := Rect2(cx - s * 0.075, top + s * 0.265, s * 0.15, s * 0.16)
	var hres := heart(ci, hrect, {"alpha": 0.95 * al, "tms": float(st.get("tms", -1.0))})
	out["heart"] = (hres["ra"] as Vector2).lerp(hres["lv"], 0.5)
	out["lungs"] = P.call(0.13, 0.33)
	# eyes
	var eyes := []
	for sd in [-1.0, 1.0]:
		var ec := P.call(0.033 * sd, 0.07) as Vector2
		var res := eye(ci, ec, s * 0.020, {"pupil_mm": 7.0 - 5.0 * float(st.get("pupil", 0.5)), "sphincter": 1.0 - float(st.get("pupil", 0.5)), "dilator": float(st.get("pupil", 0.5)), "alpha": al})
		eyes.append(ec)
	out["eye"] = eyes[0]; out["eye_r"] = eyes[1]
	# stomach, small bowel, colon
	var stom := closed_spline(PackedVector2Array([P.call(-0.10, 0.455) as Vector2, P.call(-0.03, 0.44), P.call(0.04, 0.47), P.call(0.03, 0.53), P.call(-0.04, 0.565), P.call(-0.11, 0.545), P.call(-0.135, 0.50)]), 5)
	shape(ci, stom, Color("#EFD5C2", 0.9 * al), Color(Kit.exc(), 0.5 * al), 1.4, 4.0)
	var colon := Gfx.cr_spline(PackedVector2Array([P.call(0.15, 0.68) as Vector2, P.call(0.16, 0.58), P.call(0.15, 0.50), P.call(0.0, 0.485), P.call(-0.15, 0.50), P.call(-0.16, 0.58), P.call(-0.15, 0.67), P.call(-0.08, 0.715), P.call(0.0, 0.72)]), 6)
	pipe(ci, colon, s * 0.011, Color("#EBCDB7", 0.9 * al), Color(Kit.exc(), 0.40 * al), 1.2)
	var bowel := Gfx.cr_spline(PackedVector2Array([P.call(0.02, 0.56) as Vector2, P.call(0.07, 0.575), P.call(-0.07, 0.59), P.call(0.08, 0.61), P.call(-0.08, 0.63), P.call(0.07, 0.65), P.call(-0.06, 0.665), P.call(0.04, 0.68), P.call(0.0, 0.70)]), 6)
	pipe(ci, bowel, s * 0.0085, Color("#EFD5C2", 0.9 * al), Color(Kit.exc(), 0.55 * al), 1.2)
	var bp := along(bowel, fposmod(t * 0.07, 1.0)); Gfx.glow(ci, bp, s * 0.04, Color(1, 1, 1, 0.28 * al)); ci.draw_circle(bp, s * 0.009, Color("#E2C48E", al))
	out["gut"] = along(bowel, 0.34)
	# kidneys with adrenal glands
	for sd in [-1.0, 1.0]:
		var kc := P.call(0.125 * sd, 0.425) as Vector2
		var kid := closed_spline(PackedVector2Array([kc + Vector2(-0.018, -0.03) * s, kc + Vector2(0.018, -0.03) * s, kc + Vector2(0.024, 0.0) * s, kc + Vector2(0.018, 0.03) * s, kc + Vector2(-0.018, 0.03) * s, kc + Vector2(-0.024, 0.0) * s]), 4)
		shape(ci, kid, Color("#E6C4C4", al), Color(Color("#9C4A5A"), 0.45 * al), 1.2)
		var ad := kc + Vector2(-0.004 * sd, -0.04) * s; ci.draw_circle(ad, s * 0.008, Color(symp(), 0.95 * al)); Gfx.glow(ci, ad, s * 0.035, Color(symp(), 0.25 * al * (0.4 + sy)))
		if sd < 0: out["adrenal"] = ad
	# sympathetic chain (both sides), cord outflow T1-L2, postganglionic fibres
	var sc_col := symp(); var pc_col := para()
	for sd in [-1.0, 1.0]:
		var chain := PackedVector2Array()
		for i in 13: chain.append(P.call(0.03 * sd, 0.145 + 0.038 * float(i)))
		ci.draw_polyline(chain, Color(sc_col, 0.35 * al), 1.6, true)
		for i in 13:
			ci.draw_circle(chain[i], s * 0.0042, Color(sc_col, 0.85 * al))
		for i in 6:   # white rami: spinal cord T1-L2 to the chain
			var yy := 0.235 + 0.045 * float(i); ci.draw_line(P.call(0.004 * sd, yy), P.call(0.028 * sd, yy + 0.005), Color(sc_col, 0.7 * al), 1.4, true)
		out["chain" if sd < 0 else "chain_r"] = chain
	# postganglionic: superior cervical ganglion -> eye, stellate -> heart, thoracic -> lungs, celiac -> gut and adrenals
	var f_eye := Gfx.cr_spline(PackedVector2Array([P.call(-0.03, 0.15) as Vector2, P.call(-0.075, 0.13), P.call(-0.085, 0.095), P.call(-0.052, 0.074)]), 6)
	var f_heart := Gfx.cr_spline(PackedVector2Array([P.call(-0.03, 0.225) as Vector2, P.call(-0.05, 0.29), P.call(-0.03, 0.335)]), 6)
	var f_lung := Gfx.cr_spline(PackedVector2Array([P.call(-0.03, 0.30) as Vector2, P.call(-0.09, 0.32), P.call(-0.125, 0.34)]), 6)
	var f_gut := Gfx.cr_spline(PackedVector2Array([P.call(-0.03, 0.40) as Vector2, P.call(-0.06, 0.45), P.call(-0.035, 0.50), P.call(-0.01, 0.57)]), 6)
	var f_adr := Gfx.cr_spline(PackedVector2Array([P.call(-0.03, 0.37) as Vector2, P.call(-0.085, 0.38), P.call(-0.125, 0.385)]), 6)
	for pth in [f_eye, f_heart, f_lung, f_gut, f_adr]: nerve(ci, pth, sc_col, maxf(s * 0.0035, 1.4), sy, t, al, 2)
	# parasympathetic: cranial (CN III to the eye, vagus to heart, lungs, gut) and sacral (pelvic splanchnic to the colon)
	var f3 := Gfx.cr_spline(PackedVector2Array([P.call(0.008, 0.06) as Vector2, P.call(0.02, 0.058), P.call(0.032, 0.062)]), 6)
	var vag := Gfx.cr_spline(PackedVector2Array([P.call(0.012, 0.115) as Vector2, P.call(0.048, 0.19), P.call(0.054, 0.27), P.call(0.04, 0.35), P.call(0.03, 0.43), P.call(0.012, 0.52)]), 6)
	var vag_l := Gfx.cr_spline(PackedVector2Array([P.call(-0.012, 0.115) as Vector2, P.call(-0.048, 0.19), P.call(-0.058, 0.27), P.call(-0.05, 0.35), P.call(-0.035, 0.43), P.call(-0.015, 0.52)]), 6)
	var v_heart := Gfx.cr_spline(PackedVector2Array([P.call(0.054, 0.27) as Vector2, P.call(0.03, 0.31), P.call(0.012, 0.345)]), 5)
	var v_lung := Gfx.cr_spline(PackedVector2Array([P.call(0.054, 0.285) as Vector2, P.call(0.09, 0.30), P.call(0.125, 0.33)]), 5)
	var sacral := Gfx.cr_spline(PackedVector2Array([P.call(0.004, 0.60) as Vector2, P.call(0.06, 0.64), P.call(0.13, 0.66), P.call(0.145, 0.675)]), 6)
	for pth in [f3, vag, vag_l, v_heart, v_lung, sacral]: nerve(ci, pth, pc_col, maxf(s * 0.0035, 1.4), pa, t, al, 2)
	out["vagus"] = along(vag, 0.55); out["cn3"] = along(f3, 0.5); out["sacral"] = along(sacral, 0.55); out["chain_mid"] = P.call(-0.03, 0.34)
	out["origin"] = Vector2(cx, top); out["scale"] = s
	return out

# ---------------------------------------------------------------- invertebrates
## A nematode in the plane along `pts` (head first): tapered body, pharynx with its bulb, nerve ring, ventral cord with ganglia, gut, gonad. Returns landmarks.
static func worm(ci: CanvasItem, pts: PackedVector2Array, wmax: float, st: Dictionary) -> Dictionary:
	var al := float(st.get("alpha", 1.0)); var t := float(st.get("t", 0.0)); var col := Gfx.BODY; var n := pts.size()
	var prof := func(u: float) -> float: return wmax * (0.30 + 0.70 * pow(sin(PI * clampf(0.08 + 0.80 * u, 0.0, 1.0)), 0.55)) * (1.0 - 0.75 * pow(u, 5.0))
	Gfx.tube_rim(ci, pts, prof, Color("#E6D2B5", 0.95 * al), Color(col, 0.6 * al), 1.8)
	var gut_l := PackedVector2Array(); for i in range(n / 6, int(float(n) * 0.92)): gut_l.append(pts[i])
	if gut_l.size() > 1: Gfx.tube(ci, gut_l, func(u): return wmax * 0.26 * (1.0 - 0.6 * u), Color("#C2A27A", 0.85 * al))
	var ph := PackedVector2Array(); for i in range(1, n / 6 + 1): ph.append(pts[i])
	if ph.size() > 1: Gfx.tube(ci, ph, func(u): return wmax * (0.20 + 0.04 * u), Color(col, 0.30 * al))
	ci.draw_circle(pts[n / 6], wmax * 0.34, Color(col, 0.38 * al)); ci.draw_arc(pts[n / 6], wmax * 0.34, 0, TAU, 14, Color(col, 0.8 * al), 1.2, true)
	var cord := PackedVector2Array(); for i in range(2, int(float(n) * 0.93)): cord.append(pts[i] + Vector2(0, wmax * 0.30))
	ci.draw_polyline(cord, Color(col, 0.55 * al), 1.6, true)
	for i in range(3, cord.size(), maxi(cord.size() / 11, 3)): ci.draw_circle(cord[i], wmax * 0.13, Color(col.lightened(0.2), 0.85 * al))
	var ring := pts[n / 6 + 2]; ci.draw_arc(ring, wmax * 0.62, 0, TAU, 20, Color(Kit.exc(), 0.9 * al), 2.0, true)
	for k in 3:
		var p := along(cord, fposmod(t * 0.2 + float(k) * 0.33, 1.0)); Gfx.glow(ci, p, wmax * 0.8, Color(Kit.exc(), 0.35 * al))
	var gon := PackedVector2Array(); for i in range(int(float(n) * 0.40), int(float(n) * 0.78)): gon.append(pts[i] - Vector2(0, wmax * 0.12))
	if gon.size() > 1: ci.draw_polyline(gon, Color(col, 0.22 * al), wmax * 0.22, true)
	return {"head": pts[1], "ring": ring, "mid": pts[n / 2], "tail": pts[n - 2], "cord": cord}

## A lamprey: eel-like body with a sucker mouth, gill pores, eye, notochord, a travelling wave in the spinal cord and a tail fin.
static func lamprey(ci: CanvasItem, pts: PackedVector2Array, wmax: float, st: Dictionary) -> Dictionary:
	var al := float(st.get("alpha", 1.0)); var t := float(st.get("t", 0.0)); var col := Gfx.FISH; var n := pts.size()
	var prof := func(u: float) -> float: return wmax * (0.42 + 0.58 * pow(sin(PI * clampf(0.04 + 0.70 * u, 0.0, 1.0)), 0.7)) * (1.0 - 0.55 * pow(u, 3.0))
	# the fin along the back and round the tail
	var fin := PackedVector2Array(); for i in range(n / 4, n): fin.append(pts[i] + Vector2(0, -float(prof.call(float(i) / float(n - 1))) - wmax * 0.35 * float(i) / float(n)))
	if fin.size() > 1: ci.draw_polyline(fin, Color(col, 0.30 * al), wmax * 0.2, true)
	Gfx.tube_rim(ci, pts, prof, Color("#E4E2DA", 0.95 * al), Color(col, 0.6 * al), 1.8)
	var cord := PackedVector2Array(); for i in range(2, n - 2): cord.append(pts[i] + Vector2(0, -wmax * 0.22 * (1.0 - float(i) / float(n))))
	var noto := PackedVector2Array(); for i in range(2, n - 2): noto.append(pts[i] + Vector2(0, wmax * 0.10 * (1.0 - float(i) / float(n))))
	ci.draw_polyline(noto, Color(BONE, 0.30 * al), wmax * 0.16, true)
	nerve(ci, cord, Kit.exc(), maxf(wmax * 0.10, 1.6), 0.6, t, al, 4)
	for i in 7:
		var gp := pts[mini(n - 1, n / 9 + i * (n / 40 + 1))] + Vector2(0, wmax * 0.12)
		ci.draw_circle(gp, wmax * 0.09, Color("#231C17", 0.9 * al)); ci.draw_arc(gp, wmax * 0.09, 0, TAU, 8, Color(col, 0.7 * al), 1.0, true)
	var eye_p := pts[maxi(n / 12, 1)] + Vector2(0, -wmax * 0.22); ci.draw_circle(eye_p, wmax * 0.16, Color("#FBF7EE", al)); ci.draw_circle(eye_p, wmax * 0.085, Color("#231C17", al))
	ci.draw_arc(pts[0] + Vector2(wmax * 0.12, 0), wmax * 0.30, PI * 0.5, PI * 1.5, 10, Color(col, 0.85 * al), 2.0, true)   # the oral disc
	return {"head": pts[1], "mid": pts[n / 2], "tail": pts[n - 2], "cord": cord}
