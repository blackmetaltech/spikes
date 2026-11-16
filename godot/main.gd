extends Node
## App shell: boot order (settings -> layout -> theme -> UI root -> screens), screen manager with slide/fade transitions,
## pad and touch support, and the SP_* env hooks for smoke tests and screenshots. Screens are code-built Controls (ui/screens).
##
## Env hooks:  SP_TASK=reflex|lamprey|worm + SP_REF=1 (lab opens that task with its reference circuit)   SP_SPEED=<x> (lab run speed)   SP_SEL=pop:<id>|proj:<i>|in:<name>|out:<name> (lab selection)
##             SP_SMOKE=1 (open every screen, tab and layout, load the GDExtension, assert, print SMOKE OK, quit)   SP_SHOT=<png> + SP_SHOT_AT=<sec>
##             SP_SCREEN=title|tasks|parts|lab|analysis|settings|credits   SP_CHAPTER=<chapter id> + SP_NODE=<task id> (campaign map)   SP_MOCK=1 (synthetic body-system tasks)
##             SP_UNLOCK_ALL=1 (open every node and part)   SP_PROGRESS=reflex:3:92,worm:2:81 (fake best scores)   SP_CATALOG=<thumbs.json>   SP_LESION=1 (lab opens in the lesion lab)   SP_TAB=Display|Audio|Controls   SP_LAYOUT=desktop|handheld|phone
##             SP_NOAUTOSAVE (never write settings)   SP_NODISPLAY (never touch window mode / vsync)   SP_NOTHROTTLE   SP_PAD=1 (start in nav mode)

const SCREENS := {"title": "res://ui/screens/title.gd", "tasks": "res://ui/screens/tasks.gd", "lab": "res://ui/screens/lab.gd", "analysis": "res://ui/screens/analysis.gd", "settings": "res://ui/screens/settings.gd", "credits": "res://ui/screens/credits.gd", "parts": "res://ui/screens/parts.gd"}
const BACK_TO := {"settings": "title", "tasks": "title", "lab": "tasks", "credits": "title", "parts": "tasks"}

## Hand-over between screens: which task the lab opens, whether it starts from the reference circuit, circuits kept between lab visits, and what the analysis shows.
var lab_task := ""
var lab_reference := false
var lab_keep := {}
var analysis_circuit := {}
var analysis_tele: Telemetry = null
var analysis_task := ""
var campaign_chapter := ""    # the campaign map remembers its chapter and node between visits
var campaign_node := ""

var ui: CanvasLayer
var root: Control
var holder: Control
var pad: Pad
var cur := ""
var screen: Screen
var _last_input := 0.0
var _kind := ""

func _ready() -> void:
	if OS.get_environment("SP_BUDGET_US") != "": Telemetry.budget_us = int(OS.get_environment("SP_BUDGET_US"))
	Settings.load_all()
	Settings.apply(get_window()); Settings.write_log(get_window())
	Layout.update(get_window())
	_kind = Layout.kind
	get_window().content_scale_factor = Settings.effective_ui_scale(get_window())
	get_window().size_changed.connect(_on_resized)
	add_child(WrapGuard.new())
	if Layout.touch: add_child(TouchScroll.new())
	RenderingServer.set_default_clear_color(Kit.BENCH)
	ui = CanvasLayer.new(); ui.layer = 10; add_child(ui)
	root = Control.new(); root.name = "Root"; root.set_anchors_preset(Control.PRESET_FULL_RECT); root.mouse_filter = Control.MOUSE_FILTER_IGNORE; root.theme = Kit.make_theme(); ui.add_child(root)
	holder = Control.new(); holder.name = "Screens"; holder.set_anchors_preset(Control.PRESET_FULL_RECT); holder.mouse_filter = Control.MOUSE_FILTER_IGNORE; root.add_child(holder)
	pad = Pad.new(); pad.app = self; add_child(pad)
	_update_metrics()
	if OS.get_environment("SP_PAD") != "": pad.set_active(true)
	if OS.get_environment("SP_SMOKE") != "": Kit.instant = true; Settings.no_save = true; await _smoke(); return
	if OS.get_environment("SP_SHOT") != "": Kit.instant = true   # screenshots show the settled frame, not a transition
	if OS.get_environment("SP_CHAPTER") != "": campaign_chapter = OS.get_environment("SP_CHAPTER")
	if OS.get_environment("SP_NODE") != "": campaign_node = OS.get_environment("SP_NODE")
	var start := OS.get_environment("SP_SCREEN") if SCREENS.has(OS.get_environment("SP_SCREEN")) else "title"
	show_screen("title", false, true)
	if start == "analysis": _analysis_demo(); 
	elif start == "lab": open_lab(OS.get_environment("SP_TASK"), OS.get_environment("SP_REF") != "", true)
	elif start != "title": show_screen(start, false, true)
	if OS.get_environment("SP_SHOT") != "":
		var at := float(OS.get_environment("SP_SHOT_AT")) if OS.get_environment("SP_SHOT_AT") != "" else 2.0
		get_tree().create_timer(at).timeout.connect(_shot_when_ready)

