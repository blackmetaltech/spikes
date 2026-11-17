class_name LabScreen
extends Screen
## The lab: circuit editor (left) and live instruments (right) around one running experiment.
##   editor     neurons from a NEST model list with presets, drag-to-connect, an inspector for neurons, edges and channels
##   task view  the body in its world, driven by the circuit's motor outputs
##   scopes     scrolling raster, 1-4 oscilloscope traces, output-rate gauges
##   controls   run / pause / reset, speed, score card, reference circuit, load and save JSON, analysis
## The circuit Dictionary is the single source of truth; any structural edit reloads the engine (debounced) and restarts the episode.
const SPEEDS := [["0.25", "0.25x"], ["0.5", "0.5x"], ["1.0", "1x"], ["2.0", "2x"], ["5.0", "5x"], ["10.0", "10x"], ["20.0", "20x"], ["50.0", "50x"]]
const SENSOR_KIND := {"lamprey": "dc"}   # last resort when neither the task nor its reference circuit names a sensor kind

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
var _lesion_open := false          # phone: the lesion sheet shows the table (else a one-line bar with RUN / SWEEP)
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

## v0.3 lab play: difficulty mode and variant, hint ladder, verdict highlight, post-solve histograms, diagnosis, instant check, skip, celebration, surprise.
var mode := "standard"
var variant := ""
var celebration: Celebration
static var _fails := {}            # task -> failed runs this session (the Standard hint ladder opens after HintPanel.UNLOCK_FAILS)
static var _hint_tier := {}        # task -> highest hint tier revealed
var _hints_cache: Array = []       # Task::hints() as of the last failed run
var _hint_panel: HintPanel = null
var _hint_btn: Button = null
var _hint_open := false
var _check_task = null             # instant check: the task handle that owns the async job
var _check_job := 0
var _check_t0 := 0
var _check_btn: Button = null
var _recipe_open := false          # Standard: the SHOW RECIPE button was pressed this visit (Progress.record_recipe holds it across visits)
var _last_edit_ms := -10000      # play-log edit coalescing: a slider drag is one edit
var _half_warned := false
var _mode_row: Control = null
var _skip_btn: Button = null
var _results_btn: Button = null
var _histo_ov: Control = null
var _surprise_done := {}
var _surprise_clock := 0.0
var _dx := {}                      # diagnosis case state {picked, tries, solved, gave_up}
var _dx_info := {}                 # Task::diagnosis()

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
	if not phone and task_id != "": v.add_child(_play_bar())
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
	editor.selection_changed.connect(func(_s): _sel_audio(); _build_inspector())
	editor.lesion_clicked.connect(_lesion_click)
	editor.editable = not _is_dx()   # a diagnosis case is probed, never rebuilt
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
	if _is_dx(): score_card.visible = false   # the case has no score to watch: the answer is the score
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
	celebration = Celebration.new(); ov.add_child(celebration)
	add_child(ov)
	if _pending_banner != "": _banner_raw(_pending_banner); _pending_banner = ""
	_attach()
	if OS.get_environment("SP_LABTAB") != "": tab = OS.get_environment("SP_LABTAB")
	if compact: _apply_tab()
	_file_dlg = FileDialog.new(); _file_dlg.access = FileDialog.ACCESS_FILESYSTEM; _file_dlg.filters = PackedStringArray(["*.json ; Circuit JSON"]); _file_dlg.current_dir = ProjectSettings.globalize_path(Circuits.user_dir())
	add_child(_file_dlg)
	if _is_dx(): _brief_forced = true
	_build_inspector()
	_hint_btn_sync()
	if OS.get_environment("SP_ADV") != "": _adv_open = true
	if OS.get_environment("SP_SEL") != "": _select_env(OS.get_environment("SP_SEL"))
	if OS.get_environment("SP_MENU") != "": _open_menu()
	if OS.get_environment("SP_PARTS") != "": _open_parts()
	if OS.get_environment("SP_LESION") != "": _lesion_env.call_deferred()
	_demo_hooks.call_deferred()
	if OS.get_environment("SP_TESTNOW") != "": _test_start.call_deferred()   # dev hook: run TEST x5 at once, print the result, quit

## The circuit the live view runs: the player's, with the lesion set applied in the lesion lab.
func _live_circuit() -> Dictionary: return Lesion.apply(circuit, lesions) if lesion_on and not lesions.is_empty() else circuit

func _exit_tree() -> void:
	_check_cancel()
	if task_id != "": Playlog.level_end(task_id, _rec_pass or bool(_dx.get("solved", false)))
	GameAudio.silence_body()
	if app != null and not circuit.is_empty() and not _is_dx(): app.lab_keep[task_id] = circuit
	if tele != null and tele.sim != null: tele.sim.stop()

# ---------- setup ----------
## Open `tid` ("" = free run). Starts from the task skeleton (its sensors and motors, no neurons), or the reference circuit with SP_REF=1.
func _start(tid: String) -> void:
	task_id = tid
	if OS.get_environment("SP_TASK") != "": task_id = OS.get_environment("SP_TASK")
	mode = Progress.mode(); variant = Campaign.pending_variant; Campaign.pending_variant = ""
	if OS.get_environment("SP_VARIANT") != "": variant = OS.get_environment("SP_VARIANT")
	if task_id != "": Playlog.level_start(task_id, variant)   # the only level_start of a visit (a variant switch is an event, the map sends none)
	var loaded := false
	if _is_dx() and ext_ok:   # a diagnosis case: the patient's circuit, read-only, straight from the task
		var dt = Campaign.make(task_id)
		if dt != null and dt.has_method("diagnosis"):
			_dx_info = dt.diagnosis()
			var dc := Circuits.parse(Circuits.read_reference(str(_dx_info.get("circuit", "")) if str(_dx_info.get("circuit", "")) != "" else _ref_path(task_id)))
			if not dc.is_empty(): circuit = dc; loaded = true
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
	_ensure_task_weights()
	part_card = _default_part()
	_reload(true)
	if OS.get_environment("SP_SPEED") == "" and tele.task != null: tele.speed = _default_speed(float(tele.task.duration_ms()), str(Campaign.info(task_id).get("kind", "task")))

## G3 / v0.3 pacing: the default speed by episode length, from Telemetry.default_speed (it also knows what the machine measured it can deliver).
static func _default_speed(duration_ms: float, kind := "task") -> float: return Telemetry.default_speed(duration_ms, kind)

## The part the next neuron is made from at the start: the cell type the task declares (SpikeTask.default_model(): what its reference circuit and verdicts
## assume), else the first population model of the task's reference circuit (iaf_psc_alpha included), else iaf_psc_alpha. Never a guess from the name.
func _default_part() -> String:
	if task_id == "": return "iaf_psc_alpha"
	var dm := ""
	var t = Campaign.make(task_id)
	if t != null and t.has_method("default_model"): dm = str(t.call("default_model"))
	if dm != "":
		if Catalog.has_card(dm): return dm
		var cs := Catalog.cards_of(dm)
		if not cs.is_empty(): return str(cs[0]["id"])
	var ref := Circuits.parse(Circuits.read_reference(_ref_path(task_id)))
	if not (ref.get("populations", []) as Array).is_empty():
		var tc := _task_cards()
		if not tc.is_empty(): return str(tc[0])
	return "iaf_psc_alpha"

## Catalogue card ids "for this task": the cards of the models its reference circuit uses (in order of first use), then the other cards of the task's chapter.
func _task_cards() -> Array: return _cards_for(true)

## Only the cards of the models the reference circuit uses: the parts picker lets these be chosen even when their chapter is still locked (a recipe needs them).
func _reference_cards() -> Array: return _cards_for(false)

func _cards_for(with_chapter: bool) -> Array:
	var out := []
	if task_id == "": return out
	var ref := Circuits.parse(Circuits.read_reference(_ref_path(task_id)))
	var models := []
	for p in ref.get("populations", []):
		var m := str(p.get("model", ""))
		if not models.has(m): models.append(m)
	for m in models:
		var cs := Catalog.cards_of(m)
		if cs.is_empty(): continue
		var pick: Dictionary = cs[0]
		for c in cs:   # the card whose preset matches the reference's parameters, else the first of the model
			var match_all := true
			for p in ref.get("populations", []):
				if str(p.get("model", "")) == m and not (c["preset"] as Dictionary).is_empty() and (c["preset"] as Dictionary) != p.get("params", {}): match_all = false
			if match_all and not (c["preset"] as Dictionary).is_empty(): pick = c; break
		if not out.has(pick["id"]): out.append(pick["id"])
		for c in cs:   # every preset card of a used model (a reference may use two presets of one model)
			if not out.has(c["id"]) and not with_chapter: out.append(c["id"])
	if not with_chapter: return out
	var chap := str(Campaign.info(task_id).get("chapter", ""))
	for c in Catalog.cards():
		if Catalog.chapter_of(c["id"]) == chap and not out.has(c["id"]): out.append(c["id"])
	return out

## Reference circuits do not count for progress, and neither does anything derived from one: the flag stays through every edit (a nudged weight is still the
## reference) and is cleared only when the bench is emptied, so a pass has to come from a circuit the player built.
func _mark_reference(on: bool) -> void:
	var meta: Dictionary = circuit.get("meta", {}); var ed: Dictionary = meta.get("editor", {})
	if on: ed["from_reference"] = true
	else: ed.erase("from_reference")
	meta["editor"] = ed; circuit["meta"] = meta
func _is_reference() -> bool: return bool(circuit.get("meta", {}).get("editor", {}).get("from_reference", false))

func _ref_path(tid: String) -> String: return Campaign.ref_path(tid)

func _last_path() -> String: return Circuits.user_dir().path_join("last_%s.json" % (task_id if task_id != "" else "free"))

