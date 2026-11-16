class_name LabScreen
extends Screen
## The lab: circuit editor (left) and live instruments (right) around one running experiment.
##   editor     neurons from a NEST model list with presets, drag-to-connect, an inspector for neurons, edges and channels
##   task view  the body in its world, driven by the circuit's motor outputs
##   scopes     scrolling raster, 1-4 oscilloscope traces, output-rate gauges
##   controls   run / pause / reset, speed, score card, reference circuit, load and save JSON, analysis
## The circuit Dictionary is the single source of truth; any structural edit reloads the engine (debounced) and restarts the episode.
const SPEEDS := [["0.25", "0.25x"], ["0.5", "0.5x"], ["1.0", "1x"], ["2.0", "2x"], ["5.0", "5x"], ["10.0", "10x"], ["20.0", "20x"], ["50.0", "50x"]]
const SENSOR_KIND := {"lamprey": "dc"}

var task_id := ""
var circuit := {}
var tele := Telemetry.new()
var editor: CircuitEditor
var task_view: TaskView
var score_card: ScoreCard
var raster: RasterView
var traces: TraceView
var gauges: GaugeView
var inspector: PanelContainer
var insp_body: VBoxContainer
var run_btn: Button
var back_btn: Button
var status: Label
var banner: Label
var tab_btns := {}
var panels := {}
var tab := "circuit"
var part_card := "iaf_psc_alpha"   # the parts-bin card the next added neuron is made from (a model, or a preset of one)
var timeline_view: TimelineView
var sandbox_panel: SandboxPanel
var mod_dials: ModDials
var _parts_btn: Button
var _parts_ov: Control = null
## Lesion lab (after a pass): lesions = {"proj:<i>" | "pop:<id>" | "in:<name>": true}, rows = the lesion table, _lrun = the re-run in progress.
var lesion_on := false
var lesions := {}
var lesion_rows: Array = []
var _lrun: Lesion.Runner = null
var _lesion_btn: Button
var _lesion_note := ""
var _dirty := -1.0
var _brief_forced := false
var _file_dlg: FileDialog
var _connect_btn: Button
var ext_ok := false
var ov: Control                  # overlay layer: phone inspector sheet, menu, end-of-episode card
var insp_scroll: ScrollContainer
var _pending_banner := ""        # a message that arrived before the banner label existed (the first _reload runs before build)
var _fail_shown := false
var _adv_open := false           # the neuron inspector's ADVANCED fold
var _rec_done := false           # this episode's result has been recorded
var _rec_pass := false
var _overlay: Control = null
var _overlay_dismissed := false
var _menu: Control = null
var _palette_row: Control
var _test := {}                  # the 5-seed TEST in progress: {seeds, i, res, sim, task, t}
var _test_result := {}           # {passes, n, scores}: the last finished TEST on this circuit
const TEST_SEEDS := [101, 102, 103, 104, 105]

func default_focus() -> Control: return back_btn
func on_back() -> bool: return false

func build() -> void:
	ext_ok = ClassDB.class_exists("SpikeSim") and ClassDB.class_exists("SpikeTask")
	var phone := Layout.phone; var f := f()
	if circuit.is_empty(): _start(app.lab_task if app != null else "")
	Circuits.sandbox = task_id == ""
	add_child(Kit.bench())
	var margin := MarginContainer.new(); margin.set_anchors_preset(Control.PRESET_FULL_RECT)
	var mx := int(26 * f) if not phone else 14; var my := int(18 * f) if not phone else 8
	for side in ["left", "right"]: margin.add_theme_constant_override("margin_" + side, mx)
	margin.add_theme_constant_override("margin_top", my); margin.add_theme_constant_override("margin_bottom", my); add_child(margin)
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", int(10 * f) if not phone else 6); margin.add_child(v)
	var compact := phone or get_viewport_rect().size.x < 1180.0
	v.add_child(_topbar(phone, compact))
	if compact and not phone: v.add_child(_tabs_row())
	var body := HBoxContainer.new(); body.size_flags_vertical = Control.SIZE_EXPAND_FILL; body.add_theme_constant_override("separation", int(14 * f)); v.add_child(body)
	# left: editor
	var left := VBoxContainer.new(); left.size_flags_horizontal = Control.SIZE_EXPAND_FILL; left.size_flags_stretch_ratio = 1.55; left.add_theme_constant_override("separation", int(8 * f)); body.add_child(left)
	panels["circuit"] = left
	_palette_row = _palette(phone); left.add_child(_palette_row)
	editor = CircuitEditor.new(); editor.size_flags_vertical = Control.SIZE_EXPAND_FILL; editor.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	editor.custom_minimum_size = Vector2(300, 200)
	var edp := PanelContainer.new(); edp.add_theme_stylebox_override("panel", Kit.sb(Color("#080f1a"), Kit.LINE, 1, 12, 0, false)); edp.size_flags_vertical = Control.SIZE_EXPAND_FILL; edp.add_child(editor); left.add_child(edp)
	editor.structure_changed.connect(_on_structure); editor.moved.connect(func(): pass)
	editor.selection_changed.connect(func(_s): _build_inspector())
	editor.lesion_clicked.connect(_lesion_click)
	inspector = PanelContainer.new(); inspector.add_theme_stylebox_override("panel", Kit.sb(Kit.PANEL, Kit.LINE, 1, 12, 12 if phone else 14, false))
	insp_scroll = ScrollContainer.new(); insp_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED; inspector.add_child(insp_scroll)
	insp_body = VBoxContainer.new(); insp_body.add_theme_constant_override("separation", 8); insp_body.size_flags_horizontal = Control.SIZE_EXPAND_FILL; insp_scroll.add_child(insp_body)
	ov = Control.new(); ov.set_anchors_preset(Control.PRESET_FULL_RECT); ov.mouse_filter = Control.MOUSE_FILTER_IGNORE
	if phone:   # the inspector is a bottom sheet over the editor, so the canvas keeps the whole screen when nothing is selected
		inspector.set_anchors_preset(Control.PRESET_BOTTOM_WIDE); inspector.grow_vertical = Control.GROW_DIRECTION_BEGIN
		inspector.offset_left = float(mx); inspector.offset_right = -float(mx); inspector.offset_bottom = -float(my)
		ov.add_child(inspector)
	else: left.add_child(inspector)
	# right: instruments
	var right := VBoxContainer.new(); right.size_flags_horizontal = Control.SIZE_EXPAND_FILL; right.size_flags_stretch_ratio = 1.0; right.add_theme_constant_override("separation", int(8 * f)); body.add_child(right)
	panels["scopes"] = right
	var world := VBoxContainer.new(); world.size_flags_vertical = Control.SIZE_EXPAND_FILL; world.size_flags_stretch_ratio = 3.0; world.add_theme_constant_override("separation", int(8 * f)); right.add_child(world)
	panels["world"] = world
	var info := Campaign.info(task_id) if task_id != "" else {}
	var has_tl := task_id != "" and not ((info.get("bands", []) as Array).is_empty() and (info.get("timeline", []) as Array).is_empty())
	task_view = TaskView.new(); task_view.size_flags_vertical = Control.SIZE_EXPAND_FILL; task_view.size_flags_stretch_ratio = 2.0; world.add_child(task_view)
	timeline_view = TimelineView.new(); timeline_view.visible = has_tl
	timeline_view.custom_minimum_size = Vector2(0, TimelineView.wanted_height((info.get("bands", []) as Array).size(), phone) if has_tl else 0.0); world.add_child(timeline_view)
	score_card = ScoreCard.new(); world.add_child(score_card)
	if task_id == "":   # the sandbox: no body to draw, so the world panel is the sandbox panel (dials, energy)
		task_view.visible = false; score_card.visible = false
		sandbox_panel = SandboxPanel.new().setup(func(): return tele); world.add_child(sandbox_panel)
	world.size_flags_stretch_ratio = 5.6 if has_tl else 3.0
	var body_view := has_tl or str(info.get("view", "polyline")) != "polyline"
	if body_view: task_view.custom_minimum_size.y = 110.0 if phone else 230.0   # a body view needs room for its readouts
	var scopes := VBoxContainer.new(); scopes.size_flags_vertical = Control.SIZE_EXPAND_FILL; scopes.size_flags_stretch_ratio = 3.1 if has_tl else 4.1; scopes.add_theme_constant_override("separation", int(8 * f)); right.add_child(scopes)
	panels["scopes_only"] = scopes
	raster = RasterView.new(); raster.size_flags_vertical = Control.SIZE_EXPAND_FILL; raster.size_flags_stretch_ratio = 0.8; scopes.add_child(raster)
	traces = TraceView.new(); traces.size_flags_vertical = Control.SIZE_EXPAND_FILL; traces.size_flags_stretch_ratio = 1.3; scopes.add_child(traces)
	if body_view and not phone: raster.custom_minimum_size.y = 90.0; traces.custom_minimum_size.y = 90.0   # the scopes give way to the body view and its timeline
	gauges = GaugeView.new(); gauges.custom_minimum_size = Vector2(0, 40.0 + 30.0 * maxi(1, tele.rates.size() if tele.sim != null else 2)); scopes.add_child(gauges)
	if task_id != "" and not (info.get("modulators", []) as Array).is_empty() and ModDials.available():   # task-driven neuromodulators: read-only gauges
		mod_dials = ModDials.new().setup(func(): return tele, info["modulators"], false); scopes.add_child(mod_dials)
	banner = Kit.label("", 17, Kit.RED, true); banner.visible = false; v.add_child(banner)
	add_child(ov)
	if _pending_banner != "": _banner_raw(_pending_banner); _pending_banner = ""
	_attach()
	if OS.get_environment("SP_LABTAB") != "": tab = OS.get_environment("SP_LABTAB")
	if compact: _apply_tab()
	_file_dlg = FileDialog.new(); _file_dlg.access = FileDialog.ACCESS_FILESYSTEM; _file_dlg.filters = PackedStringArray(["*.json ; Circuit JSON"]); _file_dlg.current_dir = ProjectSettings.globalize_path(Circuits.user_dir())
	add_child(_file_dlg)
	_build_inspector()
	if OS.get_environment("SP_ADV") != "": _adv_open = true
	if OS.get_environment("SP_SEL") != "": _select_env(OS.get_environment("SP_SEL"))
	if OS.get_environment("SP_MENU") != "": _open_menu()
	if OS.get_environment("SP_PARTS") != "": _open_parts()
	if OS.get_environment("SP_LESION") != "": _lesion_env.call_deferred()
	if OS.get_environment("SP_TESTNOW") != "": _test_start.call_deferred()   # dev hook: run TEST x5 at once, print the result, quit

