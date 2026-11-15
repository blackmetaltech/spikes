class_name CreditsScreen
extends Screen
## Credits: the engine, fonts and sources (mirrors CREDITS.md). A scrolling page: mouse wheel, touch drag, or D-pad / arrows / stick. B / Esc / BACK returns.
## SP_CREDITS_SCROLL=<px> scrolls on open (screenshots).

const CREDITS := [
	["Game engine", "Godot Engine 4.7, godotengine.org (MIT)."],
	["Godot bindings", "godot-rust (gdext), godot-rust.github.io (MPL-2.0)."],
	["Typeface", "Atkinson Hyperlegible by the Braille Institute of America (SIL OFL 1.1). Licence: godot/assets/fonts/OFL-AtkinsonHyperlegible.txt."],
	["Simulation", "spike-core: deterministic spiking neural network engine, written for this project. Model parameters and their literature sources are cited in its code."],
	["Art", "Everything on screen is drawn procedurally in code. No external image assets."],
]

var scroll: ScrollContainer
var back_btn: Button

func default_focus() -> Control: return back_btn

func build() -> void:
	var f := f()
	var phone := Layout.phone
	add_child(Kit.bench())
	var margin := MarginContainer.new(); margin.set_anchors_preset(Control.PRESET_FULL_RECT)
	var mx := int(Kit.f() * 190) if not phone else 40; var my := int(36 * f) if not phone else 14
	margin.add_theme_constant_override("margin_left", mx); margin.add_theme_constant_override("margin_right", mx)
	margin.add_theme_constant_override("margin_top", my); margin.add_theme_constant_override("margin_bottom", my + 6); add_child(margin)
	var sheet := Kit.panel(Kit.PANEL, int(24 * f) if not phone else 14); margin.add_child(sheet)
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", int(14 * f)); sheet.add_child(v)

	var head := HBoxContainer.new(); head.add_theme_constant_override("separation", 20); v.add_child(head)
	var title := Kit.shout("Credits", int(56 * f) if not phone else 40, Kit.TEXT)
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL; head.add_child(title)
	back_btn = Kit.btn("BACK", func(): app.back(), "panel", int(30 * f) if not phone else 24, Vector2(150 if not phone else 110, 0))
	head.add_child(back_btn)

	scroll = ScrollContainer.new(); scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL; scroll.follow_focus = true
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED; v.add_child(scroll)
	var body := VBoxContainer.new(); body.size_flags_horizontal = Control.SIZE_EXPAND_FILL; body.add_theme_constant_override("separation", int(16 * f)); scroll.add_child(body)

	var c1 := Kit.card("SPIKE", Kit.PANEL_HI, Kit.exc()); body.add_child(c1[0])
	for e in CREDITS:
		if c1[1].get_child_count() > 1: c1[1].add_child(Kit.hsep())
		var b := VBoxContainer.new(); b.add_theme_constant_override("separation", 2); c1[1].add_child(b)
		b.add_child(Kit.shout(str(e[0]), 22, Kit.TEXT))
		b.add_child(Kit.label(str(e[1]), 17, Kit.TEXT_SOFT, true))
	var end := Kit.label("Thanks for growing a nervous system.", 18, Kit.TEXT_SOFT, true); end.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	body.add_child(end)
	if OS.get_environment("SP_CREDITS_SCROLL") != "":
		var px := int(OS.get_environment("SP_CREDITS_SCROLL"))
		get_tree().create_timer(0.3).timeout.connect(func(): if is_instance_valid(scroll): scroll.scroll_vertical = px)

func _process(dt: float) -> void:
	if scroll == null: return
	var dir := Input.get_axis("ui_up", "ui_down") + Input.get_axis("ui_page_up", "ui_page_down") * 3.0
	dir += Input.get_joy_axis(0, JOY_AXIS_RIGHT_Y) if absf(Input.get_joy_axis(0, JOY_AXIS_RIGHT_Y)) > 0.3 else 0.0
	if dir != 0.0:
		scroll.scroll_vertical = clampi(scroll.scroll_vertical + int(dir * 700.0 * dt), 0, int(scroll.get_v_scroll_bar().max_value))
