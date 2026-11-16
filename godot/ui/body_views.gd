class_name BodyViews
extends RefCounted
## Generic renderers for the v0.2 view() kinds: limb, heart, lungs, eye, gut, dashboard. Each draws a body from Task::body().readouts by convention (the
## names are listed in game/mock_task.gd) and degrades to a plain readout list when a name is missing, so a new task shows something sensible before anyone
## writes a custom view. The dashboard (gauges + strip charts for every judged trace) is the one several autonomic tasks will use. Everything draws onto a
## TaskView (a Control) `c`, inside rect `r`; history comes from Telemetry.hist.

const KINDS := ["limb", "heart", "lungs", "eye", "gut", "dashboard"]

static func ro(t: Telemetry) -> Dictionary: return (t.body.get("readouts", {}) as Dictionary) if t != null else {}
static func rv(d: Dictionary, k: String, dflt := 0.0) -> float: return float(d.get(k, dflt))
static func hist(t: Telemetry, k: String) -> PackedFloat32Array: return t.hist.get(k, PackedFloat32Array()) if t != null else PackedFloat32Array()

static func draw(kind: String, c: Control, t: Telemetry, r: Rect2) -> void:
	match kind:
		"limb": _limb(c, t, r)
		"heart": _heart(c, t, r)
		"lungs": _lungs(c, t, r)
		"eye": _eye(c, t, r)
		"gut": _gut(c, t, r)
		_: _dashboard(c, t, r)