## The circuit the live view runs: the player's, with the lesion set applied in the lesion lab.
func _live_circuit() -> Dictionary: return Lesion.apply(circuit, lesions) if lesion_on and not lesions.is_empty() else circuit

func _exit_tree() -> void:
	if app != null and not circuit.is_empty(): app.lab_keep[task_id] = circuit
	if tele != null and tele.sim != null: tele.sim.stop()

# ---------- setup ----------
## Open `tid` ("" = free run). Starts from the task skeleton (its sensors and motors, no neurons), or the reference circuit with SP_REF=1.
func _start(tid: String) -> void:
	task_id = tid
	if OS.get_environment("SP_TASK") != "": task_id = OS.get_environment("SP_TASK")
	var loaded := false
	if app != null and app.lab_keep.has(task_id) and OS.get_environment("SP_REF") == "" and not app.lab_reference:
		circuit = (app.lab_keep[task_id] as Dictionary).duplicate(true); loaded = true
	if not loaded and OS.get_environment("SP_CIRCUIT") != "":   # dev hook: open this circuit file
		circuit = Circuits.parse(FileAccess.get_file_as_string(OS.get_environment("SP_CIRCUIT"))); loaded = not circuit.is_empty()
	if not loaded and ext_ok and task_id != "" and (OS.get_environment("SP_REF") != "" or app.lab_reference):
		var txt := Circuits.read_reference(_ref_path(task_id))
		var c := Circuits.parse(txt)
		if not c.is_empty(): circuit = c; loaded = true; _mark_reference(true)
	if not loaded:
		var saved := _last_path()
		if FileAccess.file_exists(saved) and OS.get_environment("SP_NOAUTOSAVE") == "": circuit = Circuits.parse(FileAccess.get_file_as_string(saved)); loaded = not circuit.is_empty()
	if not loaded: circuit = _skeleton(task_id)
	Circuits.ensure_pos(circuit)
	_reload(true)
	if task_id == "worm" and OS.get_environment("SP_SPEED") == "": tele.speed = 10.0   # a 120 s episode at 1x is a long wait

## Reference circuits do not count for progress: remember that this circuit is one, until the player edits it.
func _mark_reference(on: bool) -> void:
	var meta: Dictionary = circuit.get("meta", {}); var ed: Dictionary = meta.get("editor", {})
	if on: ed["from_reference"] = true
	else: ed.erase("from_reference")
	meta["editor"] = ed; circuit["meta"] = meta
func _is_reference() -> bool: return bool(circuit.get("meta", {}).get("editor", {}).get("from_reference", false))

func _ref_path(tid: String) -> String: return Campaign.ref_path(tid)

func _last_path() -> String: return Circuits.user_dir().path_join("last_%s.json" % (task_id if task_id != "" else "free"))

func _skeleton(tid: String) -> Dictionary:
	var c := Circuits.empty("My " + tid + " circuit" if tid != "" else "My circuit")
	if ext_ok and tid != "":
		var t = Campaign.make(tid)
		if t != null:
			var kind: String = SENSOR_KIND.get(tid, "poisson")
			for s in t.sensor_channels(): c["inputs"].append({"name": str(s), "kind": kind, "targets": [], "weight": Circuits.input_weight("iaf_psc_alpha", kind), "value": 0.0})
			for m in t.motor_channels(): c["outputs"].append({"name": str(m), "kind": "rate", "neurons": []})
		c["meta"]["task"] = tid
	return c

func _reload(first := false) -> void:
	var keep: Array = tele.watched.duplicate() if tele != null else []
	var was_running := tele.running if tele != null else true; var sp := tele.speed if tele != null else 1.0
	if tele.sim != null: tele.sim.stop()
	var old_mods: Dictionary = tele.mods.duplicate() if tele != null else {}
	tele = Telemetry.new(); tele.running = was_running; tele.speed = sp; tele.mods = old_mods
	tele.lesioned = lesion_on and not lesions.is_empty()   # the live view runs the lesioned circuit: watch the failure
	var ok := tele.setup(_live_circuit(), task_id, ext_ok)   # always makes the task, so the world and the brief show even for a half-built circuit
	if ok and not keep.is_empty() and not first:
		var valid := keep.filter(func(i): return i < tele.n)
		if not valid.is_empty(): tele.set_watched(valid)
	_fail_shown = false; _rec_done = false; _rec_pass = false; _overlay_dismissed = false; _test_result = {}
	_overlay_hide()
	if tele.error != "": _banner(tele.error)
	elif banner != null: banner.visible = false
	if first and OS.get_environment("SP_SPEED") != "": tele.speed = float(OS.get_environment("SP_SPEED"))
	if editor != null: _attach()

func _attach() -> void:
	editor.set_circuit(circuit, tele); task_view.attach(tele); timeline_view.attach(tele); score_card.attach(tele); raster.attach(tele); traces.attach(tele); gauges.attach(tele)
	gauges.custom_minimum_size = Vector2(0, 40.0 + 30.0 * maxi(1, tele.rates.size() if tele.sim != null else maxi(1, circuit.get("outputs", []).size())))
	if run_btn != null: _sync_run_btn()

func _banner(msg: String) -> void: _banner_raw("The engine rejected the circuit: " + msg)

## Show `msg` in the red banner under the lab; before build() has made the label it is kept and shown at the end of build().
func _banner_raw(msg: String) -> void:
	if banner == null: _pending_banner = msg; return
	banner.text = msg; banner.visible = true

func _on_structure() -> void:
	_dirty = 0.0; _lesion_reset()
	if _is_reference(): _mark_reference(false)   # an edited reference is the player's circuit now

func _process(dt: float) -> void:
	if _dirty >= 0.0:
		_dirty += dt
		if _dirty > 0.3 and _lrun == null: _dirty = -1.0; _reload(); _build_inspector()
	if not _test.is_empty(): _test_step()
	elif _lrun != null: _lesion_step()
	elif tele != null: tele.advance(dt)
	_watch_episode()
	if status != null and tele != null:
		var spd := String.num(tele.speed) + "x"
		if tele.sim != null and tele.running and tele.speed > 1.0 and tele.achieved < tele.speed * 0.85: spd = "%.1fx of %sx" % [tele.achieved, String.num(tele.speed)]
		status.text = "t %.2f s    %s spikes    %s" % [tele.t_ms * 0.001, _short(tele.total_spikes), spd] if tele.sim != null else "no circuit running"

