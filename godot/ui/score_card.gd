class_name ScoreCard
extends Control
## The score read-out: a pencilled needle gauge with the 0..100 number that turns into a rubber stamp ("PASSED 70 of 100") the moment the run passes,
## a status label, the verdict sentence and the score parts.
var tele: Telemetry = null
var _was_passed := false
var guilty := ""                   # v0.3 verdict highlight: the score part the failed verdict is about gets a red outline
var check_text := ""               # v0.3 instant check: the last check's result, shown beside the status chip
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
	draw_rect(Rect2(Vector2.ZERO, size), Kit.CARD)
	var c := Vector2(60, size.y * 0.5 - 4.0); var r := minf(46.0, size.y * 0.5 - 10.0)
	var sc: Dictionary = tele.score if tele != null else {}
	var v := float(sc.get("value", 0.0)); var passed := bool(sc.get("passed", false)); var has_task := tele != null and tele.task != null
	var built := has_task and tele.sim != null   # nothing runs on an empty bench: its "score" is the task's reading of silence, not a result, so the card shows none
	if has_task and not built: v = 0.0; passed = false; sc = {}
	if not passed: v = minf(v, 99.0)   # 100 is a pass: a run that is still short of it (or failed on a band) never reads 100, not even rounded up
	v = floorf(v) if not passed else roundf(v)
	var col := Kit.GREEN if passed else (Kit.exc() if v >= 40.0 else Kit.RED)
	if not has_task: col = Kit.TEXT_SOFT
	# the score: a rubber stamp once passed (it lands when the run first passes), a pencilled needle gauge until then
	if has_task and passed:
		var k := clampf(_pulse / 0.35, 0.0, 1.0) if _pulse >= 0.0 else 1.0
		Gfx.stamp(self, c, r, "PASSED", "%d" % int(v), "of 100", Kit.RED, -0.2, k)
	else:
		var gc := c + Vector2(0, r * 0.35); var gr := r * 1.02
		draw_arc(gc, gr, PI * 1.08, PI * 1.92, 40, Color(Kit.INK, 0.85), 1.6, true)
		for i in 11:   # ticks every 10
			var a := PI * 1.08 + PI * 0.84 * float(i) / 10.0; var d := Vector2(cos(a), sin(a))
			draw_line(gc + d * gr, gc + d * (gr - (8.0 if i % 5 == 0 else 4.5)), Color(Kit.INK, 0.8), 1.2 if i % 5 == 0 else 1.0, true)
		draw_arc(gc, gr - 2.0, PI * 1.08 + PI * 0.84 * 0.95, PI * 1.92, 8, Kit.GREEN, 4.0, true)
		if built:
			var na := PI * 1.08 + PI * 0.84 * clampf(v / 100.0, 0.0, 1.0); var nd := Vector2(cos(na), sin(na))
			draw_line(gc, gc + nd * (gr - 6.0), col.darkened(0.1), 2.2, true); draw_circle(gc, 4.0, Kit.INK)
		var big := Kit.fs(26)
		draw_string(Fonts.display_heavy, gc + Vector2(-r, big * 0.9), "%d" % int(v) if built else "-", HORIZONTAL_ALIGNMENT_CENTER, r * 2.0, big, Kit.TEXT)
	if has_task and tele.task != null:   # earned stars under the number
		var st := int(Progress.best(str(tele.task.id()))["stars"])
		for k in 3: draw_star(Vector2(c.x + (float(k) - 1.0) * 17.0, size.y - 11.0), 7.0, Kit.GAMBOGE if k < st else Color(Kit.LINE, 0.9))
	var x := 120.0; var w := size.x - x - 12.0
	var status := "NO TASK"
	if has_task:
		status = "PASSED" if passed else ("RUNNING" if (tele.running and not tele.finished) else ("FINISHED: NOT PASSED" if tele.finished else "PAUSED"))
		if tele.sim == null: status = "BUILD A CIRCUIT"
	var sw := fb.get_string_size(status, HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x + 16.0
	draw_rect(Rect2(x, 10, sw, float(fz) + 8.0), Color(col, 0.08)); draw_rect(Rect2(x, 10, sw, float(fz) + 8.0), col, false, 1.4)
	draw_string(fb, Vector2(x + 8, 10 + float(fz) + 1.0), status, HORIZONTAL_ALIGNMENT_LEFT, sw, fz, col.darkened(0.15))
	if check_text != "" and has_task: draw_string(f, Vector2(x + sw + 10.0, 10 + float(fz) + 1.0), check_text, HORIZONTAL_ALIGNMENT_LEFT, size.x - x - sw - 18.0, fz, Kit.TEXT_SOFT)
	var verdict := str(sc.get("verdict", "")) if has_task else ""
	if has_task and verdict == "": verdict = "Add neurons and connect sensors to motors."
	if has_task and tele != null and tele.task != null: verdict = Circuits.cap_verdict(verdict, tele.circuit, str(tele.task.id()))
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
		var s := "%s %s" % [str(BodyViews.pretty(str(k))[0]), str(parts[k]) if parts[k] is String else BodyViews.valu(str(k), float(parts[k]))]
		var tw := f.get_string_size(s, HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x + 16.0
		if px + tw > size.x - 8.0:
			row += 1
			if row >= rows_max: break
			px = x; py -= float(fz) + 12.0
			if px + tw > size.x - 8.0: break
		var bad := str(k) == guilty and not passed
		draw_style_box(Kit.sb(Color(Kit.RED, 0.10) if bad else Color("#FBF7EE"), Kit.RED if bad else Kit.LINE, 2 if bad else 1, 2, 0, false), Rect2(px, py - float(fz) - 4.0, tw, float(fz) + 8.0))
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
