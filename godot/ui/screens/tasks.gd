class_name TasksScreen
extends Screen
## Task select: one card per task with its biology brief, the pass bar, your best score, and BUILD (empty bench with the task's sensors and motors)
## or REFERENCE (the reference circuit already wired).
const ICONS := {"reflex": "reflex", "lamprey": "lamprey", "worm": "worm"}
var back_btn: Button
var first: Button

func default_focus() -> Control: return first if first != null else back_btn

func build() -> void:
	var f := f(); var phone := Layout.phone
	add_child(Kit.bench())
	var margin := MarginContainer.new(); margin.set_anchors_preset(Control.PRESET_FULL_RECT)
	var mx := int(70 * f) if not phone else 20; var my := int(34 * f) if not phone else 10
	for side in ["left", "right"]: margin.add_theme_constant_override("margin_" + side, mx)
	margin.add_theme_constant_override("margin_top", my); margin.add_theme_constant_override("margin_bottom", my); add_child(margin)
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", int(20 * f)); margin.add_child(v)
	var head := HBoxContainer.new(); head.add_theme_constant_override("separation", 20); v.add_child(head)
	back_btn = Kit.btn("BACK", func(): app.back(), "panel", int(26 * f) if not phone else 22, Vector2(130 if not phone else 100, 0)); head.add_child(back_btn)
	var tt := Kit.shout("Choose a task", int(54 * f) if not phone else 34, Kit.TEXT); tt.size_flags_horizontal = Control.SIZE_EXPAND_FILL; head.add_child(tt)
	head.add_child(Kit.btn("FREE RUN", func(): app.open_lab(""), "panel", int(24 * f) if not phone else 20))
	if not ClassDB.class_exists("SpikeTask"):
		v.add_child(Kit.label("The spike-gd GDExtension is not loaded. Run scripts/build-gd.sh.", 22, Kit.RED, true)); return
	var scroll := ScrollContainer.new(); scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL; scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED; v.add_child(scroll)
	var row: BoxContainer = HBoxContainer.new() if not phone else HBoxContainer.new()
	row.size_flags_horizontal = Control.SIZE_EXPAND_FILL; row.size_flags_vertical = Control.SIZE_EXPAND_FILL; row.add_theme_constant_override("separation", int(24 * f) if not phone else 12); scroll.add_child(row)
	for id in ClassDB.instantiate("SpikeTask").call("task_ids"):
		var t = ClassDB.instantiate("SpikeTask"); if not t.make(id): continue
		row.add_child(_card(id, t, f, phone))