static func _short(n: int) -> String: return "%.1fk" % (n / 1000.0) if n >= 10000 else str(n)

# ---------- top bar, tabs, palette ----------
func _topbar(phone: bool, compact := false) -> Control:
	var h := HFlowContainer.new(); h.add_theme_constant_override("h_separation", 8); h.add_theme_constant_override("v_separation", 6)
	var bs := 22 if not phone else 18
	back_btn = Kit.btn("BACK", func(): app.back(), "panel", bs, Vector2(96 if not phone else 70, 0)); h.add_child(back_btn)
	if not phone: var ttl := Kit.shout(_title(), 34 if _title().length() <= 14 else 28, Kit.TEXT); ttl.size_flags_vertical = Control.SIZE_SHRINK_CENTER; h.add_child(ttl)
	if phone:   # one row: back, view tabs, run controls, and a single MENU for everything else
		for t in [["circuit", "CIRCUIT"], ["world", "WORLD"], ["scopes_only", "SCOPES"]]:
			var b := Kit.btn(t[1], func(): tab = t[0]; _apply_tab(), "panel", bs, Vector2(0, 0)); tab_btns[t[0]] = b; h.add_child(b)
	run_btn = Kit.btn("PAUSE", _toggle_run, "amber", bs, Vector2(120 if not phone else 92, 0)); h.add_child(run_btn)
	h.add_child(Kit.btn("RESET", func(): _restart(), "panel", bs, Vector2(92 if not phone else 70, 0)))
	h.add_child(Kit.stepper(SPEEDS, String.num(tele.speed), func(id): tele.speed = float(id), 76.0 if not phone else 60.0, 20 if not phone else 18, 44.0 if not phone else 36.0))
	status = Kit.label("", 17 if not phone else 15, Kit.TEXT_SOFT); status.size_flags_vertical = Control.SIZE_SHRINK_CENTER; status.size_flags_horizontal = Control.SIZE_EXPAND_FILL; status.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT; status.custom_minimum_size.x = 0; status.clip_text = true
	if phone:
		status.visible = false; h.add_child(status)   # shown in the menu on a phone
		h.add_child(Kit.btn("MENU", _toggle_menu, "cyan", bs, Vector2(84, 0))); _sync_run_btn.call_deferred(); return h
	h.add_child(status)
	if task_id != "": h.add_child(Kit.btn("BRIEF", func(): editor.select({}); _brief_forced = true; _build_inspector(), "ghost", bs, Vector2(84, 0)))
	if task_id != "": h.add_child(Kit.btn("REFERENCE", _load_reference, "cyan", bs, Vector2(150, 0)))
	if task_id != "": h.add_child(Kit.btn("TEST x5", _test_start, "green", bs, Vector2(110, 0)))
	if task_id != "": _lesion_btn = Kit.btn("LESIONS", _lesion_toggle, "panel", bs, Vector2(120, 0)); h.add_child(_lesion_btn)
	h.add_child(Kit.btn("LOAD", _load_dialog, "panel", bs, Vector2(84, 0)))
	h.add_child(Kit.btn("SAVE", _save_dialog, "panel", bs, Vector2(84, 0)))
	h.add_child(Kit.btn("ANALYSIS", func(): app.open_analysis(circuit, tele, task_id), "green", bs, Vector2(130, 0)))
	_sync_run_btn.call_deferred()
	return h

func _title() -> String:
	if task_id == "": return "Sandbox"
	return str(Campaign.info(task_id)["title"]) if ext_ok else "Lab"

func _tabs_row() -> Control:
	var h := HBoxContainer.new(); h.add_theme_constant_override("separation", 8)
	for t in [["circuit", "CIRCUIT"], ["world", "WORLD"], ["scopes_only", "SCOPES"]]:
		var b := Kit.btn(t[1], func(): tab = t[0]; _apply_tab(), "panel", 20, Vector2(0, 0)); b.size_flags_horizontal = Control.SIZE_EXPAND_FILL; tab_btns[t[0]] = b; h.add_child(b)
	return h

func _apply_tab() -> void:
	for k in ["circuit", "world", "scopes_only"]:
		var p: Control = panels[k]; p.visible = (k == tab)
		if tab_btns.has(k): Kit.skin(tab_btns[k], "amber" if k == tab else "panel", 12)
	panels["scopes"].visible = tab != "circuit"; panels["circuit"].visible = tab == "circuit"
	_sheet_sync()

func _palette(phone: bool) -> Control:
	var h := HFlowContainer.new(); h.add_theme_constant_override("h_separation", 8); h.add_theme_constant_override("v_separation", 6)
	var sz := 18
	_palette_row_parent_hint = h
	if not phone: _type_pickers(h)   # on a phone the parts button lives in the MENU
	h.add_child(Kit.btn("+ EXCITATORY", func(): _add_neuron("E"), "amber", sz))
	h.add_child(Kit.btn("+ INHIBITORY", func(): _add_neuron("I"), "cyan", sz))
	h.add_child(Kit.btn("+ NOISE", _add_noise, "panel", sz))
	_connect_btn = Kit.btn("CONNECT", _toggle_connect, "panel", sz); h.add_child(_connect_btn)
	h.add_child(Kit.btn("FIT", func(): editor.fit(), "ghost", sz))
	return h

## The parts bin button: shows the model the next neuron gets and opens the catalogue cards.
func _type_pickers(parent: Control) -> void:
	var b := Kit.btn(_part_text(), _open_parts, "cyan", 18); parent.add_child(b)
	if parent == _palette_row_parent_hint: _parts_btn = b

var _palette_row_parent_hint: Control = null
func _part_text() -> String: return "PARTS: " + part_card

func _open_parts() -> void:
	_close_menu(); _close_parts()
	var root := Control.new(); root.set_anchors_preset(Control.PRESET_FULL_RECT); root.mouse_filter = Control.MOUSE_FILTER_STOP; ov.add_child(root); _parts_ov = root
	var dim := ColorRect.new(); dim.color = Color(Kit.BENCH, 0.78); dim.set_anchors_preset(Control.PRESET_FULL_RECT); root.add_child(dim)
	var phone := Layout.phone
	var pn := PanelContainer.new(); pn.add_theme_stylebox_override("panel", Kit.sb(Kit.PANEL, Kit.LINE, 1, 16, 14 if phone else 20, true)); root.add_child(pn)
	pn.set_anchors_preset(Control.PRESET_FULL_RECT); var mg := 16 if phone else int(48 * f()); pn.offset_left = mg; pn.offset_right = -mg; pn.offset_top = mg * 0.6; pn.offset_bottom = -mg * 0.6
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", 8); pn.add_child(v)
	var hd := HBoxContainer.new(); hd.add_theme_constant_override("separation", 12); v.add_child(hd)
	var t := Kit.shout("Parts bin", 34 if not phone else 24, Kit.TEXT); t.size_flags_horizontal = Control.SIZE_EXPAND_FILL; hd.add_child(t)
	hd.add_child(Kit.btn("CLOSE", _close_parts, "panel", 20 if not phone else 18))
	var bin := PartsBin.new().setup("pick", Circuits.sandbox, part_card); v.add_child(bin)
	bin.picked.connect(func(id): part_card = id; _close_parts(); _sync_parts_btn())

func _close_parts() -> void:
	if _parts_ov != null and is_instance_valid(_parts_ov): _parts_ov.queue_free()
	_parts_ov = null

func _sync_parts_btn() -> void:
	if _parts_btn != null and is_instance_valid(_parts_btn): _parts_btn.text = _part_text()

func _toggle_connect() -> void:
	editor.tool = "connect" if editor.tool == "select" else "select"; editor._connect_from = {}
	Kit.skin(_connect_btn, "amber" if editor.tool == "connect" else "panel", 12); editor.queue_redraw()

func _add_neuron(kind: String) -> void:
	var card := Catalog.card(part_card)
	var id := Circuits.add_card(circuit, card, editor.free_spot(), kind) if not card.is_empty() else Circuits.add_neuron(circuit, "iaf_psc_alpha", 0, editor.free_spot(), kind)
	editor.select({"t": "pop", "id": id}); editor.fit(); _on_structure()

func _add_noise() -> void:
	var name := Circuits.add_noise(circuit)
	editor.select({"t": "in", "name": name}); editor.fit(); _on_structure(); _build_inspector()

