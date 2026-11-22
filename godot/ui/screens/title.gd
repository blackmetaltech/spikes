class_name TitleScreen
extends Screen
## Title: Plate I. The "Spike." wordmark (Fraunces, sienna full stop), the italic motto, the menu as a numbered legend (I.-VI.) with leader dots,
## and, on the right, three procedurally inked Cajal cells: a pyramidal cell in India ink (A), a small excitatory cell in sienna (a) and an
## inhibitory basket cell in Prussian blue (b). The figure caption runs along the foot of the plate. Now and then a cell fires and a wash of
## stain blooms round its soma (still, when reduced motion is on).
var buttons := {}
var first: Button
var _col: VBoxContainer
var _foot: VBoxContainer
var _blurb: Label
var _blurb_h := 0.0
var _gaps: Array = []   # [spacer, design height]: the air above and below the wordmark, given up first when the column is short

func default_focus() -> Control: return first

func build() -> void:
	var f := f(); var phone := Layout.phone
	buttons = {}; first = null
	add_child(Kit.bench())
	var art := CajalPlate.new(); art.mouse_filter = Control.MOUSE_FILTER_IGNORE
	art.anchor_left = 0.5 if not phone else 0.52; art.anchor_right = 1.0; art.anchor_top = 0.0; art.anchor_bottom = 1.0
	add_child(art)

	var mx := int(120 * f) if not phone else 40
	var col := VBoxContainer.new(); col.anchor_left = 0.0; col.anchor_top = 0.0; col.anchor_bottom = 1.0; col.anchor_right = 0.0
	col.offset_left = mx; col.offset_top = int(64 * f) if not phone else 14; col.offset_bottom = -int(96 * f) if not phone else -44
	col.offset_right = mx + (int(720 * f) if not phone else 470)
	col.add_theme_constant_override("separation", 0); add_child(col); _col = col
	if not phone:
		col.add_child(Kit.small_caps("Plate I  ·  Histology of the nervous system of play", int(16 * f), Kit.TEXT_SOFT))
		var g1 := Kit.spacer(46 * f); col.add_child(g1); _gaps.append([g1, 46.0 * f])
	var wm := Wordmark.new(); wm.size_px = int(228 * f) if not phone else 100; col.add_child(wm)
	var g2 := Kit.spacer(30 * f if not phone else 2); col.add_child(g2); _gaps.append([g2, 30.0 * f if not phone else 2.0])
	col.add_child(Kit.italic("grow a nervous system", int(52 * f) if not phone else 30, Kit.exc()))
	if not phone:
		col.add_child(Kit.spacer(16 * f))
		_blurb = Kit.label("Wire spiking neurons, simulated with NEST, into small circuits that drive simplified bodies: pace a heart, drive a breath. Then a Bestiary of model animals and a Theory wing of computational ideas.", int(22 * f), Kit.TEXT_SOFT, true)
		col.add_child(_blurb)
	col.add_child(Kit.spacer(0, true))
	var menu := VBoxContainer.new(); menu.add_theme_constant_override("separation", 0); col.add_child(menu)
	var defs := [["I", "Play", "the campaign", "tasks"], ["II", "Laboratory", "free bench", "lab"], ["III", "What is real", "models, tuning, sources", "real"], ["IV", "Settings", "", "settings"], ["V", "Credits", "", "credits"], ["VI", "Quit", "", "quit"]]
	if OS.has_feature("web") or phone: defs.pop_back()   # no Quit on the web; on a phone the system closes apps (and the menu needs the room)
	var ms := int(40 * f) if not phone else 26
	for d in defs:
		var primary: bool = d[3] == "tasks"
		var b := Kit.legend_item(d[0], d[1], d[2], func(): _press(d[3]), primary, ms)
		if phone: b.custom_minimum_size.y = Layout.tap() if not primary else Layout.tap() + 4
		buttons[d[3]] = b; menu.add_child(b)
		if primary: menu.add_child(Kit.spacer(14 * f if not phone else 8))
		if first == null: first = b

	# the plate's foot: a ruled line, the figure caption and the edition
	# (on a phone the foot sits at the top of the figure it captions, where the plate is only fine dendrites, so the menu keeps the whole left column)
	var foot := VBoxContainer.new(); foot.anchor_left = 0.0 if not phone else 0.52; foot.anchor_right = 1.0; foot.anchor_top = 1.0; foot.anchor_bottom = 1.0
	foot.offset_left = mx if not phone else 8; foot.offset_right = -int(80 * f) if not phone else -24; foot.offset_bottom = -int(26 * f) if not phone else -6
	foot.grow_vertical = Control.GROW_DIRECTION_BEGIN
	if phone: foot.anchor_top = 0.0; foot.anchor_bottom = 0.0; foot.offset_top = 10; foot.offset_bottom = 10; foot.grow_vertical = Control.GROW_DIRECTION_END
	foot.add_theme_constant_override("separation", int(10 * f) if not phone else 4); add_child(foot)
	if not SaveIO.notices.is_empty():   # a damaged or newer save, a read-only disk: said here, with where the file is
		var nl := Kit.ident(Kit.label("\n".join(SaveIO.notices.slice(-3)), 17 if not phone else 15, Kit.RED, true), "SaveNotices") as Label
		nl.custom_minimum_size.x = 200.0; nl.size_flags_horizontal = Control.SIZE_EXPAND_FILL; foot.add_child(nl)
	if not phone:   # created only when added: an unparented node would leak as an orphan
		var rule := ColorRect.new(); rule.color = Kit.TEXT; rule.custom_minimum_size.y = 2; rule.mouse_filter = Control.MOUSE_FILTER_IGNORE
		foot.add_child(rule)
	var fr := HBoxContainer.new(); fr.add_theme_constant_override("separation", 24); foot.add_child(fr)
	var cap := "A, pyramidal cell; a, excitatory neuron (Golgi stain); b, basket interneuron, inhibitory. Simulated with NEST." if not phone else "A, pyramidal cell; a, excitatory; b, basket cell, inhibitory."
	var fc := Kit.fig_caption(1, cap, 19 if not phone else 18); fc.size_flags_horizontal = Control.SIZE_EXPAND_FILL; fr.add_child(fc)
	var ver := Kit.mono("v%s · build %s · Godot %s" % [str(ProjectSettings.get_setting("application/config/version", "dev")), SaveIO.build_id(), Engine.get_version_info().string.get_slice("-", 0)], 15, Kit.TEXT_SOFT)
	ver.size_flags_vertical = Control.SIZE_SHRINK_END; fr.add_child(ver)
	if phone: fr.remove_child(ver); foot.add_child(ver); ver.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	_foot = foot
	foot.resized.connect(_fit_column)   # a new foot each build
	if not resized.is_connected(_fit_column): resized.connect(_fit_column)   # this screen outlives its rebuilds
	_fit_column.call_deferred()

