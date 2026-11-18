class_name RealScreen
extends Screen
## "What is real": what in Spike is NEST's own maths, what was re-fitted, what was tuned until a level passed, what is illustrative and what is generated.
## The text is docs/WHAT-IS-REAL.md, read at run time from res://docs/WHAT-IS-REAL.md (a link to the repository's docs/ page, packed into exports by the
## export preset's include filter), so the page and the screen cannot drift apart. A small Markdown reader draws it: headings, paragraphs, bullet lists
## and tables (a grid of cells on a desktop, a card per row on a phone). Repository-only details are left out: the "where it is documented" columns of
## file paths and the sentence addressed to the developers of this screen. Reachable from the title menu, the credits and the lab notebook.
const PATH := "res://docs/WHAT-IS-REAL.md"
const DROP_COLUMNS := ["Where it is documented"]
const FALLBACK := "# What is real here\n\nThe neuron models are NEST's own equations. Circuit weights, body coefficients and pass bands are tuned or illustrative, and the post-solve histograms are generated variants of the reference circuit, not other players. The full page is docs/WHAT-IS-REAL.md in the source."

var scroll: ScrollContainer
var back_btn: Button

func default_focus() -> Control: return back_btn

func build() -> void:
	var f := f(); var phone := Layout.phone
	add_child(Kit.bench())
	var margin := MarginContainer.new(); margin.set_anchors_preset(Control.PRESET_FULL_RECT)
	var mx := int(Kit.f() * 120) if not phone else 24; var my := int(32 * f) if not phone else 10
	margin.add_theme_constant_override("margin_left", mx); margin.add_theme_constant_override("margin_right", mx)
	margin.add_theme_constant_override("margin_top", my); margin.add_theme_constant_override("margin_bottom", my); add_child(margin)
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", int(16 * f) if not phone else 6); margin.add_child(v)
	var hb := Kit.page_head("What is real", func(): app.back(), phone); var head: HBoxContainer = hb[0]; back_btn = hb[1]; v.add_child(head)
	if not phone: head.add_child(Kit.running_head("Plate VI  ·  sources and simplifications"))
	scroll = ScrollContainer.new(); scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL; scroll.follow_focus = true
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED; v.add_child(scroll)
	var pad := MarginContainer.new(); pad.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	for side in ["right", "bottom"]: pad.add_theme_constant_override("margin_" + side, int(Kit.SHADOW) + 14)
	scroll.add_child(pad)
	var body := VBoxContainer.new(); body.size_flags_horizontal = Control.SIZE_EXPAND_FILL; body.add_theme_constant_override("separation", int(14 * f) if not phone else 8); pad.add_child(body)
	var md := FileAccess.get_file_as_string(PATH) if FileAccess.file_exists(PATH) else ""
	render(body, md if md.strip_edges() != "" else FALLBACK, phone)
	if OS.get_environment("SP_REAL_SCROLL") != "":
		var px := int(OS.get_environment("SP_REAL_SCROLL"))
		get_tree().create_timer(0.3).timeout.connect(func(): if is_instance_valid(scroll): scroll.scroll_vertical = px)

## Inline Markdown to plain text: bold, code and link marks dropped (the faces carry no inline styling here).
static func plain(t: String) -> String:
	var s := t.replace("**", "").replace("`", "")
	var re := RegEx.new(); re.compile("\\[([^\\]]+)\\]\\([^)]+\\)"); s = re.sub(s, "$1", true)
	return s.strip_edges()

## Sentences meant for the people who maintain the docs, not players.
static func _for_players(t: String) -> String:
	var re := RegEx.new(); re.compile("The in-game \"What is real\" screen[^.]*\\.\\s*"); t = re.sub(t, "", true)
	re.compile("\\s*The game card must say[^.]*\\.\\s*"); t = re.sub(t, " ", true)
	return t.strip_edges()

static func _cells(line: String) -> Array:
	var l := line.strip_edges()
	if l.begins_with("|"): l = l.substr(1)
	if l.ends_with("|"): l = l.substr(0, l.length() - 1)
	return Array(l.split("|")).map(func(c): return plain(str(c)))

