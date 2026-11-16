class_name TasksScreen
extends Screen
## The campaign map. Five chapters (Invertebrates, Spinal cord, Heart, Breathing, Autonomic), each drawn as its organ with the chapter's tasks as node cards
## on it: best score, three stars, lock state. Pick a node to read its brief and press BUILD (an empty bench with the task's sensors and motors) or
## REFERENCE (the reference circuit already wired). Chapters and node order come from Campaign (the engine's Task::chapter()); tasks the engine does not
## have yet show as dashed "not installed" ghosts. Header buttons: PARTS BIN (the neuron catalogue) and SANDBOX (no task, every part).
const SHORT := {"invertebrates": "INVERT.", "spinal": "SPINE", "heart": "HEART", "breathing": "LUNGS", "autonomic": "AUTONOMIC"}
var back_btn: Button
var first: Control
var chapter := ""
var sel := ""
var map: MapCanvas
var detail_box: VBoxContainer
var detail_scroll: ScrollContainer
var tab_btns := {}

func default_focus() -> Control: return first if first != null else back_btn

func build() -> void:
	first = null; tab_btns = {}
	var f := f(); var phone := Layout.phone
	add_child(Kit.bench())
	var margin := MarginContainer.new(); margin.set_anchors_preset(Control.PRESET_FULL_RECT)
	var mx := int(56 * f) if not phone else 16; var my := int(30 * f) if not phone else 8
	for side in ["left", "right"]: margin.add_theme_constant_override("margin_" + side, mx)
	margin.add_theme_constant_override("margin_top", my); margin.add_theme_constant_override("margin_bottom", my); add_child(margin)
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", int(14 * f) if not phone else 6); margin.add_child(v)
	var head := HBoxContainer.new(); head.add_theme_constant_override("separation", 14 if not phone else 8); v.add_child(head)
	var bs := int(24 * f) if not phone else 20
	back_btn = Kit.btn("BACK", func(): app.back(), "panel", bs, Vector2(120 if not phone else 84, 0)); head.add_child(back_btn)
	var tt := Kit.shout("Campaign", int(50 * f) if not phone else 30, Kit.TEXT); tt.size_flags_horizontal = Control.SIZE_EXPAND_FILL; tt.size_flags_vertical = Control.SIZE_SHRINK_CENTER; head.add_child(tt)
	head.add_child(Kit.btn("PARTS BIN", func(): app.show_screen("parts"), "cyan", bs))
	head.add_child(Kit.btn("SANDBOX", func(): app.open_lab(""), "panel", bs))
	if not Campaign.ext_ok():
		v.add_child(Kit.label("The spike-gd GDExtension is not loaded. Run scripts/build-gd.sh.", 22, Kit.RED, true)); return
	if chapter == "": chapter = app.campaign_chapter if app != null and app.campaign_chapter != "" else _default_chapter()
	if sel == "": sel = app.campaign_node if app != null else ""
	v.add_child(_tabs(f, phone))
	var body := HBoxContainer.new(); body.size_flags_vertical = Control.SIZE_EXPAND_FILL; body.add_theme_constant_override("separation", int(18 * f) if not phone else 10); v.add_child(body)
	var mp := Kit.panel(Color("#0a111d"), 0); mp.size_flags_horizontal = Control.SIZE_EXPAND_FILL; mp.size_flags_stretch_ratio = 2.3; mp.size_flags_vertical = Control.SIZE_EXPAND_FILL; body.add_child(mp)
	map = MapCanvas.new(); map.screen = self; map.size_flags_vertical = Control.SIZE_EXPAND_FILL; map.size_flags_horizontal = Control.SIZE_EXPAND_FILL; mp.add_child(map)
	var dp := Kit.panel(Kit.PANEL, int(20 * f) if not phone else 10); dp.size_flags_horizontal = Control.SIZE_EXPAND_FILL; dp.size_flags_stretch_ratio = 1.0; dp.size_flags_vertical = Control.SIZE_EXPAND_FILL; body.add_child(dp)
	detail_scroll = ScrollContainer.new(); detail_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED; dp.add_child(detail_scroll)
	detail_box = VBoxContainer.new(); detail_box.size_flags_horizontal = Control.SIZE_EXPAND_FILL; detail_box.add_theme_constant_override("separation", int(10 * f) if not phone else 6); detail_scroll.add_child(detail_box)
	_show_chapter(chapter)

