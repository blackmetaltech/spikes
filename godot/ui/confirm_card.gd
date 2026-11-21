class_name ConfirmCard
extends Control
## A small modal "are you sure": a scrim, a card with a title, a sentence and two buttons (Cancel / the action). Escape or a click on the scrim cancels.
##   ConfirmCard.ask(host, "Replace the bench?", "Your circuit is kept ...", "Replace", func(): ..., true)
## `host` is any Control that fills the screen (the lab's overlay layer, a settings page); only one card is open at a time (a second ask replaces the first).
## Ids for the interaction graph: ConfirmCard (the root), BtnConfirmYes, BtnConfirmNo.

static var _open: ConfirmCard = null

static func is_open() -> bool: return _open != null and is_instance_valid(_open)
## Escape / B: close the open card. True when there was one.
static func dismiss() -> bool:
	if not is_open(): return false
	_open.close(); return true

static func ask(host: Control, title: String, text: String, yes_label: String, on_yes: Callable, danger := false) -> ConfirmCard:
	if is_open(): _open.queue_free()
	var c := ConfirmCard.new(); _open = c
	Kit.ident(c, "ConfirmCard"); c.set_anchors_preset(Control.PRESET_FULL_RECT); c.mouse_filter = Control.MOUSE_FILTER_STOP; c.z_index = 80
	var dim := ColorRect.new(); dim.color = Color(Kit.BENCH, 0.78); dim.set_anchors_preset(Control.PRESET_FULL_RECT); c.add_child(dim)
	var pn := PanelContainer.new(); pn.add_theme_stylebox_override("panel", Kit.plate_sb(Kit.PANEL, 20)); c.add_child(pn)
	pn.set_anchors_preset(Control.PRESET_CENTER); pn.grow_horizontal = Control.GROW_DIRECTION_BOTH; pn.grow_vertical = Control.GROW_DIRECTION_BOTH
	var w := minf(620.0, maxf(280.0, host.get_viewport_rect().size.x - 48.0))
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", 12); v.custom_minimum_size.x = w; pn.add_child(v)
	var t := Kit.label(title, 26, Kit.TEXT, true); v.add_child(t)
	v.add_child(Kit.label(text, 19, Kit.TEXT_SOFT, true))
	var row := HBoxContainer.new(); row.add_theme_constant_override("separation", 12); row.alignment = BoxContainer.ALIGNMENT_END; v.add_child(row)
	var no := Kit.btn("Cancel", func(): c.close(), "panel", 20); Kit.ident(no, "BtnConfirmNo"); row.add_child(no)
	var yes := Kit.btn(yes_label, func(): c.close(); on_yes.call(), "danger" if danger else "amber", 20); Kit.ident(yes, "BtnConfirmYes"); row.add_child(yes)
	host.add_child(c)
	if Kit.pad_mode: no.call_deferred("grab_focus")
	return c

func close() -> void:
	if _open == self: _open = null
	queue_free()

func _gui_input(e: InputEvent) -> void:
	if e is InputEventKey and e.pressed and e.keycode == KEY_ESCAPE: close(); accept_event()