# ---------- run controls ----------
func _toggle_run() -> void: tele.running = not tele.running; _sync_run_btn()
func _sync_run_btn() -> void:
	if run_btn != null: run_btn.text = "PAUSE" if tele.running else "RUN"
func _restart() -> void:
	_overlay_hide(); _overlay_dismissed = false; _rec_done = false; _rec_pass = false; _fail_shown = false
	if tele.sim != null: tele.reset(); tele.running = true; _sync_run_btn()
	elif tele.task != null: tele.task.reset(int(circuit.get("seed", 1)))
	task_view.attach(tele)

func _load_reference() -> void:
	var txt := Circuits.read_reference(_ref_path(task_id))
	var c := Circuits.parse(txt)
	if c.is_empty(): _banner("the reference circuit for this task is not installed (circuits/%s)" % _ref_path(task_id)); return
	circuit = c; Circuits.ensure_pos(circuit); _mark_reference(true); editor.select({}); _lesion_reset(); _reload(true); _build_inspector()

func _save_dialog() -> void:
	for cn in _file_dlg.file_selected.get_connections(): _file_dlg.file_selected.disconnect(cn["callable"])
	_file_dlg.file_mode = FileDialog.FILE_MODE_SAVE_FILE; _file_dlg.title = "Save circuit"
	_file_dlg.current_file = str(circuit.get("name", "circuit")).to_lower().replace(" ", "_") + ".json"
	_file_dlg.file_selected.connect(func(p): var fh := FileAccess.open(p, FileAccess.WRITE); if fh != null: fh.store_string(Circuits.to_json(circuit)))
	_popup_file()

func _load_dialog() -> void:
	for cn in _file_dlg.file_selected.get_connections(): _file_dlg.file_selected.disconnect(cn["callable"])
	_file_dlg.file_mode = FileDialog.FILE_MODE_OPEN_FILE; _file_dlg.title = "Load circuit"
	_file_dlg.file_selected.connect(func(p):
		var c := Circuits.parse(FileAccess.get_file_as_string(p))
		if c.is_empty(): _banner("not a circuit JSON file: " + p); return
		circuit = c; Circuits.ensure_pos(circuit); editor.select({}); _lesion_reset(); _reload(true); _build_inspector())
	_popup_file()

func _popup_file() -> void: _file_dlg.popup_centered_clamped(Vector2i(900, 560), 0.9)

## Persist the player's circuit per task (user://circuits/last_<task>.json) after each edit.
func _autosave() -> void:
	if OS.get_environment("SP_NOAUTOSAVE") != "" or Settings.no_save: return
	var fh := FileAccess.open(_last_path(), FileAccess.WRITE)
	if fh != null: fh.store_string(Circuits.to_json(circuit))

# ---------- phone menu ----------
func _toggle_menu() -> void:
	if _menu != null: _close_menu()
	else: _open_menu()

func _close_menu() -> void:
	if _menu != null: _menu.queue_free(); _menu = null

## The phone's single menu: everything the desktop top bar and palette hold besides the run controls and the add-neuron buttons.
func _open_menu() -> void:
	_close_menu()
	var scrim := Button.new(); scrim.flat = true; scrim.set_anchors_preset(Control.PRESET_FULL_RECT); scrim.focus_mode = Control.FOCUS_NONE
	scrim.pressed.connect(_close_menu); ov.add_child(scrim); _menu = scrim
	var pn := PanelContainer.new(); pn.add_theme_stylebox_override("panel", Kit.sb(Kit.PANEL, Kit.LINE, 1, 12, 12, false)); scrim.add_child(pn)
	pn.set_anchors_preset(Control.PRESET_TOP_RIGHT); pn.grow_horizontal = Control.GROW_DIRECTION_BEGIN; pn.offset_right = -14.0; pn.offset_top = 66.0
	var sc := ScrollContainer.new(); sc.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED; sc.custom_minimum_size = Vector2(430, 120); pn.add_child(sc)
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", 8); v.size_flags_horizontal = Control.SIZE_EXPAND_FILL; sc.add_child(v)
	var st := Kit.label(status.text if status != null else "", 16, Kit.TEXT_SOFT); v.add_child(st)
	v.add_child(Kit.label("NEW NEURON TYPE", 15, Kit.TEXT_SOFT)); _type_pickers(v)
	var row := HFlowContainer.new(); row.add_theme_constant_override("h_separation", 8); row.add_theme_constant_override("v_separation", 8); v.add_child(row)
	var item := func(t: String, cb: Callable, kind := "panel"): row.add_child(Kit.btn(t, func(): _close_menu(); cb.call(), kind, 18))
	if task_id != "":
		item.call("BRIEF", func(): editor.select({}); _brief_forced = true; _build_inspector(), "ghost")
		item.call("REFERENCE", _load_reference, "cyan")
		item.call("TEST x5", _test_start, "green")
		item.call("LESIONS" if not lesion_on else "LEAVE LESIONS", _lesion_toggle, "amber" if lesion_on else "panel")
	item.call("LOAD", _load_dialog); item.call("SAVE", _save_dialog)
	item.call("ANALYSIS", func(): app.open_analysis(circuit, tele, task_id), "green")
	_size_menu.call_deferred(sc, v)

func _size_menu(sc: ScrollContainer, v: Control) -> void:
	await get_tree().process_frame
	if is_instance_valid(sc) and is_instance_valid(v): sc.custom_minimum_size.y = minf(v.get_combined_minimum_size().y, size.y - 90.0)

# ---------- progress, end-of-episode card, 5-seed test ----------
func _stars_now() -> int:
	if tele == null or tele.task == null: return 0
	return Progress.stars_for(task_id, bool(tele.score.get("passed", false)), false, Circuits.neuron_count(circuit))

## Called every frame: record a pass the moment it happens and the final result at the end of the episode; raise the end card; report a stopped run.
func _watch_episode() -> void:
	if tele == null: return
	if tele.failed and not _fail_shown: _fail_shown = true; _banner_raw("The simulation stopped: " + tele.error + ". Fix the circuit and press RESET.")
	if tele.task == null or tele.sim == null or tele.failed or not _test.is_empty() or lesion_on: return
	var passed := bool(tele.score.get("passed", false))
	if passed and not _rec_pass:
		_rec_pass = true
		if not _is_reference(): Progress.record(task_id, float(tele.score.get("value", 0.0)), true, 1)
	if tele.finished and not _rec_done:
		_rec_done = true
		if not _is_reference(): Progress.record(task_id, float(tele.score.get("value", 0.0)), passed, 1 if passed else 0)
		if not _overlay_dismissed: _overlay_show()

func _overlay_hide() -> void:
	if _overlay != null and is_instance_valid(_overlay): _overlay.queue_free()
	_overlay = null

func _ov_passed() -> bool:
	if not _test_result.is_empty(): return int(_test_result["passes"]) == int(_test_result["n"])
	return bool(tele.score.get("passed", false))

func _end_text() -> String:
	var sc: Dictionary = tele.score; var passed := _ov_passed(); var parts := []
	if not _test_result.is_empty():
		parts.append("Test: %d of %d seeds passed." % [int(_test_result["passes"]), int(_test_result["n"])])
	if _is_reference():
		parts.append("This is the reference circuit. Build your own from an empty bench to earn stars.")
	elif not passed:
		if _test_result.is_empty(): parts.append(str(sc.get("verdict", "")))
		parts.append("Try: open the motor neuron's inspector and raise the weights into it, or add a neuron between sensor and motor.")
	else:
		var st := Progress.best(task_id)["stars"] as int
		var nn := Circuits.neuron_count(circuit); var bud := Progress.budget(task_id)
		if _test_result.is_empty() and st < 2: parts.append("Next star: press TEST x5. It runs 5 other random worlds; all must pass.")
		elif st == 2 and nn > bud: parts.append("Third star: pass again with %d neurons or fewer (you have %d)." % [bud, nn])
		elif st >= 3: parts.append("Three stars. Try the next task, or open ANALYSIS to see which motifs your circuit uses.")
		elif st < 3 and _test_result.has("passes") and int(_test_result["passes"]) < int(_test_result["n"]): parts.append("It passes the fixed world but not every seed: make it less fragile, then TEST again.")
	return "  ".join(parts)