var _para := ""
var _body: VBoxContainer
var _fs := 18
func _flush() -> void:
	if _para.strip_edges() != "":
		var t := _for_players(plain(_para))
		if t != "": _body.add_child(Kit.label(t, _fs, Kit.TEXT, true))
	_para = ""

func render(body: VBoxContainer, md: String, phone: bool) -> void:
	var lines := md.split("\n"); var i := 0; var fs := 18 if not phone else 16
	_para = ""; _body = body; _fs = fs
	while i < lines.size():
		var ln := lines[i]; var st := ln.strip_edges()
		if st == "": _flush(); i += 1; continue
		if st.begins_with("#"):
			_flush()
			var lvl := st.length() - st.lstrip("#").length(); var t := plain(st.lstrip("#"))
			if lvl == 1: pass   # the page title is the screen's own head
			elif lvl == 2: body.add_child(Kit.spacer(6)); body.add_child(Kit.display(t, 34 if not phone else 24, Kit.TEXT))
			else: body.add_child(Kit.display(t, 26 if not phone else 20, Kit.TEXT_SOFT))
			i += 1; continue
		if st.begins_with("|"):
			_flush()
			var rows := []
			while i < lines.size() and lines[i].strip_edges().begins_with("|"):
				var cs := _cells(lines[i])
				if not cs.all(func(c): return str(c).replace("-", "").replace(":", "").strip_edges() == ""): rows.append(cs)
				i += 1
			_table(body, rows, phone, fs)
			continue
		if st.begins_with("- "):
			_flush()
			var row := HBoxContainer.new(); row.add_theme_constant_override("separation", 10)
			row.add_child(Kit.label("·", fs, Kit.exc()))
			var l := Kit.label(_for_players(plain(st.substr(2))), fs, Kit.TEXT, true); row.add_child(l)
			body.add_child(row); i += 1; continue
		_para += (" " if _para != "" else "") + st; i += 1
	_flush()

## A Markdown table: the first row is the header. Columns in DROP_COLUMNS are left out.
func _table(body: VBoxContainer, rows: Array, phone: bool, fs: int) -> void:
	if rows.is_empty(): return
	var head: Array = rows[0]; var keep := []
	for k in head.size():
		if not DROP_COLUMNS.has(str(head[k])): keep.append(k)
	var card := Kit.ink_card(Kit.PANEL, 18 if not phone else 10); body.add_child(card)
	if phone or keep.size() > 4:   # a card per row: the first cell as its title, the others as "column: value" lines
		var v := VBoxContainer.new(); v.add_theme_constant_override("separation", 6); card.add_child(v)
		for r in range(1, rows.size()):
			var row: Array = rows[r]
			if r > 1: v.add_child(Kit.hsep())
			v.add_child(Kit.label(str(row[keep[0]]) if keep[0] < row.size() else "", fs, Kit.TEXT))
			for j in range(1, keep.size()):
				var k: int = keep[j]
				if k < row.size() and str(row[k]) != "": v.add_child(Kit.label("%s: %s" % [str(head[k]), str(row[k])], fs - 1, Kit.TEXT_SOFT, true))
		return
	var g := GridContainer.new(); g.columns = keep.size(); g.add_theme_constant_override("h_separation", 18); g.add_theme_constant_override("v_separation", 10); card.add_child(g)
	for r in rows.size():
		var row: Array = rows[r]
		for j in keep.size():
			var k: int = keep[j]
			var t := str(row[k]) if k < row.size() else ""
			var l := Kit.small_caps(t, 15, Kit.TEXT_SOFT) if r == 0 else Kit.label(t, fs - 1, Kit.TEXT if j == 0 else Kit.TEXT_SOFT, true)
			l.size_flags_horizontal = Control.SIZE_EXPAND_FILL; l.custom_minimum_size.x = 140.0 if j == 0 else 220.0
			if r == 0: l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
			g.add_child(l)

func _process(dt: float) -> void:
	if scroll == null: return
	var dir := Input.get_axis("ui_up", "ui_down") + Input.get_axis("ui_page_up", "ui_page_down") * 3.0
	if dir != 0.0: scroll.scroll_vertical = clampi(scroll.scroll_vertical + int(dir * 700.0 * dt), 0, int(scroll.get_v_scroll_bar().max_value))
