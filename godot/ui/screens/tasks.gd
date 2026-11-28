class_name TasksScreen
extends Screen
## The campaign map. Five chapters (Invertebrates, Spinal cord, Heart, Breathing, Autonomic), each drawn as its organ with the chapter's levels as pins on it:
## a SPINE of micro-puzzles (small pins), tasks and a boss (large pin) strung along the anatomy in play order, and optional BRANCHES (challenge variants,
## diagnosis cases) hanging off their task. Hover or select a pin to see its concepts. Pick a level to read its brief and press Build (an empty bench with the
## task's sensors and motors); Reference (the reference circuit already wired) appears only once the level is passed, or in Guided mode. Chapters, kinds and order come from Campaign (Task::chapter(), level_kind(),
## on_spine()); levels the engine does not have yet show as dashed "not installed" ghosts. Header: the difficulty mode (Guided / Standard / Researcher, saved,
## switchable any time), NOTEBOOK, PARTS BIN and SANDBOX.
## What each row of chapter tabs is (the second line of its label); the Theory row says it opens after chapter I.
const ROW_SUB := {"body": "organ systems", "bestiary": "model animals", "theory": "ideas, after chapter I", "atlas": "measured wiring, after chapter I"}
const SHORT := {"invertebrates": "Invert.", "spinal": "Spinal", "heart": "Heart", "breathing": "Breathing", "autonomic": "Autonomic", "worm": "Worm", "leech": "Leech", "aplysia": "Slug", "fly_compass": "Compass", "fly_mb": "Mushroom", "crab": "Crab", "dynamics": "Dynamics", "rates": "Rates", "memory": "Memory", "tuning": "Tuning", "a_worm": "Worm wiring", "a_compass": "Compass wiring"}
const KIND_NAME := {"micro": "MICRO-PUZZLE", "task": "TASK", "boss": "BOSS", "diagnosis": "DIAGNOSIS", "variant": "VARIANT"}
var back_btn: Button
var first: Control
var chapter := ""
var sel := ""
var map: MapCanvas
var detail_box: VBoxContainer
var detail_scroll: ScrollContainer
var tab_btns := {}
var mode_step: Kit.Stepper
var _brief_full := false   # the brief's More expander on the detail card

func default_focus() -> Control: return first if first != null else back_btn

func build() -> void:
	first = null; tab_btns = {}
	Playlog.leave()   # back on the map: whatever level was open in the lab is over
	var f := f(); var phone := Layout.phone
	add_child(Kit.bench())
	var margin := MarginContainer.new(); margin.set_anchors_preset(Control.PRESET_FULL_RECT)
	var mx := int(56 * f) if not phone else 16; var my := int(30 * f) if not phone else 8
	for side in ["left", "right"]: margin.add_theme_constant_override("margin_" + side, mx)
	margin.add_theme_constant_override("margin_top", my); margin.add_theme_constant_override("margin_bottom", my); add_child(margin)
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", int(14 * f) if not phone else 6); margin.add_child(v)
	var hb := Kit.page_head("Campaign", func(): app.back(), phone); var head: HBoxContainer = hb[0]; back_btn = hb[1]; v.add_child(head)
	var bs := int(22 * f) if not phone else 18
	(head.get_node("PageTitle") as Control).size_flags_horizontal = Control.SIZE_EXPAND_FILL
	mode_step = Kit.stepper([["guided", "Guided"], ["standard", "Standard"], ["researcher", "Researcher"]], Progress.mode(), func(id): _on_mode(str(id)), 170.0 if not phone else 112.0, int(24 * f) if not phone else 15, 44.0 if not phone else 32.0)
	mode_step.tooltip_text = "Difficulty. Switch any time. Guided: recipe in the brief. Standard: “Show recipe” costs the third star. Researcher: no recipe, a pass needs “Test x5”."
	Kit.ident(mode_step, "StepMode"); head.add_child(mode_step)
	head.add_child(Kit.ident(Kit.btn("Notebook" if not phone else "Notes", func(): app.show_screen("notebook"), "panel", bs), "BtnNotebook"))
	head.add_child(Kit.ident(Kit.btn("Parts bin" if not phone else "Parts", func(): app.show_screen("parts"), "panel", bs), "BtnPartsBin"))
	head.add_child(Kit.ident(Kit.btn("Sandbox" if not phone else "Lab", func(): app.open_lab(""), "ghost", bs), "BtnSandbox"))
	head.add_child(Kit.ident(Kit.btn("Tutorial", func(): app.start_tutorial(), "ghost", bs), "BtnTutorial"))   # replays Plate 0
	if not Campaign.ext_ok():
		v.add_child(Kit.label("The spike-gd GDExtension is not loaded. Run scripts/build-gd.sh.", 22, Kit.RED, true)); return
	if chapter == "": chapter = app.campaign_chapter if app != null and app.campaign_chapter != "" else _default_chapter()
	if sel == "": sel = app.campaign_node if app != null else ""
	v.add_child(_tabs(f, phone))
	var body := HBoxContainer.new(); body.size_flags_vertical = Control.SIZE_EXPAND_FILL; body.add_theme_constant_override("separation", int(18 * f) if not phone else 10); v.add_child(body)
	var mp := Kit.ink_card(Kit.PANEL, 0); mp.size_flags_horizontal = Control.SIZE_EXPAND_FILL; mp.size_flags_stretch_ratio = 2.3; mp.size_flags_vertical = Control.SIZE_EXPAND_FILL; body.add_child(mp)
	map = MapCanvas.new(); map.screen = self; map.size_flags_vertical = Control.SIZE_EXPAND_FILL; map.size_flags_horizontal = Control.SIZE_EXPAND_FILL; mp.add_child(map)
	var dp := Kit.panel(Kit.PANEL, int(20 * f) if not phone else 10); dp.size_flags_horizontal = Control.SIZE_EXPAND_FILL; dp.size_flags_stretch_ratio = 1.0; dp.size_flags_vertical = Control.SIZE_EXPAND_FILL; body.add_child(dp)
	detail_scroll = ScrollContainer.new(); detail_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED; dp.add_child(detail_scroll)
	detail_box = VBoxContainer.new(); Kit.ident(detail_box, "DetailBox"); detail_box.size_flags_horizontal = Control.SIZE_EXPAND_FILL; detail_box.add_theme_constant_override("separation", int(10 * f) if not phone else 6); detail_scroll.add_child(detail_box)
	_show_chapter(chapter)

## The player switched the difficulty mode: save it, and relabel everything that shows stars.
func _on_mode(m: String) -> void:
	Progress.set_mode(m)
	Playlog.event("mode", {"mode": m})
	if map != null:
		map.queue_redraw()
		for c in map.cards: c.queue_redraw()
	_fill_detail()

## The chapter to open on: the first one with an open, unfinished spine level.
func _default_chapter() -> String:
	for c in Campaign.CHAPTERS:
		for id in Campaign.spine_ids(c["id"]):
			if Campaign.unlocked(id) and not Campaign.passed(id): return c["id"]
	return Campaign.CHAPTERS[0]["id"]

## The chapter tabs: a wing selector (Body / Bestiary / Theory) and ONE row with the chapters of the current wing (three rows of sixteen equal tabs took a
## quarter of the screen), and "Continue: <next level>" at the right. Phone: the wing switch over a sideways strip of the wing's chapters, whose right
## edge fades when more chapters are off screen.
const WINGS := [["body", "Body"], ["bestiary", "Bestiary"], ["theory", "Theory"], ["atlas", "Atlas"]]
var wing := ""
var _wing_btns := {}
var _chapter_row: HBoxContainer
var _strip: ScrollContainer
var _fade: Control
func _tabs(f: float, phone: bool) -> Control:
	wing = Campaign.wing_of(chapter); _wing_btns = {}
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", 8 if not phone else 4)
	var top := HBoxContainer.new(); top.add_theme_constant_override("separation", 8 if not phone else 4); v.add_child(top)
	for w in WINGS:
		var wid: String = w[0]
		var b := Kit.btn(str(w[1]), func(): _show_wing(wid), "amber" if wid == wing else "panel", int(22 * f) if not phone else 16, Vector2(0, 0)); Kit.ident(b, "Wing_" + wid)
		b.tooltip_text = ROW_SUB[wid]; _wing_btns[wid] = b; top.add_child(b)
	if not phone:   # the phone layout has no room for it: do not create a label nobody adds (it would leak as an orphan node)
		var sub := Kit.italic(ROW_SUB.get(wing, ""), int(16 * f), Kit.TEXT_SOFT); sub.name = "WingSub"; sub.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		top.add_child(sub)
	var sp := Control.new(); sp.size_flags_horizontal = Control.SIZE_EXPAND_FILL; sp.mouse_filter = Control.MOUSE_FILTER_IGNORE; top.add_child(sp)
	var nx := _continue_target()
	if nx != "":
		var cb := Kit.btn(("Continue: %s →" % str(Campaign.info(nx).get("title", nx))) if not phone else "Continue →", func(): _continue(nx), "amber", int(22 * f) if not phone else 16, Vector2(0, 0)); Kit.ident(cb, "BtnContinue")
		cb.tooltip_text = "Open the next level you have not passed: %s" % str(Campaign.info(nx).get("title", nx)); top.add_child(cb)
	_chapter_row = HBoxContainer.new(); _chapter_row.add_theme_constant_override("separation", 10 if not phone else 6)
	if phone:
		var holder := Control.new(); holder.custom_minimum_size.y = 44.0; v.add_child(holder)
		_strip = ScrollContainer.new(); _strip.vertical_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED; _strip.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_SHOW_NEVER
		_strip.set_anchors_preset(Control.PRESET_FULL_RECT); _strip.set_meta("nowrap", true)   # WrapGuard must not turn this strip into a wrapping flow: it scrolls sideways on purpose
		_strip.add_child(_chapter_row); holder.add_child(_strip)
		_fade = _Fade.new(); _fade.set_anchors_preset(Control.PRESET_RIGHT_WIDE); _fade.offset_left = -48.0; holder.add_child(_fade)
		_strip.get_h_scroll_bar().value_changed.connect(func(_x): _update_fade())
		_strip.resized.connect(_update_fade)
	else: v.add_child(_chapter_row)
	_fill_chapters(f, phone)
	return v

## The right-edge fade of the phone's chapter strip: paper fading in over the last 48 px while more chapters are off screen.
class _Fade extends Control:
	func _init() -> void: mouse_filter = Control.MOUSE_FILTER_IGNORE
	func _draw() -> void:
		for i in 12:
			var x := size.x * float(i) / 12.0
			draw_rect(Rect2(x, 0, size.x / 12.0 + 0.5, size.y), Color(Kit.PAPER, float(i + 1) / 13.0))
		draw_string(Fonts.bold, Vector2(size.x - 16.0, size.y * 0.5 + 6.0), "›", HORIZONTAL_ALIGNMENT_LEFT, -1, Kit.fs(18), Kit.TEXT_SOFT)

func _update_fade() -> void:
	if _fade == null or _strip == null: return
	var bar := _strip.get_h_scroll_bar()
	_fade.visible = bar.max_value - bar.page > 1.0 and bar.value < bar.max_value - bar.page - 2.0

func _fill_chapters(f: float, phone: bool) -> void:
	for c in _chapter_row.get_children(): _chapter_row.remove_child(c); c.queue_free()
	tab_btns = {}
	var roman := ["I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X", "XI", "XII", "XIII", "XIV", "XV", "XVI"]
	var ci := 0
	for c in Campaign.CHAPTERS:
		ci += 1
		var id: String = c["id"]
		if Campaign.wing_of(id) != wing: continue
		var open := Campaign.chapter_open(id); var n := Campaign.installed_count(id); var st := Campaign.star_total(id)
		var txt := str(SHORT[id]) if phone else (("%s. %s" % [roman[mini(ci - 1, roman.size() - 1)], str(c["title"])]) if wing == "body" else str(c["title"]).trim_prefix("Bestiary: ").trim_prefix("Theory: "))
		if not open and not phone: txt += "  (locked)"
		var b := Kit.btn(txt, func(): _show_chapter(id), "amber" if id == chapter else "panel", int(20 * f) if not phone else 16, Vector2(0, 0)); Kit.ident(b, "Chapter_" + id)
		b.size_flags_horizontal = Control.SIZE_EXPAND_FILL if not phone else Control.SIZE_SHRINK_BEGIN
		b.tooltip_text = ("%s: %d of %d levels passed, %d of %d stars" % [c["title"], Campaign.passed_count(id), n, st.x, st.y]) if n > 0 else ("%s: no tasks installed yet" % c["title"])
		tab_btns[id] = b; _chapter_row.add_child(b)
	if phone: call_deferred("_update_fade")