## The card that rises when an episode ends (or a TEST runs): score, stars, best, what to try next, and the buttons that matter.
func _overlay_show(testing := false) -> void:
	_overlay_hide()
	var phone := Layout.phone
	var root := Control.new(); root.set_anchors_preset(Control.PRESET_FULL_RECT); root.mouse_filter = Control.MOUSE_FILTER_STOP; ov.add_child(root); _overlay = root
	var dim := ColorRect.new(); dim.color = Color(Kit.BENCH, 0.62); dim.set_anchors_preset(Control.PRESET_FULL_RECT); dim.mouse_filter = Control.MOUSE_FILTER_IGNORE; root.add_child(dim)
	var cc := CenterContainer.new(); cc.set_anchors_preset(Control.PRESET_FULL_RECT); cc.mouse_filter = Control.MOUSE_FILTER_IGNORE; root.add_child(cc)
	var pn := PanelContainer.new(); pn.add_theme_stylebox_override("panel", Kit.sb(Kit.PANEL, Kit.LINE, 1, 16, 14 if phone else 24, true)); cc.add_child(pn)
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", 6 if phone else 12); v.custom_minimum_size.x = 560 if not phone else 600; pn.add_child(v)
	var passed := _ov_passed(); var val := int(roundf(float(tele.score.get("value", 0.0))))
	if not _test_result.is_empty():
		var tot := 0.0
		for x in _test_result["scores"]: tot += float(x)
		val = int(roundf(tot / maxf(float((_test_result["scores"] as Array).size()), 1.0)))
	if testing:
		var i := int(_test.get("i", 0))
		v.add_child(Kit.shout("TESTING  %d of %d" % [mini(i + 1, TEST_SEEDS.size()), TEST_SEEDS.size()], 34 if not phone else 26, Kit.TEXT))
		var pl := Kit.label("", 20 if not phone else 17, Kit.TEXT_SOFT, true); pl.name = "TestProg"; v.add_child(pl)
		v.add_child(Kit.btn("CANCEL", _test_cancel, "panel", 20))
		return
	var hd := HBoxContainer.new(); hd.add_theme_constant_override("separation", 18); v.add_child(hd)
	var col := Kit.GREEN if passed else Kit.RED
	var tl := Kit.shout(("PASSED" if passed else "NOT PASSED") if _test_result.is_empty() else "%d OF %d SEEDS PASSED" % [int(_test_result["passes"]), int(_test_result["n"])], 40 if not phone else 28, col); tl.size_flags_horizontal = Control.SIZE_EXPAND_FILL; hd.add_child(tl)
	hd.add_child(Kit.shout(str(val), 54 if not phone else 36, Kit.TEXT))
	var sr := HBoxContainer.new(); sr.add_theme_constant_override("separation", 14); v.add_child(sr)
	var best := Progress.best(task_id); var ev := _OvStars.new(); ev.n = int(best["stars"]) if not _is_reference() else 0; ev.custom_minimum_size = Vector2(110, 30); sr.add_child(ev)
	var bl := Kit.label("best %d" % int(roundf(float(best["value"]))) if not _is_reference() else "reference run: not recorded", 18 if not phone else 16, Kit.TEXT_SOFT); bl.size_flags_vertical = Control.SIZE_SHRINK_CENTER; sr.add_child(bl)
	v.add_child(Kit.label(_end_text(), 19 if not phone else 16, Kit.TEXT, true))
	if tele.sim != null:   # the energy bill of this run
		var hz := tele.spike_rate_hz()
		v.add_child(Kit.label("Energy: %s spikes/s, about %s (roughly 1e8 to 1e9 ATP per spike)." % [str(int(roundf(hz))), Telemetry.atp_text(hz)], 16 if not phone else 15, Kit.exc(), true))
	var br := HFlowContainer.new(); br.add_theme_constant_override("h_separation", 10); br.add_theme_constant_override("v_separation", 8); v.add_child(br)
	br.add_child(Kit.btn("RUN AGAIN", _restart, "amber", 20))
	br.add_child(Kit.btn("TEST x5", _test_start, "green", 20))
	var nx := Progress.next_task(task_id)
	if passed and nx != "": br.add_child(Kit.btn("NEXT TASK", func(): app.open_lab(nx), "cyan", 20))
	if _lesion_ok(): br.add_child(Kit.btn("LESION LAB", _open_lesions, "panel", 20))
	br.add_child(Kit.btn("ANALYSIS", func(): app.open_analysis(circuit, tele, task_id), "panel", 20))
	br.add_child(Kit.btn("CLOSE", func(): _overlay_dismissed = true; _overlay_hide(), "ghost", 20))

class _OvStars extends Control:
	var n := 0
	func _draw() -> void:
		for k in 3: ScoreCard.draw_star_on(self, Vector2(14.0 + 34.0 * k, size.y * 0.5), 13.0, Kit.AMBER if k < n else Color(Kit.LINE, 0.95))

## TEST x5: five headless episodes of the task on five other seeds, run a slice of each frame. Pass on all five earns the second star.
func _test_start() -> void:
	if task_id == "" or tele.task == null or Circuits.neuron_count(circuit) == 0 or not _test.is_empty(): return
	if tele.wiring_issue() != "": _banner_raw(tele.wiring_issue()); return
	_close_menu(); _overlay_dismissed = false
	if tele.sim != null: tele.sim.stop()
	var sim = ClassDB.instantiate("SpikeSim"); var task = Campaign.make(task_id)
	_test = {"i": 0, "res": [], "sim": sim, "task": task, "t": -1.0}
	tele.running = false
	_overlay_show(true)

func _test_cancel() -> void:
	_test = {}; _overlay_hide(); _reload(); _build_inspector()

func _test_step() -> void:
	var sim = _test["sim"]; var task = _test["task"]
	if float(_test["t"]) < 0.0:   # begin episode i
		if not sim.load_circuit(Circuits.to_json(Telemetry.sanitized(circuit))): _test = {}; _overlay_hide(); _reload(); _banner(str(sim.error())); return
		task.reset(int(TEST_SEEDS[int(_test["i"])])); _test["t"] = 0.0
	var t0 := Time.get_ticks_usec(); var dur := float(task.duration_ms())
	while float(_test["t"]) < dur and Time.get_ticks_usec() - t0 < 12000:
		task.step(sim, 1.0); _test["t"] = float(_test["t"]) + 1.0
		if not bool(task.step_ok(sim)):
			_test = {}; _overlay_hide(); _reload(); _banner_raw("The test stopped: " + str(sim.error())); return
		if int(_test["t"]) % 200 == 0: sim.drain_spikes()
	if _overlay != null:
		var pl := _overlay.find_child("TestProg", true, false) as Label
		if pl != null:
			pl.text = "Seed %d: %d%% of the episode simulated." % [int(TEST_SEEDS[int(_test["i"])]), int(100.0 * float(_test["t"]) / maxf(dur, 1.0))]
			var done: Array = _test["res"]
			if not done.is_empty(): pl.text += "  Finished: " + ", ".join(done.map(func(r): return "%d%s" % [int(r["value"]), "" if r["passed"] else " (miss)"]))
	if float(_test["t"]) < dur: return
	var sc: Dictionary = task.score(); (_test["res"] as Array).append({"value": float(sc.get("value", 0.0)), "passed": bool(sc.get("passed", false))})
	_test["i"] = int(_test["i"]) + 1; _test["t"] = -1.0
	if int(_test["i"]) < TEST_SEEDS.size(): return
	var res: Array = _test["res"]; var passes := 0
	for r in res: passes += 1 if r["passed"] else 0
	sim.stop(); _test = {}
	_reload()   # the live view again (the engine is a process-wide singleton and the test used it)
	_test_result = {"passes": passes, "n": res.size(), "scores": res.map(func(r): return r["value"])}
	if not _is_reference() and passes > 0:
		Progress.record(task_id, float(res.map(func(r): return r["value"]).max()), true, Progress.stars_for(task_id, true, passes == res.size(), Circuits.neuron_count(circuit)) if passes == res.size() else 1)
	_rec_done = true; _overlay_show(); _build_inspector()
	if OS.get_environment("SP_TESTNOW") != "" and OS.get_environment("SP_SHOT") == "": print("TEST RESULT ", _test_result, " best ", Progress.best(task_id)); get_tree().quit()

# ---------- inspector ----------
func _select_env(s: String) -> void:   # SP_SEL=pop:L | proj:0 | in:drive | out:left_motor
	var p := s.split(":"); if p.size() < 2: return
	match p[0]:
		"pop": editor.select({"t": "pop", "id": p[1]})
		"proj": editor.select({"t": "proj", "i": int(p[1])})
		"in", "out": editor.select({"t": p[0], "name": p[1]})

