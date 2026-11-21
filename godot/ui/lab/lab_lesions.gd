class_name LabLesions
extends RefCounted
## The lesion lab (the controller is godot/ui/screens/lab.gd; its state, lesion_on / lesions / lesion_rows / _lrun, lives there): after a pass, cut an axon,
## silence a population or cut a sensor, watch the live view fail, re-run the whole episode on several seeds, sweep every single lesion, and export
## the table as Markdown or CSV.
var lab: LabScreen

## The lab opens after a pass: this circuit passed (now or earlier this session), it is the reference circuit, or the task has been passed before.
func ok() -> bool:
	if lab.task_id == "" or lab.tele == null or lab.tele.task == null: return false
	if lab.lesion_on or lab._is_reference() or lab._rec_pass or lab._is_dx(): return true
	return bool(lab.tele.score.get("passed", false)) or bool(Progress.best(lab.task_id)["passed"])

func toggle() -> void:
	if lab.lesion_on:
		lab.lesion_on = false; lab._lrun = null; lab._lesion_open = false; apply_ui(); lab._dirty = 0.0; lab._build_inspector(); return
	if not ok():
		lab._banner_raw("The lesion lab opens after a pass: pass the level, then cut and silence parts to see what each one does.")
		return
	if lab.banner != null: lab.banner.visible = false
	lab.layout.close_menu(); lab.layout.overlay_hide(); lab._overlay_dismissed = true
	lab.lesion_on = true; lab.editor.select({}); lab.editor.tool = "select"; lab.editor._connect_from = {}
	lab.layout.sync_connect_btn()
	apply_ui(); lab._build_inspector()

## Dev hook: SP_LESION=1|run|sweep opens the lesion lab (the reference circuit counts as passed), SP_LESIONS="pop:E_L,proj:3" sets lesions, run / sweep fill the table.
func env() -> void:
	await lab.get_tree().create_timer(0.5).timeout
	if not lab.lesion_on: toggle()
	for k in OS.get_environment("SP_LESIONS").split(","):
		if k != "": lab.lesions[k] = true
	lab.editor.lesions = lab.lesions; lab._dirty = 0.0; lab._build_inspector()
	await lab.get_tree().create_timer(0.5).timeout
	match OS.get_environment("SP_LESION"):
		"run": run([lab.lesions.duplicate()])
		"sweep": sweep()

func open() -> void:
	lab._overlay_dismissed = true; lab.layout.overlay_hide()
	if not lab.lesion_on: toggle()

func apply_ui() -> void:
	var editor := lab.editor
	editor.lesion_mode = lab.lesion_on; editor.editable = not lab.lesion_on and not lab._is_dx(); editor.lesions = lab.lesions; editor.queue_redraw()
	if lab.layout.palette_row != null and not Layout.phone: lab.layout.palette_row.visible = not lab.lesion_on and not lab._is_dx()

## The circuit changed or was replaced: the table no longer describes it.
func reset() -> void:
	lab.lesions = {}; lab.lesion_rows = []; lab._lesion_note = ""
	if lab.editor != null: lab.editor.lesions = lab.lesions

func click(h: Dictionary) -> void:
	if lab._lrun != null: return
	var key := ""
	match str(h.get("t", "")):
		"pop": key = "pop:" + str(h["id"])
		"proj": key = "proj:%d" % int(h["i"])
		"in":
			for ch in lab.circuit.get("inputs", []):
				if str(ch.get("name", "")) == str(h["name"]) and not (ch.get("targets", []) as Array).is_empty(): key = "in:" + str(h["name"])
	if key == "": return
	if lab.lesions.has(key): lab.lesions.erase(key)
	else:
		lab.lesions[key] = true
		if lab._is_dx(): lab._dx_probe("cut", key)   # a cut tried on the patient is a probe
	lab.editor.lesions = lab.lesions; lab.editor.queue_redraw(); lab._lesion_note = ""
	GameAudio.event("lesion_cut")
	lab._dirty = 0.0; lab._build_inspector()   # the live view restarts on the lesioned circuit