## The chapter to open on: the first one with an open, unfinished node.
func _default_chapter() -> String:
	for c in Campaign.CHAPTERS:
		for n in Campaign.nodes(c["id"]):
			if n["installed"] and Campaign.unlocked(n["id"]) and not bool(Progress.best(n["id"])["passed"]): return c["id"]
	return Campaign.CHAPTERS[0]["id"]

func _tabs(f: float, phone: bool) -> Control:
	var h := HBoxContainer.new(); h.add_theme_constant_override("separation", 8 if not phone else 4)
	for c in Campaign.CHAPTERS:
		var id: String = c["id"]; var open := Campaign.chapter_open(id); var n := Campaign.installed_count(id)
		var st := Campaign.star_total(id)
		var txt := (str(SHORT[id]) if phone else str(c["title"]).to_upper())
		if not open and not phone: txt += "  (locked)"
		var b := Kit.btn(txt, func(): _show_chapter(id), "amber" if id == chapter else "panel", int(22 * f) if not phone else 18, Vector2(0, 0))
		b.size_flags_horizontal = Control.SIZE_EXPAND_FILL; b.tooltip_text = ("%s: %d of %d stars" % [c["title"], st.x, st.y]) if n > 0 else ("%s: no tasks installed yet" % c["title"])
		tab_btns[id] = b; h.add_child(b)
	return h

func _show_chapter(id: String) -> void:
	chapter = id
	if app != null: app.campaign_chapter = id
	for k in tab_btns: Kit.skin(tab_btns[k], "amber" if k == id else "panel", 11 if Layout.phone else int(22 * f() * 0.6))
	var nodes := Campaign.nodes(id)
	var ok := false
	for n in nodes:
		if n["id"] == sel: ok = true
	if not ok:   # the first open unfinished node, else the first installed one
		sel = ""
		for n in nodes:
			if sel == "" and n["installed"] and Campaign.unlocked(n["id"]) and not bool(Progress.best(n["id"])["passed"]): sel = n["id"]
		for n in nodes:
			if sel == "" and n["installed"]: sel = n["id"]
		if sel == "" and not nodes.is_empty(): sel = nodes[0]["id"]
	map.set_chapter(id, nodes); first = map.first_card() if map.first_card() != null else back_btn
	_fill_detail()
	if Kit.pad_mode and app != null: app.pad.ensure_focus(true)