func _build_inspector() -> void:
	if insp_body == null: return
	if not editor.sel.is_empty(): _brief_forced = false
	for c in insp_body.get_children(): insp_body.remove_child(c); c.queue_free()
	_autosave()
	var sel := editor.sel if editor != null else {}
	var small := 17 if not Layout.phone else 16
	if lesion_on: _inspect_lesion(small)
	elif sel.is_empty(): _inspect_brief(small)
	else:
		match sel["t"]:
			"pop": _inspect_pop(str(sel["id"]), small)
			"proj": _inspect_proj(int(sel["i"]), small)
			"in": _inspect_channel("inputs", str(sel["name"]), small)
			"out": _inspect_channel("outputs", str(sel["name"]), small)
	_sheet_sync()
	_fit_inspector.call_deferred()

## On a phone the inspector is a bottom sheet that shows only while something is selected (or the brief is asked for) and only on the circuit tab.
func _sheet_sync() -> void:
	if inspector == null or editor == null: return
	if not Layout.phone:
		inspector.visible = true
		if _palette_row != null: _palette_row.visible = not lesion_on
		return
	var show := (not editor.sel.is_empty() or _brief_forced or lesion_on) and tab == "circuit"
	inspector.visible = show
	if _palette_row != null: _palette_row.visible = not show   # the sheet needs the room
	if not show and editor.inset_bottom != 0.0: editor.inset_bottom = 0.0; editor.fit()

## Size the inspector's scroll area to its content (capped), and keep the phone editor's neurons above the sheet.
func _fit_inspector() -> void:
	await get_tree().process_frame
	if not is_inside_tree() or insp_scroll == null or insp_body == null: return
	var want := insp_body.get_combined_minimum_size().y
	var cap := size.y * ((0.62 if lesion_on else 0.46) if Layout.phone else (0.6 if lesion_on else 0.36))
	insp_scroll.custom_minimum_size.y = minf(want, cap)
	if Layout.phone:
		await get_tree().process_frame
		if not is_inside_tree() or editor == null: return
		var inset: float = (inspector.size.y + 12.0) if inspector.visible else 0.0
		if absf(inset - editor.inset_bottom) > 12.0: editor.inset_bottom = inset; editor.fit()

func _head(title: String, kind_col: Color, tag := "") -> HBoxContainer:
	var h := HBoxContainer.new(); h.add_theme_constant_override("separation", 10)
	h.add_child(Kit.chip(title, kind_col, 18))
	var l := Kit.label(tag, 17, Kit.TEXT_SOFT); l.size_flags_horizontal = Control.SIZE_EXPAND_FILL; l.clip_text = true; h.add_child(l)
	if Layout.phone: h.add_child(Kit.btn("DONE", func(): _brief_forced = false; editor.select({}); (_lesion_toggle() if lesion_on else _build_inspector()), "panel", 18))
	return h

func _inspect_brief(small: int) -> void:
	var t: String = ""
	if task_id != "" and ext_ok:
		t = str(Campaign.info(task_id)["brief"])
	insp_body.add_child(_head("BRIEF" if task_id != "" else "FREE RUN", Kit.exc(), _title() if task_id != "" else "build any circuit and watch it run"))
	var txt := t if t != "" else "Add neurons, connect them by dragging from one disc's rim to another, and watch the raster and the voltage traces."
	var l := Kit.label(txt, small, Kit.TEXT_SOFT, true); insp_body.add_child(l)
	if task_id != "" and Progress.budget(task_id) > 0: insp_body.add_child(Kit.label("Stars: 1 pass.  2 pass on 5 seeds (TEST x5).  3 the same with %d neurons or fewer." % Progress.budget(task_id), small, Kit.TEXT_SOFT, true))
	var leg := HBoxContainer.new(); leg.add_theme_constant_override("separation", 8)
	leg.add_child(Kit.chip("EXCITATORY", Kit.exc(), 14)); leg.add_child(Kit.chip("INHIBITORY", Kit.inh(), 14)); leg.add_child(Kit.chip("SENSOR", Kit.GREEN, 14)); leg.add_child(Kit.chip("MOTOR", Gfx.VIOLET, 14))
	insp_body.add_child(leg)

func _val_row(name: String, lo: float, hi: float, step: float, val: float, unit: String, cb: Callable) -> Control:
	var h := HBoxContainer.new(); h.add_theme_constant_override("separation", 10); h.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var nm := Kit.label(name, 17, Kit.TEXT_SOFT); nm.custom_minimum_size.x = 100; h.add_child(nm)
	var s := HSlider.new(); s.min_value = lo; s.max_value = hi; s.step = step; s.value = val; s.size_flags_horizontal = Control.SIZE_EXPAND_FILL; s.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	s.custom_minimum_size = Vector2(120, maxf(30.0, Layout.tap())); s.focus_mode = Kit.focus_mode(); h.add_child(s)
	var vl := Kit.label(_num(val) + " " + unit, 17, Kit.TEXT); vl.custom_minimum_size.x = 96; vl.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT; h.add_child(vl)
	s.value_changed.connect(func(x): vl.text = _num(x) + " " + unit; cb.call(x))
	return h

static func _num(x: float) -> String: return "%d" % int(roundf(x)) if absf(x) >= 100.0 or is_equal_approx(x, roundf(x)) else "%.1f" % x

func _inspect_pop(id: String, small: int) -> void:
	var pi := Circuits.pop_index(circuit, id); if pi < 0: _inspect_brief(small); return
	var p: Dictionary = circuit["populations"][pi]; var kd := Circuits.kinds(circuit)
	var kcol := Kit.inh() if kd[id] == "I" else Kit.exc()
	insp_body.add_child(_head(id, kcol, "%s  %s" % ["inhibitory" if kd[id] == "I" else "excitatory", str(p.get("model", ""))]))
	var row := HFlowContainer.new(); row.add_theme_constant_override("h_separation", 10); row.add_theme_constant_override("v_separation", 6); insp_body.add_child(row)
	var model := str(p.get("model", "")); var presets: Array = Circuits.presets_of(model)
	var models := Circuits.model_options(model)
	row.add_child(Kit.stepper(models, model, func(m): Circuits.set_model(circuit, id, str(m), 0); _on_structure(); _build_inspector(), 230.0 if not Layout.phone else 170.0, 18, 40.0))
	var po := []
	for i in presets.size(): po.append([str(i), presets[i][0]])
	var cur := Circuits.preset_of(p)
	if not po.is_empty(): row.add_child(Kit.stepper(po, str(maxi(cur, 0)), func(i): Circuits.set_model(circuit, id, model, int(i)); _on_structure(); _build_inspector(), 190.0 if not Layout.phone else 140.0, 18, 40.0))
	row.add_child(Kit.btn("TRACE", func(): _toggle_trace(id), "cyan" if tele.watched.has(Circuits.pop_first(circuit, pi)) else "panel", 18))
	row.add_child(Kit.btn("E / I", func(): _flip_kind(id), "panel", 18))
	row.add_child(Kit.btn("DELETE", func(): editor.delete_selected(), "danger", 18))
	var nrow := HBoxContainer.new(); nrow.add_theme_constant_override("separation", 10)
	nrow.add_child(Kit.label("neurons in group", small, Kit.TEXT_SOFT)); nrow.add_child(Kit.stepper([[1, "1"], [2, "2"], [3, "3"], [4, "4"], [6, "6"], [8, "8"], [10, "10"], [12, "12"], [20, "20"]], str(Circuits.pop_n(p)), func(v): Circuits.set_n(circuit, id, int(v)); _on_structure(), 70.0, 18, 40.0))
	insp_body.add_child(nrow)
	var info: String = str(presets[cur][2]) if cur >= 0 else "Custom parameters."
	insp_body.add_child(Kit.label(info + "  Drag the disc to move it, drag from its rim to connect.", small, Kit.TEXT_SOFT, true))
	insp_body.add_child(Kit.btn("HIDE ADVANCED" if _adv_open else "ADVANCED", func(): _adv_open = not _adv_open; _build_inspector(), "ghost", 18))
	var adv := Circuits.advanced_of(model)
	if _adv_open and not adv.is_empty():
		var prm: Dictionary = p.get("params", {})
		for a in adv:
			var key: String = a[0]
			insp_body.add_child(_val_row(str(a[1]), float(a[2]), float(a[3]), float(a[4]), float(prm.get(key, a[5])), str(a[6]), func(x): var pp: Dictionary = p.get("params", {}); pp[key] = x; p["params"] = pp; _on_structure()))
		insp_body.add_child(Kit.btn("RESET PARAMETERS", func(): Circuits.set_model(circuit, id, model, 0); _on_structure(); _build_inspector(), "panel", 18))

