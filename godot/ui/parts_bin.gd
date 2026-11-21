class_name PartsBin
extends VBoxContainer
## The parts bin: one card per neuron model. Each card has a plain description, biological examples, the key parameters with units, and two thumbnails
## rendered by the engine (an f-I curve and a V_m trace, from catalog/thumbs.json via Catalog). A model is greyed out until its chapter is open (the
## sandbox opens everything) and cannot be picked when this build cannot run it. In "pick" mode (inside the lab) a card has a USE button and preset
## chips; in "browse" mode (the Parts screen) it is a reference.
signal picked(card_id: String)
signal closed
var mode := "browse"
var sandbox := false
var selected := ""
var scroll: ScrollContainer
var grid: Container
var featured: Array = []         # card ids "for this task", shown first under their own heading (pick mode)
var reference: Array = []        # card ids of the models the task's reference circuit uses: pickable even when their chapter is still locked (pick mode)
var _scroll_keep := 0
var _expanded := ""            # phone: the one card shown in full (every other part is a one-line row)

func setup(m: String, sb: bool, current_card := "", for_task: Array = [], ref_cards: Array = []) -> PartsBin:
	Kit.ident(self, "PartsBin"); mode = m; reference = ref_cards.filter(func(id): return Catalog.has_card(id)); sandbox = sb; selected = current_card; featured = for_task.filter(func(id): return Catalog.has_card(id))
	size_flags_vertical = Control.SIZE_EXPAND_FILL; size_flags_horizontal = Control.SIZE_EXPAND_FILL
	add_theme_constant_override("separation", int(10 * Kit.f()))
	_build(); return self

func _build() -> void:
	for c in get_children():
		remove_child(c); c.queue_free()
	var f := Kit.f(); var phone := Layout.phone
	var all := Catalog.cards(); var nuse := 0
	for c in all:
		if Catalog.usable(c["id"], sandbox): nuse += 1
	var note := "%d cell types, %d ready to place. The f-I curve and V_m trace on each card are measured by the engine." % [all.size(), nuse]
	if not Catalog.has_thumbs(): note = "The engine's thumbnail file (catalog/thumbs.json) is not there yet: run build/engine/spike-catalog. Cards show their text only."
	if mode == "pick": note = "Pick the type of the next neuron you add. " + note
	add_child(Kit.label(note, int(17 * f) if not phone else 15, Kit.TEXT_SOFT, true))
	scroll = ScrollContainer.new(); scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL; scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED; add_child(scroll)
	var col := VBoxContainer.new(); col.size_flags_horizontal = Control.SIZE_EXPAND_FILL; col.add_theme_constant_override("separation", int(10 * f)); scroll.add_child(col)
	var order := all.map(func(c): return c["id"]); var rk := {}
	for id in order: rk[id] = _rank(id, order)
	var ids := order.duplicate(); ids.sort_custom(func(a, b): return rk[a] < rk[b])
	var top := featured.filter(func(id): return _usable(id)) if mode == "pick" else []
	if not top.is_empty():   # N3: the cells this task's reference circuit uses (and its chapter's) come first, under their own heading
		col.add_child(Kit.shout("FOR THIS TASK", int(20 * f) if not phone else 17, Kit.exc()))
		_new_grid(col, f, phone)
		for id in top: grid.add_child(_card_or_row(id, f, phone))
		col.add_child(Kit.shout("ALL PARTS", int(20 * f) if not phone else 17, Kit.TEXT_SOFT))
	_new_grid(col, f, phone)
	for id in ids:
		if not top.has(id): grid.add_child(_card_or_row(id, f, phone))
	if _scroll_keep > 0: _restore_scroll.call_deferred()

