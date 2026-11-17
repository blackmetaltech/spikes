class_name SettingsScreen
extends Screen
## Settings: Display / Audio / Controls / Game (difficulty mode, local play log) tabs on an instrument panel. Every change is applied and saved at once.
const TABS := ["Display", "Audio", "Controls", "Game"]
var tab := "Display"
var body: VBoxContainer
var scroll: ScrollContainer
var tab_row: HBoxContainer
var back_btn: Button
var _first: Control

func default_focus() -> Control: return _first if _first != null and is_instance_valid(_first) else back_btn
func on_back() -> bool: return false

func build() -> void:
	var f := f()
	add_child(Kit.bench())
	var margin := MarginContainer.new(); margin.set_anchors_preset(Control.PRESET_FULL_RECT)
	var mx := int((Kit.f() * 150) if not Layout.phone else 40); var my := int(36 * f) if not Layout.phone else 14
	margin.add_theme_constant_override("margin_left", mx); margin.add_theme_constant_override("margin_right", mx)
	margin.add_theme_constant_override("margin_top", my); margin.add_theme_constant_override("margin_bottom", my + 6); add_child(margin)
	var sheet := Kit.panel(Kit.PANEL, int(24 * f) if not Layout.phone else 14); margin.add_child(sheet)
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", int(14 * f)); sheet.add_child(v)

	var head := HBoxContainer.new(); head.add_theme_constant_override("separation", 20); v.add_child(head)
	var title := Kit.shout("Settings", int(56 * f) if not Layout.phone else 40, Kit.TEXT)
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL; head.add_child(title)
	back_btn = Kit.btn("BACK", func(): app.back(), "panel", int(30 * f) if not Layout.phone else 24, Vector2(150 if not Layout.phone else 110, 0))
	if Layout.phone:   # one slim header row: title, tabs, back
		title.size_flags_horizontal = Control.SIZE_SHRINK_BEGIN
		tab_row = Kit.tabs(TABS, tab, func(t): set_tab(t)); tab_row.size_flags_horizontal = Control.SIZE_EXPAND_FILL; head.add_child(tab_row)
	head.add_child(back_btn)
	if not Layout.phone:
		tab_row = Kit.tabs(TABS, tab, func(t): set_tab(t)); v.add_child(tab_row)
	scroll = ScrollContainer.new(); scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL; scroll.follow_focus = true
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED; v.add_child(scroll)
	body = VBoxContainer.new(); body.size_flags_horizontal = Control.SIZE_EXPAND_FILL; body.add_theme_constant_override("separation", int(14 * f)); scroll.add_child(body)
	_fill()

func set_tab(t: String) -> void:
	tab = t
	var old := tab_row; var idx := old.get_index(); var par := old.get_parent()
	par.remove_child(old); old.queue_free()
	tab_row = Kit.tabs(TABS, tab, func(x): set_tab(x)); par.add_child(tab_row); par.move_child(tab_row, idx)
	if Layout.phone: tab_row.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_fill(); scroll.scroll_vertical = 0
	if Kit.pad_mode: app.pad.ensure_focus(true)

func _fill() -> void:
	for c in body.get_children():
		body.remove_child(c); c.queue_free()
	_first = null
	match tab:
		"Display": _display()
		"Audio": _audio()
		"Controls": _controls()
		"Game": _game()

func _commit(relayout := false) -> void:
	Settings.save_all(); Settings.apply(get_window())
	if relayout: app.relayout()

func _add_row(parent: Control, text: String, ctl: Control, hint := "") -> void:
	if parent.get_child_count() > 1: parent.add_child(Kit.hsep())
	var r := Kit.row(text, ctl, hint); parent.add_child(r)
	if _first == null and (ctl is Kit.Stepper or ctl is Button or ctl is HSlider or ctl.get_child_count() > 0): _first = _focus_of(ctl)

func _focus_of(c: Control) -> Control:
	if c is Kit.Stepper or c is BaseButton or c is HSlider: return c
	for k in c.get_children():
		if k is HSlider: return k
	return null

func _section(title: String, accent := Kit.AMBER) -> VBoxContainer:
	var c := Kit.card(title, Kit.PANEL_HI, accent); body.add_child(c[0]); return c[1]

func _display() -> void:
	var s := _section("Window")
	var desktop := not Layout.is_mobile() and not OS.has_feature("web")
	if desktop:
		_add_row(s, "Window mode", Kit.stepper([["0", "Windowed"], ["1", "Fullscreen"], ["2", "Exclusive"]], str(Settings.window_mode), func(id): Settings.window_mode = int(id); _commit()))
	_add_row(s, "V-sync", Kit.stepper([["0", "Off"], ["1", "On"], ["2", "Adaptive"]], str(Settings.vsync), func(id): Settings.vsync = int(id); _commit()))
	_add_row(s, "Frame cap", Kit.stepper([["0", "None"], ["30", "30"], ["60", "60"], ["90", "90"], ["120", "120"], ["144", "144"], ["240", "240"]], str(Settings.fps_cap), func(id): Settings.fps_cap = int(id); _commit()),
		"The app sleeps at 10 fps when unfocused.")
	var u := _section("Interface", Kit.inh())
	_add_row(u, "Layout", Kit.stepper([["auto", "Auto"], ["desktop", "Desktop"], ["handheld", "Handheld"], ["phone", "Phone"]], Settings.layout, func(id): Settings.layout = id; _commit(true)), "Auto picks from the device and window shape (now: %s)." % Layout.kind)
	_add_row(u, "UI scale", Kit.stepper([["0", "Auto"], ["0.8", "80%"], ["1", "100%"], ["1.25", "125%"], ["1.5", "150%"], ["1.75", "175%"], ["2", "200%"]], _scale_id(), func(id): Settings.ui_scale = float(id); _commit(true)))
	_add_row(u, "Text size", Kit.stepper([["1", "Normal"], ["1.15", "Large"], ["1.3", "Larger"], ["1.5", "Huge"]], _boost_id(), func(id): Settings.text_boost = float(id); _commit(true)))
	var a := _section("Accessibility", Kit.GREEN)
	_add_row(a, "Reduced motion", Kit.toggle(Settings.reduced_motion, func(v): Settings.reduced_motion = v; _commit()), "No sliding or drifting. Menus fade instead.")
	_add_row(a, "Colour-blind safe colours", Kit.toggle(Settings.colorblind, func(v): Settings.colorblind = v; _commit(true)), "Excitatory and inhibitory switch to an Okabe-Ito orange and blue.")
	var cc := _section("About", Kit.TEXT_SOFT)
	_add_row(cc, "Credits and licences", Kit.btn("CREDITS", func(): app.show_screen("credits"), "panel", 24, Vector2(170, 0)), "Engine, fonts and sources.")

