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
var model_idx := 0
var preset_idx := 0
var _dirty := -1.0
var _brief_forced := false
var _file_dlg: FileDialog
var _connect_btn: Button
var ext_ok := false

func default_focus() -> Control: return back_btn
func on_back() -> bool: return false

func build() -> void:
	ext_ok = ClassDB.class_exists("SpikeSim") and ClassDB.class_exists("SpikeTask")
	var phone := Layout.phone; var f := f()
	if circuit.is_empty(): _start(app.lab_task if app != null else "")
	add_child(Kit.bench())
	var margin := MarginContainer.new(); margin.set_anchors_preset(Control.PRESET_FULL_RECT)
	var mx := int(26 * f) if not phone else 14; var my := int(18 * f) if not phone else 8
	for side in ["left", "right"]: margin.add_theme_constant_override("margin_" + side, mx)
	margin.add_theme_constant_override("margin_top", my); margin.add_theme_constant_override("margin_bottom", my); add_child(margin)
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", int(10 * f) if not phone else 6); margin.add_child(v)
	v.add_child(_topbar(phone))
	var compact := phone or get_viewport_rect().size.x < 1180.0
	if compact: v.add_child(_tabs_row())
	var body := HBoxContainer.new(); body.size_flags_vertical = Control.SIZE_EXPAND_FILL; body.add_theme_constant_override("separation", int(14 * f)); v.add_child(body)
	# left: editor
	var left := VBoxContainer.new(); left.size_flags_horizontal = Control.SIZE_EXPAND_FILL; left.size_flags_stretch_ratio = 1.55; left.add_theme_constant_override("separation", int(8 * f)); body.add_child(left)
	panels["circuit"] = left
	left.add_child(_palette(phone))
	editor = CircuitEditor.new(); editor.size_flags_vertical = Control.SIZE_EXPAND_FILL; editor.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	editor.custom_minimum_size = Vector2(300, 200)
	var edp := PanelContainer.new(); edp.add_theme_stylebox_override("panel", Kit.sb(Color("#080f1a"), Kit.LINE, 1, 12, 0, false)); edp.size_flags_vertical = Control.SIZE_EXPAND_FILL; edp.add_child(editor); left.add_child(edp)
	editor.structure_changed.connect(_on_structure); editor.moved.connect(func(): pass)
	editor.selection_changed.connect(func(_s): _build_inspector())
	inspector = PanelContainer.new(); inspector.add_theme_stylebox_override("panel", Kit.sb(Kit.PANEL, Kit.LINE, 1, 12, 14, false)); left.add_child(inspector)
	insp_body = VBoxContainer.new(); insp_body.add_theme_constant_override("separation", 8); inspector.add_child(insp_body)
	# right: instruments
	var right := VBoxContainer.new(); right.size_flags_horizontal = Control.SIZE_EXPAND_FILL; right.size_flags_stretch_ratio = 1.0; right.add_theme_constant_override("separation", int(8 * f)); body.add_child(right)
	panels["scopes"] = right
	var world := VBoxContainer.new(); world.size_flags_vertical = Control.SIZE_EXPAND_FILL; world.size_flags_stretch_ratio = 3.0; world.add_theme_constant_override("separation", int(8 * f)); right.add_child(world)
	panels["world"] = world
	task_view = TaskView.new(); task_view.size_flags_vertical = Control.SIZE_EXPAND_FILL; world.add_child(task_view)
	score_card = ScoreCard.new(); world.add_child(score_card)
	var scopes := VBoxContainer.new(); scopes.size_flags_vertical = Control.SIZE_EXPAND_FILL; scopes.size_flags_stretch_ratio = 4.1; scopes.add_theme_constant_override("separation", int(8 * f)); right.add_child(scopes)
	panels["scopes_only"] = scopes
	raster = RasterView.new(); raster.size_flags_vertical = Control.SIZE_EXPAND_FILL; raster.size_flags_stretch_ratio = 0.8; scopes.add_child(raster)
	traces = TraceView.new(); traces.size_flags_vertical = Control.SIZE_EXPAND_FILL; traces.size_flags_stretch_ratio = 1.3; scopes.add_child(traces)
	gauges = GaugeView.new(); gauges.custom_minimum_size = Vector2(0, 40.0 + 30.0 * maxi(1, tele.rates.size() if tele.sim != null else 2)); scopes.add_child(gauges)
	banner = Kit.label("", 17, Kit.RED, true); banner.visible = false; v.add_child(banner)
	_attach()
	if OS.get_environment("SP_LABTAB") != "": tab = OS.get_environment("SP_LABTAB")
	if compact: _apply_tab()
	_file_dlg = FileDialog.new(); _file_dlg.access = FileDialog.ACCESS_FILESYSTEM; _file_dlg.filters = PackedStringArray(["*.json ; Circuit JSON"]); _file_dlg.current_dir = ProjectSettings.globalize_path(Circuits.user_dir())
	_file_dlg.size = Vector2i(900, 560); add_child(_file_dlg)
	_build_inspector()
	if OS.get_environment("SP_SEL") != "": _select_env(OS.get_environment("SP_SEL"))

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
		if not c.is_empty(): circuit = c; loaded = true
	if not loaded:
		var saved := _last_path()
		if FileAccess.file_exists(saved) and OS.get_environment("SP_NOAUTOSAVE") == "": circuit = Circuits.parse(FileAccess.get_file_as_string(saved)); loaded = not circuit.is_empty()
	if not loaded: circuit = _skeleton(task_id)
	Circuits.ensure_pos(circuit)
	_reload(true)