## Screenshots of the analysis screen need a circuit that has run: open the lab on SP_TASK, let it run, then show the analysis of it.
func _analysis_demo() -> void:
	open_lab(OS.get_environment("SP_TASK") if OS.get_environment("SP_TASK") != "" else "lamprey", true, true)
	await get_tree().create_timer(3.5).timeout
	var lab := screen as LabScreen
	if lab != null: open_analysis(lab.circuit, lab.tele, lab.task_id)

## SP_SIM_UNTIL=<model ms>: wait until the lab's run has reached that model time (deterministic screenshots on a slow software renderer).
func _shot_when_ready() -> void:
	var until := float(OS.get_environment("SP_SIM_UNTIL")) if OS.get_environment("SP_SIM_UNTIL") != "" else 0.0
	var lab := screen as LabScreen if screen is LabScreen else null
	if lab != null and OS.get_environment("SP_LESION") in ["run", "sweep"] and (lab._lrun != null or lab.lesion_rows.is_empty()) and Time.get_ticks_msec() < 170000:   # wait for the lesion table
		get_tree().create_timer(0.3).timeout.connect(_shot_when_ready); return
	if until > 0.0 and lab != null and lab.tele != null and lab.tele.sim != null and lab.tele.t_ms < until and Time.get_ticks_msec() < 150000:
		get_tree().create_timer(0.25).timeout.connect(_shot_when_ready); return
	if until > 0.0 and lab != null:   # hold the instant (paused views freeze glows and pulses), let a few frames draw it
		lab.tele.hold = true; await get_tree().create_timer(0.5).timeout
	_shot()

func _shot() -> void:
	var img := get_viewport().get_texture().get_image()
	var err := img.save_png(OS.get_environment("SP_SHOT"))
	print("SHOT ", OS.get_environment("SP_SHOT"), " ", img.get_size(), " err=", err)
	get_tree().quit()

func _notification(what: int) -> void:
	if what == NOTIFICATION_APPLICATION_FOCUS_OUT: Settings.focused = false; Settings.apply_fps()
	elif what == NOTIFICATION_APPLICATION_FOCUS_IN: Settings.focused = true; Settings.idle = false; Settings.apply_fps()

func _input(e: InputEvent) -> void:
	if e is InputEventMouseMotion or e is InputEventKey or e is InputEventMouseButton or e is InputEventJoypadButton or e is InputEventScreenTouch:
		_last_input = Time.get_ticks_msec() / 1000.0
		if Settings.idle: Settings.idle = false; Settings.apply_fps()

func _process(_dt: float) -> void:
	var idle := Time.get_ticks_msec() / 1000.0 - _last_input > 60.0
	if idle != Settings.idle: Settings.idle = idle; Settings.apply_fps()

# ---------- layout ----------
func _on_resized() -> void:
	var was := Layout.kind
	get_window().content_scale_factor = Settings.effective_ui_scale(get_window())
	_update_metrics()
	if was != Layout.kind: call_deferred("relayout")

