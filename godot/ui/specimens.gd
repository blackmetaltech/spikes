class_name Specimens
extends RefCounted
## The body views of the Bestiary wing: every level of a Bestiary chapter (its diagnosis case too) shows its animal as a living specimen in the hand of the
## campaign-map plates. The still engraving of the animal is cut from the same drawing as its plate (tools/art/specimens.py renders
## res://assets/specimens/<id>.png with specimens.json: the crop of the plate and the anchors of the live parts, in plate px); what the simulation moves
## is drawn live on top of it, from the running circuit only: the motor channels' rates, the spikes of their cells, the sensors' values and the task's own
## stimulus schedule (its timeline). Nothing moves that the simulation does not move.
##   worm      C. elegans crawling forward or backing up with the motor output, the touch cells lit where it is touched
##   leech     the two heart tubes beating with their side, the heart interneurons' bursts as a pen record with the period and phase
##   aplysia   the gill withdrawing in proportion to the motor cell's firing, the siphon touch and the tail shock marked, the withdrawal at every touch
##   crab      the stomatogastric ganglion magnified with its cells lit, the triphasic rhythm as a pen record with the phase marks
##   fly_mb    the mushroom body: the Kenyon cells as a field of points (which fire for odour A and which for odour B), the output and the dopamine cells
##   compass   the ellipsoid-body ring as 16 wedges washed by their cells' firing, the bump as a gamboge needle, the landmark, a fly turning with its heading
## Each view keeps the level's judged quantities as compact band gauges (BodyViews._gauge) and works in the phone layout (a short view drops the captions).

const CHAPTERS := ["worm", "leech", "aplysia", "crab", "fly_mb", "fly_compass"]
const SLATE := Color("#6B7A86")
const WALL := Color("#A2493F")
const WALL_PALE := Color("#E7B9A8")
const KCF := Color("#EBC3A6")
const NUC := Color("#6E4E7E")

static func chapter(t: Telemetry) -> String:
	if t == null or t.task == null: return ""
	return str(Campaign.info(str(t.task.id()))["chapter"])

static func handles(t: Telemetry) -> bool: return CHAPTERS.has(chapter(t))

static func draw(c: Control, t: Telemetry, r: Rect2) -> void:
	if t == null or t.task == null: BodyViews._empty(c, r, "No task."); return
	match chapter(t):
		"worm": _worm(c, t, r)
		"leech": _leech(c, t, r)
		"aplysia": _aplysia(c, t, r)
		"crab": _crab(c, t, r)
		"fly_mb": _fly_mb(c, t, r)
		"fly_compass": _compass(c, t, r)

# ================================================================ shared: layers, telemetry, figures

static var _tex := {}
static var _meta = null
static func meta(id: String) -> Dictionary:
	if _meta == null:
		_meta = {}
		var f := FileAccess.open("res://assets/specimens/specimens.json", FileAccess.READ)
		if f != null:
			var d = JSON.parse_string(f.get_as_text())
			if d is Dictionary: _meta = d
	return (_meta as Dictionary).get(id, {})

## The layer's texture, held in a static cache (a texture loaded into a local is freed before the canvas renders it).
static func tex(id: String) -> Texture2D:
	if not _tex.has(id):
		var p := "res://assets/specimens/%s.png" % id
		_tex[id] = load(p) as Texture2D if ResourceLoader.exists(p) else null
	return _tex[id]

## Fit a layer into `box` at its own aspect (`ax`, `ay`: where it sits in the spare room, 0..1) and return {rect, crop, k}: k = screen px per plate px.
static func fit(id: String, box: Rect2, ax := 0.5, ay := 0.5, zoom := 1.0) -> Dictionary:
	var m := meta(id)
	if m.is_empty(): return {}
	var sz := Vector2(float(m["size"][0]), float(m["size"][1])); var cr: Array = m["crop"]
	var k := minf(box.size.x / sz.x, box.size.y / sz.y) * zoom; var s := sz * k
	var rect := Rect2(box.position + (box.size - s) * Vector2(ax, ay), s)
	return {"rect": rect, "crop": Rect2(float(cr[0]), float(cr[1]), float(cr[2]), float(cr[3])), "k": rect.size.x / float(cr[2]), "a": m.get("anchors", {})}

## A plate point (an Array [x, y] in plate px) on screen.
static func mp(L: Dictionary, p) -> Vector2:
	var cr: Rect2 = L["crop"]
	return (L["rect"] as Rect2).position + (Vector2(float(p[0]), float(p[1])) - cr.position) * float(L["k"])

static func mpl(L: Dictionary, pts: Array) -> PackedVector2Array:
	var out := PackedVector2Array()
	for p in pts: out.append(mp(L, p))
	return out

static func layer(c: Control, id: String, L: Dictionary) -> void:
	var tx := tex(id)
	if tx != null and not L.is_empty(): c.draw_texture_rect(tx, L["rect"], false)

## Per-run state of a view (reset when the telemetry, the episode or the model time goes back). `dt` is the model time (ms) since the last frame.
static var _st := {}
static func state(t: Telemetry, key: String) -> Dictionary:
	var s: Dictionary = _st.get(key, {})
	if int(s.get("id", 0)) != t.get_instance_id() or int(s.get("ep", -1)) != t.episode or t.t_ms < float(s.get("prev", 0.0)) - 0.5:
		s = {"id": t.get_instance_id(), "ep": t.episode, "prev": t.t_ms, "dt": 0.0, "max": {}}
		_st[key] = s
	s["dt"] = clampf(t.t_ms - float(s["prev"]), 0.0, 250.0); s["prev"] = t.t_ms
	return s

## A level's smoothed rate of an output channel, Hz per cell (Telemetry.rates), 0 when the circuit has no such channel.
static func rate(t: Telemetry, ch: String) -> float: return float(t.rates.get(ch, 0.0))
static func has_out(t: Telemetry, ch: String) -> bool: return t.out_mask.has(ch)
static func motors(t: Telemetry) -> Array: return Array(t.task.motor_channels()) if t.task != null else []

## `v` against the largest value of `key` seen in this run (at least `floor_`), 0..1.
static func norm(s: Dictionary, key: String, v: float, floor_: float) -> float:
	var mx: Dictionary = s["max"]; var m := maxf(float(mx.get(key, floor_)), v); mx[key] = m
	return clampf(v / maxf(m, 1e-6), 0.0, 1.0)

## The task's own stimulus schedule: is an event whose label contains one of `needles` on now (an event with no duration lasts from its start on)?
static func ev_on(t: Telemetry, needles: Array) -> bool:
	for e in Campaign.info(str(t.task.id()))["timeline"]:
		var lab := str(e["label"]).to_lower(); var t0 := float(e["t_ms"]); var du := float(e["dur_ms"])
		if t.t_ms < t0 or (du > 0.0 and t.t_ms > t0 + du): continue
		for n in needles:
			if lab.contains(str(n)): return true
	return false

## Events whose label contains `needle`: [[t0, t1, label], ...].
static func ev_list(t: Telemetry, needle: String) -> Array:
	var out := []
	for e in Campaign.info(str(t.task.id()))["timeline"]:
		var lab := str(e["label"]).to_lower()
		if lab.contains(needle): out.append([float(e["t_ms"]), float(e["t_ms"]) + maxf(float(e["dur_ms"]), 1.0), lab])
	return out

## The value the task sets on an input channel now (0 when the circuit has no such input).
static func inp(t: Telemetry, ch: String) -> float: return float(t.sim.input_value(ch)) if t.sim != null else 0.0

## Spike times (ms) of every output channel's cells, collected incrementally from Telemetry.sp_n / sp_t (once per frame), the last 15 s.
static var _lg := {}
static func spikes(t: Telemetry) -> Dictionary:
	if int(_lg.get("id", 0)) != t.get_instance_id() or int(_lg.get("ep", -1)) != t.episode or t.t_ms < float(_lg.get("now", 0.0)) - 0.5 or _lg.get("keys", []) != t.out_mask.keys():
		_lg = {"id": t.get_instance_id(), "ep": t.episode, "now": t.t_ms, "last": -1.0, "fr": -1, "ch": {}, "keys": t.out_mask.keys(), "cell": PackedFloat32Array()}
		for k in t.out_mask: _lg["ch"][k] = PackedFloat32Array()
		var cl := PackedFloat32Array(); cl.resize(t.n); cl.fill(-1.0e9); _lg["cell"] = cl
	_lg["now"] = t.t_ms
	if int(_lg["fr"]) == Engine.get_process_frames(): return _lg
	_lg["fr"] = Engine.get_process_frames()
	var last := float(_lg["last"]); var nsp := t.sp_t.size(); var j := nsp - 1
	while j >= 0 and t.sp_t[j] > last: j -= 1
	j += 1
	if j < nsp:
		var cl: PackedFloat32Array = _lg["cell"]; _lg["cell"] = PackedFloat32Array()   # dropped from the dictionary while it is written, so the writes do not copy it
		for i in range(j, nsp):
			var id := t.sp_n[i]
			if id < cl.size(): cl[id] = t.sp_t[i]
		var chs: Dictionary = _lg["ch"]
		for k in chs.keys():
			var a: PackedFloat32Array = chs[k]; chs[k] = PackedFloat32Array()
			var m: PackedByteArray = t.out_mask[k]
			for i in range(j, nsp):
				var id := t.sp_n[i]
				if id < m.size() and m[id] == 1: a.append(t.sp_t[i])
			if a.size() > 6000:
				var cut := t.t_ms - 15000.0; var q := 0
				while q < a.size() and a[q] < cut: q += 1
				a = a.slice(q)
			chs[k] = a
		_lg["cell"] = cl; _lg["last"] = t.sp_t[nsp - 1]
	return _lg