func run(sets: Array) -> void:
	if lab._lrun != null or lab.tele == null or lab.tele.task == null or Circuits.neuron_count(lab.circuit) == 0 or sets.is_empty(): return
	var jobs := []
	var have_base := false
	for r in lab.lesion_rows:
		if (r["lesions"] as Array).is_empty(): have_base = true
	if not have_base: jobs.append({"label": Lesion.label(lab.circuit, {}), "lesions": {}})
	for ls in sets: jobs.append({"label": Lesion.label(lab.circuit, ls), "lesions": ls})
	if lab._is_dx():
		for ls in sets: lab._dx_probe("run", Lesion.label(lab.circuit, ls))   # every lesion re-run is a probe: a sweep of all single lesions is as many as there are lesions
	if lab.tele.sim != null: lab.tele.sim.stop()
	lab._lrun = Lesion.Runner.new(); lab._lrun.mode = lab.mode; lab._lrun.variant = lab.variant; lab._lrun.begin(lab.task_id, lab.circuit, jobs); lab._lesion_note = ""
	lab._build_inspector()

func sweep() -> void:
	var sets := []
	for ls in Lesion.singles(lab.circuit):
		var lab_ := Lesion.label(lab.circuit, ls); var known := false
		for r in lab.lesion_rows:
			if r["label"] == lab_: known = true
		if not known: sets.append(ls)
	if sets.is_empty(): lab._lesion_note = "Every single lesion is already in the table."; lab._build_inspector(); return
	run(sets)

func cancel() -> void:
	lab._lrun = null; lab._reload(); lab._build_inspector()

func step() -> void:
	var lr := lab._lrun
	lr.step(12000)
	var pl := lab.insp_body.find_child("LesionProg", true, false) as Label if lab.insp_body != null else null
	if pl != null and not lr.done(): pl.text = "Running %d of %d: %s, seed %d of %d (%d%%)" % [lr.i + 1, lr.jobs.size(), lr.current_label(), lr.si + 1, lr.seeds.size(), int(100.0 * lr.fraction())]
	if not lr.done(): return
	if lr.failed != "": lab._banner_raw("The lesion run stopped: " + lr.failed)
	for res in lr.results:
		var at := -1
		for i in lab.lesion_rows.size():
			if lab.lesion_rows[i]["label"] == res["label"]: at = i
		if at >= 0: lab.lesion_rows[at] = res
		else: lab.lesion_rows.append(res)
	lab.lesion_rows.sort_custom(func(a, b):   # the intact baseline on top, then the most damaging lesion first
		var ea := (a["lesions"] as Array).is_empty(); var eb := (b["lesions"] as Array).is_empty()
		if ea != eb: return ea
		return float(a["score"]) < float(b["score"]))
	lab._lrun = null; lab._dirty = -1.0; lab._lesion_open = true; lab._reload(); lab._build_inspector()
	var sur := Surprise.lesion_improves(lab.lesion_rows)
	if not sur.is_empty() and not lab._surprise_done.has("les:" + str(sur["label"])):
		lab._surprise_done["les:" + str(sur["label"])] = true; GameAudio.event("celebrate")
		lab.celebration.toast("Surprise: a lesion that helps!", "With \"%s\" the score went UP by %d points. Something that part does is working against the task: why?" % [str(sur["label"]), int(roundf(float(sur["gain"])))], Kit.GREEN, 8.0)

func markdown() -> String: return Lesion.markdown(lab._title(), str(lab.circuit.get("name", "circuit")), int(lab.circuit.get("seed", 1)), lab.lesion_rows)

func export_table(kind: String) -> void:
	if lab.lesion_rows.is_empty(): return
	match kind:
		"md": DisplayServer.clipboard_set(markdown()); lab._lesion_note = "Markdown table copied to the clipboard."
		"csv": DisplayServer.clipboard_set(Lesion.csv(lab.lesion_rows)); lab._lesion_note = "CSV copied to the clipboard."
		"save":
			var dir := Lesion.save(lab.task_id, markdown(), Lesion.csv(lab.lesion_rows))
			lab._lesion_note = ("Saved .md and .csv to " + dir) if dir != "" else "Could not write the files."
	lab._build_inspector()

