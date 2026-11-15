class_name TitleScreen
extends Screen
## Title: the wordmark over a slowly firing circuit, a spike-train motif, and the menu.
var buttons := {}
var first: Button

func default_focus() -> Control: return first

func build() -> void:
	var f := f()
	add_child(Kit.bench())
	var backdrop := CircuitBackdrop.new(); backdrop.set_anchors_preset(Control.PRESET_FULL_RECT); backdrop.mouse_filter = Control.MOUSE_FILTER_IGNORE; add_child(backdrop)
	var margin := MarginContainer.new(); margin.set_anchors_preset(Control.PRESET_FULL_RECT)
	for side in ["left", "right"]: margin.add_theme_constant_override("margin_" + side, int(110 * f))
	margin.add_theme_constant_override("margin_top", int(48 * f)); margin.add_theme_constant_override("margin_bottom", int(56 * f))
	add_child(margin)
	var cols := HBoxContainer.new(); cols.add_theme_constant_override("separation", int(60 * f)); margin.add_child(cols)

	var left := VBoxContainer.new(); left.size_flags_horizontal = Control.SIZE_EXPAND_FILL; left.size_flags_stretch_ratio = 1.5; left.alignment = BoxContainer.ALIGNMENT_CENTER
	left.add_theme_constant_override("separation", int(6 * f)); cols.add_child(left)
	left.add_child(Kit.shout("SPIKE", int(230 * f), Kit.TEXT))
	var trace := SpikeMotif.new(); trace.custom_minimum_size = Vector2(0, 90 * f); left.add_child(trace)
	left.add_child(Kit.label("grow a nervous system", int(44 * f), Kit.exc()))
	left.add_child(Kit.spacer(14 * f))
	left.add_child(Kit.label("Wire spiking neurons into circuits that do real jobs: steer a worm up a smell gradient, swim a lamprey, dodge a shadow.", int(24 * f), Kit.TEXT_SOFT, true))

	var right := VBoxContainer.new(); right.size_flags_horizontal = Control.SIZE_EXPAND_FILL; right.size_flags_stretch_ratio = 1.0
	right.alignment = BoxContainer.ALIGNMENT_CENTER; right.add_theme_constant_override("separation", int(18 * f)); cols.add_child(right)
	var bsize := int(clampf(38 * f, 24, 38))
	var defs := [["LAB", "amber", "lab"], ["SETTINGS", "panel", "settings"], ["CREDITS", "panel", "credits"], ["QUIT", "ghost", "quit"]]
	if OS.has_feature("web"): defs.pop_back()
	for d in defs:
		var b := Kit.btn(d[0], func(): _press(d[2]), d[1], bsize)
		b.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		buttons[d[2]] = b; right.add_child(b)
		if first == null: first = b

	var foot := Kit.label("v%s    Godot %s    %s" % [ProjectSettings.get_setting("application/config/version", "dev"), Engine.get_version_info().string.get_slice(" ", 0), Layout.kind], 15, Kit.TEXT_SOFT)
	foot.set_anchors_preset(Control.PRESET_BOTTOM_LEFT); foot.offset_left = 16; foot.offset_bottom = -8; foot.offset_top = -30
	add_child(foot)

func _press(id: String) -> void:
	match id:
		"lab": app.show_screen("lab")
		"settings": app.show_screen("settings")
		"credits": app.show_screen("credits")
		"quit": app.quit_game()

## A resting-potential line with a spike: the logo's underline.
class SpikeMotif extends Control:
	func _draw() -> void:
		var w := size.x; var h := size.y; var base := h * 0.8
		var pts := PackedVector2Array([Vector2(0, base)])
		var xs := [0.18, 0.2, 0.22, 0.24, 0.26, 0.28, 0.5, 0.52, 0.54, 0.56, 0.58, 0.6, 1.0]
		var ys := [base, base, h * 0.1, base + h * 0.14, base, base, base, base, h * 0.1, base + h * 0.14, base, base, base]
		for i in xs.size(): pts.append(Vector2(w * float(xs[i]) * 0.78, ys[i]))
		draw_polyline(pts, Color(Kit.exc(), 0.25), 9.0, true)
		draw_polyline(pts, Kit.exc(), 3.0, true)
		draw_circle(pts[3], 7.0, Kit.inh())

## Decorative neurons and axons that fire now and then. Static when reduced motion is on.
class CircuitBackdrop extends Control:
	var nodes: Array = []   # [pos(0..1), excitatory]
	var links: Array = []
	var t := 0.0
	func _ready() -> void:
		var rng := RandomNumberGenerator.new(); rng.seed = 7
		for i in 22: nodes.append([Vector2(rng.randf_range(0.03, 0.97), rng.randf_range(0.05, 0.95)), rng.randf() > 0.25])
		for i in nodes.size():
			var best := []
			for j in nodes.size():
				if j != i: best.append([(nodes[i][0] - nodes[j][0]).length(), j])
			best.sort_custom(func(a, b): return a[0] < b[0])
			for k in 2: links.append([i, best[k][1]])
	func _process(dt: float) -> void:
		if Kit.motion(): t += dt; queue_redraw()
	func _draw() -> void:
		var s := size
		for l in links:
			var a: Vector2 = nodes[l[0]][0] * s; var b: Vector2 = nodes[l[1]][0] * s
			var mid := (a + b) * 0.5 + (b - a).orthogonal().normalized() * (b - a).length() * 0.18
			var col: Color = Kit.exc() if nodes[l[0]][1] else Kit.inh()
			var pts := PackedVector2Array()
			for k in 17:
				var u := float(k) / 16.0
				pts.append(a.lerp(mid, u).lerp(mid.lerp(b, u), u))
			draw_polyline(pts, Color(col, 0.16), 1.5, true)
			var ph := fmod(t * 0.25 + float(l[0]) * 0.37, 2.5)   # a pulse dot travels now and then
			if ph < 1.0: draw_circle(pts[int(ph * 16.0)], 3.5, Color(col, 0.85))
		for i in nodes.size():
			var col: Color = Kit.exc() if nodes[i][1] else Kit.inh()
			var p: Vector2 = nodes[i][0] * s
			var fire := maxf(0.0, 1.0 - fmod(t * 0.25 + float(i) * 0.37, 2.5) * 4.0) if Kit.motion() else 0.0
			draw_circle(p, 18.0 + 10.0 * fire, Color(col, 0.05 + 0.2 * fire))
			draw_circle(p, 8.0, Color(col, 0.35 + 0.6 * fire))