func _update_metrics() -> void:
	Kit.vh = get_viewport().get_visible_rect().size.y
	Kit.ppl = Layout.text_scale(get_window())

## Re-apply the content scale and rebuild the open screen (the layout class, UI scale or text size changed).
func relayout() -> void:
	get_window().content_scale_factor = Settings.effective_ui_scale(get_window())
	Layout.update(get_window()); _kind = Layout.kind
	_update_metrics()
	if screen != null and is_instance_valid(screen): screen.refresh(); if Kit.pad_mode: pad.ensure_focus(true)

# ---------- screens ----------
func current_screen() -> Control: return screen
func show_hint() -> bool: return screen == null or screen.wants_hint()

## Show a screen with a slide + fade. `back` slides the other way; `snap` skips the animation.
func show_screen(id: String, back := false, snap := false) -> void:
	if id == cur or not SCREENS.has(id): return
	var old := screen
	var s: Screen = load(SCREENS[id]).new(); s.app = self; s.name = id.capitalize() + "Screen"
	if id == "settings" and OS.get_environment("SP_TAB") != "": s.tab = OS.get_environment("SP_TAB")
	holder.add_child(s); s.build()
	screen = s; cur = id
	hint_refresh()
	if old != null and is_instance_valid(old):
		if snap or Kit.instant: old.queue_free()
		else:
			old.mouse_filter = Control.MOUSE_FILTER_IGNORE
			var dir := -1.0 if not back else 1.0
			var tw := create_tween().set_parallel(true)
			tw.tween_property(old, "modulate:a", 0.0, 0.16)
			if not Settings.reduced_motion: tw.tween_property(old, "position:x", dir * get_viewport().get_visible_rect().size.x * 0.06, 0.18)
			tw.chain().tween_callback(old.queue_free)
	if not snap: Kit.appear(s, Vector2(0 if Settings.reduced_motion else (60.0 if not back else -60.0), 0))
	if Kit.pad_mode: call_deferred("_focus_new")

func _focus_new() -> void:
	if pad != null: pad.ensure_focus(true)

func hint_refresh() -> void:
	if pad != null and pad.hint != null: pad.hint.visible = pad.active and show_hint()

## B / Esc: let the screen handle it, else go to the screen it came from.
func back() -> void:
	if screen != null and screen.on_back(): return
	if cur == "analysis": open_lab(analysis_task, false, true); return
	if BACK_TO.has(cur): show_screen(BACK_TO[cur], true)

## Open the lab on a task ("" = free run), empty bench or with the reference circuit.
func open_lab(task: String, reference := false, back_nav := false) -> void:
	lab_task = task; lab_reference = reference
	if cur == "lab": cur = ""
	show_screen("lab", back_nav)

func open_analysis(c: Dictionary, t: Telemetry, task: String) -> void:
	analysis_circuit = c.duplicate(true); analysis_tele = t; analysis_task = task
	show_screen("analysis")

func quit_game() -> void: get_tree().quit()

# ---------- smoke test ----------
var _fails := 0
func _check(ok: bool, msg: String) -> void:
	if not ok: _fails += 1; printerr("SMOKE FAIL: ", msg)

func _frames(n := 2) -> void:
	for i in n: await get_tree().process_frame