func select_node(id: String) -> void:
	sel = id
	if app != null: app.campaign_node = id
	map.queue_redraw()
	for c in map.cards: c.queue_redraw()
	_fill_detail()

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
		detail_box.add_child(Kit.shout(str(cd["title"]), int(34 * f) if not phone else 24, Kit.TEXT))
		detail_box.add_child(Kit.label(str(cd["blurb"]), small, Kit.TEXT_SOFT, true)); return
	var info := Campaign.info(sel) if node["installed"] else {}
	detail_box.add_child(Kit.label(str(cd["title"]).to_upper(), int(16 * f) if not phone else 15, Gfx.VIOLET))
	detail_box.add_child(Kit.shout(str(node["title"]), int(36 * f) if not phone else 24, Kit.TEXT))
	if not node["installed"]:
		detail_box.add_child(Kit.label("Not installed in this build yet. It appears here as soon as the engine has the task; nothing to do on your side.", small, Kit.TEXT_SOFT, true))
		detail_box.add_child(Kit.label(str(cd["blurb"]), small, Kit.TEXT_SOFT, true)); return
	var open := Campaign.unlocked(sel); var best: Dictionary = Progress.best(sel); var par := Progress.budget(sel)
	var chips := HFlowContainer.new(); chips.add_theme_constant_override("h_separation", 8); chips.add_theme_constant_override("v_separation", 6); detail_box.add_child(chips)
	chips.add_child(Kit.chip("%d s episode" % int(round(float(info["duration_ms"]) / 1000.0)), Kit.PANEL_HI, 15)); (chips.get_child(0).get_child(0) as Label).label_settings = Kit.ls(15, Kit.TEXT, true)
	chips.add_child(Kit.chip("BEST %d" % int(round(float(best["value"]))) if float(best["value"]) > 0.0 else "NOT TRIED", Kit.GREEN if bool(best["passed"]) else Kit.exc(), 15))
	if bool(best["passed"]): chips.add_child(Kit.chip("PASSED", Kit.GREEN, 15))
	if not open: chips.add_child(Kit.chip("LOCKED", Kit.RED, 15))
	if str(info["view"]) != "polyline": chips.add_child(Kit.chip(str(info["view"]).to_upper() + " VIEW", Gfx.VIOLET, 15))
	var bt := HBoxContainer.new(); bt.add_theme_constant_override("separation", 10); detail_box.add_child(bt)
	var sid := sel
	var b := Kit.btn("BUILD" if open else "LOCKED", func(): if open: app.open_lab(sid), "amber" if open else "panel", int(24 * f) if not phone else 19); b.size_flags_horizontal = Control.SIZE_EXPAND_FILL; bt.add_child(b)
	if not open: b.disabled = true
	var rb := Kit.btn("REFERENCE", func(): app.open_lab(sid, true), "cyan", int(24 * f) if not phone else 19); rb.size_flags_horizontal = Control.SIZE_EXPAND_FILL; bt.add_child(rb)
	var tiers := [["Pass the task", 1], ["Pass on 5 random seeds (TEST x5)", 2], ["Pass with %d neurons or fewer" % par if par > 0 else "Pass on 5 seeds, small circuit", 3]]
	for t in tiers:
		var r := HBoxContainer.new(); r.add_theme_constant_override("separation", 10); detail_box.add_child(r)
		var s := _Star.new(); s.lit = int(best["stars"]) >= int(t[1]); s.custom_minimum_size = Vector2(26, 26); r.add_child(s)
		var l := Kit.label(str(t[0]), int(17 * f) if not phone else 15, Kit.TEXT if s.lit else Kit.TEXT_SOFT, true); l.size_flags_vertical = Control.SIZE_SHRINK_CENTER; r.add_child(l)
	if not open: detail_box.add_child(Kit.label(Campaign.lock_reason(sel) + ".", small, Kit.RED, true))
	var brief := str(info["brief"])
	detail_box.add_child(Kit.label(_lead(brief, 420) if not phone else _lead(brief, 220), small, Kit.TEXT_SOFT, true))
	for pair in [["SENSORS", info["sensors"], Kit.GREEN], ["MOTORS", info["motors"], Gfx.VIOLET], ["MODULATORS", info["modulators"], Kit.exc()]]:
		if (pair[1] as Array).is_empty(): continue
		var row := HFlowContainer.new(); row.add_theme_constant_override("h_separation", 8); row.add_theme_constant_override("v_separation", 6); detail_box.add_child(row)
		var kl := Kit.label(pair[0], int(15 * f) if not phone else 15, Kit.TEXT_SOFT); kl.custom_minimum_size.x = 112 * f if not phone else 100; kl.size_flags_vertical = Control.SIZE_SHRINK_CENTER; row.add_child(kl)
		for ch in pair[1]: row.add_child(Kit.chip(str(ch), pair[2], 15))

## The first sentences of a brief that fit in `n` characters.
static func _lead(t: String, n: int) -> String:
	var out := ""
	for sent in t.split(". "):
		var nxt := (out + (" " if out != "" else "") + sent + ("" if sent.ends_with(".") else "."))
		if out != "" and nxt.length() > n: break
		out = nxt
	return out

class _Star extends Control:
	var lit := false
	func _draw() -> void: ScoreCard.draw_star_on(self, size * 0.5, minf(size.x, size.y) * 0.46, Kit.AMBER if lit else Color(Kit.LINE, 0.95))

