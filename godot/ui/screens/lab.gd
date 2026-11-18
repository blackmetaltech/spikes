class_name LabScreen
extends Screen
## The lab: circuit editor (left) and live instruments (right) around one running experiment. This file is the CONTROLLER: the state of the visit
## (the circuit, the live run, the episode record, the 5-seed test, the reference lock, diagnosis answers) and the actions that change it. The rest is in
## godot/ui/lab/:
##   lab_layout.gd     the panels, the top bar and palette, the More menu, the parts bin, the end-of-episode card and the histogram card
##   lab_inspector.gd  the inspector: brief, neuron / axon / channel editors, the lesion table, the diagnosis case card
##   lab_play_bar.gd   play controls: difficulty mode and variant, hints, verdict highlight, instant check, skip
##   lab_lesions.gd    the lesion lab (cut, silence, re-run on seeds, sweep, export)
##   lab_unlocks.gd    progressive disclosure: which tools a level shows
## The circuit Dictionary is the single source of truth; any structural edit reloads the engine (debounced) and restarts the episode.
const SPEEDS := [["0.25", "0.25x"], ["0.5", "0.5x"], ["1.0", "1x"], ["2.0", "2x"], ["5.0", "5x"], ["10.0", "10x"], ["20.0", "20x"], ["50.0", "50x"]]
const SENSOR_KIND := {"lamprey": "dc"}   # last resort when neither the task nor its reference circuit names a sensor kind
const TEST_SEEDS := [101, 102, 103, 104, 105]

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
var insp_scroll: ScrollContainer
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
var celebration: Celebration
var ov: Control                    # overlay layer: phone inspector sheet, More menu, end-of-episode card
var ext_ok := false

## the modules (see the header)
var layout := LabLayout.new()
var insp := LabInspector.new()
var play := LabPlayBar.new()
var les := LabLesions.new()

## Lesion lab (after a pass): lesions = {"proj:<i>" | "pop:<id>" | "in:<name>": true}, rows = the lesion table, _lrun = the re-run in progress.
var lesion_on := false
var lesions := {}
var lesion_rows: Array = []
var _lrun: Lesion.Runner = null
var _lesion_open := false          # phone: the lesion sheet shows the table (else a one-line bar with Run / Sweep)
var _lesion_note := ""
var _dirty := -1.0
var _brief_forced := false
var _brief_full := false           # the brief's "More" expander is open
var _file_dlg: FileDialog
var _pending_banner := ""          # a message that arrived before the banner label existed (the first _reload runs before build)
var _fail_shown := false
var _adv_open := false             # the neuron inspector's Advanced fold
var _rec_done := false             # this episode's result has been recorded
var _rec_pass := false
var _overlay: Control = null
var _overlay_dismissed := false
var _test := {}                    # the 5-seed TEST in progress: {seeds, i, res, sim, task, t}
var _test_result := {}             # {passes, n, scores}: the last finished TEST on this circuit

## v0.3 lab play: difficulty mode and variant, hint ladder, verdict highlight, post-solve histograms, diagnosis, instant check, skip, celebration, surprise.
var mode := "standard"
var variant := ""
static var _fails := {}            # task -> failed runs this session (the Standard hint ladder opens after HintPanel.UNLOCK_FAILS)
static var _hint_tier := {}        # task -> highest hint tier revealed
static var _ref_loaded := {}       # task -> the REFERENCE button filled the bench this session (an edited copy is still derived; cleared by Empty bench)
var _hints_cache: Array = []       # Task::hints() as of the last failed run
var _recipe_open := false          # Standard: Show recipe was pressed this visit (Progress.record_recipe holds it across visits)
var _last_edit_ms := -10000        # play-log edit coalescing: a slider drag is one edit
var _half_warned := false
var _surprise_done := {}
var _surprise_clock := 0.0
var _dx := {}                      # diagnosis case state {picked, tries, solved, gave_up}
var _dx_info := {}                 # Task::diagnosis()
var _ref_cache := {"n": -1, "fp": "", "v": false}

func _init() -> void:
	super()
	layout.lab = self; insp.lab = self; play.lab = self; les.lab = self

