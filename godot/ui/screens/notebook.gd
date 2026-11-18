class_name NotebookScreen
extends Screen
## The lab notebook (docs/DESIGN-0.3.md section 7): one entry per level, unlocked on its first pass, each a short true fact with its citation, grouped by
## chapter ("rotation"); plus the concept glossary built from every level's concepts_new, a concept being learned when the level that introduces it is passed.
## Entries come from Task::notebook() (Campaign.notebook); a level with no entry yet is simply absent. Scroll with the wheel, touch drag, or D-pad / stick.
## SP_NB_TAB=Notes|Concepts opens on a tab; SP_NB_SCROLL=<px> scrolls on open (screenshots).
const TABS := ["Notes", "Concepts"]
var tab := "Notes"
var scroll: ScrollContainer
var body: VBoxContainer
var tab_row: HBoxContainer
var head_count: Label
var back_btn: Button

func default_focus() -> Control: return back_btn

func build() -> void:
	var f := f(); var phone := Layout.phone
	if OS.get_environment("SP_NB_TAB") in TABS: tab = OS.get_environment("SP_NB_TAB")
	add_child(Kit.bench())
	var margin := MarginContainer.new(); margin.set_anchors_preset(Control.PRESET_FULL_RECT)
	var mx := int(Kit.f() * 120) if not phone else 24; var my := int(32 * f) if not phone else 10
	margin.add_theme_constant_override("margin_left", mx); margin.add_theme_constant_override("margin_right", mx)
	margin.add_theme_constant_override("margin_top", my); margin.add_theme_constant_override("margin_bottom", my); add_child(margin)
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", int(16 * f) if not phone else 6); margin.add_child(v)
	var hb := Kit.page_head("Lab notebook", func(): app.back(), phone); var head: HBoxContainer = hb[0]; back_btn = hb[1]; v.add_child(head)
	if phone:
		tab_row = Kit.tabs(TABS, tab, func(t): set_tab(t)); tab_row.size_flags_horizontal = Control.SIZE_EXPAND_FILL; tab_row.alignment = BoxContainer.ALIGNMENT_END; head.add_child(tab_row)
	else:
		head_count = Kit.italic("", 22, Kit.TEXT_SOFT); head_count.size_flags_horizontal = Control.SIZE_EXPAND_FILL; head_count.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT; head_count.size_flags_vertical = Control.SIZE_SHRINK_CENTER; head.add_child(head_count)
		tab_row = Kit.tabs(TABS, tab, func(t): set_tab(t)); v.add_child(tab_row)
	scroll = ScrollContainer.new(); scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL; scroll.follow_focus = true
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED; v.add_child(scroll)
	var pad := MarginContainer.new(); pad.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	for side in ["right", "bottom"]: pad.add_theme_constant_override("margin_" + side, int(Kit.SHADOW) + 14)
	scroll.add_child(pad)
	body = VBoxContainer.new(); body.size_flags_horizontal = Control.SIZE_EXPAND_FILL; body.add_theme_constant_override("separation", int(18 * f) if not phone else 10); pad.add_child(body)
	_fill()
	if OS.get_environment("SP_NB_SCROLL") != "":
		var px := int(OS.get_environment("SP_NB_SCROLL"))
		get_tree().create_timer(0.3).timeout.connect(func(): if is_instance_valid(scroll): scroll.scroll_vertical = px)

func set_tab(t: String) -> void:
	tab = t
	var old := tab_row; var idx := old.get_index(); var par := old.get_parent()
	par.remove_child(old); old.queue_free()
	tab_row = Kit.tabs(TABS, tab, func(x): set_tab(x)); par.add_child(tab_row); par.move_child(tab_row, idx)
	if Layout.phone: tab_row.size_flags_horizontal = Control.SIZE_EXPAND_FILL; tab_row.alignment = BoxContainer.ALIGNMENT_END
	_fill(); scroll.scroll_vertical = 0

func _fill() -> void:
	_chap = 0
	for c in body.get_children():
		body.remove_child(c); c.queue_free()
	if tab == "Notes": _notes()
	else: _concepts()