## The empty bench of a task: its sensors and motors with the kinds the task reads (dc or Poisson sensors, rate or modulator motors) from the task's
## channel_kinds() or, until the extension has it, from its reference circuit.
func _skeleton(tid: String) -> Dictionary:
	var c := Circuits.empty("My " + tid + " circuit" if tid != "" else "My circuit")
	if ext_ok and tid != "":
		var t = Campaign.make(tid)
		if t != null:
			var kinds := Circuits.task_channel_kinds(tid, t)
			for s in t.sensor_channels():
				var kind: String = str((kinds["inputs"] as Dictionary).get(str(s), SENSOR_KIND.get(tid, "poisson")))
				var tw: float = float((kinds["weights"] as Dictionary).get(str(s), 0.0))   # the task's own sensor weight (ChannelKind::weight), else the heuristic
				c["inputs"].append({"name": str(s), "kind": kind, "targets": [], "weight": tw if tw > 0.0 else Circuits.input_weight("iaf_psc_alpha", kind), "value": 0.0})
			for m in t.motor_channels():
				var ch := {"name": str(m), "kind": "rate", "neurons": []}
				var ok: Dictionary = (kinds["outputs"] as Dictionary).get(str(m), {"kind": "rate"})
				Circuits.set_output_kind(ch, str(ok.get("kind", "rate")), str(ok.get("modulator", "NE")), float(ok.get("gain", Circuits.MOD_GAIN)), float(ok.get("tau_ms", Circuits.MOD_TAU)))
				c["outputs"].append(ch)
		c["meta"]["task"] = tid
		_store_task_weights(c, tid, t)
	return c

## Remember the task-declared sensor weights in meta.editor.task_weights = {name: {kind, w}} so the first connect (editor, family wiring) uses them.
func _store_task_weights(c: Dictionary, tid: String, t: Variant = null) -> void:
	if tid == "" or not ext_ok: return
	if t == null: t = Campaign.make(tid)
	if t == null: return
	var kinds := Circuits.task_channel_kinds(tid, t); var tw := {}
	for name in kinds["weights"]:
		tw[name] = {"kind": str((kinds["inputs"] as Dictionary).get(name, "poisson")), "w": float(kinds["weights"][name])}
	var meta: Dictionary = c.get("meta", {}); var ed: Dictionary = meta.get("editor", {})
	if tw.is_empty(): ed.erase("task_weights")
	else: ed["task_weights"] = tw
	meta["editor"] = ed; c["meta"] = meta

func _ensure_task_weights() -> void:
	if task_id != "" and not _is_dx() and not (circuit.get("meta", {}).get("editor", {}) as Dictionary).has("task_weights"): _store_task_weights(circuit, task_id)

func _reload(first := false) -> void:
	var keep: Array = tele.watched.duplicate() if tele != null else []
	var was_running := tele.running if tele != null else true; var sp := tele.speed if tele != null else 1.0
	if tele.sim != null: tele.sim.stop()
	var old_mods: Dictionary = tele.mods.duplicate() if tele != null else {}
	if celebration != null: celebration.abort()
	tele = Telemetry.new(); tele.running = was_running; tele.speed = sp; tele.mods = old_mods; tele.mode = mode; tele.variant = variant
	tele.lesioned = lesion_on and not lesions.is_empty()   # the live view runs the lesioned circuit: watch the failure
	var ok := tele.setup(_live_circuit(), task_id, ext_ok)   # always makes the task, so the world and the brief show even for a half-built circuit
	if ok and not keep.is_empty() and not first:
		var valid := keep.filter(func(i): return i < tele.n)
		if not valid.is_empty(): tele.set_watched(valid)
	_fail_shown = false; _rec_done = false; _rec_pass = false; _overlay_dismissed = false; _test_result = {}
	if editor != null: editor.verdict_target = ""
	if score_card != null: score_card.guilty = ""
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
	if task_id != "" and Time.get_ticks_msec() - _last_edit_ms > 1500: Playlog.event("edit", {"task": task_id})
	_last_edit_ms = Time.get_ticks_msec()
	_dirty = 0.0; _lesion_reset(); _surprise_done.erase("osc")
	if editor != null: editor.verdict_target = ""
	if score_card != null: score_card.guilty = ""
	if _is_reference() and Circuits.neuron_count(circuit) == 0: _mark_reference(false)   # an emptied bench is the player's own again; an edited reference is still derived from it

func _process(dt: float) -> void:
	if _dirty >= 0.0:
		_dirty += dt
		if _dirty > 0.3 and _lrun == null: _dirty = -1.0; _reload(); _build_inspector()
	if not _test.is_empty(): _test_step()
	elif _lrun != null: _lesion_step()
	elif tele != null: tele.advance(dt)
	_watch_episode()
	_check_poll(); _surprise_tick(dt)
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
	var dxm := _is_dx()
	if task_id != "": h.add_child(Kit.btn("CASE" if dxm else "BRIEF", func(): editor.select({}); _brief_forced = true; _build_inspector(), "ghost", bs, Vector2(84, 0)))
	if task_id != "" and not dxm: h.add_child(Kit.btn("REFERENCE", _load_reference, "cyan", bs, Vector2(150, 0)))
	if task_id != "" and not dxm: h.add_child(Kit.btn("EMPTY BENCH", _empty_bench, "panel", bs, Vector2(0, 0)))
	if task_id != "" and not dxm: h.add_child(Kit.btn("TEST x5", _test_start, "green", bs, Vector2(110, 0)))
	if task_id != "": _lesion_btn = Kit.btn("LESIONS", _lesion_toggle, "panel", bs, Vector2(120, 0)); h.add_child(_lesion_btn)
	if not dxm: h.add_child(Kit.btn("LOAD", _load_dialog, "panel", bs, Vector2(84, 0)))
	if not dxm: h.add_child(Kit.btn("SAVE", _save_dialog, "panel", bs, Vector2(84, 0)))
	h.add_child(Kit.btn("ANALYSIS", func(): app.open_analysis(circuit, tele, task_id), "green", bs, Vector2(130, 0)))
	_sync_run_btn.call_deferred()
	return h

func _is_dx() -> bool: return task_id != "" and str(Campaign.info(task_id).get("kind", "")) == "diagnosis"

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
	var bin := PartsBin.new().setup("pick", Circuits.sandbox, part_card, _task_cards(), _reference_cards()); v.add_child(bin)
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
## Selecting a population makes only that population click (GameAudio.set_spike_focus); selecting anything else (or nothing) lets them all click.
func _sel_audio() -> void:
	var sel := editor.sel if editor != null else {}
	var f := -1
	if str(sel.get("t", "")) == "pop": f = Circuits.pop_index(circuit, str(sel["id"]))
	GameAudio.set_spike_focus(f)

func _toggle_run() -> void:
	tele.running = not tele.running
	GameAudio.event("run" if tele.running else "pause")
	if not tele.running: GameAudio.silence_body()
	_sync_run_btn()
func _sync_run_btn() -> void:
	if run_btn != null: run_btn.text = "PAUSE" if tele.running else "RUN"
func _restart() -> void:
	_overlay_hide(); _overlay_dismissed = false; _rec_done = false; _rec_pass = false; _fail_shown = false
	if editor != null: editor.verdict_target = ""
	if score_card != null: score_card.guilty = ""
	if celebration != null: celebration.abort()
	GameAudio.event("run")
	if tele.sim != null: tele.reset(); tele.running = true; _sync_run_btn()
	elif tele.task != null: tele.task.reset(int(circuit.get("seed", 1)))
	task_view.attach(tele)

func _load_reference() -> void:
	var txt := Circuits.read_reference(_ref_path(task_id))
	var c := Circuits.parse(txt)
	if c.is_empty(): _banner("the reference circuit for this task is not installed (circuits/%s)" % _ref_path(task_id)); return
	circuit = c; Circuits.ensure_pos(circuit); _mark_reference(true); editor.select({}); _lesion_reset(); _reload(true); _build_inspector()

## Start over from the task's skeleton: its sensors and motors, no neurons (and no reference-derived flag).
func _empty_bench() -> void:
	if task_id == "": return
	_close_menu(); _test = {}; _lrun = null
	circuit = _skeleton(task_id); Circuits.ensure_pos(circuit); editor.select({}); _lesion_reset(); _reload(true); _build_inspector()

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
	var dxm := _is_dx()
	if task_id != "":
		item.call("CASE" if dxm else "BRIEF", func(): editor.select({}); _brief_forced = true; _build_inspector(), "ghost")
		if not dxm: item.call("REFERENCE", _load_reference, "cyan")
		if not dxm: item.call("EMPTY BENCH", _empty_bench)
		if not dxm: item.call("TEST x5", _test_start, "green")
		item.call("LESIONS" if not lesion_on else "LEAVE LESIONS", _lesion_toggle, "amber" if lesion_on else "panel")
		v.add_child(Kit.hsep()); v.add_child(Kit.label("PLAY", 15, Kit.TEXT_SOFT))
		var prow := HFlowContainer.new(); prow.add_theme_constant_override("h_separation", 8); prow.add_theme_constant_override("v_separation", 8); v.add_child(prow)
		_play_controls(prow, true)
		for b in prow.get_children():   # a menu choice closes the menu (steppers stay open to compare)
			if b is Button and b != _hint_btn: (b as Button).pressed.connect(_close_menu)
	if not dxm: item.call("LOAD", _load_dialog); item.call("SAVE", _save_dialog)
	item.call("ANALYSIS", func(): app.open_analysis(circuit, tele, task_id), "green")
	_size_menu.call_deferred(sc, v)

func _size_menu(sc: ScrollContainer, v: Control) -> void:
	await get_tree().process_frame
	if is_instance_valid(sc) and is_instance_valid(v): sc.custom_minimum_size.y = minf(v.get_combined_minimum_size().y, size.y - 90.0)

# ---------- progress, end-of-episode card, 5-seed test ----------
func _rate_per_neuron() -> float: return tele.spike_rate_hz() / maxf(float(Circuits.neuron_count(circuit)), 1.0) if tele != null and tele.sim != null else -1.0

func _stars_now() -> int:
	if tele == null or tele.task == null: return 0
	var st := Progress.stars_for(task_id, bool(tele.score.get("passed", false)), false, Circuits.neuron_count(circuit), tele.spike_rate_hz() / maxf(float(Circuits.neuron_count(circuit)), 1.0))
	return mini(st, 2) if _recipe_used() else st