func _toggle_trace(id: String) -> void:
	var pi := Circuits.pop_index(circuit, id); var g := Circuits.pop_first(circuit, pi); var w: Array = tele.watched.duplicate()
	if w.has(g): w.erase(g)
	else:
		w.append(g)
		if w.size() > 4: w.pop_front()
	if tele.sim != null: tele.set_watched(w)
	_build_inspector()

func _flip_kind(id: String) -> void:
	var flipped := false
	var kd := Circuits.kinds(circuit); var want := "E" if kd[id] == "I" else "I"
	for pr in circuit["projections"]:
		if Circuits.proj_pop(pr.get("pre", "")) == id:
			Circuits.set_proj_weight(pr, -Circuits.proj_weight(pr)); flipped = true
	var ed: Dictionary = circuit["meta"].get("editor", {}); var ei: Dictionary = ed.get("ei", {}); ei[id] = want; ed["ei"] = ei; circuit["meta"]["editor"] = ed
	_on_structure(); _build_inspector()

func _inspect_proj(i: int, small: int) -> void:
	if i >= circuit["projections"].size(): _inspect_brief(small); return
	var pr: Dictionary = circuit["projections"][i]; var w := Circuits.proj_weight(pr)
	var pre := Circuits.proj_pop(pr.get("pre", "")); var post := Circuits.proj_pop(pr.get("post", ""))
	insp_body.add_child(_head("%s > %s" % [pre, post], Kit.inh() if w < 0.0 else Kit.exc(), "%s synapse   %s" % ["inhibitory" if w < 0.0 else "excitatory", str(pr.get("rule", "explicit edges"))]))
	var pi := Circuits.pop_index(circuit, post); var wmax: float = 4.0 * Circuits.weight_of(str(circuit["populations"][pi].get("model", ""))) if pi >= 0 else 400.0
	var unit := "nS" if pi >= 0 and str(circuit["populations"][pi].get("model", "")).contains("cond") else "pA"
	var r1 := HFlowContainer.new(); r1.add_theme_constant_override("h_separation", 10); insp_body.add_child(r1)
	var cur_syn := Circuits.proj_model(pr)
	if cur_syn != "static_synapse": r1.add_child(Kit.stepper([["static_synapse", "static"], [cur_syn, cur_syn.replace("_synapse", "")]], cur_syn, func(m): var s: Dictionary = pr.get("synapse", {}); s["synapse_model"] = str(m); pr["synapse"] = s; _on_structure(), 190.0 if not Layout.phone else 150.0, 18, 40.0))
	r1.add_child(Kit.btn("E / I", func(): Circuits.set_proj_weight(pr, -Circuits.proj_weight(pr)); _on_structure(); _build_inspector(), "panel", 18))
	r1.add_child(Kit.btn("DELETE", func(): editor.delete_selected(), "danger", 18))
	insp_body.add_child(_val_row("weight", 0.0, wmax, wmax / 200.0, absf(w), unit, func(x): Circuits.set_proj_weight(pr, x * (-1.0 if Circuits.proj_weight(pr) < 0.0 else 1.0)); _on_structure()))
	insp_body.add_child(_val_row("delay", 0.1, 20.0, 0.1, Circuits.proj_delay(pr), "ms", func(x): Circuits.set_proj_delay(pr, x); _on_structure()))


func _inspect_channel(key: String, name: String, small: int) -> void:
	var ch := {}
	for c in circuit.get(key, []):
		if str(c.get("name", "")) == name: ch = c
	if ch.is_empty(): _inspect_brief(small); return
	var is_in := key == "inputs"; var driven := false
	if tele.task != null and is_in: driven = (tele.task.sensor_channels() as PackedStringArray).has(name)
	insp_body.add_child(_head(name, Kit.GREEN if is_in else Gfx.VIOLET, ("sensor, driven by the task" if driven else "input device") + "  " + str(ch.get("kind", "")) if is_in else "motor output: mean rate of the connected neurons"))
	var tg: Array = ch.get("targets" if is_in else "neurons", [])
	var names := []
	for x in tg: names.append(Circuits.proj_pop(x))
	var l := Kit.label(("Connected to: " + ", ".join(names)) if not names.is_empty() else ("Not connected yet: drag from the card's right edge onto a neuron." if is_in else "Not connected yet: drag from a neuron's rim onto this card."), small, Kit.TEXT_SOFT, true); insp_body.add_child(l)
	if is_in:
		var kind := str(ch.get("kind", "dc")); var unit := "pA" if kind == "dc" else "Hz"
		var wunit := "pA"; var wmax := 1500.0
		if not tg.is_empty():
			var tpi := Circuits.pop_index(circuit, Circuits.proj_pop(tg[0]))
			if tpi >= 0:
				var tm := str(circuit["populations"][tpi].get("model", ""))
				wunit = "nS" if tm.contains("cond") else "pA"; wmax = 8.0 * Circuits.weight_of(tm)
		insp_body.add_child(_val_row("weight", 0.0, wmax, wmax / 300.0, float(ch.get("weight", 1.0)), wunit, func(x): ch["weight"] = x; _on_structure()))
		if not driven: insp_body.add_child(_val_row("rate" if kind != "dc" else "value", 0.0, 1000.0 if kind == "dc" else 400.0, 1.0, float(ch.get("value", 0.0)), unit, func(x): ch["value"] = x; _on_structure()))
		if not driven: insp_body.add_child(Kit.btn("REMOVE INPUT", func(): editor.delete_selected(), "danger", 18))
	insp_body.add_child(Kit.btn("DISCONNECT ALL", func(): ch["targets" if is_in else "neurons"] = []; _on_structure(); _build_inspector(), "panel", 18))


# ---------- lesion lab ----------
## The lab opens after a pass: this circuit passed (now or earlier this session), it is the reference circuit, or the task has been passed before.
func _lesion_ok() -> bool:
	if task_id == "" or tele == null or tele.task == null: return false
	if lesion_on or _is_reference() or _rec_pass: return true
	return bool(tele.score.get("passed", false)) or bool(Progress.best(task_id)["passed"])

func _lesion_toggle() -> void:
	if lesion_on:
		lesion_on = false; _lrun = null; _lesion_apply_ui(); _dirty = 0.0; _build_inspector(); return
	if not _lesion_ok():
		_banner_raw("The lesion lab opens after a pass: pass the task (or press REFERENCE), then cut and silence parts to see what each one does.")
		return
	if banner != null: banner.visible = false
	_close_menu(); _overlay_hide(); _overlay_dismissed = true
	lesion_on = true; editor.select({}); editor.tool = "select"; editor._connect_from = {}
	if _connect_btn != null: Kit.skin(_connect_btn, "panel", 12)
	_lesion_apply_ui(); _build_inspector()

## Dev hook: SP_LESION=1|run|sweep opens the lesion lab (the reference circuit counts as passed), SP_LESIONS="pop:E_L,proj:3" sets lesions, run / sweep fill the table.
func _lesion_env() -> void:
	await get_tree().create_timer(0.5).timeout
	if not lesion_on: _lesion_toggle()
	for k in OS.get_environment("SP_LESIONS").split(","):
		if k != "": lesions[k] = true
	editor.lesions = lesions; _dirty = 0.0; _build_inspector()
	await get_tree().create_timer(0.5).timeout
	match OS.get_environment("SP_LESION"):
		"run": _lesion_run([lesions.duplicate()])
		"sweep": _lesion_sweep()

func _open_lesions() -> void:
	_overlay_dismissed = true; _overlay_hide()
	if not lesion_on: _lesion_toggle()

func _lesion_apply_ui() -> void:
	editor.lesion_mode = lesion_on; editor.editable = not lesion_on; editor.lesions = lesions; editor.queue_redraw()
	if _lesion_btn != null:
		Kit.skin(_lesion_btn, "amber" if lesion_on else "panel", 13)
	if _palette_row != null and not Layout.phone: _palette_row.visible = not lesion_on

## The circuit changed or was replaced: the table no longer describes it.
func _lesion_reset() -> void:
	lesions = {}; lesion_rows = []; _lesion_note = ""
	if editor != null: editor.lesions = lesions