static func times(t: Telemetry, ch: String) -> PackedFloat32Array: return (spikes(t)["ch"] as Dictionary).get(ch, PackedFloat32Array())

## How recently a cell fired, 1 at a spike decaying with `tau` ms of model time (so a paused or held frame keeps its glow).
static func cell_act(t: Telemetry, id: int, tau := 60.0) -> float:
	var cl: PackedFloat32Array = spikes(t)["cell"]
	return exp(-(t.t_ms - cl[id]) / tau) if id >= 0 and id < cl.size() else 0.0

## Burst onsets in a spike train: a spike after at least `gap` ms of silence.
static func onsets(a: PackedFloat32Array, t0: float, gap := 180.0) -> PackedFloat32Array:
	var out := PackedFloat32Array(); var prev := -1.0e9
	for x in a:
		if x - prev >= gap and x >= t0: out.append(x)
		prev = x
	return out

## Neurons of the circuit's populations whose id matches `pred` (case-insensitive prefix list), in order.
static func pop_cells(t: Telemetry, prefixes: Array) -> PackedInt32Array:
	var out := PackedInt32Array(); var pops: Array = t.circuit.get("populations", [])
	for i in t.pop_of.size():
		var pi := t.pop_of[i]
		if pi < 0 or pi >= pops.size(): continue
		var nm := str((pops[pi] as Dictionary).get("id", "")).to_upper()
		for p in prefixes:
			if nm.begins_with(str(p).to_upper()): out.append(i); break
	return out

static func out_cells(t: Telemetry, ch: String) -> PackedInt32Array:
	if t.sim == null: return PackedInt32Array()
	for o in t.sim.outputs():
		if str(o["name"]) == ch: return o["neurons"]
	return PackedInt32Array()

## Mean rate (Hz per cell) of a set of cells over the last `win` ms (from the spike list).
static func cells_rate(t: Telemetry, cells: PackedInt32Array, win := 150.0) -> float:
	if cells.is_empty(): return 0.0
	var m := {}
	for id in cells: m[id] = true
	var j := t.sp_t.size() - 1; var t0 := t.t_ms - win; var cnt := 0
	while j >= 0 and t.sp_t[j] >= t0:
		if m.has(t.sp_n[j]): cnt += 1
		j -= 1
	return float(cnt) / (win * 0.001 * float(cells.size()))

# ---------------------------------------------------------------- figures

static func fz() -> int: return Kit.fs(13)

## "Fig. N." in bold italic, the caption in soft italic, at baseline p; clipped to width w.
static func fig(c: Control, p: Vector2, num: int, text: String, w: float) -> void:
	var f := fz(); var lead := "Fig. %d. " % num
	var lw := Fonts.bold.get_string_size(lead, HORIZONTAL_ALIGNMENT_LEFT, -1, f).x
	c.draw_string(Fonts.bold, p, lead, HORIZONTAL_ALIGNMENT_LEFT, w, f, Kit.TEXT)
	if w - lw > 20.0: c.draw_string(Gfx.serif_italic(), p + Vector2(lw, 0), text, HORIZONTAL_ALIGNMENT_LEFT, w - lw, f, Kit.TEXT_SOFT)

static func tag(c: Control, s: String, p: Vector2, col: Color, align := HORIZONTAL_ALIGNMENT_LEFT) -> Rect2: return Anatomy.tag(c, s, p, col, 13, align, false, 0.72)

## An engraved figure: an italic caption over a value in the readout face.
static func readout(c: Control, p: Vector2, cap: String, val: String, col: Color, w: float, big := 22) -> float:
	var f := fz()
	c.draw_string(Gfx.serif_italic(), p + Vector2(0, f), cap, HORIZONTAL_ALIGNMENT_LEFT, w, f, Kit.TEXT_SOFT)
	c.draw_string(Gfx.mono(), p + Vector2(0, f + Kit.fs(big) + 2), val, HORIZONTAL_ALIGNMENT_LEFT, w, Kit.fs(big), col)
	return float(f + Kit.fs(big) + 8)

## The level's judged quantities as compact band gauges, stacked down `r` (or side by side when `row`).
static func bands(c: Control, t: Telemetry, r: Rect2, row := false) -> void:
	var d := BodyViews.ro(t); var bs: Array = Campaign.info(str(t.task.id()))["bands"]
	if bs.is_empty() or r.size.x < 40.0 or r.size.y < 30.0: return
	var n := bs.size(); var gh := minf(r.size.y / float(n), 86.0) if not row else r.size.y; var gw := r.size.x if not row else r.size.x / float(n)
	for i in n:
		var b: Dictionary = bs[i]; var cr := Rect2(r.position + (Vector2(gw * float(i), 0) if row else Vector2(0, gh * float(i))), Vector2(gw, gh)).grow(-3.0)
		BodyViews._gauge(c, t, cr, b, BodyViews.rv(d, str(b["name"])), true, d.has(str(b["name"])))

## A pen record in the plate's Fig. 3 manner: one row per entry {label, times (spike ms), col, spans ([[t0, t1]] of a stimulus), span_col}, the last `win` ms,
## spikes as inked sticks on a hairline, stimulus spans as a pale wash under the row. Returns the row rects (for marks).
static var _sticks := PackedVector2Array()
static func record(c: Control, r: Rect2, rows: Array, t_now: float, win: float) -> Array:
	var f := fz(); var gut := 0.0
	for row in rows: gut = maxf(gut, Gfx.serif_italic().get_string_size(str(row["label"]), HORIZONTAL_ALIGNMENT_LEFT, -1, f).x)
	gut += 10.0
	var n := rows.size(); var rh := r.size.y / float(maxi(n, 1)); var x0 := r.position.x + gut; var w := r.size.x - gut; var t0 := t_now - win
	var out := []
	for k in n:
		var row: Dictionary = rows[k]; var y1 := r.position.y + rh * float(k + 1) - 3.0; var hh := rh * 0.78
		var rr := Rect2(x0, y1 - hh, w, hh); out.append(rr)
		for sp in row.get("spans", []):
			var a := clampf((float(sp[0]) - t0) / win, 0.0, 1.0); var b := clampf((float(sp[1]) - t0) / win, 0.0, 1.0)
			if b > a: c.draw_rect(Rect2(x0 + w * a, y1 - hh, w * (b - a), hh), Color(row.get("span_col", Kit.SENSOR), 0.16))
		c.draw_line(Vector2(x0, y1), Vector2(x0 + w, y1), Color(Kit.INK, 0.35), 1.0)
		c.draw_string(Gfx.serif_italic(), Vector2(r.position.x, y1 - 2.0), str(row["label"]), HORIZONTAL_ALIGNMENT_LEFT, gut, f, Kit.TEXT)
		var a2: PackedFloat32Array = row.get("times", PackedFloat32Array())
		_sticks.clear(); var j := a2.size() - 1
		while j >= 0 and a2[j] >= t0:
			var x := x0 + w * (a2[j] - t0) / win; _sticks.append(Vector2(x, y1)); _sticks.append(Vector2(x, y1 - hh * 0.92)); j -= 1
		if _sticks.size() > 1: c.draw_multiline(_sticks, Color(row.get("col", Kit.INK), 0.85), 1.0)
	return out

static func small(r: Rect2) -> bool: return r.size.y < 150.0

## Split a view: the specimen at the left, a column at the right (`frac` of the width) for the figures and gauges.
static func split(r: Rect2, frac: float) -> Array:
	var rw := r.size.x * frac
	return [Rect2(r.position, Vector2(r.size.x - rw - 10.0, r.size.y)), Rect2(r.end.x - rw, r.position.y, rw, r.size.y)]

# ================================================================ worm (C. elegans touch: w_latch, w_switch, worm_touch, dx_worm)

static var _wq := PackedVector2Array([Vector2.ZERO, Vector2.ZERO, Vector2.ZERO, Vector2.ZERO])
static var _wu := PackedVector2Array([Vector2.ZERO, Vector2.ZERO, Vector2.ZERO, Vector2.ZERO])
static var _wc := PackedColorArray([Color.WHITE, Color.WHITE, Color.WHITE, Color.WHITE])
const WN := 56