func default_focus() -> Control: return back_btn
func on_back() -> bool: return false

func build() -> void:
	ext_ok = ClassDB.class_exists("SpikeSim") and ClassDB.class_exists("SpikeTask")
	if circuit.is_empty(): _start(app.lab_task if app != null else "")
	Circuits.sandbox = task_id == ""
	layout.build()
	if _pending_banner != "": _banner_raw(_pending_banner); _pending_banner = ""
	_attach()
	if OS.get_environment("SP_LABTAB") != "": tab = OS.get_environment("SP_LABTAB")
	if layout.compact: layout.apply_tab()
	_file_dlg = FileDialog.new(); _file_dlg.access = FileDialog.ACCESS_FILESYSTEM; _file_dlg.filters = PackedStringArray(["*.json ; Circuit JSON"]); _file_dlg.current_dir = ProjectSettings.globalize_path(Circuits.user_dir())
	add_child(_file_dlg)
	if _is_dx(): _brief_forced = true
	_build_inspector()
	play.hint_btn_sync()
	if OS.get_environment("SP_ADV") != "": _adv_open = true
	if OS.get_environment("SP_SEL") != "": _select_env(OS.get_environment("SP_SEL"))
	if OS.get_environment("SP_MENU") != "": layout.open_menu()
	if OS.get_environment("SP_PARTS") != "": layout.open_parts()
	if OS.get_environment("SP_LESION") != "": les.env.call_deferred()
	_demo_hooks.call_deferred()
	if OS.get_environment("SP_TESTNOW") != "": _test_start.call_deferred()   # dev hook: run TEST x5 at once, print the result, quit

## The circuit the live view runs: the player's, with the lesion set applied in the lesion lab.
func _live_circuit() -> Dictionary: return Lesion.apply(circuit, lesions) if lesion_on and not lesions.is_empty() else circuit

func _exit_tree() -> void:
	play.check_cancel()
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
	var kept := false   # the circuit continues one from earlier in this session (so a REFERENCE taint carries over)
	if app != null and app.lab_keep.has(task_id) and OS.get_environment("SP_REF") == "" and not app.lab_reference:
		circuit = (app.lab_keep[task_id] as Dictionary).duplicate(true); loaded = true; kept = true
	if not kept: _ref_loaded.erase(task_id)
	if not loaded and OS.get_environment("SP_CIRCUIT") != "":   # dev hook: open this circuit file
		circuit = Circuits.parse(FileAccess.get_file_as_string(OS.get_environment("SP_CIRCUIT"))); loaded = not circuit.is_empty()
	if not loaded and ext_ok and task_id != "" and (OS.get_environment("SP_REF") != "" or app.lab_reference):
		var txt := Circuits.read_reference(_ref_path(task_id))
		var c := Circuits.parse(txt)
		if not c.is_empty(): circuit = c; loaded = true; _ref_loaded[task_id] = true
	if not loaded:
		var saved := _last_path()
		if FileAccess.file_exists(saved) and OS.get_environment("SP_NOAUTOSAVE") == "": circuit = Circuits.parse(FileAccess.get_file_as_string(saved)); loaded = not circuit.is_empty()
	if not loaded: circuit = _skeleton(task_id)
	_drop_old_flag()
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

## The reference lock. Reference circuits do not count for progress, and neither does anything derived from one. The game decides it, not the file:
## the circuit's structural fingerprint (Circuits.fingerprint: populations, models, projections, channels, weights to three significant figures) equals
## the reference's, OR the REFERENCE button filled this bench in this session and it has not been emptied since (a nudged weight is still the reference).
## LOAD from disk, a copied file or retyping the reference's numbers are all caught by the fingerprint.
func _is_reference() -> bool:
	if task_id == "" or _is_dx(): return false
	if bool(_ref_loaded.get(task_id, false)): return true
	var fp := Circuits.fingerprint(circuit)
	if fp != str(_ref_cache["fp"]): _ref_cache = {"fp": fp, "v": Circuits.is_reference_of(circuit, task_id)}
	return bool(_ref_cache["v"])