## A new card container, kept in `grid`: a flow of cards on a desktop, a plain list on a phone (M1).
func _new_grid(parent: Control, f: float, phone: bool) -> Container:
	if phone and mode == "pick":
		grid = VBoxContainer.new(); grid.add_theme_constant_override("separation", 6)
	else:
		var g := HFlowContainer.new(); g.add_theme_constant_override("h_separation", int(14 * f)); g.add_theme_constant_override("v_separation", int(14 * f)); grid = g
		g.last_wrap_alignment = FlowContainer.LAST_WRAP_ALIGNMENT_BEGIN   # cards stretch to fill each row (no empty gutter at the right); the last row keeps the column widths
	grid.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	parent.add_child(grid)
	return grid

func _card_or_row(m: String, f: float, phone: bool) -> Control:
	if phone and mode == "pick" and m != _expanded: return _row(m)
	var c := _card(m, f, phone)
	if phone and mode == "pick": c.size_flags_horizontal = Control.SIZE_EXPAND_FILL; c.custom_minimum_size.x = 0
	return c

## Phone picker row: title, model and family, the f-I sparkline and a USE button; tapping the row opens the full card.
func _row(m: String) -> Control:
	var usable := _usable(m)
	var p := PanelContainer.new(); p.add_theme_stylebox_override("panel", Kit.sb(Kit.PANEL if usable else Kit.WELL, Kit.exc() if m == selected else Kit.LINE, 2 if m == selected else 1, 10, 8, false))
	p.mouse_filter = Control.MOUSE_FILTER_STOP; p.gui_input.connect(func(e): if e is InputEventMouseButton and e.pressed and e.button_index == MOUSE_BUTTON_LEFT: _select(m))
	var h := HBoxContainer.new(); h.add_theme_constant_override("separation", 10); p.add_child(h)
	var v := VBoxContainer.new(); v.size_flags_horizontal = Control.SIZE_EXPAND_FILL; v.add_theme_constant_override("separation", 0); h.add_child(v)
	var tl := Kit.shout(Catalog.title(m), 17, Kit.TEXT if usable else Kit.TEXT_SOFT); tl.clip_text = true; v.add_child(tl)
	var cd := Catalog.card(m); var fam := str(cd.get("family", ""))
	var sub := str(cd.get("model", m)) + ("   " + fam if fam != "" else "")
	if not Catalog.available(m): sub = "not in this build"
	elif not usable: sub = "unlocks in " + str(Campaign.chapter_def(Catalog.chapter_of(m))["title"])
	var sl := Kit.label(sub, 14, Gfx.VIOLET if usable else Kit.TEXT_SOFT); sl.clip_text = true; v.add_child(sl)
	var ser := Catalog.series(m, "fi"); var t := Thumb.new(); t.kind = "fi"; t.xs = ser[0]; t.ys = ser[1]; t.dim = not usable; t.custom_minimum_size = Vector2(120, 50); t.size_flags_vertical = Control.SIZE_SHRINK_CENTER; t.mouse_filter = Control.MOUSE_FILTER_IGNORE; h.add_child(t)
	if usable: var b := Kit.btn("USE", func(): picked.emit(m), "amber", 17); Kit.ident(b, "Use_" + m); b.size_flags_vertical = Control.SIZE_SHRINK_CENTER; h.add_child(b)
	return p

func _restore_scroll() -> void:
	await get_tree().process_frame
	if is_instance_valid(scroll): scroll.scroll_vertical = _scroll_keep

## Usable cards first, then the ones that unlock later, then the ones this build cannot run.
func _rank(m: String, order: Array) -> int:
	var avail := Catalog.available(m); var open := _usable(m) or Catalog.unlocked(m, sandbox)
	return (0 if (avail and open) else (1 if avail else 2)) * 1000 + order.find(m)

func _select(m: String) -> void:
	if not _usable(m): return
	selected = m; _expanded = m
	_scroll_keep = scroll.scroll_vertical if scroll != null else 0
	_build()

func _usable(m: String) -> bool:
	return Catalog.usable(m, sandbox) or (mode == "pick" and reference.has(m) and Catalog.available(m))