static func _worm(c: Control, t: Telemetry, r: Rect2) -> void:
	var sm := small(r); var f := fz(); var s := state(t, "worm")
	var parts := split(r, 0.27 if not sm else 0.30); var box: Rect2 = parts[0]; var col: Rect2 = parts[1]
	var m := meta("worm"); var tx := tex("worm")
	if m.is_empty() or tx == null: BodyViews._empty(c, r, "The worm specimen is missing (tools/art/specimens.py)."); return
	var A: Dictionary = m["anchors"]; var cr: Array = m["crop"]
	var x0 := float(A["x0"]); var x1 := float(A["x1"]); var span := x1 - x0; var prof: Array = A["prof"]
	var rev := rate(t, "reverse"); var fwd := rate(t, "forward")
	var v := clampf((fwd - rev) / 25.0, -1.0, 1.0)
	var dt := float(s["dt"]) * 0.001
	if Kit.motion() or not s.has("ph"): s["ph"] = float(s.get("ph", 0.0)) + TAU * 1.2 * v * dt   # the body wave runs head to tail when it crawls forward
	s["x"] = clampf(float(s.get("x", 0.0)) - 0.10 * v * dt, -0.13, 0.13)                            # and the worm moves toward its head
	var cap_h := 0.0 if sm else float(f) + 8.0
	var dish := Rect2(box.position.x, box.position.y + 2.0, box.size.x, box.size.y - cap_h - 4.0)
	# the agar: a pale strip with an engraved edge and the worm's track
	c.draw_rect(dish, Color(Gfx.SHEET, 0.85)); c.draw_rect(dish, Color(Kit.INK, 0.35), false, 1.0)
	var Lpx := dish.size.x * 0.70; var k := Lpx / span
	var half := float(cr[3]) * 0.5 * k
	var amp := minf(Lpx * 0.05, maxf(dish.size.y * 0.5 - half - 18.0, 4.0))
	var ctr := dish.get_center() + Vector2(float(s["x"]) * dish.size.x, -6.0 if not sm else -2.0)
	var ph := float(s["ph"])
	var sa := (float(cr[0]) - x0) / span; var sb := (float(cr[0]) + float(cr[2]) - x0) / span
	var P := PackedVector2Array(); var Nn := PackedVector2Array(); var S := PackedFloat32Array()
	for i in WN + 1:
		var u := lerpf(sa, sb, float(i) / float(WN)); var uc := clampf(u, 0.0, 1.0)
		var env := 0.55 + 0.45 * smoothstep(0.0, 0.25, uc)
		P.append(Vector2(ctr.x - Lpx * 0.5 + u * Lpx, ctr.y + amp * env * sin(TAU * 1.5 * uc - ph))); S.append(u)
	for i in WN + 1:
		var tg := (P[mini(i + 1, WN)] - P[maxi(i - 1, 0)]).normalized(); Nn.append(Vector2(-tg.y, tg.x))
	# the engraved worm laid along the crawling centreline: a textured strip, quad by quad
	for i in WN:
		var u0 := (x0 + S[i] * span - float(cr[0])) / float(cr[2]); var u1 := (x0 + S[i + 1] * span - float(cr[0])) / float(cr[2])
		_wq[0] = P[i] - Nn[i] * half; _wq[1] = P[i + 1] - Nn[i + 1] * half; _wq[2] = P[i + 1] + Nn[i + 1] * half; _wq[3] = P[i] + Nn[i] * half
		_wu[0] = Vector2(u0, 0.0); _wu[1] = Vector2(u1, 0.0); _wu[2] = Vector2(u1, 1.0); _wu[3] = Vector2(u0, 1.0)
		c.draw_primitive(_wq, _wc, _wu, tx)
	var at := func(u: float, off: float) -> Vector2:
		var fi := clampf((u - sa) / (sb - sa), 0.0, 1.0) * float(WN); var i := mini(int(fi), WN - 1); var a := fi - float(i)
		var pr := float(prof[clampi(int(roundf(u * 100.0)), 0, 100)]) * k
		return P[i].lerp(P[i + 1], a) + Nn[i].lerp(Nn[i + 1], a) * off * pr
	# motor output: the ventral cord stained madder plum with the motor neurons' firing
	var mo := norm(s, "motor", maxf(rev, fwd), 20.0)
	if mo > 0.03:
		var vc := PackedVector2Array()
		for q in 25: vc.append(at.call(lerpf(0.12, 0.88, float(q) / 24.0), 0.84))
		c.draw_polyline(vc, Color(Kit.MOTOR, 0.25 + 0.6 * mo), 1.5 + 2.0 * mo, true)
	# the touches, from the task's schedule (and the sensors' values): the touch cells of that end light, a hair touches the body there
	var head := false; var tail := false
	for ch in Campaign.info(str(t.task.id()))["sensors"]:
		var nm := str(ch).to_lower(); var on := ev_on(t, [nm]) or inp(t, str(ch)) > 0.0
		if not on: continue
		if nm.contains("tail") or nm.contains("fwd"): tail = true
		elif nm.contains("head") or nm.contains("rev") or nm == "touch": head = true
	for end in [["ALM", "AVM", head, 0.035, "touch"], ["PLM", "PVM", tail, 0.95, "touch"]]:
		var cells: Dictionary = A["cells"]
		for nm in [end[0], end[1]]:
			var p: Vector2 = at.call(float(cells[nm][0]), float(cells[nm][1]))
			if bool(end[2]): Gfx.wash(c, p, maxf(half * 1.4, 8.0), Kit.SENSOR, 0.55, 3)
			c.draw_circle(p, maxf(half * 0.16, 2.2), Color(Kit.SENSOR, 1.0 if bool(end[2]) else 0.55))
		if bool(end[2]):
			var tp: Vector2 = at.call(float(end[3]), -1.0); var hair := tp + Vector2(10.0, -maxf(half * 2.6, 26.0))
			c.draw_line(hair, tp + Vector2(0, -2), Kit.INK, 1.4, true); c.draw_circle(hair, 2.0, Kit.INK)
			if not sm: tag(c, "touch", hair + Vector2(6, 2), Kit.SENSOR)
	# where it is going: an engraved arrow under the worm, its length the speed
	var ay := minf(ctr.y + amp + half + float(f) + 4.0, dish.end.y - 6.0)
	var word := "at rest"
	if absf(v) > 0.08:
		var dirx := -signf(v); var a0 := Vector2(ctr.x - dirx * 10.0, ay); var a1 := a0 + Vector2(dirx * (24.0 + 60.0 * absf(v)), 0)
		c.draw_line(a0, a1, Kit.MOTOR, 2.0, true); Gfx.arrow_head(c, a1, Vector2(dirx, 0), Kit.MOTOR, 9.0)
		word = "crawling forward" if v > 0.0 else "backing up"
	if absf(v) <= 0.08: c.draw_string(Gfx.serif_italic(), Vector2(ctr.x - 80.0, ay + 5.0), word, HORIZONTAL_ALIGNMENT_CENTER, 160.0, f, Kit.MOTOR)
	elif v > 0.0: c.draw_string(Gfx.serif_italic(), Vector2(ctr.x + 18.0, ay + 5.0), word, HORIZONTAL_ALIGNMENT_LEFT, 160.0, f, Kit.MOTOR)
	else: c.draw_string(Gfx.serif_italic(), Vector2(ctr.x - 178.0, ay + 5.0), word, HORIZONTAL_ALIGNMENT_RIGHT, 160.0, f, Kit.MOTOR)
	if not sm:
		tag(c, "head", at.call(0.0, 0.0) + Vector2(-4, -half - 8.0), Kit.TEXT_SOFT, HORIZONTAL_ALIGNMENT_CENTER)
		fig(c, Vector2(box.position.x, box.end.y - 4.0), 1, "C. elegans: it crawls with its motor output.", box.size.x)
	var y := col.position.y + BodyViews._banner(c, t, col)
	bands(c, t, Rect2(col.position.x, y, col.size.x, col.end.y - y))

# ================================================================ fly compass (c_ring, c_shift, fly_compass, dx_compass)