func _ref_path(tid: String) -> String:
	var t = ClassDB.instantiate("SpikeTask"); return str(t.call("reference_circuit_of", tid))

func _last_path() -> String: return Circuits.user_dir().path_join("last_%s.json" % (task_id if task_id != "" else "free"))

func _skeleton(tid: String) -> Dictionary:
	var c := Circuits.empty("My " + tid + " circuit" if tid != "" else "My circuit")
	if ext_ok and tid != "":
		var t = ClassDB.instantiate("SpikeTask")
		if t.make(tid):
			var kind: String = SENSOR_KIND.get(tid, "poisson")
			for s in t.sensor_channels(): c["inputs"].append({"name": str(s), "kind": kind, "targets": [], "weight": 300.0 if kind == "dc" else 120.0, "value": 0.0})
			for m in t.motor_channels(): c["outputs"].append({"name": str(m), "kind": "rate", "neurons": []})
		c["meta"]["task"] = tid
	return c

func _reload(first := false) -> void:
	var keep: Array = tele.watched.duplicate() if tele != null else []
	var was_running := tele.running if tele != null else true; var sp := tele.speed if tele != null else 1.0
	if tele.sim != null: tele.sim.stop()
	tele = Telemetry.new(); tele.running = was_running; tele.speed = sp
	var ok := false
	if Circuits.neuron_count(circuit) > 0: ok = tele.setup(circuit, task_id, ext_ok)
	else:
		tele.circuit = circuit; tele.error = ""
		if ext_ok and task_id != "": tele.task = ClassDB.instantiate("SpikeTask"); tele.task.make(task_id); tele.body = tele.task.body(); tele.score = tele.task.score()
	if ok and not keep.is_empty() and not first:
		var valid := keep.filter(func(i): return i < tele.n)
		if not valid.is_empty(): tele.set_watched(valid)
	if not ok and tele.error != "": _banner(tele.error)
	elif banner != null: banner.visible = false
	if first and OS.get_environment("SP_SPEED") != "": tele.speed = float(OS.get_environment("SP_SPEED"))
	if editor != null: _attach()

func _attach() -> void:
	editor.set_circuit(circuit, tele); task_view.attach(tele); score_card.attach(tele); raster.attach(tele); traces.attach(tele); gauges.attach(tele)
	gauges.custom_minimum_size = Vector2(0, 40.0 + 30.0 * maxi(1, tele.rates.size() if tele.sim != null else maxi(1, circuit.get("outputs", []).size())))
	if run_btn != null: _sync_run_btn()

func _banner(msg: String) -> void:
	if banner == null: return
	banner.text = "The engine rejected the circuit: " + msg; banner.visible = true

func _on_structure() -> void: _dirty = 0.0