## The lesion lab's inspector: what to click, the Run / Sweep / Clear / Exit row, the progress line, and the table.
func inspect(small: int) -> void:
	var I := lab.insp
	I.add(I.head("LESION LAB", Kit.RED, "cut an axon, silence a population or cut a sensor"))
	var running := lab._lrun != null; var nseeds := Lesion.SEEDS.size(); var lesions := lab.lesions; var rows := lab.lesion_rows
	var names := []
	for k in lesions: names.append(Lesion.key_label(lab.circuit, str(k)))
	var compact := Layout.phone and not lab._lesion_open and not running   # M1: while picking, the phone sheet is one line of text and the buttons
	if compact:
		I.add(Kit.label(("Lesioned: " + ", ".join(names)) if not names.is_empty() else "Tap an axon, group or sensor to lesion it.", small, Kit.RED if not names.is_empty() else Kit.TEXT_SOFT, true))
	else:
		I.add(Kit.label(("Tap an axon, a population or a sensor card. “Run” re-runs the episode on %d seeds; “Sweep” tries every single lesion." % nseeds) if Layout.phone else "Click an axon, a population or a sensor card on the circuit. The live view restarts with it removed; “Run lesion” re-runs the whole episode on %d seeds (the world and the circuit both reseeded) and puts the mean and range in the table. “Sweep all singles” tries every single lesion." % nseeds, small, Kit.TEXT_SOFT, true))
		I.add(Kit.label(("Lesioned now: " + ", ".join(names)) if not names.is_empty() else "Nothing lesioned yet.", small, Kit.RED if not names.is_empty() else Kit.TEXT_SOFT, true))
	var br := HFlowContainer.new(); br.add_theme_constant_override("h_separation", 8); br.add_theme_constant_override("v_separation", 6); I.add(br)
	var rb := Kit.btn("Run" if Layout.phone else "Run lesion", func(): run([lab.lesions.duplicate()]), "amber", 18); rb.disabled = lesions.is_empty() or running; br.add_child(rb)
	var sw := Kit.btn("Sweep" if Layout.phone else "Sweep all singles", sweep, "panel", 18); sw.disabled = running; br.add_child(sw)
	var cl := Kit.btn("Clear" if Layout.phone else "Clear set", func(): lab.lesions.clear(); lab.editor.queue_redraw(); lab._dirty = 0.0; lab._build_inspector(), "panel", 18); cl.disabled = lesions.is_empty() or running; br.add_child(cl)
	if Layout.phone and not running and not rows.is_empty():
		br.add_child(Kit.ident(Kit.btn(("Hide table" if lab._lesion_open else "Table (%d)" % rows.size()), func(): lab._lesion_open = not lab._lesion_open; lab._build_inspector(), "green" if not lab._lesion_open else "panel", 18), "BtnTable"))
	br.add_child(Kit.btn("Exit", toggle, "ghost", 18))
	if running:
		var pl := Kit.label("Starting...", small, Kit.exc(), true); pl.name = "LesionProg"; I.add(pl)
		I.add(Kit.btn("Cancel", cancel, "panel", 18))
	if compact: return
	I.add(Kit.hsep())
	I.add(Kit.label("LESION TABLE  (%d rows, %d seeds each)" % [rows.size(), int(rows[0].get("n", nseeds))] if not rows.is_empty() else "LESION TABLE: empty. Set a lesion and press “Run lesion”.", small, Kit.TEXT if not rows.is_empty() else Kit.TEXT_SOFT, true))
	if Lesion.baseline_flaky(rows): I.add(Kit.label("The intact circuit fails on some seeds: a few points between rows are noise, not the lesion.", small, Kit.AMBER, true))
	var base_score := -1.0
	for r in rows:
		if (r["lesions"] as Array).is_empty(): base_score = float(r["score"])
	var grid := GridContainer.new(); grid.columns = 1 if Layout.phone or lab.size.x < 1100.0 else 2; grid.add_theme_constant_override("h_separation", 8); grid.add_theme_constant_override("v_separation", 6); grid.size_flags_horizontal = Control.SIZE_EXPAND_FILL; I.add(grid)
	for r in rows:
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
	if not rows.is_empty():
		var ex := HFlowContainer.new(); ex.add_theme_constant_override("h_separation", 8); ex.add_theme_constant_override("v_separation", 6); I.add(ex)
		ex.add_child(Kit.btn("Copy Markdown", func(): export_table("md"), "panel", 18)); ex.add_child(Kit.btn("Copy CSV", func(): export_table("csv"), "panel", 18)); ex.add_child(Kit.btn("Save files", func(): export_table("save"), "green", 18))
	if lab._lesion_note != "": I.add(Kit.label(lab._lesion_note, small, Kit.GREEN, true))