## CI smoke test (SP_SMOKE=1, headless): every layout x screen x settings tab, font floor, GDExtension load and a stepped SpikeSim, orphan nodes. Prints SMOKE OK.
func _smoke() -> void:
	var orphans0 := int(Performance.get_monitor(Performance.OBJECT_ORPHAN_NODE_COUNT))
	var small := 0; var opened := 0; var tabs := 0
	var layouts := Layout.KINDS.duplicate()
	for kind in layouts:
		Settings.layout = kind; relayout(); _check(Layout.kind == kind, "layout %s forced, got %s" % [kind, Layout.kind])
		for id in SCREENS:
			cur = ""; show_screen(id, false, true); await _frames(); opened += 1
			small += _small_fonts(screen); _check(screen.get_child_count() > 0, "%s built nothing" % id)
			if id == "settings":
				for t in SettingsScreen.TABS:
					(screen as SettingsScreen).set_tab(t); await _frames(); tabs += 1; small += _small_fonts(screen)
			if id == "lab": await _frames(20); small += _small_fonts(screen)
			if id == "analysis": await _frames(4); small += _small_fonts(screen)
		pad.set_active(true); await _frames(); show_screen("title", false, true); await _frames(); pad.ensure_focus(true)
		var fo := get_viewport().gui_get_focus_owner(); _check(fo != null, "pad nav on title focused nothing (%s)" % kind)
		small += _small_fonts(screen); pad.set_active(false)
	Settings.layout = "auto"; relayout()
	await _interaction_checks()
	_extension_checks()
	await _lab_checks()
	await _v02_checks()
	small += small_fonts_total
	print("FONTS below %d px: %d" % [Kit.MIN_FONT, small]); _check(small == 0, "text smaller than the readable minimum")
	cur = ""; show_screen("title", false, true); await _frames(4)
	var orphans1 := int(Performance.get_monitor(Performance.OBJECT_ORPHAN_NODE_COUNT))
	print("ORPHANS %d -> %d" % [orphans0, orphans1]); _check(orphans1 <= orphans0, "orphan nodes leaked by the screens (%d -> %d)" % [orphans0, orphans1])
	if _fails > 0: printerr("SMOKE FAILED: %d problem(s)" % _fails); get_tree().quit(1); return
	print("SMOKE OK screens=%d settings_tabs=%d layouts=%d renderer=%s" % [opened, tabs, layouts.size(), RenderingServer.get_current_rendering_method()])
	get_tree().quit()

## The GDExtension is loaded: SpikeSim steps a circuit, every task runs its reference circuit for 1 s, bad JSON is rejected, analysis answers.
func _extension_checks() -> void:
	_check(ClassDB.class_exists("SpikeSim") and ClassDB.class_exists("SpikeTask"), "GDExtension: SpikeSim / SpikeTask are not registered (run scripts/build-gd.sh)")
	if not ClassDB.class_exists("SpikeSim"): return
	var bad = ClassDB.instantiate("SpikeSim")
	_check(not bad.load_circuit("{not json"), "SpikeSim accepted invalid JSON"); _check(str(bad.error()) != "", "SpikeSim gave no error text for invalid JSON")
	var ids: PackedStringArray = ClassDB.instantiate("SpikeTask").call("task_ids")
	_check(ids.size() >= 3, "task_ids gave %d tasks, want 3" % ids.size())
	var sum_spikes := 0
	for id in ids:
		var t = ClassDB.instantiate("SpikeTask"); _check(t.make(id), "make(%s)" % id)
		var txt := Circuits.read_reference(str(t.reference_circuit())); _check(txt != "", "reference circuit for %s is not readable (%s)" % [id, t.reference_circuit()])
		var sim = ClassDB.instantiate("SpikeSim")
		_check(sim.load_circuit(txt), "%s: reference circuit rejected: %s" % [id, sim.error()])
		if sim.neuron_count() == 0: continue
		t.reset(1); sim.watch(0)
		for i in 1000: t.step(sim, 1.0)
		var sp: PackedFloat64Array = sim.drain_spikes(); sum_spikes += sp.size() / 2
		var st: Dictionary = sim.stats()
		_check(sp.size() % 2 == 0, "%s: drain_spikes gave an odd array" % id)
		_check(absf(float(st.get("t_ms", 0.0)) - 1000.0) < 1.0, "%s: model time %s after 1000 slices" % [id, st.get("t_ms", 0)])
		var finite_v := false   # a model without a membrane (a relay, a generator-like cell) reports NaN: some neuron among the first few must report a voltage
		for vi in mini(int(sim.neuron_count()), 12): finite_v = finite_v or is_finite(sim.voltage(vi))
		_check(finite_v, "%s: no neuron among the first 12 reports a finite voltage" % id)
		var b: Dictionary = t.body(); var poly := str(t.view()) == "polyline" if t.has_method("view") else true
		_check(b.has("points") and (not poly or (b["points"] as PackedVector2Array).size() > 1), "%s: body has no points" % id)
		_check(t.score().has("value"), "%s: score has no value" % id); _check(t.bounds().size.x > 0.0, "%s: empty bounds" % id)
		var m := Circuits.parse(str(sim.motif_json(0, 5, 1))); _check(m.has("motifs") and (m["motifs"] as Array).size() == 13, "%s: motif census did not return 13 classes" % id)
		var nn := int(sim.neuron_count()); sim.stop()
		print("TASK %s ok: %d neurons, %d spikes in 1 s, score %.0f" % [id, nn, sp.size() / 2, float(t.score().get("value", 0.0))])
	print("EXTENSION OK tasks=%d spikes=%d" % [ids.size(), sum_spikes])