## Switch wing: its chapters fill the row and the map shows the wing's chapter you were last on (else its first open one).
var _wing_last := {}
func _show_wing(w: String) -> void:
	if w == wing and chapter != "": return
	if chapter != "": _wing_last[wing] = chapter
	var target := str(_wing_last.get(w, ""))
	if target == "":
		for c in Campaign.CHAPTERS:
			if target == "" and Campaign.wing_of(str(c["id"])) == w and Campaign.chapter_open(str(c["id"])): target = str(c["id"])
		for c in Campaign.CHAPTERS:
			if target == "" and Campaign.wing_of(str(c["id"])) == w: target = str(c["id"])
	_show_chapter(target)

## The level "Continue" opens: the first open, unpassed spine level in campaign order (none once everything open is passed).
func _continue_target() -> String:
	for id in Campaign.order():
		var tid := str(id)
		if Campaign.unlocked(tid) and not Campaign.passed(tid) and bool(Campaign.info(tid).get("spine", true)): return tid
	return ""

func _continue(tid: String) -> void:
	if app == null: return
	app.campaign_chapter = Campaign.chapter_of(tid); app.campaign_node = tid
	app.open_lab(tid)

func _show_chapter(id: String) -> void:
	chapter = id
	if app != null: app.campaign_chapter = id
	if _chapter_row != null and Campaign.wing_of(id) != wing:
		wing = Campaign.wing_of(id); _fill_chapters(f(), Layout.phone)
		for w in _wing_btns: Kit.skin(_wing_btns[w], "amber" if w == wing else "panel", 10 if Layout.phone else int(22 * f() * 0.6))
		var ws := find_child("WingSub", true, false) as Label
		if ws != null: ws.text = ROW_SUB.get(wing, "")
	for k in tab_btns: Kit.skin(tab_btns[k], "amber" if k == id else "panel", 10 if Layout.phone else int(20 * f() * 0.6))
	var nodes := Campaign.nodes(id)
	var ok := false
	for n in nodes:
		if n["id"] == sel: ok = true
	if not ok:   # the first open unfinished spine level, else the first installed one
		sel = ""
		for n in nodes:
			if sel == "" and n["installed"] and bool(n["spine"]) and Campaign.unlocked(n["id"]) and not Campaign.passed(n["id"]): sel = n["id"]
		for n in nodes:
			if sel == "" and n["installed"]: sel = n["id"]
		if sel == "" and not nodes.is_empty(): sel = nodes[0]["id"]
	map.set_chapter(id, nodes); first = map.first_card() if map.first_card() != null else back_btn
	_fill_detail()
	if Kit.pad_mode and app != null: app.pad.ensure_focus(true)

func select_node(id: String) -> void:
	if id != sel: _brief_full = false
	sel = id
	if app != null: app.campaign_node = id
	map.update_labels(id)
	map.queue_redraw()
	if map.overlay != null: map.overlay.queue_redraw()
	for c in map.cards: c.queue_redraw()
	_fill_detail()

func _chips_row(label: String, items: Array, col: Color, f: float, phone: bool, soft := false) -> void:
	if items.is_empty(): return
	var row := HFlowContainer.new(); row.add_theme_constant_override("h_separation", 8); row.add_theme_constant_override("v_separation", 6); detail_box.add_child(row)
	var kl := Kit.small_caps(label, int(15 * f) if not phone else 15, Kit.TEXT_SOFT); kl.custom_minimum_size.x = 112 * f if not phone else 100; kl.size_flags_vertical = Control.SIZE_SHRINK_CENTER; row.add_child(kl)
	for it in items:
		var c := Kit.chip(str(it), col, 15)
		if soft:
			(c.get_child(0) as Label).label_settings = Kit.ls(15, Kit.TEXT, true)
			c.add_theme_stylebox_override("panel", Kit.sb(Kit.PANEL_HI, Kit.TEXT_SOFT, 1, 6, 8, false))
		row.add_child(c)

## The three star conditions, worded for the difficulty mode (Progress.stars_for decides what earns them; the mode only changes how the pass is judged).
## Star 2 is the five worlds of CHECK; star 3 is the level's own challenge, with its text (Progress.challenge), never an unstated budget. Guided stops at two stars.
func _star_tiers(par: int, diagnosis := false, tid := "") -> Array:
	var m := Progress.mode()
	if diagnosis and tid != "" and Campaign.is_bet(tid):   # a prediction bet is a pass by committing a right answer before the run
		return [["Commit a bet that the run bears out", 1], ["Be right on the first bet (no wrong bets)", 2], ["Right on the first bet, within the free instrument reads", 3]] if m != "guided" \
			else [["Commit a bet that the run bears out", 1], ["Be right on the first bet (no wrong bets)", 2]]
	if diagnosis:   # a diagnosis case is a pass by naming the cause: the second star is a right first answer, the third one after a probe that tells the candidates apart
		return [["Name the cause", 1], ["Name it on the first try (no wrong answers)", 2], ["First try, after at least one probe that tells the candidates apart, and few probes", 3]] if m != "guided" \
			else [["Name the cause", 1], ["Name it on the first try (no wrong answers)", 2]]
	var t1 := "Pass the task"; var t2 := "CHECK passes all five worlds"
	var ch := str(Progress.challenge(tid)["text"]) if tid != "" else ""
	var t3 := "Challenge: " + (ch if ch != "" else "pass on all five worlds with a small circuit")
	if m == "guided": return [["Pass the task (wide bands, hints free)", 1], [t2 + " (Guided stops at two stars)", 2]]
	elif m == "standard": t3 += " Never open “Show recipe”" + (" (used)." if tid != "" and Progress.recipe_used(tid) else ".")
	elif m == "researcher": t1 = "Pass all five worlds under noise (CHECK, required)"; t2 = "CHECK passes all five worlds with tight bands (earns the Researcher seal)"
	return [[t1, 1], [t2, 2], [t3, 3]]

func _fill_detail() -> void:
	for c in detail_box.get_children():
		detail_box.remove_child(c); c.queue_free()
	var f := f(); var phone := Layout.phone
	var cd := Campaign.chapter_def(chapter)
	var node := {}
	for n in Campaign.nodes(chapter):
		if n["id"] == sel: node = n
	var small := int(18 * f) if not phone else 15
	if node.is_empty():
		detail_box.add_child(Kit.display(str(cd["title"]), int(34 * f) if not phone else 24, Kit.TEXT))
		detail_box.add_child(Kit.label(str(cd["blurb"]), small, Kit.TEXT_SOFT, true)); return
	var tid := str(node["task"]); var kind := str(node["kind"]); var is_var := kind == "variant"
	var info := Campaign.info(tid) if node["installed"] else {}
	var kicker := Kit.small_caps("%s  ·  %s" % [str(cd["title"]), "BET" if kind == "diagnosis" and Campaign.is_bet(tid) else str(KIND_NAME.get(kind, "TASK"))], int(16 * f) if not phone else 15, Kit.MOTOR)
	kicker.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART; kicker.custom_minimum_size.x = 80; detail_box.add_child(kicker)
	detail_box.add_child(Kit.display(str(node["title"]), int(36 * f) if not phone else 24, Kit.TEXT))
	if not node["installed"]:
		detail_box.add_child(Kit.label("Not installed in this build yet. It appears here as soon as the engine has the task; nothing to do on your side.", small, Kit.TEXT_SOFT, true))
		detail_box.add_child(Kit.label(str(cd["blurb"]), small, Kit.TEXT_SOFT, true)); return
	var open := Campaign.unlocked(sel); var best: Dictionary = Progress.best(tid); var par := Progress.budget(tid)
	var done := Campaign.passed(sel)
	if is_var: best = {"value": 0.0, "passed": done, "stars": 1 if done else 0, "modes": {}, "hints": {}, "variants": []}
	var chips := HFlowContainer.new(); chips.add_theme_constant_override("h_separation", 8); chips.add_theme_constant_override("v_separation", 6); detail_box.add_child(chips)
	chips.add_child(Kit.chip("%d s episode" % int(round(float(info["duration_ms"]) / 1000.0)), Kit.PANEL_HI, 15)); (chips.get_child(0).get_child(0) as Label).label_settings = Kit.ls(15, Kit.TEXT, true)
	if not is_var: chips.add_child(Kit.chip("BEST %d" % int(round(float(best["value"]))) if float(best["value"]) > 0.0 else "NOT TRIED", Kit.success() if bool(best["passed"]) else Kit.exc(), 15))
	if done: chips.add_child(Kit.chip("PASSED" if not is_var else "DONE", Kit.success(), 15))
	if not open: chips.add_child(Kit.chip("LOCKED", Kit.RED, 15))
	if not bool(node["spine"]): chips.add_child(Kit.chip("OPTIONAL", Kit.TEXT_SOFT, 15))
	if str(info["view"]) != "polyline": chips.add_child(Kit.chip(str(info["view"]).to_upper() + ("" if Layout.phone else " VIEW"), Kit.MOTOR, 15))   # "DASHBOARD VIEW" does not fit the phone card's row beside the other two chips: it dropped to a line of its own
	var bt := HBoxContainer.new(); bt.add_theme_constant_override("separation", 10); detail_box.add_child(bt)
	var sid := tid; var vid := str(node["variant"]); var synth := bool(info.get("synthetic", false))
	var go := func(ref: bool):
		Campaign.pending_variant = vid; app.open_lab(sid, ref)   # the lab logs the visit (Playlog.level_start), once
	var b := Kit.btn("Build" if open else "Locked", func(): if open and not synth: go.call(false), "amber" if open else "panel", int(24 * f) if not phone else 19); Kit.ident(b, "BtnBuild"); b.size_flags_horizontal = Control.SIZE_EXPAND_FILL; bt.add_child(b)
	if not open or synth: b.disabled = true
	if not is_var and (done or Progress.mode() == "guided"):   # the reference circuit is the solution: only after a pass, or in Guided mode
		var rb := Kit.btn("Reference", func(): if not synth: go.call(true), "panel", int(24 * f) if not phone else 19); rb.size_flags_horizontal = Control.SIZE_EXPAND_FILL; rb.disabled = synth; bt.add_child(rb)
		rb.tooltip_text = "Open the reference circuit, already wired. It earns no stars."
	if not open: detail_box.add_child(Kit.label(Campaign.lock_reason(sel) + ".", small, Kit.RED, true))
	elif Progress.mode() == "guided" and Campaign.is_wing(chapter) and not Campaign.wing_gate_met(chapter):
		detail_box.add_child(Kit.label("Guided mode leaves this chapter open; its prerequisite is %s." % str(Campaign.WING_GATES[chapter]["title"]), 15, Kit.TEXT_SOFT, true))
	if is_var:
		detail_box.add_child(Kit.label("A challenge variant of %s. It never gates the spine." % str(info["title"]), small, Kit.TEXT_SOFT, true))
	else:
		# stars, worded for the difficulty mode, lit from this mode's best
		var m := Progress.mode(); var bm := Progress.best_in(tid, m)
		var sh := HBoxContainer.new(); sh.add_theme_constant_override("separation", 8); detail_box.add_child(sh)
		sh.add_child(Kit.small_caps("Stars", int(16 * f) if not phone else 15, Kit.TEXT_SOFT))
		sh.add_child(Kit.chip(Progress.mode_name(m).to_upper() + " MODE", Kit.exc(), 15))
		for t in _star_tiers(par, kind == "diagnosis", tid):
			var r := HBoxContainer.new(); r.add_theme_constant_override("separation", 10); detail_box.add_child(r)
			var st := _Star.new(); st.lit = int(bm["stars"]) >= int(t[1]); st.custom_minimum_size = Vector2(26, 26); r.add_child(st)
			var l := Kit.label(str(t[0]), int(17 * f) if not phone else 15, Kit.TEXT if st.lit else Kit.TEXT_SOFT, true); l.size_flags_vertical = Control.SIZE_SHRINK_CENTER; l.size_flags_horizontal = Control.SIZE_EXPAND_FILL; r.add_child(l)
		var per := []
		for mm in Progress.MODES:
			var br := Progress.best_in(tid, mm)
			per.append("%s: %s" % [Progress.mode_name(mm), (("%d of %d stars" % [int(br["stars"]), 2 if mm == "guided" else 3]) + (", sealed" if bool(br.get("seal", false)) else "")) if bool(br["passed"]) else "not yet"])
		detail_box.add_child(Kit.label("   ".join(per), 15, Kit.TEXT_SOFT, true))
	var brief := str(info["brief"]); var ld := LabInspector.lead(brief, 230 if not phone else 170)
	detail_box.add_child(Kit.label(brief if _brief_full or not bool(ld[1]) else str(ld[0]), small, Kit.TEXT_SOFT, true))
	if not _brief_full and bool(ld[1]):   # what to build and the pass rule stay in view, not behind More
		var bl := LabInspector.build_lines(brief, str(ld[0]), 420 if not phone else 220)
		if bl != "":
			var brow := HBoxContainer.new(); brow.add_theme_constant_override("separation", 8); detail_box.add_child(brow)
			var bchip := Kit.chip("TO BUILD", Kit.exc(), 14); bchip.size_flags_vertical = Control.SIZE_SHRINK_BEGIN; brow.add_child(bchip)
			var btext := Kit.label(bl, small, Kit.TEXT, true); btext.size_flags_horizontal = Control.SIZE_EXPAND_FILL; brow.add_child(btext)
		var pl := LabInspector.pass_lines(brief, str(ld[0]) + " " + bl, 230 if not phone else 150)
		if pl != "":
			var pr := HBoxContainer.new(); pr.add_theme_constant_override("separation", 8); detail_box.add_child(pr)
			var pc := Kit.chip("TO PASS", Kit.AMBER, 14); pc.size_flags_vertical = Control.SIZE_SHRINK_BEGIN; pr.add_child(pc)
			var pt := Kit.label(pl, small, Kit.TEXT, true); pt.size_flags_horizontal = Control.SIZE_EXPAND_FILL; pr.add_child(pt)
	if bool(ld[1]):
		var mb := Kit.btn("Less" if _brief_full else "More…", func(): _brief_full = not _brief_full; _fill_detail(), "ghost", 16); mb.size_flags_horizontal = Control.SIZE_SHRINK_BEGIN; detail_box.add_child(mb)
	if str(info.get("symptoms", "")) != "": detail_box.add_child(Kit.label("Patient: " + str(info["symptoms"]), small, Kit.TEXT, true))
	_chips_row("NEW IDEA", (node["concepts_new"] as Array).map(func(c): return Campaign.concept_short(str(c))), Kit.exc(), f, phone)
	_chips_row("USES", (node["concepts_used"] as Array).map(func(c): return Campaign.concept_short(str(c))), Kit.GAMBOGE, f, phone, true)
	if not is_var and bool(node["installed"]):   # what the level builds on and has not been passed yet: "needs: <concept> (from <level>)"
		var nd := []
		for n2 in Campaign.needs(tid):
			if not bool(n2["met"]): nd.append("%s (from %s)" % [str(n2["title"]), str(n2["level_title"])])
		_chips_row("NEEDS", nd, Kit.RED, f, phone)
	if not is_var:
		var hu := Progress.hints_used(tid)
		if int(hu["count"]) > 0: detail_box.add_child(Kit.label("Hints used: %d (highest tier %d). Recorded, never punished." % [int(hu["count"]), int(hu["max_tier"])], 15, Kit.TEXT_SOFT, true))
		var vs: Dictionary = info["variants"]
		if not vs.is_empty():
			var dn := Progress.variants_done(tid); var k := 0
			for vk in vs: k += 1 if dn.has(vk) else 0
			detail_box.add_child(Kit.label("Challenge variants: %d of %d done" % [k, vs.size()], 15, Kit.TEXT_SOFT, true))
	for pair in [["SENSORS", info["sensors"], Kit.SENSOR], ["MOTORS", info["motors"], Kit.MOTOR], ["MODULATORS", info["modulators"], Kit.GAMBOGE]]:
		_chips_row(pair[0], pair[1], pair[2], f, phone)