func _process(dt: float) -> void:
	if scroll == null: return
	var dir := Input.get_axis("ui_up", "ui_down") + Input.get_axis("ui_page_up", "ui_page_down") * 3.0
	dir += Input.get_joy_axis(0, JOY_AXIS_RIGHT_Y) if absf(Input.get_joy_axis(0, JOY_AXIS_RIGHT_Y)) > 0.3 else 0.0
	if dir != 0.0:
		scroll.scroll_vertical = clampi(scroll.scroll_vertical + int(dir * 700.0 * dt), 0, int(scroll.get_v_scroll_bar().max_value))

func _count_line(a: int, b: int, what: String) -> void:
	var s := "%d of %d %s" % [a, b, what]
	if head_count != null: head_count.text = s
	else: body.add_child(Kit.label(s, 16, Kit.TEXT_SOFT))

func _cols() -> int: return 1 if Layout.phone or Kit.vh < 700.0 else 2

## Levels of a chapter that have a notebook entry, in play order (spine and diagnosis; variants are not levels).
func _levels_with_notes(ch: String) -> Array:
	var out := []
	for n in Campaign.nodes(ch):
		if n["installed"] and str(n["kind"]) != "variant" and str(Campaign.notebook(str(n["id"]))["title"]) != "": out.append(n)
	return out

func _notes() -> void:
	var total := 0; var got := 0
	for c in Campaign.CHAPTERS:
		for n in _levels_with_notes(c["id"]):
			total += 1; got += 1 if Campaign.passed(str(n["id"])) else 0
	_count_line(got, total, "entries unlocked")
	if total == 0:
		body.add_child(Kit.label("No notebook entries are installed yet. Each level adds one: a short, sourced fact about the real nervous system, unlocked the first time you pass it.", 20, Kit.TEXT_SOFT, true)); return
	if got == 0: body.add_child(Kit.label("Pass a level to write its page.", 19, Kit.TEXT_SOFT, true))
	for c in Campaign.CHAPTERS:
		var ns := _levels_with_notes(c["id"])
		if ns.is_empty(): continue
		var k := 0
		for n in ns: k += 1 if Campaign.passed(str(n["id"])) else 0
		_chapter_head(str(c["title"]), "%d of %d" % [k, ns.size()])
		var g := GridContainer.new(); g.columns = _cols(); g.add_theme_constant_override("h_separation", 14); g.add_theme_constant_override("v_separation", 14); body.add_child(g)
		for n in ns: g.add_child(_entry(n))

## A chapter's running head: its name in Fraunces, a rule to the right edge, the count in italic.
var _chap := 0
func _chapter_head(title: String, count: String) -> void:
	_chap += 1
	var h := HBoxContainer.new(); h.add_theme_constant_override("separation", 14); body.add_child(h)
	var num := Kit.italic("%s." % _roman(_chap), 22, Kit.TEXT_SOFT); num.size_flags_vertical = Control.SIZE_SHRINK_CENTER; h.add_child(num)
	var t := Kit.display(title, 28, Kit.TEXT); t.size_flags_vertical = Control.SIZE_SHRINK_CENTER; h.add_child(t)
	var lead := Kit.Leader.new(); lead.size_flags_horizontal = Control.SIZE_EXPAND_FILL; h.add_child(lead)
	if count != "": var cnt := Kit.italic(count, 20, Kit.TEXT_SOFT); cnt.size_flags_vertical = Control.SIZE_SHRINK_CENTER; h.add_child(cnt)

static func _roman(n: int) -> String:
	var vals := [[10, "X"], [9, "IX"], [5, "V"], [4, "IV"], [1, "I"]]; var out := ""
	for v in vals:
		while n >= int(v[0]): out += str(v[1]); n -= int(v[0])
	return out

## An index card: ink plate once written, a pale ruled card while it is still blank.
func _card_box(done: bool) -> StyleBox:
	return Kit.plate_sb(Kit.PANEL_HI, 18) if done else Kit.sb(Kit.WELL, Kit.LINE, 2, 3, 18, false)