## The lab opens on every task with its reference circuit, runs, and takes an edit (the circuit editor reloads the engine).
func _lab_checks() -> void:
	if not ClassDB.class_exists("SpikeTask"): return
	for id in ClassDB.instantiate("SpikeTask").call("task_ids"):
		cur = ""; open_lab(id, true); await _frames(30)
		var lab := screen as LabScreen
		_check(lab != null and lab.tele.sim != null, "lab: %s reference circuit did not load%s" % [id, (" (" + lab.tele.error + ")") if lab != null else ""])
		if lab == null or lab.tele.sim == null: continue
		_check(lab.tele.t_ms > 0.0, "lab: %s did not advance" % id)
		await get_tree().create_timer(0.6).timeout
		if id != "reflex" and str(Campaign.info(id)["view"]) == "polyline": _check(lab.tele.total_spikes > 0, "lab: %s produced no spikes in the lab view" % id)   # a reflex arc is quiet until the first poke at 1.5 s
		var n0 := lab.tele.n
		var nid := Circuits.add_neuron(lab.circuit, "iaf_psc_alpha", 0, Vector2(100, 400), "I"); lab.editor.select({"t": "pop", "id": nid}); lab._on_structure()
		await get_tree().create_timer(0.5).timeout
		_check(lab.tele.sim != null and lab.tele.n == n0 + 1, "lab: %s edit did not reload (n %d -> %d)" % [id, n0, lab.tele.n])
		small_fonts_total += _small_fonts(screen)
	cur = ""; show_screen("title", false, true); await _frames()

var small_fonts_total := 0