## Called every frame: record a pass the moment it happens and the final result at the end of the episode; raise the end card; report a stopped run.
func _watch_episode() -> void:
	if tele == null: return
	if tele.failed and not _fail_shown: _fail_shown = true; _banner_raw("The simulation stopped: " + tele.error + ". Fix the circuit and press RESET.")
	if tele.task == null or tele.sim == null or tele.failed or not _test.is_empty() or lesion_on or _is_dx(): return
	var passed := bool(tele.score.get("passed", false))
	if passed and not _rec_pass:
		_rec_pass = true
		var first := not bool(Progress.best(task_id)["passed"]) and not _is_reference() and variant == ""
		_record(true, false)
		if mode == "researcher" and variant == "": first = false; celebration.toast("Passed once", "Researcher mode counts a pass only from TEST x5: press TEST x5 to record it.", Kit.AMBER, 5.0)
		if editor != null: editor.hint_target = ""; editor.verdict_target = ""
		if score_card != null: score_card.guilty = ""
		GameAudio.event("pass")
		if first: _celebrate()
	if tele.finished and not _rec_done:
		_rec_done = true
		_record(passed, false)
		if not passed: _note_fail()
		if not _overlay_dismissed: _overlay_show()

## Record a result: a pass in a challenge variant goes on the variant list (no stars), everything else is the task's own best.
func _record(passed: bool, five_seeds: bool, mean := -1.0, rate := -2.0) -> void:
	var r := _rate_per_neuron() if rate < -1.5 else rate
	var v := float(tele.score.get("value", 0.0)) if mean < 0.0 else mean
	if variant != "":
		if passed and not _is_reference() and _variant_ok(): Progress.record_variant(task_id, variant)
		return
	if mode == "researcher" and not five_seeds: passed = false   # Researcher: a pass needs all five TEST seeds
	Progress.record_result(task_id, v, {"passed": passed, "five_seeds": five_seeds, "neurons": Circuits.neuron_count(circuit), "rate_hz": r, "reference_derived": _is_reference(), "recipe_used": _recipe_used()})

## The challenge variants that bind the lab, not only the engine's verdict: half_neurons passes only within half the par (Progress.budget).
func _variant_ok() -> bool:
	if variant != "half_neurons": return true
	var par := Progress.budget(task_id)
	if par <= 0: return true
	var lim := par / 2; var n := Circuits.neuron_count(circuit)
	if n <= lim: return true
	if not _half_warned and celebration != null:
		_half_warned = true
		celebration.toast("Over the half-neuron limit", "%d neurons; this variant allows %d (half of par %d). It does not count until the circuit is smaller." % [n, lim, par], Kit.AMBER, 6.0)
	return false

## A recipe is in force for the stars: Guided shows it inline (no cost), Standard costs the third star once SHOW RECIPE was pressed.
func _recipe_used() -> bool: return mode == "standard" and (_recipe_open or Progress.recipe_used(task_id))

## Task::recipe() when the extension has it (concrete numbers kept apart from the brief), else "".
func _recipe_text() -> String:
	if task_id == "" or tele == null or tele.task == null or not tele.task.has_method("recipe"): return ""
	return str(tele.task.call("recipe"))

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
		parts.append("This is the reference circuit, or an edited copy of it: it earns no stars. Press EMPTY BENCH and build your own.")
	elif not passed:
		if _test_result.is_empty(): parts.append(str(sc.get("verdict", "")))
		if mode == "researcher": parts.append("Researcher mode: no hints. Read the scopes and the lesion table.")
		elif HintPanel.available(mode, int(_fails.get(task_id, 0))): parts.append("The glowing part of the circuit is what the verdict is about. Press HINT for a nudge.")
		else: parts.append("The glowing part of the circuit is what the verdict is about. Hints open after %d missed runs (%d so far)." % [HintPanel.UNLOCK_FAILS, int(_fails.get(task_id, 0))])
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
	var bl := Kit.label("best %d" % int(roundf(float(best["value"]))) if not _is_reference() else "reference-derived: not recorded", 18 if not phone else 16, Kit.TEXT_SOFT); bl.size_flags_vertical = Control.SIZE_SHRINK_CENTER; sr.add_child(bl)
	v.add_child(Kit.label(_end_text(), 19 if not phone else 16, Kit.TEXT, true))
	if tele.sim != null:   # the energy bill of this run
		var hz := tele.spike_rate_hz()
		v.add_child(Kit.label("Energy: %s spikes/s, about %s (roughly 1e8 to 1e9 ATP per spike)." % [str(int(roundf(hz))), Telemetry.atp_text(hz)], 16 if not phone else 15, Kit.exc(), true))
	var br := HFlowContainer.new(); br.add_theme_constant_override("h_separation", 10); br.add_theme_constant_override("v_separation", 8); v.add_child(br)
	br.add_child(Kit.btn("RUN AGAIN", _restart, "amber", 20))
	br.add_child(Kit.btn("TEST x5", _test_start, "green", 20))
	if passed and not _is_reference(): br.add_child(Kit.btn("RESULTS", _histo_show, "cyan", 20))
	if not passed and mode != "researcher" and _test_result.is_empty(): br.add_child(Kit.btn(HintPanel.button_text(mode, int(_fails.get(task_id, 0)), int(_hint_tier.get(task_id, 0))), func(): _overlay_dismissed = true; _overlay_hide(); _hint_toggle(), "amber" if HintPanel.available(mode, int(_fails.get(task_id, 0))) else "ghost", 20))
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
	var sim = ClassDB.instantiate("SpikeSim"); var task = Campaign.make(task_id); Telemetry.configure(task, mode, variant)
	_test = {"i": 0, "res": [], "sim": sim, "task": task, "t": -1.0}
	tele.running = false
	_overlay_show(true)

func _test_cancel() -> void:
	_test = {}; _overlay_hide(); _reload(); _build_inspector()

func _test_step() -> void:
	var sim = _test["sim"]; var task = _test["task"]
	if float(_test["t"]) < 0.0:   # begin episode i
		var tc := Telemetry.sanitized(circuit)
		tc["seed"] = 1000 * int(TEST_SEEDS[int(_test["i"])]) + int(circuit.get("seed", 1))   # the circuit is reseeded with the world (as the engine's tests do), so a deterministic world is not five identical runs
		if not sim.load_circuit(Circuits.to_json(tc)): _test = {}; _overlay_hide(); _reload(); _banner(str(sim.error())); return
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
	if passes > 0:   # the mean of the five scores is the record, not the best of them
		var mean := 0.0
		for r in res: mean += float(r["value"])
		mean /= float(res.size())
		_record(true, passes == res.size(), mean, -1.0)
	if passes < res.size(): _note_fail()
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
	elif sel.is_empty(): _inspect_dx(small) if _is_dx() else _inspect_brief(small)
	elif _is_dx(): _inspect_dx_pick(sel, small)
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
		if _palette_row != null: _palette_row.visible = not lesion_on and not _is_dx()
		return
	var show := (not editor.sel.is_empty() or _brief_forced or lesion_on) and tab == "circuit"
	inspector.visible = show
	if _palette_row != null: _palette_row.visible = not show and not _is_dx()   # the sheet needs the room
	if not show and editor.inset_bottom != 0.0: editor.inset_bottom = 0.0; editor.fit()

## Size the inspector's scroll area to its content (capped), and keep the phone editor's neurons above the sheet.
func _fit_inspector() -> void:
	await get_tree().process_frame
	if not is_inside_tree() or insp_scroll == null or insp_body == null: return
	var want := insp_body.get_combined_minimum_size().y
	var big := lesion_on or (_is_dx() and editor.sel.is_empty())   # the lesion table and the case card need more room than a neuron's inspector
	var cap := size.y * ((0.62 if big else 0.46) if Layout.phone else (0.6 if lesion_on else (0.5 if big else 0.36)))
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
	var rec := _recipe_text()
	if rec != "" and mode != "researcher":
		if mode == "guided" or _recipe_open:
			var rh := HBoxContainer.new(); rh.add_theme_constant_override("separation", 8); rh.add_child(Kit.chip("RECIPE", Kit.AMBER, 14)); insp_body.add_child(rh)
			insp_body.add_child(Kit.label(rec, small, Kit.TEXT, true))
			if mode == "standard": insp_body.add_child(Kit.label("You used the recipe: this level can earn two stars, not three.", small, Kit.AMBER, true))
		else:
			var rb := Kit.btn("SHOW RECIPE", func(): _recipe_open = true; Progress.record_recipe(task_id); _build_inspector(), "amber", 18); rb.size_flags_horizontal = Control.SIZE_SHRINK_BEGIN; insp_body.add_child(rb)
			insp_body.add_child(Kit.label("The recipe gives the numbers. Using it costs the third star.", small, Kit.TEXT_SOFT, true))
	if task_id != "" and Progress.budget(task_id) > 0: insp_body.add_child(Kit.label("Stars: 1 pass.  2 pass on 5 seeds (TEST x5).  3 the same with %d neurons or fewer." % Progress.budget(task_id), small, Kit.TEXT_SOFT, true))
	var leg := HBoxContainer.new(); leg.add_theme_constant_override("separation", 8)
	leg.add_child(Kit.chip("EXCITATORY", Kit.exc(), 14)); leg.add_child(Kit.chip("INHIBITORY", Kit.inh(), 14)); leg.add_child(Kit.chip("SENSOR", Kit.GREEN, 14)); leg.add_child(Kit.chip("MOTOR", Gfx.VIOLET, 14))
	insp_body.add_child(leg)

## A slider row. `quad` makes the slider square-law (value = hi * s^2 for s in 0..1): fine steps near zero for weights, which run from fractions of a pA (a burster
## onto an integrate-and-fire cell) to thousands (a relay), with the same slider.
func _val_row(name: String, lo: float, hi: float, step: float, val: float, unit: String, cb: Callable, dec := -1, quad := false) -> Control:
	var h := HBoxContainer.new(); h.add_theme_constant_override("separation", 10); h.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var nm := Kit.label(name, 17, Kit.TEXT_SOFT); nm.custom_minimum_size.x = 100; h.add_child(nm)
	var s := HSlider.new(); s.size_flags_horizontal = Control.SIZE_EXPAND_FILL; s.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	if quad: s.min_value = 0.0; s.max_value = 1.0; s.step = QUAD_STEP; s.value = sqrt(clampf(val, 0.0, hi) / maxf(hi, 1e-9))
	else: s.min_value = lo; s.max_value = hi; s.step = step; s.value = val
	s.custom_minimum_size = Vector2(120, maxf(30.0, Layout.tap())); s.focus_mode = Kit.focus_mode(); h.add_child(s)
	var vl := Kit.label(_numd(val, dec if dec >= 0 or not quad else _quad_dec(val)) + " " + unit, 17, Kit.TEXT); vl.custom_minimum_size.x = 96; vl.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT; h.add_child(vl)
	s.value_changed.connect(func(x):
		var v: float = hi * x * x if quad else x
		vl.text = _numd(v, dec if dec >= 0 else (-1 if not quad else _quad_dec(v))) + " " + unit; cb.call(v))
	return h