## 0.3 saves marked reference-derived circuits with a flag in the file; the fingerprint replaces it, so the flag is dropped on load.
func _drop_old_flag() -> void:
	var ed: Dictionary = circuit.get("meta", {}).get("editor", {})
	if ed.has("from_reference"): ed.erase("from_reference")

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
	layout.overlay_hide()
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
	_dirty = 0.0; les.reset(); _surprise_done.erase("osc")
	if editor != null: editor.verdict_target = ""
	if score_card != null: score_card.guilty = ""
	if Circuits.neuron_count(circuit) == 0: _ref_loaded.erase(task_id)   # an emptied bench is the player's own again; an edited reference is still derived from it

func _process(dt: float) -> void:
	if _dirty >= 0.0:
		_dirty += dt
		if _dirty > 0.3 and _lrun == null: _dirty = -1.0; _reload(); _build_inspector()
	if not _test.is_empty(): _test_step()
	elif _lrun != null: les.step()
	elif tele != null: tele.advance(dt)
	_watch_episode()
	play.check_poll(); _surprise_tick(dt)
	if status != null and tele != null:
		var spd := String.num(tele.speed) + "x"
		if tele.sim != null and tele.running and tele.speed > 1.0 and tele.achieved < tele.speed * 0.85: spd = "%.1fx of %sx" % [tele.achieved, String.num(tele.speed)]
		status.text = "t %.2f s    %s spikes    %s" % [tele.t_ms * 0.001, _short(tele.total_spikes), spd] if tele.sim != null else "no circuit running"

static func _short(n: int) -> String: return "%.1fk" % (n / 1000.0) if n >= 10000 else str(n)

func _is_dx() -> bool: return task_id != "" and str(Campaign.info(task_id).get("kind", "")) == "diagnosis"

func _title() -> String:
	if task_id == "": return "Sandbox"
	return str(Campaign.info(task_id)["title"]) if ext_ok else "Lab"

# ---------- editing actions ----------
func _toggle_connect() -> void:
	editor.tool = "connect" if editor.tool == "select" else "select"; editor._connect_from = {}
	layout.sync_connect_btn(); editor.queue_redraw()

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
	if run_btn != null: run_btn.text = "Pause" if tele.running else "Run"
func _restart() -> void:
	layout.overlay_hide(); _overlay_dismissed = false; _rec_done = false; _rec_pass = false; _fail_shown = false
	if editor != null: editor.verdict_target = ""
	if score_card != null: score_card.guilty = ""
	if celebration != null: celebration.abort()
	GameAudio.event("run")
	if tele.sim != null: tele.reset(); tele.running = true; _sync_run_btn()
	elif tele.task != null: tele.task.reset(int(circuit.get("seed", 1)))
	task_view.attach(tele)

## REFERENCE: the reference circuit on the bench. Offered only once the level is passed, or in Guided mode (LabLayout).
func _load_reference() -> void:
	var txt := Circuits.read_reference(_ref_path(task_id))
	var c := Circuits.parse(txt)
	if c.is_empty(): _banner("the reference circuit for this task is not installed (circuits/%s)" % _ref_path(task_id)); return
	circuit = c; Circuits.ensure_pos(circuit); _ref_loaded[task_id] = true; editor.select({}); les.reset(); _reload(true); _build_inspector()

## Start over from the task's skeleton: its sensors and motors, no neurons (and no longer reference-derived).
func _empty_bench() -> void:
	if task_id == "": return
	layout.close_menu(); _test = {}; _lrun = null; _ref_loaded.erase(task_id)
	circuit = _skeleton(task_id); Circuits.ensure_pos(circuit); editor.select({}); les.reset(); _reload(true); _build_inspector()

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
		circuit = c; _drop_old_flag(); Circuits.ensure_pos(circuit); editor.select({}); les.reset(); _reload(true); _build_inspector())
	_popup_file()

func _popup_file() -> void: _file_dlg.popup_centered_clamped(Vector2i(900, 560), 0.9)

## Persist the player's circuit per task (user://circuits/last_<task>.json) after each edit.
func _autosave() -> void:
	if OS.get_environment("SP_NOAUTOSAVE") != "" or Settings.no_save: return
	var fh := FileAccess.open(_last_path(), FileAccess.WRITE)
	if fh != null: fh.store_string(Circuits.to_json(circuit))