static var _wedge := PackedVector2Array()
static func _compass(c: Control, t: Telemetry, r: Rect2) -> void:
	var d := BodyViews.ro(t); var sm := small(r); var f := fz()
	if d.is_empty(): BodyViews._empty(c, r, "No readouts from this task yet."); return
	var parts := split(r, 0.44 if not sm else 0.52); var box: Rect2 = parts[0]; var col: Rect2 = parts[1]
	var cap_h := 0.0 if sm else float(f) + 8.0
	var area := Rect2(box.position, Vector2(box.size.x, box.size.y - cap_h))
	var ro := maxf(minf(area.size.x * 0.5 - 30.0, area.size.y * 0.5 - 16.0), 26.0); var ri := ro * 0.40; var cc := area.get_center()
	var ang := func(deg: float) -> float: return deg_to_rad(deg) - PI * 0.5
	# the ring cells: the heading channel's cells, in circuit order round the ring (16 wedges; a larger ring shares a wedge between neighbours)
	var cells := out_cells(t, "heading"); var n := cells.size()
	var wact := PackedFloat32Array(); wact.resize(16)
	if n > 0:
		var cnt := PackedFloat32Array(); cnt.resize(16)
		for i in n:
			var w := int(float(i) * 16.0 / float(n)) % 16; wact[w] += cell_act(t, cells[i], 90.0); cnt[w] += 1.0
		for w in 16: wact[w] = clampf(wact[w] / maxf(cnt[w], 1.0), 0.0, 1.0)
	c.draw_circle(cc, ro + 2.0, Color(Anatomy.TISSUE, 1.0))
	for w in 16:   # a wedge is washed sienna by its cells' firing
		if wact[w] < 0.03: continue
		var a0: float = ang.call(22.5 * float(w)); var a1 := a0 + TAU / 16.0
		_wedge.clear()
		for q in 7: var a := lerpf(a0, a1, float(q) / 6.0); _wedge.append(cc + Vector2(cos(a), sin(a)) * ro)
		for q in 7: var a := lerpf(a1, a0, float(q) / 6.0); _wedge.append(cc + Vector2(cos(a), sin(a)) * ri)
		c.draw_colored_polygon(_wedge, Color(Kit.exc(), 0.12 + 0.68 * wact[w]))
	Gfx.stipple_disc(c, cc, ro - 2.0, 160, Color(Anatomy.BONE, 0.22), 6, 1.0, 1.3)
	for w in 16:   # the engraved spokes: tiles (pairs of wedges) heavier
		var a: float = ang.call(22.5 * float(w)); var dv := Vector2(cos(a), sin(a))
		c.draw_line(cc + dv * ri, cc + dv * ro, Color(Anatomy.BONE, 0.8 if w % 2 == 0 else 0.35), 1.3 if w % 2 == 0 else 0.8, true)
	c.draw_arc(cc, ro, 0.0, TAU, 96, Color(Anatomy.BONE, 0.9), 1.8, true)
	c.draw_arc(cc, (ro + ri) * 0.5, 0.0, TAU, 72, Color(Anatomy.BONE, 0.25), 0.8, true)
	c.draw_circle(cc, ri, Gfx.SHEET); c.draw_arc(cc, ri, 0.0, TAU, 64, Color(Anatomy.BONE, 0.85), 1.4, true)
	# the fly in the canal, turning with its true heading (0 = up, clockwise)
	var tdeg := BodyViews.rv(d, "true_deg"); var fl := tex("fly")
	if fl != null:
		var s := ri * 1.5 / float(fl.get_height())
		c.draw_set_transform(cc, deg_to_rad(tdeg), Vector2(s, s))
		c.draw_texture(fl, -fl.get_size() * 0.5)
		c.draw_set_transform(Vector2.ZERO, 0.0, Vector2.ONE)
	var ta: float = ang.call(tdeg); var td := Vector2(cos(ta), sin(ta))
	c.draw_line(cc + td * (ro + 3.0), cc + td * (ro + 13.0), Kit.INK, 2.4, true)   # the true heading on the rim
	# the landmark: a verdigris tick outside the rim while the lights are on
	var dark := BodyViews.rv(d, "dark") > 0.5
	if not dark and d.has("landmark_deg"):
		var la: float = ang.call(BodyViews.rv(d, "landmark_deg")); var ld := Vector2(cos(la), sin(la))
		c.draw_line(cc + ld * (ro + 4.0), cc + ld * (ro + 18.0), Kit.SENSOR, 3.0, true)
		Gfx.arrow_head(c, cc + ld * (ro + 4.0), -ld, Kit.SENSOR, 8.0)
		if not sm: tag(c, "landmark", cc + ld * (ro + 22.0) + Vector2(-26 if ld.x < -0.3 else 0, 4), Kit.SENSOR)
	# the bump: a gamboge needle across the ring, its reach the bump's strength
	var st := clampf(BodyViews.rv(d, "bump_strength") / 0.8, 0.0, 1.0); var bdeg := BodyViews.rv(d, "bump_deg")
	if st > 0.02:
		var ba: float = ang.call(bdeg); var bd := Vector2(cos(ba), sin(ba)); var tip := cc + bd * (ri + (ro + 6.0 - ri) * st)
		c.draw_line(cc + bd * (ri - 2.0), tip, Color(Gfx.SHEET, 0.9), 5.5, true)
		c.draw_line(cc + bd * (ri - 2.0), tip, Kit.GAMBOGE, 3.0, true); Gfx.arrow_head(c, tip + bd * 4.0, bd, Kit.GAMBOGE, 10.0)
	if not sm: fig(c, Vector2(box.position.x, box.end.y - 4.0), 1, "The ellipsoid body; gamboge: the bump.", box.size.x)
	# the numbers: decoded heading, true heading, error
	var y := col.position.y + BodyViews._banner(c, t, col); var cw := col.size.x / 3.0
	var err := absf(wrapf(bdeg - tdeg, -180.0, 180.0))
	y += readout(c, Vector2(col.position.x, y), "bump", ("%d°" % int(roundf(fposmod(bdeg, 360.0)))) if st > 0.02 else "—", Color("#8A6410"), cw - 6.0)
	readout(c, Vector2(col.position.x + cw, y - (float(f) + Kit.fs(22) + 8.0)), "heading", "%d°" % int(roundf(fposmod(tdeg, 360.0))), Kit.TEXT, cw - 6.0)
	readout(c, Vector2(col.position.x + cw * 2.0, y - (float(f) + Kit.fs(22) + 8.0)), "error", ("%d°" % int(roundf(err))) if st > 0.02 else "no bump", Kit.RED if st > 0.02 and err > 45.0 else Kit.TEXT, cw - 6.0)
	var nb := (Campaign.info(str(t.task.id()))["bands"] as Array).size(); var gh := 0.0 if nb == 0 else minf(74.0 * float(nb), (col.end.y - y) * 0.5)
	var tr := Rect2(col.position.x, y + 4.0, col.size.x, col.end.y - y - gh - 8.0)
	if tr.size.y > 44.0: _heading_trace(c, t, tr)
	bands(c, t, Rect2(col.position.x, col.end.y - gh, col.size.x, gh))

## Heading (ink) and bump (gamboge) over the last 8 s, unwrapped, from the readout history.
static func _heading_trace(c: Control, t: Telemetry, r: Rect2) -> void:
	var f := fz(); fig(c, r.position + Vector2(0, f), 2, "Heading and bump, last 8 s.", r.size.x)
	var P := Rect2(r.position.x, r.position.y + f + 6.0, r.size.x, r.size.y - f - 8.0)
	var ht := t.hist_t; var hd: PackedFloat32Array = t.hist.get("true_deg", PackedFloat32Array()); var hb: PackedFloat32Array = t.hist.get("bump_deg", PackedFloat32Array())
	var hs: PackedFloat32Array = t.hist.get("bump_strength", PackedFloat32Array())
	c.draw_line(Vector2(P.position.x, P.end.y), Vector2(P.end.x, P.end.y), Color(Kit.INK, 0.3), 1.0)
	if ht.size() < 2 or hd.size() != ht.size(): return
	var t0 := t.t_ms - 8000.0; var i0 := ht.size() - 1
	while i0 > 0 and ht[i0 - 1] >= t0: i0 -= 1
	var ud := PackedFloat32Array(); var ub := PackedFloat32Array(); var pd := 0.0; var pb := 0.0
	for i in range(i0, ht.size()):
		var a := hd[i]; var b := hb[i] if i < hb.size() else a
		if ud.is_empty(): pd = a; pb = a + wrapf(b - a, -180.0, 180.0)
		else:
			pd += wrapf(a - fposmod(pd, 360.0), -180.0, 180.0); pb += wrapf(b - fposmod(pb, 360.0), -180.0, 180.0)
		ud.append(pd); ub.append(pb)
	var lo := ud[0]; var hi := ud[0]
	for q in ud: lo = minf(lo, q); hi = maxf(hi, q)
	for q in ub: lo = minf(lo, q); hi = maxf(hi, q)
	var mid := (lo + hi) * 0.5; var span := maxf(hi - lo, 90.0) * 1.15
	var lb := PackedVector2Array(); var lh := PackedVector2Array()
	for k in ud.size():
		var x := P.position.x + P.size.x * (ht[i0 + k] - t0) / 8000.0
		lh.append(Vector2(x, P.get_center().y - (ud[k] - mid) / span * P.size.y))
		var ok := (i0 + k) < hs.size() and hs[i0 + k] > 0.02
		if ok: lb.append(Vector2(x, P.get_center().y - (ub[k] - mid) / span * P.size.y))
		elif lb.size() > 1: c.draw_polyline(lb, Kit.GAMBOGE, 2.0, true); lb.clear()
		else: lb.clear()
	if lh.size() > 1: c.draw_polyline(lh, Kit.INK, 1.3, true)
	if lb.size() > 1: c.draw_polyline(lb, Kit.GAMBOGE, 2.0, true)
	if lh.size() > 0: c.draw_string(Gfx.serif_italic(), Vector2(P.position.x, P.end.y - 4.0), "ink: heading", HORIZONTAL_ALIGNMENT_LEFT, 120.0, f, Kit.TEXT)
	c.draw_string(Gfx.serif_italic(), Vector2(P.position.x + 96.0, P.end.y - 4.0), "gamboge: bump", HORIZONTAL_ALIGNMENT_LEFT, 120.0, f, Color("#8A6410"))