const QUAD_STEP := 0.0025
## Decimals for a square-law weight: whole numbers above 100, one decimal above 1, else three.
static func _quad_dec(v: float) -> int: return 0 if v >= 100.0 else (1 if v >= 1.0 else 3)

static func _numd(x: float, dec: int) -> String: return _num(x) if dec < 0 else String.num(x, dec)
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
	if _adv_open: _inspect_modulation(id, model, small)

## ADVANCED: how this group responds to the circuit-wide NE / ACh / DA levels. Cells with ach / ne inputs (the NESTML cells) follow those levels by themselves
## (the engine's per-model default); a row here overrides it for one parameter: value = value + gain * level, or value * (1 + scale * level).
func _inspect_modulation(id: String, model: String, small: int) -> void:
	insp_body.add_child(Kit.hsep())
	insp_body.add_child(Kit.label("RESPONDS TO NEUROMODULATORS", small, Kit.TEXT, true))
	var params := Circuits.mod_params(model)
	if params.is_empty(): insp_body.add_child(Kit.label("This model has no parameter a modulator can act on.", small, Kit.TEXT_SOFT, true)); return
	var rows := Circuits.mod_rows(circuit, id)
	insp_body.add_child(Kit.label(("Rows below replace the default response of this group." if not rows.is_empty() else "No rows: cells with ach / ne inputs follow the ACh and NE levels by themselves; others ignore them. Add a row to make any parameter respond."), small, Kit.TEXT_SOFT, true))
	for ri in rows:
		var row: Dictionary = (circuit["modulation"] as Array)[ri]
		var scale := row.has("scale_per_unit"); var val := float(row.get("scale_per_unit" if scale else "gain", 1.0))
		var h := HFlowContainer.new(); h.add_theme_constant_override("h_separation", 8); h.add_theme_constant_override("v_separation", 4); insp_body.add_child(h)
		h.add_child(Kit.stepper(Circuits.MODULATORS.map(func(m): return [m, m]), str(row.get("modulator", "NE")), func(m): row["modulator"] = str(m); _on_structure(), 90.0, 18, 40.0))
		var po := params.map(func(q): return [q, q])
		h.add_child(Kit.stepper(po, str(row.get("param", params[0])), func(q): row["param"] = str(q); _on_structure(), 150.0, 18, 40.0))
		h.add_child(Kit.stepper([["gain", "add"], ["scale", "scale"]], "scale" if scale else "gain", func(m):
			var v: float = float(row.get("scale_per_unit", row.get("gain", 1.0))); row.erase("gain"); row.erase("scale_per_unit")
			row["scale_per_unit" if str(m) == "scale" else "gain"] = v; _on_structure(); _build_inspector(), 100.0, 18, 40.0))
		h.add_child(Kit.btn("X", func(): Circuits.remove_mod_row(circuit, ri); _on_structure(); _build_inspector(), "danger", 18))
		var small_p := str(row.get("param", "")) in ["ach", "ne"]
		insp_body.add_child(_val_row("per unit", -2.0 if small_p else -500.0, 2.0 if small_p else 500.0, 0.05 if small_p else 5.0, val, "", func(x): row["scale_per_unit" if row.has("scale_per_unit") else "gain"] = x; _on_structure(), 2 if small_p else -1))
	insp_body.add_child(Kit.btn("ADD ROW", func(): Circuits.add_mod_row(circuit, id, "NE", str(params[0]), 1.0 if str(params[0]) in ["ach", "ne"] else 50.0); _on_structure(); _build_inspector(), "panel", 18))

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
	var pi := Circuits.pop_index(circuit, post); var wmax: float = Circuits.proj_wmax(str(circuit["populations"][pi].get("model", ""))) if pi >= 0 else 400.0
	var unit := Catalog.unit_of(str(circuit["populations"][pi].get("model", ""))) if pi >= 0 else "pA"
	var r1 := HFlowContainer.new(); r1.add_theme_constant_override("h_separation", 10); insp_body.add_child(r1)
	var cur_syn := Circuits.proj_model(pr)
	if cur_syn != "static_synapse": r1.add_child(Kit.stepper([["static_synapse", "static"], [cur_syn, cur_syn.replace("_synapse", "")]], cur_syn, func(m): var s: Dictionary = pr.get("synapse", {}); s["synapse_model"] = str(m); pr["synapse"] = s; _on_structure(), 190.0 if not Layout.phone else 150.0, 18, 40.0))
	r1.add_child(Kit.btn("E / I", func(): Circuits.set_proj_weight(pr, -Circuits.proj_weight(pr)); _on_structure(); _build_inspector(), "panel", 18))
	r1.add_child(Kit.btn("DELETE", func(): editor.delete_selected(), "danger", 18))
	var pa_i := Circuits.pop_index(circuit, pre)
	var na := Circuits.pop_n(circuit["populations"][pa_i]) if pa_i >= 0 else 1; var nb := Circuits.pop_n(circuit["populations"][pi]) if pi >= 0 else 1
	if na > 1 and nb > 1:   # N4: segment wiring, neuron k to neuron k + offset, instead of every pair
		var wr := HFlowContainer.new(); wr.add_theme_constant_override("h_separation", 10); wr.add_theme_constant_override("v_separation", 4); insp_body.add_child(wr)
		var wiring := Circuits.proj_wiring(pr); var off := Circuits.segment_offset(pr)
		wr.add_child(Kit.label("wiring", small, Kit.TEXT_SOFT))
		wr.add_child(Kit.stepper([["all", "every pair"], ["segment", "neuron k to k"]], wiring, func(m): Circuits.set_wiring(circuit, pr, str(m), 0); _on_structure(); _build_inspector(), 190.0 if not Layout.phone else 160.0, 18, 40.0))
		if wiring == "segment":
			wr.add_child(Kit.label("shift", small, Kit.TEXT_SOFT))
			wr.add_child(Kit.stepper([[-2, "-2"], [-1, "-1"], [0, "0"], [1, "+1"], [2, "+2"]], str(off), func(o): Circuits.set_wiring(circuit, pr, "segment", int(o)); _on_structure(); _build_inspector(), 64.0, 18, 40.0))
		insp_body.add_child(Kit.label("Segment wiring connects neuron k of %s to neuron k%s of %s, one synapse per segment, instead of all %d x %d pairs." % [pre, ("" if off == 0 else " %+d" % off), post, na, nb], small, Kit.TEXT_SOFT, true))
	insp_body.add_child(_val_row("weight", 0.0, wmax, wmax / 200.0, absf(w), unit, func(x): Circuits.set_proj_weight(pr, x * (-1.0 if Circuits.proj_weight(pr) < 0.0 else 1.0)); _on_structure(), -1, true))
	insp_body.add_child(_val_row("delay", 0.0, Circuits.DELAY_MAX, 0.1, Circuits.proj_delay(pr), "ms", func(x): Circuits.set_proj_delay(pr, maxf(0.1, snappedf(x, 0.1))); _on_structure(), -1, true))
	if pr.get("synapse", {}).get("synapse_model", "static_synapse") != "gap_junction":
		insp_body.add_child(_val_row("spread", 0.0, 100.0, 1.0, float(pr.get("weight_cv", 0.0)) * 100.0, "%", func(x): Circuits.set_proj_spread(pr, snappedf(x, 1.0) * 0.01); _on_structure(), 0))
		insp_body.add_child(Kit.label("Spread: each synapse of this axon gets its own weight, varying by this much around the value above (fixed by the circuit seed; TEST x5 draws it again for every seed). A circuit that only works with identical weights fails here.", small, Kit.TEXT_SOFT, true))


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
	var fam := Circuits.channel_family(circuit, key, name)
	if not fam.is_empty():   # N4: one drag or one button wires a whole group to the family
		var fnames: Array = fam["names"]
		insp_body.add_child(Kit.label("%s0 to %s%d are a numbered family of %d. A group of %d neurons wired to any one of them connects neuron k to %s k." % [fam["stem"], fam["stem"], fnames.size() - 1, fnames.size(), fnames.size(), "sensor" if is_in else "channel"], small, Kit.TEXT_SOFT, true))
		var fr := HFlowContainer.new(); fr.add_theme_constant_override("h_separation", 8); fr.add_theme_constant_override("v_separation", 6); insp_body.add_child(fr)
		for pp in circuit.get("populations", []):
			if Circuits.pop_n(pp) == fnames.size():
				var pid := str(pp.get("id", ""))
				fr.add_child(Kit.btn("WIRE %s (%d) TO ALL" % [pid, fnames.size()] if not is_in else "WIRE ALL TO %s" % pid, func(): Circuits.wire_family(circuit, key, name, pid); _on_structure(); _build_inspector(), "cyan", 18))
	if is_in:
		var kind := str(ch.get("kind", "dc")); var unit := "pA" if kind == "dc" else "Hz"
		insp_body.add_child(Kit.stepper([["poisson", "Poisson spikes"], ["dc", "DC current"]], kind, func(k):
			ch["kind"] = str(k); var tm0 := "iaf_psc_alpha"
			if not tg.is_empty():
				var tp0 := Circuits.pop_index(circuit, Circuits.proj_pop(tg[0]))
				if tp0 >= 0: tm0 = str(circuit["populations"][tp0].get("model", tm0))
			ch["weight"] = Circuits.input_weight(tm0, str(k)); _on_structure(); _build_inspector(), 250.0 if not Layout.phone else 200.0, 18, 40.0))
		var wunit := "pA"; var wmax := Circuits.DC_WMAX if kind == "dc" else 1500.0   # a dc input is a current: pA of drive per unit of value, whatever the cell
		if not tg.is_empty() and kind != "dc":
			var tpi := Circuits.pop_index(circuit, Circuits.proj_pop(tg[0]))
			if tpi >= 0:
				var tm := str(circuit["populations"][tpi].get("model", ""))
				wunit = Catalog.unit_of(tm); wmax = Circuits.proj_wmax(tm)
		insp_body.add_child(_val_row("weight", 0.0, wmax, wmax / 300.0, float(ch.get("weight", 1.0)), wunit + ("/unit" if kind == "dc" else ""), func(x): ch["weight"] = x; _on_structure(), -1, true))
		if not driven: insp_body.add_child(_val_row("rate" if kind != "dc" else "value", 0.0, 1000.0 if kind == "dc" else 400.0, 1.0, float(ch.get("value", 0.0)), unit, func(x): ch["value"] = x; _on_structure()))
		if not driven: insp_body.add_child(Kit.btn("REMOVE INPUT", func(): editor.delete_selected(), "danger", 18))
	if not is_in: _inspect_output_kind(ch, small)
	insp_body.add_child(Kit.btn("DISCONNECT ALL", func(): ch["targets" if is_in else "neurons"] = []; _on_structure(); _build_inspector(), "panel", 18))