func _card(m: String, f: float, phone: bool) -> Control:
	var usable := _usable(m); var avail := Catalog.available(m); var open := usable or Catalog.unlocked(m, sandbox)
	var is_sel := m == selected
	var w := (360.0 * f) if not phone else 300.0
	var p := PanelContainer.new(); p.custom_minimum_size = Vector2(w, 0); p.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var edge := Kit.exc() if is_sel else (Kit.LINE if usable else Color(Kit.LINE, 0.6))
	p.add_theme_stylebox_override("panel", Kit.sb(Kit.PANEL if usable else Kit.WELL, edge, 2 if is_sel else 1, 12, 14, false))
	p.mouse_filter = Control.MOUSE_FILTER_STOP; p.gui_input.connect(func(e): if e is InputEventMouseButton and e.pressed and e.button_index == MOUSE_BUTTON_LEFT: _select(m))
	if mode == "pick" and usable: p.mouse_default_cursor_shape = Control.CURSOR_POINTING_HAND
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", int(7 * f) if not phone else 5); p.add_child(v)
	var tcol := Kit.TEXT if usable else Kit.TEXT_SOFT
	var hd := HBoxContainer.new(); hd.add_theme_constant_override("separation", 8); v.add_child(hd)
	var tl := Kit.shout(Catalog.title(m), int(21 * f) if not phone else 18, tcol); tl.size_flags_horizontal = Control.SIZE_EXPAND_FILL; tl.autowrap_mode = TextServer.AUTOWRAP_WORD; hd.add_child(tl)   # break at word boundaries only ("integrate-and-fir / e")
	var chap: String = Catalog.chapter_of(m)
	var tag := "AVAILABLE"; var tc := Kit.GREEN
	if not avail: tag = "NOT IN THIS BUILD"; tc = Kit.TEXT_SOFT
	elif not open: tag = "UNLOCKS IN " + str(Campaign.chapter_def(chap)["title"]).to_upper(); tc = Kit.exc()
	var chip := Kit.chip(tag, tc, 14); chip.size_flags_vertical = Control.SIZE_SHRINK_BEGIN; hd.add_child(chip)
	v.add_child(Kit.label(str(Catalog.card(m).get("model", m)) + ("   " + str(Catalog.card(m).get("family", "")) if str(Catalog.card(m).get("family", "")) != "" else ""), 15, Gfx.VIOLET))
	var dl := Kit.label(Catalog.description(m), int(16 * f) if not phone else 15, Kit.TEXT_SOFT, true); v.add_child(dl)
	var th := HBoxContainer.new(); th.add_theme_constant_override("separation", 8); v.add_child(th)
	for kind in ["fi", "vm"]:
		var s := Catalog.series(m, kind); var t := Thumb.new(); t.kind = kind; t.xs = s[0]; t.ys = s[1]; t.dim = not usable
		t.custom_minimum_size = Vector2(0, 74 * f if not phone else 66); t.size_flags_horizontal = Control.SIZE_EXPAND_FILL; th.add_child(t)
	var ex := Catalog.examples(m)
	if not ex.is_empty(): v.add_child(Kit.label("Examples: " + ", ".join(ex), int(15 * f) if not phone else 15, Kit.TEXT_SOFT, true))
	var pr := Catalog.params(m)
	if not pr.is_empty():
		var fl := HFlowContainer.new(); fl.add_theme_constant_override("h_separation", 6); fl.add_theme_constant_override("v_separation", 6); v.add_child(fl)
		for q in pr:
			var txt := "%s %s %s" % [q[0], q[1], q[2]] if str(q[1]) != "" else str(q[0])
			var pc := PanelContainer.new(); pc.add_theme_stylebox_override("panel", Kit.sb(Kit.WELL, Kit.LINE, 1, 6, 6, false)); (pc.get_theme_stylebox("panel") as StyleBoxFlat).content_margin_top = 2; (pc.get_theme_stylebox("panel") as StyleBoxFlat).content_margin_bottom = 2
			pc.mouse_filter = Control.MOUSE_FILTER_IGNORE; pc.add_child(Kit.label(txt.strip_edges(), 15, Kit.TEXT)); fl.add_child(pc)
	if is_sel:   # the selected card shows where its numbers come from and who it suits
		var cd := Catalog.card(m)
		if str(cd.get("ei", "")) != "": v.add_child(Kit.label("Suits: %s. %s" % [str(cd["ei"]), str(cd.get("ei_note", ""))], 15, Kit.TEXT_SOFT, true))
		if str(cd.get("source", "")) != "": v.add_child(Kit.label("Source: " + str(cd["source"]), 14, Kit.TEXT_SOFT, true))
		if str(cd.get("error", "")) != "": v.add_child(Kit.label(str(cd["error"]), 14, Kit.RED, true))
	if mode == "pick" and usable:
		if is_sel: v.add_child(Kit.ident(Kit.btn("USE THIS PART", func(): picked.emit(m), "amber", 20), "Use_" + m))
		else: v.add_child(Kit.ident(Kit.btn("SELECT", func(): _select(m), "panel", 18), "Select_" + m))
	return p

