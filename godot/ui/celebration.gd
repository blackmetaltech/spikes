class_name Celebration
extends Control
## The lab's moments layer (docs/DESIGN-0.3.md section 8): toasts, and the first-pass celebration: slow motion, a camera push on the circuit, a glow trail that runs
## down the critical path (sensor to motor), a banner, and the lab-notebook entry unlocked by the pass. One node over the whole lab; it ignores the mouse.
## Reduced motion keeps the banner and the toasts and drops the slow motion, the push and the trail.
const SLOW_SPEED := 0.25
const SLOW_S := 2.4
const TRAIL_LOOP_S := 1.0
const TRAIL_LOOPS := 3
const PUSH := 0.14

var _toasts: Array = []             # {node, age, life}: the toast on screen (at most one)
var _queue: Array = []              # toasts waiting for their turn: {title, text, col, life}
## () -> bool, set by the lab: true while the result modal is open. A toast never shows over the modal (its title would be covered): it waits, and appears when the modal closes.
var blocked: Callable = Callable()
var _col: VBoxContainer
var _editor: CircuitEditor = null
var _tele: Telemetry = null
var _t := -1.0                      # seconds into a celebration, < 0 = none
var _saved_speed := 1.0
var _slowed := false
var _note := {}
var _note_shown := false
var _banner: StampMark = null

## The first-pass banner: a vermilion rubber stamp ("FIRST PASS", the score, "of 100") that lands on the lab.
class StampMark extends Control:
	var k := 1.0
	var score := 100
	func _draw() -> void: Gfx.stamp(self, size * 0.5, minf(size.x, size.y) * 0.5 - 6.0, "FIRST PASS", "%d" % score, "of 100", Kit.RED, -0.2, k)

func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT); mouse_filter = Control.MOUSE_FILTER_IGNORE
	z_index = 60   # above the end card, the hint card and the sheets the lab adds to the overlay later: the notebook entry must not sit behind them
	_col = VBoxContainer.new(); _col.mouse_filter = Control.MOUSE_FILTER_IGNORE; _col.add_theme_constant_override("separation", 8)
	add_child(_col)

## A toast: a title and a line of text, gone after `life` seconds. `col` is its accent.
## Toasts are queued: only one is on screen at a time, the next one appears when it has gone (and none while the result modal is open).
func toast(title: String, text: String, col: Color = Kit.AMBER, life := 5.0) -> void:
	if _col == null: return
	for q in _queue:
		if str(q["title"]) == title and str(q["text"]) == text: return   # the same message twice in a row is one message
	_queue.append({"title": title, "text": text, "col": col, "life": life})
	while _queue.size() > 6: _queue.pop_front()
	_pump()

func _is_blocked() -> bool: return blocked.is_valid() and bool(blocked.call())

## Show the next queued toast when none is on screen and the modal is not up.
func _pump() -> void:
	if _col == null or not _toasts.is_empty() or _queue.is_empty() or _is_blocked(): return
	var q: Dictionary = _queue.pop_front()
	_show_toast(str(q["title"]), str(q["text"]), q["col"], float(q["life"]))

func _show_toast(title: String, text: String, col: Color, life: float) -> void:
	var pn := PanelContainer.new(); Kit.ident(pn, "Toast"); pn.mouse_filter = Control.MOUSE_FILTER_IGNORE
	pn.add_theme_stylebox_override("panel", Kit.sb(Kit.PANEL_HI, col, 2, 12, 12 if Layout.phone else 16, true))
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", 2); v.mouse_filter = Control.MOUSE_FILTER_IGNORE; pn.add_child(v)
	v.add_child(Kit.shout(title, 26 if not Layout.phone else 20, col))
	var l := Kit.label(text, 17 if not Layout.phone else 15, Kit.TEXT, true); l.custom_minimum_size.x = 520.0 if not Layout.phone else 440.0; v.add_child(l)
	_col.add_child(pn); _toasts.append({"node": pn, "age": 0.0, "life": life})