# ================================================================ crab (s_pacemaker, s_phase, crab_pyloric, dx_crab)

const CRAB_CELLS := [["AB", Vector2(-0.30, -0.32)], ["PD", Vector2(0.30, -0.32)], ["LP", Vector2(-0.30, 0.34)], ["PY", Vector2(0.30, 0.34)]]

## The rows of a crab level's record: the motor channels in the order of the rhythm, and a sensor that plays a cell (s_phase drives 'pd' from outside) as its schedule.
static func _crab_rows(t: Telemetry) -> Array:
	var rows := []; var ms := motors(t); var sens: Array = Campaign.info(str(t.task.id()))["sensors"]
	for nm in ["ab", "pd", "lp", "py"]:
		if ms.has(nm): rows.append({"label": nm.to_upper(), "times": times(t, nm), "col": Kit.INK, "ch": nm})
		elif sens.has(nm):
			var sp := []
			for e in ev_list(t, nm): sp.append([e[0], e[1]])
			rows.append({"label": nm.to_upper() + " (driven)", "times": PackedFloat32Array(), "spans": sp, "span_col": Kit.SENSOR, "ch": "", "ev": nm})
	return rows

static func _crab(c: Control, t: Telemetry, r: Rect2) -> void:
	var sm := small(r); var f := fz(); var s := state(t, "crab")
	var parts := split(r, 0.50 if not sm else 0.56); var box: Rect2 = parts[0]; var col: Rect2 = parts[1]
	var cap_h := 0.0 if sm else float(f) + 8.0
	var L := fit("crab", Rect2(box.position, Vector2(box.size.x * 0.80, box.size.y - cap_h)), 0.0, 0.5)
	if L.is_empty(): BodyViews._empty(c, r, "The crab specimen is missing (tools/art/specimens.py)."); return
	layer(c, "crab", L)
	var A: Dictionary = L["a"]; var k := float(L["k"])
	# the cells of the pyloric circuit, by population (or the channel that reads them), and how hard each fires now
	var acts := {}; var tot := 0.0
	for cl in CRAB_CELLS:
		var nm: String = cl[0]; var ids := pop_cells(t, [nm]); var hz := 0.0
		if not ids.is_empty(): hz = cells_rate(t, ids, 160.0)
		elif has_out(t, nm.to_lower()): hz = rate(t, nm.to_lower())
		elif ev_on(t, [nm.to_lower()]): hz = 20.0
		var a := norm(s, nm, hz, 15.0) if hz > 0.0 else 0.0
		acts[nm] = a; tot = maxf(tot, a)
	# the STG on the stomach, stained with the ganglion's activity, the pyloric nerves with it
	var stg := mp(L, A["stg"]); var dvn := mpl(L, A["dvn"])
	if dvn.size() > 1: c.draw_polyline(dvn, Color(Kit.exc(), 0.4 + 0.5 * tot), 1.5 + 1.5 * tot, true)
	Gfx.wash(c, stg, 46.0 * k * (1.0 + 0.6 * tot), Kit.exc(), 0.15 + 0.5 * tot, 4)
	Gfx.ink_poly(c, Gfx.ellipse(stg, maxf(22.0 * k, 5.0), maxf(16.0 * k, 3.6), 24), Color("#E9C9A0"), Kit.exc(), 1.3)
	# the magnifier: the ganglion drawn large, its four kinds of cell (inhibitory: Prussian blue) washed when they fire
	var mr := minf(box.size.y * 0.30, box.size.x * 0.22); var mc := Vector2(box.end.x - mr - 4.0, box.position.y + mr + 6.0)
	var dv := (mc - stg).normalized(); var nv := dv.orthogonal()
	for sg in [-1.0, 1.0]: c.draw_line(stg + nv * sg * 6.0 * maxf(k * 3.0, 1.0), mc - dv * mr * 0.0 + nv * sg * mr * 0.96 - dv * mr * 0.25, Color(Kit.INK, 0.35), 1.0, true)
	c.draw_circle(mc, mr + 3.0, Color(Gfx.SHEET, 0.96)); c.draw_arc(mc, mr + 3.0, 0, TAU, 72, Kit.INK, 1.6, true); c.draw_arc(mc, mr - 1.0, 0, TAU, 72, Color(Kit.INK, 0.35), 0.8, true)
	Gfx.stipple_disc(c, mc, mr * 0.95, 70, Color(NUC, 0.18), 9, 1.0, 1.4)
	var cr := mr * 0.17
	for cl in CRAB_CELLS:
		var nm: String = cl[0]; var p: Vector2 = mc + (cl[1] as Vector2) * mr; var a := float(acts[nm])
		var known := not pop_cells(t, [nm]).is_empty() or has_out(t, nm.to_lower()) or (Campaign.info(str(t.task.id()))["sensors"] as Array).has(nm.to_lower())
		if a > 0.03: Gfx.wash(c, p, cr * (1.6 + 1.0 * a), Kit.inh(), 0.25 + 0.6 * a, nm.length() + int(p.x))
		Gfx.soma(c, p, cr, Kit.inh() if known else Color(Kit.inh(), 0.0).lerp(Gfx.SHEET, 0.7), int(p.x), 0.0, 0.0 if known else 0.7)
		var lx := p.x - cr * 1.25 - 40.0 if (cl[1] as Vector2).x < 0.0 else p.x + cr * 1.25
		c.draw_string(Fonts.bold, Vector2(lx, p.y + 5.0), nm, HORIZONTAL_ALIGNMENT_RIGHT if (cl[1] as Vector2).x < 0.0 else HORIZONTAL_ALIGNMENT_LEFT, 40.0, Kit.fs(12), Kit.TEXT if known else Kit.TEXT_SOFT)
	if not sm:
		tag(c, "STG", mc + Vector2(-mr * 0.25, mr + float(f) + 4.0), Kit.EXC_INK, HORIZONTAL_ALIGNMENT_CENTER)
		fig(c, Vector2(box.position.x, box.end.y - 4.0), 1, "Cancer borealis; the STG magnified.", box.size.x)
	# Fig. 3: the triphasic rhythm as a pen record, cycle starts (PD onsets) marked in gamboge, and the phases
	var y := col.position.y + BodyViews._banner(c, t, col)
	var rows := _crab_rows(t)
	var nb := (Campaign.info(str(t.task.id()))["bands"] as Array).size(); var gh := 0.0 if nb == 0 else minf(76.0, (col.end.y - y) * 0.36)
	var rec_top := y + (float(f) + 6.0 if not sm else 0.0)
	var rec := Rect2(col.position.x, rec_top, col.size.x, col.end.y - gh - rec_top - float(f) - 10.0)
	if not sm: fig(c, Vector2(col.position.x, y + f), 3, "The triphasic rhythm; gamboge: cycle starts.", col.size.x)
	var win := 3000.0
	var rr := record(c, rec, rows, t.t_ms, win)
	# cycle starts: the PD bursts (or the driven PD schedule)
	var pd_on := PackedFloat32Array()
	for row in rows:
		if str(row["label"]).begins_with("PD"):
			if str(row.get("ch", "")) != "": pd_on = onsets(row["times"], t.t_ms - 9000.0, 150.0)
			else:
				for e in ev_list(t, str(row["ev"])):
					if e[0] <= t.t_ms: pd_on.append(e[0])
	if not rr.is_empty() and not pd_on.is_empty():
		var top: Rect2 = rr[0]; var bot: Rect2 = rr[rr.size() - 1]
		for o in pd_on:
			if o < t.t_ms - win: continue
			var x := top.position.x + top.size.x * (o - (t.t_ms - win)) / win
			Gfx.dashed(c, PackedVector2Array([Vector2(x, top.position.y - 2.0), Vector2(x, bot.end.y)]), Color(Kit.GAMBOGE, 0.8), 1.2, 4.0, 3.0)
	# phases of LP and PY in the PD cycle
	var txt := ""
	if pd_on.size() >= 2:
		var per := (pd_on[pd_on.size() - 1] - pd_on[maxi(0, pd_on.size() - 4)]) / float(mini(3, pd_on.size() - 1))
		txt = "period %.2f s" % (per * 0.001); var ph := ""
		for nm in ["lp", "py"]:
			if not motors(t).has(nm): continue
			var on := onsets(times(t, nm), t.t_ms - 9000.0, 150.0)
			if on.is_empty(): continue
			var o := on[on.size() - 1]; var p0 := -1.0
			for q in pd_on:
				if q <= o: p0 = q
			if p0 >= 0.0 and per > 1.0: ph += ("" if ph == "" else ", ") + "%s %.2f" % [nm.to_upper(), fposmod((o - p0) / per, 1.0)]
		if ph != "": txt += "   phase " + ph
	if txt != "": c.draw_string(Gfx.mono(), Vector2(col.position.x, rec.end.y + float(f) + 4.0), txt, HORIZONTAL_ALIGNMENT_LEFT, col.size.x, f, Kit.TEXT)
	bands(c, t, Rect2(col.position.x, col.end.y - gh, col.size.x, gh), true)

