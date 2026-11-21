class_name TuningView
extends RefCounted
## The tuning wing's world view (BodyViews kind "tuning"): a dial for the stimulus the task shows, with angle labels, and beside it what the level is about. A level whose readout is one cell
## (t_tuned) shows that cell's tuning curve, its response against the orientation shown, filled in as the orientations come round; a level with a ring of cells shows a bar per cell and,
## where a ring readout decodes something (t_decode), the decoded direction as a cyan arrow. The period of the tuned input is the full circle: for an orientation it is 180 degrees and the dial
## says so. Called from BodyViews._tuning.

## Score parts "rate_<angle>_hz" (the measured response at each orientation) as {angle: hz}, in angle order; {} when the level reports none.
static func curve_points(t: Telemetry) -> Dictionary:
	var out := {}
	var parts: Dictionary = t.score.get("parts", {}) if t != null else {}
	for k in parts:
		var ks := str(k)
		if ks.begins_with("rate_") and ks.ends_with("_hz") and ks.substr(5, ks.length() - 8).is_valid_int(): out[int(ks.substr(5, ks.length() - 8))] = float(parts[k])
	var keys := out.keys(); keys.sort()
	var sorted := {}
	for k in keys: sorted[k] = out[k]
	return sorted

## The angle labels of a dial whose full circle is `period` degrees: four, at a quarter of the circle each ("0", "45", "90", "135" for 180; "0", "90", "180", "270" for 360).
static func dial_labels(period: float) -> Array:
	var out := []
	for k in 4: out.append(period * float(k) / 4.0)
	return out