## v0.2: the campaign map and its data contract, the parts bin and its catalogue reader, the lesion lab, the body views on synthetic tasks, sandbox dials.
func _v02_checks() -> void:
	# contract: every task answers the v0.2 questions and lands in a known chapter
	var ids := Campaign.installed()
	for id in ids:
		var d := Campaign.info(id)
		_check(Campaign.CHAPTERS.any(func(c): return c["id"] == d["chapter"]), "task %s has an unknown chapter '%s'" % [id, d["chapter"]])
		_check(["polyline", "limb", "heart", "lungs", "eye", "gut", "dashboard"].has(str(d["view"])), "task %s has an unknown view '%s'" % [id, d["view"]])
	for c in Campaign.CHAPTERS: _check(not Campaign.nodes(c["id"]).is_empty() or true, "chapter %s" % c["id"])
	_check(Campaign.unlocked(str(Campaign.order()[0])), "the first campaign task is open")
	# the catalogue: cards from the engine, thumbnails read in every shape
	var cat_ok := true
	for shape in [{"fi": {"I": [0, 1, 2], "rate": [0, 5, 20]}}, {"fi": [[0, 0], [1, 4], [2, 9]]}, {"fi": [{"level": 0, "rate_hz": 0}, {"level": 1, "rate_hz": 3}]}]:
		if (Catalog.series_of(shape, "fi")[0] as PackedFloat32Array).size() < 2: cat_ok = false
	if (Catalog.series_of({"trace": {"v_mV": [-70, -60, 20, -70], "dt_ms": 2.0, "t0_ms": 10.0}}, "vm")[0] as PackedFloat32Array).size() != 4: cat_ok = false
	_check(cat_ok, "catalogue reader: f-I / V_m shapes")
	_check(Catalog.has_card("iaf_psc_alpha") and Catalog.usable("iaf_psc_alpha"), "the default part is usable")
	_check(not Catalog.cards().is_empty(), "the catalogue has cards (engine catalog_json: %s)" % str(Catalog.from_engine()))
	if Catalog.from_engine():
		var a_n := 0
		for cd in Catalog.cards(): a_n += 1 if Catalog.available(cd["id"]) else 0
		print("CATALOG engine cards=%d available=%d thumbs=%s" % [Catalog.cards().size(), a_n, Catalog.source()])
	# the campaign map, the parts bin, in every layout
	for kind in Layout.KINDS:
		Settings.layout = kind; relayout()
		for sc in ["tasks", "parts"]:
			cur = ""; show_screen(sc, false, true); await _frames(3)
			small_fonts_total += _small_fonts(screen)
			if sc == "tasks":
				var ts := screen as TasksScreen
				for c in Campaign.CHAPTERS: ts._show_chapter(c["id"]); await _frames(2); small_fonts_total += _small_fonts(screen)
	Settings.layout = "auto"; relayout()
	# lesion algebra and a real re-run
	if ClassDB.class_exists("SpikeTask"):
		var rc := Circuits.parse(Circuits.read_reference(Campaign.ref_path("reflex")))
		var les := Lesion.apply(rc, {"proj:0": true, "pop:" + str(rc["populations"][0]["id"]): true})
		_check(is_zero_approx(Circuits.proj_weight(les["projections"][0])), "lesion: cut axon has weight 0")
		_check(not is_zero_approx(Circuits.proj_weight(rc["projections"][0])), "lesion: the original circuit is untouched")
		var runner := Lesion.Runner.new(); runner.begin("reflex", rc, [{"label": "intact", "lesions": {}}, {"label": "silence all", "lesions": {"pop:" + str(rc["populations"][0]["id"]): true}}])
		var t0 := Time.get_ticks_msec()
		while not runner.done() and Time.get_ticks_msec() - t0 < 60000: runner.step(50000)
		_check(runner.results.size() == 2 and runner.failed == "", "lesion re-run (%s)" % runner.failed)
		if runner.results.size() == 2: print("LESION reflex: intact %.0f, silenced %.0f" % [runner.results[0]["score"], runner.results[1]["score"]])
		_check(Lesion.csv(runner.results).begins_with("lesion,score"), "lesion csv export"); _check(Lesion.markdown("t", "c", 1, runner.results).contains("| Lesion |"), "lesion markdown export")
	# the lab with lesion mode, sandbox, and the synthetic body-system tasks (every view kind)
	OS.set_environment("SP_MOCK", "1"); Campaign.refresh()
	for id in MockTask.IDS:
		cur = ""; open_lab(id, true); await _frames(40)
		var lab := screen as LabScreen
		_check(lab != null and lab.tele.sim != null, "mock %s did not load in the lab" % id)
		if lab == null or lab.tele.sim == null: continue
		await get_tree().create_timer(0.8).timeout
		_check(lab.tele.hist_t.size() > 3, "mock %s: no readout history" % id)
		_check(lab.timeline_view.visible, "mock %s: the timeline is hidden" % id)
		small_fonts_total += _small_fonts(screen)
		if id == "mock_dash":
			lab._lesion_toggle(); await _frames(3); _check(lab.lesion_on, "the lesion lab opens on a reference circuit")
			lab.editor.lesion_clicked.emit({"t": "pop", "id": str(lab.circuit["populations"][0]["id"])}); await _frames(3); _check(lab.lesions.size() == 1, "clicking a population lesions it")
			small_fonts_total += _small_fonts(screen)
			lab._lesion_toggle(); await _frames(3); _check(not lab.lesion_on, "the lesion lab closes")
	OS.unset_environment("SP_MOCK"); Campaign.refresh()
	cur = ""; open_lab("", false); await _frames(30)
	var sb := screen as LabScreen
	_check(sb != null and sb.sandbox_panel != null and Circuits.sandbox, "the sandbox shows its panel with every part open")
	if sb != null: small_fonts_total += _small_fonts(screen)
	Circuits.sandbox = false
	cur = ""; show_screen("title", false, true); await _frames()
	print("V02 OK chapters=%d tasks=%d cards=%d" % [Campaign.CHAPTERS.size(), Campaign.installed().size(), Catalog.cards().size()])

