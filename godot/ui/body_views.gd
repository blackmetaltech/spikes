class_name BodyViews
extends RefCounted
## Renderers for the v0.2 view() kinds: limb, heart, lungs, eye, gut, dashboard. Each draws a body from Task::body().readouts by convention (the names are
## listed in game/mock_task.gd) and degrades to a plain readout list when a name is missing, so a new task shows something sensible before anyone writes a
## custom view. The anatomy itself (heart chambers, bronchial tree, arm, iris, bowel) lives in anatomy.gd; this file reads the readouts, lays the view out and
## draws the numbers: strip charts, the ECG (from the real beat times), the gauges of the dashboard. Everything draws onto a TaskView (a Control) `c`, inside
## rect `r`; history comes from Telemetry.hist.

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

# ---------------------------------------------------------------- readout names and units (G6)
## Explicit names for the readouts and score parts the engine reports: key (lower case) -> [label, unit].
const NAMES := {
	"hr_bpm": ["heart rate", "bpm"], "hr_target": ["target heart rate", "bpm"], "hr_error_bpm": ["heart rate error", "bpm"], "hr_mean_bpm": ["mean heart rate", "bpm"],
	"map_mmhg": ["mean pressure", "mmHg"], "sys_mmhg": ["systolic pressure", "mmHg"], "dia_mmhg": ["diastolic pressure", "mmHg"], "pressure_mmhg": ["pressure", "mmHg"],
	"stroke_ml": ["stroke volume", "mL"], "co_l_min": ["cardiac output", "L/min"], "cvp_mmhg": ["venous pressure", "mmHg"], "baro_hz": ["baroreceptor rate", "Hz"],
	"blood_lost_ml": ["blood lost", "mL"], "pooled_ml": ["pooled blood", "mL"], "contractility": ["contractility", ""], "tpr": ["vascular resistance", ""],
	"workload": ["workload", ""], "ne": ["noradrenaline", ""], "ach": ["acetylcholine", ""], "da": ["dopamine", ""], "beats": ["beats", ""], "cells": ["SA cells", ""],
	"ejection": ["ejection", ""], "standing": ["standing", ""], "haemorrhage_ml_s": ["bleeding", "mL/s"], "threat": ["threat", ""],
	"volume": ["lung volume", ""], "rr_per_min": ["breath rate", "/min"], "co2_mmhg": ["arterial CO2", "mmHg"], "paco2_mmhg": ["arterial CO2", "mmHg"], "tissue_pco2_mmhg": ["tissue CO2", "mmHg"],
	"ventilation": ["ventilation", "L/min"], "ve_l_min": ["ventilation", "L/min"], "va_l_min": ["alveolar ventilation", "L/min"], "tidal_l": ["tidal volume", "L"], "volume_l": ["lung volume", "L"],
	"flow_l_s": ["airflow", "L/s"], "phrenic_drive": ["phrenic drive", ""], "chemo_hz": ["chemoreceptor rate", "Hz"], "periph_chemo_hz": ["peripheral chemoreceptor", "Hz"], "central_chemo_hz": ["central chemoreceptor", "Hz"],
	"resp_rate_bpm": ["breath rate", "/min"], "inspiring": ["inspiring", ""],
	"pupil_mm": ["pupil diameter", "mm"], "light": ["light level", ""], "luminance_cdm2": ["luminance", "cd/m2"], "retinal_hz": ["retinal rate", "Hz"], "retinal_td": ["retinal illuminance", "td"],
	"sphincter": ["sphincter activity", ""], "dilator": ["dilator activity", ""], "sphincter_hz": ["sphincter drive", "Hz"], "dilator_hz": ["dilator drive", "Hz"],
	"bolus": ["bolus position", ""], "bolus_pos": ["bolus position", "segments"], "bolus_vel": ["bolus speed", "segments/s"], "gut_motility": ["gut motility", "%"],
	"sympathetic_hz": ["sympathetic rate", "Hz"], "parasympathetic_hz": ["parasympathetic rate", "Hz"],
	"theta_deg": ["joint angle", "deg"], "angle_deg": ["joint angle", "deg"], "theta_rad": ["joint angle", "rad"], "target_deg": ["target angle", "deg"], "target_rad": ["target angle", "rad"],
	"error_deg": ["angle error", "deg"], "err_rad": ["angle error", "rad"], "ext_torque_nm": ["external torque", "N m"], "gamma": ["gamma drive", ""], "load": ["load", ""],
	"ag_force_n": ["agonist force", "N"], "ant_force_n": ["antagonist force", "N"], "ag_ia_hz": ["agonist Ia rate", "Hz"], "ant_ia_hz": ["antagonist Ia rate", "Hz"],
	"ag_ii_hz": ["agonist II rate", "Hz"], "ant_ii_hz": ["antagonist II rate", "Hz"], "ag_ib_hz": ["agonist Ib rate", "Hz"], "ant_ib_hz": ["antagonist Ib rate", "Hz"],
	"ag_emg_hz": ["agonist EMG", "Hz"], "ant_emg_hz": ["antagonist EMG", "Hz"], "ag_act": ["agonist activation", ""], "ant_act": ["antagonist activation", ""],
	"in_band_pct": ["time in band", "%"], "in_band": ["time in band", ""], "cv": ["beat variability", "%"], "synchrony": ["synchrony", ""], "rmssd_ms": ["RMSSD", "ms"], "sdnn_ms": ["SDNN", "ms"],
	"spikes": ["spikes", ""], "neurons": ["neurons", ""], "energy": ["energy", ""], "atp": ["ATP", ""], "ve_ratio": ["ventilation ratio", ""], "rate_cv": ["rate variability", "%"],
	"touch_rate_hz": ["touch rate", "Hz"], "asel_hz": ["ASEL rate", "Hz"], "aser_hz": ["ASER rate", "Hz"], "conc": ["salt concentration", ""], "dlnc_dt": ["concentration slope", ""],
	"tail_amp": ["tail amplitude", ""], "speed": ["speed", ""], "omega": ["turning rate", ""], "progress": ["progress", ""]}
const WORDS := {"hr": "heart rate", "rr": "breath rate", "ve": "ventilation", "co2": "CO2", "paco2": "arterial CO2", "pco2": "CO2", "ne": "NE", "ach": "ACh", "da": "DA", "map": "mean pressure",
	"sa": "SA", "cv": "variability", "emg": "EMG", "ag": "agonist", "ant": "antagonist", "ia": "Ia", "ii": "II", "ib": "Ib", "haem": "haemorrhage", "err": "error", "dist": "distance",
	"rmssd": "RMSSD", "sdnn": "SDNN", "atp": "ATP", "sys": "systolic", "dia": "diastolic", "tpr": "resistance", "l": "L", "td": "td", "t": "time", "asel": "ASEL", "aser": "ASER"}
