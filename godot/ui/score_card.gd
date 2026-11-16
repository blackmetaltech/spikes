class_name ScoreCard
extends Control
## The score read-out: a ring gauge with the 0..100 number, a status chip, the verdict sentence and the score parts.
var tele: Telemetry = null
var _was_passed := false
var _pulse := -1.0                 # seconds since the score first passed (glow pulse on the ring)
func _ready() -> void: custom_minimum_size = Vector2(300, 104 if Layout.phone else 134); clip_contents = true
func min_text_px() -> float: return float(Kit.fs(14)) * Kit.ppl
func attach(t: Telemetry) -> void: tele = t
func _process(dt: float) -> void:
	var p := tele != null and bool(tele.score.get("passed", false))
	if p and not _was_passed: _pulse = 0.0
	_was_passed = p
	if _pulse >= 0.0:
		_pulse += dt
		if _pulse > 1.4: _pulse = -1.0
	if is_visible_in_tree(): queue_redraw()

func _draw() -> void:
	var fz := Kit.fs(14); var f := Fonts.regular; var fb := Fonts.bold
	draw_style_box(Kit.sb(Kit.PANEL, Kit.LINE, 1, 12, 0, false), Rect2(Vector2.ZERO, size))
	var c := Vector2(58, size.y * 0.5); var r := minf(36.0, size.y * 0.5 - 12.0)
	var sc: Dictionary = tele.score if tele != null else {}
	var v := float(sc.get("value", 0.0)); var passed := bool(sc.get("passed", false)); var has_task := tele != null and tele.task != null
	var col := Kit.GREEN if passed else (Kit.exc() if v >= 40.0 else Kit.RED)
	if not has_task: col = Kit.TEXT_SOFT
	draw_arc(c, r, 0, TAU, 64, Color(Kit.LINE, 0.9), 7.0, true)
	if _pulse >= 0.0:
		var k := _pulse / 1.4
		Gfx.glow(self, c, r * (1.9 + 1.6 * k), Color(Kit.GREEN, 0.5 * (1.0 - k)))
		draw_arc(c, r + 4.0 + 18.0 * k, 0, TAU, 64, Color(Kit.GREEN, 0.8 * (1.0 - k)), 3.0, true)
	if has_task and v > 0.0:
		Gfx.glow(self, c, r * 1.9, Color(col, 0.10))
		draw_arc(c, r, -PI * 0.5, -PI * 0.5 + TAU * clampf(v / 100.0, 0.0, 1.0), 64, col, 7.0, true)
	var big := Kit.fs(26)
	draw_string(fb, c + Vector2(-r, big * 0.36), "%d" % int(roundf(v)) if has_task else "-", HORIZONTAL_ALIGNMENT_CENTER, r * 2.0, big, Kit.TEXT)
	if has_task and tele.task != null:   # earned stars under the number
		var st := int(Progress.best(str(tele.task.id()))["stars"])
		for k in 3: draw_star(Vector2(c.x + (float(k) - 1.0) * 17.0, size.y - 11.0), 7.0, Kit.AMBER if k < st else Color(Kit.LINE, 0.9))
	var x := 120.0; var w := size.x - x - 12.0
	var status := "NO TASK"
	if has_task:
		status = "PASSED" if passed else ("RUNNING" if (tele.running and not tele.finished) else ("FINISHED: NOT PASSED" if tele.finished else "PAUSED"))
		if tele.sim == null: status = "BUILD A CIRCUIT"
	var sw := fb.get_string_size(status, HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x + 16.0
	draw_style_box(Kit.sb(col, col, 0, 6, 0, false), Rect2(x, 10, sw, float(fz) + 8.0))
	draw_string(fb, Vector2(x + 8, 10 + float(fz) + 1.0), status, HORIZONTAL_ALIGNMENT_LEFT, sw, fz, Kit.BENCH)
	var verdict := str(sc.get("verdict", "")) if has_task else ""
	if has_task and verdict == "": verdict = "Add neurons and connect sensors to motors."
	draw_multiline_string(f, Vector2(x, 10 + float(fz) * 2.6), verdict, HORIZONTAL_ALIGNMENT_LEFT, w, fz, 2, Kit.TEXT_SOFT)
	var parts: Dictionary = (sc.get("parts", {}) as Dictionary).duplicate() if has_task else {}
	if has_task and tele.task != null and Progress.budget(str(tele.task.id())) > 0:
		parts["neurons"] = "%d of %d" % [Circuits.neuron_count(tele.circuit), Progress.budget(str(tele.task.id()))]
	if has_task and tele.task != null and tele.sim != null and tele.t_ms > 200.0:   # the energy read-out: spikes per second and the ATP they cost (an order of magnitude)
		var hz := tele.spike_rate_hz(); var ar := Telemetry.atp_range(hz)
		parts["energy"] = "%s spikes/s" % (str(int(roundf(hz))) if hz >= 10.0 else "%.1f" % hz)
		parts["ATP"] = "%s to %s /s" % [Telemetry._sci(ar.x), Telemetry._sci(ar.y)]
	var rows_max := 2 if size.y >= 124.0 else 1
	var px := x; var py := size.y - 12.0; var row := 0
	for k in parts:
		var s := "%s %s" % [str(k).replace("_", " "), str(parts[k]) if parts[k] is String else _fmt(float(parts[k]))]
		var tw := f.get_string_size(s, HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x + 16.0
		if px + tw > size.x - 8.0:
			row += 1
			if row >= rows_max: break
			px = x; py -= float(fz) + 12.0
			if px + tw > size.x - 8.0: break
		draw_style_box(Kit.sb(Kit.PANEL_HI, Kit.LINE, 1, 6, 0, false), Rect2(px, py - float(fz) - 4.0, tw, float(fz) + 8.0))
		draw_string(f, Vector2(px + 8, py), s, HORIZONTAL_ALIGNMENT_LEFT, tw, fz, Kit.TEXT); px += tw + 6.0

static func draw_star_on(ci: CanvasItem, c: Vector2, r: float, col: Color) -> void:
	var pts := PackedVector2Array()
	for i in 10:
		var a := -PI * 0.5 + float(i) * PI / 5.0; var rr := r if i % 2 == 0 else r * 0.45
		pts.append(c + Vector2(cos(a), sin(a)) * rr)
	ci.draw_colored_polygon(pts, col)

func draw_star(c: Vector2, r: float, col: Color) -> void: draw_star_on(self, c, r, col)

static func _fmt(v: float) -> String:
	if absf(v) >= 100.0 or is_equal_approx(v, roundf(v)): return "%d" % int(roundf(v))
	return "%.1f" % v if absf(v) >= 1.0 else "%.2f" % v