static func draw(c: Control, t: Telemetry, r: Rect2) -> void:
	var fz := Kit.fs(14)
	if t == null or t.sim == null or t.task == null: BodyViews._empty(c, r, "Build the cells: the tuning appears here."); return
	var stim := ""; var period := 360.0
	for ch in t.circuit.get("inputs", []):
		if str(ch.get("kind", "")) == "tuned_poisson": stim = str(ch.get("name", "")); period = float(ch.get("period_deg", 360.0))
	var y0 := r.position.y + BodyViews._banner(c, t, r)
	var area := Rect2(r.position.x, y0 + 4.0, r.size.x, r.end.y - y0 - 4.0 - float(fz) * 2.0 - 16.0)
	var rad := maxf(minf(area.size.y * 0.38, area.size.x * 0.2), 28.0); var cc := Vector2(area.position.x + rad + 40.0, area.get_center().y)
	c.draw_circle(cc, rad + 3.0, Color(Gfx.SHEET, 0.9)); c.draw_arc(cc, rad, 0.0, TAU, 64, Color(Kit.INK, 0.85), 1.6, true)
	for k in 12:
		var a := TAU * float(k) / 12.0 - PI * 0.5; var dv := Vector2(cos(a), sin(a))
		c.draw_line(cc + dv * rad, cc + dv * (rad - (9.0 if k % 3 == 0 else 5.0)), Color(Kit.INK, 0.7), 1.2, true)
	var labs := dial_labels(period)   # angle labels outside the ring: 0 at the top, increasing clockwise
	for k in labs.size():
		var a2 := TAU * float(k) / 4.0 - PI * 0.5; var txt := "%d°" % int(roundf(float(labs[k])))
		var tw := Fonts.regular.get_string_size(txt, HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x
		var p := cc + Vector2(cos(a2), sin(a2)) * (rad + 8.0 + (tw * 0.5 if k % 2 == 1 else float(fz) * 0.6))
		c.draw_string(Fonts.regular, Vector2(p.x - tw * 0.5, p.y + float(fz) * 0.35), txt, HORIZONTAL_ALIGNMENT_LEFT, -1, fz, Kit.TEXT_SOFT)
	if stim != "":
		var ang := float(t.sim.input_value(stim)) / period * TAU - PI * 0.5; var d := Vector2(cos(ang), sin(ang))
		c.draw_line(cc, cc + d * (rad - 4.0), Kit.INK, 3.0, true); c.draw_circle(cc + d * (rad - 4.0), 5.0, Kit.INK)
	var motors: Array = t.task.motor_channels(); var kinds := {}
	for o in t.sim.outputs(): kinds[str(o["name"])] = o
	var decoded := false
	for m in motors:
		if kinds.has(str(m)) and str(kinds[str(m)]["kind"]) == "population_vector":
			decoded = true
			var st := float(t.sim.output_strength(str(m)))
			if st > 0.02:
				var ba := float(t.sim.output_angle(str(m))) - PI * 0.5; var bd := Vector2(cos(ba), sin(ba))
				c.draw_line(cc, cc + bd * (rad - 6.0) * clampf(st / 0.8, 0.0, 1.0), Kit.CYAN, 5.0, true); c.draw_circle(cc + bd * (rad - 6.0) * clampf(st / 0.8, 0.0, 1.0), 6.0, Kit.CYAN)
	var cells := PackedInt32Array()
	if not motors.is_empty() and kinds.has(str(motors[0])): cells = kinds[str(motors[0])]["neurons"]
	var n := cells.size(); var bx := area.position.x + rad * 2.0 + 110.0
	var pts := curve_points(t) if n == 1 else {}
	if not pts.is_empty(): _curve(c, Rect2(bx, area.position.y + float(fz), area.end.x - bx - 12.0, area.size.y - float(fz) * 2.0), pts, period, t, fz)
	elif n > 0:
		var bw := (area.end.x - bx - 10.0) / float(maxi(n, 1)); var base := area.end.y - 6.0
		for k in n:
			var lit := clampf(float(t.flash[cells[k]]) if cells[k] < t.flash.size() else 0.0, 0.0, 1.0)
			c.draw_rect(Rect2(bx + bw * float(k) + 2.0, base - lit * area.size.y * 0.8, maxf(bw - 4.0, 2.0), maxf(lit * area.size.y * 0.8, 2.0)), Color(Kit.exc(), 0.3 + 0.7 * lit))
		c.draw_line(Vector2(bx, base), Vector2(area.end.x - 6.0, base), Kit.INK, 1.2)
		c.draw_string(Fonts.regular, Vector2(bx, base + float(fz) + 2.0), "cells, in circuit order (preferred angle rises to the right)", HORIZONTAL_ALIGNMENT_LEFT, area.end.x - bx - 8.0, fz, Kit.TEXT_SOFT)
	var cap := "ink: the stimulus"
	if decoded: cap += "; cyan: the decoded direction"
	cap += "; " + ("curve: the cell's response to each orientation" if not pts.is_empty() else "bars: each cell's activity")
	var yb := r.end.y - 6.0
	if period < 359.0: c.draw_string(Gfx.serif_italic(), Vector2(r.position.x + 6.0, yb - float(fz) - 4.0), "the full circle is %d° here: an orientation, not a direction" % int(roundf(period)), HORIZONTAL_ALIGNMENT_LEFT, r.size.x - 12.0, fz, Kit.TEXT_SOFT)
	c.draw_string(Gfx.serif_italic(), Vector2(r.position.x + 6.0, yb), cap, HORIZONTAL_ALIGNMENT_LEFT, r.size.x - 12.0, fz, Kit.TEXT_SOFT)

## The tuning curve of one cell: response (Hz) against orientation (degrees), the points joined and the axes labelled.
static func _curve(c: Control, P: Rect2, pts: Dictionary, period: float, t: Telemetry, fz: int) -> void:
	c.draw_rect(P, Color(Gfx.SHEET, 0.9)); c.draw_rect(P, Kit.INK, false, 1.2)
	var hi := 10.0
	for k in pts: hi = maxf(hi, float(pts[k]))
	hi = ceilf(hi * 1.15 / 10.0) * 10.0
	for g in 3:   # y ticks at 0, half, full
		var v := hi * float(g) / 2.0; var yy := P.end.y - P.size.y * v / hi
		c.draw_line(Vector2(P.position.x, yy), Vector2(P.end.x, yy), Color(Kit.INK, 0.18 if g > 0 else 0.5), 1.0)
		c.draw_string(Fonts.regular, Vector2(P.position.x - 40.0, yy + float(fz) * 0.35), "%d" % int(roundf(v)), HORIZONTAL_ALIGNMENT_RIGHT, 34.0, fz, Kit.TEXT_SOFT)
	c.draw_string(Fonts.regular, Vector2(P.position.x - 40.0, P.position.y - 6.0), "Hz", HORIZONTAL_ALIGNMENT_LEFT, 40.0, fz, Kit.TEXT_SOFT)
	for q in labs_for(period):
		var xx := P.position.x + P.size.x * float(q) / period
		c.draw_line(Vector2(xx, P.end.y), Vector2(xx, P.end.y + 4.0), Kit.INK, 1.0)
		c.draw_string(Fonts.regular, Vector2(xx - 20.0, P.end.y + float(fz) + 4.0), "%d°" % int(q), HORIZONTAL_ALIGNMENT_CENTER, 40.0, fz, Kit.TEXT_SOFT)
	var line := PackedVector2Array()
	for k in pts: line.append(Vector2(P.position.x + P.size.x * float(k) / period, P.end.y - P.size.y * clampf(float(pts[k]) / hi, 0.0, 1.0)))
	if t.t_ms > 1.0:
		if line.size() > 1: c.draw_polyline(line, Kit.exc(), 2.4, true)
		for p in line: c.draw_circle(p, 5.0, Kit.exc())

static func labs_for(period: float) -> Array:
	var out := []
	for k in 5: out.append(period * float(k) / 4.0)
	return out
