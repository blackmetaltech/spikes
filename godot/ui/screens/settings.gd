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
	var f := f(); var phone := Layout.phone
	add_child(Kit.bench())
	var margin := MarginContainer.new(); margin.set_anchors_preset(Control.PRESET_FULL_RECT)
	var mx := int((Kit.f() * 150) if not phone else 28); var my := int(34 * f) if not phone else 10
	margin.add_theme_constant_override("margin_left", mx); margin.add_theme_constant_override("margin_right", mx)
	margin.add_theme_constant_override("margin_top", my); margin.add_theme_constant_override("margin_bottom", my); add_child(margin)
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", int(18 * f) if not phone else 6); margin.add_child(v)
	var hb := Kit.page_head("Settings", func(): app.back(), phone); var head: HBoxContainer = hb[0]; back_btn = hb[1]; v.add_child(head)
	if phone:   # one slim header row: back, title, tabs
		tab_row = Kit.tabs(TABS, tab, func(t): set_tab(t)); tab_row.size_flags_horizontal = Control.SIZE_EXPAND_FILL; tab_row.alignment = BoxContainer.ALIGNMENT_END; head.add_child(tab_row)
	else:
		head.add_child(Kit.running_head("Plate III  ·  the instrument"))
		tab_row = Kit.tabs(TABS, tab, func(t): set_tab(t)); v.add_child(tab_row)
	scroll = ScrollContainer.new(); scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL; scroll.follow_focus = true
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED; v.add_child(scroll)
	var pad := MarginContainer.new(); pad.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	for side in ["right", "bottom"]: pad.add_theme_constant_override("margin_" + side, int(Kit.SHADOW) + 14)   # room for the plates' offset shadows and the scroll bar
	scroll.add_child(pad)
	body = VBoxContainer.new(); body.add_theme_constant_override("separation", int(22 * f) if not phone else 12); pad.add_child(body)
	# one card width on every tab (it was 800 px on one tab and 1800 on another): a readable column under the tabs
	body.size_flags_horizontal = Control.SIZE_EXPAND_FILL if phone else Control.SIZE_SHRINK_BEGIN
	if not phone: body.custom_minimum_size.x = minf(1180.0 * f, get_viewport_rect().size.x - 2.0 * float(mx) - 40.0)
	_fill()

func set_tab(t: String) -> void:
	tab = t
	var old := tab_row; var idx := old.get_index(); var par := old.get_parent()
	par.remove_child(old); old.queue_free()
	tab_row = Kit.tabs(TABS, tab, func(x): set_tab(x)); par.add_child(tab_row); par.move_child(tab_row, idx)
	if Layout.phone: tab_row.size_flags_horizontal = Control.SIZE_EXPAND_FILL; tab_row.alignment = BoxContainer.ALIGNMENT_END
	_fill(); scroll.scroll_vertical = 0
	if Kit.pad_mode: app.pad.ensure_focus(true)

func _fill() -> void:
	for c in body.get_children():
		body.remove_child(c); c.queue_free()
	_first = null; _fig = 0
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
	if str(ctl.get_meta("sp", "")) in ["", "Stepper", "Toggle"]: Kit.ident(ctl, "Opt" + Kit.slug(text))   # the graph's id of a settings row
	var r := Kit.row(text, ctl, hint); parent.add_child(r)
	if _first == null and (ctl is Kit.Stepper or ctl is Button or ctl is HSlider or ctl.get_child_count() > 0): _first = _focus_of(ctl)

func _focus_of(c: Control) -> Control:
	if c is Kit.Stepper or c is BaseButton or c is HSlider: return c
	for k in c.get_children():
		if k is HSlider: return k
	return null

var _fig := 0
## A numbered plate (Fig. N) for one group of settings; `accent` is kept for callers, the plates are ink.
func _section(title: String, _accent := Kit.AMBER, note := "") -> VBoxContainer:
	_fig += 1
	var c := Kit.fig_card(_fig, title, note, Kit.PANEL, 22 if not Layout.phone else 14); body.add_child(c[0]); return c[1]

