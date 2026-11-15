class_name GaugeView
extends Control
## Output-rate gauges: one LED bar graph per motor channel (Hz), plus the run read-out (simulated time, spikes, delivered speed).
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
	draw_style_box(Kit.sb(Kit.WELL, Kit.LINE, 1, 12, 0, false), Rect2(Vector2.ZERO, size))
	if tele == null or tele.sim == null: return
	draw_string(f, Vector2(14, 20), "OUTPUT RATES", HORIZONTAL_ALIGNMENT_LEFT, 200, fz, Kit.TEXT_SOFT)
	var r := 0; var lab_w := float(fz) * 9.0; var bh := minf(18.0, (size.y - 34.0) / float(rows()) - 6.0)
	for name in tele.rates:
		var v := float(tele.rates[name]); var mx := maxf(float(_max.get(name, 40.0)) * 0.995, maxf(40.0, v * 1.25)); _max[name] = mx
		var y := 32.0 + r * (bh + 8.0); r += 1
		draw_string(f, Vector2(14, y + bh * 0.5 + float(fz) * 0.35), str(name), HORIZONTAL_ALIGNMENT_LEFT, lab_w, fz, Kit.TEXT)
		var x0 := 14.0 + lab_w; var x1 := size.x - 90.0; var segs := 28; var sw := (x1 - x0) / segs
		for k in segs:
			var u := float(k + 1) / segs; var lit := v / mx >= u - 0.5 / segs
			var col := Gfx.VIOLET if u < 0.75 else (Kit.exc() if u < 0.92 else Kit.RED)
			draw_rect(Rect2(x0 + k * sw + 1.0, y, sw - 2.0, bh), Color(col, 0.95 if lit else 0.13))
		draw_string(f, Vector2(x1 + 8, y + bh * 0.5 + float(fz) * 0.35), "%d Hz" % int(roundf(v)), HORIZONTAL_ALIGNMENT_LEFT, 80, fz, Kit.TEXT)
