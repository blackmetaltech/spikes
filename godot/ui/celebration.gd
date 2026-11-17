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

var _toasts: Array = []             # {node, age, life}
var _col: VBoxContainer
var _editor: CircuitEditor = null
var _tele: Telemetry = null
var _t := -1.0                      # seconds into a celebration, < 0 = none
var _saved_speed := 1.0
var _slowed := false
var _note := {}
var _note_shown := false
var _banner: Label = null

func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT); mouse_filter = Control.MOUSE_FILTER_IGNORE
	z_index = 60   # above the end card, the hint card and the sheets the lab adds to the overlay later: the notebook entry must not sit behind them
	_col = VBoxContainer.new(); _col.mouse_filter = Control.MOUSE_FILTER_IGNORE; _col.add_theme_constant_override("separation", 8)
	add_child(_col)

## A toast: a title and a line of text, gone after `life` seconds. `col` is its accent.
func toast(title: String, text: String, col: Color = Kit.AMBER, life := 5.0) -> void:
	if _col == null: return
	var pn := PanelContainer.new(); pn.mouse_filter = Control.MOUSE_FILTER_IGNORE
	pn.add_theme_stylebox_override("panel", Kit.sb(Kit.PANEL_HI, col, 2, 12, 12 if Layout.phone else 16, true))
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", 2); v.mouse_filter = Control.MOUSE_FILTER_IGNORE; pn.add_child(v)
	v.add_child(Kit.shout(title, 26 if not Layout.phone else 20, col))
	var l := Kit.label(text, 17 if not Layout.phone else 15, Kit.TEXT, true); l.custom_minimum_size.x = 520.0 if not Layout.phone else 440.0; v.add_child(l)
	_col.add_child(pn); _toasts.append({"node": pn, "age": 0.0, "life": life})
	while _toasts.size() > 3: var old: Dictionary = _toasts.pop_front(); (old["node"] as Node).queue_free()

func clear_toasts() -> void:
	for t in _toasts: (t["node"] as Node).queue_free()
	_toasts.clear()

## Start the first-pass celebration on `editor` / `tele`: `path` is the critical path (Circuits.critical_path), `note` the notebook entry (Task::notebook()).
func first_pass(editor: CircuitEditor, tele: Telemetry, path: Array, note: Dictionary) -> void:
	if _t >= 0.0: return
	_editor = editor; _tele = tele; _note = note; _note_shown = false; _t = 0.0
	GameAudio.event("celebrate")
	if _banner == null:
		_banner = Kit.shout("FIRST PASS!", 64 if not Layout.phone else 40, Kit.GREEN); _banner.mouse_filter = Control.MOUSE_FILTER_IGNORE
		_banner.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER; add_child(_banner)
	_banner.visible = true; _banner.modulate.a = 0.0
	if Kit.motion() or OS.get_environment("SP_CELEB") != "":   # the dev hook shows the motion even where screenshots turn it off
		_saved_speed = tele.speed; tele.speed = SLOW_SPEED; _slowed = true
		editor.push(PUSH, SLOW_S + 0.4)
		editor.trail = path; editor.trail_u = 0.0 if path.size() >= 2 else -1.0

func active() -> bool: return _t >= 0.0

func _process(dt: float) -> void:
	if _col != null: _col.position = Vector2((size.x - _col.size.x) * 0.5, 150.0 if not Layout.phone else 44.0)   # positioned by hand: the layer's own size is only known once it is in the tree
	for i in range(_toasts.size() - 1, -1, -1):
		var t: Dictionary = _toasts[i]; t["age"] = float(t["age"]) + dt
		var left := float(t["life"]) - float(t["age"])
		if left < 0.0: (t["node"] as Node).queue_free(); _toasts.remove_at(i)
		elif left < 0.5: (t["node"] as Control).modulate.a = left / 0.5
	if _t < 0.0: return
	_t += dt
	# banner: pops in over 0.25 s, holds, fades from 2.2 s
	if _banner != null:
		var a := clampf(_t / 0.25, 0.0, 1.0) * (1.0 - clampf((_t - 2.2) / 0.6, 0.0, 1.0))
		_banner.modulate.a = a; _banner.pivot_offset = _banner.size * 0.5; _banner.scale = Vector2.ONE * (0.7 + 0.3 * clampf(_t / 0.25, 0.0, 1.0))
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