## A small plot of an engine-rendered series: the f-I curve (rate against input current) or the V_m trace.
class Thumb extends Control:
	var kind := "fi"
	var xs := PackedFloat32Array()
	var ys := PackedFloat32Array()
	var dim := false
	func min_text_px() -> float: return float(Kit.fs(14)) * Kit.ppl
	func _draw() -> void:
		var fz := Kit.fs(14); var f := Fonts.regular
		draw_style_box(Kit.sb(Gfx.SHEET, Color(Kit.INK, 0.5), 1, 2, 0, false), Rect2(Vector2.ZERO, size))
		var col := (Kit.exc() if kind == "fi" else Kit.inh()); if dim: col = Color(col, 0.45)
		var title := "f-I" if kind == "fi" else "V_m"
		draw_string(f, Vector2(8, float(fz) + 2.0), title, HORIZONTAL_ALIGNMENT_LEFT, size.x - 12, fz, Color(Kit.TEXT_SOFT, 0.95))
		if xs.size() < 2:
			draw_string(f, Vector2(8, size.y - 8.0), "not rendered yet", HORIZONTAL_ALIGNMENT_LEFT, size.x - 12, fz, Color(Kit.TEXT_SOFT, 0.6)); return
		var x0 := xs[0]; var x1 := xs[xs.size() - 1]; var lo := ys[0]; var hi := ys[0]
		for y in ys: lo = minf(lo, y); hi = maxf(hi, y)
		if kind == "fi": lo = 0.0; hi = maxf(hi, 1.0)
		else: lo -= 3.0; hi += 3.0
		var inner := Rect2(Vector2(6, float(fz) + 8.0), size - Vector2(12, float(fz) + 14.0))
		var pts := PackedVector2Array()
		for i in xs.size(): pts.append(Vector2(inner.position.x + inner.size.x * (xs[i] - x0) / maxf(x1 - x0, 1e-6), inner.end.y - inner.size.y * (ys[i] - lo) / maxf(hi - lo, 1e-6)))
		if kind == "vm" and lo < -60.0 and hi > -60.0: pass
		draw_line(Vector2(inner.position.x, inner.end.y), Vector2(inner.end.x, inner.end.y), Color(Kit.LINE, 0.8), 1.0)
		draw_polyline(pts, col, 1.6, true)
		if size.x < 200.0: return   # a narrow thumbnail has no room for the range text
		var rng := ("%d Hz" % int(roundf(hi))) if kind == "fi" else ("%d to %d mV" % [int(roundf(lo + 3.0)), int(roundf(hi - 3.0))])
		draw_string(f, Vector2(size.x - 8.0 - 120.0, float(fz) + 2.0), rng, HORIZONTAL_ALIGNMENT_RIGHT, 120, fz, Color(Kit.TEXT, 0.9))
