class_name ScoreCard
extends Control
## The score read-out: a ring gauge with the 0..100 number, a status chip, the verdict sentence and the score parts.
var tele: Telemetry = null
func _ready() -> void: custom_minimum_size = Vector2(300, 104); clip_contents = true
func min_text_px() -> float: return float(Kit.fs(14)) * Kit.ppl
func attach(t: Telemetry) -> void: tele = t
func _process(_dt: float) -> void:
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
	if has_task and v > 0.0:
		Gfx.glow(self, c, r * 1.9, Color(col, 0.10))
		draw_arc(c, r, -PI * 0.5, -PI * 0.5 + TAU * clampf(v / 100.0, 0.0, 1.0), 64, col, 7.0, true)
	var big := Kit.fs(26)
	draw_string(fb, c + Vector2(-r, big * 0.36), "%d" % int(roundf(v)) if has_task else "-", HORIZONTAL_ALIGNMENT_CENTER, r * 2.0, big, Kit.TEXT)
	var x := 120.0; var w := size.x - x - 12.0
	var status := "NO TASK"
	if has_task:
		status = "PASSED" if passed else ("RUNNING" if (tele.running and not tele.finished) else ("FINISHED: NOT PASSED" if tele.finished else "PAUSED"))
		if tele.sim == null: status = "BUILD A CIRCUIT"
	var sw := fb.get_string_size(status, HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x + 16.0
	draw_style_box(Kit.sb(col, col, 0, 6, 0, false), Rect2(x, 10, sw, float(fz) + 8.0))
	draw_string(fb, Vector2(x + 8, 10 + float(fz) + 1.0), status, HORIZONTAL_ALIGNMENT_LEFT, sw, fz, Kit.BENCH)
	var verdict := str(sc.get("verdict", "")) if has_task and tele.sim != null else ("Add neurons and connect sensors to motors." if has_task else "")
	draw_multiline_string(f, Vector2(x, 10 + float(fz) * 2.6), verdict, HORIZONTAL_ALIGNMENT_LEFT, w, fz, 2, Kit.TEXT_SOFT)
	var parts: Dictionary = sc.get("parts", {}) if has_task else {}
	var px := x; var py := size.y - 12.0
	for k in parts:
		var s := "%s %s" % [str(k).replace("_", " "), _fmt(float(parts[k]))]
		var tw := f.get_string_size(s, HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x + 16.0
		if px + tw > size.x - 8.0: break
		draw_style_box(Kit.sb(Kit.PANEL_HI, Kit.LINE, 1, 6, 0, false), Rect2(px, py - float(fz) - 4.0, tw, float(fz) + 8.0))
		draw_string(f, Vector2(px + 8, py), s, HORIZONTAL_ALIGNMENT_LEFT, tw, fz, Kit.TEXT); px += tw + 6.0

static func _fmt(v: float) -> String:
	if absf(v) >= 100.0 or is_equal_approx(v, roundf(v)): return "%d" % int(roundf(v))
	return "%.1f" % v if absf(v) >= 1.0 else "%.2f" % v
