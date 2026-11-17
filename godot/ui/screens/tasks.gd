class_name TasksScreen
extends Screen
## The campaign map. Five chapters (Invertebrates, Spinal cord, Heart, Breathing, Autonomic), each drawn as its organ with the chapter's levels as pins on it:
## a SPINE of micro-puzzles (small pins), tasks and a boss (large pin) strung along the anatomy in play order, and optional BRANCHES (challenge variants,
## diagnosis cases) hanging off their task. Hover or select a pin to see its concepts. Pick a level to read its brief and press BUILD (an empty bench with the
## task's sensors and motors) or REFERENCE (the reference circuit already wired). Chapters, kinds and order come from Campaign (Task::chapter(), level_kind(),
## on_spine()); levels the engine does not have yet show as dashed "not installed" ghosts. Header: the difficulty mode (Guided / Standard / Researcher, saved,
## switchable any time), NOTEBOOK, PARTS BIN and SANDBOX.
const SHORT := {"invertebrates": "INVERT.", "spinal": "SPINE", "heart": "HEART", "breathing": "LUNGS", "autonomic": "AUTONOMIC"}
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
	var head := HBoxContainer.new(); head.add_theme_constant_override("separation", 14 if not phone else 8); v.add_child(head)
	var bs := int(24 * f) if not phone else 18
	back_btn = Kit.btn("BACK", func(): app.back(), "panel", bs, Vector2(120 if not phone else 70, 0)); head.add_child(back_btn)
	var tt := Kit.shout("Campaign", int(50 * f) if not phone else 26, Kit.TEXT); tt.size_flags_horizontal = Control.SIZE_EXPAND_FILL; tt.size_flags_vertical = Control.SIZE_SHRINK_CENTER; head.add_child(tt)
	mode_step = Kit.stepper([["guided", "Guided"], ["standard", "Standard"], ["researcher", "Researcher"]], Progress.mode(), func(id): _on_mode(str(id)), 170.0 if not phone else 112.0, int(24 * f) if not phone else 15, 44.0 if not phone else 32.0)
	mode_step.tooltip_text = "Difficulty. Switch any time; it only changes the star labels and how tight the tasks are."
	head.add_child(mode_step)
	head.add_child(Kit.btn("NOTEBOOK" if not phone else "NOTES", func(): app.show_screen("notebook"), "panel", bs))
	head.add_child(Kit.btn("PARTS BIN" if not phone else "PARTS", func(): app.show_screen("parts"), "cyan", bs))
	head.add_child(Kit.btn("SANDBOX" if not phone else "LAB", func(): app.open_lab(""), "panel", bs))
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

func _tabs(f: float, phone: bool) -> Control:
	var h := HBoxContainer.new(); h.add_theme_constant_override("separation", 8 if not phone else 4)
	for c in Campaign.CHAPTERS:
		var id: String = c["id"]; var open := Campaign.chapter_open(id); var n := Campaign.installed_count(id)
		var st := Campaign.star_total(id)
		var txt := (str(SHORT[id]) if phone else str(c["title"]).to_upper())
		if not open and not phone: txt += "  (locked)"
		var b := Kit.btn(txt, func(): _show_chapter(id), "amber" if id == chapter else "panel", int(22 * f) if not phone else 18, Vector2(0, 0))
		b.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		b.tooltip_text = ("%s: %d of %d levels passed, %d of %d stars" % [c["title"], Campaign.passed_count(id), n, st.x, st.y]) if n > 0 else ("%s: no tasks installed yet" % c["title"])
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
	sel = id
	if app != null: app.campaign_node = id
	map.queue_redraw()
	if map.overlay != null: map.overlay.queue_redraw()
	for c in map.cards: c.queue_redraw()
	_fill_detail()

func _chips_row(label: String, items: Array, col: Color, f: float, phone: bool, soft := false) -> void:
	if items.is_empty(): return
	var row := HFlowContainer.new(); row.add_theme_constant_override("h_separation", 8); row.add_theme_constant_override("v_separation", 6); detail_box.add_child(row)
	var kl := Kit.label(label, int(15 * f) if not phone else 15, Kit.TEXT_SOFT); kl.custom_minimum_size.x = 112 * f if not phone else 100; kl.size_flags_vertical = Control.SIZE_SHRINK_CENTER; row.add_child(kl)
	for it in items:
		var c := Kit.chip(str(it), col, 15)
		if soft:
			(c.get_child(0) as Label).label_settings = Kit.ls(15, Kit.TEXT, true)
			c.add_theme_stylebox_override("panel", Kit.sb(Kit.PANEL_HI, Kit.inh(), 1, 6, 8, false))
		row.add_child(c)