const SKIP := ["t_ms", "beat", "heart_phase", "last_beat_ms"]
## One row of "name value" pairs along the bottom edge of r, for the readouts a view does not draw itself (what does not fit is left out).
static func _strip(c: Control, r: Rect2, items: Array) -> void:
	var fz := Kit.fs(14); var f := Fonts.regular; var x := r.position.x; var y := r.end.y - 4.0
	for it in items:
		if SKIP.has(str(it[0])): continue
		var s := "%s %s" % [str(it[0]).replace("_", " "), str(it[1])]
		var w := f.get_string_size(s, HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x + 22.0
		if x + w > r.end.x: break
		c.draw_string(f, Vector2(x, y), s, HORIZONTAL_ALIGNMENT_LEFT, w, fz, Kit.TEXT_SOFT); x += w

static func first(d: Dictionary, keys: Array, dflt := 0.0) -> float:
	for k in keys:
		if d.has(k): return float(d[k])
	return dflt
static func has_any(d: Dictionary, keys: Array) -> bool:
	for k in keys:
		if d.has(k): return true
	return false

static func _n(v: float, d := 0) -> String:
	if d == 0: return "%d" % int(roundf(v))
	return ("%." + str(d) + "f") % v

static func _empty(c: Control, r: Rect2, msg: String) -> void:
	c.draw_string(Fonts.regular, Vector2(r.position.x, r.get_center().y), msg, HORIZONTAL_ALIGNMENT_CENTER, r.size.x, Kit.fs(16), Kit.TEXT_SOFT)

# ---------------------------------------------------------------- limb
## A single joint with an agonist / antagonist pair. Reads theta_deg (or angle_deg: from the horizontal, 0 = pointing right, y up, so upright is 90),
## target_rad / target_deg (a dashed ghost), ag_act / ant_act (0..1) or ag_force_N / ant_force_N (scaled to the largest seen) for the two muscle bellies, load (0..1).
## With theta_deg the limb is drawn from the task's body points (hand, elbow, shoulder, world coordinates); otherwise from a synthetic L-shaped joint.
static func _limb(c: Control, t: Telemetry, r: Rect2) -> void:
	var d := ro(t); var fz := Kit.fs(14)
	if not has_any(d, ["theta_deg", "theta_rad", "angle_deg"]): _empty(c, r, "Waiting for theta_deg (or angle_deg) in the task's readouts."); return
	var deg := first(d, ["theta_deg", "angle_deg"], rad_to_deg(first(d, ["theta_rad"])))
	var tdeg := first(d, ["target_deg"], rad_to_deg(first(d, ["target_rad"], deg_to_rad(deg))))
	var s := minf(r.size.x * 0.62, r.size.y * 1.05); var W := s * 0.055
	var ang := deg_to_rad(deg); var tgt := deg_to_rad(tdeg)
	var pts: PackedVector2Array = t.body.get("points", PackedVector2Array())
	var shoulder: Vector2; var elbow: Vector2; var tip: Vector2; var L2: float
	if d.has("theta_deg") and pts.size() >= 3:   # the task's own geometry, fitted into the left of the view
		var bnd: Rect2 = t.task.bounds(); var box := Rect2(r.position.x, r.position.y + 6.0, r.size.x * 0.64, r.size.y - 36.0)
		var k := minf(box.size.x / maxf(bnd.size.x, 1e-6), box.size.y / maxf(bnd.size.y, 1e-6)) * 0.92
		var tf := func(p: Vector2) -> Vector2: return box.get_center() + Vector2(p.x - bnd.get_center().x, -(p.y - bnd.get_center().y)) * k
		tip = tf.call(pts[0]); elbow = tf.call(pts[1]); shoulder = tf.call(pts[2]); L2 = elbow.distance_to(tip); W = L2 * 0.1
	else:
		elbow = Vector2(r.position.x + r.size.x * 0.36, r.position.y + r.size.y * 0.8); L2 = s * 0.62
		shoulder = elbow - Vector2(s * 0.52, 0.0); tip = elbow + Vector2(cos(ang), -sin(ang)) * L2
	var dir := (tip - elbow).normalized() if tip.distance_to(elbow) > 0.01 else Vector2.RIGHT
	var ghost := elbow + Vector2(cos(tgt), -sin(tgt)) * L2
	Gfx.dashed(c, PackedVector2Array([elbow, ghost]), Color(Kit.GREEN, 0.9), 3.0, 8.0, 7.0); c.draw_circle(ghost, W * 0.55, Color(Kit.GREEN, 0.9))
	Gfx.glow(c, tip, s * 0.2, Color(Gfx.BODY, 0.10 + 0.3 * clampf(rv(d, "load"), 0, 1)))
	Gfx.tube_rim(c, PackedVector2Array([shoulder, elbow]), func(u): return W, Color("#10282b"), Color(Gfx.BODY, 0.95), 2.0)
	Gfx.tube_rim(c, PackedVector2Array([elbow, elbow.lerp(tip, 0.5), tip]), func(u): return W * (1.0 - 0.25 * u), Color("#10282b").lerp(Gfx.BODY, 0.25), Color(Gfx.BODY, 0.95), 2.0)
	var nrm := Vector2(-dir.y, dir.x)
	for k in 2:   # the muscle bellies lie along the limb on either side and swell with activation
		var act := 0.0
		var ak := ["ag_act", "ag_force_N"] if k == 0 else ["ant_act", "ant_force_N"]
		if d.has(ak[0]): act = clampf(rv(d, ak[0]), 0.0, 1.0)
		elif d.has(ak[1]):
			var hh := hist(t, ak[1]); var mx := 1e-6
			for q in hh: mx = maxf(mx, q)
			act = clampf(rv(d, ak[1]) / mx, 0.0, 1.0)
		var col := Kit.exc() if k == 0 else Kit.inh(); var sd := 1.0 if k == 0 else -1.0
		var a := elbow + dir * L2 * 0.14 + nrm * sd * W * 1.6; var b := elbow + dir * L2 * 0.84 + nrm * sd * W * 1.6
		Gfx.glow(c, (a + b) * 0.5, L2 * 0.45, Color(col, 0.30 * act))
		c.draw_line(a, b, Color(col, 0.22 + 0.5 * act), W * (0.55 + 1.5 * act), true); c.draw_line(a, b, Color(col.lightened(0.3), 0.35 + 0.65 * act), W * (0.2 + 0.7 * act), true)
		c.draw_string(Fonts.regular, b + nrm * sd * W * 2.6 + Vector2(-50.0, 4.0), "agonist" if k == 0 else "antagonist", HORIZONTAL_ALIGNMENT_CENTER, 100.0, fz, Color(col, 0.95))
	c.draw_circle(elbow, W * 1.15, Color(Kit.BENCH, 0.9)); c.draw_circle(elbow, W * 0.7, Gfx.BODY)
	c.draw_arc(elbow, L2 * 0.4, minf(-ang, -tgt), maxf(-ang, -tgt), 20, Color(Kit.RED, 0.95), 3.0, true)   # the error
	if d.has("load"): c.draw_circle(tip, W * (0.7 + 1.3 * clampf(rv(d, "load"), 0, 1)), Color(1, 1, 1, 0.55))
	var err := deg - tdeg
	var rr := Rect2(r.position.x + r.size.x * 0.68, r.position.y + 14.0, r.size.x * 0.31, r.size.y - 40.0)
	c.draw_string(Fonts.bold, Vector2(rr.position.x, rr.position.y + float(Kit.fs(36))), "%s deg" % _n(deg, 1 if absf(deg) < 100.0 else 0), HORIZONTAL_ALIGNMENT_LEFT, rr.size.x, Kit.fs(36), Kit.TEXT)
	c.draw_string(Fonts.regular, Vector2(rr.position.x, rr.position.y + float(Kit.fs(36)) + float(fz) + 6.0), "target %s, off by %s" % [_n(tdeg, 1), _n(err, 1)], HORIZONTAL_ALIGNMENT_LEFT, rr.size.x, fz, Kit.RED if absf(err) > 6.0 else Kit.GREEN)
	var er := Rect2(rr.position.x, rr.position.y + float(Kit.fs(36)) + float(fz) * 2.0 + 20.0, rr.size.x, rr.size.y - float(Kit.fs(36)) - float(fz) * 2.0 - 20.0)
	if er.size.y > 50.0: _spark(c, er, hist(t, "theta_deg" if d.has("theta_deg") else "angle_deg"), t, 15.0, 0.0, 0.0, Kit.exc(), "joint angle, last 15 s")
	var extra := []
	for k in d:
		if not ["angle_deg", "theta_deg", "theta_rad", "target_deg", "target_rad", "error_deg"].has(str(k)): extra.append([k, _n(rv(d, str(k)), 2)])
	_strip(c, Rect2(r.position.x, r.end.y - 30.0, r.size.x, 28.0), extra)

# ---------------------------------------------------------------- heart
static func _ecg(p: float) -> float:
	var g := func(x: float, mu: float, sg: float, a: float) -> float: return a * exp(-pow((x - mu) / sg, 2.0))
	return g.call(p, 0.12, 0.025, 0.12) + g.call(p, 0.27, 0.007, -0.14) + g.call(p, 0.30, 0.009, 1.0) + g.call(p, 0.33, 0.009, -0.28) + g.call(p, 0.56, 0.05, 0.30)

## A beating heart (scale follows the beat phase), the rate and pressure as big numbers, and a sweeping ECG whose beats follow hr_bpm. Reads hr_bpm, beat (0..1), map_mmhg, stroke_ml.
static func _heart(c: Control, t: Telemetry, r: Rect2) -> void:
	var d := ro(t); var fz := Kit.fs(14)
	if not d.has("hr_bpm"): _empty(c, r, "Waiting for hr_bpm in the task's readouts."); return
	var hr := rv(d, "hr_bpm"); var beat := rv(d, "heart_phase", rv(d, "beat", fposmod(t.t_ms * 0.001 * hr / 60.0, 1.0)))   # heart_phase (0..1 since the last beat); mock tasks send the phase as beat
	var small := r.size.y < 130.0   # a phone: the heart on the left, the rate beside it
	var ec := Vector2(r.position.x + (r.size.x * 0.1 if small else r.size.x * 0.24), r.position.y + r.size.y * 0.5); var s := minf(r.size.x * 0.4, r.size.y * 0.95) / 36.0 * (1.0 + 0.075 * exp(-beat * 9.0))
	var pts := PackedVector2Array()
	for i in 72:
		var a := TAU * float(i) / 72.0; pts.append(ec + Vector2(16.0 * pow(sin(a), 3.0), -(13.0 * cos(a) - 5.0 * cos(2.0 * a) - 2.0 * cos(3.0 * a) - cos(4.0 * a))) * s)
	Gfx.glow(c, ec, s * 26.0, Color(Kit.RED, 0.10 + 0.25 * exp(-beat * 7.0)))
	c.draw_colored_polygon(pts, Color("#3a1424")); var closed := pts.duplicate(); closed.append(pts[0])
	c.draw_polyline(closed, Color(Kit.RED, 0.25), 9.0, true); c.draw_polyline(closed, Color(Kit.RED, 0.95), 2.4, true)
	if small:
		c.draw_string(Fonts.bold, ec + Vector2(s * 19.0, 4.0), _n(hr), HORIZONTAL_ALIGNMENT_LEFT, 90.0, Kit.fs(30), Kit.TEXT)
		c.draw_string(Fonts.regular, ec + Vector2(s * 19.0, 4.0 + float(fz) + 2.0), "beats / min", HORIZONTAL_ALIGNMENT_LEFT, 120.0, fz, Kit.TEXT_SOFT)
	else:
		c.draw_string(Fonts.bold, ec + Vector2(-s * 12.0, s * 1.5), _n(hr), HORIZONTAL_ALIGNMENT_CENTER, s * 24.0, Kit.fs(42), Kit.TEXT)
		c.draw_string(Fonts.regular, ec + Vector2(-s * 12.0, s * 1.5 + float(Kit.fs(18)) + 6.0), "beats / min", HORIZONTAL_ALIGNMENT_CENTER, s * 24.0, fz, Kit.TEXT_SOFT)
	# right: pressure and stroke volume, then the ECG
	var x := r.position.x + r.size.x * 0.5; var w := r.end.x - x - 14.0; var y := r.position.y + 16.0
	var tiles := []
	if has_any(d, ["map_mmHg", "map_mmhg"]):
		var mapv := first(d, ["map_mmHg", "map_mmhg"])
		if d.has("sys_mmHg") and d.has("dia_mmHg"): tiles.append(["pressure, mean %s (mmHg)" % _n(mapv), "%s/%s" % [_n(rv(d, "sys_mmHg")), _n(rv(d, "dia_mmHg"))], "", Kit.RED])
		else: tiles.append(["mean pressure (mmHg)", _n(mapv), "", Kit.RED])
	if d.has("stroke_ml"): tiles.append(["stroke volume (ml)", _n(rv(d, "stroke_ml")), "", Kit.inh()])
	for k in tiles.size():
		var tx := x + (w * 0.5 + 4.0) * k; var tw := w * 0.5 - 4.0 if tiles.size() > 1 else w
		c.draw_style_box(Kit.sb(Kit.WELL, Kit.LINE, 1, 8, 0, false), Rect2(tx, y, tw, float(Kit.fs(30)) + float(fz) + 18.0))
		c.draw_string(Fonts.regular, Vector2(tx + 8, y + float(fz) + 3.0), tiles[k][0], HORIZONTAL_ALIGNMENT_LEFT, tw - 12, fz, Kit.TEXT_SOFT)
		c.draw_string(Fonts.bold, Vector2(tx + 8, y + float(fz) + float(Kit.fs(30)) + 7.0), "%s %s" % [tiles[k][1], tiles[k][2]], HORIZONTAL_ALIGNMENT_LEFT, tw - 12, Kit.fs(30), tiles[k][3])
	var ey := y + (float(Kit.fs(30)) + float(fz) + 28.0 if not tiles.is_empty() else 0.0); var er := Rect2(x, ey, w, r.end.y - ey - 44.0)
	if er.size.y > 30.0:
		c.draw_rect(er, Color(Kit.BENCH, 0.6)); c.draw_rect(er, Color(Kit.LINE, 0.9), false, 1.0)
		var hh := hist(t, "hr_bpm"); var window := 4.0; var tn := t.t_ms * 0.001; var ph_now := beat
		var pl := PackedVector2Array(); var n := int(er.size.x / 2.0)
		for i in n:
			var tt := tn - window * (1.0 - float(i) / float(n - 1))   # seconds at this pixel
			var ph := ph_now
			# integrate backward from now using the heart-rate history (50 ms steps) down to tt
			var back := tn - tt; var k := hh.size() - 1; var acc := 0.0; var left := back
			while left > 0.0 and k >= 0: var dt := minf(left, 0.05); acc += float(hh[k]) / 60.0 * dt; left -= dt; k -= 1
			if left > 0.0: acc += hr / 60.0 * left
			ph = fposmod(ph_now - acc, 1.0)
			pl.append(Vector2(er.position.x + er.size.x * float(i) / float(n - 1), er.get_center().y - er.size.y * 0.38 * _ecg(ph) * 1.2 + er.size.y * 0.12))
		c.draw_polyline(pl, Color(Kit.GREEN, 0.25), 6.0, true); c.draw_polyline(pl, Kit.GREEN, 1.8, true)
		c.draw_string(Fonts.regular, er.position + Vector2(8, float(fz) + 2.0), "ECG", HORIZONTAL_ALIGNMENT_LEFT, 60, fz, Kit.TEXT_SOFT)
	var extra := []
	for k in d:
		if not ["hr_bpm", "beat", "heart_phase", "map_mmhg", "map_mmHg", "sys_mmHg", "dia_mmHg", "stroke_ml"].has(str(k)): extra.append([k, _n(rv(d, str(k)), 2)])
	_strip(c, Rect2(r.position.x, r.end.y - 30.0, r.size.x, 28.0), extra)

# ---------------------------------------------------------------- lungs
## Two lungs that inflate with volume (0..1), a diaphragm that drops, a breath-volume trace, and the rate and CO2. Reads volume, rr_per_min, co2_mmhg, ventilation.
static func _lungs(c: Control, t: Telemetry, r: Rect2) -> void:
	var d := ro(t); var fz := Kit.fs(14)
	if not d.has("volume"): _empty(c, r, "Waiting for volume in the task's readouts."); return
	var vol := clampf(rv(d, "volume"), 0.0, 1.0); var cx := r.position.x + r.size.x * 0.27; var cy := r.position.y + r.size.y * 0.4
	var h := r.size.y * 0.58; var w := minf(r.size.x * 0.17, h * 0.55); var k := 0.86 + 0.26 * vol
	for side in [-1.0, 1.0]:
		var lc := Vector2(cx + side * w * 0.58, cy); var pts := PackedVector2Array()
		for i in 44:
			var a := TAU * float(i) / 44.0; pts.append(lc + Vector2(cos(a) * w * 0.5 * (0.84 + 0.16 * (1.0 if sin(a) < 0.0 else 0.72)), sin(a) * h * 0.5) * k)
		Gfx.glow(c, lc, h * 0.7 * k, Color(Kit.CYAN, 0.04 + 0.12 * vol))
		c.draw_colored_polygon(pts, Color("#0d2a3a").lerp(Color("#15506b"), vol)); var cl := pts.duplicate(); cl.append(pts[0])
		c.draw_polyline(cl, Color(Kit.CYAN, 0.2), 7.0, true); c.draw_polyline(cl, Color(Kit.CYAN, 0.9), 2.0, true)
		for b in 4: c.draw_line(Vector2(cx + side * w * 0.12, cy - h * 0.3), lc + Vector2(side * w * (0.1 + 0.08 * b), -h * 0.3 + float(b) * h * 0.2) * k, Color(Kit.CYAN, 0.25), 1.6, true)
	c.draw_line(Vector2(cx, cy - h * 0.62), Vector2(cx, cy - h * 0.3), Color(Kit.CYAN, 0.8), 5.0, true)
	c.draw_line(Vector2(cx, cy - h * 0.3), Vector2(cx - w * 0.35, cy - h * 0.12), Color(Kit.CYAN, 0.6), 3.5, true); c.draw_line(Vector2(cx, cy - h * 0.3), Vector2(cx + w * 0.35, cy - h * 0.12), Color(Kit.CYAN, 0.6), 3.5, true)
	var dy := minf(cy + h * 0.5 * k + 8.0 + vol * h * 0.08, r.end.y - 36.0)   # the diaphragm: a dome under the lungs that flattens as they fill
	c.draw_arc(Vector2(cx, dy + w * 0.9 * (1.0 - 0.25 * vol)), w * 1.25, PI * 1.12, PI * 1.88, 24, Color(Gfx.VIOLET, 0.9), 4.0, true)
	# right: the numbers and a volume trace
	var x := r.position.x + r.size.x * 0.54; var wd := r.end.x - x - 14.0; var y := r.position.y + 14.0
	var tiles := [["breath rate (/min)", _n(rv(d, "rr_per_min"), 1), "", Kit.TEXT], ["blood CO2 (mmHg)", _n(rv(d, "co2_mmhg"), 1), "", Kit.exc()]]
	for q in tiles.size():
		if q == 1 and not d.has("co2_mmhg"): continue
		if q == 0 and not d.has("rr_per_min"): continue
		var tx := x + (wd * 0.5 + 4.0) * q; var tw := wd * 0.5 - 4.0
		c.draw_style_box(Kit.sb(Kit.WELL, Kit.LINE, 1, 8, 0, false), Rect2(tx, y, tw, float(Kit.fs(30)) + float(fz) + 18.0))
		c.draw_string(Fonts.regular, Vector2(tx + 8, y + float(fz) + 3.0), tiles[q][0], HORIZONTAL_ALIGNMENT_LEFT, tw - 12, fz, Kit.TEXT_SOFT)
		c.draw_string(Fonts.bold, Vector2(tx + 8, y + float(fz) + float(Kit.fs(30)) + 7.0), "%s %s" % [tiles[q][1], tiles[q][2]], HORIZONTAL_ALIGNMENT_LEFT, tw - 12, Kit.fs(30), tiles[q][3])
	var ey := y + float(Kit.fs(30)) + float(fz) + 28.0; var er := Rect2(x, ey, wd, r.end.y - ey - 44.0)
	if er.size.y > 30.0: _spark(c, er, hist(t, "volume"), t, 12.0, 0.0, 1.0, Kit.CYAN, "breath volume")
	var extra := []
	for kk in d:
		if not ["volume", "rr_per_min", "co2_mmhg"].has(str(kk)): extra.append([kk, _n(rv(d, str(kk)), 2)])
	_strip(c, Rect2(r.position.x, r.end.y - 30.0, r.size.x, 28.0), extra)

## A boxed strip chart of the last `window` seconds of `arr` (50 ms samples), y from lo to hi.
static func _spark(c: Control, r: Rect2, arr: PackedFloat32Array, t: Telemetry, window: float, lo: float, hi: float, col: Color, name: String) -> void:
	var fz := Kit.fs(14)
	if lo == hi and arr.size() > 1:   # auto range over the window
		var n0 := mini(arr.size(), int(window / 0.05)); lo = arr[arr.size() - 1]; hi = lo
		for i in range(arr.size() - n0, arr.size()): lo = minf(lo, arr[i]); hi = maxf(hi, arr[i])
		var pad := maxf((hi - lo) * 0.1, 1e-6); lo -= pad; hi += pad
	c.draw_rect(r, Color(Kit.BENCH, 0.6)); c.draw_rect(r, Color(Kit.LINE, 0.9), false, 1.0)
	if arr.size() < 2:
		c.draw_string(Fonts.regular, r.position + Vector2(8, float(fz) + 2.0), name, HORIZONTAL_ALIGNMENT_LEFT, r.size.x - 12, fz, Kit.TEXT_SOFT); return
	var n := mini(arr.size(), int(window / 0.05)); var pl := PackedVector2Array(); var step := maxi(1, n / maxi(int(r.size.x / 2.0), 1))
	for i in range(arr.size() - n, arr.size(), step):
		var u := float(i - (arr.size() - n)) / float(maxi(n - 1, 1))
		pl.append(Vector2(r.position.x + r.size.x * u, r.end.y - 4.0 - (r.size.y - 8.0) * clampf((arr[i] - lo) / maxf(hi - lo, 1e-6), 0.0, 1.0)))
	if pl.size() > 1: c.draw_polyline(pl, Color(col, 0.25), 6.0, true); c.draw_polyline(pl, col, 1.8, true)
	var lw := Fonts.regular.get_string_size(name, HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x + 12.0   # the caption sits on a plate so the trace never runs through it
	c.draw_rect(Rect2(r.position + Vector2(2, 2), Vector2(lw, float(fz) + 6.0)), Color(Kit.BENCH, 0.85))
	c.draw_string(Fonts.regular, r.position + Vector2(8, float(fz) + 2.0), name, HORIZONTAL_ALIGNMENT_LEFT, r.size.x - 12, fz, Kit.TEXT_SOFT)

# ---------------------------------------------------------------- eye
## An eye: the pupil radius follows pupil_mm, light (0..1) shows as rays and a glow at the left. Reads pupil_mm, light.
static func _eye(c: Control, t: Telemetry, r: Rect2) -> void:
	var d := ro(t); var fz := Kit.fs(14)
	if not d.has("pupil_mm"): _empty(c, r, "Waiting for pupil_mm in the task's readouts."); return
	var ec := Vector2(r.position.x + r.size.x * 0.5, r.position.y + r.size.y * 0.47); var ew := minf(r.size.x * 0.34, r.size.y * 0.95); var eh := ew * 0.55
	var light := clampf(rv(d, "light", 0.5), 0.0, 1.0)
	for i in 5:   # light rays from the left
		var y := ec.y + (float(i) - 2.0) * eh * 0.32
		c.draw_line(Vector2(r.position.x + 10.0, y), Vector2(ec.x - ew * 1.02, y + (ec.y - y) * 0.15), Color(1, 0.95, 0.7, 0.05 + 0.65 * light), 1.0 + 2.5 * light, true)
	Gfx.glow(c, Vector2(r.position.x + 24.0, ec.y), eh * 1.6, Color(1, 0.95, 0.7, 0.45 * light))
	var lid := PackedVector2Array()
	for i in 33:
		var u := float(i) / 32.0; lid.append(ec + Vector2((u - 0.5) * 2.0 * ew, -sin(u * PI) * eh))
	for i in 33:
		var u := 1.0 - float(i) / 32.0; lid.append(ec + Vector2((u - 0.5) * 2.0 * ew, sin(u * PI) * eh))
	c.draw_colored_polygon(lid, Color("#d9e4f2", 0.92)); var lc := lid.duplicate(); lc.append(lid[0])
	c.draw_polyline(lc, Color(Gfx.FISH, 0.9), 3.0, true)
	var ir := eh * 0.86; var pr := ir * clampf(rv(d, "pupil_mm") / 9.0, 0.1, 0.95)
	c.draw_circle(ec, ir, Color("#2d6fa8")); c.draw_circle(ec, ir * 0.82, Color("#3f8bc9"))
	for i in 24: var a := TAU * float(i) / 24.0; c.draw_line(ec + Vector2(cos(a), sin(a)) * pr, ec + Vector2(cos(a), sin(a)) * ir * 0.8, Color(1, 1, 1, 0.14), 1.0, true)
	c.draw_circle(ec, pr, Color("#04070c")); c.draw_circle(ec + Vector2(-pr * 0.35, -pr * 0.4), pr * 0.16, Color(1, 1, 1, 0.8))
	c.draw_arc(ec, ir, 0, TAU, 40, Color(Kit.BENCH, 0.8), 2.0, true)
	c.draw_string(Fonts.bold, Vector2(r.end.x - 190.0, r.position.y + 14.0 + float(Kit.fs(32))), "%s mm" % _n(rv(d, "pupil_mm"), 1), HORIZONTAL_ALIGNMENT_RIGHT, 176.0, Kit.fs(32), Kit.TEXT)
	c.draw_string(Fonts.regular, Vector2(r.end.x - 190.0, r.position.y + 14.0 + float(Kit.fs(32)) + float(fz) + 6.0), "pupil diameter", HORIZONTAL_ALIGNMENT_RIGHT, 176.0, fz, Kit.TEXT_SOFT)
	var extra := []
	for k in d:
		if k != "pupil_mm": extra.append([k, _n(rv(d, str(k)), 2)])
	_strip(c, Rect2(r.position.x, r.end.y - 30.0, r.size.x, 28.0), extra)

# ---------------------------------------------------------------- gut
## A gut segment chain: eight smooth-muscle segments that squeeze with seg0..seg7 (0..1) and a bolus at bolus (0..1 along the tube).
static func _gut(c: Control, t: Telemetry, r: Rect2) -> void:
	var d := ro(t); var fz := Kit.fs(14)
	if not d.has("bolus"): _empty(c, r, "Waiting for bolus in the task's readouts."); return
	var n := 8; var x0 := r.position.x + 28.0; var x1 := r.end.x - 28.0; var cy := r.position.y + r.size.y * 0.46; var seg_w := (x1 - x0) / float(n); var H := minf(r.size.y * 0.3, seg_w * 0.9)
	for i in n:
		var act := clampf(rv(d, "seg%d" % i), 0.0, 1.0); var hh := H * (1.0 - 0.55 * act); var x := x0 + seg_w * float(i)
		var rect := Rect2(x + 2.0, cy - hh, seg_w - 4.0, hh * 2.0)
		Gfx.glow(c, rect.get_center(), seg_w * 0.9, Color(Kit.exc(), 0.35 * act))
		Gfx.rounded_rect(c, rect, Color("#2a1a12").lerp(Color("#7a4a1c"), act), Color(Kit.exc(), 0.35 + 0.6 * act), seg_w * 0.3, 2.0)
		c.draw_string(Fonts.regular, Vector2(x, cy + H + float(fz) + 8.0), "%d" % (i + 1), HORIZONTAL_ALIGNMENT_CENTER, seg_w, fz, Kit.TEXT_SOFT)
	var bx := x0 + (x1 - x0) * clampf(rv(d, "bolus"), 0.0, 1.0)
	Gfx.glow(c, Vector2(bx, cy), H * 0.9, Color(1, 1, 1, 0.25)); c.draw_circle(Vector2(bx, cy), H * 0.32, Color("#e9f3ff", 0.92))
	c.draw_string(Fonts.regular, Vector2(x0, r.position.y + float(fz) + 6.0), "mouth", HORIZONTAL_ALIGNMENT_LEFT, 80, fz, Kit.TEXT_SOFT)
	c.draw_string(Fonts.regular, Vector2(x1 - 140.0, r.position.y + float(fz) + 6.0), "end of the tube", HORIZONTAL_ALIGNMENT_RIGHT, 140.0, fz, Kit.TEXT_SOFT)
	var extra := [["bolus at", "%d%%" % int(roundf(100.0 * rv(d, "bolus")))]]
	for k in d:
		if not str(k).begins_with("seg") and k != "bolus": extra.append([k, _n(rv(d, str(k)), 2)])
	_strip(c, Rect2(r.position.x, r.end.y - 30.0, r.size.x, 28.0), extra)

# ---------------------------------------------------------------- dashboard
## Gauges for every judged trace (the arc is the tolerance band, the needle the value, green inside it and red outside, with a sparkline of the last 20 s), then
## small tiles for every other readout. A banner names the scripted event that is on now. Compact tiles when the view is short (a phone).
static func _dashboard(c: Control, t: Telemetry, r: Rect2) -> void:
	var d := ro(t); var fz := Kit.fs(14); var info := Campaign.info(str(t.task.id())); var bands: Array = info["bands"]
	if d.is_empty(): _empty(c, r, "No readouts from this task yet."); return
	var judged := {}
	for b in bands: judged[str(b["name"])] = true
	var others := []
	for k in d:
		if not judged.has(str(k)): others.append(str(k))
	others.sort()
	# the event that is on now
	var banner := ""
	for e in info["timeline"]:
		if t.t_ms >= float(e["t_ms"]) and t.t_ms <= float(e["t_ms"]) + float(e["dur_ms"]): banner = str(e["label"])
	var y := r.position.y
	if banner != "":
		var bw := Fonts.bold.get_string_size(banner.to_upper(), HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x + 24.0
		Gfx.rounded_rect(c, Rect2(r.position.x, y, bw, float(fz) + 10.0), Color(Kit.exc(), 0.9), Kit.exc(), 7.0, 1.0)
		c.draw_string(Fonts.bold, Vector2(r.position.x + 12.0, y + float(fz) + 3.0), banner.to_upper(), HORIZONTAL_ALIGNMENT_LEFT, bw, fz, Kit.BENCH)
		y += float(fz) + 16.0
	var area := Rect2(r.position.x, y, r.size.x, r.end.y - y)
	var nb := bands.size()
	if nb == 0: _tiles(c, t, area, others, d); return
	var strip_h := 0.0 if others.is_empty() else float(fz) + 10.0   # the readouts that are not judged: one line of "name value" along the bottom
	var g := Rect2(area.position, Vector2(area.size.x, area.size.y - strip_h))
	var cols := mini(nb, maxi(1, int(g.size.x / 175.0))); var rows := int(ceil(float(nb) / float(cols)))
	var cw := g.size.x / float(cols); var chh := g.size.y / float(rows); var compact := chh < 112.0
	for i in nb:
		var b: Dictionary = bands[i]; var cr := Rect2(g.position + Vector2(cw * float(i % cols), chh * float(i / cols)), Vector2(cw, chh)).grow(-4.0)
		_gauge(c, t, cr, b, rv(d, str(b["name"])), compact)
	if strip_h > 0.0:
		var items := []
		for k in others: items.append([k, _n(rv(d, k), 0 if absf(rv(d, k)) >= 100.0 else 2)])
		_strip(c, Rect2(area.position.x, area.end.y - strip_h, area.size.x, strip_h), items)

static func _gauge(c: Control, t: Telemetry, r: Rect2, b: Dictionary, v: float, compact: bool) -> void:
	var fz := Kit.fs(14); var lo := float(b["lo"]); var hi := float(b["hi"]); var span := maxf(hi - lo, 1e-6)
	var ok := v >= lo and v <= hi; var col := Kit.GREEN if ok else Kit.RED
	var arr := hist(t, str(b["name"])); var glo := lo - span * 0.8; var ghi := hi + span * 0.8
	for q in arr: glo = minf(glo, q); ghi = maxf(ghi, q)
	glo = minf(glo, v); ghi = maxf(ghi, v)
	c.draw_style_box(Kit.sb(Kit.WELL, Color(col, 0.55), 1, 10, 0, false), r)
	c.draw_string(Fonts.regular, r.position + Vector2(10, float(fz) + 4.0), "%s%s" % [str(b["name"]).replace("_", " "), (" (" + str(b["unit"]) + ")") if str(b["unit"]) != "" else ""], HORIZONTAL_ALIGNMENT_LEFT, r.size.x - 16.0, fz, Kit.TEXT_SOFT)
	if compact:   # a bar with the band marked and a marker at the value
		var bx := Rect2(r.position.x + 10.0, r.end.y - 18.0, r.size.x - 20.0, 8.0)
		c.draw_rect(bx, Color(Kit.LINE, 0.7)); c.draw_rect(Rect2(bx.position.x + bx.size.x * (lo - glo) / (ghi - glo), bx.position.y, bx.size.x * span / (ghi - glo), bx.size.y), Color(Kit.GREEN, 0.7))
		var mx := bx.position.x + bx.size.x * clampf((v - glo) / (ghi - glo), 0, 1); c.draw_rect(Rect2(mx - 2.0, bx.position.y - 4.0, 4.0, bx.size.y + 8.0), col)
		c.draw_string(Fonts.bold, Vector2(r.position.x + 10.0, r.position.y + float(fz) + float(Kit.fs(24)) + 6.0), _n(v, 0 if absf(v) >= 100.0 else 1), HORIZONTAL_ALIGNMENT_LEFT, r.size.x - 16.0, Kit.fs(24), col)
		return
	# an arc gauge: 240 degrees, the tolerance band on the rim, the needle on top, the value in the gap below the hub
	var spark_h := minf(r.size.y * 0.24, 40.0)
	var gh := r.size.y - float(fz) - 12.0 - spark_h
	var rad := minf(r.size.x * 0.34, gh / 1.55); var cc := Vector2(r.get_center().x, r.position.y + float(fz) + 10.0 + rad)
	var a0 := PI * 0.5 + deg_to_rad(60.0); var sweep := deg_to_rad(240.0)
	var ang := func(x: float) -> float: return a0 + sweep * clampf((x - glo) / (ghi - glo), 0.0, 1.0)
	c.draw_arc(cc, rad, a0, a0 + sweep, 40, Color(Kit.LINE, 0.9), 8.0, true)
	c.draw_arc(cc, rad, ang.call(lo), ang.call(hi), 24, Color(Kit.GREEN, 0.9), 8.0, true)
	var na: float = ang.call(v); var tip := cc + Vector2(cos(na), sin(na)) * (rad - 4.0)
	Gfx.glow(c, tip, rad * 0.4, Color(col, 0.3))
	c.draw_line(cc, tip, col, 3.0, true); c.draw_circle(cc, 6.0, col); c.draw_circle(cc, 3.0, Kit.BENCH)
	c.draw_string(Fonts.bold, Vector2(r.position.x, cc.y + rad * 0.78), _n(v, 0 if absf(v) >= 100.0 else 1), HORIZONTAL_ALIGNMENT_CENTER, r.size.x, Kit.fs(24), col)
	var sr := Rect2(r.position.x + 10.0, r.end.y - spark_h - 6.0, r.size.x - 20.0, spark_h)
	if arr.size() > 2:
		var n := mini(arr.size(), 400); var pl := PackedVector2Array(); var step := maxi(1, n / maxi(int(sr.size.x / 2.0), 1))
		for i in range(arr.size() - n, arr.size(), step): pl.append(Vector2(sr.position.x + sr.size.x * float(i - (arr.size() - n)) / float(maxi(n - 1, 1)), sr.end.y - sr.size.y * clampf((arr[i] - glo) / (ghi - glo), 0, 1)))
		var bt := sr.end.y - sr.size.y * (hi - glo) / (ghi - glo); var bb := sr.end.y - sr.size.y * (lo - glo) / (ghi - glo)
		c.draw_rect(Rect2(sr.position.x, bt, sr.size.x, bb - bt), Color(Kit.GREEN, 0.14))
		c.draw_polyline(pl, Color(col, 0.9), 1.6, true)

static func _tiles(c: Control, t: Telemetry, r: Rect2, keys: Array, d: Dictionary) -> void:
	if keys.is_empty(): return
	var fz := Kit.fs(14); var tw := float(Kit.fs(14)) * 9.0; var cols := maxi(1, int(r.size.x / tw)); var rows := int(ceil(float(keys.size()) / float(cols)))
	var cw := r.size.x / float(cols); var chh := clampf(r.size.y / float(rows), 46.0, 70.0)
	for i in keys.size():
		var cr := Rect2(r.position + Vector2(cw * float(i % cols), chh * float(i / cols)), Vector2(cw, chh)).grow(-3.0)
		if cr.end.y > r.end.y + 1.0: break
		c.draw_style_box(Kit.sb(Kit.WELL, Kit.LINE, 1, 8, 0, false), cr)
		c.draw_string(Fonts.regular, cr.position + Vector2(8, float(fz) + 2.0), str(keys[i]).replace("_", " "), HORIZONTAL_ALIGNMENT_LEFT, cr.size.x - 12.0, fz, Kit.TEXT_SOFT)
		var arr := hist(t, str(keys[i]))
		c.draw_string(Fonts.bold, Vector2(cr.position.x + 8.0, cr.end.y - 7.0), _n(rv(d, str(keys[i])), 0 if absf(rv(d, str(keys[i]))) >= 100.0 else 2), HORIZONTAL_ALIGNMENT_LEFT, cr.size.x * 0.5, Kit.fs(20), Kit.TEXT)
		if arr.size() > 2 and cr.size.x > 150.0:
			var lo := arr[0]; var hi := arr[0]
			for q in arr: lo = minf(lo, q); hi = maxf(hi, q)
			var sr := Rect2(cr.position.x + cr.size.x * 0.52, cr.position.y + float(fz) + 6.0, cr.size.x * 0.44, cr.size.y - float(fz) - 12.0); var pl := PackedVector2Array(); var n := mini(arr.size(), 400); var step := maxi(1, n / maxi(int(sr.size.x / 2.0), 1))
			for k in range(arr.size() - n, arr.size(), step): pl.append(Vector2(sr.position.x + sr.size.x * float(k - (arr.size() - n)) / float(maxi(n - 1, 1)), sr.end.y - sr.size.y * (arr[k] - lo) / maxf(hi - lo, 1e-6)))
			if pl.size() > 1: c.draw_polyline(pl, Color(Gfx.VIOLET, 0.95), 1.5, true)
