class_name GaugeView
extends Control
## Output-rate gauges: one engraved rail per motor channel, filled with madder-plum hatching up to its rate (Hz), the number in mono at its end.
var tele: Telemetry = null
var _max := {}

func _ready() -> void: clip_contents = true
func min_text_px() -> float: return float(Kit.fs(14)) * Kit.ppl
func attach(t: Telemetry) -> void: tele = t; _max = {}
func _process(_dt: float) -> void:
	if is_visible_in_tree(): queue_redraw()

func rows() -> int: return maxi(1, tele.rates.size() if tele != null else 1)

func _draw() -> void:
	var fz := Kit.fs(14); var f := Fonts.regular
	draw_rect(Rect2(Vector2.ZERO, size), Kit.CARD)
	if tele == null or tele.sim == null: return
	draw_string(Gfx.serif_italic(), Vector2(14, 20), "Motor output", HORIZONTAL_ALIGNMENT_LEFT, 200, Kit.fs(16), Kit.TEXT)
	draw_string(Gfx.mono(), Vector2(size.x - 80, 20), "rate" if not tele.rate_units.is_empty() else "Hz", HORIZONTAL_ALIGNMENT_RIGHT, 66, fz, Kit.TEXT_SOFT)
	var r := 0; var lab_w := float(fz) * 9.0; var bh := minf(18.0, (size.y - 34.0) / float(rows()) - 6.0)
	for name in tele.rates:
		var v := float(tele.rates[name]); var ru: bool = tele.rate_units.has(name); var floor_mx := 1.0 if ru and v < 20.0 else 40.0   # rate units: arbitrary units around 1 (a siegert unit is in Hz)
		var mx := maxf(float(_max.get(name, floor_mx)) * 0.995, maxf(floor_mx, v * 1.25)); _max[name] = mx
		var y := 32.0 + r * (bh + 8.0); r += 1
		draw_string(Gfx.serif_italic(), Vector2(14, y + bh * 0.5 + float(fz) * 0.35), str(name), HORIZONTAL_ALIGNMENT_LEFT, lab_w, fz, Kit.TEXT)
		var x0 := 14.0 + lab_w; var x1 := size.x - 70.0
		var rail := Rect2(x0, y + 2.0, x1 - x0, bh - 4.0); var fw := rail.size.x * clampf(v / mx, 0.0, 1.0)
		draw_rect(rail, Color("#FBF7EE"))
		if fw > 0.5:
			var fr := Rect2(rail.position, Vector2(fw, rail.size.y)); draw_rect(fr, Color(Kit.MOTOR, 0.12))
			Gfx.hatch(self, PackedVector2Array([fr.position, Vector2(fr.end.x, fr.position.y), fr.end, Vector2(fr.position.x, fr.end.y)]), -PI * 0.3, 4.0, Kit.MOTOR, 1.6)
		draw_rect(rail, Color(Kit.INK, 0.8), false, 1.0)
		draw_string(Gfx.mono(), Vector2(x1 + 8, y + bh * 0.5 + float(fz) * 0.35), ("%.2f" % v if ru and v < 20.0 else "%d" % int(roundf(v))), HORIZONTAL_ALIGNMENT_RIGHT, 52, fz, Kit.TEXT)
