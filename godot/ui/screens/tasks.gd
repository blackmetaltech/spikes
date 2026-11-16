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

## One task on the map: a pin on the anatomy with a small label (title and stars). The detail (brief, lock reason, best score) lives in the side panel.
class NodeCard extends Button:
	var node := {}
	var screen: TasksScreen
	var idx := 0
	var side := "r"
	var gap := 0.0
	var disc := Vector2.ZERO      # centre of the pin disc in local coordinates
	var _plate := Rect2()
	func _init() -> void:
		flat = true; focus_mode = Kit.focus_mode()
		for n in ["normal", "hover", "pressed", "disabled", "focus"]: add_theme_stylebox_override(n, StyleBoxEmpty.new())
	func min_text_px() -> float: return float(Kit.fs(14)) * Kit.ppl
	func _R() -> float: return 13.0 if Layout.phone else 17.0
	func _status() -> Array:
		var id := str(node["id"]); var inst: bool = node["installed"]
		var best: Dictionary = Progress.best(id) if inst else {"value": 0.0, "passed": false, "stars": 0}
		if not inst: return ["NOT YET", Kit.TEXT_SOFT, best]
		if not Campaign.unlocked(id): return ["LOCKED", Kit.RED, best]
		if float(best["value"]) > 0.0: return ["best %d" % int(round(float(best["value"]))), Kit.TEXT_SOFT, best]
		return ["NOT TRIED", Kit.exc(), best]
	## Size the button for its label and put the disc on `anchor` (map coordinates), keeping the whole label inside `bounds`.
	func place(anchor: Vector2, side_: String, bounds: Rect2, gap_ := 0.0) -> void:
		side = side_; gap = gap_; var R := _R(); var ft := Kit.fs(16 if not Layout.phone else 15); var fz := Kit.fs(14)
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
	func _has_point(p: Vector2) -> bool: return Rect2(Vector2.ZERO, size).grow(4.0).has_point(p)
	func _draw() -> void:
		var id := str(node["id"]); var inst: bool = node["installed"]; var st := _status(); var best: Dictionary = st[2]
		var open := inst and Campaign.unlocked(id); var passed := bool(best["passed"]); var selected := screen != null and screen.sel == id
		var edge := Kit.GREEN if passed else (Kit.exc() if open else Kit.LINE.lightened(0.35))
		var hov := is_hovered(); var R := _R()
		var ft := Kit.fs(16 if not Layout.phone else 15); var fz := Kit.fs(14)
		# the label plate
		draw_style_box(Kit.sb(Color(Kit.BENCH, 0.86 if not hov else 0.95), Color(edge, 0.85 if (selected or hov) else 0.45), 2 if selected else 1, 9, 0, false), _plate)
		var tcol := Kit.TEXT if open else Kit.TEXT_SOFT
		var tx := _plate.position.x + 9.0; var ty := _plate.position.y + 6.0 + float(ft) * 0.86
		draw_string(Fonts.bold, Vector2(tx, ty), str(node["title"]), HORIZONTAL_ALIGNMENT_LEFT, _plate.size.x - 14.0, ft, tcol)
		var y2 := ty + 7.0
		for k in 3: ScoreCard.draw_star_on(self, Vector2(tx + 8.0 + 18.0 * k, y2 + float(fz) * 0.5), 7.0, Kit.AMBER if k < int(best["stars"]) else Color(Kit.LINE, 0.95))
		if not Layout.phone: draw_string(Fonts.regular, Vector2(tx + 56.0, y2 + float(fz) * 0.86), str(st[0]), HORIZONTAL_ALIGNMENT_LEFT, _plate.size.x - 64.0, fz, st[1])
		if gap > 0.0:
			var q := Vector2(clampf(disc.x, _plate.position.x, _plate.end.x), clampf(disc.y, _plate.position.y, _plate.end.y))
			draw_line(disc + (q - disc).normalized() * R, q, Color(edge, 0.7), 1.6, true)
		# the pin
		if open: Gfx.glow(self, disc, R * (3.4 if hov else 2.6), Color(edge, 0.30 if hov else 0.20))
		if selected:
			var pu := 0.5 + 0.5 * sin(Time.get_ticks_msec() * 0.004) if Kit.motion() else 1.0
			draw_arc(disc, R + 6.0 + 2.0 * pu, 0, TAU, 40, Color(1, 1, 1, 0.55 + 0.4 * pu), 2.0, true)
		draw_circle(disc, R, Color("#0a1424", 0.96))
		draw_arc(disc, R, 0, TAU, 40, Color(edge, 0.95), 3.0 if open or passed else 2.0, true)
		if not inst:
			Gfx.dashed(self, PackedVector2Array([disc + Vector2(-R * 0.5, 0), disc + Vector2(R * 0.5, 0)]), Color(Kit.TEXT_SOFT, 0.6), 1.6, 3.0, 3.0)
		elif passed:
			draw_circle(disc, R - 3.5, Kit.GREEN)
			draw_polyline(PackedVector2Array([disc + Vector2(-R * 0.35, 0), disc + Vector2(-R * 0.08, R * 0.28), disc + Vector2(R * 0.40, -R * 0.26)]), Kit.BENCH, 2.6, true)
		elif not open:
			draw_rect(Rect2(disc + Vector2(-R * 0.30, -R * 0.02), Vector2(R * 0.60, R * 0.46)), Kit.TEXT_SOFT); draw_arc(disc + Vector2(0, -R * 0.02), R * 0.22, PI, TAU, 10, Kit.TEXT_SOFT, 1.8, true)
		else:
			draw_string(Fonts.bold, disc + Vector2(-R, float(fz) * 0.36), str(idx + 1), HORIZONTAL_ALIGNMENT_CENTER, R * 2.0, fz, Kit.exc())
		if has_focus(): draw_style_box(Kit.ring_sb(), Rect2(Vector2.ZERO, size))