## An output is a rate readout (the task reads the mean firing rate of its neurons) or a modulator release: each spike adds `gain` to the NE / ACh / DA pool,
## which decays with `tau` (docs/CIRCUIT-FORMAT.md). The heart tasks read ACh (vagal) and NE (sympathetic) that way.
func _inspect_output_kind(ch: Dictionary, small: int) -> void:
	var kind := str(ch.get("kind", "rate"))
	var wanted := ""
	if tele != null and tele.task != null:
		var kk := Circuits.task_channel_kinds(task_id, tele.task); var wk: Dictionary = (kk["outputs"] as Dictionary).get(str(ch.get("name", "")), {})
		if str(wk.get("kind", "rate")) == "modulator": wanted = str(wk.get("modulator", ""))
	var r := HFlowContainer.new(); r.add_theme_constant_override("h_separation", 10); r.add_theme_constant_override("v_separation", 6); insp_body.add_child(r)
	r.add_child(Kit.stepper([["rate", "rate readout"], ["modulator", "releases modulator"]], kind, func(k):
		if str(k) == "modulator": Circuits.set_output_kind(ch, "modulator", wanted if wanted != "" else "NE")
		else: Circuits.set_output_kind(ch, "rate")
		_on_structure(); _build_inspector(), 270.0 if not Layout.phone else 220.0, 18, 40.0))
	if kind != "modulator":
		if wanted != "": insp_body.add_child(Kit.label("This task reads %s from this channel: switch it to \"releases modulator\" so the spikes of its neurons make %s." % [wanted, wanted], small, Kit.AMBER, true))
		return
	var mod := str(ch.get("modulator", "NE")); var gain := float(ch.get("gain", Circuits.MOD_GAIN)); var tau := float(ch.get("tau_ms", Circuits.MOD_TAU))
	r.add_child(Kit.stepper(Circuits.MODULATORS.map(func(m): return [m, m]), mod, func(m): ch["modulator"] = str(m); _on_structure(); _build_inspector(), 90.0, 18, 40.0))
	insp_body.add_child(_val_row("gain", 0.001, 0.5, 0.001, gain, "/spike", func(x): ch["gain"] = x; _on_structure(), 3))
	insp_body.add_child(_val_row("tau", 50.0, 5000.0, 50.0, tau, "ms", func(x): ch["tau_ms"] = x; _on_structure()))
	insp_body.add_child(Kit.label("Each spike adds %s to the %s level, which fades with tau %s ms. At 20 spikes/s the level settles near %s." % [String.num(gain, 3), mod, str(int(roundf(tau))), String.num(gain * 20.0 * tau * 0.001, 2)], small, Kit.TEXT_SOFT, true))


# ---------- lesion lab ----------
## The lab opens after a pass: this circuit passed (now or earlier this session), it is the reference circuit, or the task has been passed before.
func _lesion_ok() -> bool:
	if task_id == "" or tele == null or tele.task == null: return false
	if lesion_on or _is_reference() or _rec_pass or _is_dx(): return true
	return bool(tele.score.get("passed", false)) or bool(Progress.best(task_id)["passed"])

func _lesion_toggle() -> void:
	if lesion_on:
		lesion_on = false; _lrun = null; _lesion_open = false; _lesion_apply_ui(); _dirty = 0.0; _build_inspector(); return
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
	editor.lesion_mode = lesion_on; editor.editable = not lesion_on and not _is_dx(); editor.lesions = lesions; editor.queue_redraw()
	if _lesion_btn != null:
		Kit.skin(_lesion_btn, "amber" if lesion_on else "panel", 13)
	if _palette_row != null and not Layout.phone: _palette_row.visible = not lesion_on and not _is_dx()

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
	GameAudio.event("lesion_cut")
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
	_lrun = Lesion.Runner.new(); _lrun.mode = mode; _lrun.variant = variant; _lrun.begin(task_id, circuit, jobs); _lesion_note = ""
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
	if pl != null and not _lrun.done(): pl.text = "Running %d of %d: %s, seed %d of %d (%d%%)" % [_lrun.i + 1, _lrun.jobs.size(), _lrun.current_label(), _lrun.si + 1, _lrun.seeds.size(), int(100.0 * _lrun.fraction())]
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
	_lrun = null; _dirty = -1.0; _lesion_open = true; _reload(); _build_inspector()
	var sur := Surprise.lesion_improves(lesion_rows)
	if not sur.is_empty() and not _surprise_done.has("les:" + str(sur["label"])):
		_surprise_done["les:" + str(sur["label"])] = true; GameAudio.event("celebrate")
		celebration.toast("Surprise: a lesion that helps!", "With \"%s\" the score went UP by %d points. Something that part does is working against the task: why?" % [str(sur["label"]), int(roundf(float(sur["gain"])))], Kit.GREEN, 8.0)

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
	var running := _lrun != null; var nseeds := Lesion.SEEDS.size()
	var names := []
	for k in lesions: names.append(Lesion.key_label(circuit, str(k)))
	var compact := Layout.phone and not _lesion_open and not running   # M1: while picking, the phone sheet is one line of text and the buttons
	if compact:
		insp_body.add_child(Kit.label(("Lesioned: " + ", ".join(names)) if not names.is_empty() else "Tap an axon, group or sensor to lesion it.", small, Kit.RED if not names.is_empty() else Kit.TEXT_SOFT, true))
	else:
		insp_body.add_child(Kit.label(("Tap an axon, a population or a sensor card. RUN LESION re-runs the episode on %d seeds; SWEEP tries every single lesion." % nseeds) if Layout.phone else "Click an axon, a population or a sensor card on the circuit. The live view restarts with it removed; RUN LESION re-runs the whole episode on %d seeds (the world and the circuit both reseeded) and puts the mean and range in the table. SWEEP tries every single lesion." % nseeds, small, Kit.TEXT_SOFT, true))
		insp_body.add_child(Kit.label(("Lesioned now: " + ", ".join(names)) if not names.is_empty() else "Nothing lesioned yet.", small, Kit.RED if not names.is_empty() else Kit.TEXT_SOFT, true))
	var br := HFlowContainer.new(); br.add_theme_constant_override("h_separation", 8); br.add_theme_constant_override("v_separation", 6); insp_body.add_child(br)
	var run := Kit.btn("RUN", func(): _lesion_run([lesions.duplicate()]), "amber", 18) if Layout.phone else Kit.btn("RUN LESION", func(): _lesion_run([lesions.duplicate()]), "amber", 18); run.disabled = lesions.is_empty() or running; br.add_child(run)
	var sw := Kit.btn("SWEEP" if Layout.phone else "SWEEP ALL SINGLES", _lesion_sweep, "cyan", 18); sw.disabled = running; br.add_child(sw)
	var cl := Kit.btn("CLEAR" if Layout.phone else "CLEAR SET", func(): lesions.clear(); editor.queue_redraw(); _dirty = 0.0; _build_inspector(), "panel", 18); cl.disabled = lesions.is_empty() or running; br.add_child(cl)
	if Layout.phone and not running and not lesion_rows.is_empty():
		br.add_child(Kit.btn(("HIDE TABLE" if _lesion_open else "TABLE (%d)" % lesion_rows.size()), func(): _lesion_open = not _lesion_open; _build_inspector(), "green" if not _lesion_open else "panel", 18))
	br.add_child(Kit.btn("EXIT", _lesion_toggle, "ghost", 18))
	if running:
		var pl := Kit.label("Starting...", small, Kit.exc(), true); pl.name = "LesionProg"; insp_body.add_child(pl)
		insp_body.add_child(Kit.btn("CANCEL", _lesion_cancel, "panel", 18))
	if compact: return
	insp_body.add_child(Kit.hsep())
	insp_body.add_child(Kit.label("LESION TABLE  (%d rows, %d seeds each)" % [lesion_rows.size(), int(lesion_rows[0].get("n", nseeds))] if not lesion_rows.is_empty() else "LESION TABLE: empty. Set a lesion and press RUN LESION.", small, Kit.TEXT if not lesion_rows.is_empty() else Kit.TEXT_SOFT, true))
	if Lesion.baseline_flaky(lesion_rows): insp_body.add_child(Kit.label("The intact circuit fails on some seeds: a few points between rows are noise, not the lesion.", small, Kit.AMBER, true))
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
		var pt := Lesion.pass_text(r)
		vb.add_child(Kit.label(str(r["verdict"]) + ("  (" + pt + " passed)" if pt != "" else ""), 15, Kit.TEXT_SOFT, true))
		var bio := str(r.get("bio", ""))   # what this lesion means in the body: the expected biology of the task's lesion table
		if bio != "":
			var nm := str(r.get("name", ""))
			vb.add_child(Kit.label(("%s: %s" % [nm, bio]) if nm != "" else bio, 15, Kit.AMBER if bool(r.get("exact", false)) else Kit.TEXT_SOFT, true))
		var sc := float(r["score"]); var d := "" if base_score < 0.0 or (r["lesions"] as Array).is_empty() else ("  %+d" % int(roundf(sc - base_score)))
		var sv := VBoxContainer.new(); sv.size_flags_vertical = Control.SIZE_SHRINK_CENTER; sv.add_theme_constant_override("separation", 0); h.add_child(sv)
		var sl := Kit.shout("%d%s" % [int(roundf(sc)), d], 22 if not Layout.phone else 18, Kit.TEXT); sl.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT; sv.add_child(sl)
		if int(r.get("n", 1)) > 1:
			var rl := Kit.label("%d-%d" % [int(roundf(float(r["lo"]))), int(roundf(float(r["hi"])))], 14, Kit.TEXT_SOFT); rl.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT; sv.add_child(rl)
	if not lesion_rows.is_empty():
		var ex := HFlowContainer.new(); ex.add_theme_constant_override("h_separation", 8); ex.add_theme_constant_override("v_separation", 6); insp_body.add_child(ex)
		ex.add_child(Kit.btn("COPY MARKDOWN", func(): _lesion_export("md"), "panel", 18)); ex.add_child(Kit.btn("COPY CSV", func(): _lesion_export("csv"), "panel", 18)); ex.add_child(Kit.btn("SAVE FILES", func(): _lesion_export("save"), "green", 18))
	if _lesion_note != "": insp_body.add_child(Kit.label(_lesion_note, small, Kit.GREEN, true))