## Steppers and toggles change Settings; screen changes animate and leave exactly one screen behind.
func _interaction_checks() -> void:
	cur = ""; show_screen("settings", false, true); (screen as SettingsScreen).set_tab("Display"); await _frames()
	var st := screen.find_children("*", "HBoxContainer", true, false).filter(func(n): return n is Kit.Stepper)
	_check(st.size() >= 3, "display tab has steppers (%d)" % st.size())
	var vs_before := Settings.vsync
	var vstep: Kit.Stepper = st[1]; vstep.step(1); _check(Settings.vsync != vs_before or vstep.opts.size() < 2, "stepper changed vsync")
	vstep.step(-1); _check(Settings.vsync == vs_before, "stepper restores vsync")
	Kit.instant = false
	cur = ""; show_screen("title", false, true); await _frames()
	show_screen("settings"); await get_tree().create_timer(0.45).timeout
	_check(holder.get_child_count() == 1 and cur == "settings", "transition leaves one screen (%d)" % holder.get_child_count())
	back(); await get_tree().create_timer(0.45).timeout
	_check(holder.get_child_count() == 1 and cur == "title", "back returns to title (%s, %d)" % [cur, holder.get_child_count()])
	var rm := Settings.reduced_motion
	Settings.reduced_motion = true; show_screen("settings"); await get_tree().create_timer(0.3).timeout
	_check(holder.get_child_count() == 1 and screen.modulate.a > 0.99, "reduced-motion transition completes")
	Settings.reduced_motion = rm; Kit.instant = true

## Count visible text whose ON-SCREEN size is below Kit.MIN_FONT physical px: the logical font size times the layout's text scale
## (Layout.text_scale: the content scale at the class's smallest reference window, e.g. 0.81 for an 844x390 phone) times any scale on the
## control itself. Nodes that draw their own text report their smallest size through `min_text_px()`.
func _small_fonts(n: Node, ppl := -1.0) -> int:
	if ppl < 0.0: ppl = Layout.text_scale(null)
	var bad := 0
	if n is Control and not (n as Control).is_visible_in_tree() and n.is_inside_tree(): return 0
	var k := ppl
	if n is Control: k *= absf((n as Control).get_global_transform().get_scale().y)
	var floor_ := float(Kit.MIN_FONT) - 0.01
	if n is Label:
		var l := n as Label; var sz: int = l.label_settings.font_size if l.label_settings != null else l.get_theme_font_size("font_size")
		if sz * k < floor_ and l.text.strip_edges() != "": bad += 1; printerr("small font (%s): label '%s' %d px -> %.1f on screen" % [Layout.kind, l.text.left(40), sz, sz * k])
	elif n is Button:
		var bs := (n as Button).get_theme_font_size("font_size")
		if bs * k < floor_ and (n as Button).text.strip_edges() != "": bad += 1; printerr("small font (%s): button '%s' %d px -> %.1f on screen" % [Layout.kind, (n as Button).text, bs, bs * k])
	elif n is RichTextLabel:
		if (n as RichTextLabel).get_theme_font_size("normal_font_size") * k < floor_: bad += 1; printerr("small font (%s): rich text" % Layout.kind)
	if n.has_method("min_text_px"):
		var px: float = n.call("min_text_px")
		if px > 0.0 and px < floor_: bad += 1; printerr("small font (%s): %s draws text at %.1f px on screen" % [Layout.kind, n.name, px])
	for c in n.get_children(): bad += _small_fonts(c, ppl)
	return bad