# ================================================================ leech (l_burst, l_rebound, leech_heart, dx_leech)

static func _leech(c: Control, t: Telemetry, r: Rect2) -> void:
	var sm := small(r); var f := fz(); var s := state(t, "leech")
	var parts := split(r, 0.42 if not sm else 0.48); var box: Rect2 = parts[0]; var col: Rect2 = parts[1]
	var cap_h := 0.0 if sm else float(f) + 8.0
	var L := fit("leech", Rect2(box.position + Vector2(0, 4), Vector2(box.size.x, box.size.y - cap_h - 4.0)))
	if L.is_empty(): BodyViews._empty(c, r, "The leech specimen is missing (tools/art/specimens.py)."); return
	layer(c, "leech", L)
	var A: Dictionary = L["a"]; var k := float(L["k"]); var ms := motors(t)
	# the two heart interneurons' channels (left, right; a one-cell level has one) and the heart motor neurons they inhibit, where the circuit has them
	var side_ch := {"L": "left" if ms.has("left") else (str(ms[0]) if ms.size() == 1 else ""), "R": "right" if ms.has("right") else ""}
	var hn := {}; var beat := {}
	for sd in ["L", "R"]:
		var ch: String = side_ch[sd]
		hn[sd] = norm(s, "hn" + sd, rate(t, ch), 10.0) if ch != "" else 0.0
		var he := pop_cells(t, ["HE_" + sd])
		beat[sd] = norm(s, "he" + sd, cells_rate(t, he, 160.0), 10.0) if not he.is_empty() else float(hn[sd])
	# heart tubes: tubes[0] lies on the animal's right (the top: dorsal view, head left), tubes[1] on its left. A beating tube constricts: narrower and darker.
	var tubes: Array = A["tubes"]; var bul: Array = A["bulges"]
	for q in 2:
		var sd := "R" if q == 0 else "L"; var b := float(beat[sd])
		var pts := mpl(L, tubes[q][0]); var w := maxf(5.5 * k * 2.6, 2.5) * (1.15 - 0.6 * b)
		c.draw_polyline(pts, Color(WALL, 0.45 + 0.5 * b), w + 1.4, true); c.draw_polyline(pts, Color(WALL_PALE.lerp(WALL, b), 0.9), maxf(w - 1.2, 1.0), true)
		for p in bul[q]: c.draw_circle(mp(L, p), maxf(w * 0.75, 1.6), Color(WALL, 0.55 + 0.4 * b))
		if not sm: tag(c, ("right" if q == 0 else "left") + " heart tube", pts[int(pts.size() * 0.55)] + Vector2(0, -10.0 if q == 0 else 22.0), WALL, HORIZONTAL_ALIGNMENT_CENTER)
	# the heart interneurons in ganglion 4: Prussian blue (inhibitory), washed when they burst
	var g4 := mp(L, A["hn"][1]); var off := Vector2(0, maxf(float(A["hn_r"]) * k * 1.4, 4.0))
	for sd in ["R", "L"]:
		var p := g4 + (off * (-1.0 if sd == "R" else 1.0)); var a := float(hn[sd])
		if side_ch[sd] == "": continue
		if a > 0.03: Gfx.wash(c, p, 10.0 + 12.0 * a, Kit.inh(), 0.3 + 0.6 * a, 7 if sd == "L" else 8)
		c.draw_circle(p, 3.6, Kit.inh()); c.draw_arc(p, 3.6, 0, TAU, 16, Kit.INK, 1.0, true)
	if not sm:
		tag(c, "HN(4)", g4 + Vector2(-18, -16), Kit.inh(), HORIZONTAL_ALIGNMENT_RIGHT)
		fig(c, Vector2(box.position.x, box.end.y - 4.0), 1, "Hirudo: the heart tubes beat in turn.", box.size.x)
	# Fig. 2: the bursts as a pen record, and the period and phase
	var y := col.position.y + BodyViews._banner(c, t, col)
	var rows := []
	for sd in ["L", "R"]:
		if side_ch[sd] != "": rows.append({"label": ("HN(L)" if sd == "L" else "HN(R)") if ms.size() > 1 else str(side_ch[sd]), "times": times(t, side_ch[sd]), "col": Kit.INK})
	var dur := maxf(t.duration(), 1.0)
	for sn in Campaign.info(str(t.task.id()))["sensors"]:
		var sp := []
		for e in ev_list(t, str(sn).to_lower()):
			if float(e[1]) - float(e[0]) < 0.8 * dur: sp.append([e[0], e[1]])   # a drive that is on all run tells nothing in a record
		if not sp.is_empty(): rows.append({"label": str(sn), "times": PackedFloat32Array(), "spans": sp, "span_col": Kit.inh() if str(sn).contains("inhib") else Kit.SENSOR})
	var nb := (Campaign.info(str(t.task.id()))["bands"] as Array).size(); var gh := 0.0 if nb == 0 else minf(76.0, (col.end.y - y) * 0.36)
	var top := y + (float(f) + 6.0 if not sm else 0.0)
	if not sm: fig(c, Vector2(col.position.x, y + f), 2, "The bursts, last 4 s.", col.size.x)
	var rec := Rect2(col.position.x, top, col.size.x, col.end.y - gh - top - float(f) - 10.0)
	record(c, rec, rows, t.t_ms, 4000.0)
	var txt := ""
	if side_ch["L"] != "":
		var o0 := onsets(times(t, side_ch["L"]), t.t_ms - 12000.0, 300.0)
		if o0.size() >= 2:
			var per := (o0[o0.size() - 1] - o0[maxi(0, o0.size() - 4)]) / float(mini(3, o0.size() - 1)); txt = "period %.2f s" % (per * 0.001)
			if side_ch["R"] != "":
				var o1 := onsets(times(t, side_ch["R"]), t.t_ms - 12000.0, 300.0)
				if not o1.is_empty():
					var o := o1[o1.size() - 1]; var p0 := -1.0
					for q in o0:
						if q <= o: p0 = q
					if p0 >= 0.0: txt += "   phase of R %.2f" % fposmod((o - p0) / per, 1.0)
	if txt != "": c.draw_string(Gfx.mono(), Vector2(col.position.x, rec.end.y + float(f) + 4.0), txt, HORIZONTAL_ALIGNMENT_LEFT, col.size.x, f, Kit.TEXT)
	bands(c, t, Rect2(col.position.x, col.end.y - gh, col.size.x, gh), true)

# ================================================================ aplysia (a_depress, a_facilitate, aplysia_gill, dx_aplysia)