func _process(dt: float) -> void:
	if _dirty >= 0.0:
		_dirty += dt
		if _dirty > 0.3: _dirty = -1.0; _reload(); _build_inspector()
	if tele != null: tele.advance(dt)
	if status != null and tele != null:
		var spd := String.num(tele.speed) + "x"
		if tele.sim != null and tele.running and tele.speed > 1.0 and tele.achieved < tele.speed * 0.85: spd = "%.1fx of %sx" % [tele.achieved, String.num(tele.speed)]
		status.text = "t %.2f s    %s spikes    %s" % [tele.t_ms * 0.001, _short(tele.total_spikes), spd] if tele.sim != null else "no circuit running"

static func _short(n: int) -> String: return "%.1fk" % (n / 1000.0) if n >= 10000 else str(n)

# ---------- top bar, tabs, palette ----------
func _topbar(phone: bool) -> Control:
	var h := HFlowContainer.new(); h.add_theme_constant_override("h_separation", 10); h.add_theme_constant_override("v_separation", 6)
	var bs := 22 if not phone else 20
	back_btn = Kit.btn("BACK", func(): app.back(), "panel", bs, Vector2(96, 0)); h.add_child(back_btn)
	if not phone: var ttl := Kit.shout(_title(), 34, Kit.TEXT); ttl.size_flags_vertical = Control.SIZE_SHRINK_CENTER; h.add_child(ttl)
	run_btn = Kit.btn("PAUSE", _toggle_run, "amber", bs, Vector2(120, 0)); h.add_child(run_btn)
	h.add_child(Kit.btn("RESET", func(): _restart(), "panel", bs, Vector2(92, 0)))
	h.add_child(Kit.stepper(SPEEDS, String.num(tele.speed), func(id): tele.speed = float(id), 76.0 if not phone else 64.0, 20, 44.0))
	status = Kit.label("", 17 if not phone else 15, Kit.TEXT_SOFT); status.size_flags_vertical = Control.SIZE_SHRINK_CENTER; status.size_flags_horizontal = Control.SIZE_EXPAND_FILL; status.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT; status.custom_minimum_size.x = 220 if not phone else 120; h.add_child(status)
	if phone: _sync_run_btn.call_deferred(); return h
	if task_id != "": h.add_child(Kit.btn("BRIEF", func(): editor.select({}); _build_inspector(), "ghost", bs, Vector2(84, 0)))
	if task_id != "": h.add_child(Kit.btn("REFERENCE", _load_reference, "cyan", bs, Vector2(150, 0)))
	h.add_child(Kit.btn("LOAD", _load_dialog, "panel", bs, Vector2(84, 0)))
	h.add_child(Kit.btn("SAVE", _save_dialog, "panel", bs, Vector2(84, 0)))
	h.add_child(Kit.btn("ANALYSIS", func(): app.open_analysis(circuit, tele, task_id), "green", bs, Vector2(130, 0)))
	_sync_run_btn.call_deferred()
	return h

func _title() -> String:
	if task_id == "" or not ext_ok: return "Lab"
	var t = ClassDB.instantiate("SpikeTask"); t.make(task_id); return str(t.title())

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

func _palette(phone: bool) -> Control:
	var h := HFlowContainer.new(); h.add_theme_constant_override("h_separation", 8); h.add_theme_constant_override("v_separation", 6)
	var sz := 18 if not phone else 18
	var models := []
	for m in Circuits.MODELS: models.append([m, m])
	h.add_child(Kit.stepper(models, Circuits.MODELS[model_idx], func(id): model_idx = Circuits.MODELS.find(id); preset_idx = 0; _refresh_presets(), 176.0 if not phone else 150.0, 18, 40.0))
	_preset_holder = HBoxContainer.new(); h.add_child(_preset_holder); _refresh_presets()
	h.add_child(Kit.btn("+ EXCITATORY", func(): _add_neuron("E"), "amber", sz))
	h.add_child(Kit.btn("+ INHIBITORY", func(): _add_neuron("I"), "cyan", sz))
	_connect_btn = Kit.btn("CONNECT", _toggle_connect, "panel", sz); h.add_child(_connect_btn)
	h.add_child(Kit.btn("FIT", func(): editor.fit(), "ghost", sz))
	if phone:
		if task_id != "": h.add_child(Kit.btn("BRIEF", func(): editor.select({}); _brief_forced = true; _build_inspector(), "ghost", sz)); h.add_child(Kit.btn("REFERENCE", _load_reference, "cyan", sz))
		h.add_child(Kit.btn("LOAD", _load_dialog, "panel", sz)); h.add_child(Kit.btn("SAVE", _save_dialog, "panel", sz))
		h.add_child(Kit.btn("ANALYSIS", func(): app.open_analysis(circuit, tele, task_id), "green", sz))
	return h

