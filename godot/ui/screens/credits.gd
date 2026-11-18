class_name CreditsScreen
extends Screen
## Credits: the engine, fonts and sources (mirrors CREDITS.md). A scrolling page: mouse wheel, touch drag, or D-pad / arrows / stick. B / Esc / BACK returns.
## SP_CREDITS_SCROLL=<px> scrolls on open (screenshots).

const CREDITS := [
	["Game engine", "Godot Engine 4.7, godotengine.org (MIT)."],
	["Godot bindings", "godot-cpp (GDExtension C++ bindings), github.com/godotengine/godot-cpp (MIT)."],
	["Typefaces", "Fraunces by Undercase Type (Phaedra Charles, Flavia Zimbardi), the display and caption face. Atkinson Hyperlegible by the Braille Institute of America, the body face. IBM Plex Mono by IBM / Bold Monday, the readouts. All under the SIL Open Font License 1.1; licences in godot/assets/fonts (OFL-Fraunces.txt, OFL-AtkinsonHyperlegible.txt, OFL-IBMPlexMono.txt)."],
	["Simulation", "NEST Simulator (GPL-2.0-or-later), nest-simulator.org: every neuron and synapse model in the game is a NEST model, run by the embedded libspike engine."],
	["Models and tasks", "Izhikevich 2003, Naud et al. 2008 (adaptive exponential model), Hodgkin and Huxley 1952, Grillner 2003 (lamprey locomotion), Suzuki et al. 2008 (C. elegans salt chemotaxis), Milo et al. 2002 (network motifs). Each task header cites its sources."],
	["Art", "Everything on screen is drawn procedurally in code, after the Golgi-stained plates of Santiago Ramon y Cajal (Textura del sistema nervioso, 1899-1904): every cell is grown from a seed. No external image assets."],
]

var scroll: ScrollContainer
var back_btn: Button

func default_focus() -> Control: return back_btn

func build() -> void:
	var f := f()
	var phone := Layout.phone
	add_child(Kit.bench())
	var margin := MarginContainer.new(); margin.set_anchors_preset(Control.PRESET_FULL_RECT)
	var mx := int(Kit.f() * 190) if not phone else 28; var my := int(34 * f) if not phone else 10
	margin.add_theme_constant_override("margin_left", mx); margin.add_theme_constant_override("margin_right", mx)
	margin.add_theme_constant_override("margin_top", my); margin.add_theme_constant_override("margin_bottom", my); add_child(margin)
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", int(18 * f) if not phone else 6); margin.add_child(v)
	var hb := Kit.page_head("Credits", func(): app.back(), phone); var head: HBoxContainer = hb[0]; back_btn = hb[1]; v.add_child(head)
	if not phone: head.add_child(Kit.running_head("Plate IV  ·  acknowledgements"))

	scroll = ScrollContainer.new(); scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL; scroll.follow_focus = true
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED; v.add_child(scroll)
	var pad := MarginContainer.new(); pad.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	for side in ["right", "bottom"]: pad.add_theme_constant_override("margin_" + side, int(Kit.SHADOW) + 14)
	scroll.add_child(pad)
	var body := VBoxContainer.new(); body.size_flags_horizontal = Control.SIZE_EXPAND_FILL; body.add_theme_constant_override("separation", int(16 * f)); pad.add_child(body)

	var c1 := Kit.fig_card(1, "Spike, and what it stands on", "", Kit.PANEL, 24 if not phone else 14); body.add_child(c1[0])
	for e in CREDITS:
		if c1[1].get_child_count() > 1: c1[1].add_child(Kit.hsep())
		var b := VBoxContainer.new(); b.add_theme_constant_override("separation", 2); c1[1].add_child(b)
		b.add_child(Kit.shout(str(e[0]), 24, Kit.TEXT))
		b.add_child(Kit.label(str(e[1]), 18, Kit.TEXT_SOFT, true))
	var end := Kit.italic("Thanks for growing a nervous system.", 22, Kit.TEXT_SOFT); end.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
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