# ---------- v0.3 lab play: mode and variant, hints, verdict, check, skip, surprise, celebration ----------
## The row under the top bar (the phone's MENU holds the same controls): difficulty mode, challenge variant, instant check, skip, hints, results.
func _play_bar() -> Control:
	var h := HFlowContainer.new(); h.add_theme_constant_override("h_separation", 8); h.add_theme_constant_override("v_separation", 6)
	_play_controls(h, false)
	if status != null:   # the run read-out sits at the end of this row: the top bar has no room for it once the mode controls are there
		if status.get_parent() != null: status.get_parent().remove_child(status)
		status.custom_minimum_size.x = 260.0; status.clip_text = true; h.add_child(status)
	return h

func _play_controls(h: Control, phone: bool) -> void:
	if task_id == "": return
	var sz := 18; var dxm := _is_dx()
	h.add_child(Kit.stepper([["guided", "GUIDED"], ["standard", "STANDARD"], ["researcher", "RESEARCHER"]], mode, func(m): _set_mode(str(m)), 250.0 if not phone else 220.0, 18, 40.0))
	var vs: Dictionary = Campaign.info(task_id).get("variants", {})
	if not vs.is_empty() and not dxm:
		var opts := [["base", "BASE TASK"]]
		for k in vs: opts.append([str(k), str(vs[k])])
		h.add_child(Kit.stepper(opts, variant if variant != "" else "base", func(v): _set_variant("" if str(v) == "base" else str(v)), 320.0 if not phone else 260.0, 18, 40.0))
	if not dxm:
		_check_btn = Kit.btn("CHECK", _check_start, "green", sz); h.add_child(_check_btn)
	_skip_btn = Kit.btn("SKIP", _skip_event, "panel", sz); h.add_child(_skip_btn)
	_hint_btn = Kit.btn("HINT", _hint_toggle, "amber", sz); h.add_child(_hint_btn)
	if not dxm:
		_results_btn = Kit.btn("RESULTS", _histo_show, "cyan", sz); h.add_child(_results_btn)
	_hint_btn_sync()

func _set_mode(m: String) -> void:
	if m == mode: return
	mode = m; Progress.set_mode(m)
	_hint_btn_sync(); if _hint_open: _hint_render()
	_reload(); _build_inspector()

func _set_variant(v: String) -> void:
	if v == variant: return
	variant = v; _surprise_done.clear(); _reload(); _build_inspector()
	if task_id != "": Playlog.event("variant", {"task": task_id, "variant": variant})

## The hint button's label and visibility: it counts the failed runs until the ladder opens, and is gone in Researcher mode.
func _hint_btn_sync() -> void:
	if _hint_btn == null or not is_instance_valid(_hint_btn): return
	var fails := int(_fails.get(task_id, 0)); var shown := int(_hint_tier.get(task_id, 0))
	_hint_btn.visible = mode != "researcher"
	_hint_btn.text = HintPanel.button_text(mode, fails, shown)
	Kit.skin(_hint_btn, "amber" if HintPanel.available(mode, fails) else "ghost", 12)
	if mode == "researcher" and _hint_open: _hint_close()

func _task_hints() -> Array:
	var out := []
	if tele != null and tele.task != null and tele.task.has_method("hints"):
		for h in tele.task.hints(): out.append({"tier": int(h.get("tier", 0)), "text": str(h.get("text", "")), "target": str(h.get("target", ""))})
	if out.is_empty() and OS.get_environment("SP_FAKEHINTS") != "" and not (circuit.get("populations", []) as Array).is_empty():   # dev hook: hints for tasks the engine has none for yet
		var pid := str(circuit["populations"][0].get("id", ""))
		out = [{"tier": 1, "text": "This needs inhibition between the two sides: when one side fires, the other must be held quiet.", "target": ""},
			{"tier": 2, "text": "Look at the group %s: what drives it, and what should hold it back?" % pid, "target": pid},
			{"tier": 3, "text": "Add an inhibitory neuron after %s and connect it back onto the opposite side with a negative weight." % pid, "target": pid}]
	return out

func _hint_toggle() -> void:
	if _hint_open: _hint_close(); return
	if mode == "researcher": return
	_hint_open = true
	var live := _task_hints()
	if not live.is_empty(): _hints_cache = live
	_hint_render()

func _hint_close() -> void:
	_hint_open = false
	if _hint_panel != null and is_instance_valid(_hint_panel): _hint_panel.queue_free()
	_hint_panel = null

func _hint_render() -> void:
	if _hint_panel == null or not is_instance_valid(_hint_panel):
		_hint_panel = HintPanel.new()
		_hint_panel.next_pressed.connect(_hint_next); _hint_panel.close_pressed.connect(_hint_close)
		if Layout.phone:
			ov.add_child(_hint_panel); _hint_panel.set_anchors_preset(Control.PRESET_TOP_LEFT); _hint_panel.offset_left = 14.0; _hint_panel.offset_top = 52.0; _hint_panel.custom_minimum_size.x = 400.0
			_hint_panel.max_h = maxf((ov.size.y if ov.size.y > 100.0 else get_viewport_rect().size.y) - 52.0 - 8.0, 120.0)   # the tiers scroll inside the card so tier 3 is reachable on a short screen
		else:
			panels["circuit"].add_child(_hint_panel); panels["circuit"].move_child(_hint_panel, 1)
	_hint_panel.show_state(mode, _hints_cache, int(_hint_tier.get(task_id, 0)), int(_fails.get(task_id, 0)))
	_hint_btn_sync()

func _hint_next() -> void:
	var live := _task_hints()
	if not live.is_empty(): _hints_cache = live
	var by := {}
	for h in _hints_cache:
		if not by.has(int(h["tier"])): by[int(h["tier"])] = h
	var nxt := int(_hint_tier.get(task_id, 0)) + 1
	while nxt <= 3 and not by.has(nxt): nxt += 1
	if by.is_empty() or nxt > 3: _hint_render(); return
	_hint_tier[task_id] = nxt; Progress.record_hint(task_id, nxt); GameAudio.event("hint")
	var tgt := str(by[nxt].get("target", ""))
	if tgt != "" and nxt >= 2: editor.hint_target = tgt; editor.queue_redraw()
	_hint_render()

## An episode (or a check, or a TEST) ended without a pass: count it toward the Standard hint ladder, cache the task's hints for this score state, light up the guilty part.
func _note_fail() -> void:
	if task_id == "" or _is_reference() or Circuits.neuron_count(circuit) == 0 or _is_dx(): return
	var before := int(_fails.get(task_id, 0)); var was := HintPanel.available(mode, before)
	_fails[task_id] = before + 1
	var h := _task_hints()
	if not h.is_empty(): _hints_cache = h
	_verdict_show()
	GameAudio.event("fail")
	if mode == "standard" and not was and HintPanel.available(mode, before + 1) and celebration != null:
		celebration.toast("Hints unlocked", "Two runs have missed. The HINT button now gives a nudge: first the idea, then the place, then a partial recipe.", Kit.AMBER, 6.0)
	_hint_btn_sync(); if _hint_open: _hint_render()

# ----- verdict highlight -----
## The element of the circuit a failed score is about: {target, part}. The task's own tier-2 hint target is the authority; failing that, a score part that names a channel
## or population; failing that, the motor channel that fires least (a silent output) or, for a part that counts false triggers, the one that fires most.
func _guilty() -> Dictionary:
	var out := {"target": "", "part": ""}
	var sc: Dictionary = tele.score if tele != null else {}
	var parts: Dictionary = sc.get("parts", {})
	for h in _hints_cache:
		var t := str(h.get("target", ""))
		if int(h.get("tier", 0)) == 2 and t != "" and editor._element_exists(t): out["target"] = t; break
	var pops: Array = circuit.get("populations", [])
	for k in parts:
		var low := str(k).to_lower()
		for ch in circuit.get("outputs", []) + circuit.get("inputs", []):
			var nm := str(ch.get("name", "")).to_lower()
			if nm != "" and nm in low:
				if out["part"] == "": out["part"] = str(k)
				if out["target"] == "": out["target"] = ("out:" if circuit.get("outputs", []).has(ch) else "in:") + str(ch["name"])
		for p in pops:
			var id := str(p.get("id", "")).to_lower()
			if id.length() >= 2 and id in low:
				if out["part"] == "": out["part"] = str(k)
				if out["target"] == "": out["target"] = str(p["id"])
	if out["target"] == "" and tele != null and tele.sim != null:
		var act := Circuits.pop_activity(circuit, tele.count)
		var over := false
		for k in parts:
			var low := str(k).to_lower()
			if ("false" in low or "spurious" in low or "extra" in low) and not (parts[k] is String) and float(parts[k]) > 0.0: over = true; if out["part"] == "": out["part"] = str(k)
		var best_ch := ""; var best_v := 1.0e12 if not over else -1.0
		for ch in circuit.get("outputs", []):
			var v := 0.0
			for x in ch.get("neurons", []): v += float(act.get(Circuits.proj_pop(x), 0))
			if (over and v > best_v) or (not over and v < best_v): best_v = v; best_ch = str(ch.get("name", ""))
		if best_ch != "": out["target"] = "out:" + best_ch
		if out["part"] == "":   # name the worst-looking part chip: the first numeric one
			for k in parts:
				if not (parts[k] is String): out["part"] = str(k); break
	return out