class _Star extends Control:
	var lit := false
	func _draw() -> void: ScoreCard.draw_star_on(self, size * 0.5, minf(size.x, size.y) * 0.46, Kit.AMBER if lit else Color(Kit.LINE, 0.95))

## One level on the map: a pin on the anatomy. Tasks and bosses carry a small label plate (title, stars); micro-puzzles (small pin), variants (diamond) and
## diagnosis cases (square pin, with a label plate) carry their name; micro-puzzles and variants are bare pins, and their name and concept chips show on hover or selection (MapCanvas overlay). Bosses are large pins.
## The detail (brief, lock reason, best score) lives in the side panel.
class NodeCard extends Button:
	var node := {}
	var screen: TasksScreen
	var idx := 0
	var side := "r"
	var gap := 0.0
	var disc := Vector2.ZERO      # centre of the pin disc in local coordinates
	var _plate := Rect2()
	var _leader := false          # the plate sits away from its pin: a leader line joins them
	var spine_no := 0             # 1, 2, 3 ... along the chapter's spine (play order); 0 = a branch
	func _init() -> void:
		flat = true; focus_mode = Kit.focus_mode()
		for n in ["normal", "hover", "pressed", "disabled", "focus"]: add_theme_stylebox_override(n, StyleBoxEmpty.new())
	func min_text_px() -> float: return float(Kit.fs(14)) * Kit.ppl
	func kind() -> String: return str(node.get("kind", "task"))
	## Only the selected pin and its neighbours on the spine (and the branches of the selected level) carry a label plate: twelve plates at once buried the
	## anatomy. The others are numbered pins; hovering one names it.
	func has_plate() -> bool:
		if not (kind() in ["task", "boss", "micro", "diagnosis"]): return false
		var m := get_parent() as MapCanvas
		return m == null or m.labelled.is_empty() or m.labelled.has(str(node["id"]))
	func _R() -> float:
		var k := kind(); var ph := Layout.phone
		match k:
			"micro": return 8.0 if ph else 10.0
			"boss": return 19.0 if ph else 25.0
			"variant": return 7.0 if ph else 9.0
			"diagnosis": return 9.0 if ph else 12.0
		return 13.0 if ph else 17.0
	func _status() -> Array:
		var id := str(node["id"]); var inst: bool = node["installed"]
		var best: Dictionary = Progress.best(Campaign.task_of(id)) if inst and kind() != "variant" else {"value": 0.0, "passed": false, "stars": 0}
		if kind() == "variant": best = {"value": 0.0, "passed": Campaign.passed(id), "stars": Campaign.stars(id)}
		if not inst: return ["NOT YET", Kit.TEXT_SOFT, best]
		if not Campaign.unlocked(id): return ["LOCKED", Kit.RED, best]
		if float(best["value"]) > 0.0: return ["best %d" % int(round(float(best["value"]))), Kit.TEXT_SOFT, best]
		return ["NOT TRIED", Kit.exc(), best]
	## Size the button for its label and put the disc on `anchor` (map coordinates), keeping the whole label inside `bounds`.
	func place(anchor: Vector2, side_: String, bounds: Rect2, gap_ := 0.0) -> void:
		side = side_; gap = gap_; var R := _R()
		if not has_plate():
			var e := 2.0 * R + 6.0; size = Vector2(e, e); disc = Vector2(e, e) * 0.5; _plate = Rect2()
			position = anchor - disc; custom_minimum_size = Vector2.ZERO; return
		var ft := Kit.fs(16 if not Layout.phone else 15); var fz := Kit.fs(14)
		var title := str(node["title"]); var st := _status()
		var tw := Fonts.bold.get_string_size(title, HORIZONTAL_ALIGNMENT_LEFT, -1, ft).x
		var sw := 56.0 + (0.0 if Layout.phone else Fonts.regular.get_string_size(str(st[0]), HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x)
		var lw := maxf(tw, sw) + 18.0; var lh := float(ft) + float(fz) + 16.0
		var W := 0.0; var H := 0.0; var d := Vector2.ZERO
		match side:
			"l": W = lw + gap + 2.0 * R + 8.0; H = maxf(lh, 2.0 * R + 6.0); d = Vector2(W - R - 2.0, H * 0.5); _plate = Rect2(0, (H - lh) * 0.5, lw, lh)
			"u": W = maxf(lw, 2.0 * R + 6.0); H = lh + gap + 2.0 * R + 8.0; d = Vector2(W * 0.5, H - R - 2.0); _plate = Rect2((W - lw) * 0.5, 0, lw, lh)
			"d": W = maxf(lw, 2.0 * R + 6.0); H = lh + gap + 2.0 * R + 8.0; d = Vector2(W * 0.5, R + 2.0); _plate = Rect2((W - lw) * 0.5, H - lh, lw, lh)
			_: W = lw + gap + 2.0 * R + 8.0; H = maxf(lh, 2.0 * R + 6.0); d = Vector2(R + 2.0, H * 0.5); _plate = Rect2(2.0 * R + 8.0 + gap, (H - lh) * 0.5, lw, lh)
		size = Vector2(W, H)
		var pos := anchor - d
		var cl := Vector2(clampf(pos.x, bounds.position.x + 4.0, maxf(bounds.end.x - W - 4.0, bounds.position.x + 4.0)), clampf(pos.y, bounds.position.y + 4.0, maxf(bounds.end.y - H - 4.0, bounds.position.y + 4.0)))
		position = cl; disc = anchor - cl
		custom_minimum_size = Vector2.ZERO
	## Phase 1 of the map layout: the pin alone, its disc on `anchor` (map coordinates).
	func place_disc(anchor: Vector2) -> void:
		var e := 2.0 * _R() + 6.0; size = Vector2(e, e); disc = Vector2(e, e) * 0.5; _plate = Rect2(); _leader = false
		position = anchor - disc; custom_minimum_size = Vector2.ZERO
	## The size of the label plate (title and stars) of a task or boss pin.
	func plate_size() -> Vector2:
		var ft := Kit.fs(16 if not Layout.phone else 15); var fz := Kit.fs(14)
		var tw := Fonts.bold.get_string_size(str(node["title"]), HORIZONTAL_ALIGNMENT_LEFT, -1, ft).x
		var sw := 56.0 + (0.0 if Layout.phone else Fonts.regular.get_string_size(str(_status()[0]), HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x)
		return Vector2(maxf(tw, sw) + 18.0, float(ft) + float(fz) + 16.0)
	## Phase 2: the plate at `pr` (map coordinates) with the disc where place_disc put it. The button covers both; a leader joins them when they do not touch.
	func place_plate(pr: Rect2) -> void:
		var anchor := position + disc; var R := _R()
		var dr := Rect2(anchor - Vector2(R + 3.0, R + 3.0), Vector2(2.0 * R + 6.0, 2.0 * R + 6.0))
		var u := dr.merge(pr)
		position = u.position; size = u.size; disc = anchor - u.position; _plate = Rect2(pr.position - u.position, pr.size)
		_leader = not dr.grow(3.0).intersects(pr)
	## The part that is clickable: the plate and the pin, with a touch-sized pad around a bare pin.
	func _has_point(p: Vector2) -> bool:
		var r := maxf(_R() + 4.0, Layout.tap() * 0.5)
		if p.distance_to(disc) <= r: return true
		return has_plate() and _plate.size.x > 0.0 and _plate.grow(4.0).has_point(p)
	## The pin's footprint in map coordinates (what the overlay keeps clear): the disc, and the plate when it touches the disc.
	func pin_rect() -> Rect2:
		var dr := Rect2(position + disc - Vector2(_R(), _R()), Vector2(_R(), _R()) * 2.0)
		return dr.merge(plate_rect()) if has_plate() and _plate.size.x > 0.0 and not _leader else dr
	func plate_rect() -> Rect2: return Rect2(position + _plate.position, _plate.size)
	func _draw() -> void:
		# a lettered call-out on the plate: an inked ring with an italic letter (a, b, c ... in play order), a paper slip with the title and stars
		var id := str(node["id"]); var inst: bool = node["installed"]; var st := _status(); var best: Dictionary = st[2]
		var open := inst and Campaign.unlocked(id); var passed := bool(best["passed"]); var selected := screen != null and screen.sel == id
		var edge := Kit.success() if passed else (Kit.exc() if open else Color(Kit.INK, 0.45))
		var hov := is_hovered(); var R := _R(); var k := kind()
		var ft := Kit.fs(16 if not Layout.phone else 15); var fz := Kit.fs(14)
		var paper := Color("#FBF7EE")
		if has_plate():
			draw_rect(Rect2(_plate.position + Vector2(3, 3), _plate.size), Color(Kit.INK, 0.10 if not (hov or selected) else 0.18))
			draw_rect(_plate, paper if not hov else Kit.PANEL_HI)
			draw_rect(_plate, Color(Kit.INK, 0.9 if (selected or hov) else 0.55), false, 2.0 if selected else 1.0)
			if selected: draw_line(_plate.position + Vector2(6, _plate.size.y - 3.0), _plate.end - Vector2(6, 3.0), Kit.GAMBOGE, 2.4)
			var tcol := Kit.TEXT if open else Kit.TEXT_SOFT
			var tx := _plate.position.x + 9.0; var ty := _plate.position.y + 6.0 + float(ft) * 0.86
			draw_string(Fonts.bold, Vector2(tx, ty), str(node["title"]), HORIZONTAL_ALIGNMENT_LEFT, _plate.size.x - 14.0, ft, tcol)
			var y2 := ty + 7.0
			for j in 3: ScoreCard.draw_star_on(self, Vector2(tx + 8.0 + 18.0 * j, y2 + float(fz) * 0.5), 7.0, Kit.GAMBOGE if j < int(best["stars"]) else Color(Kit.LINE, 0.95))
			if not Layout.phone: draw_string(Gfx.serif_italic(), Vector2(tx + 56.0, y2 + float(fz) * 0.86), str(st[0]).to_lower(), HORIZONTAL_ALIGNMENT_LEFT, _plate.size.x - 64.0, fz, st[1])
			if gap > 0.0 or _leader:   # the leader from the ring to the slip
				var q := Vector2(clampf(disc.x, _plate.position.x, _plate.end.x), clampf(disc.y, _plate.position.y, _plate.end.y))
				draw_line(disc + (q - disc).normalized() * R, q, Color(Kit.INK, 0.7), 1.0, true)
		if open and hov: draw_circle(disc, R + 5.0, Color(Kit.GAMBOGE, 0.25))
		if selected:
			var pu := 0.5 + 0.5 * sin(Time.get_ticks_msec() * 0.004) if Kit.motion() else 1.0
			if k == "variant": draw_polyline(_diamond(disc, R + 5.0 + 2.0 * pu), Kit.GAMBOGE, 2.4, true)
			else: draw_arc(disc, R + 5.0 + 2.0 * pu, 0, TAU, 40, Kit.GAMBOGE, 2.4, true)
		var lw := 2.0 if open or passed else 1.2
		match k:
			"variant":
				var dm := _diamond(disc, R)
				draw_colored_polygon(dm, Kit.success() if passed else paper); draw_polyline(dm, edge, lw, true)
				if passed: draw_polyline(PackedVector2Array([disc + Vector2(-R * 0.4, 0), disc + Vector2(-R * 0.08, R * 0.32), disc + Vector2(R * 0.42, -R * 0.3)]), paper, 2.0, true)
			"diagnosis":
				var rr := Rect2(disc - Vector2(R, R), Vector2(R, R) * 2.0)
				draw_rect(rr, Kit.success() if passed else paper); draw_rect(rr, edge, false, lw)
				var cc := paper if passed else edge
				draw_line(disc + Vector2(-R * 0.5, 0), disc + Vector2(R * 0.5, 0), cc, 2.0, true); draw_line(disc + Vector2(0, -R * 0.5), disc + Vector2(0, R * 0.5), cc, 2.0, true)
			_:
				draw_circle(disc, R, paper)
				if k == "boss": draw_arc(disc, R + 4.0, 0, TAU, 48, Color(edge, 0.7), 1.0, true)
				if not inst:
					Gfx.dashed(self, PackedVector2Array(Array(Gfx.ellipse(disc, R, R, 32)) + [disc + Vector2(R, 0)]), Color(Kit.INK, 0.45), 1.2, 3.0, 3.0)
				else: draw_arc(disc, R, 0, TAU, 40, edge, lw, true)
				if not inst: pass
				elif passed:
					draw_circle(disc, R - 3.0, Kit.success())
					if k == "boss": ScoreCard.draw_star_on(self, disc, R * 0.62, paper)
					else: draw_polyline(PackedVector2Array([disc + Vector2(-R * 0.35, 0), disc + Vector2(-R * 0.08, R * 0.28), disc + Vector2(R * 0.40, -R * 0.26)]), paper, 2.4 if k == "task" else 2.0, true)
				elif not open:
					if k == "micro": draw_circle(disc, R * 0.3, Color(Kit.TEXT_SOFT, 0.7))
					else:
						draw_rect(Rect2(disc + Vector2(-R * 0.30, -R * 0.02), Vector2(R * 0.60, R * 0.46)), Kit.TEXT_SOFT); draw_arc(disc + Vector2(0, -R * 0.02), R * 0.22, PI, TAU, 10, Kit.TEXT_SOFT, 1.8, true)
				elif k == "boss": ScoreCard.draw_star_on(self, disc, R * 0.6, Kit.exc())
				elif spine_no > 0:   # a numbered gamboge disc: the play order, readable at a glance
					draw_circle(disc, R - 2.0, Color(Kit.GAMBOGE, 0.9))
					var lz := Kit.fs(16 if R >= 13.0 else 14)
					draw_string(Fonts.bold, disc + Vector2(-R - 4.0, float(lz) * 0.34), str(spine_no), HORIZONTAL_ALIGNMENT_CENTER, R * 2.0 + 8.0, lz, Kit.INK)
				elif k == "micro": draw_circle(disc, R * 0.36, Kit.exc())
		if has_focus(): draw_style_box(Kit.ring_sb(), Rect2(Vector2.ZERO, size) if has_plate() else Rect2(disc - Vector2(R, R) - Vector2(5, 5), Vector2(R, R) * 2.0 + Vector2(10, 10)))
	static func _diamond(c: Vector2, r: float) -> PackedVector2Array:
		return PackedVector2Array([c + Vector2(0, -r * 1.2), c + Vector2(r * 1.2, 0), c + Vector2(0, r * 1.2), c + Vector2(-r * 1.2, 0), c + Vector2(0, -r * 1.2)])

## Concept chips and the name of a bare pin, drawn over the map for the hovered pin (or the selected one when nothing is hovered).
class MapOverlay extends Control:
	var map: Control
	func _init() -> void: mouse_filter = Control.MOUSE_FILTER_IGNORE; set_anchors_preset(Control.PRESET_FULL_RECT)
	func min_text_px() -> float: return float(Kit.fs(14)) * Kit.ppl
	func _draw() -> void:
		var card: NodeCard = null
		for c in map.cards:
			if c.is_hovered(): card = c
		if card == null:
			for c in map.cards:
				if c.screen != null and c.screen.sel == str(c.node["id"]): card = c
		if card == null or not bool(card.node["installed"]): return
		var fz := Kit.fs(14); var ft := Kit.fs(15)
		var rows := []   # [[text, kind]] per row of chips, kind: "new" | "used"
		var chips := []
		# no concept chips over the map (the level card on the right lists the concepts): only the name of a pin that has no label plate
		var bare := not card.has_plate()
		if chips.is_empty() and not bare: return
		var maxw := minf(size.x - 16.0, 360.0 if not Layout.phone else 300.0)
		var lines := []; var cur := []; var curw := 0.0
		for ch in chips:
			var w := Fonts.bold.get_string_size(str(ch[0]), HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x + 18.0
			if curw + w > maxw and not cur.is_empty(): lines.append(cur); cur = []; curw = 0.0
			cur.append([ch[0], ch[1], w]); curw += w + 5.0
		if not cur.is_empty(): lines.append(cur)
		var title := str(card.node["title"]); var tw := Fonts.bold.get_string_size(title, HORIZONTAL_ALIGNMENT_LEFT, -1, ft).x if bare else 0.0
		var bw := maxf(tw + 4.0, 40.0)
		for ln in lines:
			var lw := 0.0
			for ch in ln: lw += float(ch[2]) + 5.0
			bw = maxf(bw, lw)
		var lh := float(fz) + 8.0
		var bh := (float(ft) + 6.0 if bare else 0.0) + lh * lines.size() + 10.0
		# below, above, right or left of the pin: whichever covers the fewest other plates and pins (and stays on the page)
		var pr := card.pin_rect(); var cw := bw + 12.0
		if card.has_plate() and card._plate.size.x > 0.0: pr = pr.merge(card.plate_rect())   # the chips go beside the pin and its own label plate, not over it
		var cands := [Vector2(pr.get_center().x - cw * 0.5, pr.end.y + 6.0), Vector2(pr.get_center().x - cw * 0.5, pr.position.y - bh - 6.0),
			Vector2(pr.end.x + 8.0, pr.get_center().y - bh * 0.5), Vector2(pr.position.x - cw - 8.0, pr.get_center().y - bh * 0.5)]
		var bx := 0.0; var by := 0.0; var bc := INF
		for k in cands.size():
			var q: Vector2 = cands[k]
			q = Vector2(clampf(q.x, 4.0, maxf(size.x - cw - 4.0, 4.0)), clampf(q.y, 4.0, maxf(size.y - bh - 4.0, 4.0)))
			var rc := Rect2(q, Vector2(cw, bh)); var cost := float(k) * 0.01
			if rc.intersects(pr.grow(2.0)): cost += 3.0   # pushed back onto the page, it would cover its own pin and label plate
			for c in map.cards:
				if c == card or not c.visible: continue
				if c.has_plate() and c._plate.size.x > 0.0: cost += MapCanvas._frac(c.plate_rect().grow(4.0), rc) + (1.0 if c.plate_rect().intersects(rc) else 0.0)
				if c.pin_rect().intersects(rc): cost += 0.5
			if rc.end.y > size.y - 40.0 and rc.position.x < 520.0: cost += 1.0   # the plate caption at the foot
			cost += (map as MapCanvas)._keep_cost(rc, 100.0) * 0.004   # and off the drawing's lettering (it is only up while hovered or selected, so anatomy is fair game)
			if cost < bc: bc = cost; bx = q.x; by = q.y
		draw_rect(Rect2(bx + 3.0, by + 3.0, bw + 12.0, bh), Color(Kit.INK, 0.12)); draw_rect(Rect2(bx, by, bw + 12.0, bh), Color("#FBF7EE")); draw_rect(Rect2(bx, by, bw + 12.0, bh), Color(Kit.INK, 0.8), false, 1.0)
		var y := by + 6.0
		if bare:
			draw_string(Fonts.bold, Vector2(bx + 6.0, y + float(ft) * 0.86), title, HORIZONTAL_ALIGNMENT_LEFT, bw, ft, Kit.TEXT); y += float(ft) + 6.0
		for ln in lines:
			var x := bx + 6.0
			for ch in ln:
				var w: float = ch[2]; var isnew: bool = ch[1]
				draw_rect(Rect2(x, y, w, lh - 2.0), Kit.INK if isnew else Kit.PANEL_HI); draw_rect(Rect2(x, y, w, lh - 2.0), Kit.INK if isnew else Color(Kit.INK, 0.5), false, 1.0)
				draw_string(Fonts.bold if isnew else Fonts.regular, Vector2(x + 8.0, y + float(fz) * 0.9), str(ch[0]), HORIZONTAL_ALIGNMENT_LEFT, w - 8.0, fz, Kit.PAPER if isnew else Kit.TEXT)
				x += w + 5.0
			y += lh

## The anatomy of the chapter with the level pins on it. Each chapter scene draws its organs and returns {task id: [anchor, label side]} for the tasks it knows;
## the other spine levels (micro-puzzles, bosses) are strung along the path between those anchors in play order, and branches hang off their task.
class MapCanvas extends Control:
	var screen: TasksScreen
	var chapter := ""
	var nodes: Array = []
	var cards: Array = []
	var overlay: MapOverlay
	var _t := 0.35
	var _cap_rect := Rect2()   # the plate caption at the foot ("Atlas I. Invertebrates. 0 of 33 stars")
	var labelled := {}   # pin ids that carry a label plate: the selected one, its spine neighbours, its branches
	func _ready() -> void: clip_contents = true
	func update_labels(sel_id: String) -> void:
		labelled = {}
		var sp := cards.filter(func(c): return bool(c.node["spine"]))
		var si := -1
		for i in sp.size():
			if str(sp[i].node["id"]) == sel_id: si = i
		var parent := sel_id
		for c in cards:
			if str(c.node["id"]) == sel_id and not bool(c.node["spine"]): parent = str(c.node["parent"])
		for i in sp.size():
			if str(sp[i].node["id"]) == parent: si = i
		for i in range(si - 1, si + 2):
			if si >= 0 and i >= 0 and i < sp.size(): labelled[str(sp[i].node["id"])] = true
		labelled[sel_id] = true
		for c in cards:
			if not bool(c.node["spine"]) and str(c.node["parent"]) == parent: labelled[str(c.node["id"])] = true
	func min_text_px() -> float: return float(Kit.fs(14)) * Kit.ppl
	func first_card() -> Control:
		for c in cards:
			if bool(c.node["installed"]) and bool(c.node["spine"]) and Campaign.unlocked(str(c.node["id"])): return c
		for c in cards:
			if bool(c.node["installed"]) and Campaign.unlocked(str(c.node["id"])): return c
		return cards[0] if not cards.is_empty() else null
	func set_chapter(id: String, ns: Array) -> void:
		chapter = id; nodes = ns
		for c in cards:
			remove_child(c); c.queue_free()
		cards = []
		if overlay == null: overlay = MapOverlay.new(); overlay.map = self
		if overlay.get_parent() != null: remove_child(overlay)
		for i in ns.size():
			var n: Dictionary = ns[i]
			var c := NodeCard.new(); Kit.ident(c, "Pin_" + str(n["id"])); c.node = n; c.idx = i; c.screen = screen; c.spine_no = (ns.slice(0, i + 1).filter(func(x): return bool(x["spine"])).size()) if bool(n["spine"]) else 0; c.pressed.connect(func(): screen.select_node(str(n["id"])))
			c.tooltip_text = ""; c.mouse_entered.connect(overlay.queue_redraw); c.mouse_exited.connect(overlay.queue_redraw); add_child(c); cards.append(c)
		add_child(overlay); update_labels(screen.sel if screen != null else ""); queue_redraw()
	func _notification(what: int) -> void:
		if what == NOTIFICATION_RESIZED: queue_redraw()
	func _process(dt: float) -> void:
		if Kit.motion() and is_visible_in_tree():
			_t += dt; queue_redraw(); if overlay != null: overlay.queue_redraw()
			for c in cards: c.queue_redraw()

	static func _bez(p0: Vector2, c: Vector2, p1: Vector2, u: float) -> Vector2:
		var a := 1.0 - u; return p0 * (a * a) + c * (2.0 * a * u) + p1 * (u * u)
	static func _bow(p0: Vector2, p1: Vector2) -> Vector2:
		return (p0 + p1) * 0.5 + (p1 - p0).orthogonal().normalized() * minf(40.0, (p1 - p0).length() * 0.12)

	## The far end of a run of pins that sticks out of the anatomy from `p`: the direction (not toward `avoid`) with the most room inside `inner`, preferring `want`.
	static func _free_end(p: Vector2, want: Vector2, avoid: Vector2, desired: float, inner: Rect2) -> Vector2:
		var best := p + want * 10.0; var bs := -1e9
		for k in 24:
			var d := Vector2.from_angle(TAU * float(k) / 24.0)
			if d.dot(avoid) > 0.45: continue
			var room := desired
			if d.x > 0.001: room = minf(room, (inner.end.x - p.x) / d.x)
			elif d.x < -0.001: room = minf(room, (inner.position.x - p.x) / d.x)
			if d.y > 0.001: room = minf(room, (inner.end.y - p.y) / d.y)
			elif d.y < -0.001: room = minf(room, (inner.position.y - p.y) / d.y)
			room = maxf(room, 0.0)
			var sc := room + 0.35 * desired * d.dot(want)
			if sc > bs: bs = sc; best = p + d * room
		return best

	## Positions of the spine cards and the curve segments between them: returns {"pos": [Vector2 per spine card], "seg": [[p0, ctrl, p1, u0, u1] per gap]}.
	func _spine_layout(sp: Array, anch: Dictionary, r: Rect2) -> Dictionary:
		var n := sp.size(); var pts := []; var A := []
		for i in n:
			var id := str(sp[i].node["id"]); var np: Vector2 = sp[i].node["pos"]; var p = null
			if anch.has(id): p = anch[id][0]
			elif np.x >= 0.0: p = Vector2(np.x * size.x, np.y * size.y)
			pts.append(p)
			if p != null: A.append(i)
		var inner := r.grow(-52.0); inner.size.y -= 34.0   # and clear of the plate caption at the foot
		var pos := []; pos.resize(n); var seg := []; seg.resize(maxi(n - 1, 0))
		var runs := []   # [first idx, last idx, P0, C, P1]
		if A.is_empty():
			var q0 := Vector2(inner.position.x, inner.position.y + inner.size.y * 0.5); var q1 := Vector2(inner.end.x, q0.y)
			runs.append([0, n - 1, q0, _bow(q0, q1), q1])
		else:
			var a0: int = A[0]; var aN: int = A[A.size() - 1]
			if a0 > 0:
				var toward := ((pts[A[1]] as Vector2) - (pts[a0] as Vector2)).normalized() if A.size() > 1 else Vector2(1, 0)
				var e := _free_end(pts[a0], -toward, toward, minf(size.x, size.y) * (0.12 + 0.045 * a0), inner)
				runs.append([0, a0, e, (e + (pts[a0] as Vector2)) * 0.5, pts[a0]])
			for k in range(A.size() - 1):
				var pa: Vector2 = pts[A[k]]; var pb: Vector2 = pts[A[k + 1]]; runs.append([A[k], A[k + 1], pa, _bow(pa, pb), pb])
			if aN < n - 1:
				var back := ((pts[A[A.size() - 2]] as Vector2) - (pts[aN] as Vector2)).normalized() if A.size() > 1 else Vector2(-1, 0)
				var e2 := _free_end(pts[aN], -back, back, minf(size.x, size.y) * (0.12 + 0.045 * (n - 1 - aN)), inner)
				runs.append([aN, n - 1, pts[aN], ((pts[aN] as Vector2) + e2) * 0.5, e2])
		for rn in runs:
			var i0: int = rn[0]; var i1: int = rn[1]
			for k in range(i0, i1 + 1):
				var u := float(k - i0) / float(maxi(i1 - i0, 1))
				if pos[k] == null: pos[k] = _bez(rn[2], rn[3], rn[4], u)
				if k < i1: seg[k] = [rn[2], rn[3], rn[4], u, float(k + 1 - i0) / float(maxi(i1 - i0, 1))]
		for i in n:
			if pos[i] == null: pos[i] = Vector2(size.x * 0.5, size.y * 0.5)
		return {"pos": pos, "seg": seg}

	func _draw() -> void:
		var r := Rect2(Vector2.ZERO, size)
		Gfx.plate_frame(self, r.grow(-3.0), Gfx.SHEET)   # the atlas page: an engraved double rule around the specimen drawing
		var a := 1.0 if Campaign.chapter_open(chapter) else 0.5
		keep = []
		var anch := _scene(chapter, r.grow(-10.0), a)
		var sp := cards.filter(func(c): return bool(c.node["spine"]))
		var lay := _spine_layout(sp, anch, r)
		var placed := {}
		# 1. every pin's disc first (spine pins on the anatomy, in play order), so the label plates can be laid out around all of them
		for i in sp.size():
			var c = sp[i]; c.place_disc(lay["pos"][i]); placed[str(c.node["id"])] = c
		# the spine: a line from pin to pin along the anatomy, green once the level before it is passed, amber for the way ahead, faint beyond
		for i in range(sp.size() - 1):
			var sg: Array = lay["seg"][i]; var pc = sp[i]; var nc = sp[i + 1]
			var pts := PackedVector2Array()
			for k in 11: pts.append(_bez(sg[0], sg[1], sg[2], lerpf(float(sg[3]), float(sg[4]), float(k) / 10.0)))
			keep_path(pts, 30.0)
			# the spine is one SOLID inked path (branches are dashed): green where it is walked, ink for the way ahead, faint where it is not open
			if bool(pc.node["installed"]) and Campaign.passed(str(pc.node["id"])): draw_polyline(pts, Color(Kit.success(), 0.9), 2.6, true)
			elif bool(pc.node["installed"]) and Campaign.unlocked(str(pc.node["id"])) and bool(nc.node["installed"]): draw_polyline(pts, Color(Kit.INK, 0.8), 2.2, true)
			else: draw_polyline(pts, Color(Kit.INK, 0.3), 1.6, true)
		# branches hang off their spine pin: the free direction with the most room
		var pins := []
		for c in sp: pins.append((c as NodeCard).pin_rect())
		var by_parent := {}
		for c in cards:
			if bool(c.node["spine"]): continue
			var pk := str(c.node["parent"])
			if not by_parent.has(pk): by_parent[pk] = []
			(by_parent[pk] as Array).append(c)
		var dist := (40.0 if Layout.phone else 56.0)
		for pid in by_parent:
			if not placed.has(pid):
				for c in by_parent[pid]: c.place_disc(Vector2(size.x * 0.5, size.y * 0.5)); c.visible = false
				continue
			var pc: NodeCard = placed[pid]; var centre := pc.position + pc.disc
			for c in by_parent[pid]:
				c.visible = true
				var bestp := centre + Vector2(dist, 0); var bests := -1e9
				var spot := anch.has(str(c.node["id"]))   # the scene has a spot for it (a drawn plate's quiet patch)
				if spot: bestp = anch[str(c.node["id"])][0]
				for k in (0 if spot else 16):
					var ang := TAU * float(k) / 16.0 + 0.2
					var d := dist * (1.0 + 0.5 * float(k % 2))
					var q := centre + Vector2(cos(ang), sin(ang)) * (d + pc._R())
					var sc := 0.0
					var clear := 1e9
					for pr in pins: clear = minf(clear, (pr as Rect2).grow(10.0).get_center().distance_to(q) - maxf((pr as Rect2).size.x, (pr as Rect2).size.y) * 0.5)
					for pr in pins:
						if (pr as Rect2).grow(14.0).has_point(q): sc -= 1000.0
					if not r.grow(-18.0).has_point(q): sc -= 500.0
					sc += clear - 0.4 * _keep_cost(Rect2(q - Vector2(10, 10), Vector2(20, 20)))
					if sc > bests: bests = sc; bestp = q
				c.place_disc(bestp)
				pins.append(c.pin_rect())
				keep_path(PackedVector2Array([centre, centre.lerp(bestp, 0.5), bestp]), 20.0)
				Gfx.dashed(self, PackedVector2Array([centre, bestp]), Color(Kit.success(), 0.9) if Campaign.passed(str(c.node["id"])) else Color(Kit.INK, 0.5), 1.2, 4.0, 4.0)   # a branch: dashed
		# 2. the label plates of tasks and bosses: each takes the free spot nearest its pin that covers no other pin or plate, as little anatomy and
		# lettering as it can, and stays on the page; a leader line joins a plate that had to move away from its pin
		keep.append([Rect2(0, size.y - float(Kit.fs(15)) - 26.0 - (0.0 if Layout.phone else float(Kit.fs(14)) + 16.0), minf(size.x * 0.62, 760.0), float(Kit.fs(15)) + 26.0 + (0.0 if Layout.phone else float(Kit.fs(14)) + 16.0)), 400.0])   # the plate caption and the key
		if not Campaign.chapter_open(chapter): keep.append([Rect2(0, 0, minf(size.x * 0.7, 640.0), 44.0), 400.0])   # the lock notice
		var cd0 := Campaign.chapter_def(chapter); var st0 := Campaign.star_total(chapter); var pz0 := Kit.fs(15)   # the caption's own box: no label plate may sit on it (a phone's small plate has room for little else, and the soft keep above lost)
		var cap0 := ("%s.  %d of %d stars" % [str(cd0["title"]), st0.x, st0.y]) if st0.y > 0 else str(cd0["title"]) + "."
		_cap_rect = Rect2(10, size.y - float(pz0) - 18.0, Fonts.bold.get_string_size("Atlas XV.", HORIZONTAL_ALIGNMENT_LEFT, -1, pz0).x + Gfx.serif_italic().get_string_size(cap0, HORIZONTAL_ALIGNMENT_LEFT, -1, pz0).x + 26.0, float(pz0) + 10.0)
		var plates := []; _leaders = []
		for c in sp + cards.filter(func(c): return not bool(c.node["spine"]) and c.visible):   # spine levels first, then the diagnosis cases and micro-puzzles off them
			var nc := c as NodeCard
			if not nc.has_plate(): continue
			var want := ""
			if anch.has(str(nc.node["id"])): want = str((anch[str(nc.node["id"])] as Array)[1])
			var pr := _best_plate(nc, pins, plates, r, want)
			nc.place_plate(pr); plates.append(pr)
			if nc._leader: _leaders.append([nc.position + nc.disc, Vector2(clampf(nc.position.x + nc.disc.x, pr.position.x, pr.end.x), clampf(nc.position.y + nc.disc.y, pr.position.y, pr.end.y))])
		var cd := Campaign.chapter_def(chapter); var st := Campaign.star_total(chapter); var fz := Kit.fs(14)
		var cap := ("%s.  %d of %d stars" % [str(cd["title"]), st.x, st.y]) if st.y > 0 else str(cd["title"]) + "."
		var pz := Kit.fs(15); var pl := "Atlas %s." % NotebookScreen._roman(Campaign.chapter_index(chapter) + 1)   # one numeral per chapter, in campaign order (I. Invertebrates ... XV. Tuning); "Atlas", so it never collides with the screens' "Plate I-VI" running heads
		draw_rect(Rect2(10, size.y - float(pz) - 18.0, Fonts.bold.get_string_size(pl, HORIZONTAL_ALIGNMENT_LEFT, -1, pz).x + Gfx.serif_italic().get_string_size(cap, HORIZONTAL_ALIGNMENT_LEFT, -1, pz).x + 26.0, float(pz) + 10.0), Color(Gfx.SHEET, 0.85))
		draw_string(Fonts.bold, Vector2(16, size.y - 14), pl, HORIZONTAL_ALIGNMENT_LEFT, -1, pz, Kit.TEXT)
		draw_string(Gfx.serif_italic(), Vector2(22 + Fonts.bold.get_string_size(pl, HORIZONTAL_ALIGNMENT_LEFT, -1, pz).x, size.y - 14), cap, HORIZONTAL_ALIGNMENT_LEFT, -1, pz, Kit.TEXT)
		_draw_key(pz)
		if not Campaign.chapter_open(chapter):
			var lr := Campaign.chapter_lock_reason(chapter)
			Anatomy.tag(self, ("Locked. %s." % lr) if lr != "" else "Locked.", Vector2(14, 10.0 + float(fz)), Kit.RED, 14, HORIZONTAL_ALIGNMENT_LEFT, false, 0.7)

	## The key to the pin glyphs, a small italic legend in the bottom-left corner above the plate caption (not on a phone: the level card names the kind).
	func _draw_key(pz: int) -> void:
		if Layout.phone: return
		var fz := Kit.fs(14); var fi := Gfx.serif_italic(); var y := size.y - float(pz) - 30.0; var x := 18.0
		var items := [["num", "level, in play order"], ["boss", "chapter finale"], ["diag", "diagnosis case"], ["var", "variant"], ["done", "passed"]]
		var w := 0.0
		for it in items: w += 26.0 + fi.get_string_size(str(it[1]), HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x + 14.0
		draw_rect(Rect2(10, y - float(fz) - 4.0, w + 12.0, float(fz) + 12.0), Color(Gfx.SHEET, 0.85))
		var paper := Color("#FBF7EE")
		for it in items:
			var c := Vector2(x + 8.0, y - float(fz) * 0.32); var R := 7.0
			match str(it[0]):
				"num": draw_circle(c, R, Color(Kit.GAMBOGE, 0.9)); draw_arc(c, R, 0, TAU, 20, Kit.exc(), 1.4, true); draw_string(Fonts.bold, c + Vector2(-R, 4.0), "1", HORIZONTAL_ALIGNMENT_CENTER, R * 2.0, Kit.fs(11), Kit.INK)
				"boss": draw_circle(c, R, paper); draw_arc(c, R, 0, TAU, 20, Kit.exc(), 1.4, true); ScoreCard.draw_star_on(self, c, R * 0.7, Kit.exc())
				"diag": draw_rect(Rect2(c - Vector2(R, R), Vector2(R, R) * 2.0), paper); draw_rect(Rect2(c - Vector2(R, R), Vector2(R, R) * 2.0), Kit.exc(), false, 1.4); draw_line(c - Vector2(R * 0.5, 0), c + Vector2(R * 0.5, 0), Kit.exc(), 1.6); draw_line(c - Vector2(0, R * 0.5), c + Vector2(0, R * 0.5), Kit.exc(), 1.6)
				"var": draw_polyline(NodeCard._diamond(c, R * 0.85), Kit.exc(), 1.4, true)
				"done": draw_circle(c, R, Kit.success())
			draw_string(fi, Vector2(x + 20.0, y), str(it[1]), HORIZONTAL_ALIGNMENT_LEFT, -1, fz, Kit.TEXT_SOFT)
			x += 26.0 + fi.get_string_size(str(it[1]), HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x + 14.0

	## Keep-out areas of the current scene for the label plates: [Rect2, weight] (anatomy, its lettering, the spine path). Filled while _draw runs.
	var keep: Array = []
	var _leaders: Array = []   # [disc, plate end] of the label plates placed so far that sit away from their pin
	func keep_rect(rc: Rect2, w: float) -> void: keep.append([rc, w])
	func keep_path(pts: PackedVector2Array, w: float, half := 7.0) -> void:
		for i in pts.size():
			if i % 2 == 0 or i == pts.size() - 1: keep.append([Rect2(pts[i] - Vector2(half, half), Vector2(half, half) * 2.0), w])
	static func _frac(a: Rect2, b: Rect2) -> float:
		var i := a.intersection(b)
		return (i.size.x * i.size.y) / maxf(a.size.x * a.size.y, 1.0) if i.size.x > 0.0 and i.size.y > 0.0 else 0.0
	func _keep_cost(rc: Rect2, wmin := 0.0) -> float:   # wmin: count only the keep-out areas at least that heavy (100 and up: lettering)
		var c := 0.0
		for k in keep:
			if float(k[1]) >= wmin: c += float(k[1]) * _frac(rc, k[0]) * 10.0
		return c
	## Where a pin's label plate goes: candidates in 16 directions at growing distances from the disc; the cheapest wins. Covering another pin or plate,
	## or leaving the page, is ruled out; anatomy, lettering and the spine path cost by the area covered; distance costs a little; the side the scene
	## suggests (`want`: l r u d) gets a small bonus.
	func _best_plate(c: NodeCard, pins: Array, plates: Array, r: Rect2, want: String) -> Rect2:
		var sz := c.plate_size(); var ctr := c.position + c.disc; var R := c._R()
		var inner := r.grow(-8.0)
		var wdir := {"l": Vector2(-1, 0), "r": Vector2(1, 0), "u": Vector2(0, -1), "d": Vector2(0, 1)}.get(want, Vector2.ZERO) as Vector2
		var b0 := ctr + Vector2(R + 6.0, -sz.y * 0.5)   # the fallback (nothing fits): beside the pin, pushed back inside the page
		var best := Rect2(Vector2(clampf(b0.x, inner.position.x, maxf(inner.end.x - sz.x, inner.position.x)), clampf(b0.y, inner.position.y, maxf(inner.end.y - sz.y, inner.position.y))), sz); var bc := INF
		for di in [4.0, 22.0, 46.0, 80.0, 120.0, 170.0, 230.0]:
			for k in 16:
				var d := Vector2.from_angle(TAU * float(k) / 16.0)
				var ext := absf(d.x) * sz.x * 0.5 + absf(d.y) * sz.y * 0.5
				var cen: Vector2 = ctr + d * (R + float(di) + ext)
				var rc := Rect2(cen - sz * 0.5, sz)
				if not inner.encloses(rc): continue
				var cost := 0.0
				for pr in pins:
					if (pr as Rect2).grow(5.0).intersects(rc) and not (pr as Rect2).is_equal_approx(Rect2(ctr - Vector2(R, R), Vector2(R, R) * 2.0)): cost += 1e5 * (0.2 + _frac(rc, (pr as Rect2).grow(5.0)))
				for pl in plates:
					if (pl as Rect2).grow(6.0).intersects(rc): cost += 1e5
				if di > 30.0:   # a leader that would run across another label plate or pin reads as joined to the wrong level
					var q := Vector2(clampf(ctr.x, rc.position.x, rc.end.x), clampf(ctr.y, rc.position.y, rc.end.y))
					for ld in _leaders:
						if Geometry2D.segment_intersects_segment(ctr, q, ld[0], ld[1]) != null: cost += 300.0   # two leaders that cross
					for t in [0.2, 0.35, 0.5, 0.65, 0.8]:
						var lp: Vector2 = ctr.lerp(q, t)
						for ld in _leaders:
							if rc.has_point(ld[0].lerp(ld[1], t)): cost += 300.0   # nor lie across another plate's leader
						for pl in plates:
							if (pl as Rect2).has_point(lp): cost += 300.0
						for pr in pins:
							if (pr as Rect2).grow(2.0).has_point(lp) and not (pr as Rect2).has_point(ctr): cost += 300.0
				if _cap_rect.grow(4.0).intersects(rc): cost += 5e4
				cost += _keep_cost(rc) + di * 0.6 - 25.0 * maxf(d.dot(wdir), 0.0)
				if cost < bc: bc = cost; best = rc
		return best

	func _scene(id: String, r: Rect2, a: float) -> Dictionary:
		match str(Campaign.chapter_def(id)["organ"]):
			"worm": return _sc_invert(r, a)
			"spine": return _sc_spine(r, a)
			"heart": return _sc_heart(r, a)
			"lungs": return _sc_lungs(r, a)
			"autonomic": return _sc_auto(r, a)
		return _sc_plate_image(id, r, a)

	## A chapter plate drawn as a picture (the Bestiary and Theory wings): res://assets/plates/<chapter id>.png from tools/art/plates.py, scaled to fit the
	## page inside the frame at its own aspect (2000 x 1250, the map canvas's). plates.json gives the pin spots the drawing was composed around and the
	## keep-out rects of its anatomy and lettering, as fractions of the plate: they come back as the pin anchors and go into `keep` for the label plates.
	## The textures are held in a static cache: a texture loaded into a local would be freed when this returns, before the canvas is rendered, and the
	## page would then draw the renderer's default white texture (the blank white plates of the 0.5 playtest).
	static var _plate_tex := {}
	static var _plate_meta = null
	static func plate_meta() -> Dictionary:
		if _plate_meta == null:
			_plate_meta = {}
			var f := FileAccess.open("res://assets/plates/plates.json", FileAccess.READ)
			if f != null:
				var d = JSON.parse_string(f.get_as_text())
				if d is Dictionary: _plate_meta = d
		return _plate_meta
	func _sc_plate_image(id: String, r: Rect2, a: float) -> Dictionary:
		if not _plate_tex.has(id):
			var path := "res://assets/plates/%s.png" % id
			_plate_tex[id] = load(path) as Texture2D if ResourceLoader.exists(path) else null
		var tex: Texture2D = _plate_tex[id]
		if tex == null: return {}
		var meta: Dictionary = plate_meta().get(id, {})
		var pins: Dictionary = meta.get("pins", {})
		# A phone's map frame is a wide, short strip (about 3.3 : 1) and the page is 1.6 : 1: shown whole it would be a small picture with empty sides (the squashed
		# plates of the review). Then only the band of the page that holds the pins is shown, at the full width of the frame.
		var crop := Rect2(0, 0, 1, 1)
		var asp_f := r.size.x / maxf(r.size.y, 1.0); var asp_t := float(tex.get_width()) / float(tex.get_height())
		if Layout.phone and asp_f > asp_t * 1.25 and not pins.is_empty():
			var ch := clampf(asp_t / asp_f, 0.3, 1.0); var y0 := 1.0; var y1 := 0.0
			for pid in pins: y0 = minf(y0, float(pins[pid][1])); y1 = maxf(y1, float(pins[pid][1]))
			crop = Rect2(0.0, clampf((y0 + y1) * 0.5 - ch * 0.5, 0.0, 1.0 - ch), 1.0, ch)
		var src := Rect2(crop.position * tex.get_size(), crop.size * tex.get_size())
		var k := minf(r.size.x / src.size.x, r.size.y / src.size.y); var sz := src.size * k
		var pr := Rect2(r.get_center() - sz * 0.5, sz)
		draw_texture_rect_region(tex, pr, src, Color(1, 1, 1, a))
		var at := func(fx: float, fy: float) -> Vector2: return pr.position + (Vector2(fx, fy) - crop.position) / crop.size * pr.size   # page fraction -> frame px
		for kr in meta.get("keep", []):
			var w := float(kr[4])
			if Layout.phone and w >= 100.0: continue   # the drawing's lettering is too small to read on a phone: as on the body plates (_lab), only the anatomy counts
			if float(kr[1]) + float(kr[3]) < crop.position.y or float(kr[1]) > crop.end.y: continue   # outside the band shown
			# the drawn specimens and figures fill most of the page, so they count half: a label may lie over a corner of a figure rather than
			# stray far from its pin on a long leader; the lettering (100 and up) counts in full
			var p0: Vector2 = at.call(float(kr[0]), float(kr[1])); var p1: Vector2 = at.call(float(kr[0]) + float(kr[2]), float(kr[1]) + float(kr[3]))
			keep_rect(Rect2(p0, p1 - p0), w if w >= 100.0 else w * 0.5)
		var out := {}
		for pid in pins: out[pid] = [at.call(float(pins[pid][0]), float(pins[pid][1])), ""]
		return out

	func _lab(s: String, p: Vector2, col := Kit.TEXT_SOFT, align := HORIZONTAL_ALIGNMENT_LEFT) -> void:
		if Layout.phone: return
		keep_rect(Anatomy.tag(self, s, p, Color(col, 0.95), 14, align, false, 0.55), 120.0)

	# ------------------------------------------------------------ heart
	func _sc_heart(r: Rect2, a: float) -> Dictionary:
		var w := r.size.x; var h := r.size.y; var m := minf(w, h)
		var hr := Rect2(r.position.x + w * 0.31, r.position.y + h * 0.05, w * 0.50, h * 0.88)
		keep_rect(Rect2(hr.position + hr.size * Vector2(0.05, 0.0), hr.size * Vector2(0.9, 1.0)), 40.0)
		var res := Anatomy.heart(self, hr, {"tms": fposmod(_t, 0.95) * 1000.0, "alpha": a, "labels": not Layout.phone, "vagus": 0.55, "symp": 0.45, "t": _t})
		var M := r.position + Vector2(w * 0.135, h * 0.24); var wm := m * 0.105
		var med := Anatomy.medulla(self, M, wm, {"hi": ["nts", "dmv", "amb"], "alpha": a, "t": _t, "act": 0.4})
		keep_rect(Rect2(M - Vector2(wm * 1.15, wm * 1.05), Vector2(wm * 2.3, wm * 2.0)), 60.0)
		_lab("MEDULLA", M + Vector2(0, -wm * 0.95), Kit.TEXT_SOFT, HORIZONTAL_ALIGNMENT_CENTER)
		# vagus: medulla -> heart
		var vpath := Gfx.cr_spline(PackedVector2Array([med["amb_r"], M + Vector2(wm * 0.98, wm * 0.05), M + Vector2(wm * 1.9, wm * 0.5), (M + res["vagus_in"] as Vector2) * 0.5 + Vector2(0, wm * 0.2), res["vagus_in"]]), 8)
		Anatomy.nerve(self, vpath, Anatomy.para(), maxf(m * 0.006, 2.0), 0.55, _t, a); keep_path(vpath, 25.0)
		# spinal cord with the sympathetic chain, and the cardiac nerves
		var cx0 := M.x - wm * 0.15; var cy0 := M.y + wm * 0.72; var cy1 := r.position.y + h * 0.90
		var cord := PackedVector2Array([Vector2(cx0, cy0), Vector2(cx0, cy1)])
		Anatomy.nerve(self, cord, Anatomy.NUC, maxf(m * 0.012, 3.0), 0.0, _t, a * 0.7)
		keep_rect(Rect2(cx0 - 12.0, cy0, wm * 0.36 + 26.0, cy1 - cy0), 30.0)
		var chain_x := cx0 + wm * 0.36
		var chain := PackedVector2Array([Vector2(chain_x, cy0 + wm * 0.2), Vector2(chain_x, cy0 + h * 0.46)])
		self.draw_polyline(chain, Color(Anatomy.symp(), 0.35 * a), 1.8, true)
		for i in 9: self.draw_circle(Vector2(chain_x, lerpf(chain[0].y, chain[1].y, float(i) / 8.0)), 3.6, Color(Anatomy.symp(), 0.9 * a))
		var sym_y := cy0 + h * 0.22
		var spath := Gfx.cr_spline(PackedVector2Array([Vector2(chain_x, sym_y), Vector2(chain_x + (res["symp_in"].x - chain_x) * 0.5, sym_y - h * 0.01), res["symp_in"]]), 8)
		Anatomy.nerve(self, spath, Anatomy.symp(), maxf(m * 0.006, 2.0), 0.45, _t, a); keep_path(spath, 25.0)
		# baroreceptor afferents: carotid sinus and aortic arch -> NTS
		var ct: Vector2 = res["carotid_top"]; var nts: Vector2 = med["nts_r"]
		var bpath := Gfx.cr_spline(PackedVector2Array([res["carotid"], ct + Vector2(0, -1.0), Vector2(lerpf(ct.x, M.x, 0.6), r.position.y + h * 0.04), M + Vector2(wm * 0.7, -wm * 0.85), nts]), 8)
		Anatomy.nerve(self, bpath, Kit.success(), maxf(m * 0.005, 1.8), 0.5, _t, a); keep_path(bpath, 25.0)
		self.draw_circle(res["carotid"], m * 0.014, Color(Kit.success(), 0.25 * a)); self.draw_arc(res["carotid"], m * 0.014, 0, TAU, 14, Color(Kit.success(), 0.9 * a), 1.6, true)
		var cp: Vector2 = res["carotid"]
		_lab("carotid sinus", cp + Vector2(m * 0.04, -m * 0.045), Kit.success())
		_lab("aortic arch", (res["arch"] as Vector2) + Vector2(m * 0.03, -m * 0.045))
		_lab("vagus", along_pt(vpath, 0.5) + Vector2(wm * 0.1, wm * 0.42), Anatomy.para())
		_lab("sympathetic", Vector2(chain_x + 10.0, sym_y + wm * 0.8), Anatomy.symp())
		_lab("SVC", (res["svc_top"] as Vector2) + Vector2(-m * 0.045, m * 0.11), Kit.TEXT_SOFT, HORIZONTAL_ALIGNMENT_RIGHT)
		_lab("aorta", (res["aorta_exit"] as Vector2) + Vector2(-m * 0.02, m * 0.07), Kit.TEXT_SOFT, HORIZONTAL_ALIGNMENT_RIGHT)
		_lab("IVC", (res["ivc_bottom"] as Vector2) + Vector2(-m * 0.045, -m * 0.07), Kit.TEXT_SOFT, HORIZONTAL_ALIGNMENT_RIGHT)
		var sa: Vector2 = res["sa"]; var av: Vector2 = res["av"]
		_lab("AV node", av + Vector2(m * 0.05, m * 0.075), Kit.GAMBOGE.darkened(0.25))
		return {"heart_pacemaker": [sa, "l"], "heart_autonomic": [along_pt(vpath, 0.72), "u", 12.0], "baroreflex": [res["carotid"], "r"]}

	static func along_pt(p: PackedVector2Array, u: float) -> Vector2: return Anatomy.along(p, u)

	# ------------------------------------------------------------ lungs
	func _sc_lungs(r: Rect2, a: float) -> Dictionary:
		var w := r.size.x; var h := r.size.y; var m := minf(w, h)
		var phase := _t * 0.30; var vol := 0.5 - 0.5 * cos(TAU * phase)
		var burst := clampf(sin(TAU * phase) * 1.6 + 0.2, 0.0, 1.0) if cos(TAU * phase) < 0.99 else 0.0   # the phrenic burst while the volume rises
		var lr := Rect2(r.position.x + w * 0.17, r.position.y + h * 0.045, w * 0.50, h * 0.93)
		var res := Anatomy.lungs(self, lr, {"vol": vol, "alpha": a, "t": _t})
		keep_rect(lr, 40.0)
		var M := r.position + Vector2(w * 0.805, h * 0.30); var wm := m * 0.165
		var med := Anatomy.medulla(self, M, wm, {"hi": ["prebotc", "rtn"], "alpha": a, "t": _t, "act": burst, "labels": not Layout.phone})
		keep_rect(Rect2(M - Vector2(wm * 1.15, wm * 1.05), Vector2(wm * 2.3, wm * 2.0)), 60.0)
		_lab("MEDULLA, transverse section", M + Vector2(0, -wm * 0.98), Kit.TEXT_SOFT, HORIZONTAL_ALIGNMENT_CENTER)
		var s: float = res["scale"]; var o: Vector2 = res["origin"]
		var c4: Vector2 = o + Vector2(0.545, 0.035) * s
		# bulbospinal axons (preBötC -> phrenic motor nucleus in C3-C5), then the phrenic nerves to the diaphragm
		var bs := Gfx.cr_spline(PackedVector2Array([med["prebotc"], M + Vector2(-wm * 0.5, wm * 0.45), M + Vector2(-wm * 1.4, wm * 1.0), (M + c4) * 0.5 + Vector2(0, wm * 0.6), c4]), 8)
		Anatomy.nerve(self, bs, Anatomy.symp(), maxf(m * 0.005, 2.0), burst, _t, a); keep_path(bs, 25.0)
		self.draw_circle(c4, m * 0.011, Color(Anatomy.NUC, 0.95 * a)); Gfx.glow(self, c4, m * 0.04, Color(Anatomy.NUC, 0.3 * a))
		for sd in [-1.0, 1.0]:
			var pn := Gfx.cr_spline(PackedVector2Array([c4, o + Vector2(0.5 + 0.045 * sd, 0.18) * s, o + Vector2(0.5 + 0.052 * sd, 0.45) * s, o + Vector2(0.5 + 0.075 * sd, 0.72) * s, res["dome_left" if sd > 0 else "dome_right"]]), 8)
			Anatomy.nerve(self, pn, Anatomy.para().lerp(Gfx.VIOLET, 0.5), maxf(m * 0.004, 1.8), burst, _t, a)
		_lab("C3 to C5", c4 + Vector2(m * 0.03, -m * 0.01), Anatomy.NUC)
		_lab("phrenic nerve", o + Vector2(0.60, 0.52) * s, Gfx.VIOLET)
		_lab("diaphragm", (res["dome_left"] as Vector2) + Vector2(m * 0.02, m * 0.09), Gfx.VIOLET)
		_lab("bronchial tree", (res["hilum_r"] as Vector2) + Vector2(-m * 0.28, m * 0.12), Kit.TEXT_SOFT)
		_lab("trachea", (res["trachea_top"] as Vector2) + Vector2(-m * 0.03, m * 0.03), Kit.TEXT_SOFT, HORIZONTAL_ALIGNMENT_RIGHT)
		return {"breathing": [med["prebotc_r"], "d", 40.0]}

	# ------------------------------------------------------------ spine
	func _sc_spine(r: Rect2, a: float) -> Dictionary:
		var w := r.size.x; var h := r.size.y; var m := minf(w, h)
		var top := Vector2(r.position.x + w * 0.075, r.position.y + h * 0.045)
		var sp := Anatomy.spine(self, top, h * 0.90, {"alpha": a, "t": _t, "act": 0.4})
		keep_rect(Rect2(top.x - m * 0.07, top.y, m * 0.14, h * 0.9), 40.0)
		_lab("cervical", top + Vector2(m * 0.07, h * 0.03), Kit.TEXT_SOFT); _lab("thoracic", top + Vector2(m * 0.09, h * 0.30), Kit.TEXT_SOFT); _lab("lumbar", top + Vector2(m * 0.09, h * 0.66), Kit.TEXT_SOFT)
		var C := r.position + Vector2(w * 0.40, h * 0.54); var wc := minf(w * 0.165, h * 0.30)
		var cs := Anatomy.cord_section(self, C, wc, {"alpha": a, "t": _t, "act": 0.55, "hi": ["mn", "renshaw"], "labels": not Layout.phone})
		keep_rect(Rect2(C - Vector2(wc * 1.05, wc * 0.85), Vector2(wc * 2.1, wc * 1.7)), 50.0)
		# zoom lines from the C5 level to the section
		var lv: Vector2 = sp["C5"] + Vector2(h * 0.07, 0)
		for sd in [-1.0, 1.0]:
			Gfx.dashed(self, PackedVector2Array([lv + Vector2(0, sd * 6.0), C + Vector2(-wc * 0.95, sd * wc * 0.55)]), Color(Anatomy.GRAY, 0.40 * a), 1.2, 5.0, 5.0)
		_lab("C6 segment", C + Vector2(0, -wc * 1.0), Kit.TEXT_SOFT, HORIZONTAL_ALIGNMENT_CENTER)
		# the arm
		var sh := r.position + Vector2(w * 0.695, h * 0.17); var L := minf(w * 0.22, h * 0.29)
		var swing := sin(_t * 1.1)
		var el := sh + Vector2(w * 0.035, h * 0.30)
		var ang := deg_to_rad(-38.0) + swing * 0.18
		var hd := el + Vector2(cos(ang), sin(ang)) * L
		keep_rect(Rect2(sh, Vector2.ZERO).expand(el).expand(hd).grow(m * 0.05), 40.0)
		var limb := Anatomy.limb(self, sh, el, hd, {"alpha": a, "t": _t, "ag": 0.40 + 0.3 * swing, "ant": 0.25 - 0.15 * swing, "ia_ag": 0.5 + 0.4 * swing, "ia_ant": 0.15})
		# the reflex arc closes through the arm: Ia afferent from the spindle to the dorsal root, motor axon from the ventral root to the muscle
		var aend: Vector2 = limb["ag_aff"]; var dre: Vector2 = cs["dorsal_root_end"]
		var aff := Anatomy.bez2(aend, aend.lerp(dre, 0.5) + Vector2(0, -h * 0.07), dre, 24)
		Anatomy.nerve(self, aff, Kit.success(), maxf(m * 0.004, 1.8), 0.6, _t, a); keep_path(aff, 25.0)
		var ag_path: PackedVector2Array = limb["ag_path"]
		var tgt: Vector2 = Anatomy.along(ag_path, 0.30); var vre: Vector2 = cs["ventral_root_end"]
		var eff := Anatomy.bez2(vre, vre.lerp(tgt, 0.5) + Vector2(w * 0.04, h * 0.05), tgt, 24)
		Anatomy.nerve(self, eff, Gfx.VIOLET, maxf(m * 0.005, 2.0), 0.6, _t, a); keep_path(eff, 25.0)
		_lab("biceps (agonist)", (limb["ag"] as Vector2) + Vector2(-m * 0.02, -m * 0.10), Kit.exc(), HORIZONTAL_ALIGNMENT_CENTER)
		_lab("triceps", (limb["ant"] as Vector2) + Vector2(m * 0.13, m * 0.05), Color("#4F5C66"), HORIZONTAL_ALIGNMENT_CENTER)
		_lab("muscle spindle", aend + Vector2(0, -m * 0.02), Kit.success(), HORIZONTAL_ALIGNMENT_CENTER)
		return {"stretch_reflex": [cs["mn"], "r", 6.0], "renshaw": [cs["renshaw"], "d", 14.0], "posture": [el, "r", 8.0], "reach": [hd, "u", 8.0]}

	# ------------------------------------------------------------ autonomic
	func _sc_auto(r: Rect2, a: float) -> Dictionary:
		var w := r.size.x; var h := r.size.y; var m := minf(w, h)
		var sy := 0.5 + 0.35 * sin(_t * 0.5); var pa := 0.5 - 0.35 * sin(_t * 0.5)
		var bs := h * 0.955 / 0.765; var br := Rect2(r.position.x + w * 0.5 - bs * 0.45, r.position.y + h * 0.015, bs * 0.9, bs)
		keep_rect(Rect2(br.position.x + br.size.x * 0.18, br.position.y, br.size.x * 0.64, br.size.y), 30.0)
		var res := Anatomy.body(self, br, {"alpha": a, "t": _t, "sy": sy, "pa": pa, "pupil": 0.5 + 0.4 * sin(_t * 0.5), "tms": fposmod(_t, 0.95) * 1000.0})
		var s: float = res["scale"]; var o: Vector2 = res["origin"]
		# the legend: two divisions, one body
		var lx := r.position.x + 18.0; var ly := r.position.y + h * 0.04
		if not Layout.phone:
			keep_rect(Rect2(lx - 4.0, ly - 16.0, 360.0, 102.0), 150.0)
			self.draw_line(Vector2(lx, ly), Vector2(lx + 34, ly), Anatomy.symp(), 3.0, true)
			Anatomy.text(self, "Sympathetic", Vector2(lx + 44, ly + 5), Anatomy.symp(), 15, HORIZONTAL_ALIGNMENT_LEFT, -1.0, true)
			Anatomy.text(self, "thoracolumbar outflow T1 to L2", Vector2(lx + 44, ly + 25), Kit.TEXT_SOFT, 14)
			self.draw_line(Vector2(lx, ly + 52), Vector2(lx + 34, ly + 52), Anatomy.para(), 3.0, true)
			Anatomy.text(self, "Parasympathetic", Vector2(lx + 44, ly + 57), Anatomy.para(), 15, HORIZONTAL_ALIGNMENT_LEFT, -1.0, true)
			Anatomy.text(self, "cranial (III, X) and sacral (S2 to S4)", Vector2(lx + 44, ly + 77), Kit.TEXT_SOFT, 14)
		var P := func(x: float, y: float) -> Vector2: return o + Vector2(x, y) * s
		var rx := o.x + s * 0.30
		for q in [["brainstem", Vector2(0.03, 0.09), Kit.TEXT_SOFT], ["vagus (X)", res["vagus"], Anatomy.para()], ["lungs", Vector2(0.13, 0.32), Kit.TEXT_SOFT], ["heart", res["heart"], Kit.TEXT_SOFT], ["stomach and gut", Vector2(0.10, 0.50), Kit.TEXT_SOFT], ["sacral outflow", res["sacral"], Anatomy.para()]]:
			var ap: Vector2 = P.call((q[1] as Vector2).x, (q[1] as Vector2).y) if (q[1] as Vector2).x < 0.5 and (q[1] as Vector2).y < 1.0 and not (q[0] in ["vagus (X)", "heart", "sacral outflow"]) else q[1]
			if Layout.phone: continue
			self.draw_line(ap, Vector2(rx - 6.0, ap.y), Color(q[2], 0.45), 1.2, true); self.draw_circle(ap, 2.6, q[2])
			keep_rect(Anatomy.tag(self, str(q[0]), Vector2(rx, ap.y + 5.0), q[2], 14, HORIZONTAL_ALIGNMENT_LEFT, false, 0.55), 120.0)
		_lab("sympathetic chain", P.call(-0.075, 0.54), Anatomy.symp(), HORIZONTAL_ALIGNMENT_RIGHT)
		return {"pupil": [(res["eye"] as Vector2) + Vector2(-s * 0.045, -s * 0.012), "l", 6.0], "peristalsis": [res["gut"], "r", 6.0], "fight_or_flight": [res["adrenal"], "l", 6.0]}

	# ------------------------------------------------------------ invertebrates
	func _sc_invert(r: Rect2, a: float) -> Dictionary:
		var w := r.size.x; var h := r.size.y; var m := minf(w, h)
		# salt gradient behind the salt-seeking worm
		var salt := r.position + Vector2(w * 0.80, h * 0.20)
		Gfx.stipple_disc(self, salt, m * 0.42, 900, Color(Kit.SENSOR, 0.55 * a), 17, 2.4, 1.8)   # stippled, densest at the source
		keep_rect(Rect2(salt - Vector2(m * 0.2, m * 0.2), Vector2(m * 0.4, m * 0.4)), 8.0)
		_lab("NaCl gradient", salt + Vector2(0, m * 0.07), Kit.SENSOR, HORIZONTAL_ALIGNMENT_CENTER)
		# the nematode that is touched (left, lower)
		var p1 := PackedVector2Array()
		for i in 40:
			var u := float(i) / 39.0; var th := 3.0 * PI * u
			p1.append(r.position + Vector2(w * (0.07 + 0.26 * u), h * (0.55 + 0.055 * sin(th + _t * 0.5))))
		var wr1 := Anatomy.worm(self, Gfx.cr_spline(p1, 3), h * 0.034, {"alpha": a, "t": _t}); keep_path(p1, 50.0, h * 0.04)
		var probe: Vector2 = wr1["head"]
		self.draw_line(probe + Vector2(-m * 0.06, -m * 0.10), probe + Vector2(-m * 0.012, -m * 0.012), Color(Kit.TEXT, 0.8 * a), 3.0, true)
		Gfx.wash(self, probe + Vector2(-m * 0.012, -m * 0.012), m * 0.025, Kit.RED, 0.45 * a, 3)
		# the worm crawling up the salt gradient (centre, top)
		var p2 := PackedVector2Array()
		for i in 44:
			var u := float(i) / 43.0
			p2.append(r.position + Vector2(w * (0.36 + 0.34 * u), h * (0.46 - 0.18 * u + 0.05 * sin(u * 9.0 + _t * 0.7))))
		var wr2 := Anatomy.worm(self, Gfx.cr_spline(p2, 3), h * 0.034, {"alpha": a, "t": _t + 1.0}); keep_path(p2, 50.0, h * 0.04)
		_lab("ASE salt neurons", (wr2["tail"] as Vector2) + Vector2(m * 0.02, m * 0.07), Kit.exc())
		# the lamprey (right, lower)
		var p3 := PackedVector2Array()
		for i in 48:
			var u := float(i) / 47.0; var wv := 0.045 * sin(u * 7.0 - _t * 3.0) * u
			p3.append(r.position + Vector2(w * (0.52 + 0.43 * u), h * (0.82 + wv * 2.2)))
		var lm := Anatomy.lamprey(self, p3, h * 0.040, {"alpha": a, "t": _t}); keep_path(p3, 50.0, h * 0.05)
		_lab("spinal cord CPG", (lm["mid"] as Vector2) + Vector2(0, -m * 0.09), Kit.exc(), HORIZONTAL_ALIGNMENT_CENTER)
		return {"reflex": [wr1["ring"], "u", 20.0], "worm": [wr2["head"], "u", 8.0], "lamprey": [lm["mid"], "d", 6.0]}
