class_name DiagnosisPanel
extends VBoxContainer
## The diagnosis case card (docs/DESIGN-0.3.md section 6): a working circuit with a hidden lesion and the patient's symptoms. The player probes it with the scopes
## and the lesion lab, forms a hypothesis from the candidate list, submits it, and gets the reveal with the explanation. The panel only draws; the lab keeps the
## state dictionary and the live view:
##   state = {picked: String, tries: int, probes: int, solved: bool, gave_up: bool, verdict: String, score: float}
## and answers the signals. `dx` is Task::diagnosis() ({circuit, symptoms, candidates, answer, labels, explain}: plain-language name and authored "why not" per candidate); `note` is Task::notebook() ({title, text, citation}), the reveal's explanation.
## SCORING (0.5): the case is scored on what it took: 100 minus a cost for every wrong guess and for every probe (a scope opened, a cut tried, a lesion re-run) beyond the first few, with a floor.
## A wrong pick says only that it is not that one: the reasons (`explain`) appear on the result card, once the case is solved or revealed.

signal picked(cand: String)
signal submit_pressed
## Wrong submissions before "Reveal answer" is offered (probing comes first; giving up is for after several misses).
const REVEAL_AFTER := 3
signal reveal_pressed
signal lesion_lab_pressed
var submit_row: Control = null   # the Submit / Reveal row: the inspector pins it below its scroll area, so it is never cut by the scroll edge