static var _leaf := PackedVector2Array()
static func _aplysia(c: Control, t: Telemetry, r: Rect2) -> void:
	var sm := small(r); var f := fz(); var s := state(t, "aplysia")
	var parts := split(r, 0.36 if not sm else 0.42); var box: Rect2 = parts[0]; var col: Rect2 = parts[1]
	var cap_h := 0.0 if sm else float(f) + 8.0
	var L := fit("aplysia", Rect2(box.position + Vector2(0, 8), Vector2(box.size.x, box.size.y - cap_h - 8.0)))
	if L.is_empty(): BodyViews._empty(c, r, "The sea slug specimen is missing (tools/art/specimens.py)."); return
	layer(c, "aplysia", L)
	var A: Dictionary = L["a"]; var k := float(L["k"])
	var g := norm(s, "gill", rate(t, "gill"), 15.0)
	# the gill withdraws in proportion to the motor cell's firing: the plume pulls in toward its base, its leaflets shorten and darken
	var ax := mpl(L, A["gill"]); var base := ax[0]; var shr := 1.0 - 0.6 * g
	if g > 0.05: Gfx.dashed(c, PackedVector2Array([base, ax[ax.size() - 1]]), Color(WALL, 0.55), 1.2, 4.0, 4.0)   # where the gill reaches at rest
	_leaf.clear()
	for q in 34:
		var u := 0.03 + 0.94 * float(q) / 33.0; var fi := u * float(ax.size() - 1); var i := mini(int(fi), ax.size() - 2)
		var p := ax[i].lerp(ax[i + 1], fi - float(i)); var ctr := base + (p - base) * shr
		var tg := (ax[i + 1] - ax[i]).normalized(); var nr := tg.orthogonal()
		var ln := 62.0 * k * (1.0 - 0.65 * u) * (0.85 + 0.15 * sin(float(q) * 1.7)) * (1.0 - 0.6 * g)
		for sg in [-1.0, 1.0]:
			var tip: Vector2 = ctr + nr * sg * ln + tg * 0.03 * 380.0 * k * shr; var mid: Vector2 = (ctr + tip) * 0.5 + tg * 7.0 * k
			_leaf.append(ctr); _leaf.append(mid); _leaf.append(mid); _leaf.append(tip)
	c.draw_multiline(_leaf, Color(WALL, 0.6 + 0.35 * g), maxf(3.0 * k * 2.2, 1.6))
	c.draw_multiline(_leaf, Color(WALL_PALE.lerp(WALL, 0.4 * g), 0.9), maxf(1.1 * k * 2.2, 0.8))
	c.draw_line(base, base + (ax[ax.size() - 1] - base) * shr, WALL, maxf(3.0 * k * 2.4, 2.0), true)
	# the branchial nerve (motor, madder plum) and the abdominal ganglion, stained with the motor cell's firing
	var bran := mpl(L, A["bran"]); var abd := mp(L, A["abd"])
	c.draw_polyline(bran, Color(Kit.MOTOR, 0.35 + 0.6 * g), 1.4 + 1.6 * g, true)
	Gfx.wash(c, abd, 56.0 * k * (1.0 + 0.5 * g), Kit.exc(), 0.1 + 0.45 * g, 9)
	# serotonin from the facilitator (a gamboge wash on the ganglion) and the tail shock (a gamboge mark at the tail)
	if has_out(t, "serotonin"):
		var se := norm(s, "5ht", rate(t, "serotonin"), 10.0)
		if se > 0.03: Gfx.wash(c, abd + Vector2(8, -6), 40.0 * k + 10.0 * se, Kit.GAMBOGE, 0.25 + 0.5 * se, 13)
	var tail := mp(L, A["tail"])
	if ev_on(t, ["shock"]) or inp(t, "shock") > 0.0:
		Gfx.wash(c, tail, 26.0, Kit.GAMBOGE, 0.6, 5)
		var z := PackedVector2Array([tail + Vector2(-6, -40), tail + Vector2(4, -24), tail + Vector2(-4, -20), tail + Vector2(6, -4)])
		c.draw_polyline(z, Color("#8A6410"), 2.4, true); Gfx.arrow_head(c, tail + Vector2(6, -2), Vector2(0.3, 1), Color("#8A6410"), 8.0)
		if not sm: tag(c, "shock to the tail", tail + Vector2(12, -30), Color("#8A6410"))
	# the touch on the siphon: a hair, the siphon nerve lit verdigris
	if ev_on(t, ["touch"]) or inp(t, "touch") > 0.0:
		var sp := mp(L, A["siphon_tip"]); var hair := sp + Vector2(-26, -22)
		Gfx.wash(c, sp, 16.0, Kit.SENSOR, 0.55, 2)
		c.draw_polyline(mpl(L, A["siph_n"]), Color(Kit.SENSOR, 0.9), 2.6, true)
		c.draw_line(hair, sp, Kit.INK, 1.4, true); c.draw_circle(hair, 2.0, Kit.INK)
		if not sm: tag(c, "touch", hair + Vector2(-6, -4), Kit.SENSOR, HORIZONTAL_ALIGNMENT_RIGHT)
	if not sm:
		tag(c, "gill", ax[0] + Vector2(8, -8), WALL)
		fig(c, Vector2(box.position.x, box.end.y - 4.0), 1, "Aplysia: the gill withdraws as the motor cell fires.", box.size.x)
	# Fig. 2: the withdrawal at each touch (mean motor rate over the touch), shocks marked between them
	var y := col.position.y + BodyViews._banner(c, t, col)
	var nb := (Campaign.info(str(t.task.id()))["bands"] as Array).size(); var gh := 0.0 if nb == 0 else minf(76.0, (col.end.y - y) * 0.34)
	var fr := Rect2(col.position.x, y, col.size.x, col.end.y - gh - y - 6.0)
	if fr.size.y > 50.0: _per_stim(c, t, fr, "touch", "gill", "shock", "The withdrawal at each touch.", 2, sm)
	bands(c, t, Rect2(col.position.x, col.end.y - gh, col.size.x, gh), true)

## Bars of a motor channel's mean rate during each presentation of a stimulus (events whose label contains `stim`), in order; events labelled with `mark`
## are drawn as gamboge marks between the bars (a shock, a punishment). `split_ab`: bars of "odour a" in sienna, others in ink.
static func _per_stim(c: Control, t: Telemetry, r: Rect2, stim: String, ch: String, mark: String, cap: String, num: int, sm: bool, split_ab := false) -> void:
	var f := fz(); var y0 := r.position.y
	if not sm: fig(c, Vector2(r.position.x, y0 + f), num, cap, r.size.x); y0 += float(f) + 8.0
	var ev := ev_list(t, stim); var mk := ev_list(t, mark) if mark != "" else []
	var P := Rect2(r.position.x + 4.0, y0 + 4.0, r.size.x - 8.0, r.end.y - y0 - float(f) - 10.0)
	c.draw_line(Vector2(P.position.x, P.end.y), Vector2(P.end.x, P.end.y), Kit.INK, 1.2)
	if ev.is_empty() or P.size.y < 16.0: return
	var ncell := maxi(out_cells(t, ch).size(), 1); var tm := times(t, ch)
	var vals := []; var hi := 1.0
	for e in ev:
		if float(e[0]) > t.t_ms: vals.append(-1.0); continue
		var a := float(e[0]); var b := minf(float(e[1]) + 250.0, t.t_ms); var cnt := 0
		for x in tm:
			if x >= a and x <= b: cnt += 1
		var v := float(cnt) / (maxf(b - a, 50.0) * 0.001 * float(ncell)); vals.append(v); hi = maxf(hi, v)
	var n := ev.size(); var bw := P.size.x / float(n)
	for i in n:
		var x := P.position.x + bw * (float(i) + 0.2); var w := bw * 0.6
		var lab := str(ev[i][2]); var colr := Kit.exc() if (not split_ab or lab.contains("odour a")) else Kit.INK
		if float(vals[i]) >= 0.0:
			var h := P.size.y * float(vals[i]) / hi
			var br := Rect2(x, P.end.y - h, w, h)
			c.draw_rect(br, Color(colr, 0.35)); Gfx.hatch(c, PackedVector2Array([br.position, Vector2(br.end.x, br.position.y), br.end, Vector2(br.position.x, br.end.y)]), 0.8, 4.0, Color(colr, 0.7), 1.0)
			c.draw_rect(br, Kit.INK, false, 1.0)
		else: c.draw_rect(Rect2(x, P.end.y - 2.0, w, 2.0), Color(Kit.INK, 0.25))
		var nm := str(i + 1) if not split_ab else ("A" if lab.contains("odour a") else "B")
		c.draw_string(Gfx.serif_italic(), Vector2(x, P.end.y + float(f) + 2.0), nm, HORIZONTAL_ALIGNMENT_CENTER, w, f, Kit.TEXT_SOFT)
		for m in mk:   # a mark event that starts before this presentation and after the previous one (or overlaps this one)
			var m0 := float(m[0]); var prev := float(ev[i - 1][1]) if i > 0 else -1.0e9
			if (m0 > prev and m0 <= float(ev[i][0])) or (m0 >= float(ev[i][0]) and m0 < float(ev[i][1])):
				var mx := x - bw * 0.2 if m0 < float(ev[i][0]) else x + w * 0.5; var my := P.position.y + 4.0
				c.draw_polyline(PackedVector2Array([Vector2(mx - 3, my), Vector2(mx + 3, my + 7), Vector2(mx - 2, my + 9), Vector2(mx + 4, my + 17)]), Color("#8A6410"), 2.0, true)

# ================================================================ fly mushroom body (k_sparse, k_depress, fly_mb, dx_mb_apl, dx_mb_dan)

static var _kc_pos := PackedVector2Array()
static var _kc_key := ""
## Kenyon-cell positions in a unit ellipse (a sunflower spiral: evenly spread, the same every frame), cached by count.
static func _kc_layout(n: int) -> PackedVector2Array:
	if _kc_key == str(n): return _kc_pos
	_kc_pos = PackedVector2Array(); _kc_key = str(n)
	for i in n:
		var rr := sqrt((float(i) + 0.5) / float(n)); var a := float(i) * 2.39996
		_kc_pos.append(Vector2(cos(a), sin(a)) * rr)
	return _kc_pos

## The Kenyon cells of the circuit: the 'kc' channel's cells, or the populations named KC... (k_depress has KCA and KCB).
static func _kc_cells(t: Telemetry) -> PackedInt32Array:
	var ids := out_cells(t, "kc")
	return ids if not ids.is_empty() else pop_cells(t, ["KC"])

## Per-cell tallies of spikes during odour A and odour B (the task's schedule), incremental.
static func _kc_tally(t: Telemetry, s: Dictionary, ids: PackedInt32Array) -> void:
	if not s.has("A") or (s["A"] as PackedInt32Array).size() != t.n:
		var z := PackedInt32Array(); z.resize(t.n); s["A"] = z; s["B"] = z.duplicate(); s["last"] = -1.0
		var m := PackedByteArray(); m.resize(t.n)
		for id in ids: m[id] = 1
		s["mask"] = m
	var oa := ev_on(t, ["odour a"]); var ob := ev_on(t, ["odour b"])
	var last := float(s["last"]); var nsp := t.sp_t.size(); var j := nsp - 1
	while j >= 0 and t.sp_t[j] > last: j -= 1
	if nsp > 0: s["last"] = t.sp_t[nsp - 1]
	if not (oa or ob): return
	var A: PackedInt32Array = s["A"]; var B: PackedInt32Array = s["B"]; var m: PackedByteArray = s["mask"]
	for i in range(j + 1, nsp):
		var id := t.sp_n[i]
		if id < m.size() and m[id] == 1:
			if oa: A[id] += 1
			else: B[id] += 1
	s["A"] = A; s["B"] = B