var _preset_holder: HBoxContainer
func _refresh_presets() -> void:
	for c in _preset_holder.get_children(): c.queue_free()
	var m: String = Circuits.MODELS[model_idx]; var opts := []
	for i in Circuits.PRESETS[m].size(): opts.append([str(i), Circuits.PRESETS[m][i][0]])
	_preset_holder.add_child(Kit.stepper(opts, str(preset_idx), func(id): preset_idx = int(id), 168.0 if not Layout.phone else 130.0, 18, 40.0))

func _toggle_connect() -> void:
	editor.tool = "connect" if editor.tool == "select" else "select"; editor._connect_from = {}
	Kit.skin(_connect_btn, "amber" if editor.tool == "connect" else "panel", 12); editor.queue_redraw()

func _add_neuron(kind: String) -> void:
	var id := Circuits.add_neuron(circuit, Circuits.MODELS[model_idx], preset_idx, editor.free_spot(), kind)
	editor.select({"t": "pop", "id": id}); editor.fit(); _on_structure()

# ---------- run controls ----------
func _toggle_run() -> void: tele.running = not tele.running; _sync_run_btn()
func _sync_run_btn() -> void:
	if run_btn != null: run_btn.text = "PAUSE" if tele.running else "RUN"
func _restart() -> void:
	if tele.sim != null: tele.reset()
	elif tele.task != null: tele.task.reset(int(circuit.get("seed", 1)))
	task_view.attach(tele)

func _load_reference() -> void:
	var txt := Circuits.read_reference(_ref_path(task_id))
	var c := Circuits.parse(txt)
	if c.is_empty(): _banner("the reference circuit for this task is not installed (circuits/%s)" % _ref_path(task_id)); return
	circuit = c; Circuits.ensure_pos(circuit); editor.select({}); _reload(true); _build_inspector()

func _save_dialog() -> void:
	for cn in _file_dlg.file_selected.get_connections(): _file_dlg.file_selected.disconnect(cn["callable"])
	_file_dlg.file_mode = FileDialog.FILE_MODE_SAVE_FILE; _file_dlg.title = "Save circuit"
	_file_dlg.current_file = str(circuit.get("name", "circuit")).to_lower().replace(" ", "_") + ".json"
	_file_dlg.file_selected.connect(func(p): var fh := FileAccess.open(p, FileAccess.WRITE); if fh != null: fh.store_string(Circuits.to_json(circuit)))
	_file_dlg.popup_centered()

func _load_dialog() -> void:
	for cn in _file_dlg.file_selected.get_connections(): _file_dlg.file_selected.disconnect(cn["callable"])
	_file_dlg.file_mode = FileDialog.FILE_MODE_OPEN_FILE; _file_dlg.title = "Load circuit"
	_file_dlg.file_selected.connect(func(p):
		var c := Circuits.parse(FileAccess.get_file_as_string(p))
		if c.is_empty(): _banner("not a circuit JSON file: " + p); return
		circuit = c; Circuits.ensure_pos(circuit); editor.select({}); _reload(true); _build_inspector())
	_file_dlg.popup_centered()

## Persist the player's circuit per task (user://circuits/last_<task>.json) after each edit.
func _autosave() -> void:
	if OS.get_environment("SP_NOAUTOSAVE") != "" or Settings.no_save: return
	var fh := FileAccess.open(_last_path(), FileAccess.WRITE)
	if fh != null: fh.store_string(Circuits.to_json(circuit))

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
	inspector.visible = not (Layout.phone and sel.is_empty() and not _brief_forced)
	if sel.is_empty(): _inspect_brief(small); return
	match sel["t"]:
		"pop": _inspect_pop(str(sel["id"]), small)
		"proj": _inspect_proj(int(sel["i"]), small)
		"in": _inspect_channel("inputs", str(sel["name"]), small)
		"out": _inspect_channel("outputs", str(sel["name"]), small)