const UNIT_TOK := {"ms": "ms", "s": "s", "mm": "mm", "rad": "rad", "hz": "Hz", "bpm": "bpm", "mmhg": "mmHg", "pct": "%", "deg": "deg", "ml": "mL", "n": "N", "nm": "N m", "cdm2": "cd/m2", "l": "L"}
const COUNTS := ["beats", "cells", "spikes", "bursts", "hits", "misses", "neurons", "reaches_done", "pushes_done", "stretches_done", "slow_waves", "rest_breaths"]

## [label, unit] for a readout or score-part key. Raw keys never reach the player: known names come from NAMES, the rest are split into words, abbreviations
## expanded, and a trailing unit token (_ms, _mm, _hz, _l_min, ...) moved into the unit.
static func pretty(key: String) -> Array:
	var lk := key.to_lower()
	if NAMES.has(lk): return NAMES[lk]
	if key.begins_with("seg") and key.substr(3).is_valid_int(): return ["segment %d" % (int(key.substr(3)) + 1), ""]
	if key.begins_with("ex") and key.substr(2).is_valid_int(): return ["ACh drive, segment %d" % (int(key.substr(2)) + 1), ""]
	if key.begins_with("inh") and key.substr(3).is_valid_int(): return ["NO drive, segment %d" % (int(key.substr(3)) + 1), ""]
	if key.begins_with("sw") and key.substr(2).is_valid_int(): return ["slow wave, segment %d" % (int(key.substr(2)) + 1), ""]
	var tk := lk.split("_"); var unit := ""
	if tk.size() >= 3 and tk[tk.size() - 2] == "l" and tk[tk.size() - 1] == "min": unit = "L/min"; tk = tk.slice(0, tk.size() - 2)
	elif tk.size() >= 2 and UNIT_TOK.has(tk[tk.size() - 1]): unit = str(UNIT_TOK[tk[tk.size() - 1]]); tk = tk.slice(0, tk.size() - 1)
	var words := []
	for w in tk: words.append(str(WORDS.get(w, w)))
	var s := " ".join(words)
	return [s if s != "" else key, unit]

## "label (unit)", with the unit left out when the label already says it.
static func titled(key: String) -> String:
	var p := pretty(key); var u := str(p[1])
	return str(p[0]) if u == "" or str(p[0]).contains(u) else "%s (%s)" % [p[0], u]

static func num(v: float, key := "") -> String:
	if COUNTS.has(key.to_lower()) or (is_equal_approx(v, roundf(v)) and absf(v) < 1.0e6): return "%d" % int(roundf(v))
	if absf(v) >= 100.0: return "%d" % int(roundf(v))
	if absf(v) >= 10.0: return "%.1f" % v
	return "%.2f" % v

## A value with its unit, no doubled units: "68 bpm", "4.95 L/min".
static func valu(key: String, v: float) -> String:
	var u := str(pretty(key)[1])
	var s := num(v, key)
	return s if u == "" else (s + u if u == "%" else "%s %s" % [s, u])