## One task on the map: a button that draws itself (title, stars, best, lock / ghost state).
class NodeCard extends Button:
	var node := {}
	var screen: TasksScreen
	func _init() -> void:
		flat = true; focus_mode = Kit.focus_mode()
		for n in ["normal", "hover", "pressed", "disabled", "focus"]: add_theme_stylebox_override(n, StyleBoxEmpty.new())
	func min_text_px() -> float: return float(Kit.fs(14)) * Kit.ppl
	func _draw() -> void:
		var id := str(node["id"]); var inst: bool = node["installed"]
		var best: Dictionary = Progress.best(id) if inst else {"value": 0.0, "passed": false, "stars": 0}
		var open := inst and Campaign.unlocked(id); var passed := bool(best["passed"]); var selected := screen != null and screen.sel == id
		var edge := Kit.GREEN if passed else (Kit.exc() if open else Kit.LINE)
		var fill := Color("#101c30") if open else Color("#0c1422")
		var hov := is_hovered()
		var r := Rect2(Vector2.ZERO, size)
		if open: Gfx.glow(self, size * 0.5, size.x * 0.72, Color(edge, 0.10 if not hov else 0.2))
		draw_style_box(Kit.sb(fill.lightened(0.06) if hov else fill, Color(edge, 0.9 if (open or passed) else 0.7), 2 if selected else 1, 12, 0, false), r)
		if selected: draw_style_box(Kit.sb(Color(0, 0, 0, 0), Color(1, 1, 1, 0.9), 2, 15, 0, false), r.grow(4.0))
		var fz := Kit.fs(14); var ft := Kit.fs(22 if not Layout.phone else 17); var pad := 14.0
		var tcol := Kit.TEXT if open else Kit.TEXT_SOFT
		draw_string(Fonts.bold, Vector2(pad, pad + float(ft) * 0.88), str(node["title"]), HORIZONTAL_ALIGNMENT_LEFT, size.x - pad * 2.0 - (22.0 if passed else 0.0), ft, tcol)
		if passed:   # a tick
			var c := Vector2(size.x - pad - 8.0, pad + float(ft) * 0.5)
			draw_circle(c, 9.0, Kit.GREEN); draw_polyline(PackedVector2Array([c + Vector2(-4, 0), c + Vector2(-1, 3.5), c + Vector2(5, -3.5)]), Kit.BENCH, 2.2, true)
		var y2 := pad + float(ft) + 8.0
		for k in 3: ScoreCard.draw_star_on(self, Vector2(pad + 9.0 + 22.0 * k, y2 + 9.0), 8.5, Kit.AMBER if k < int(best["stars"]) else Color(Kit.LINE, 0.95))
		var line3 := ""; var col3 := Kit.TEXT_SOFT
		if not inst: line3 = "NOT YET"
		elif not open: line3 = "LOCKED"; col3 = Kit.RED
		elif float(best["value"]) > 0.0: line3 = "best %d" % int(round(float(best["value"])))
		else: line3 = "NOT TRIED"; col3 = Kit.exc()
		draw_string(Fonts.regular, Vector2(pad + 78.0, y2 + 15.0), line3, HORIZONTAL_ALIGNMENT_LEFT, size.x - pad - 78.0, fz, col3)
		var par := Progress.budget(id) if inst else 0
		var y3 := size.y - pad + 2.0
		if not inst: draw_string(Fonts.regular, Vector2(pad, y3), "comes with the engine", HORIZONTAL_ALIGNMENT_LEFT, size.x - pad * 2.0, fz, Color(Kit.TEXT_SOFT, 0.8))
		elif not open:
			_lock_icon(Vector2(pad + 8.0, y3 - 7.0))
			draw_string(Fonts.regular, Vector2(pad + 22.0, y3), Campaign.lock_reason(id), HORIZONTAL_ALIGNMENT_LEFT, size.x - pad * 2.0 - 22.0, fz, Kit.TEXT_SOFT)
		else: draw_string(Fonts.regular, Vector2(pad, y3), ("par %d neurons" % par) if par > 0 else ("%d s" % int(round(float(Campaign.info(id)["duration_ms"]) / 1000.0))), HORIZONTAL_ALIGNMENT_LEFT, size.x - pad * 2.0, fz, Kit.TEXT_SOFT)
		if has_focus(): draw_style_box(Kit.ring_sb(), r)
	func _lock_icon(c: Vector2) -> void:
		draw_rect(Rect2(c + Vector2(-5, -1), Vector2(10, 8)), Kit.TEXT_SOFT); draw_arc(c + Vector2(0, -1), 3.6, PI, TAU, 10, Kit.TEXT_SOFT, 1.8, true)