## The three star conditions, worded for the difficulty mode (Progress.stars_for decides what earns them; the mode only changes how the pass is judged).
func _star_tiers(par: int) -> Array:
	var m := Progress.mode()
	var t1 := "Pass the task"; var t2 := "Pass on 5 random seeds (TEST x5)"
	if m == "guided": t1 = "Pass the task (wide bands, hints free)"
	elif m == "researcher": t1 = "Pass on all 5 seeds (required)"; t2 = "Pass on 5 seeds (physiological noise)"
	return [[t1, 1], [t2, 2], ["Pass with %d neurons or fewer" % par if par > 0 else "Pass on 5 seeds, small circuit", 3]]

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
	var tid := str(node["task"]); var kind := str(node["kind"]); var is_var := kind == "variant"
	var info := Campaign.info(tid) if node["installed"] else {}
	detail_box.add_child(Kit.label("%s  /  %s" % [str(cd["title"]).to_upper(), str(KIND_NAME.get(kind, "TASK"))], int(16 * f) if not phone else 15, Gfx.VIOLET))
	detail_box.add_child(Kit.shout(str(node["title"]), int(36 * f) if not phone else 24, Kit.TEXT))
	if not node["installed"]:
		detail_box.add_child(Kit.label("Not installed in this build yet. It appears here as soon as the engine has the task; nothing to do on your side.", small, Kit.TEXT_SOFT, true))
		detail_box.add_child(Kit.label(str(cd["blurb"]), small, Kit.TEXT_SOFT, true)); return
	var open := Campaign.unlocked(sel); var best: Dictionary = Progress.best(tid); var par := Progress.budget(tid)
	var done := Campaign.passed(sel)
	if is_var: best = {"value": 0.0, "passed": done, "stars": 1 if done else 0, "modes": {}, "hints": {}, "variants": []}
	var chips := HFlowContainer.new(); chips.add_theme_constant_override("h_separation", 8); chips.add_theme_constant_override("v_separation", 6); detail_box.add_child(chips)
	chips.add_child(Kit.chip("%d s episode" % int(round(float(info["duration_ms"]) / 1000.0)), Kit.PANEL_HI, 15)); (chips.get_child(0).get_child(0) as Label).label_settings = Kit.ls(15, Kit.TEXT, true)
	if not is_var: chips.add_child(Kit.chip("BEST %d" % int(round(float(best["value"]))) if float(best["value"]) > 0.0 else "NOT TRIED", Kit.GREEN if bool(best["passed"]) else Kit.exc(), 15))
	if done: chips.add_child(Kit.chip("PASSED" if not is_var else "DONE", Kit.GREEN, 15))
	if not open: chips.add_child(Kit.chip("LOCKED", Kit.RED, 15))
	if not bool(node["spine"]): chips.add_child(Kit.chip("OPTIONAL", Kit.inh(), 15))
	if str(info["view"]) != "polyline": chips.add_child(Kit.chip(str(info["view"]).to_upper() + " VIEW", Gfx.VIOLET, 15))
	var bt := HBoxContainer.new(); bt.add_theme_constant_override("separation", 10); detail_box.add_child(bt)
	var sid := tid; var vid := str(node["variant"]); var synth := bool(info.get("synthetic", false))
	var go := func(ref: bool):
		Campaign.pending_variant = vid; Playlog.level_start(sid, vid); app.open_lab(sid, ref)
	var b := Kit.btn("BUILD" if open else "LOCKED", func(): if open and not synth: go.call(false), "amber" if open else "panel", int(24 * f) if not phone else 19); b.size_flags_horizontal = Control.SIZE_EXPAND_FILL; bt.add_child(b)
	if not open or synth: b.disabled = true
	if not is_var:
		var rb := Kit.btn("REFERENCE", func(): if not synth: go.call(true), "cyan", int(24 * f) if not phone else 19); rb.size_flags_horizontal = Control.SIZE_EXPAND_FILL; rb.disabled = synth; bt.add_child(rb)
	if not open: detail_box.add_child(Kit.label(Campaign.lock_reason(sel) + ".", small, Kit.RED, true))
	if is_var:
		detail_box.add_child(Kit.label("A challenge variant of %s. It never gates the spine." % str(info["title"]), small, Kit.TEXT_SOFT, true))
	else:
		# stars, worded for the difficulty mode, lit from this mode's best
		var m := Progress.mode(); var bm := Progress.best_in(tid, m)
		var sh := HBoxContainer.new(); sh.add_theme_constant_override("separation", 8); detail_box.add_child(sh)
		sh.add_child(Kit.label("STARS", int(15 * f) if not phone else 15, Kit.TEXT_SOFT))
		sh.add_child(Kit.chip(Progress.mode_name(m).to_upper() + " MODE", Kit.exc(), 15))
		for t in _star_tiers(par):
			var r := HBoxContainer.new(); r.add_theme_constant_override("separation", 10); detail_box.add_child(r)
			var st := _Star.new(); st.lit = int(bm["stars"]) >= int(t[1]); st.custom_minimum_size = Vector2(26, 26); r.add_child(st)
			var l := Kit.label(str(t[0]), int(17 * f) if not phone else 15, Kit.TEXT if st.lit else Kit.TEXT_SOFT, true); l.size_flags_vertical = Control.SIZE_SHRINK_CENTER; l.size_flags_horizontal = Control.SIZE_EXPAND_FILL; r.add_child(l)
		var per := []
		for mm in Progress.MODES:
			var br := Progress.best_in(tid, mm)
			per.append("%s: %s" % [Progress.mode_name(mm), ("%d of 3 stars" % int(br["stars"])) if bool(br["passed"]) else "not yet"])
		detail_box.add_child(Kit.label("   ".join(per), 15, Kit.TEXT_SOFT, true))
	var brief := str(info["brief"])
	detail_box.add_child(Kit.label(_lead(brief, 420) if not phone else _lead(brief, 220), small, Kit.TEXT_SOFT, true))
	if str(info.get("symptoms", "")) != "": detail_box.add_child(Kit.label("Patient: " + str(info["symptoms"]), small, Kit.TEXT, true))
	_chips_row("NEW IDEA", (node["concepts_new"] as Array).map(func(c): return Campaign.concept_short(str(c))), Kit.exc(), f, phone)
	_chips_row("USES", (node["concepts_used"] as Array).map(func(c): return Campaign.concept_short(str(c))), Kit.inh(), f, phone, true)
	if not is_var:
		var hu := Progress.hints_used(tid)
		if int(hu["count"]) > 0: detail_box.add_child(Kit.label("Hints used: %d (highest tier %d). Recorded, never punished." % [int(hu["count"]), int(hu["max_tier"])], 15, Kit.TEXT_SOFT, true))
		var vs: Dictionary = info["variants"]
		if not vs.is_empty():
			var dn := Progress.variants_done(tid); var k := 0
			for vk in vs: k += 1 if dn.has(vk) else 0
			detail_box.add_child(Kit.label("Challenge variants: %d of %d done" % [k, vs.size()], 15, Kit.TEXT_SOFT, true))
	for pair in [["SENSORS", info["sensors"], Kit.GREEN], ["MOTORS", info["motors"], Gfx.VIOLET], ["MODULATORS", info["modulators"], Kit.exc()]]:
		_chips_row(pair[0], pair[1], pair[2], f, phone)

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