func _display() -> void:
	var s := _section("Window")
	var desktop := not Layout.is_mobile() and not OS.has_feature("web")
	if desktop:
		_add_row(s, "Window mode", Kit.stepper([["0", "Windowed"], ["1", "Fullscreen"], ["2", "Exclusive"]], str(Settings.window_mode), func(id): Settings.window_mode = int(id); _commit()))
	_add_row(s, "V-sync", Kit.stepper([["0", "Off"], ["1", "On"], ["2", "Adaptive"]], str(Settings.vsync), func(id): Settings.vsync = int(id); _commit()))
	_add_row(s, "Frame cap", Kit.stepper([["0", "None"], ["30", "30"], ["60", "60"], ["90", "90"], ["120", "120"], ["144", "144"], ["240", "240"]], str(Settings.fps_cap), func(id): Settings.fps_cap = int(id); _commit()),
		"The app sleeps at 10 fps when unfocused.")
	var u := _section("Interface")
	_add_row(u, "Layout", Kit.stepper([["auto", "Auto"], ["desktop", "Desktop"], ["handheld", "Handheld"], ["phone", "Phone"]], Settings.layout, func(id): Settings.layout = id; _commit(true)), "Auto picks from the device and window shape (now: %s)." % Layout.kind)
	_add_row(u, "UI scale", Kit.stepper([["0", "Auto"], ["0.8", "80%"], ["1", "100%"], ["1.25", "125%"], ["1.5", "150%"], ["1.75", "175%"], ["2", "200%"]], _scale_id(), func(id): Settings.ui_scale = float(id); _commit(true)))
	_add_row(u, "Text size", Kit.stepper([["1", "Normal"], ["1.15", "Large"], ["1.3", "Larger"], ["1.5", "Huge"]], _boost_id(), func(id): Settings.text_boost = float(id); _commit(true)),
		("UI scale and text size together are held at %d%% in this window, so the lab always fits 1280 x 720." % int(roundf(Settings.max_scale(get_window()) * 100.0))) if Settings.scale_capped(get_window()) else "")
	var a := _section("Accessibility", Kit.GREEN)
	_add_row(a, "Reduced motion", Kit.toggle(Settings.reduced_motion, func(v): Settings.reduced_motion = v; _commit()), "No sliding or drifting. Menus fade instead.")
	_add_row(a, "Colour-blind safe colours", Kit.toggle(Settings.colorblind, func(v): Settings.colorblind = v; _commit(true)), "Sienna and Prussian blue already stay apart for every common colour vision type; this also pulls them apart in lightness.")
	var cc := _section("About", Kit.TEXT_SOFT)
	_add_row(cc, "Credits and licences", Kit.btn("Credits", func(): app.show_screen("credits"), "panel", 24, Vector2(170, 0)), "Engine, fonts and sources.")

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
	br.add_child(Kit.btn("CLEAR", func(): ConfirmCard.ask(self, "Clear the play log?", "All %d recorded events are deleted. Your progress is not touched." % Playlog.count(), "Clear", func(): Playlog.clear(); _log_refresh()), "panel", 22, Vector2(130, 0)))
	_log_refresh()
	var sv := _section("Save and logs", Kit.inh())
	sv.add_child(Kit.label("Progress, settings, goals, the play log and every circuit you have on a bench live in %s. Each file is written safely and keeps three backups." % ProjectSettings.globalize_path("user://"), 17, Kit.TEXT, true))
	_save_note = Kit.label("", 17, Kit.TEXT_SOFT, true); sv.add_child(_save_note)
	var sb := HFlowContainer.new(); sb.add_theme_constant_override("h_separation", 12); sb.add_theme_constant_override("v_separation", 8); sv.add_child(sb)
	sb.add_child(Kit.btn("Export save", _export_save, "panel", 22))
	sb.add_child(Kit.btn("Import save", _import_dialog, "panel", 22))
	sb.add_child(Kit.btn("Open log folder", func(): OS.shell_open(SaveIO.log_dir()), "panel", 22))
	sb.add_child(Kit.btn("Reset progress", func(): ConfirmCard.ask(self, "Reset all progress?", "Every star, score and unlock goes. Your circuits stay. The old progress file is kept next to the new one (progress.cfg.reset-...), so it can be put back by hand.", "Reset progress", _reset_progress, true), "danger", 22))