func _open_analysis() -> void: app.open_analysis(circuit, tele, task_id)

# ---------- progress, the episode record, 5-seed test ----------
func _rate_per_neuron() -> float: return tele.spike_rate_hz() / maxf(float(Circuits.neuron_count(circuit)), 1.0) if tele != null and tele.sim != null else -1.0

func _stars_now() -> int:
	if tele == null or tele.task == null: return 0
	var st := Progress.stars_for(task_id, bool(tele.score.get("passed", false)), false, Circuits.neuron_count(circuit), tele.spike_rate_hz() / maxf(float(Circuits.neuron_count(circuit)), 1.0))
	return mini(st, 2) if _recipe_used() else st

## Called every frame: record a pass the moment it happens and the final result at the end of the episode; raise the end card; report a stopped run.
func _watch_episode() -> void:
	if tele == null: return
	if tele.failed and not _fail_shown: _fail_shown = true; _banner_raw("The simulation stopped: " + tele.error + ". Fix the circuit and press “Reset”.")
	if tele.task == null or tele.sim == null or tele.failed or not _test.is_empty() or lesion_on or _is_dx(): return
	var passed := bool(tele.score.get("passed", false))
	if passed and not _rec_pass:
		_rec_pass = true
		var first := not bool(Progress.best(task_id)["passed"]) and not _is_reference() and variant == ""
		_record(true, false)
		if mode == "researcher" and variant == "": first = false; celebration.toast("Passed once", "Researcher mode counts a pass only from “Test x5”: press it to record the pass.", Kit.AMBER, 5.0)
		if editor != null: editor.hint_target = ""; editor.verdict_target = ""
		if score_card != null: score_card.guilty = ""
		GameAudio.event("pass")
		if first: _celebrate()
		layout.refresh_bar()   # passing opens tools (Test x5 on the bar, Reference, Lesions, Results)
	if tele.finished and not _rec_done:
		_rec_done = true
		_record(passed, false)
		if not passed: play.note_fail()
		if not _overlay_dismissed: layout.overlay_show()

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
		celebration.toast("Over the half-neuron limit", "%s; this variant allows %d (half of par %d). It does not count until the circuit is smaller." % [Progress.neurons_text(n), lim, par], Kit.AMBER, 6.0)
	return false

## A recipe is in force for the stars: Guided shows it inline (no cost), Standard costs the third star once Show recipe was pressed.
func _recipe_used() -> bool: return mode == "standard" and (_recipe_open or Progress.recipe_used(task_id))

## Task::recipe() when the extension has it (concrete numbers kept apart from the brief), else "".
func _recipe_text() -> String:
	if task_id == "" or tele == null or tele.task == null or not tele.task.has_method("recipe"): return ""
	return str(tele.task.call("recipe"))

func _ov_passed() -> bool:
	if not _test_result.is_empty(): return int(_test_result["passes"]) == int(_test_result["n"])
	return bool(tele.score.get("passed", false))

## TEST x5: five headless episodes of the task on five other seeds, run a slice of each frame. Pass on all five earns the second star.
func _test_start() -> void:
	if task_id == "" or tele.task == null or Circuits.neuron_count(circuit) == 0 or not _test.is_empty(): return
	if tele.wiring_issue() != "": _banner_raw(tele.wiring_issue()); return
	layout.close_menu(); _overlay_dismissed = false
	if tele.sim != null: tele.sim.stop()
	var sim = ClassDB.instantiate("SpikeSim"); var task = Campaign.make(task_id); Telemetry.configure(task, mode, variant)
	_test = {"i": 0, "res": [], "sim": sim, "task": task, "t": -1.0}
	tele.running = false
	layout.overlay_show(true)

func _test_cancel() -> void:
	_test = {}; layout.overlay_hide(); _reload(); _build_inspector()