func _entry(n: Dictionary) -> Control:
	var id := str(n["id"]); var done := Campaign.passed(id); var e := Campaign.notebook(id)
	var p := PanelContainer.new(); p.size_flags_horizontal = Control.SIZE_EXPAND_FILL; p.size_flags_vertical = Control.SIZE_FILL
	p.add_theme_stylebox_override("panel", _card_box(done))
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", 6); p.add_child(v)
	v.add_child(Kit.small_caps(str(n["title"]), 15, Kit.MOTOR if done else Kit.TEXT_SOFT))
	if done:
		v.add_child(Kit.shout(str(e["title"]), 23, Kit.TEXT)); (v.get_child(-1) as Label).autowrap_mode = TextServer.AUTOWRAP_WORD_SMART; (v.get_child(-1) as Label).custom_minimum_size.x = 120
		v.add_child(Kit.label(str(e["text"]), 18, Kit.TEXT, true))
		if str(e["citation"]) != "": v.add_child(Kit.italic(str(e["citation"]), 17, Kit.TEXT_SOFT, true))
	else:
		v.add_child(Kit.italic("Not written yet", 22, Kit.TEXT_SOFT))
		v.add_child(Kit.label(Campaign.lock_reason(id) + " to unlock." if Campaign.lock_reason(id) != "" else "Pass this level to unlock.", 17, Kit.TEXT_SOFT, true))
	return p

func _concepts() -> void:
	var idx := Campaign.concept_index(); var total := idx.size(); var got := 0
	for c in idx: got += 1 if Campaign.passed(str(idx[c]["level"])) else 0
	_count_line(got, total, "concepts learned")
	if total == 0:
		body.add_child(Kit.label("No concepts are installed yet. Every level introduces one idea; they collect here as you pass the levels.", 20, Kit.TEXT_SOFT, true)); return
	var by_ch := {}
	for c in idx:
		var ch := Campaign.chapter_of(str(idx[c]["level"]))
		if not by_ch.has(ch): by_ch[ch] = []
		(by_ch[ch] as Array).append(c)
	for ch in Campaign.CHAPTERS:
		if not by_ch.has(ch["id"]): continue
		_chapter_head(str(ch["title"]), "")
		var g := GridContainer.new(); g.columns = _cols(); g.add_theme_constant_override("h_separation", 14); g.add_theme_constant_override("v_separation", 14); body.add_child(g)
		for c in by_ch[ch["id"]]: g.add_child(_concept_card(str(c), idx[c]))

func _concept_card(c: String, ix: Dictionary) -> Control:
	var lvl := str(ix["level"]); var done := Campaign.passed(lvl); var title := str(Campaign.info(lvl)["title"])
	var p := PanelContainer.new(); p.size_flags_horizontal = Control.SIZE_EXPAND_FILL; p.size_flags_vertical = Control.SIZE_FILL
	p.add_theme_stylebox_override("panel", _card_box(done))
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", 6); p.add_child(v)
	var t := Kit.shout(Campaign.concept_title(c) if done else "???", 22, Kit.TEXT if done else Kit.TEXT_SOFT); t.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART; t.custom_minimum_size.x = 120; v.add_child(t)
	if done:
		var bl := Campaign.concept_blurb(c)
		if bl != "": v.add_child(Kit.label(bl, 18, Kit.TEXT, true))
		var cc := Campaign.concept_citation(c)
		if cc != "": v.add_child(Kit.italic(cc, 17, Kit.CYAN, true))
		v.add_child(Kit.label("Introduced in %s." % title, 16, Kit.TEXT_SOFT, true))
		var used := []
		for u in ix["used_by"]:
			if Campaign.passed(str(u)): used.append(str(Campaign.info(str(u))["title"]))
		if not used.is_empty(): v.add_child(Kit.label("Used again in %s." % ", ".join(used), 16, Kit.TEXT_SOFT, true))
	else:
		v.add_child(Kit.label("Learned by passing %s." % title, 17, Kit.TEXT_SOFT, true))
	return p