func _card(id: String, t, f: float, phone: bool) -> Control:
	var p := Kit.panel(Kit.PANEL, int(22 * f) if not phone else 12); p.size_flags_horizontal = Control.SIZE_EXPAND_FILL; p.size_flags_vertical = Control.SIZE_EXPAND_FILL
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", int(12 * f)); p.add_child(v)
	var ic := TaskIcon.new(); ic.kind = id; ic.custom_minimum_size = Vector2(0, 230 * f if not phone else 70); v.add_child(ic)
	v.add_child(Kit.shout(str(t.title()), int(34 * f) if not phone else 24, Kit.TEXT))
	var btxt := str(t.brief())
	if phone: btxt = _lead(btxt, 150)
	var br := Kit.label(btxt, int(19 * f) if not phone else 15, Kit.TEXT_SOFT, true); v.add_child(br)
	v.add_child(Kit.spacer(0, true))
	for pair in [["SENSORS", t.sensor_channels(), Kit.GREEN], ["MOTORS", t.motor_channels(), Gfx.VIOLET]]:
		var row := HFlowContainer.new(); row.add_theme_constant_override("h_separation", 8); row.add_theme_constant_override("v_separation", 6); v.add_child(row)
		var kl := Kit.label(pair[0], int(16 * f) if not phone else 15, Kit.TEXT_SOFT); kl.custom_minimum_size.x = 100 * f if not phone else 80; kl.size_flags_vertical = Control.SIZE_SHRINK_CENTER; row.add_child(kl)
		for ch in pair[1]: row.add_child(Kit.chip(str(ch), pair[2], 16))
	var best: Dictionary = Progress.best(id)
	var chips := HBoxContainer.new(); chips.add_theme_constant_override("separation", 8); v.add_child(chips)
	chips.add_child(Kit.chip("%d s episode" % int(round(float(t.duration_ms()) / 1000.0)), Kit.PANEL_HI, 15)); (chips.get_child(0).get_child(0) as Label).label_settings = Kit.ls(15, Kit.TEXT, true)
	chips.add_child(Kit.chip("BEST %d" % int(round(float(best["value"]))) if float(best["value"]) > 0.0 else "NOT TRIED", Kit.GREEN if bool(best["passed"]) else Kit.exc(), 15))
	if bool(best["passed"]): chips.add_child(Kit.chip("PASSED", Kit.GREEN, 15))
	var bt := HBoxContainer.new(); bt.add_theme_constant_override("separation", 10); v.add_child(bt)
	var b := Kit.btn("BUILD", func(): app.open_lab(id), "amber", int(26 * f) if not phone else 20); b.size_flags_horizontal = Control.SIZE_EXPAND_FILL; bt.add_child(b)
	var r := Kit.btn("REFERENCE", func(): app.open_lab(id, true), "cyan", int(26 * f) if not phone else 20); r.size_flags_horizontal = Control.SIZE_EXPAND_FILL; bt.add_child(r)
	if first == null: first = b
	return p

## The first sentences of a brief that fit in `n` characters.
static func _lead(t: String, n: int) -> String:
	var out := ""
	for sent in t.split(". "):
		var nxt := (out + (" " if out != "" else "") + sent + ".")
		if out != "" and nxt.length() > n: break
		out = nxt
	return out

## A small vector emblem of the task's creature.
class TaskIcon extends Control:
	var kind := ""
	func _draw() -> void:
		var c := size * 0.5; var s := minf(size.x * 0.5, size.y * 1.4)
		draw_style_box(Kit.sb(Color("#081019"), Kit.LINE, 1, 12, 0, false), Rect2(Vector2.ZERO, size))
		var col := Gfx.FISH if kind == "lamprey" else Gfx.BODY
		var pts := PackedVector2Array()
		match kind:
			"lamprey":
				for i in 11: pts.append(c + Vector2(s * 0.8 * (0.5 - float(i) / 10.0) * 2.0, sin(float(i) * 0.9) * size.y * 0.16))
			"worm":
				for i in 12: pts.append(c + Vector2(s * 0.3 * (0.5 - float(i) / 11.0) * 2.0 + sin(float(i) * 0.5) * 6.0, sin(float(i) * 0.8 + 1.0) * size.y * 0.18))
				draw_arc(c, size.y * 0.45, 0, TAU, 48, Color("#2c4a73"), 2.0, true); Gfx.glow(self, c + Vector2(size.y * 0.15, -size.y * 0.1), size.y * 0.45, Color("#5fd9c0", 0.5))
			_:
				pts = PackedVector2Array([c + Vector2(-s * 0.35, 0), c, c + Vector2(s * 0.3, 0)])
				draw_circle(c + Vector2(s * 0.5, 0), size.y * 0.07, Color(1, 1, 1, 0.85))
		var sp := Gfx.cr_spline(pts, 5); var W := size.y * (0.07 if kind != "reflex" else 0.1)
		if kind == "reflex":
			draw_polyline(sp, Color(col, 0.25), W * 2.4, true); draw_polyline(sp, col, 2.0, true)
			return
		Gfx.tube_rim(self, sp, func(u): return W * (0.45 + 0.55 * sin(PI * clampf(u * 0.9 + 0.05, 0, 1))) * (1.0 - 0.4 * u), Color(col.r * 0.25, col.g * 0.3, col.b * 0.3), col, 1.6)