static func _fly_mb(c: Control, t: Telemetry, r: Rect2) -> void:
	var sm := small(r); var f := fz(); var s := state(t, "mb")
	var parts := split(r, 0.34 if not sm else 0.30); var box: Rect2 = parts[0]; var col: Rect2 = parts[1]
	var L := fit("fly_mb", Rect2(box.position, Vector2(box.size.x * 0.40, box.size.y - (0.0 if sm else float(f) + 8.0))), 0.0)
	if L.is_empty(): BodyViews._empty(c, r, "The mushroom body specimen is missing (tools/art/specimens.py)."); return
	layer(c, "fly_mb", L)
	var A: Dictionary = L["a"]; var k := float(L["k"])
	var ids := _kc_cells(t); _kc_tally(t, s, ids)
	var frac := 0.0
	for id in ids: frac += 1.0 if cell_act(t, id, 60.0) > 0.35 else 0.0
	frac /= maxf(float(ids.size()), 1.0)
	# the Kenyon-cell cap over the calyx (the cells themselves are in Fig. 2), the lobes stained with their activity
	var kcc := mp(L, A["kc_c"]); var kr: Array = A["kc_r"]
	var cap := Gfx.ellipse(kcc, float(kr[0]) * k, float(kr[1]) * k, 32)
	Gfx.ink_poly(c, cap, Color(KCF, 0.85), Color(Kit.exc(), 0.6), 1.0)
	Gfx.stipple_poly(c, cap, 40, Color(Kit.exc(), 0.5), 3, 2.0)
	var lob := clampf(frac * 4.0, 0.0, 1.0)
	if lob > 0.02:
		Gfx.wash(c, kcc, float(kr[0]) * k * 1.2, Kit.exc(), 0.2 + 0.6 * lob, 21)
		for key in ["vl", "hl"]: c.draw_polyline(mpl(L, A[key]), Color(Kit.exc(), 0.25 + 0.5 * lob), maxf(16.0 * k, 3.0), true)
	# dopamine cells (gamboge): lit during punishment (and by the DAN cells when the circuit has them)
	var dan := pop_cells(t, ["DAN", "PPL", "PAM"]); var da := norm(s, "dan", cells_rate(t, dan, 160.0), 10.0) if not dan.is_empty() else 0.0
	if ev_on(t, ["punish"]): da = maxf(da, 0.8)
	var dpath: Array = [A["ppl1_path"]] + (A["pam_paths"] as Array)
	for p in dpath: c.draw_polyline(mpl(L, p), Color(Kit.GAMBOGE, 0.35 + 0.6 * da), 1.2 + 1.5 * da, true)
	for p in [A["ppl1"]] + (A["pam"] as Array):
		var q := mp(L, p)
		if da > 0.05: Gfx.wash(c, q, 9.0 + 6.0 * da, Kit.GAMBOGE, 0.5 * da, 4)
		c.draw_circle(q, 3.2, Kit.GAMBOGE); c.draw_arc(q, 3.2, 0, TAU, 12, Kit.INK, 0.9, true)
	# the output neuron: an arrow out of the vertical lobe, as heavy as the 'avoid' output fires
	if has_out(t, "avoid"):
		var mo := norm(s, "avoid", rate(t, "avoid"), 10.0); var mb := mpl(L, A["mbon"])
		c.draw_polyline(mb, Color(Kit.exc(), 0.35 + 0.6 * mo), 1.2 + 2.6 * mo, true)
		Gfx.arrow_head(c, mb[mb.size() - 1], mb[mb.size() - 1] - mb[mb.size() - 2], Color(Kit.exc(), 0.35 + 0.6 * mo), 8.0 + 4.0 * mo)
		if not sm: tag(c, "output", mb[mb.size() - 1] + Vector2(2, -8), Kit.EXC_INK, HORIZONTAL_ALIGNMENT_CENTER)
	# Fig. 2: the Kenyon cells magnified, a field of points: lit when they fire; a sienna dot for the cells that fired for odour A, an ink dot for odour B
	var fx := Rect2(L["rect"].end.x + 8.0, box.position.y, box.end.x - L["rect"].end.x - 8.0, box.size.y - (0.0 if sm else float(f) + 8.0))
	if fx.size.x > 80.0 and not ids.is_empty():
		var y0 := fx.position.y
		if not sm: fig(c, Vector2(fx.position.x, y0 + f), 2, "The Kenyon cells, %d." % ids.size(), fx.size.x); y0 += float(f) + 6.0
		var leg_h := 0.0 if sm else float(f) * 3.0 + 14.0
		var E := Rect2(fx.position.x, y0, fx.size.x, fx.end.y - y0 - leg_h); var ec := E.get_center(); var ex := E.size.x * 0.48; var ey := E.size.y * 0.46
		Gfx.ink_poly(c, Gfx.ellipse(ec, ex + 6.0, ey + 6.0, 48), Color(KCF, 0.35), Color(Kit.exc(), 0.5), 1.2)
		leader_to(c, kcc, Vector2(E.position.x, ec.y))
		var pos := _kc_layout(ids.size()); var cr := clampf(sqrt(ex * ey / float(ids.size())) * 0.62, 1.6, 9.0)
		var TA: PackedInt32Array = s["A"]; var TB: PackedInt32Array = s["B"]; var na := 0; var nb := 0; var both := 0
		for i in ids.size():
			var id := ids[i]; var p := ec + Vector2(pos[i].x * ex, pos[i].y * ey); var a := cell_act(t, id, 70.0)
			c.draw_circle(p, cr, Color(Kit.exc(), 0.12 + 0.85 * a) if a > 0.05 else Color(KCF, 0.9))
			c.draw_arc(p, cr, 0, TAU, 10, Color(Kit.exc(), 0.45), 0.8, true)
			var ia := TA[id] > 0; var ib := TB[id] > 0
			if ia and ib: both += 1; c.draw_circle(p + Vector2(-cr * 0.3, 0), cr * 0.38, Kit.EXC_INK); c.draw_circle(p + Vector2(cr * 0.3, 0), cr * 0.38, Kit.INK)
			elif ia: na += 1; c.draw_circle(p, cr * 0.42, Kit.EXC_INK)
			elif ib: nb += 1; c.draw_circle(p, cr * 0.42, Kit.INK)
		if not sm:
			var ly := fx.end.y - float(f) * 2.0 - 10.0
			c.draw_circle(Vector2(fx.position.x + 5, ly - 4), 3.5, Kit.EXC_INK); c.draw_string(Gfx.serif_italic(), Vector2(fx.position.x + 14, ly), "fired for odour A: %d" % (na + both), HORIZONTAL_ALIGNMENT_LEFT, fx.size.x - 14.0, f, Kit.TEXT)
			c.draw_circle(Vector2(fx.position.x + 5, ly + float(f) + 1.0), 3.5, Kit.INK); c.draw_string(Gfx.serif_italic(), Vector2(fx.position.x + 14, ly + float(f) + 5.0), "for odour B: %d   (both: %d)" % [nb + both, both], HORIZONTAL_ALIGNMENT_LEFT, fx.size.x - 14.0, f, Kit.TEXT)
			c.draw_string(Gfx.serif_italic(), Vector2(fx.position.x, ly + float(f) * 2.0 + 10.0), "firing now: %d of %d" % [int(roundf(frac * float(ids.size()))), ids.size()], HORIZONTAL_ALIGNMENT_LEFT, fx.size.x, f, Kit.TEXT_SOFT)
	if not sm: fig(c, Vector2(box.position.x, box.end.y - 4.0), 1, "Drosophila: a mushroom body.", L["rect"].size.x + 60.0)
	# the right column: the output at each odour (where the level has an output), the gauges
	var y := col.position.y + BodyViews._banner(c, t, col)
	var nbd := (Campaign.info(str(t.task.id()))["bands"] as Array).size(); var gh := 0.0 if nbd == 0 else minf(76.0 * float(nbd), (col.end.y - y) * (0.36 if has_out(t, "avoid") else 1.0))
	if has_out(t, "avoid"):
		var fr := Rect2(col.position.x, y, col.size.x, col.end.y - gh - y - 6.0)
		if fr.size.y > 50.0: _per_stim(c, t, fr, "odour", "avoid", "punish", "The output at each odour.", 3, sm, true)
	bands(c, t, Rect2(col.position.x, col.end.y - gh, col.size.x, gh))

static func leader_to(c: Control, a: Vector2, b: Vector2) -> void:
	Gfx.dashed(c, PackedVector2Array([a, b]), Color(Kit.INK, 0.45), 1.0, 4.0, 3.0); c.draw_circle(a, 2.2, Color(Kit.INK, 0.6))