## The organ and the node cards on it. Cards sit at the chapter's planned positions; the organ is a quiet vector drawing behind them.
class MapCanvas extends Control:
	var screen: TasksScreen
	var chapter := ""
	var nodes: Array = []
	var cards: Array = []
	var _t := 0.0
	func _ready() -> void: clip_contents = true
	func min_text_px() -> float: return float(Kit.fs(14)) * Kit.ppl
	func first_card() -> Control:
		for c in cards:
			if bool(c.node["installed"]) and Campaign.unlocked(str(c.node["id"])): return c
		return cards[0] if not cards.is_empty() else null
	func card_size() -> Vector2:
		var f := Kit.f()
		if Layout.phone: return Vector2(minf(size.x * 0.34, 210.0), float(Kit.fs(17)) + float(Kit.fs(14)) * 2.0 + 44.0)
		return Vector2(clampf(size.x * 0.26, 200.0, 360.0), float(Kit.fs(22)) + float(Kit.fs(14)) * 2.0 + 62.0)
	func set_chapter(id: String, ns: Array) -> void:
		chapter = id; nodes = ns
		for c in cards:
			remove_child(c); c.queue_free()
		cards = []
		for n in ns:
			var c := NodeCard.new(); c.node = n; c.screen = screen; c.pressed.connect(func(): screen.select_node(str(n["id"])))
			c.tooltip_text = str(n["title"]); add_child(c); cards.append(c)
		_layout(); queue_redraw()
	func _notification(what: int) -> void:
		if what == NOTIFICATION_RESIZED: _layout()
	func _layout() -> void:
		var cs := card_size()
		for c in cards:
			var p: Vector2 = c.node["pos"]; c.size = cs
			c.position = Vector2(clampf(p.x * size.x - cs.x * 0.5, 8.0, maxf(size.x - cs.x - 8.0, 8.0)), clampf(p.y * size.y - cs.y * 0.5, 8.0, maxf(size.y - cs.y - 8.0, 8.0)))
		queue_redraw()
	func _process(dt: float) -> void:
		if Kit.motion() and is_visible_in_tree(): _t += dt; queue_redraw()
	func _draw() -> void:
		var r := Rect2(Vector2.ZERO, size)
		draw_rect(r, Color("#0b1626"))
		_organ(chapter, r)
		# the path through the chapter: a curve from card to card, bright once the earlier task is passed
		var prev: Control = null; var prev_pass := false
		for c in cards:
			if prev != null:
				var a := prev.position + prev.size * 0.5; var b: Vector2 = c.position + c.size * 0.5
				var mid := (a + b) * 0.5 + (b - a).orthogonal().normalized() * minf(40.0, (b - a).length() * 0.14)
				var pts := PackedVector2Array()
				for k in 21:
					var u := float(k) / 20.0; pts.append(a.lerp(mid, u).lerp(mid.lerp(b, u), u))
				Gfx.dashed(self, pts, Color(Kit.GREEN if prev_pass else Kit.LINE, 0.75 if prev_pass else 0.8), 2.4 if prev_pass else 1.8, 9.0, 8.0, _t * 18.0 if prev_pass else 0.0)
				if prev_pass and Kit.motion():
					var q := pts[int(fmod(_t * 0.4, 1.0) * 20.0)]; draw_circle(q, 4.0, Kit.GREEN); Gfx.glow(self, q, 14.0, Color(Kit.GREEN, 0.5))
			prev = c; prev_pass = bool(c.node["installed"]) and bool(Progress.best(str(c.node["id"]))["passed"])
		var cd := Campaign.chapter_def(chapter); var st := Campaign.star_total(chapter); var fz := Kit.fs(14)
		var cap := ("%s   %d of %d stars" % [str(cd["title"]).to_upper(), st.x, st.y]) if st.y > 0 else str(cd["title"]).to_upper()
		draw_string(Fonts.bold, Vector2(14, size.y - 12), cap, HORIZONTAL_ALIGNMENT_LEFT, size.x - 28, fz, Kit.TEXT_SOFT)
		if not Campaign.chapter_open(chapter):
			var first_id := str(Campaign.PLANNED[chapter][0][0]) if Campaign.PLANNED.has(chapter) else ""
			draw_string(Fonts.regular, Vector2(14, 10.0 + float(fz)), "Locked. " + Campaign.lock_reason(first_id) + ".", HORIZONTAL_ALIGNMENT_LEFT, size.x - 28, fz, Kit.RED)

	func _organ(id: String, r: Rect2) -> void:
		var a := 1.0 if Campaign.chapter_open(id) else 0.5
		match str(Campaign.chapter_def(id)["organ"]):
			"worm": _worm(r, a)
			"spine": _spine(r, a)
			"heart": _heart(r, a)
			"lungs": _lungs(r, a)
			"autonomic": _autonomic(r, a)

	func _worm(r: Rect2, a: float) -> void:
		var pts := PackedVector2Array(); var n := 36
		for i in n:
			var u := float(i) / float(n - 1); pts.append(Vector2(r.size.x * (0.06 + 0.88 * u), r.size.y * (0.5 + 0.17 * sin(u * 9.0 + _t * 0.6))))
		var sp := Gfx.cr_spline(pts, 4); var W := r.size.y * 0.07
		Gfx.tube_rim(self, sp, func(u): return W * (0.5 + 0.5 * sqrt(clampf(sin(PI * clampf(u * 0.92 + 0.04, 0, 1)), 0, 1))), Color("#0c2328"), Color(Gfx.BODY, 0.28 * a), 1.6)
		draw_polyline(sp, Color(Gfx.BODY, 0.22 * a), 2.0, true)   # the nerve cord
		for i in range(0, sp.size(), 9): draw_circle(sp[i], 4.0, Color(Gfx.BODY, 0.30 * a))   # ganglia
		Gfx.glow(self, Vector2(r.size.x * 0.92, r.size.y * 0.26), r.size.y * 0.5, Color("#5fd9c0", 0.10 * a))   # the salt
		Gfx.glow(self, Vector2(r.size.x * 0.08, r.size.y * 0.86), r.size.y * 0.35, Color(Gfx.FISH, 0.06 * a))

	func _spine(r: Rect2, a: float) -> void:
		var n := 14; var y0 := r.size.y * 0.5
		for i in n:
			var u := float(i) / float(n - 1); var x := r.size.x * (0.05 + 0.9 * u); var y := y0 - sin(u * PI) * r.size.y * 0.06
			var vw := r.size.x * 0.05; var vh := r.size.y * 0.2
			Gfx.rounded_rect(self, Rect2(Vector2(x - vw * 0.5, y - vh * 0.5), Vector2(vw, vh)), Color("#122338", 0.9 * a), Color(Kit.LINE, 0.9 * a), vw * 0.35, 1.4)
			draw_line(Vector2(x, y - vh * 0.5), Vector2(x - vw * 0.2, y - vh * 1.45), Color(Kit.CYAN, 0.22 * a), 1.6, true)   # dorsal root (sensory)
			draw_line(Vector2(x, y + vh * 0.5), Vector2(x + vw * 0.2, y + vh * 1.45), Color(Gfx.VIOLET, 0.22 * a), 1.6, true)  # ventral root (motor)
		var cord := PackedVector2Array()
		for i in 40:
			var u := float(i) / 39.0; cord.append(Vector2(r.size.x * (0.05 + 0.9 * u), y0 - sin(u * PI) * r.size.y * 0.06))
		draw_polyline(cord, Color(Kit.exc(), 0.12 * a), 11.0, true); draw_polyline(cord, Color(Kit.exc(), 0.35 * a), 2.4, true)
		for i in 5: Gfx.glow(self, cord[int(fposmod(_t * 0.12 + float(i) * 0.21, 1.0) * 39.0)], 11.0, Color(Kit.exc(), 0.5 * a))   # spikes travelling along the cord

	func _heart_pts(c: Vector2, s: float) -> PackedVector2Array:
		var out := PackedVector2Array()
		for i in 72:
			var t := TAU * float(i) / 72.0
			out.append(c + Vector2(16.0 * pow(sin(t), 3.0), -(13.0 * cos(t) - 5.0 * cos(2.0 * t) - 2.0 * cos(3.0 * t) - cos(4.0 * t))) * s)
		return out
	func _heart(r: Rect2, a: float) -> void:
		var beat := 1.0 + 0.035 * pow(maxf(sin(_t * TAU * 1.1), 0.0), 6.0)
		var c := Vector2(r.size.x * 0.5, r.size.y * 0.52); var s := minf(r.size.x / 50.0, r.size.y / 42.0) * beat
		var pts := _heart_pts(c, s)
		Gfx.glow(self, c, minf(r.size.x, r.size.y) * 0.7, Color(Kit.RED, 0.07 * a))
		draw_colored_polygon(pts, Color("#2a1020", 0.9 * a))
		var closed := pts.duplicate(); closed.append(pts[0])
		draw_polyline(closed, Color(Kit.RED, 0.14 * a), 9.0, true); draw_polyline(closed, Color(Kit.RED, 0.55 * a), 2.0, true)
		draw_arc(c + Vector2(0, -s * 10.0), s * 3.2, PI * 1.05, PI * 1.95, 16, Color(Kit.RED, 0.45 * a), 4.0, true)   # aorta
		draw_line(c + Vector2(s * 3.0, -s * 11.0), c + Vector2(s * 4.5, -s * 17.0), Color(Kit.RED, 0.5 * a), 7.0, true)
		draw_line(c + Vector2(-s * 5.0, -s * 11.0), c + Vector2(-s * 7.0, -s * 17.0), Color(Kit.CYAN, 0.45 * a), 7.0, true)
		var ecg := PackedVector2Array(); var y := r.size.y * 0.9   # an ECG running across the bottom
		for i in 120:
			var u := float(i) / 119.0; var ph := fposmod(u * 4.0 - _t * 0.5, 1.0); var v := 0.0
			if ph > 0.18 and ph < 0.22: v = -1.0
			elif ph >= 0.22 and ph < 0.26: v = 0.5
			elif ph > 0.4 and ph < 0.5: v = -0.25 * sin((ph - 0.4) / 0.1 * PI)
			ecg.append(Vector2(r.size.x * u, y + v * r.size.y * 0.05))
		draw_polyline(ecg, Color(Kit.GREEN, 0.28 * a), 1.8, true)
	func _lung(c: Vector2, w: float, h: float, flip: float, k: float, a: float) -> void:
		var pts := PackedVector2Array()
		for i in 40:
			var t := TAU * float(i) / 40.0
			pts.append(c + Vector2(cos(t) * w * 0.5 * (0.82 + 0.18 * (1.0 if sin(t) < 0.0 else 0.7)), sin(t) * h * 0.5) * k)
		draw_colored_polygon(pts, Color("#0d2230", 0.9 * a))
		var closed := pts.duplicate(); closed.append(pts[0])
		draw_polyline(closed, Color(Kit.CYAN, 0.14 * a), 8.0, true); draw_polyline(closed, Color(Kit.CYAN, 0.5 * a), 2.0, true)
		for b in 4:   # bronchioles
			draw_line(Vector2(c.x - flip * w * 0.5, c.y - h * 0.38), Vector2(c.x + flip * w * (0.12 + 0.09 * b), c.y - h * 0.3 + float(b) * h * 0.2 + h * 0.06), Color(Kit.CYAN, 0.2 * a), 1.6, true)
	func _lungs(r: Rect2, a: float) -> void:
		var k := 1.0 + 0.045 * sin(_t * TAU * 0.28); var cy := r.size.y * 0.5
		var w := r.size.x * 0.27; var h := r.size.y * 0.78
		_lung(Vector2(r.size.x * 0.33, cy), w, h, -1.0, k, a); _lung(Vector2(r.size.x * 0.67, cy), w, h, 1.0, k, a)
		draw_line(Vector2(r.size.x * 0.5, r.size.y * 0.06), Vector2(r.size.x * 0.5, r.size.y * 0.36), Color(Kit.CYAN, 0.55 * a), 5.0, true)   # trachea
		draw_line(Vector2(r.size.x * 0.5, r.size.y * 0.36), Vector2(r.size.x * 0.36, r.size.y * 0.5), Color(Kit.CYAN, 0.4 * a), 3.5, true)
		draw_line(Vector2(r.size.x * 0.5, r.size.y * 0.36), Vector2(r.size.x * 0.64, r.size.y * 0.5), Color(Kit.CYAN, 0.4 * a), 3.5, true)
		draw_arc(Vector2(r.size.x * 0.5, r.size.y * 1.02), r.size.x * 0.3, PI * 1.12, PI * 1.88, 24, Color(Gfx.VIOLET, 0.3 * a), 3.0, true)   # the diaphragm
	func _autonomic(r: Rect2, a: float) -> void:
		var ec := Vector2(r.size.x * 0.17, r.size.y * 0.34); var ew := r.size.x * 0.13; var eh := r.size.y * 0.15   # the eye, left
		var lid := PackedVector2Array()
		for i in 25:
			var u := float(i) / 24.0; lid.append(ec + Vector2((u - 0.5) * 2.0 * ew, -sin(u * PI) * eh))
		for i in 25:
			var u := 1.0 - float(i) / 24.0; lid.append(ec + Vector2((u - 0.5) * 2.0 * ew, sin(u * PI) * eh))
		draw_colored_polygon(lid, Color("#0e1b2c", 0.9 * a)); var lc := lid.duplicate(); lc.append(lid[0]); draw_polyline(lc, Color(Gfx.FISH, 0.5 * a), 2.0, true)
		var pr := eh * (0.38 + 0.12 * sin(_t * 0.8)); draw_circle(ec, eh * 0.78, Color("#1d3a5a", a)); draw_circle(ec, pr, Color("#04070c", a)); draw_circle(ec + Vector2(-pr * 0.35, -pr * 0.35), pr * 0.18, Color(1, 1, 1, 0.5 * a))
		var gut := PackedVector2Array()   # the gut, bottom
		for i in 60:
			var u := float(i) / 59.0; gut.append(Vector2(r.size.x * (0.1 + 0.8 * u), r.size.y * (0.74 + 0.07 * sin(u * 14.0))))
		draw_polyline(gut, Color(Kit.exc(), 0.10 * a), 18.0, true); draw_polyline(gut, Color(Kit.exc(), 0.28 * a), 2.2, true)
		Gfx.glow(self, gut[int(fposmod(_t * 0.08, 1.0) * 59.0)], 22.0, Color(Kit.exc(), 0.30 * a))   # the bolus
		var hc := Vector2(r.size.x * 0.83, r.size.y * 0.34)   # the heart, right
		var hp := _heart_pts(hc, minf(r.size.x, r.size.y) * 0.014 * (1.0 + 0.04 * pow(maxf(sin(_t * TAU * 1.1), 0.0), 6.0)))
		draw_colored_polygon(hp, Color("#2a1020", 0.85 * a)); var hl := hp.duplicate(); hl.append(hp[0]); draw_polyline(hl, Color(Kit.RED, 0.5 * a), 2.0, true)
		for k in 2:   # the two nerves between eye and heart: sympathetic (amber) and parasympathetic (cyan)
			var col := Kit.exc() if k == 0 else Kit.inh()
			var pts := Gfx.bez_points(ec + Vector2(ew, 0), Vector2(r.size.x * 0.35, r.size.y * (0.12 + 0.5 * k)), Vector2(r.size.x * 0.65, r.size.y * (0.12 + 0.5 * k)), hc - Vector2(30.0, 0), 28)
			draw_polyline(pts, Color(col, 0.20 * a), 1.8, true)