## The anatomy of the chapter with the task pins on it. Each chapter scene draws its organs, returns {task id: [anchor, label side]}, and the pins are placed on them.
class MapCanvas extends Control:
	var screen: TasksScreen
	var chapter := ""
	var nodes: Array = []
	var cards: Array = []
	var _t := 0.35
	func _ready() -> void: clip_contents = true
	func min_text_px() -> float: return float(Kit.fs(14)) * Kit.ppl
	func first_card() -> Control:
		for c in cards:
			if bool(c.node["installed"]) and Campaign.unlocked(str(c.node["id"])): return c
		return cards[0] if not cards.is_empty() else null
	func set_chapter(id: String, ns: Array) -> void:
		chapter = id; nodes = ns
		for c in cards:
			remove_child(c); c.queue_free()
		cards = []
		for i in ns.size():
			var n: Dictionary = ns[i]
			var c := NodeCard.new(); c.node = n; c.idx = i; c.screen = screen; c.pressed.connect(func(): screen.select_node(str(n["id"])))
			c.tooltip_text = str(n["title"]); add_child(c); cards.append(c)
		queue_redraw()
	func _notification(what: int) -> void:
		if what == NOTIFICATION_RESIZED: queue_redraw()
	func _process(dt: float) -> void:
		if Kit.motion() and is_visible_in_tree():
			_t += dt; queue_redraw()
			for c in cards: c.queue_redraw()

	func _draw() -> void:
		var r := Rect2(Vector2.ZERO, size)
		draw_rect(r, Color("#0a1423"))
		var a := 1.0 if Campaign.chapter_open(chapter) else 0.5
		var anch := _scene(chapter, r.grow(-10.0), a)
		var fallback := 0
		for c in cards:
			var id := str(c.node["id"]); var pa: Array
			if anch.has(id): pa = anch[id]
			else:
				var p: Vector2 = c.node["pos"]; pa = [Vector2(p.x * size.x, p.y * size.y), "r"]; fallback += 1
			c.place(pa[0], str(pa[1]), r, float(pa[2]) if pa.size() > 2 else 0.0)
		# the path through the chapter: a thin line from pin to pin, bright once the earlier task is passed
		for i in range(1, cards.size()):
			var pc = cards[i - 1]; var cc = cards[i]
			if not (bool(pc.node["installed"]) and bool(Progress.best(str(pc.node["id"]))["passed"])): continue
			var p0: Vector2 = pc.position + pc.disc; var p1: Vector2 = cc.position + cc.disc
			var mid := (p0 + p1) * 0.5 + (p1 - p0).orthogonal().normalized() * minf(40.0, (p1 - p0).length() * 0.12)
			var pts := PackedVector2Array()
			for k in 21:
				var u := float(k) / 20.0; pts.append(p0.lerp(mid, u).lerp(mid.lerp(p1, u), u))
			Gfx.dashed(self, pts, Color(Kit.GREEN, 0.8), 2.4, 9.0, 8.0, _t * 18.0)
		var cd := Campaign.chapter_def(chapter); var st := Campaign.star_total(chapter); var fz := Kit.fs(14)
		var cap := ("%s   %d of %d stars" % [str(cd["title"]).to_upper(), st.x, st.y]) if st.y > 0 else str(cd["title"]).to_upper()
		Anatomy.tag(self, cap, Vector2(14, size.y - 12), Kit.TEXT_SOFT, 14, HORIZONTAL_ALIGNMENT_LEFT, true, 0.6)
		if not Campaign.chapter_open(chapter):
			var first_id := str(Campaign.PLANNED[chapter][0][0]) if Campaign.PLANNED.has(chapter) else ""
			Anatomy.tag(self, "Locked. " + Campaign.lock_reason(first_id) + ".", Vector2(14, 10.0 + float(fz)), Kit.RED, 14, HORIZONTAL_ALIGNMENT_LEFT, false, 0.7)

	func _scene(id: String, r: Rect2, a: float) -> Dictionary:
		match str(Campaign.chapter_def(id)["organ"]):
			"worm": return _sc_invert(r, a)
			"spine": return _sc_spine(r, a)
			"heart": return _sc_heart(r, a)
			"lungs": return _sc_lungs(r, a)
			"autonomic": return _sc_auto(r, a)
		return {}

	func _lab(s: String, p: Vector2, col := Kit.TEXT_SOFT, align := HORIZONTAL_ALIGNMENT_LEFT) -> void:
		if Layout.phone: return
		Anatomy.tag(self, s, p, Color(col, 0.95), 14, align, false, 0.55)

	# ------------------------------------------------------------ heart
	func _sc_heart(r: Rect2, a: float) -> Dictionary:
		var w := r.size.x; var h := r.size.y; var m := minf(w, h)
		var hr := Rect2(r.position.x + w * 0.31, r.position.y + h * 0.05, w * 0.50, h * 0.88)
		var res := Anatomy.heart(self, hr, {"tms": fposmod(_t, 0.95) * 1000.0, "alpha": a, "labels": not Layout.phone, "vagus": 0.55, "symp": 0.45, "t": _t})
		var M := r.position + Vector2(w * 0.135, h * 0.24); var wm := m * 0.105
		var med := Anatomy.medulla(self, M, wm, {"hi": ["nts", "dmv", "amb"], "alpha": a, "t": _t, "act": 0.4})
		_lab("MEDULLA", M + Vector2(0, -wm * 0.95), Kit.TEXT_SOFT, HORIZONTAL_ALIGNMENT_CENTER)
		# vagus: medulla -> heart (cyan)
		var vpath := Gfx.cr_spline(PackedVector2Array([med["amb_r"], M + Vector2(wm * 0.98, wm * 0.05), M + Vector2(wm * 1.9, wm * 0.5), (M + res["vagus_in"] as Vector2) * 0.5 + Vector2(0, wm * 0.2), res["vagus_in"]]), 8)
		Anatomy.nerve(self, vpath, Anatomy.para(), maxf(m * 0.006, 2.0), 0.55, _t, a)
		# spinal cord with the sympathetic chain, and the cardiac nerves (amber)
		var cx0 := M.x - wm * 0.15; var cy0 := M.y + wm * 0.72; var cy1 := r.position.y + h * 0.90
		var cord := PackedVector2Array([Vector2(cx0, cy0), Vector2(cx0, cy1)])
		Anatomy.nerve(self, cord, Anatomy.NUC, maxf(m * 0.012, 3.0), 0.0, _t, a * 0.7)
		var chain_x := cx0 + wm * 0.36
		var chain := PackedVector2Array([Vector2(chain_x, cy0 + wm * 0.2), Vector2(chain_x, cy0 + h * 0.46)])
		self.draw_polyline(chain, Color(Anatomy.symp(), 0.35 * a), 1.8, true)
		for i in 9: self.draw_circle(Vector2(chain_x, lerpf(chain[0].y, chain[1].y, float(i) / 8.0)), 3.6, Color(Anatomy.symp(), 0.9 * a))
		var sym_y := cy0 + h * 0.22
		var spath := Gfx.cr_spline(PackedVector2Array([Vector2(chain_x, sym_y), Vector2(chain_x + (res["symp_in"].x - chain_x) * 0.5, sym_y - h * 0.01), res["symp_in"]]), 8)
		Anatomy.nerve(self, spath, Anatomy.symp(), maxf(m * 0.006, 2.0), 0.45, _t, a)
		# baroreceptor afferents (green): carotid sinus and aortic arch -> NTS
		var ct: Vector2 = res["carotid_top"]; var nts: Vector2 = med["nts_r"]
		var bpath := Gfx.cr_spline(PackedVector2Array([res["carotid"], ct + Vector2(0, -1.0), Vector2(lerpf(ct.x, M.x, 0.6), r.position.y + h * 0.04), M + Vector2(wm * 0.7, -wm * 0.85), nts]), 8)
		Anatomy.nerve(self, bpath, Kit.GREEN, maxf(m * 0.005, 1.8), 0.5, _t, a)
		self.draw_circle(res["carotid"], m * 0.014, Color(Kit.GREEN, 0.25 * a)); self.draw_arc(res["carotid"], m * 0.014, 0, TAU, 14, Color(Kit.GREEN, 0.9 * a), 1.6, true)
		var cp: Vector2 = res["carotid"]
		_lab("carotid sinus", cp + Vector2(m * 0.04, -m * 0.045), Kit.GREEN)
		_lab("aortic arch", (res["arch"] as Vector2) + Vector2(m * 0.03, -m * 0.045))
		_lab("vagus (cyan)", along_pt(vpath, 0.28) + Vector2(wm * 0.1, -wm * 0.28), Anatomy.para())
		_lab("sympathetic", Vector2(chain_x + 10.0, sym_y + wm * 0.8), Anatomy.symp())
		_lab("SVC", (res["svc_top"] as Vector2) + Vector2(-m * 0.045, m * 0.11), Kit.TEXT_SOFT, HORIZONTAL_ALIGNMENT_RIGHT)
		_lab("aorta", (res["aorta_exit"] as Vector2) + Vector2(-m * 0.02, m * 0.07), Kit.TEXT_SOFT, HORIZONTAL_ALIGNMENT_RIGHT)
		_lab("IVC", (res["ivc_bottom"] as Vector2) + Vector2(-m * 0.045, -m * 0.07), Kit.TEXT_SOFT, HORIZONTAL_ALIGNMENT_RIGHT)
		var sa: Vector2 = res["sa"]; var av: Vector2 = res["av"]
		_lab("AV node", av + Vector2(m * 0.05, m * 0.075), Color("#ffd27a"))
		return {"heart_pacemaker": [sa, "l"], "heart_autonomic": [along_pt(vpath, 0.72), "u", 12.0], "baroreflex": [res["carotid"], "r"]}

	static func along_pt(p: PackedVector2Array, u: float) -> Vector2: return Anatomy.along(p, u)

	# ------------------------------------------------------------ lungs
	func _sc_lungs(r: Rect2, a: float) -> Dictionary:
		var w := r.size.x; var h := r.size.y; var m := minf(w, h)
		var phase := _t * 0.30; var vol := 0.5 - 0.5 * cos(TAU * phase)
		var burst := clampf(sin(TAU * phase) * 1.6 + 0.2, 0.0, 1.0) if cos(TAU * phase) < 0.99 else 0.0   # the phrenic burst while the volume rises
		var lr := Rect2(r.position.x + w * 0.17, r.position.y + h * 0.045, w * 0.50, h * 0.93)
		var res := Anatomy.lungs(self, lr, {"vol": vol, "alpha": a, "t": _t})
		var M := r.position + Vector2(w * 0.805, h * 0.30); var wm := m * 0.165
		var med := Anatomy.medulla(self, M, wm, {"hi": ["prebotc", "rtn"], "alpha": a, "t": _t, "act": burst, "labels": not Layout.phone})
		_lab("MEDULLA, transverse section", M + Vector2(0, -wm * 0.98), Kit.TEXT_SOFT, HORIZONTAL_ALIGNMENT_CENTER)
		var s: float = res["scale"]; var o: Vector2 = res["origin"]
		var c4: Vector2 = o + Vector2(0.545, 0.035) * s
		# bulbospinal axons (preBötC -> phrenic motor nucleus in C3-C5), then the phrenic nerves to the diaphragm
		var bs := Gfx.cr_spline(PackedVector2Array([med["prebotc"], M + Vector2(-wm * 0.5, wm * 0.45), M + Vector2(-wm * 1.4, wm * 1.0), (M + c4) * 0.5 + Vector2(0, wm * 0.6), c4]), 8)
		Anatomy.nerve(self, bs, Anatomy.symp(), maxf(m * 0.005, 2.0), burst, _t, a)
		self.draw_circle(c4, m * 0.011, Color(Anatomy.NUC, 0.95 * a)); Gfx.glow(self, c4, m * 0.04, Color(Anatomy.NUC, 0.3 * a))
		for sd in [-1.0, 1.0]:
			var pn := Gfx.cr_spline(PackedVector2Array([c4, o + Vector2(0.5 + 0.045 * sd, 0.18) * s, o + Vector2(0.5 + 0.052 * sd, 0.45) * s, o + Vector2(0.5 + 0.075 * sd, 0.72) * s, res["dome_left" if sd > 0 else "dome_right"]]), 8)
			Anatomy.nerve(self, pn, Anatomy.para().lerp(Gfx.VIOLET, 0.5), maxf(m * 0.004, 1.8), burst, _t, a)
		_lab("C3 to C5", c4 + Vector2(m * 0.03, -m * 0.01), Anatomy.NUC)
		_lab("phrenic nerve", o + Vector2(0.60, 0.52) * s, Gfx.VIOLET)
		_lab("diaphragm", (res["dome_left"] as Vector2) + Vector2(m * 0.02, m * 0.09), Gfx.VIOLET)
		_lab("bronchial tree", (res["hilum_r"] as Vector2) + Vector2(-m * 0.28, m * 0.12), Kit.CYAN)
		_lab("trachea", (res["trachea_top"] as Vector2) + Vector2(-m * 0.03, m * 0.03), Kit.TEXT_SOFT, HORIZONTAL_ALIGNMENT_RIGHT)
		return {"breathing": [med["prebotc_r"], "d", 40.0]}

	# ------------------------------------------------------------ spine
	func _sc_spine(r: Rect2, a: float) -> Dictionary:
		var w := r.size.x; var h := r.size.y; var m := minf(w, h)
		var top := Vector2(r.position.x + w * 0.075, r.position.y + h * 0.045)
		var sp := Anatomy.spine(self, top, h * 0.90, {"alpha": a, "t": _t, "act": 0.4})
		_lab("cervical", top + Vector2(m * 0.07, h * 0.03), Kit.TEXT_SOFT); _lab("thoracic", top + Vector2(m * 0.09, h * 0.30), Kit.TEXT_SOFT); _lab("lumbar", top + Vector2(m * 0.09, h * 0.66), Kit.TEXT_SOFT)
		var C := r.position + Vector2(w * 0.40, h * 0.54); var wc := minf(w * 0.165, h * 0.30)
		var cs := Anatomy.cord_section(self, C, wc, {"alpha": a, "t": _t, "act": 0.55, "hi": ["mn", "renshaw"], "labels": not Layout.phone})
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
		var limb := Anatomy.limb(self, sh, el, hd, {"alpha": a, "t": _t, "ag": 0.40 + 0.3 * swing, "ant": 0.25 - 0.15 * swing, "ia_ag": 0.5 + 0.4 * swing, "ia_ant": 0.15})
		# the reflex arc closes through the arm: Ia afferent from the spindle to the dorsal root, motor axon from the ventral root to the muscle
		var aend: Vector2 = limb["ag_aff"]; var dre: Vector2 = cs["dorsal_root_end"]
		var aff := Anatomy.bez2(aend, aend.lerp(dre, 0.5) + Vector2(0, -h * 0.07), dre, 24)
		Anatomy.nerve(self, aff, Kit.GREEN, maxf(m * 0.004, 1.8), 0.6, _t, a)
		var ag_path: PackedVector2Array = limb["ag_path"]
		var tgt: Vector2 = Anatomy.along(ag_path, 0.30); var vre: Vector2 = cs["ventral_root_end"]
		var eff := Anatomy.bez2(vre, vre.lerp(tgt, 0.5) + Vector2(w * 0.04, h * 0.05), tgt, 24)
		Anatomy.nerve(self, eff, Gfx.VIOLET, maxf(m * 0.005, 2.0), 0.6, _t, a)
		_lab("biceps (agonist)", (limb["ag"] as Vector2) + Vector2(-m * 0.02, -m * 0.10), Kit.exc(), HORIZONTAL_ALIGNMENT_CENTER)
		_lab("triceps", (limb["ant"] as Vector2) + Vector2(m * 0.13, m * 0.05), Kit.inh(), HORIZONTAL_ALIGNMENT_CENTER)
		_lab("muscle spindle", aend + Vector2(0, -m * 0.02), Kit.GREEN, HORIZONTAL_ALIGNMENT_CENTER)
		return {"stretch_reflex": [cs["mn"], "r", 6.0], "renshaw": [cs["renshaw"], "d", 14.0], "posture": [el, "r", 8.0], "reach": [hd, "u", 8.0]}

	# ------------------------------------------------------------ autonomic
	func _sc_auto(r: Rect2, a: float) -> Dictionary:
		var w := r.size.x; var h := r.size.y; var m := minf(w, h)
		var sy := 0.5 + 0.35 * sin(_t * 0.5); var pa := 0.5 - 0.35 * sin(_t * 0.5)
		var bs := h * 0.955 / 0.765; var br := Rect2(r.position.x + w * 0.5 - bs * 0.45, r.position.y + h * 0.015, bs * 0.9, bs)
		var res := Anatomy.body(self, br, {"alpha": a, "t": _t, "sy": sy, "pa": pa, "pupil": 0.5 + 0.4 * sin(_t * 0.5), "tms": fposmod(_t, 0.95) * 1000.0})
		var s: float = res["scale"]; var o: Vector2 = res["origin"]
		# the legend: two divisions, one body
		var lx := r.position.x + 18.0; var ly := r.position.y + h * 0.04
		if not Layout.phone:
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
			Anatomy.tag(self, str(q[0]), Vector2(rx, ap.y + 5.0), q[2], 14, HORIZONTAL_ALIGNMENT_LEFT, false, 0.55)
		_lab("sympathetic chain", P.call(-0.075, 0.54), Anatomy.symp(), HORIZONTAL_ALIGNMENT_RIGHT)
		return {"pupil": [(res["eye"] as Vector2) + Vector2(-s * 0.045, -s * 0.012), "l", 6.0], "peristalsis": [res["gut"], "r", 6.0], "fight_or_flight": [res["adrenal"], "l", 6.0]}

	# ------------------------------------------------------------ invertebrates
	func _sc_invert(r: Rect2, a: float) -> Dictionary:
		var w := r.size.x; var h := r.size.y; var m := minf(w, h)
		# salt gradient behind the salt-seeking worm
		var salt := r.position + Vector2(w * 0.80, h * 0.20)
		for k in 5: Gfx.glow(self, salt, m * (0.55 - 0.09 * float(k)), Color("#5fd9c0", 0.05 * a))
		_lab("NaCl gradient", salt + Vector2(0, m * 0.07), Color("#5fd9c0"), HORIZONTAL_ALIGNMENT_CENTER)
		# the nematode that is touched (left, lower)
		var p1 := PackedVector2Array()
		for i in 40:
			var u := float(i) / 39.0; var th := 3.0 * PI * u
			p1.append(r.position + Vector2(w * (0.07 + 0.26 * u), h * (0.70 + 0.055 * sin(th + _t * 0.5))))
		var wr1 := Anatomy.worm(self, Gfx.cr_spline(p1, 3), h * 0.034, {"alpha": a, "t": _t})
		var probe: Vector2 = wr1["head"]
		self.draw_line(probe + Vector2(-m * 0.06, -m * 0.10), probe + Vector2(-m * 0.012, -m * 0.012), Color(Kit.TEXT, 0.8 * a), 3.0, true)
		Gfx.glow(self, probe + Vector2(-m * 0.012, -m * 0.012), m * 0.04, Color(Kit.RED, 0.45 * a))
		# the worm crawling up the salt gradient (centre, top)
		var p2 := PackedVector2Array()
		for i in 44:
			var u := float(i) / 43.0
			p2.append(r.position + Vector2(w * (0.36 + 0.34 * u), h * (0.46 - 0.18 * u + 0.05 * sin(u * 9.0 + _t * 0.7))))
		var wr2 := Anatomy.worm(self, Gfx.cr_spline(p2, 3), h * 0.034, {"alpha": a, "t": _t + 1.0})
		_lab("ASE salt neurons", (wr2["tail"] as Vector2) + Vector2(m * 0.02, m * 0.07), Kit.exc())
		# the lamprey (right, lower)
		var p3 := PackedVector2Array()
		for i in 48:
			var u := float(i) / 47.0; var wv := 0.045 * sin(u * 7.0 - _t * 3.0) * u
			p3.append(r.position + Vector2(w * (0.52 + 0.43 * u), h * (0.82 + wv * 2.2)))
		var lm := Anatomy.lamprey(self, p3, h * 0.040, {"alpha": a, "t": _t})
		_lab("spinal cord CPG", (lm["mid"] as Vector2) + Vector2(0, -m * 0.09), Kit.exc(), HORIZONTAL_ALIGNMENT_CENTER)
		return {"reflex": [wr1["ring"], "u", 20.0], "worm": [wr2["head"], "u", 8.0], "lamprey": [lm["mid"], "d", 6.0]}