## One level on the map: a pin on the anatomy. Tasks and bosses carry a small label plate (title, stars); micro-puzzles (small pin), variants (diamond) and
## diagnosis cases (square) are bare pins, and their name and concept chips show on hover or selection (MapCanvas overlay). Bosses are large pins.
## The detail (brief, lock reason, best score) lives in the side panel.
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
	func kind() -> String: return str(node.get("kind", "task"))
	func has_plate() -> bool: return kind() == "task" or kind() == "boss"
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
	## The part that is clickable: the plate and the pin, with a touch-sized pad around a bare pin.
	func _has_point(p: Vector2) -> bool:
		if not has_plate():
			var r := maxf(_R() + 4.0, Layout.tap() * 0.5); return p.distance_to(disc) <= r
		return Rect2(Vector2.ZERO, size).grow(4.0).has_point(p)
	## The pin's footprint in map coordinates (what the overlay keeps clear).
	func pin_rect() -> Rect2: return Rect2(position, size) if has_plate() else Rect2(position + disc - Vector2(_R(), _R()), Vector2(_R(), _R()) * 2.0)
	func _draw() -> void:
		var id := str(node["id"]); var inst: bool = node["installed"]; var st := _status(); var best: Dictionary = st[2]
		var open := inst and Campaign.unlocked(id); var passed := bool(best["passed"]); var selected := screen != null and screen.sel == id
		var edge := Kit.GREEN if passed else (Kit.exc() if open else Kit.LINE.lightened(0.35))
		var hov := is_hovered(); var R := _R(); var k := kind()
		var ft := Kit.fs(16 if not Layout.phone else 15); var fz := Kit.fs(14)
		if has_plate():
			draw_style_box(Kit.sb(Color(Kit.BENCH, 0.86 if not hov else 0.95), Color(edge, 0.85 if (selected or hov) else 0.45), 2 if selected else 1, 9, 0, false), _plate)
			var tcol := Kit.TEXT if open else Kit.TEXT_SOFT
			var tx := _plate.position.x + 9.0; var ty := _plate.position.y + 6.0 + float(ft) * 0.86
			draw_string(Fonts.bold, Vector2(tx, ty), str(node["title"]), HORIZONTAL_ALIGNMENT_LEFT, _plate.size.x - 14.0, ft, tcol)
			var y2 := ty + 7.0
			for j in 3: ScoreCard.draw_star_on(self, Vector2(tx + 8.0 + 18.0 * j, y2 + float(fz) * 0.5), 7.0, Kit.AMBER if j < int(best["stars"]) else Color(Kit.LINE, 0.95))
			if not Layout.phone: draw_string(Fonts.regular, Vector2(tx + 56.0, y2 + float(fz) * 0.86), str(st[0]), HORIZONTAL_ALIGNMENT_LEFT, _plate.size.x - 64.0, fz, st[1])
			if gap > 0.0:
				var q := Vector2(clampf(disc.x, _plate.position.x, _plate.end.x), clampf(disc.y, _plate.position.y, _plate.end.y))
				draw_line(disc + (q - disc).normalized() * R, q, Color(edge, 0.7), 1.6, true)
		if open: Gfx.glow(self, disc, R * (3.4 if hov else 2.6), Color(edge, 0.30 if hov else 0.20))
		if selected:
			var pu := 0.5 + 0.5 * sin(Time.get_ticks_msec() * 0.004) if Kit.motion() else 1.0
			if k == "variant": draw_polyline(_diamond(disc, R + 6.0 + 2.0 * pu), Color(1, 1, 1, 0.55 + 0.4 * pu), 2.0, true)
			else: draw_arc(disc, R + 6.0 + 2.0 * pu, 0, TAU, 40, Color(1, 1, 1, 0.55 + 0.4 * pu), 2.0, true)
		var inner := Color("#0a1424", 0.96); var lw := 3.0 if open or passed else 2.0
		match k:
			"variant":
				var dm := _diamond(disc, R)
				draw_colored_polygon(dm, Kit.GREEN if passed else inner); draw_polyline(dm, Color(edge, 0.95), lw, true)
				if passed: draw_polyline(PackedVector2Array([disc + Vector2(-R * 0.4, 0), disc + Vector2(-R * 0.08, R * 0.32), disc + Vector2(R * 0.42, -R * 0.3)]), Kit.BENCH, 2.0, true)
			"diagnosis":
				var rr := Rect2(disc - Vector2(R, R), Vector2(R, R) * 2.0)
				draw_style_box(Kit.sb(Kit.GREEN if passed else inner, Color(edge, 0.95), int(lw), 4, 0, false), rr)
				var cc := Kit.BENCH if passed else edge
				draw_line(disc + Vector2(-R * 0.5, 0), disc + Vector2(R * 0.5, 0), cc, 2.4, true); draw_line(disc + Vector2(0, -R * 0.5), disc + Vector2(0, R * 0.5), cc, 2.4, true)
			_:
				draw_circle(disc, R, inner)
				if k == "boss": draw_arc(disc, R + 5.0, 0, TAU, 48, Color(edge, 0.55), 2.0, true)
				draw_arc(disc, R, 0, TAU, 40, Color(edge, 0.95), lw, true)
				if not inst:
					Gfx.dashed(self, PackedVector2Array([disc + Vector2(-R * 0.5, 0), disc + Vector2(R * 0.5, 0)]), Color(Kit.TEXT_SOFT, 0.6), 1.6, 3.0, 3.0)
				elif passed:
					draw_circle(disc, R - 3.5, Kit.GREEN)
					if k == "boss": ScoreCard.draw_star_on(self, disc, R * 0.62, Kit.BENCH)
					else: draw_polyline(PackedVector2Array([disc + Vector2(-R * 0.35, 0), disc + Vector2(-R * 0.08, R * 0.28), disc + Vector2(R * 0.40, -R * 0.26)]), Kit.BENCH, 2.6 if k == "task" else 2.0, true)
				elif not open:
					if k == "micro": draw_circle(disc, R * 0.3, Color(Kit.TEXT_SOFT, 0.7))
					else:
						draw_rect(Rect2(disc + Vector2(-R * 0.30, -R * 0.02), Vector2(R * 0.60, R * 0.46)), Kit.TEXT_SOFT); draw_arc(disc + Vector2(0, -R * 0.02), R * 0.22, PI, TAU, 10, Kit.TEXT_SOFT, 1.8, true)
				elif k == "boss": ScoreCard.draw_star_on(self, disc, R * 0.6, Kit.exc())
				elif k == "micro": draw_circle(disc, R * 0.42, Kit.exc())
				else: draw_string(Fonts.bold, disc + Vector2(-R, float(fz) * 0.36), str(idx + 1), HORIZONTAL_ALIGNMENT_CENTER, R * 2.0, fz, Kit.exc())
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
		for c in card.node["concepts_new"]: chips.append([Campaign.concept_short(str(c)), true])
		for c in card.node["concepts_used"]: chips.append([Campaign.concept_short(str(c)), false])
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
		var pr := card.pin_rect(); var bx := clampf(pr.get_center().x - (bw + 12.0) * 0.5, 4.0, maxf(size.x - bw - 16.0, 4.0))
		var by := pr.end.y + 6.0
		if by + bh > size.y - 4.0: by = pr.position.y - bh - 6.0
		by = clampf(by, 4.0, maxf(size.y - bh - 4.0, 4.0))
		Gfx.rounded_rect(self, Rect2(bx, by, bw + 12.0, bh), Color(Kit.BENCH, 0.94), Color(Kit.LINE, 1.0), 8.0, 1.0)
		var y := by + 6.0
		if bare:
			draw_string(Fonts.bold, Vector2(bx + 6.0, y + float(ft) * 0.86), title, HORIZONTAL_ALIGNMENT_LEFT, bw, ft, Kit.TEXT); y += float(ft) + 6.0
		for ln in lines:
			var x := bx + 6.0
			for ch in ln:
				var w: float = ch[2]; var isnew: bool = ch[1]
				Gfx.rounded_rect(self, Rect2(x, y, w, lh - 2.0), Color(Kit.exc(), 0.9) if isnew else Color(Kit.PANEL_HI, 1.0), Color(Kit.inh(), 0.7) if not isnew else Color(0, 0, 0, 0), 6.0, 1.0)
				draw_string(Fonts.bold if isnew else Fonts.regular, Vector2(x + 8.0, y + float(fz) * 0.9), str(ch[0]), HORIZONTAL_ALIGNMENT_LEFT, w - 8.0, fz, Kit.BENCH if isnew else Kit.TEXT)
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
	func _ready() -> void: clip_contents = true
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
			var c := NodeCard.new(); c.node = n; c.idx = i; c.screen = screen; c.pressed.connect(func(): screen.select_node(str(n["id"])))
			c.tooltip_text = ""; c.mouse_entered.connect(overlay.queue_redraw); c.mouse_exited.connect(overlay.queue_redraw); add_child(c); cards.append(c)
		add_child(overlay); queue_redraw()
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
		var inner := r.grow(-52.0)
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
		draw_rect(r, Color("#0a1423"))
		var a := 1.0 if Campaign.chapter_open(chapter) else 0.5
		var anch := _scene(chapter, r.grow(-10.0), a)
		var sp := cards.filter(func(c): return bool(c.node["spine"]))
		var lay := _spine_layout(sp, anch, r)
		var placed := {}
		for i in sp.size():
			var c = sp[i]; var id := str(c.node["id"]); var side := "r"; var gp := 0.0
			if anch.has(id): side = str((anch[id] as Array)[1]); gp = float((anch[id] as Array)[2]) if (anch[id] as Array).size() > 2 else 0.0
			var ps: Vector2 = lay["pos"][i]
			if not anch.has(id): side = "l" if ps.x > size.x * 0.55 else "r"
			c.place(ps, side, r, gp); placed[id] = c
		# the spine: a line from pin to pin along the anatomy, green once the level before it is passed, amber for the way ahead, faint beyond
		for i in range(sp.size() - 1):
			var sg: Array = lay["seg"][i]; var pc = sp[i]; var nc = sp[i + 1]
			var pts := PackedVector2Array()
			for k in 11: pts.append(_bez(sg[0], sg[1], sg[2], lerpf(float(sg[3]), float(sg[4]), float(k) / 10.0)))
			if bool(pc.node["installed"]) and Campaign.passed(str(pc.node["id"])): Gfx.dashed(self, pts, Color(Kit.GREEN, 0.85), 2.6, 9.0, 8.0, _t * 18.0)
			elif bool(pc.node["installed"]) and Campaign.unlocked(str(pc.node["id"])) and bool(nc.node["installed"]): Gfx.dashed(self, pts, Color(Kit.exc(), 0.55), 2.0, 5.0, 7.0, _t * 10.0)
			else: draw_polyline(pts, Color(Kit.LINE, 0.8), 1.6, true)
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
				for c in by_parent[pid]: c.place(Vector2(size.x * 0.5, size.y * 0.5), "r", r); c.visible = false
				continue
			var pc: NodeCard = placed[pid]; var centre := pc.position + pc.disc
			for c in by_parent[pid]:
				c.visible = true
				var bestp := centre + Vector2(dist, 0); var bests := -1e9
				for k in 16:
					var ang := TAU * float(k) / 16.0 + 0.2
					var d := dist * (1.0 + 0.5 * float(k % 2))
					var q := centre + Vector2(cos(ang), sin(ang)) * (d + pc._R())
					var sc := 0.0
					var clear := 1e9
					for pr in pins: clear = minf(clear, (pr as Rect2).grow(10.0).get_center().distance_to(q) - maxf((pr as Rect2).size.x, (pr as Rect2).size.y) * 0.5)
					for pr in pins:
						if (pr as Rect2).grow(14.0).has_point(q): sc -= 1000.0
					if pc.has_plate() and Rect2(pc.position + pc._plate.position, pc._plate.size).grow(16.0).has_point(q): sc -= 1000.0
					if not r.grow(-18.0).has_point(q): sc -= 500.0
					sc += clear
					if sc > bests: bests = sc; bestp = q
				c.place(bestp, "r", r)
				pins.append(c.pin_rect())
				draw_line(centre, bestp, Color(Kit.GREEN if Campaign.passed(str(c.node["id"])) else Kit.LINE, 0.9), 1.6, true)
		var cd := Campaign.chapter_def(chapter); var st := Campaign.star_total(chapter); var fz := Kit.fs(14)
		var cap := ("%s   %d of %d stars" % [str(cd["title"]).to_upper(), st.x, st.y]) if st.y > 0 else str(cd["title"]).to_upper()
		Anatomy.tag(self, cap, Vector2(14, size.y - 12), Kit.TEXT_SOFT, 14, HORIZONTAL_ALIGNMENT_LEFT, true, 0.6)
		if not Campaign.chapter_open(chapter):
			var g := Campaign.gate_chapter(chapter)
			Anatomy.tag(self, ("Locked. Pass the %s spine first." % str(Campaign.chapter_def(g)["title"])) if g != "" else "Locked.", Vector2(14, 10.0 + float(fz)), Kit.RED, 14, HORIZONTAL_ALIGNMENT_LEFT, false, 0.7)

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