func _lesion_click(h: Dictionary) -> void:
	if _lrun != null: return
	var key := ""
	match str(h.get("t", "")):
		"pop": key = "pop:" + str(h["id"])
		"proj": key = "proj:%d" % int(h["i"])
		"in":
			for ch in circuit.get("inputs", []):
				if str(ch.get("name", "")) == str(h["name"]) and not (ch.get("targets", []) as Array).is_empty(): key = "in:" + str(h["name"])
	if key == "": return
	if lesions.has(key): lesions.erase(key)
	else: lesions[key] = true
	editor.lesions = lesions; editor.queue_redraw(); _lesion_note = ""
	_dirty = 0.0; _build_inspector()   # the live view restarts on the lesioned circuit

func _lesion_run(sets: Array) -> void:
	if _lrun != null or tele == null or tele.task == null or Circuits.neuron_count(circuit) == 0 or sets.is_empty(): return
	var jobs := []
	var have_base := false
	for r in lesion_rows:
		if (r["lesions"] as Array).is_empty(): have_base = true
	if not have_base: jobs.append({"label": Lesion.label(circuit, {}), "lesions": {}})
	for ls in sets: jobs.append({"label": Lesion.label(circuit, ls), "lesions": ls})
	if tele.sim != null: tele.sim.stop()
	_lrun = Lesion.Runner.new(); _lrun.begin(task_id, circuit, jobs); _lesion_note = ""
	_build_inspector()

func _lesion_sweep() -> void:
	var sets := []
	for ls in Lesion.singles(circuit):
		var lab := Lesion.label(circuit, ls); var known := false
		for r in lesion_rows:
			if r["label"] == lab: known = true
		if not known: sets.append(ls)
	if sets.is_empty(): _lesion_note = "Every single lesion is already in the table."; _build_inspector(); return
	_lesion_run(sets)

func _lesion_cancel() -> void:
	_lrun = null; _reload(); _build_inspector()

func _lesion_step() -> void:
	_lrun.step(12000)
	var pl := insp_body.find_child("LesionProg", true, false) as Label if insp_body != null else null
	if pl != null and not _lrun.done(): pl.text = "Running %d of %d: %s (%d%%)" % [_lrun.i + 1, _lrun.jobs.size(), _lrun.current_label(), int(100.0 * _lrun.fraction())]
	if not _lrun.done(): return
	if _lrun.failed != "": _banner_raw("The lesion run stopped: " + _lrun.failed)
	for res in _lrun.results:
		var at := -1
		for i in lesion_rows.size():
			if lesion_rows[i]["label"] == res["label"]: at = i
		if at >= 0: lesion_rows[at] = res
		else: lesion_rows.append(res)
	lesion_rows.sort_custom(func(a, b):   # the intact baseline on top, then the most damaging lesion first
		var ea := (a["lesions"] as Array).is_empty(); var eb := (b["lesions"] as Array).is_empty()
		if ea != eb: return ea
		return float(a["score"]) < float(b["score"]))
	_lrun = null; _dirty = -1.0; _reload(); _build_inspector()

func _lesion_md() -> String: return Lesion.markdown(_title(), str(circuit.get("name", "circuit")), int(circuit.get("seed", 1)), lesion_rows)

func _lesion_export(kind: String) -> void:
	if lesion_rows.is_empty(): return
	match kind:
		"md": DisplayServer.clipboard_set(_lesion_md()); _lesion_note = "Markdown table copied to the clipboard."
		"csv": DisplayServer.clipboard_set(Lesion.csv(lesion_rows)); _lesion_note = "CSV copied to the clipboard."
		"save":
			var dir := Lesion.save(task_id, _lesion_md(), Lesion.csv(lesion_rows))
			_lesion_note = ("Saved .md and .csv to " + dir) if dir != "" else "Could not write the files."
	_build_inspector()

func _inspect_lesion(small: int) -> void:
	insp_body.add_child(_head("LESION LAB", Kit.RED, "cut an axon, silence a population or cut a sensor"))
	var running := _lrun != null
	insp_body.add_child(Kit.label("Click an axon, a population or a sensor card. RUN LESION re-runs the whole episode on the same seed; SWEEP tries every single lesion." if Layout.phone else "Click an axon, a population or a sensor card on the circuit. The live view restarts with it removed; RUN LESION re-runs the whole episode on the same seed and puts the result in the table. SWEEP tries every single lesion.", small, Kit.TEXT_SOFT, true))
	var names := []
	for k in lesions: names.append(Lesion.key_label(circuit, str(k)))
	insp_body.add_child(Kit.label(("Lesioned now: " + ", ".join(names)) if not names.is_empty() else "Nothing lesioned yet.", small, Kit.RED if not names.is_empty() else Kit.TEXT_SOFT, true))
	var br := HFlowContainer.new(); br.add_theme_constant_override("h_separation", 8); br.add_theme_constant_override("v_separation", 6); insp_body.add_child(br)
	var run := Kit.btn("RUN LESION", func(): _lesion_run([lesions.duplicate()]), "amber", 18); run.disabled = lesions.is_empty() or running; br.add_child(run)
	var sw := Kit.btn("SWEEP ALL SINGLES", _lesion_sweep, "cyan", 18); sw.disabled = running; br.add_child(sw)
	var cl := Kit.btn("CLEAR SET", func(): lesions.clear(); editor.queue_redraw(); _dirty = 0.0; _build_inspector(), "panel", 18); cl.disabled = lesions.is_empty() or running; br.add_child(cl)
	br.add_child(Kit.btn("EXIT", _lesion_toggle, "ghost", 18))
	if running:
		var pl := Kit.label("Starting...", small, Kit.exc(), true); pl.name = "LesionProg"; insp_body.add_child(pl)
		insp_body.add_child(Kit.btn("CANCEL", _lesion_cancel, "panel", 18))
	insp_body.add_child(Kit.hsep())
	insp_body.add_child(Kit.label("LESION TABLE  (%d rows)" % lesion_rows.size() if not lesion_rows.is_empty() else "LESION TABLE: empty. Set a lesion and press RUN LESION.", small, Kit.TEXT if not lesion_rows.is_empty() else Kit.TEXT_SOFT, true))
	var base_score := -1.0
	for r in lesion_rows:
		if (r["lesions"] as Array).is_empty(): base_score = float(r["score"])
	var grid := GridContainer.new(); grid.columns = 1 if Layout.phone or size.x < 1100.0 else 2; grid.add_theme_constant_override("h_separation", 8); grid.add_theme_constant_override("v_separation", 6); grid.size_flags_horizontal = Control.SIZE_EXPAND_FILL; insp_body.add_child(grid)
	for r in lesion_rows:
		var pn := PanelContainer.new(); pn.size_flags_horizontal = Control.SIZE_EXPAND_FILL; pn.add_theme_stylebox_override("panel", Kit.sb(Kit.PANEL_HI, Kit.GREEN if r["passed"] else Kit.RED, 1, 8, 8, false)); grid.add_child(pn)
		var h := HBoxContainer.new(); h.add_theme_constant_override("separation", 10); pn.add_child(h)
		var chip := Kit.chip("PASS" if r["passed"] else "FAIL", Kit.GREEN if r["passed"] else Kit.RED, 14); chip.size_flags_vertical = Control.SIZE_SHRINK_CENTER; h.add_child(chip)
		var vb := VBoxContainer.new(); vb.size_flags_horizontal = Control.SIZE_EXPAND_FILL; vb.add_theme_constant_override("separation", 0); h.add_child(vb)
		vb.add_child(Kit.shout(str(r["label"]), 16 if not Layout.phone else 15, Kit.TEXT))
		vb.add_child(Kit.label(str(r["verdict"]), 15, Kit.TEXT_SOFT, true))
		var sc := float(r["score"]); var d := "" if base_score < 0.0 or (r["lesions"] as Array).is_empty() else ("  %+d" % int(roundf(sc - base_score)))
		var sl := Kit.shout("%d%s" % [int(roundf(sc)), d], 22 if not Layout.phone else 18, Kit.TEXT); sl.size_flags_vertical = Control.SIZE_SHRINK_CENTER; h.add_child(sl)
	if not lesion_rows.is_empty():
		var ex := HFlowContainer.new(); ex.add_theme_constant_override("h_separation", 8); ex.add_theme_constant_override("v_separation", 6); insp_body.add_child(ex)
		ex.add_child(Kit.btn("COPY MARKDOWN", func(): _lesion_export("md"), "panel", 18)); ex.add_child(Kit.btn("COPY CSV", func(): _lesion_export("csv"), "panel", 18)); ex.add_child(Kit.btn("SAVE FILES", func(): _lesion_export("save"), "green", 18))
	if _lesion_note != "": insp_body.add_child(Kit.label(_lesion_note, small, Kit.GREEN, true))