func _head(title: String, kind_col: Color, tag := "") -> HBoxContainer:
	var h := HBoxContainer.new(); h.add_theme_constant_override("separation", 10)
	h.add_child(Kit.chip(title, kind_col, 18))
	if tag != "": var l := Kit.label(tag, 17, Kit.TEXT_SOFT); l.size_flags_horizontal = Control.SIZE_EXPAND_FILL; l.clip_text = true; h.add_child(l)
	return h

func _inspect_brief(small: int) -> void:
	var t: String = ""
	if task_id != "" and ext_ok:
		var tk = ClassDB.instantiate("SpikeTask"); tk.make(task_id); t = str(tk.brief())
	insp_body.add_child(_head("BRIEF" if task_id != "" else "FREE RUN", Kit.exc(), _title() if task_id != "" else "build any circuit and watch it run"))
	var txt := t if t != "" else "Add neurons, connect them by dragging from one disc's rim to another, and watch the raster and the voltage traces."
	var l := Kit.label(txt, small, Kit.TEXT_SOFT, true); insp_body.add_child(l)
	var leg := HBoxContainer.new(); leg.add_theme_constant_override("separation", 8)
	leg.add_child(Kit.chip("EXCITATORY", Kit.exc(), 14)); leg.add_child(Kit.chip("INHIBITORY", Kit.inh(), 14)); leg.add_child(Kit.chip("SENSOR", Kit.GREEN, 14)); leg.add_child(Kit.chip("MOTOR", Gfx.VIOLET, 14))
	insp_body.add_child(leg)

func _val_row(name: String, lo: float, hi: float, step: float, val: float, unit: String, cb: Callable) -> Control:
	var h := HBoxContainer.new(); h.add_theme_constant_override("separation", 10); h.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var nm := Kit.label(name, 17, Kit.TEXT_SOFT); nm.custom_minimum_size.x = 84; h.add_child(nm)
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
	var model := str(p.get("model", "")); var presets: Array = Circuits.PRESETS.get(model, [])
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
	var pi := Circuits.pop_index(circuit, post); var wmax: float = 4.0 * float(Circuits.WEIGHT.get(str(circuit["populations"][pi].get("model", "")), 100.0)) if pi >= 0 else 400.0
	var unit := "nS" if pi >= 0 and str(circuit["populations"][pi].get("model", "")).contains("cond") else "pA"
	var r1 := HFlowContainer.new(); r1.add_theme_constant_override("h_separation", 10); insp_body.add_child(r1)
	r1.add_child(Kit.stepper(SYNAPSE_OPTS, Circuits.proj_model(pr), func(m): var s: Dictionary = pr.get("synapse", {}); s["synapse_model"] = str(m); pr["synapse"] = s; _on_structure(), 190.0 if not Layout.phone else 150.0, 18, 40.0))
	r1.add_child(Kit.btn("E / I", func(): Circuits.set_proj_weight(pr, -Circuits.proj_weight(pr)); _on_structure(); _build_inspector(), "panel", 18))
	r1.add_child(Kit.btn("DELETE", func(): editor.delete_selected(), "danger", 18))
	insp_body.add_child(_val_row("weight", 0.0, wmax, wmax / 200.0, absf(w), unit, func(x): Circuits.set_proj_weight(pr, x * (-1.0 if Circuits.proj_weight(pr) < 0.0 else 1.0)); _on_structure()))
	insp_body.add_child(_val_row("delay", 0.1, 20.0, 0.1, Circuits.proj_delay(pr), "ms", func(x): Circuits.set_proj_delay(pr, x); _on_structure()))

const SYNAPSE_OPTS := [["static_synapse", "static"], ["stdp_synapse", "STDP"], ["tsodyks_synapse", "Tsodyks"]]

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
		insp_body.add_child(_val_row("weight", 0.0, 1500.0, 5.0, float(ch.get("weight", 1.0)), "pA", func(x): ch["weight"] = x; _on_structure()))
		if not driven: insp_body.add_child(_val_row("value", 0.0, 1000.0 if kind == "dc" else 400.0, 1.0, float(ch.get("value", 0.0)), unit, func(x): ch["value"] = x; _on_structure()))
	insp_body.add_child(Kit.btn("DISCONNECT ALL", func(): ch["targets" if is_in else "neurons"] = []; _on_structure(); _build_inspector(), "panel", 18))