func _verdict_show() -> void:
	var g := _guilty()
	editor.verdict_target = str(g["target"]); score_card.guilty = str(g["part"]); editor.queue_redraw()

# ----- instant check -----
func _check_start() -> void:
	if task_id == "" or tele == null or tele.task == null or _check_job != 0: return
	if Circuits.neuron_count(circuit) == 0 or tele.wiring_issue() != "": _banner_raw(tele.wiring_issue() if tele != null and tele.wiring_issue() != "" else "Add a neuron first."); return
	if not ClassDB.class_has_method("SpikeTask", "score_headless_async"):
		celebration.toast("Instant check is not in this build", "Rebuild the extension (scripts/build-gd.sh) to get headless scoring.", Kit.RED, 4.0); return
	_check_task = Campaign.make(task_id)
	if _check_task == null: return
	var cseed := int(circuit.get("seed", 1)); var cj := Circuits.to_json(Telemetry.sanitized(_live_circuit()))
	var nargs := 0
	for m in _check_task.get_method_list():
		if str(m["name"]) == "score_headless_async": nargs = (m["args"] as Array).size()
	if nargs >= 5: _check_job = int(_check_task.score_headless_async(cj, PackedInt64Array([cseed]), mode, variant, cseed))   # extension with a circuit-seed argument: the live world and circuit seeds
	else: _check_job = int(_check_task.score_headless_async(cj, PackedInt64Array([cseed]), mode, variant))
	_check_t0 = Time.get_ticks_msec()
	if _check_job == 0: _check_task = null; celebration.toast("Check could not start", "The headless scorer refused the circuit.", Kit.RED, 4.0); return
	if _check_btn != null and is_instance_valid(_check_btn): _check_btn.text = "CHECKING..."; _check_btn.disabled = true
	Playlog.event("check", {"task": task_id})

## Stop a running instant check (leaving the lab, or a new check): the worker process is killed when the extension can, else the result is just dropped.
func _check_cancel() -> void:
	if _check_job != 0 and _check_task != null:
		if _check_task.has_method("headless_cancel") or ClassDB.class_has_method("SpikeTask", "headless_cancel"): _check_task.call("headless_cancel", _check_job)
	_check_job = 0; _check_task = null

var _check_toast: PanelContainer = null
var _check_lbl: Label = null
## While the async check runs: a card under the header with the seed count and the elapsed time (the worker reports no finer progress than done / not done).
func _check_progress(on: bool) -> void:
	if not on:
		if _check_toast != null and is_instance_valid(_check_toast): _check_toast.queue_free()
		_check_toast = null; _check_lbl = null; return
	if _check_toast == null or not is_instance_valid(_check_toast):
		_check_toast = PanelContainer.new(); _check_toast.mouse_filter = Control.MOUSE_FILTER_IGNORE
		_check_toast.add_theme_stylebox_override("panel", Kit.sb(Kit.PANEL_HI, Kit.CYAN, 2, 12, 12 if Layout.phone else 16, true))
		_check_lbl = Kit.label("", 17 if not Layout.phone else 15, Kit.TEXT, true); _check_lbl.mouse_filter = Control.MOUSE_FILTER_IGNORE; _check_toast.add_child(_check_lbl)
		ov.add_child(_check_toast); _check_toast.z_index = 61
		_check_toast.set_anchors_preset(Control.PRESET_TOP_LEFT); _check_toast.offset_left = 240.0; _check_toast.offset_top = 56.0; _check_lbl.custom_minimum_size.x = 380.0
	_check_lbl.text = "CHECK running... 1/1 seed, %.1f s" % ((Time.get_ticks_msec() - _check_t0) * 0.001)

func _check_poll() -> void:
	if _check_job == 0 or _check_task == null: return
	var r: Dictionary = _check_task.score_headless_poll(_check_job)
	if not bool(r.get("done", false)): _check_progress(true); return
	_check_progress(false)
	var ms := float(r.get("ms", float(Time.get_ticks_msec() - _check_t0)))
	_check_job = 0; _check_task = null
	if _check_btn != null and is_instance_valid(_check_btn): _check_btn.text = "CHECK"; _check_btn.disabled = false
	if not bool(r.get("ok", false)): celebration.toast("Check failed", str(r.get("error", "the headless run did not finish")), Kit.RED, 5.0); return
	var passed := bool(r.get("passed", false)); var val := int(roundf(float(r.get("value", 0.0))))
	var runs: Array = r.get("runs", []); var verdict := str((runs[0] as Dictionary).get("verdict", "")) if not runs.is_empty() else ""
	celebration.toast(("CHECK: PASSED  %d" if passed else "CHECK: NOT PASSED  %d") % val, "%s  (%.1f s)" % [verdict, ms * 0.001], Kit.GREEN if passed else Kit.RED, 6.0)
	GameAudio.event("pass" if passed else "fail")
	if score_card != null: score_card.check_text = "check %d%s" % [val, "" if passed else " (miss)"]
	if not passed and not _is_reference(): _note_fail()
	elif passed and not lesion_on: _record_check(val)

## A passing check counts as a pass (it ran the task's own seed), but not as the end-of-episode five-seed record.
func _record_check(val: int) -> void:
	if _is_reference() or variant != "" or _rec_pass: return
	_record(true, false, float(val), -1.0)

# ----- skip to the next scripted event -----
func _skip_event() -> void:
	if tele == null or tele.task == null or tele.sim == null or tele.finished: return
	var nx := -1.0
	if tele.task.has_method("next_event_ms"): nx = float(tele.task.next_event_ms(tele.t_ms))
	if nx < 0.0: celebration.toast("No more events", "Nothing else is scripted in this episode: it just runs to the end. Raise the speed to get there sooner.", Kit.TEXT_SOFT, 3.0); return
	var label := ""
	for e in tele.task.timeline():
		if absf(float(e.get("t_ms", -1.0)) - nx) < 1.5: label = str(e.get("label", ""))
	tele.skip_until(maxf(nx - 150.0, tele.t_ms + 1.0)); _sync_run_btn()
	celebration.toast("Skipping to %s" % (label if label != "" else "the next event"), "t = %.1f s" % (nx * 0.001), Kit.CYAN, 2.0)

# ----- surprise: an unintended rhythm -----
func _surprise_tick(dt: float) -> void:
	_surprise_clock += dt
	if _surprise_clock < 0.8: return
	_surprise_clock = 0.0
	if task_id == "" or tele == null or tele.sim == null or tele.finished or not tele.running or not _test.is_empty() or _lrun != null or _surprise_done.has("osc") or _is_dx(): return
	var info := Campaign.info(task_id)
	if Surprise.is_rhythm_task(task_id, (info.get("concepts_new", []) as Array) + (info.get("concepts_used", []) as Array)): return
	var r := Surprise.rhythm(tele)
	if not bool(r["found"]): return
	_surprise_done["osc"] = true
	var tx := Surprise.oscillation_text(r)
	GameAudio.event("celebrate")
	celebration.toast(str(tx["title"]), str(tx["text"]), Kit.GREEN, 9.0)
	Playlog.event("surprise", {"task": task_id, "period_ms": r["period_ms"], "pop": r["pop"]})

# ----- first-pass celebration -----
func _celebrate() -> void:
	var sens := Array(tele.task.sensor_channels()) if tele.task != null else []
	var mot := Array(tele.task.motor_channels()) if tele.task != null else []
	var path := Circuits.critical_path(circuit, Circuits.pop_activity(circuit, tele.count), sens, mot)
	celebration.first_pass(editor, tele, path, Campaign.notebook(task_id))
	Playlog.event("celebrate", {"task": task_id})

# ----- post-solve histograms -----
## The three axes of the post-solve card: neurons, energy (mean spikes/s per neuron, the ATP bill) and the task's own quality metric.
func _metric_rows() -> Array:
	var nn := Circuits.neuron_count(circuit)
	var hz := tele.spike_rate_hz() / maxf(float(nn), 1.0) if tele != null and tele.sim != null else 0.0
	var rows := [{"id": "neurons", "label": "Neurons", "value": float(nn), "unit": "neurons", "lower": true},
		{"id": "energy_hz", "label": "Energy", "value": hz, "unit": "spikes/s/neuron", "lower": true}]
	var found := false
	if tele != null and tele.task != null and tele.task.has_method("metrics"):
		var m: Dictionary = tele.task.metrics()
		for k in m:
			if str(k) in ["neurons", "energy_hz"] or found: continue
			var d: Dictionary = m[k]
			rows.append({"id": str(k), "label": BodyViews.pretty(str(k))[0] if BodyViews.pretty(str(k)).size() > 0 else str(k).capitalize(), "value": float(d.get("value", 0.0)), "unit": str(d.get("unit", "")), "lower": bool(d.get("lower_is_better", true))}); found = true
	if not found: rows.append({"id": "score", "label": "Task score", "value": float(tele.score.get("value", 0.0)) if tele != null else 0.0, "unit": "points", "lower": false})
	return rows

func _histo_hide() -> void:
	if _histo_ov != null and is_instance_valid(_histo_ov): _histo_ov.queue_free()
	_histo_ov = null