func clear_toasts() -> void:
	for t in _toasts: (t["node"] as Node).queue_free()
	_toasts.clear(); _queue.clear()

## Start the first-pass celebration on `editor` / `tele`: `path` is the critical path (Circuits.critical_path), `note` the notebook entry (Task::notebook()).
func first_pass(editor: CircuitEditor, tele: Telemetry, path: Array, note: Dictionary) -> void:
	if _t >= 0.0: return
	_editor = editor; _tele = tele; _note = note; _note_shown = false; _t = 0.0
	GameAudio.event("celebrate")
	if _banner == null:
		_banner = StampMark.new(); Kit.ident(_banner, "FirstPassStamp"); _banner.mouse_filter = Control.MOUSE_FILTER_IGNORE; add_child(_banner)
	var side := 260.0 if not Layout.phone else 170.0
	_banner.size = Vector2(side, side); _banner.score = int(roundf(float(tele.score.get("value", 100.0))))
	_banner.visible = true; _banner.modulate.a = 0.0
	if Kit.motion() or OS.get_environment("SP_CELEB") != "":   # the dev hook shows the motion even where screenshots turn it off
		_saved_speed = tele.speed; tele.speed = SLOW_SPEED; _slowed = true
		editor.push(PUSH, SLOW_S + 0.4)
		editor.trail = path; editor.trail_u = 0.0 if path.size() >= 2 else -1.0

func active() -> bool: return _t >= 0.0

func _process(dt: float) -> void:
	if _col != null: _col.position = Vector2((size.x - _col.size.x) * 0.5, 150.0 if not Layout.phone else 44.0)   # positioned by hand: the layer's own size is only known once it is in the tree
	var held := _is_blocked()
	if _col != null: _col.visible = not held   # the result modal is up: the toast on screen waits (its clock stops) instead of covering the modal's title
	if not held: _pump()
	for i in range(_toasts.size() - 1, -1, -1):
		if held: break
		var t: Dictionary = _toasts[i]; t["age"] = float(t["age"]) + dt
		var left := float(t["life"]) - float(t["age"])
		if left < 0.0: (t["node"] as Node).queue_free(); _toasts.remove_at(i)
		elif left < 0.5: (t["node"] as Control).modulate.a = left / 0.5
	if _t < 0.0: return
	_t += dt
	# banner: pops in over 0.25 s, holds, fades from 2.2 s
	if _banner != null:
		_banner.modulate.a = 1.0 - clampf((_t - 2.2) / 0.6, 0.0, 1.0); _banner.k = clampf(_t / 0.25, 0.0, 1.0); _banner.queue_redraw()
		_banner.position = (size - _banner.size) * 0.5
	if _editor != null and is_instance_valid(_editor) and _editor.trail_u >= 0.0:
		var loop := _t / TRAIL_LOOP_S
		_editor.trail_u = -1.0 if loop >= float(TRAIL_LOOPS) else fposmod(loop, 1.0)
		if _editor.trail_u < 0.0: _editor.trail = []
	if _slowed and _t >= SLOW_S and _tele != null:
		_tele.speed = _saved_speed; _slowed = false
	if not _note_shown and _t >= 1.0:
		_note_shown = true
		if str(_note.get("title", "")) != "": toast("Notebook: " + str(_note["title"]), str(_note.get("text", "")) + ("  " + str(_note["citation"]) if str(_note.get("citation", "")) != "" else ""), Kit.CYAN, 8.0)
	if _t >= 3.2:
		_t = -1.0
		if _banner != null: _banner.visible = false
		if _slowed and _tele != null: _tele.speed = _saved_speed; _slowed = false
		if _editor != null and is_instance_valid(_editor): _editor.trail_u = -1.0; _editor.trail = []

## Stop a celebration at once (the circuit was edited or reloaded): the speed goes back, the trail goes out.
func abort() -> void:
	if _slowed and _tele != null: _tele.speed = _saved_speed
	_slowed = false; _t = -1.0
	if _banner != null: _banner.visible = false
	if _editor != null and is_instance_valid(_editor): _editor.trail_u = -1.0; _editor.trail = []