func _test_step() -> void:
	var sim = _test["sim"]; var task = _test["task"]
	if float(_test["t"]) < 0.0:   # begin episode i
		var tc := Telemetry.sanitized(circuit)
		tc["seed"] = 1000 * int(TEST_SEEDS[int(_test["i"])]) + int(circuit.get("seed", 1))   # the circuit is reseeded with the world (as the engine's tests do), so a deterministic world is not five identical runs
		if not sim.load_circuit(Circuits.to_json(tc)): _test = {}; layout.overlay_hide(); _reload(); _banner(str(sim.error())); return
		task.reset(int(TEST_SEEDS[int(_test["i"])])); _test["t"] = 0.0
	var t0 := Time.get_ticks_usec(); var dur := float(task.duration_ms())
	while float(_test["t"]) < dur and Time.get_ticks_usec() - t0 < 12000:
		task.step(sim, 1.0); _test["t"] = float(_test["t"]) + 1.0
		if not bool(task.step_ok(sim)):
			_test = {}; layout.overlay_hide(); _reload(); _banner_raw("The test stopped: " + str(sim.error())); return
		if int(_test["t"]) % 200 == 0: sim.drain_spikes()
	layout.test_progress(int(TEST_SEEDS[int(_test["i"])]), 100.0 * float(_test["t"]) / maxf(dur, 1.0), _test["res"])
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
	if passes < res.size(): play.note_fail()
	_rec_done = true; layout.overlay_show(); _build_inspector(); layout.refresh_bar()
	if OS.get_environment("SP_TESTNOW") != "" and OS.get_environment("SP_SHOT") == "": print("TEST RESULT ", _test_result, " best ", Progress.best(task_id)); get_tree().quit()

# ---------- inspector (lab_inspector.gd) ----------
func _select_env(s: String) -> void:   # SP_SEL=pop:L | proj:0 | in:drive | out:left_motor
	var p := s.split(":"); if p.size() < 2: return
	match p[0]:
		"pop": editor.select({"t": "pop", "id": p[1]})
		"proj": editor.select({"t": "proj", "i": int(p[1])})
		"in", "out": editor.select({"t": p[0], "name": p[1]})

func _build_inspector() -> void: insp.build()

# ---------- lesion lab (lab_lesions.gd); kept here for the smoke test and the end card ----------
func _lesion_ok() -> bool: return les.ok()
func _lesion_toggle() -> void: les.toggle()

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

# ---------- diagnosis cases ----------
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
		if wrong != "" and play.hint_open: play.hint_render()
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
## SP_FAILS=n sets the failed-run count, SP_CHECK=1 presses Check, SP_SKIP=1 presses Skip, SP_BRIEF=full opens the brief's expander.
func _demo_hooks() -> void:
	if OS.get_environment("SP_FAILS") != "": _fails[task_id] = int(OS.get_environment("SP_FAILS")); play.hint_btn_sync()
	if OS.get_environment("SP_BRIEF") == "full": _brief_full = true; _build_inspector()
	if OS.get_environment("SP_HINT") != "":
		await get_tree().create_timer(0.8).timeout
		_hints_cache = play.task_hints(); play.hint_open = true; _hint_tier[task_id] = 0
		for i in int(OS.get_environment("SP_HINT")): play.hint_next()
		play.hint_render()
	if OS.get_environment("SP_VERDICT") != "":
		await get_tree().create_timer(1.0).timeout
		_hints_cache = play.task_hints(); play.verdict_show()
	if OS.get_environment("SP_HISTO") != "":
		await get_tree().create_timer(1.2).timeout
		layout.histo_show()
	if OS.get_environment("SP_CELEB") != "":
		await get_tree().create_timer(1.0).timeout
		_celebrate()
	if OS.get_environment("SP_CHECK") != "":
		await get_tree().create_timer(1.0).timeout
		play.check_start()
	if OS.get_environment("SP_SKIP") != "":
		await get_tree().create_timer(1.0).timeout
		play.skip_event()
	if OS.get_environment("SP_DXPICK") != "":
		await get_tree().create_timer(0.8).timeout
		_dx["picked"] = OS.get_environment("SP_DXPICK"); _build_inspector()
		if OS.get_environment("SP_DXSUBMIT") != "": _dx_submit()