## The menu column ends above the plate's foot, whose caption wraps to two lines on narrow windows and with Large text; when the
## column would still run into it, the air round the wordmark shrinks, then the blurb under the motto goes (the menu and the wordmark stay).
func _fit_column() -> void:
	if _col == null or _foot == null or not is_instance_valid(_foot): return
	if Layout.phone: _col.offset_bottom = -6; return
	var gap := 20.0 * f()
	_col.offset_bottom = -(_foot.size.y - _foot.offset_bottom + gap)
	var room := size.y - _col.offset_top + _col.offset_bottom
	if room <= 0.0: return
	for g in _gaps: (g[0] as Control).custom_minimum_size.y = g[1]
	if _blurb != null and _blurb.visible and _blurb.size.y > 0.0: _blurb_h = _blurb.size.y   # its laid-out height (a wrapped label's minimum depends on its width)
	var over := _col.get_combined_minimum_size().y + (_blurb_h if _blurb != null and not _blurb.visible else 0.0) - room
	for g in _gaps:   # first the air round the wordmark, down to a third of it
		var take := minf(maxf(over, 0.0), float(g[1]) * 0.66)
		(g[0] as Control).custom_minimum_size.y = float(g[1]) - take; over -= take
	if _blurb != null: _blurb.visible = over <= 0.0

func _press(id: String) -> void:
	match id:
		"tasks": app.tutorial.on_play()   # a fresh save starts Plate 0; otherwise the map
		"lab": app.open_lab("")
		"settings": app.show_screen("settings")
		"credits": app.show_screen("credits")
		"real": app.open_real("title")
		"quit": app.quit_game()

