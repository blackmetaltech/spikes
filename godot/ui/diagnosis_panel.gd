class_name DiagnosisPanel
extends VBoxContainer
## The diagnosis case card (docs/DESIGN-0.3.md section 6): a working circuit with a hidden lesion and the patient's symptoms. The player probes it with the scopes
## and the lesion lab, forms a hypothesis from the candidate list, submits it, and gets the reveal with the explanation. The panel only draws; the lab keeps the
## state dictionary and the live view:
##   state = {picked: String, tries: int, solved: bool, gave_up: bool}
## and answers the signals. `dx` is Task::diagnosis() ({circuit, symptoms, candidates, answer}); `note` is Task::notebook() ({title, text, citation}), the reveal's explanation.

signal picked(cand: String)
signal submit_pressed
signal reveal_pressed
signal lesion_lab_pressed

func setup(dx: Dictionary, state: Dictionary, note: Dictionary, lesion_lab_on: bool) -> DiagnosisPanel:
	for c in get_children(): remove_child(c); c.queue_free()
	add_theme_constant_override("separation", 6 if Layout.phone else 8)
	var fs := 17 if not Layout.phone else 15
	var done := bool(state.get("solved", false)) or bool(state.get("gave_up", false))
	# symptoms
	var sc := PanelContainer.new(); sc.add_theme_stylebox_override("panel", Kit.sb(Kit.PANEL_HI, Kit.AMBER, 1, 10, 10, false)); add_child(sc)
	var sv := VBoxContainer.new(); sv.add_theme_constant_override("separation", 2); sc.add_child(sv)
	sv.add_child(Kit.chip("PATIENT SYMPTOMS", Kit.AMBER, 14))
	sv.add_child(Kit.label(str(dx.get("symptoms", "No symptoms recorded.")), fs, Kit.TEXT, true))
	# probes
	var pr := HFlowContainer.new(); pr.add_theme_constant_override("h_separation", 8); pr.add_theme_constant_override("v_separation", 6); add_child(pr)
	pr.add_child(Kit.btn("LEAVE LESION LAB" if lesion_lab_on else "LESION LAB", func(): lesion_lab_pressed.emit(), "amber" if lesion_lab_on else "cyan", 18))
	pr.add_child(Kit.label("Scopes on the right; click a group to trace it." if Layout.phone else "Probes: the raster and traces on the right show the patient's circuit running. Click a group to trace it, or open the lesion lab to cut and silence parts and see what each one does. Then name the lesion.", fs - 1, Kit.TEXT_SOFT, true))
	# candidates
	add_child(Kit.label("WHICH CONNECTION IS MISSING OR LESIONED?", fs, Kit.TEXT))
	var answer := str(dx.get("answer", ""))
	for cand in dx.get("candidates", []):
		var cs := str(cand)
		var row := HBoxContainer.new(); row.add_theme_constant_override("separation", 8); add_child(row)
		var is_pick := str(state.get("picked", "")) == cs
		var kind := "panel"
		if done and cs == answer: kind = "green"
		elif is_pick: kind = "amber"
		var b := Kit.btn(_cap(Circuits.candidate_text(cs)), func(): picked.emit(cs), kind, 17); b.size_flags_horizontal = Control.SIZE_EXPAND_FILL; b.disabled = done and cs != answer and not is_pick
		b.alignment = HORIZONTAL_ALIGNMENT_LEFT; row.add_child(b)
	# submit and reveal
	if not done:
		var br := HFlowContainer.new(); br.add_theme_constant_override("h_separation", 8); add_child(br)
		var sb := Kit.btn("SUBMIT DIAGNOSIS", func(): submit_pressed.emit(), "green", 20); sb.disabled = str(state.get("picked", "")) == ""; br.add_child(sb)
		if int(state.get("tries", 0)) >= 1: br.add_child(Kit.btn("REVEAL ANSWER", func(): reveal_pressed.emit(), "panel", 18))
		if int(state.get("tries", 0)) >= 1: add_child(Kit.label("Not that one (%d wrong so far). Probe again: cut the candidate in the lesion lab and compare with the symptoms." % int(state.get("tries", 0)), fs, Kit.RED, true))
		return self
	var ok := bool(state.get("solved", false))
	var rc := PanelContainer.new(); rc.add_theme_stylebox_override("panel", Kit.sb(Kit.PANEL_HI, Kit.GREEN if ok else Kit.AMBER, 1, 10, 12, false)); add_child(rc)
	var rv := VBoxContainer.new(); rv.add_theme_constant_override("separation", 4); rc.add_child(rv)
	rv.add_child(Kit.shout("CORRECT" if ok else "THE ANSWER", 28 if not Layout.phone else 22, Kit.GREEN if ok else Kit.AMBER))
	rv.add_child(Kit.label("The lesion: %s." % Circuits.candidate_text(answer), fs, Kit.TEXT, true))
	if str(note.get("text", "")) != "":
		rv.add_child(Kit.label(str(note.get("title", "")), fs, Kit.TEXT))
		rv.add_child(Kit.label(str(note["text"]), fs, Kit.TEXT_SOFT, true))
		if str(note.get("citation", "")) != "": rv.add_child(Kit.label(str(note["citation"]), fs - 1, Kit.CYAN, true))
	return self

static func _cap(s: String) -> String: return s.substr(0, 1).to_upper() + s.substr(1)