func _histo_show() -> void:
	if task_id == "" or tele == null or _is_dx(): return
	_histo_hide(); _close_menu(); _overlay_hide(); _overlay_dismissed = true
	var phone := Layout.phone
	var root := Control.new(); root.set_anchors_preset(Control.PRESET_FULL_RECT); root.mouse_filter = Control.MOUSE_FILTER_STOP; ov.add_child(root); _histo_ov = root
	var dim := ColorRect.new(); dim.color = Color(Kit.BENCH, 0.78); dim.set_anchors_preset(Control.PRESET_FULL_RECT); root.add_child(dim)
	var pn := PanelContainer.new(); pn.add_theme_stylebox_override("panel", Kit.sb(Kit.PANEL, Kit.LINE, 1, 16, 12 if phone else 20, true)); root.add_child(pn)
	pn.set_anchors_preset(Control.PRESET_FULL_RECT); var mg := 14 if phone else int(70 * f()); pn.offset_left = mg; pn.offset_right = -mg; pn.offset_top = mg * 0.5; pn.offset_bottom = -mg * 0.5
	if not phone: pn.set_anchors_preset(Control.PRESET_CENTER); pn.grow_horizontal = Control.GROW_DIRECTION_BOTH; pn.grow_vertical = Control.GROW_DIRECTION_BOTH; pn.offset_left = -float(size.x) * 0.45; pn.offset_right = float(size.x) * 0.45; pn.offset_top = 0.0; pn.offset_bottom = 0.0
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", 6 if phone else 10); pn.add_child(v)
	var hd := HBoxContainer.new(); hd.add_theme_constant_override("separation", 12); v.add_child(hd)
	var t := Kit.shout("How does it compare?", 32 if not phone else 22, Kit.TEXT); t.size_flags_horizontal = Control.SIZE_EXPAND_FILL; hd.add_child(t)
	hd.add_child(Kit.btn("CLOSE", _histo_hide, "panel", 20 if not phone else 18))
	var raw := HistogramView.load_file(task_id)
	var passed := bool(tele.score.get("passed", false)) or bool(Progress.best(task_id)["passed"])
	v.add_child(Kit.label(("Your run against other solutions of this task, on three axes. Beat the median on any one of them." if not raw.is_empty() else "No solution distribution exists for this task yet (it is generated offline from developer and AI solutions). Your numbers are below; pin a target to chase."), 17 if not phone else 15, Kit.TEXT_SOFT, true))
	if not passed: v.add_child(Kit.label("This run has not passed yet: the numbers are for the run so far.", 16, Kit.AMBER, true))
	var sc := ScrollContainer.new(); sc.size_flags_vertical = Control.SIZE_EXPAND_FILL; sc.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED; sc.custom_minimum_size.y = 0.0 if phone else 190.0; v.add_child(sc)
	var fl := HFlowContainer.new(); fl.size_flags_horizontal = Control.SIZE_EXPAND_FILL; fl.add_theme_constant_override("h_separation", 14); fl.add_theme_constant_override("v_separation", 10); sc.add_child(fl)
	var wins := 0
	for r in _metric_rows():
		var hv := HistogramView.new().setup(task_id, str(r["id"]), str(r["label"]), float(r["value"]), str(r["unit"]), bool(r["lower"]), (raw.get(str(r["id"]), {}) as Dictionary) if raw.get(str(r["id"]), {}) is Dictionary else {})
		hv.custom_minimum_size.x = 300.0 if phone else 440.0; hv.size_flags_horizontal = Control.SIZE_EXPAND_FILL; fl.add_child(hv)
		if HistogramView.beats_median(hv.dist, float(r["value"]), bool(r["lower"])): wins += 1
	if wins > 0: v.add_child(Kit.label("You beat the median on %d of 3 axes." % wins, 19, Kit.GREEN, true))
	Playlog.event("histogram", {"task": task_id})

# ---------- diagnosis cases ----------
func _inspect_dx(small: int) -> void:
	if Layout.phone:
		var hb := Kit.btn("HIDE CASE", func(): _brief_forced = false; _build_inspector(), "panel", 16); hb.size_flags_horizontal = Control.SIZE_SHRINK_BEGIN; insp_body.add_child(hb)
	var note := Campaign.notebook(task_id)
	var panel := DiagnosisPanel.new()
	panel.picked.connect(func(c): _dx["picked"] = c; _build_inspector())
	panel.submit_pressed.connect(_dx_submit); panel.reveal_pressed.connect(_dx_reveal); panel.lesion_lab_pressed.connect(_lesion_toggle)
	insp_body.add_child(panel.setup(_dx_info, _dx, note if (bool(_dx.get("solved", false)) or bool(_dx.get("gave_up", false))) else {}, lesion_on))

## A population or axon picked in a diagnosis case: read-only facts, a TRACE button, a way back to the case card.
func _inspect_dx_pick(sel: Dictionary, small: int) -> void:
	var id := str(sel.get("id", sel.get("name", "")))
	insp_body.add_child(_head(id if str(sel["t"]) != "proj" else "axon", Kit.exc(), "read-only: this is the patient's circuit"))
	if str(sel["t"]) == "pop":
		var pi := Circuits.pop_index(circuit, id)
		if pi >= 0:
			var p: Dictionary = circuit["populations"][pi]
			insp_body.add_child(Kit.label("%s, %d cell%s. Trace it to watch its membrane voltage, or select it and listen: only its spikes click." % [str(p.get("model", "")), Circuits.pop_n(p), "" if Circuits.pop_n(p) == 1 else "s"], small, Kit.TEXT_SOFT, true))
			insp_body.add_child(Kit.btn("TRACE", func(): _toggle_trace(id), "cyan" if tele.watched.has(Circuits.pop_first(circuit, pi)) else "panel", 18))
	elif str(sel["t"]) == "proj":
		var i := int(sel["i"])
		if i < circuit["projections"].size():
			var pr: Dictionary = circuit["projections"][i]; var w := Circuits.proj_weight(pr)
			insp_body.add_child(Kit.label("%s > %s, %s, weight %s, delay %s ms." % [Circuits.proj_pop(pr.get("pre", "")), Circuits.proj_pop(pr.get("post", "")), "inhibitory" if w < 0.0 else "excitatory", _num(absf(w)), _num(Circuits.proj_delay(pr))], small, Kit.TEXT_SOFT, true))
	insp_body.add_child(Kit.btn("BACK TO THE CASE", func(): editor.select({}); _brief_forced = true; _build_inspector(), "ghost", 18))

func _dx_submit() -> void:
	var cand := str(_dx.get("picked", ""))
	if cand == "" or tele == null or tele.task == null or not tele.task.has_method("set_variant"): return
	tele.task.set_variant(cand)   # the diagnosis tasks take the answer through the variant slot (engine/src/tasks/dx_common.h)
	var sc: Dictionary = tele.task.score(); var ok := bool(sc.get("passed", false))
	Playlog.event("diagnosis", {"task": task_id, "answer": cand, "correct": ok})
	if ok:
		_dx["solved"] = true; editor.verdict_target = Circuits.candidate_focus(cand); editor.verdict_ok = true
		var first := not bool(Progress.best(task_id)["passed"])
		Progress.record_result(task_id, 100.0, {"passed": true, "five_seeds": int(_dx.get("tries", 0)) == 0, "neurons": Circuits.neuron_count(circuit), "rate_hz": -1.0, "reference_derived": false})
		GameAudio.event("pass")
		if first: celebration.first_pass(editor, tele, Circuits.critical_path(circuit, Circuits.pop_activity(circuit, tele.count), Array(tele.task.sensor_channels()), Array(tele.task.motor_channels())), Campaign.notebook(task_id))
	else:
		_dx["tries"] = int(_dx.get("tries", 0)) + 1; GameAudio.event("fail")
		var wrong := Circuits.candidate_focus(cand)
		if wrong != "" and _hint_open: _hint_render()
	_build_inspector(); _scroll_end.call_deferred()

## Bring the bottom of the inspector (a diagnosis reveal) into view once the new content is laid out.
func _scroll_end() -> void:
	await get_tree().process_frame; await get_tree().process_frame
	if insp_scroll != null and is_instance_valid(insp_scroll): insp_scroll.scroll_vertical = int(insp_scroll.get_v_scroll_bar().max_value)

func _dx_reveal() -> void:
	_dx["gave_up"] = true
	if tele != null and tele.task != null and tele.task.has_method("set_variant"): tele.task.set_variant(str(_dx_info.get("answer", "")))
	editor.verdict_target = Circuits.candidate_focus(str(_dx_info.get("answer", ""))); editor.verdict_ok = false
	_build_inspector(); _scroll_end.call_deferred()

# ---------- dev hooks for screenshots ----------
## SP_HINT=1|2|3 opens the hint card with that many tiers shown (with SP_FAKEHINTS=1 for tasks the engine has no hints for), SP_VERDICT=1 lights the guilty element,
## SP_HISTO=1 opens the histograms, SP_CELEB=1 runs the first-pass celebration, SP_DXPICK=<candidate> picks and SP_DXSUBMIT=1 submits in a diagnosis case,
## SP_FAILS=n sets the failed-run count, SP_CHECK=1 presses CHECK, SP_SKIP=1 presses SKIP.
func _demo_hooks() -> void:
	if OS.get_environment("SP_FAILS") != "": _fails[task_id] = int(OS.get_environment("SP_FAILS"))
	if OS.get_environment("SP_HINT") != "":
		await get_tree().create_timer(0.8).timeout
		_hints_cache = _task_hints(); _hint_open = true; _hint_tier[task_id] = 0
		for i in int(OS.get_environment("SP_HINT")): _hint_next()
		_hint_render()
	if OS.get_environment("SP_VERDICT") != "":
		await get_tree().create_timer(1.0).timeout
		_hints_cache = _task_hints(); _verdict_show()
	if OS.get_environment("SP_HISTO") != "":
		await get_tree().create_timer(1.2).timeout
		_histo_show()
	if OS.get_environment("SP_CELEB") != "":
		await get_tree().create_timer(1.0).timeout
		_celebrate()
	if OS.get_environment("SP_CHECK") != "":
		await get_tree().create_timer(1.0).timeout
		_check_start()
	if OS.get_environment("SP_SKIP") != "":
		await get_tree().create_timer(1.0).timeout
		_skip_event()
	if OS.get_environment("SP_DXPICK") != "":
		await get_tree().create_timer(0.8).timeout
		_dx["picked"] = OS.get_environment("SP_DXPICK"); _build_inspector()
		if OS.get_environment("SP_DXSUBMIT") != "": _dx_submit()