## "Spike." in heavy Fraunces, tightly set, with the full stop in Golgi sienna.
class Wordmark extends Control:
	var size_px := 250
	var _font: FontVariation
	func _ready() -> void:
		mouse_filter = Control.MOUSE_FILTER_IGNORE
		_font = FontVariation.new(); _font.base_font = Fonts.display_heavy; _font.spacing_glyph = -int(size_px * 0.02)
		var w := _font.get_string_size("Spike.", HORIZONTAL_ALIGNMENT_LEFT, -1, size_px).x
		custom_minimum_size = Vector2(w, size_px * 1.02)
	func _draw() -> void:
		if _font == null: return
		var base := size_px * 0.8
		draw_string(_font, Vector2(-size_px * 0.02, base), "Spike", HORIZONTAL_ALIGNMENT_LEFT, -1, size_px, Kit.TEXT)
		var w := _font.get_string_size("Spike", HORIZONTAL_ALIGNMENT_LEFT, -1, size_px).x
		draw_string(_font, Vector2(w - size_px * 0.02, base), ".", HORIZONTAL_ALIGNMENT_LEFT, -1, size_px, Kit.exc())

## Plate I's figure: three seeded Golgi cells, drawn as vector ink in a 940 x 1080 design space scaled to the control's height.
class CajalPlate extends Control:
	const W := 940.0
	const H := 1080.0
	var pyr: Dictionary
	var exc_cell: Dictionary
	var bas: Dictionary
	var wash: WashLayer
	func _ready() -> void:
		var r := RandomNumberGenerator.new()
		r.seed = 11; pyr = Kit.Cajal.pyramidal(r, Vector2(470, 600), 1.25)
		r.seed = 5; exc_cell = Kit.Cajal.pyramidal(r, Vector2(250, 820), 0.62)
		r.seed = 3; bas = Kit.Cajal.basket(r, Vector2(700, 760), 0.9)
		wash = WashLayer.new(); wash.plate = self; wash.set_anchors_preset(Control.PRESET_FULL_RECT); wash.mouse_filter = Control.MOUSE_FILTER_IGNORE
		wash.show_behind_parent = true; add_child(wash)
		resized.connect(queue_redraw)
	func k() -> float: return size.y / H
	func origin() -> Vector2: return Vector2((size.x - W * k()) * 0.5 + (W * k() * 0.04), 0.0)
	func _draw() -> void:
		var kk := k(); var o := origin()
		Kit.Cajal.draw(self, pyr, Kit.TEXT, 0.93, o, kk)
		Kit.Cajal.draw(self, exc_cell, Kit.exc(), 1.0, o, kk)
		Kit.Cajal.draw(self, bas, Kit.inh(), 1.0, o, kk)
		var z := maxi(Kit.fs(18), int(30 * kk))
		draw_string(Fonts.italic, o + Vector2(512, 642) * kk, "A", HORIZONTAL_ALIGNMENT_LEFT, -1, z, Kit.TEXT)
		draw_string(Fonts.italic, o + Vector2(282, 870) * kk, "a", HORIZONTAL_ALIGNMENT_LEFT, -1, z, Kit.EXC_INK)
		draw_string(Fonts.italic, o + Vector2(728, 806) * kk, "b", HORIZONTAL_ALIGNMENT_LEFT, -1, z, Kit.inh())

## The stain washes behind the cells: the sienna cell (a) and the basket cell (b) fire in turn; each spike blooms and fades like wet pigment.
class WashLayer extends Control:
	var plate: CajalPlate
	var t := 0.8
	func _process(dt: float) -> void:
		if Kit.motion(): t += dt; queue_redraw()
	func _amount(phase: float) -> float:
		if not Kit.motion(): return 0.45
		var u := fmod(t + phase, 3.4)
		return 0.0 if u > 1.6 else (u / 0.18 if u < 0.18 else 1.0 - (u - 0.18) / 1.42)
	func _draw() -> void:
		if plate == null: return
		var kk := plate.k(); var o := plate.origin()
		Kit.draw_wash(self, o + Vector2(250, 818) * kk, 70.0 * kk, Kit.exc(), _amount(0.0), 1)
		Kit.draw_wash(self, o + Vector2(700, 760) * kk, 64.0 * kk, Kit.inh(), _amount(1.7), 2)