var _mode_note: Label
var _save_note: Label
var _import_dlg: FileDialog
func _say(msg: String) -> void:
	if _save_note != null and is_instance_valid(_save_note): _save_note.text = msg

func _export_save() -> void:
	var dir := OS.get_system_dir(OS.SYSTEM_DIR_DOCUMENTS)
	if dir == "" or not DirAccess.dir_exists_absolute(dir): dir = ProjectSettings.globalize_path("user://")
	var path := dir.path_join("spike-save-%s.zip" % Time.get_datetime_string_from_system().replace(":", "").replace("-", ""))
	var err := SaveIO.export_zip(path)
	_say(("Saved to " + path) if err == "" else "Could not export: " + err)

func _import_dialog() -> void:
	if _import_dlg == null:
		_import_dlg = FileDialog.new(); Kit.ident(_import_dlg, "ImportDlg"); _import_dlg.access = FileDialog.ACCESS_FILESYSTEM; _import_dlg.file_mode = FileDialog.FILE_MODE_OPEN_FILE
		_import_dlg.filters = PackedStringArray(["*.zip ; Spike save"]); _import_dlg.title = "Import save"; add_child(_import_dlg)
		_import_dlg.file_selected.connect(func(p): ConfirmCard.ask(self, "Replace your save?", "Progress, settings, goals, the play log and circuits are replaced by the ones in %s. The files they replace keep their backups." % p.get_file(), "Import", func(): _do_import(p), true))
	_import_dlg.popup_centered_clamped(Vector2i(900, 560), 0.9)

func _do_import(p: String) -> void:
	var err := SaveIO.import_zip(p)
	if err == "": Progress.reload(); Settings.load_all(); Settings.apply(get_window()); app.relayout(); _say("Imported " + p.get_file() + ".")
	else: _say("Nothing was imported: " + err)

func _reset_progress() -> void:
	var moved := Progress.reset()
	_say("Progress reset." + ((" The old file is kept at " + moved) if moved != "" else ""))
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
	var n := _section("Note")
	n.add_child(Kit.label("Every spike is a soft click: warm for excitatory neurons, cool for inhibitory, pitch set by the population. Dense firing is thinned into a crackle. The heartbeat and breathing follow the body.", 17, Kit.TEXT, true))

func _controls() -> void:
	var intro := Kit.label("Spike is mouse and touch first. Keyboard and gamepad can drive every menu.", 18, Kit.TEXT, true)
	body.add_child(intro)
	for def in [["Keyboard", Kit.AMBER, [["Move focus", "Arrow keys"], ["Press", "Enter"], ["Back", "Esc"]]], ["Gamepad", Kit.TEXT, [["Move focus", "D-pad"], ["Press", "A"], ["Back", "B"]]], ["Touch and mouse", Kit.GREEN, [["Press", "Tap or click"], ["Scroll lists", "Drag"]]]]:
		_fig += 1
		var c := Kit.fig_card(_fig, def[0], "", Kit.PANEL, 22 if not Layout.phone else 14); body.add_child(c[0])
		for r in def[2]:
			var h := HBoxContainer.new(); h.add_theme_constant_override("separation", 8)
			var l := Kit.label(r[0], 18, Kit.TEXT); l.size_flags_horizontal = Control.SIZE_EXPAND_FILL; h.add_child(l)
			h.add_child(Kit.mono(r[1], 18, Kit.TEXT_SOFT)); c[1].add_child(h)
	_first = null