const SKIP := ["t_ms", "beat", "heart_phase", "last_beat_ms", "ejection"]
## One row of "name value unit" pairs along the bottom edge of r, for the readouts a view does not draw itself (what does not fit is left out). `only` limits it
## to the keys listed (in that order) when not empty.
static func _strip(c: Control, r: Rect2, items: Array, only := []) -> void:
	var fz := Kit.fs(14); var x := r.position.x; var y := r.end.y - 4.0
	var list := []
	if only.is_empty(): list = items
	else:
		for k in only:
			for it in items:
				if str(it[0]).to_lower() == str(k).to_lower(): list.append(it)
	for it in list:
		var key := str(it[0])
		if SKIP.has(key): continue
		var lab := str(pretty(key)[0]); var val: String = str(it[1]) if it[1] is String else valu(key, float(it[1]))
		var wl := Fonts.regular.get_string_size(lab + " ", HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x; var wv := Fonts.bold.get_string_size(val, HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x
		var w := wl + wv + 20.0
		if x + w > r.end.x: break
		c.draw_string(Fonts.regular, Vector2(x, y), lab, HORIZONTAL_ALIGNMENT_LEFT, wl, fz, Kit.TEXT_SOFT)
		c.draw_string(Fonts.bold, Vector2(x + wl, y), val, HORIZONTAL_ALIGNMENT_LEFT, wv + 4.0, fz, Kit.TEXT)
		x += w

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

## The largest value of a readout's history (at least `floor_`), for scaling a quantity that has no natural range.
static func hmax(t: Telemetry, k: String, floor_ := 1e-6) -> float:
	var m := floor_
	for q in hist(t, k): m = maxf(m, absf(q))
	return m

## A tile: caption on top, big value, in a recessed well.
static func _tile(c: Control, r: Rect2, cap: String, val: String, col: Color, big := 30) -> void:
	var fz := Kit.fs(14)
	c.draw_style_box(Kit.sb(Kit.WELL, Kit.LINE, 1, 8, 0, false), r)
	c.draw_string(Fonts.regular, Vector2(r.position.x + 9, r.position.y + float(fz) + 3.0), cap, HORIZONTAL_ALIGNMENT_LEFT, r.size.x - 14, fz, Kit.TEXT_SOFT)
	c.draw_string(Fonts.bold, Vector2(r.position.x + 9, r.end.y - 8.0), val, HORIZONTAL_ALIGNMENT_LEFT, r.size.x - 14, Kit.fs(big), col)

static func _tile_h(big := 30) -> float: return float(Kit.fs(big)) + float(Kit.fs(14)) + 18.0

## The scripted event that is on now, as an amber chip at the top-left of r. Returns the height it took.
static func _banner(c: Control, t: Telemetry, r: Rect2) -> float:
	var fz := Kit.fs(14); var info := Campaign.info(str(t.task.id())); var banner := ""
	for e in info["timeline"]:
		if t.t_ms >= float(e["t_ms"]) and t.t_ms <= float(e["t_ms"]) + float(e["dur_ms"]): banner = str(e["label"])
	if banner == "": return 0.0
	var bw := Fonts.bold.get_string_size(banner.to_upper(), HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x + 24.0
	Gfx.rounded_rect(c, Rect2(r.position.x, r.position.y, bw, float(fz) + 10.0), Color(Kit.exc(), 0.9), Kit.exc(), 7.0, 1.0)
	c.draw_string(Fonts.bold, Vector2(r.position.x + 12.0, r.position.y + float(fz) + 3.0), banner.to_upper(), HORIZONTAL_ALIGNMENT_LEFT, bw, fz, Kit.BENCH)
	return float(fz) + 16.0

# ---------------------------------------------------------------- limb
## A single joint with an agonist / antagonist pair. Reads theta_deg (or angle_deg: from the horizontal, 0 = pointing right, y up, so upright is 90), ag_act /
## ant_act (0..1) or ag_force_N / ant_force_N (scaled to the largest seen) for the two muscle bellies, ag_Ia_hz / ant_Ia_hz for the spindles, ext_torque_Nm for the
## push arrow, load (0..1), and the target (target_deg or target_rad) which is drawn as a ghost only when `has_target` says the task has one.
static func _limb(c: Control, t: Telemetry, r: Rect2) -> void:
	var d := ro(t); var fz := Kit.fs(14)
	if not has_any(d, ["theta_deg", "theta_rad", "angle_deg"]): _empty(c, r, "Waiting for theta_deg (or angle_deg) in the task's readouts."); return
	var small := r.size.y < 150.0
	var deg := first(d, ["theta_deg", "angle_deg"], rad_to_deg(first(d, ["theta_rad"])))
	var has_t := (rv(d, "has_target") > 0.5) if d.has("has_target") else (d.has("target_deg") and not d.has("theta_deg"))
	var tdeg := first(d, ["target_deg"], rad_to_deg(first(d, ["target_rad"], deg_to_rad(deg))))
	var strip_h := 0.0 if small else 30.0
	var left_w := r.size.x * (0.60 if not small else 0.52)
	var box := Rect2(r.position.x, r.position.y + 2.0, left_w, r.size.y - strip_h - 4.0)
	# the geometry: the task's own points (hand, elbow, shoulder in world metres), or the same arm built from the angle
	var pts: PackedVector2Array = t.body.get("points", PackedVector2Array())
	var wp: Array
	var ang := deg_to_rad(deg)
	var real := d.has("theta_deg") and pts.size() >= 3
	if real: wp = [pts[2], pts[1], pts[0]]
	else: wp = [Vector2(-0.12, -0.28), Vector2(0, 0), Vector2(0.3 * cos(ang), 0.3 * sin(ang))]
	# fixed frame round the elbow: the shoulder and the full reach of the forearm, so the arm keeps its scale as it moves
	var Lr := maxf((wp[2] as Vector2).distance_to(wp[1]), 0.05) if real else 0.3
	var bx0 := minf((wp[0] as Vector2).x, -Lr * 1.1) - Lr * 0.12; var bx1 := Lr * 1.18; var by0 := minf((wp[0] as Vector2).y, -Lr * 1.1) - Lr * 0.12; var by1 := Lr * 1.18
	var bnd := Rect2(bx0, by0, bx1 - bx0, by1 - by0)
	var k := minf(box.size.x / maxf(bnd.size.x, 1e-6), box.size.y / maxf(bnd.size.y, 1e-6)) * 0.94
	var tf := func(p: Vector2) -> Vector2: return box.get_center() + Vector2(p.x - bnd.get_center().x, -(p.y - bnd.get_center().y)) * k
	var sh: Vector2 = tf.call(wp[0]); var el: Vector2 = tf.call(wp[1]); var hd: Vector2 = tf.call(wp[2])
	var u := el.distance_to(hd)
	# activations: the muscle bellies from activation or force, the spindles from the Ia rate
	var acts := [0.0, 0.0]; var ias := [0.0, 0.0]
	for q in 2:
		var ag := q == 0
		var ak := ["ag_act", "ag_force_N"] if ag else ["ant_act", "ant_force_N"]
		if d.has(ak[0]): acts[q] = clampf(rv(d, ak[0]), 0.0, 1.0)
		elif d.has(ak[1]): acts[q] = clampf(rv(d, ak[1]) / maxf(hmax(t, ak[1], 0.5), 0.5), 0.0, 1.0)
		var ik := "ag_Ia_hz" if ag else "ant_Ia_hz"
		if d.has(ik): ias[q] = clampf(rv(d, ik) / maxf(hmax(t, ik, 20.0), 20.0), 0.0, 1.0)
		else: ias[q] = acts[q] * 0.6
	var tt := t.t_ms * 0.001
	var dirv := (hd - el) / maxf(u, 1.0)
	# the ghost of the target, behind the arm (only when the task has one)
	if has_t:
		var tg := deg_to_rad(tdeg); var gh := el + Vector2(cos(tg), -sin(tg)) * u
		Gfx.tube_rim(c, PackedVector2Array([el, gh]), func(_q): return u * 0.10, Color(Kit.GREEN, 0.10), Color(Kit.GREEN, 0.55), 1.6)
		Gfx.dashed(c, PackedVector2Array([el, gh]), Color(Kit.GREEN, 0.8), 2.0, 8.0, 7.0); c.draw_circle(gh, u * 0.07, Color(Kit.GREEN, 0.85))
		c.draw_string(Fonts.regular, gh + Vector2(10.0, -8.0), "target", HORIZONTAL_ALIGNMENT_LEFT, 80.0, fz, Kit.GREEN)
	# the horizontal reference and the angle
	Gfx.dashed(c, PackedVector2Array([el + Vector2(-u * 0.25, 0.0), el + Vector2(u * 1.12, 0.0)]), Color(Kit.TEXT_SOFT, 0.55), 1.4, 6.0, 6.0)
	var a1 := -ang
	c.draw_arc(el, u * 0.36, minf(0.0, a1), maxf(0.0, a1), 24, Color(Kit.exc(), 0.9), 2.4, true)
	# the arm with its muscles and spindles
	var res := Anatomy.limb(c, sh, el, hd, {"ag": acts[0], "ant": acts[1], "ia_ag": ias[0], "ia_ant": ias[1], "t": tt})
	var uw: float = res["uw"]
	if not small:
		c.draw_string(Fonts.regular, (res["ag"] as Vector2) + Vector2(-uw * 3.0, -uw * 3.2), "agonist", HORIZONTAL_ALIGNMENT_CENTER, uw * 6.0, fz, Color(Kit.exc(), 0.95))
		c.draw_string(Fonts.regular, (res["ant"] as Vector2) + Vector2(-uw * 3.0, uw * 4.6), "antagonist", HORIZONTAL_ALIGNMENT_CENTER, uw * 6.0, fz, Color(Kit.inh(), 0.95))
		c.draw_string(Fonts.regular, (res["ag_aff"] as Vector2) + Vector2(-uw * 6.5, -uw * 0.6), "spindle Ia", HORIZONTAL_ALIGNMENT_RIGHT, uw * 6.0, fz, Color(Kit.GREEN, 0.9))
	# forces on the hand: the push (external torque, from the timeline) and gravity
	var nrm := Vector2(dirv.y, -dirv.x)
	if d.has("ext_torque_Nm"):
		var tq := rv(d, "ext_torque_Nm"); var tm := maxf(hmax(t, "ext_torque_Nm", 0.05), 0.05)
		if absf(tq) > 0.02 * tm:
			var ln := u * (0.22 + 0.45 * clampf(absf(tq) / tm, 0.0, 1.0)); var sg := signf(tq)
			var tail := hd + nrm * sg * (uw * 1.6); var tip := tail + nrm * sg * ln
			c.draw_line(tail, tip, Color(Kit.RED, 0.95), maxf(uw * 0.5, 2.5), true); Gfx.arrow_head(c, tip, nrm * sg, Kit.RED, uw * 1.8)
			if not small: c.draw_string(Fonts.bold, tip + nrm * sg * 6.0 + Vector2(-30.0, 14.0 if sg < 0 else -6.0), "push", HORIZONTAL_ALIGNMENT_CENTER, 60.0, fz, Kit.RED)
	var gl := u * (0.26 + 0.25 * clampf(rv(d, "load"), 0.0, 1.0))
	var gp := Vector2(box.position.x + uw * 2.5, box.end.y - gl - float(fz) * (0.5 if small else 1.6))
	Gfx.dashed(c, PackedVector2Array([gp, gp + Vector2(0, gl)]), Color(Kit.TEXT_SOFT, 0.7), 1.6, 5.0, 4.0); Gfx.arrow_head(c, gp + Vector2(0, gl), Vector2(0, 1), Color(Kit.TEXT_SOFT, 0.8), uw * 1.4)
	if not small: c.draw_string(Fonts.regular, gp + Vector2(8.0, gl * 0.55), "gravity", HORIZONTAL_ALIGNMENT_LEFT, 80.0, fz, Kit.TEXT_SOFT)
	# the numbers
	var rr := Rect2(r.position.x + left_w + 14.0, r.position.y + 4.0, r.size.x - left_w - 14.0, r.size.y - strip_h - 8.0)
	var ban := _banner(c, t, rr)
	var big := Kit.fs(36 if not small else 28); var y := rr.position.y + ban + float(big) * 0.95
	c.draw_string(Fonts.bold, Vector2(rr.position.x, y), "%s deg" % _n(deg, 1 if absf(deg) < 100.0 else 0), HORIZONTAL_ALIGNMENT_LEFT, rr.size.x, big, Kit.TEXT)
	var sub := "joint angle, 0 = horizontal"; var subcol := Kit.TEXT_SOFT
	if has_t:
		var err := deg - tdeg; sub = "target %s deg, off by %s" % [_n(tdeg, 1), _n(err, 1)]; subcol = Kit.RED if absf(err) > 6.0 else Kit.GREEN
	c.draw_string(Fonts.regular, Vector2(rr.position.x, y + float(fz) + 6.0), sub, HORIZONTAL_ALIGNMENT_LEFT, rr.size.x, fz, subcol)
	var er := Rect2(rr.position.x, y + float(fz) * 2.0 + 14.0, rr.size.x, rr.end.y - (y + float(fz) * 2.0 + 14.0))
	if er.size.y > 46.0: _spark(c, er, hist(t, "theta_deg" if d.has("theta_deg") else "angle_deg"), t, 15.0, 0.0, 0.0, Kit.exc(), "joint angle, last 15 s", tdeg if has_t else NAN)
	if strip_h > 0.0:
		var extra := []
		for kk in ["ag_force_N", "ant_force_N", "ext_torque_Nm", "ag_emg_hz", "ant_emg_hz", "gamma"]:
			if d.has(kk): extra.append([kk, rv(d, kk)])
		_strip(c, Rect2(r.position.x, r.end.y - 30.0, r.size.x, 28.0), extra)

# ---------------------------------------------------------------- heart
static var _bc := {"id": 0, "n": 0, "ep": -1, "beats": PackedFloat64Array(), "prev": -1.0, "pph": 0.0}

## Beat times (ms of model time) of the whole episode, read from the readout history: `last_beat_ms` where the task has it, else the places where `heart_phase`
## (or `beat`, the mock tasks' phase) wraps back to 0, each placed by its phase and the rate at that time. Cached and extended incrementally.
static func beat_times(t: Telemetry) -> PackedFloat64Array:
	if t == null: return PackedFloat64Array()
	var ht := t.hist_t; var n := ht.size()
	if int(_bc["id"]) != t.get_instance_id() or n < int(_bc["n"]) or int(_bc["ep"]) != t.episode:
		_bc = {"id": t.get_instance_id(), "n": 0, "ep": t.episode, "beats": PackedFloat64Array(), "prev": -1.0, "pph": 0.0}
	var beats: PackedFloat64Array = _bc["beats"]
	var lb: PackedFloat32Array = hist(t, "last_beat_ms"); var ph: PackedFloat32Array = hist(t, "heart_phase"); var hr: PackedFloat32Array = hist(t, "hr_bpm")
	if ph.is_empty(): ph = hist(t, "beat")
	var prev := float(_bc["prev"]); var pph := float(_bc["pph"])
	for i in range(int(_bc["n"]), n):
		if lb.size() == n:
			var v := float(lb[i])
			if v > 0.0 and absf(v - prev) > 0.5: beats.append(v); prev = v
		elif ph.size() == n and hr.size() == n:
			var p := float(ph[i])
			if i > 0 and p < pph - 0.3:
				var rr := 60000.0 / maxf(float(hr[i]), 20.0); var tb := float(ht[i]) - p * rr
				if beats.is_empty() or tb > beats[beats.size() - 1] + 100.0: beats.append(tb)
			pph = p
	if beats.size() > 600: beats = beats.slice(beats.size() - 400)
	_bc["n"] = n; _bc["beats"] = beats; _bc["prev"] = prev; _bc["pph"] = pph
	return beats

## The ECG complex relative to the node's firing (the beat time): P wave while the impulse crosses the atria, the QRS after the AV delay (vertices in ms, volts),
## the T wave. Every shape is a function of time since a beat, so a pause, an early beat or a changing rate shows as it happened.
const ECG_QRS := [[125.0, 0.0], [138.0, -0.12], [166.0, 1.0], [192.0, -0.26], [208.0, 0.0]]
static func _ecg_v(tm: float, beats: PackedFloat64Array, lo: int, hi: int) -> float:
	var v := 0.0
	for i in range(lo, hi):
		var dt := tm - beats[i]
		if dt < -10.0 or dt > 700.0: continue
		v += 0.16 * exp(-pow((dt - 55.0) / 24.0, 2.0)) + 0.30 * exp(-pow((dt - 375.0) / 58.0, 2.0))
		if dt > float(ECG_QRS[0][0]) and dt < float(ECG_QRS[4][0]):
			for j in 4:
				if dt <= float(ECG_QRS[j + 1][0]):
					var a: Array = ECG_QRS[j]; var b: Array = ECG_QRS[j + 1]; v += lerpf(float(a[1]), float(b[1]), (dt - float(a[0])) / (float(b[0]) - float(a[0]))); break
	return v

## The trace over [t0, t1] as pixel points inside rect `er` (baseline at `base`, `amp` px per volt). Time steps of 4 ms plus the exact QRS vertices of every beat,
## so no complex is ever missed or aliased by the sampling.
static func ecg_points(beats: PackedFloat64Array, t0: float, t1: float, er: Rect2, base: float, amp: float) -> PackedVector2Array:
	var times := PackedFloat64Array(); var tm := t0
	while tm < t1: times.append(tm); tm += 4.0
	times.append(t1)
	var lo := beats.size(); var hi := 0
	for i in beats.size():
		if beats[i] > t0 - 700.0 and beats[i] < t1 + 10.0:
			lo = mini(lo, i); hi = i + 1
			for q in ECG_QRS:
				var qt: float = beats[i] + float(q[0])
				if qt > t0 and qt < t1: times.append(qt)
	times.sort()
	var out := PackedVector2Array()
	for x in times: out.append(Vector2(er.position.x + er.size.x * (x - t0) / (t1 - t0), base - amp * _ecg_v(x, beats, lo, maxi(hi, lo))))
	return out

## The heart: an anatomical four-chamber heart whose atria and then ventricles contract on the real beats (with the AV delay) and whose SA node flashes, the rate,
## pressure and stroke volume as numbers, and an ECG drawn from the beat times. Reads hr_bpm, heart_phase / last_beat_ms / beat, map_mmHg, sys_mmHg, dia_mmHg, stroke_ml,
## NE and ACh (the nerves light with them).
static func _heart(c: Control, t: Telemetry, r: Rect2) -> void:
	var d := ro(t); var fz := Kit.fs(14)
	if not d.has("hr_bpm"): _empty(c, r, "Waiting for hr_bpm in the task's readouts."); return
	var hr := rv(d, "hr_bpm"); var tn := t.t_ms
	var beats := beat_times(t)
	var small := r.size.y < 150.0
	var strip_h := 0.0 if small else 30.0
	# milliseconds since the last beat: from the beat list, else from the phase and the rate
	var tms := -1.0
	if not beats.is_empty(): tms = tn - beats[beats.size() - 1]
	elif d.has("heart_phase") or d.has("beat"): tms = rv(d, "heart_phase", rv(d, "beat")) * 60000.0 / maxf(hr, 20.0)
	if tms > 2500.0: tms = -1.0
	var hw := r.size.x * (0.30 if not small else 0.20)
	var hbox := Rect2(r.position.x, r.position.y + 2.0, hw, r.size.y - strip_h - 4.0)
	var st := {"tms": tms, "t": t.t_ms * 0.001}
	if d.has("NE") or d.has("ACh"): st["symp"] = clampf(rv(d, "NE"), 0.0, 1.0); st["vagus"] = clampf(rv(d, "ACh"), 0.0, 1.0)
	Anatomy.heart(c, hbox, st)
	# the numbers: rate, pressure, stroke volume
	var x := r.position.x + hw + 14.0; var w := r.end.x - x; var y := r.position.y + 4.0
	var tiles := []
	tiles.append(["heart rate (bpm)", _n(hr), Kit.TEXT])
	var mapv := first(d, ["map_mmHg", "map_mmhg"], -1.0)
	if d.has("sys_mmHg") and d.has("dia_mmHg"): tiles.append(["pressure (mmHg)", "%s/%s" % [_n(rv(d, "sys_mmHg")), _n(rv(d, "dia_mmHg"))], Kit.RED])
	elif mapv >= 0.0: tiles.append(["mean pressure (mmHg)", _n(mapv), Kit.RED])
	if d.has("stroke_ml"): tiles.append(["stroke volume (mL)", _n(rv(d, "stroke_ml")), Kit.inh()])
	if small and tiles.size() > 2: tiles.resize(2)
	y += _banner(c, t, Rect2(x, y, w, 20.0)) if not small else 0.0
	var th := _tile_h(26 if small else 30); var gap := 6.0
	var tw := (w - gap * float(tiles.size() - 1)) / float(tiles.size())
	for k in tiles.size(): _tile(c, Rect2(x + (tw + gap) * float(k), y, tw, th), tiles[k][0], tiles[k][1], tiles[k][2], 26 if small else 30)
	var ey := y + th + 8.0; var er := Rect2(x, ey, w, r.end.y - strip_h - ey - 2.0)
	if er.size.y > 30.0: _ecg_panel(c, er, beats, tn)
	if strip_h > 0.0:
		var extra := []
		for kk in ["co_l_min", "cvp_mmHg", "baro_hz", "blood_lost_ml", "pooled_ml", "contractility", "hr_target", "workload", "beats", "cells"]:
			if d.has(kk): extra.append([kk, rv(d, kk)])
		_strip(c, Rect2(r.position.x, r.end.y - 30.0, r.size.x, 28.0), extra)

## The ECG strip: paper grid, the trace, the sweep position and the last R-R interval.
static func _ecg_panel(c: Control, er: Rect2, beats: PackedFloat64Array, tn: float) -> void:
	var fz := Kit.fs(14); var W := 5000.0
	c.draw_rect(er, Color("#06100f", 0.92))
	var t0 := tn - W; var base := er.position.y + er.size.y * 0.68; var amp := er.size.y * 0.50
	var g := ceilf(t0 / 200.0) * 200.0   # paper: 200 ms minor, 1 s major
	while g <= tn:
		var gx := er.position.x + er.size.x * (g - t0) / W; var major := int(roundf(g / 200.0)) % 5 == 0
		c.draw_line(Vector2(gx, er.position.y), Vector2(gx, er.end.y), Color(Kit.GREEN, 0.16 if major else 0.06), 1.0)
		g += 200.0
	for k in 4:
		var gy := er.position.y + er.size.y * float(k + 1) / 5.0; c.draw_line(Vector2(er.position.x, gy), Vector2(er.end.x, gy), Color(Kit.GREEN, 0.06), 1.0)
	c.draw_line(Vector2(er.position.x, base), Vector2(er.end.x, base), Color(Kit.GREEN, 0.18), 1.0)
	c.draw_rect(er, Color(Kit.LINE, 0.9), false, 1.0)
	if beats.is_empty():
		c.draw_string(Fonts.regular, er.position + Vector2(10, float(fz) + 3.0), "ECG: waiting for the first beat", HORIZONTAL_ALIGNMENT_LEFT, er.size.x - 20.0, fz, Kit.TEXT_SOFT); return
	var pl := ecg_points(beats, tn - W, tn, er, base, amp)
	if pl.size() > 1:
		c.draw_polyline(pl, Color(Kit.GREEN, 0.14), 7.0, true); c.draw_polyline(pl, Color(Kit.GREEN, 0.9), 1.7, true)
		Gfx.glow(c, pl[pl.size() - 1], 12.0, Color(Kit.GREEN, 0.5)); c.draw_circle(pl[pl.size() - 1], 2.6, Color(1, 1, 1, 0.9))
	var cap := "ECG, last 5 s"
	if beats.size() >= 2: cap += "   R-R %d ms" % int(roundf(beats[beats.size() - 1] - beats[beats.size() - 2]))
	var cw := Fonts.regular.get_string_size(cap, HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x + 14.0
	c.draw_rect(Rect2(er.position + Vector2(2, 2), Vector2(cw, float(fz) + 6.0)), Color(Kit.BENCH, 0.85))
	c.draw_string(Fonts.regular, er.position + Vector2(8, float(fz) + 2.0), cap, HORIZONTAL_ALIGNMENT_LEFT, cw, fz, Kit.TEXT_SOFT)

# ---------------------------------------------------------------- lungs
## The lungs: the asymmetric pair with their bronchial tree inflating with volume (0..1), the diaphragm dome below flattening as it contracts, the phrenic nerves
## lit by phrenic_drive; the breath rate and CO2 as numbers and a volume trace. Reads volume, rr_per_min, co2_mmhg, ventilation, phrenic_drive, tidal_l.
static func _lungs(c: Control, t: Telemetry, r: Rect2) -> void:
	var d := ro(t); var fz := Kit.fs(14)
	if not d.has("volume"): _empty(c, r, "Waiting for volume in the task's readouts."); return
	var vol := clampf(rv(d, "volume"), 0.0, 1.0); var small := r.size.y < 150.0
	var strip_h := 0.0 if small else 30.0
	var lw := r.size.x * (0.36 if not small else 0.24)
	var lbox := Rect2(r.position.x, r.position.y + 2.0, lw, r.size.y - strip_h - 4.0)
	var drive := clampf(rv(d, "phrenic_drive") / maxf(hmax(t, "phrenic_drive", 1.0), 1.0), 0.0, 1.0) if d.has("phrenic_drive") else (1.0 if rv(d, "inspiring") > 0.5 else 0.0)
	Anatomy.lungs(c, lbox, {"vol": vol, "t": t.t_ms * 0.001, "phrenic": drive})
	var x := r.position.x + lw + 14.0; var wd := r.end.x - x; var y := r.position.y + 4.0
	y += _banner(c, t, Rect2(x, y, wd, 20.0)) if not small else 0.0
	var th := _tile_h(26 if small else 30)
	var tiles := []
	if d.has("rr_per_min"): tiles.append(["breath rate (/min)", _n(rv(d, "rr_per_min"), 1), Kit.TEXT])
	if d.has("co2_mmhg"): tiles.append(["arterial CO2 (mmHg)", _n(rv(d, "co2_mmhg"), 1), Kit.exc()])
	if d.has("ventilation"): tiles.append(["ventilation (L/min)", _n(rv(d, "ventilation"), 1), Kit.inh()])
	if tiles.size() > 2 and small: tiles.resize(2)
	var gap := 6.0
	for q in tiles.size():
		var tw := (wd - gap * float(tiles.size() - 1)) / float(tiles.size())
		_tile(c, Rect2(x + (tw + gap) * float(q), y, tw, th), tiles[q][0], tiles[q][1], tiles[q][2], 26 if small else 30)
	var ey := y + (th + 8.0 if not tiles.is_empty() else 0.0); var er := Rect2(x, ey, wd, r.end.y - strip_h - ey - 2.0)
	if er.size.y > 36.0: _spark(c, er, hist(t, "volume"), t, 30.0, 0.0, 1.0, Kit.CYAN, "lung volume, last 30 s")
	if strip_h > 0.0:
		var extra := []
		for kk in ["tidal_l", "flow_l_s", "phrenic_drive", "tissue_pco2_mmHg", "chemo_hz"]:
			if d.has(kk): extra.append([kk, rv(d, kk)])
		_strip(c, Rect2(r.position.x, r.end.y - 30.0, r.size.x, 28.0), extra)

## A boxed strip chart of the last `window` seconds of `arr` (50 ms samples), y from lo to hi (auto when equal). `ref` draws a dashed reference line.
static func _spark(c: Control, r: Rect2, arr: PackedFloat32Array, t: Telemetry, window: float, lo: float, hi: float, col: Color, name: String, ref := NAN) -> void:
	var fz := Kit.fs(14)
	if lo == hi and arr.size() > 1:   # auto range over the window
		var n0 := mini(arr.size(), int(window / 0.05)); lo = arr[arr.size() - 1]; hi = lo
		for i in range(arr.size() - n0, arr.size()): lo = minf(lo, arr[i]); hi = maxf(hi, arr[i])
		if not is_nan(ref): lo = minf(lo, ref); hi = maxf(hi, ref)
		var pad := maxf((hi - lo) * 0.1, 1e-6); lo -= pad; hi += pad
	c.draw_rect(r, Color(Kit.BENCH, 0.6)); c.draw_rect(r, Color(Kit.LINE, 0.9), false, 1.0)
	if arr.size() < 2:
		c.draw_string(Fonts.regular, r.position + Vector2(8, float(fz) + 2.0), name, HORIZONTAL_ALIGNMENT_LEFT, r.size.x - 12, fz, Kit.TEXT_SOFT); return
	var n := mini(arr.size(), int(window / 0.05)); var pl := PackedVector2Array(); var step := maxi(1, n / maxi(int(r.size.x / 2.0), 1))
	var yy := func(v: float) -> float: return r.end.y - 4.0 - (r.size.y - 8.0) * clampf((v - lo) / maxf(hi - lo, 1e-6), 0.0, 1.0)
	if not is_nan(ref): Gfx.dashed(c, PackedVector2Array([Vector2(r.position.x, yy.call(ref)), Vector2(r.end.x, yy.call(ref))]), Color(Kit.GREEN, 0.7), 1.2, 6.0, 5.0)
	for i in range(arr.size() - n, arr.size(), step):
		var u := float(i - (arr.size() - n)) / float(maxi(n - 1, 1))
		pl.append(Vector2(r.position.x + r.size.x * u, yy.call(arr[i])))
	if pl.size() > 1: c.draw_polyline(pl, Color(col, 0.25), 6.0, true); c.draw_polyline(pl, col, 1.8, true)
	var lw := Fonts.regular.get_string_size(name, HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x + 12.0   # the caption sits on a plate so the trace never runs through it
	c.draw_rect(Rect2(r.position + Vector2(2, 2), Vector2(lw, float(fz) + 6.0)), Color(Kit.BENCH, 0.85))
	c.draw_string(Fonts.regular, r.position + Vector2(8, float(fz) + 2.0), name, HORIZONTAL_ALIGNMENT_LEFT, r.size.x - 12, fz, Kit.TEXT_SOFT)

# ---------------------------------------------------------------- eye
## An eye: the iris with its radial dilator fibres (amber, sympathetic) and the circular sphincter at the pupil margin (cyan, parasympathetic), the pupil of
## pupil_mm, light (0..1) as rays and a glow at the left. Reads pupil_mm, light, sphincter, dilator (0..1 activations).
static func _eye(c: Control, t: Telemetry, r: Rect2) -> void:
	var d := ro(t); var fz := Kit.fs(14)
	if not d.has("pupil_mm"): _empty(c, r, "Waiting for pupil_mm in the task's readouts."); return
	var small := r.size.y < 150.0; var strip_h := 0.0 if small else 30.0
	var area := Rect2(r.position.x, r.position.y + 2.0, r.size.x * (0.66 if not small else 0.55), r.size.y - strip_h - 4.0)
	var ew := minf(area.size.x * 0.34, area.size.y * 0.78)
	var ec := Vector2(area.position.x + area.size.x * 0.46, area.get_center().y)
	var light := clampf(rv(d, "light", 0.5), 0.0, 1.0)
	for i in 5:   # light rays from the left
		var y := ec.y + (float(i) - 2.0) * ew * 0.22; var ye := y + (ec.y - y) * 0.15; var xe := ec.x - ew * 1.08
		var lc := Color(1, 0.95, 0.7, 0.05 + 0.65 * light)
		c.draw_line(Vector2(area.position.x + 8.0, y), Vector2(xe, ye), lc, 1.0 + 2.5 * light, true)
		Gfx.arrow_head(c, Vector2(xe, ye), Vector2(xe - area.position.x - 8.0, ye - y), lc, 8.0)
	Gfx.glow(c, Vector2(area.position.x + 20.0, ec.y), ew * 0.9, Color(1, 0.95, 0.7, 0.40 * light))
	var sph := rv(d, "sphincter", clampf(1.0 - rv(d, "pupil_mm") / 8.0, 0.0, 1.0)); var dil := rv(d, "dilator", clampf(rv(d, "pupil_mm") / 8.0, 0.0, 1.0))
	if d.has("sphincter_hz"): sph = clampf(rv(d, "sphincter_hz") / maxf(hmax(t, "sphincter_hz", 20.0), 20.0), 0.0, 1.0)
	if d.has("dilator_hz"): dil = clampf(rv(d, "dilator_hz") / maxf(hmax(t, "dilator_hz", 20.0), 20.0), 0.0, 1.0)
	var res := Anatomy.eye(c, ec, ew, {"pupil_mm": rv(d, "pupil_mm"), "sphincter": clampf(sph, 0.0, 1.0), "dilator": clampf(dil, 0.0, 1.0)})
	if not small:   # callouts on the two muscles
		var s1: Vector2 = res["sphincter"]; var s2: Vector2 = res["dilator"]
		var p1 := Vector2(ec.x + ew * 0.45, ec.y - ew * 0.86); var p2 := Vector2(ec.x + ew * 0.45, ec.y + ew * 0.78)
		c.draw_line(s1, p1, Color(Kit.inh(), 0.7), 1.3, true); c.draw_circle(s1, 3.0, Kit.inh())
		c.draw_line(s2, p2, Color(Kit.exc(), 0.7), 1.3, true); c.draw_circle(s2, 3.0, Kit.exc())
		c.draw_string(Fonts.bold, p1 + Vector2(4, 0), "sphincter", HORIZONTAL_ALIGNMENT_LEFT, 120.0, fz, Kit.inh())
		c.draw_string(Fonts.regular, p1 + Vector2(4, float(fz) + 2.0), "parasympathetic", HORIZONTAL_ALIGNMENT_LEFT, 150.0, fz, Kit.TEXT_SOFT)
		c.draw_string(Fonts.bold, p2 + Vector2(4, float(fz) * 0.8), "dilator", HORIZONTAL_ALIGNMENT_LEFT, 120.0, fz, Kit.exc())
		c.draw_string(Fonts.regular, p2 + Vector2(4, float(fz) * 1.8 + 2.0), "sympathetic", HORIZONTAL_ALIGNMENT_LEFT, 150.0, fz, Kit.TEXT_SOFT)
	var rr := Rect2(r.position.x + area.size.x + 10.0, r.position.y + 4.0, r.size.x - area.size.x - 10.0, r.size.y - strip_h - 8.0)
	var ban := _banner(c, t, rr); var big := Kit.fs(32 if not small else 26)
	var y0 := rr.position.y + ban + float(big) * 0.95
	c.draw_string(Fonts.bold, Vector2(rr.position.x, y0), "%s mm" % _n(rv(d, "pupil_mm"), 1), HORIZONTAL_ALIGNMENT_LEFT, rr.size.x, big, Kit.TEXT)
	c.draw_string(Fonts.regular, Vector2(rr.position.x, y0 + float(fz) + 6.0), "pupil diameter", HORIZONTAL_ALIGNMENT_LEFT, rr.size.x, fz, Kit.TEXT_SOFT)
	var er := Rect2(rr.position.x, y0 + float(fz) * 2.0 + 14.0, rr.size.x, rr.end.y - (y0 + float(fz) * 2.0 + 14.0))
	if er.size.y > 46.0: _spark(c, er, hist(t, "pupil_mm"), t, 30.0, 0.0, 0.0, Kit.CYAN, "pupil, last 30 s")
	if strip_h > 0.0:
		var extra := []
		for kk in ["light", "luminance_cdm2", "retinal_hz", "sphincter_hz", "dilator_hz", "threat"]:
			if d.has(kk): extra.append([kk, rv(d, kk)])
		_strip(c, Rect2(r.position.x, r.end.y - 30.0, r.size.x, 28.0), extra)

# ---------------------------------------------------------------- gut
## A bowel segment as a tube with eight circular-muscle bands that constrict behind the bolus and relax ahead of it (the law of the intestine), the bands coloured by
## ACh (amber) and NO (cyan). Reads bolus (0..1 along the tube), seg0..seg7 (0..1 contraction) and, when the task has them, ex0..ex7 / inh0..inh7.
static func _gut(c: Control, t: Telemetry, r: Rect2) -> void:
	var d := ro(t); var fz := Kit.fs(14)
	if not d.has("bolus"): _empty(c, r, "Waiting for bolus in the task's readouts."); return
	var small := r.size.y < 150.0; var strip_h := 0.0 if small else 30.0
	var seg := []; var ex := []; var inh := []
	for i in 8:
		seg.append(rv(d, "seg%d" % i))
		ex.append(clampf(rv(d, "ex%d" % i) / maxf(hmax(t, "ex%d" % i, 1.0), 1.0), 0.0, 1.0) if d.has("ex%d" % i) else rv(d, "seg%d" % i))
		inh.append(clampf(rv(d, "inh%d" % i) / maxf(hmax(t, "inh%d" % i, 1.0), 1.0), 0.0, 1.0) if d.has("inh%d" % i) else 0.0)
	var ban := _banner(c, t, r)
	var area := Rect2(r.position.x, r.position.y + 6.0 + ban, r.size.x, r.size.y - strip_h - 8.0 - ban)
	var res := Anatomy.gut(c, area, {"seg": seg, "ex": ex, "inh": inh, "bolus": rv(d, "bolus"), "t": t.t_ms * 0.001, "labels": not small})
	var R: float = res["R"]; var x0: float = res["x0"]; var x1: float = res["x1"]
	if not small:
		var ly := area.position.y + float(fz) + 2.0
		c.draw_string(Fonts.regular, Vector2(x0 - R * 0.8, ly), "mouth end", HORIZONTAL_ALIGNMENT_LEFT, 120.0, fz, Kit.TEXT_SOFT)
		c.draw_string(Fonts.regular, Vector2(x1 - 140.0 + R * 0.8, ly), "far end", HORIZONTAL_ALIGNMENT_RIGHT, 140.0, fz, Kit.TEXT_SOFT)
		var lx := area.position.x + 12.0; var lyy := area.end.y - float(fz) * 0.6
		c.draw_line(Vector2(lx, lyy - 4.0), Vector2(lx + 20.0, lyy - 4.0), Kit.exc(), 3.0, true); c.draw_string(Fonts.regular, Vector2(lx + 26.0, lyy), "ACh: circular muscle contracts", HORIZONTAL_ALIGNMENT_LEFT, 240.0, fz, Kit.TEXT_SOFT)
		c.draw_line(Vector2(lx + 270.0, lyy - 4.0), Vector2(lx + 290.0, lyy - 4.0), Kit.CYAN, 3.0, true); c.draw_string(Fonts.regular, Vector2(lx + 296.0, lyy), "NO: it relaxes", HORIZONTAL_ALIGNMENT_LEFT, 200.0, fz, Kit.TEXT_SOFT)
	if strip_h > 0.0:
		var extra := [["bolus", "%d%% along" % int(roundf(100.0 * rv(d, "bolus")))]]
		for kk in ["gut_motility", "bolus_vel", "ACh"]:
			if d.has(kk): extra.append([kk, rv(d, kk)])
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
		if not judged.has(str(k)) and not SKIP.has(str(k)): others.append(str(k))
	others.sort()
	var y := r.position.y + _banner(c, t, r)
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
		for k in others: items.append([k, rv(d, k)])
		_strip(c, Rect2(area.position.x, area.end.y - strip_h, area.size.x, strip_h), items, DASH_FIRST)

const DASH_FIRST := ["NE", "ACh", "DA", "sympathetic_hz", "parasympathetic_hz", "rr_per_min", "co2_mmHg", "ventilation", "sys_mmHg", "dia_mmHg", "stroke_ml", "volume", "threat", "standing", "haemorrhage_ml_s"]

static func _gauge(c: Control, t: Telemetry, r: Rect2, b: Dictionary, v: float, compact: bool) -> void:
	var fz := Kit.fs(14); var lo := float(b["lo"]); var hi := float(b["hi"]); var span := maxf(hi - lo, 1e-6)
	var ok := v >= lo and v <= hi; var col := Kit.GREEN if ok else Kit.RED
	var key := str(b["name"]); var arr := hist(t, key); var glo := lo - span * 0.8; var ghi := hi + span * 0.8
	for q in arr: glo = minf(glo, q); ghi = maxf(ghi, q)
	glo = minf(glo, v); ghi = maxf(ghi, v)
	var unit := str(b["unit"])
	if unit == "": unit = str(pretty(key)[1])
	var title := str(pretty(key)[0])
	var cap := title if unit == "" or title.contains(unit) else "%s (%s)" % [title, unit]
	c.draw_style_box(Kit.sb(Kit.WELL, Color(col, 0.55), 1, 10, 0, false), r)
	c.draw_string(Fonts.regular, r.position + Vector2(10, float(fz) + 4.0), cap, HORIZONTAL_ALIGNMENT_LEFT, r.size.x - 16.0, fz, Kit.TEXT_SOFT)
	var vtxt := _n(v, 0 if absf(v) >= 100.0 else 1)
	if compact:   # a bar with the band marked and a marker at the value
		var bx := Rect2(r.position.x + 10.0, r.end.y - 18.0, r.size.x - 20.0, 8.0)
		c.draw_rect(bx, Color(Kit.LINE, 0.7)); c.draw_rect(Rect2(bx.position.x + bx.size.x * (lo - glo) / (ghi - glo), bx.position.y, bx.size.x * span / (ghi - glo), bx.size.y), Color(Kit.GREEN, 0.7))
		var mx := bx.position.x + bx.size.x * clampf((v - glo) / (ghi - glo), 0, 1); c.draw_rect(Rect2(mx - 2.0, bx.position.y - 4.0, 4.0, bx.size.y + 8.0), col)
		c.draw_string(Fonts.bold, Vector2(r.position.x + 10.0, r.position.y + float(fz) + float(Kit.fs(24)) + 6.0), vtxt, HORIZONTAL_ALIGNMENT_LEFT, r.size.x - 16.0, Kit.fs(24), col)
		return
	# an arc gauge: 240 degrees, the tolerance band on the rim, the needle on top, the value in the gap below the hub
	var spark_h := minf(r.size.y * 0.24, 40.0)
	var gh := r.size.y - float(fz) - 12.0 - spark_h
	var rad := minf(r.size.x * 0.34, gh / 1.8); var cc := Vector2(r.get_center().x, r.position.y + float(fz) + 12.0 + rad)
	var a0 := PI * 0.5 + deg_to_rad(60.0); var sweep := deg_to_rad(240.0)
	var ang := func(x: float) -> float: return a0 + sweep * clampf((x - glo) / (ghi - glo), 0.0, 1.0)
	c.draw_arc(cc, rad, a0, a0 + sweep, 48, Color(Kit.LINE, 0.9), 8.0, true)
	for k in 13:   # ticks
		var a := a0 + sweep * float(k) / 12.0; var dv := Vector2(cos(a), sin(a))
		c.draw_line(cc + dv * (rad + 7.0), cc + dv * (rad + (11.0 if k % 3 == 0 else 8.5)), Color(Kit.TEXT_SOFT, 0.55), 1.2, true)
	c.draw_arc(cc, rad, ang.call(lo), ang.call(hi), 28, Color(Kit.GREEN, 0.92), 8.0, true)
	var na: float = ang.call(v); var dn := Vector2(cos(na), sin(na)); var tip := cc + dn * (rad - 2.0)
	Gfx.glow(c, tip, rad * 0.4, Color(col, 0.3))
	var nl := PackedVector2Array([cc + dn.orthogonal() * 4.0, tip, cc - dn.orthogonal() * 4.0, cc - dn * 8.0])
	c.draw_colored_polygon(nl, col); c.draw_circle(cc, 7.0, col); c.draw_circle(cc, 3.0, Kit.BENCH)
	c.draw_string(Fonts.bold, Vector2(r.position.x, cc.y + rad * 0.78), vtxt, HORIZONTAL_ALIGNMENT_CENTER, r.size.x, Kit.fs(24), col)
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
		c.draw_string(Fonts.regular, cr.position + Vector2(8, float(fz) + 2.0), titled(str(keys[i])), HORIZONTAL_ALIGNMENT_LEFT, cr.size.x - 12.0, fz, Kit.TEXT_SOFT)
		var arr := hist(t, str(keys[i]))
		c.draw_string(Fonts.bold, Vector2(cr.position.x + 8.0, cr.end.y - 7.0), num(rv(d, str(keys[i])), str(keys[i])), HORIZONTAL_ALIGNMENT_LEFT, cr.size.x * 0.5, Kit.fs(20), Kit.TEXT)
		if arr.size() > 2 and cr.size.x > 150.0:
			var lo := arr[0]; var hi := arr[0]
			for q in arr: lo = minf(lo, q); hi = maxf(hi, q)
			var sr := Rect2(cr.position.x + cr.size.x * 0.52, cr.position.y + float(fz) + 6.0, cr.size.x * 0.44, cr.size.y - float(fz) - 12.0); var pl := PackedVector2Array(); var n := mini(arr.size(), 400); var step := maxi(1, n / maxi(int(sr.size.x / 2.0), 1))
			for k in range(arr.size() - n, arr.size(), step): pl.append(Vector2(sr.position.x + sr.size.x * float(k - (arr.size() - n)) / float(maxi(n - 1, 1)), sr.end.y - sr.size.y * (arr[k] - lo) / maxf(hi - lo, 1e-6)))
			if pl.size() > 1: c.draw_polyline(pl, Color(Gfx.VIOLET, 0.95), 1.5, true)