func setup(dx: Dictionary, state: Dictionary, note: Dictionary, lesion_lab_on: bool) -> DiagnosisPanel:
	for c in get_children(): remove_child(c); c.queue_free()
	Kit.ident(self, "DxPanel")
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
	pr.add_child(Kit.ident(Kit.btn("LEAVE LESION LAB" if lesion_lab_on else "LESION LAB", func(): lesion_lab_pressed.emit(), "amber" if lesion_lab_on else "cyan", 18), "BtnLesionLab"))
	pr.add_child(Kit.label("Scopes on the right; click a group to trace it." if Layout.phone else "Probes: every candidate connection is drawn, but one of them is dead. Watch which groups fire and which do not answer; or open the lesion lab and cut a suspected connection: a working one changes the symptoms or the activity downstream, the dead one changes nothing. Then name the lesion.", fs - 1, Kit.TEXT_SOFT, true))
	if not done:
		var pn := int(state.get("probes", 0)); var wn := int(state.get("tries", 0))
		add_child(Kit.ident(Kit.label("Used so far: %d probe%s (the first 3 are free), %d wrong guess%s. Each wrong guess and each probe beyond the free ones lowers the score of the case." % [pn, "" if pn == 1 else "s", wn, "" if wn == 1 else "es"], fs - 1, Kit.AMBER, true), "DxUsed"))
	# candidates
	add_child(Kit.label("Which connection is missing or lesioned?", fs, Kit.TEXT))
	var answer := str(dx.get("answer", ""))
	var labels: Dictionary = dx.get("labels", {}); var explain: Dictionary = dx.get("explain", {})
	for cand in dx.get("candidates", []):
		var cs := str(cand)
		var row := HBoxContainer.new(); row.add_theme_constant_override("separation", 8); add_child(row)
		var is_pick := str(state.get("picked", "")) == cs
		var kind := "panel"
		if done and cs == answer: kind = "green"
		elif is_pick: kind = "amber"
		var plain := str(labels.get(cs, ""))
		var b := Kit.btn(_cap(plain) if plain != "" else _cap(Circuits.candidate_text(cs)), func(): picked.emit(cs), kind, 17); Kit.ident(b, "Cand_" + cs); b.size_flags_horizontal = Control.SIZE_EXPAND_FILL; b.disabled = done and cs != answer and not is_pick
		b.alignment = HORIZONTAL_ALIGNMENT_LEFT; row.add_child(b)
	# submit and reveal
	if not done:
		var br := HFlowContainer.new(); br.add_theme_constant_override("h_separation", 8); add_child(br); submit_row = br
		var sb := Kit.btn("SUBMIT DIAGNOSIS", func(): state["last_submitted"] = str(state.get("picked", "")); submit_pressed.emit(), "green", 20); sb.disabled = str(state.get("picked", "")) == ""; br.add_child(sb)
		if int(state.get("tries", 0)) >= REVEAL_AFTER: br.add_child(Kit.btn("REVEAL ANSWER", func(): reveal_pressed.emit(), "panel", 18))   # not after the first wrong pick: probing comes first
		if int(state.get("tries", 0)) >= 1:
			add_child(Kit.label("Not that one (%d wrong so far)." % int(state.get("tries", 0)), fs, Kit.RED, true))
			add_child(Kit.label("Probe again: watch the groups at both ends of each candidate, or cut it in the lesion lab; a working connection changes something, the dead one does not.", fs - 1, Kit.TEXT_SOFT, true))
		return self
	var ok := bool(state.get("solved", false))
	var rc := PanelContainer.new(); rc.add_theme_stylebox_override("panel", Kit.sb(Kit.PANEL_HI, Kit.GREEN if ok else Kit.AMBER, 1, 10, 12, false)); add_child(rc)
	var rv := VBoxContainer.new(); rv.add_theme_constant_override("separation", 4); rc.add_child(rv)
	rv.add_child(Kit.shout("CORRECT" if ok else "THE ANSWER", 28 if not Layout.phone else 22, Kit.GREEN if ok else Kit.AMBER))
	rv.add_child(Kit.label("The lesion: %s." % (str(labels.get(answer, "")) if str(labels.get(answer, "")) != "" else Circuits.candidate_text(answer)), fs, Kit.TEXT, true))
	var pn := int(state.get("probes", 0)); var wn := int(state.get("tries", 0))
	var tally := "Used: %d probe%s (the first 3 are free), %d wrong guess%s." % [pn, "" if pn == 1 else "s", wn, "" if wn == 1 else "es"]
	if ok: tally = "Score %d of 100.  " % int(roundf(float(state.get("score", 0.0)))) + tally   # the engine's verdict says it in words (no digits); the numbers are shown here
	rv.add_child(Kit.ident(Kit.label(tally, fs, Kit.AMBER, true), "DxTally"))
	if str(state.get("verdict", "")) != "": rv.add_child(Kit.ident(Kit.label(str(state["verdict"]), fs, Kit.TEXT_SOFT, true), "DxResult"))
	if (not ok or str(state.get("verdict", "")) == "") and str(explain.get(answer, "")) != "": rv.add_child(Kit.label(str(explain[answer]), fs, Kit.TEXT_SOFT, true))
	# why each of the other candidates does not fit: shown only now, never after a wrong pick
	var others := []
	for cand in dx.get("candidates", []):
		if str(cand) != answer and str(explain.get(str(cand), "")) != "": others.append(str(cand))
	if not others.is_empty():
		rv.add_child(Kit.label("Why not the others", fs, Kit.TEXT))
		for cs in others:
			var pl := str(labels.get(cs, "")); var nm := _cap(pl) if pl != "" else _cap(Circuits.candidate_text(cs))
			rv.add_child(Kit.label("%s: %s" % [nm, str(explain[cs])], fs - 1, Kit.TEXT_SOFT, true))
	if str(note.get("text", "")) != "":
		rv.add_child(Kit.label(str(note.get("title", "")), fs, Kit.TEXT))
		rv.add_child(Kit.label(str(note["text"]), fs, Kit.TEXT_SOFT, true))
		if str(note.get("citation", "")) != "": rv.add_child(Kit.label(str(note["citation"]), fs - 1, Kit.CYAN, true))
	return self

static func _cap(s: String) -> String: return s.substr(0, 1).to_upper() + s.substr(1)
