class_name SpikeScope
extends Control
## Live scopes for a SpikeSim: a spike raster (one row per neuron, amber = excitatory, cyan = inhibitory) over a membrane-voltage trace.
## Steps the simulation in real time from _process while `running`. Draws everything itself, so it is crisp at every scale.
const WINDOW_MS := 500.0
const VOLT_SAMPLES := 500          # one sample per ms of simulated time
const V_MIN := -80.0
const V_MAX := -40.0
const CHUNK := 10                  # steps per voltage sample
var sim = null
var kinds: Array = []
var running := true
var speed := 1.0
var watch := [0, 6]                # neurons whose voltage is traced
var _spikes := PackedInt32Array()  # id, step pairs inside the window
var _cursor := 0
var _volt := {}                    # id -> PackedFloat32Array ring (oldest first)
var _carry := 0.0
var total_spikes := 0

func _ready() -> void:
	custom_minimum_size = Vector2(300, 220)
	clip_contents = true

func min_text_px() -> float: return float(Kit.fs(14)) * Kit.ppl

func attach(s, neuron_kinds: Array) -> void:
	sim = s; kinds = neuron_kinds; _spikes = PackedInt32Array(); _cursor = 0; _volt = {}; total_spikes = 0
	for id in watch: _volt[id] = PackedFloat32Array()

func _process(dt: float) -> void:
	if sim == null or not running or not is_visible_in_tree(): return
	_carry += dt * 1000.0 * speed / float(sim.dt_ms())   # steps owed this frame
	var steps := mini(int(_carry), 4000); _carry -= float(int(_carry)); if steps < CHUNK: _carry += float(steps); steps = 0
	while steps >= CHUNK:
		sim.step(CHUNK); steps -= CHUNK
		for id in watch:
			var a: PackedFloat32Array = _volt[id]; a.append(float(sim.voltage(id)))
			if a.size() > VOLT_SAMPLES: a = a.slice(a.size() - VOLT_SAMPLES)
			_volt[id] = a
	var fresh: PackedInt32Array = sim.spikes_since(_cursor)
	if fresh.size() > 0:
		total_spikes += fresh.size() / 2; _spikes.append_array(fresh)
	_cursor = int(sim.step_count())
	var keep_from := _cursor - int(WINDOW_MS / float(sim.dt_ms()))
	var i := 0
	while i < _spikes.size() and _spikes[i + 1] < keep_from: i += 2
	if i > 0: _spikes = _spikes.slice(i)
	queue_redraw()

func _kind_col(id: int) -> Color:
	return Kit.inh() if id < kinds.size() and kinds[id] == "I" else Kit.exc()

func _draw() -> void:
	var f := Fonts.regular; var fz := Kit.fs(14)
	var w := size.x; var h := size.y
	draw_style_box(Kit.sb(Kit.WELL, Kit.LINE, 1, 10, 0, false), Rect2(Vector2.ZERO, size))
	var left := float(fz) * 3.2; var top := 10.0; var gap := 12.0 + float(fz)
	var rh := (h - top * 2.0 - gap) * 0.55; var vy := top + rh + gap; var vh := h - vy - 10.0
	var pw := w - left - 12.0
	# raster
	var n := maxi(kinds.size(), 1); var row := rh / float(n)
	for k in range(0, 5):
		var x := left + pw * float(k) / 4.0
		draw_line(Vector2(x, top), Vector2(x, top + rh), Color(Kit.LINE, 0.55), 1.0)
	draw_string(f, Vector2(8, top + float(fz)), "unit", HORIZONTAL_ALIGNMENT_LEFT, -1, fz, Kit.TEXT_SOFT)
	if sim != null:
		var t_now := float(_cursor) * float(sim.dt_ms()); var t0 := t_now - WINDOW_MS
		var i := 0
		while i < _spikes.size():
			var t := float(_spikes[i + 1]) * float(sim.dt_ms())
			var x := left + pw * (t - t0) / WINDOW_MS
			var id := _spikes[i]
			var y := top + row * (float(id) + 0.5)
			var col := _kind_col(id)
			draw_line(Vector2(x, y - row * 0.38), Vector2(x, y + row * 0.38), col, 2.0)
			i += 2
	draw_string(f, Vector2(left, top + rh + float(fz) + 2.0), "-500 ms", HORIZONTAL_ALIGNMENT_LEFT, -1, fz, Kit.TEXT_SOFT)
	draw_string(f, Vector2(left + pw - 40.0, top + rh + float(fz) + 2.0), "now", HORIZONTAL_ALIGNMENT_RIGHT, 40, fz, Kit.TEXT_SOFT)
	# voltage
	draw_rect(Rect2(left, vy, pw, vh), Color(Kit.PANEL, 0.6), true)
	for mv in [-70.0, -50.0]:
		var y := vy + vh * (V_MAX - mv) / (V_MAX - V_MIN)
		draw_line(Vector2(left, y), Vector2(left + pw, y), Color(Kit.LINE, 0.7), 1.0)
		draw_string(f, Vector2(4, y + float(fz) * 0.35), "%d" % int(mv), HORIZONTAL_ALIGNMENT_LEFT, -1, fz, Kit.TEXT_SOFT)
	draw_string(f, Vector2(left + 6.0, vy + float(fz) + 2.0), "membrane potential, mV", HORIZONTAL_ALIGNMENT_LEFT, -1, fz, Kit.TEXT_SOFT)
	for id in watch:
		if not _volt.has(id): continue
		var a: PackedFloat32Array = _volt[id]; if a.size() < 2: continue
		var pts := PackedVector2Array()
		for j in a.size():
			var x := left + pw * (float(j + VOLT_SAMPLES - a.size()) / float(VOLT_SAMPLES - 1))
			pts.append(Vector2(x, vy + vh * (V_MAX - clampf(a[j], V_MIN, V_MAX)) / (V_MAX - V_MIN)))
		draw_polyline(pts, _kind_col(id), 2.0, true)