func _game() -> void:
	var d := _section("Difficulty", Kit.exc())
	_add_row(d, "Mode", Kit.stepper([["guided", "Guided"], ["standard", "Standard"], ["researcher", "Researcher"]], Progress.mode(), func(id): Settings.set_mode(str(id)); _mode_note.text = str(Progress.MODE_BLURB[Progress.mode()]); Playlog.event("mode", {"mode": id}), 260.0),
		"Part of your save. Switch any time with no penalty; only the star labels say which mode a result was earned in.")
	_mode_note = Kit.label(str(Progress.MODE_BLURB[Progress.mode()]), 18, Kit.TEXT, true); d.add_child(_mode_note)
	var l := _section("Play log", Kit.GREEN)
	_add_row(l, "Record my play", Kit.toggle(Settings.telemetry, func(v): Settings.telemetry = v; _commit(); _log_refresh()), "Off by default. Keeps time per level, runs, hint tiers, quits and edits before your first pass, on this device only. Nothing is ever sent.")
	_log_note = Kit.label("", 17, Kit.TEXT_SOFT, true); l.add_child(_log_note)
	var br := HBoxContainer.new(); br.add_theme_constant_override("separation", 12); l.add_child(br)
	br.add_child(Kit.btn("EXPORT JSON", func(): _export_log(), "panel", 22, Vector2(200, 0)))
	br.add_child(Kit.btn("CLEAR", func(): Playlog.clear(); _log_refresh(), "panel", 22, Vector2(130, 0)))
	_log_refresh()

var _mode_note: Label
var _log_note: Label
func _log_refresh(msg := "") -> void:
	if _log_note == null or not is_instance_valid(_log_note): return
	var n := Playlog.count()
	_log_note.text = msg if msg != "" else ("%s. %d events recorded." % ["Recording" if Settings.telemetry else "Not recording", n])
func _export_log() -> void:
	var path := Playlog.export_file()
	_log_refresh(("Saved to " + path) if path != "" else "Could not write the file.")

func _scale_id() -> String:
	for id in ["0", "0.8", "1", "1.25", "1.5", "1.75", "2"]:
		if absf(float(id) - Settings.ui_scale) < 0.01: return id
	return "0"
func _boost_id() -> String:
	for id in ["1", "1.15", "1.3", "1.5"]:
		if absf(float(id) - Settings.text_boost) < 0.01: return id
	return "1"

func _audio() -> void:   # audio section (item H)
	var s := _section("Volume")
	_add_row(s, "Master", Kit.slider(Settings.volume, func(x): Settings.volume = x; _commit()))
	_add_row(s, "Spike clicks", Kit.slider(Settings.spike_volume, func(x): Settings.spike_volume = x; _commit()))
	_add_row(s, "Body and UI", Kit.slider(Settings.sfx_volume, func(x): Settings.sfx_volume = x; _commit()))
	_add_row(s, "Ambience", Kit.slider(Settings.music_volume, func(x): Settings.music_volume = x; _commit()))
	var n := _section("Note", Kit.inh())
	n.add_child(Kit.label("Every spike is a soft click: warm for excitatory neurons, cool for inhibitory, pitch set by the population. Dense firing is thinned into a crackle. The heartbeat and breathing follow the body.", 17, Kit.TEXT, true))

func _controls() -> void:
	var intro := Kit.label("Spike is mouse and touch first. Keyboard and gamepad can drive every menu.", 18, Kit.TEXT, true)
	body.add_child(intro)
	for def in [["Keyboard", Kit.AMBER, [["Move focus", "Arrow keys"], ["Press", "Enter"], ["Back", "Esc"]]], ["Gamepad", Kit.inh(), [["Move focus", "D-pad"], ["Press", "A"], ["Back", "B"]]], ["Touch and mouse", Kit.GREEN, [["Press", "Tap or click"], ["Scroll lists", "Drag"]]]]:
		var c := Kit.card(def[0], Kit.PANEL_HI, def[1]); body.add_child(c[0])
		for r in def[2]:
			var h := HBoxContainer.new(); h.add_theme_constant_override("separation", 8)
			var l := Kit.label(r[0], 18, Kit.TEXT); l.size_flags_horizontal = Control.SIZE_EXPAND_FILL; h.add_child(l)
			h.add_child(Kit.label(r[1], 18, Kit.TEXT_SOFT)); c[1].add_child(h)
	_first = null
