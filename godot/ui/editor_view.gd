class_name CircuitEditor
extends Control
## The circuit editor and the live circuit view in one: neurons are glowing discs (amber excitatory, cyan inhibitory) that flash on a spike,
## projections are curved axons with pulses that travel at (a slowed-down) synaptic delay, sensors sit on the left and motors on the right.
## Drag a disc to move it, drag from its rim to another disc to connect, click an axon to edit it. Everything is drawn here, so it is crisp at any scale.
signal structure_changed          # populations / projections / channels added or removed: the lab must reload the engine
signal selection_changed(sel: Dictionary)
signal moved                       # editor positions changed (no reload needed)
signal lesion_clicked(h: Dictionary)   # lesion lab: a population, axon or sensor card was clicked

var tele: Telemetry = null
var circuit := {}
var sel := {}                      # {t:"pop", id} | {t:"proj", i} | {t:"in", name} | {t:"out", name}
var tool := "select"               # select | connect
var editable := true
var view_scale := 1.0
var view_off := Vector2.ZERO
var _hover := {}
var _drag := {}                    # {mode: move|connect, ...}
var _connect_from := {}            # tool == connect: the first tap
var _cards := {}                   # cached channel card rects (screen space) by "in:name" / "out:name"
var _r := 20.0
var _fit_pending := true
var lesion_mode := false           # lesion lab: clicks cut and silence instead of selecting and dragging
var lesions := {}                  # the lab's lesion set ("proj:<i>", "pop:<id>", "in:<name>"): drawn cut / silenced
var inset_bottom := 0.0            # px at the bottom covered by the phone inspector sheet: fit() keeps the neurons above it

func _ready() -> void:
	clip_contents = true; mouse_filter = Control.MOUSE_FILTER_STOP; focus_mode = Control.FOCUS_CLICK
	resized.connect(func(): _fit_pending = true)

func _process(_dt: float) -> void:
	if is_visible_in_tree() and tele != null and (tele.sim != null or tele.running): queue_redraw()

func min_text_px() -> float: return float(Kit.fs(14)) * Kit.ppl

func set_circuit(c: Dictionary, t: Telemetry) -> void:
	circuit = c; tele = t; Circuits.ensure_pos(c); sel = {}; _fit_pending = true; queue_redraw()

func fit() -> void: _fit_pending = true; queue_redraw()

# ---------- geometry ----------
func _neurons() -> Array: return Circuits.neurons(circuit)

func _bbox(ns: Array) -> Rect2:
	if ns.is_empty(): return Rect2(0, 0, 300, 200)
	var lo := Vector2(1e9, 1e9); var hi := Vector2(-1e9, -1e9)
	for n in ns:
		lo = lo.min(n["pos"]); hi = hi.max(n["pos"])
	return Rect2(lo, hi - lo)

func _card_size() -> Vector2:
	var k := clampf(Kit.f(), 0.85, 1.0); return Vector2(150.0 * k, maxf(52.0 * k, float(Kit.fs(14)) * 2.0 + 18.0))

## Fit the neuron block into the canvas, leaving room (in screen px) for the sensor cards on the left and the motor cards on the right.
func _do_fit(ns: Array) -> void:
	var bb := _bbox(ns).grow(46.0)
	var cw := _card_size().x
	var side_in := cw + 76.0 if not circuit.get("inputs", []).is_empty() else 0.0
	var side_out := cw + 76.0 if not circuit.get("outputs", []).is_empty() else 0.0
	var avail := Vector2(size.x - 24.0 - side_in - side_out, size.y - (44.0 if inset_bottom > 0.0 else 80.0) - inset_bottom)
	view_scale = clampf(minf(avail.x / maxf(bb.size.x, 1.0), avail.y / maxf(bb.size.y, 1.0)), 0.3, 2.0)
	view_off = Vector2(12.0 + side_in + avail.x * 0.5, (size.y - inset_bottom) * 0.5 + 6.0) - bb.get_center() * view_scale
	_fit_pending = false

func to_screen(m: Vector2) -> Vector2: return m * view_scale + view_off
func to_model(s: Vector2) -> Vector2: return (s - view_off) / view_scale

## Disc radius in screen px: scaled to the nearest-neighbour spacing so dense populations stay readable.
func _calc_radius(ns: Array) -> float:
	var nn := 1e9
	for i in ns.size():
		for j in range(i + 1, mini(ns.size(), i + 40)):
			nn = minf(nn, (ns[i]["pos"] as Vector2).distance_to(ns[j]["pos"]))
	nn *= view_scale
	var base := 26.0 * clampf(view_scale, 0.8, 1.4) * clampf(Kit.f() + 0.25, 0.8, 1.15)
	return clampf(minf(base, nn * 0.42), 8.0, 36.0)

## Population geometry in screen space: centroid and radius of the cluster outline.
func _pops(ns: Array) -> Array:
	var out := []; var pops: Array = circuit.get("populations", [])
	for pi in pops.size(): out.append({"id": str(pops[pi].get("id", "")), "pts": PackedVector2Array(), "n": Circuits.pop_n(pops[pi]), "first": Circuits.pop_first(circuit, pi)})
	for n in ns:
		var arr: PackedVector2Array = out[n["pi"]]["pts"]; arr.append(to_screen(n["pos"])); out[n["pi"]]["pts"] = arr
	for p in out:
		var pts: PackedVector2Array = p["pts"]; var c := Vector2.ZERO
		for q in pts: c += q
		c /= maxf(float(pts.size()), 1.0)
		var rr := 0.0
		for q in pts: rr = maxf(rr, q.distance_to(c))
		p["c"] = c; p["rad"] = (rr + _r + 8.0) if p["n"] > 1 else _r
		if p["n"] > 1 and pts.size() > 0:
			var lo := pts[0]; var hi := pts[0]
			for q in pts: lo = lo.min(q); hi = hi.max(q)
			p["box"] = Rect2(lo, hi - lo).grow(_r + 9.0)
	return out

func _pop_dict(pops: Array, id: String) -> Dictionary:
	for p in pops:
		if p["id"] == id: return p
	return {}

## Control points of the axon from population a to b (screen space). Reciprocal pairs bow to opposite sides; self-loops make a loop.
func _axon(pa: Dictionary, pb: Dictionary) -> PackedVector2Array:
	var a: Vector2 = pa["c"]; var b: Vector2 = pb["c"]
	if pa["id"] == pb["id"]:
		var rr: float = pa["rad"]; var top := a.y - rr
		if pa.has("box"): top = (pa["box"] as Rect2).position.y; rr = minf(rr, 70.0)
		var loop := minf(rr * 1.0, 60.0)
		return PackedVector2Array([Vector2(a.x - loop * 0.4, top), Vector2(a.x - loop * 1.5, top - loop * 1.7), Vector2(a.x + loop * 1.5, top - loop * 1.7), Vector2(a.x + loop * 0.4, top)])
	var d := b - a; var l := maxf(d.length(), 1.0); var nrm := d.orthogonal() / l
	var bow := clampf(l * 0.2, 22.0, 90.0)
	var a2 := _edge_pt(pa, d / l); var b2 := _edge_pt(pb, -d / l)
	var d2 := b2 - a2
	return PackedVector2Array([a2, a2 + d2 * 0.33 + nrm * bow * 1.25, a2 + d2 * 0.67 + nrm * bow * 1.25, b2])

## Where a ray from the population's centre in direction `dir` leaves its outline (circle for one neuron, rounded box for a group).
func _edge_pt(p: Dictionary, dir: Vector2) -> Vector2:
	var c: Vector2 = p["c"]
	if p.has("box"):
		var bx: Rect2 = p["box"]; var tx := 1e9; var ty := 1e9
		if absf(dir.x) > 1e-4: tx = (bx.size.x * 0.5) / absf(dir.x)
		if absf(dir.y) > 1e-4: ty = (bx.size.y * 0.5) / absf(dir.y)
		return bx.get_center() + dir * minf(tx, ty)
	return c + dir * float(p["rad"])

func _kind_col(kind: String) -> Color: return Kit.inh() if kind == "I" else Kit.exc()

# ---------- draw ----------
func _draw() -> void:
	if circuit.is_empty(): return
	var ns := _neurons()
	if _fit_pending: _do_fit(ns)
	_r = _calc_radius(ns)
	var pops := _pops(ns); var kd := Circuits.kinds(circuit)
	var now := tele.now() if tele != null else 0.0
	var fz := Kit.fs(14); var font := Fonts.regular
	_cards.clear()
	# --- sensor / motor cards (model-space layout beside the neuron block)
	var bb := _bbox(ns)
	var ins: Array = circuit.get("inputs", []); var outs: Array = circuit.get("outputs", [])
	var cs := _card_size(); var card_w := cs.x; var card_h := cs.y
	var pad := _r + 14.0
	var xl := to_screen(bb.position).x - pad - 44.0 - card_w * 0.5; var xr := to_screen(bb.end).x + pad + 44.0 + card_w * 0.5
	var cy := to_screen(bb.get_center()).y; var pitch := maxf(card_h + 12.0, minf(96.0, maxf(bb.size.y, 120.0) / 2.0) * view_scale)
	for i in ins.size():
		var c := Vector2(xl, cy + (float(i) - float(ins.size() - 1) * 0.5) * pitch)
		_cards["in:" + str(ins[i].get("name", ""))] = Rect2(c - cs * 0.5, cs)
	for i in outs.size():
		var c := Vector2(xr, cy + (float(i) - float(outs.size() - 1) * 0.5) * pitch)
		_cards["out:" + str(outs[i].get("name", ""))] = Rect2(c - cs * 0.5, cs)
	var in_sensor := {}
	if tele != null and tele.task != null:
		for s in tele.task.sensor_channels(): in_sensor[str(s)] = true
	# --- sensor edges
	for ch in ins:
		var key := "in:" + str(ch.get("name", "")); var r: Rect2 = _cards[key]
		var act := 0.0
		if tele != null and tele.sim != null:
			var v: float = float(ch.get("value", 0.0)); act = 1.0 if v > 0.0 else 0.0
		var selected: bool = sel.get("t", "") == "in" and sel.get("name", "") == ch.get("name", "")
		for tg in ch.get("targets", []):
			var pd := _pop_dict(pops, Circuits.proj_pop(tg)); if pd.is_empty(): continue
			var a := Vector2(r.end.x, r.get_center().y); var b: Vector2 = pd["c"]
			var dir := (b - a).normalized(); var b2: Vector2 = _edge_pt(pd, -dir) - dir * 4.0
			var mid := (a + b2) * 0.5; var nrm := (b2 - a).orthogonal().normalized() * 20.0
			var pts := Gfx.bez_points(a, a + (b2 - a) * 0.4 + nrm, a + (b2 - a) * 0.7 + nrm, b2, 20)
			var col := Color(Kit.GREEN, 0.34 + 0.4 * act + (0.3 if selected else 0.0))
			Gfx.dashed(self, pts, col, 1.8, 8.0, 6.0, now * 26.0 * act)
			Gfx.arrow_head(self, b2, pts[pts.size() - 1] - pts[pts.size() - 2], Color(Kit.GREEN, 0.8), 9.0)
	# --- motor edges
	for ch in outs:
		var key := "out:" + str(ch.get("name", "")); var r: Rect2 = _cards[key]
		var rate := 0.0
		if tele != null: rate = float(tele.rates.get(str(ch.get("name", "")), 0.0))
		for tg in ch.get("neurons", []):
			var pd := _pop_dict(pops, Circuits.proj_pop(tg)); if pd.is_empty(): continue
			var a: Vector2 = pd["c"]; var b := Vector2(r.position.x, r.get_center().y)
			var dir := (b - a).normalized(); var a2: Vector2 = _edge_pt(pd, dir) + dir * 4.0
			var nrm := (b - a2).orthogonal().normalized() * 20.0
			var pts := Gfx.bez_points(a2, a2 + (b - a2) * 0.35 + nrm, a2 + (b - a2) * 0.65 + nrm, b, 20)
			Gfx.dashed(self, pts, Color(Gfx.VIOLET, 0.30 + clampf(rate / 80.0, 0.0, 0.5)), 1.8, 8.0, 6.0, -now * 26.0 * clampf(rate / 40.0, 0.0, 1.0))
			Gfx.arrow_head(self, b, pts[pts.size() - 1] - pts[pts.size() - 2], Color(Gfx.VIOLET, 0.8), 9.0)
	# --- projections
	var projs: Array = circuit.get("projections", [])
	var geo := []
	for pj in projs.size():
		var pr: Dictionary = projs[pj]
		var pa := _pop_dict(pops, Circuits.proj_pop(pr.get("pre", ""))); var pb := _pop_dict(pops, Circuits.proj_pop(pr.get("post", "")))
		if pa.is_empty() or pb.is_empty(): geo.append(PackedVector2Array()); continue
		var cp := _axon(pa, pb); geo.append(cp)
		var w := Circuits.proj_weight(pr); var col := _kind_col("I" if w < 0.0 else "E")
		var isel: bool = sel.get("t", "") == "proj" and int(sel.get("i", -1)) == pj
		var ihov: bool = _hover.get("t", "") == "proj" and int(_hover.get("i", -1)) == pj
		var width := (1.6 + clampf(sqrt(absf(w) / Circuits.weight_of(str(circuit["populations"][Circuits.pop_index(circuit, str(pb["id"]))].get("model", "")))), 0.0, 2.0) * 1.2) * clampf(view_scale, 0.85, 1.2)
		var pts := Gfx.bez_points(cp[0], cp[1], cp[2], cp[3], 28)
		if isel or ihov: draw_polyline(pts, Color(col, 0.34 if isel else 0.2), width + 9.0, true)
		draw_polyline(pts, Color(col, 0.30), width + 3.0, true)
		draw_polyline(pts, Color(col.lightened(0.25 if isel else 0.0), 0.95 if isel else (0.78 if ihov else 0.58)), width + (1.0 if isel else 0.0), true)
		var tan := cp[3] - cp[2]
		var tip := cp[3]
		if w < 0.0: Gfx.bar_head(self, tip, tan, Color(col, 0.95), 11.0 * clampf(view_scale, 0.9, 1.2), width)
		else: Gfx.arrow_head(self, tip, tan, Color(col, 0.95), 11.0 * clampf(view_scale, 0.9, 1.2))
		if lesions.has("proj:%d" % pj):   # a cut axon: dashed red with an X at its middle
			draw_polyline(pts, Color(Kit.BENCH, 0.75), width + 6.0, true); Gfx.dashed(self, pts, Color(Kit.RED, 0.95), 2.2, 5.0, 7.0)
			var cm := Gfx.bez(cp[0], cp[1], cp[2], cp[3], 0.5); draw_circle(cm, 13.0, Kit.BENCH); draw_arc(cm, 13.0, 0, TAU, 24, Kit.RED, 2.2, true)
			draw_line(cm + Vector2(-6, -6), cm + Vector2(6, 6), Kit.RED, 3.0, true); draw_line(cm + Vector2(-6, 6), cm + Vector2(6, -6), Kit.RED, 3.0, true)
	# --- pulses
	if tele != null and tele.sim != null:
		for pj in projs.size():
			var cp: PackedVector2Array = geo[pj]; if cp.size() < 4: continue
			var pr: Dictionary = projs[pj]; var col := _kind_col("I" if Circuits.proj_weight(pr) < 0.0 else "E")
			var trav := Telemetry.pulse_seconds(Circuits.proj_delay(pr))
			var pa: PackedFloat64Array = tele.pulses[pj]
			for birth in pa:
				var u := (now - birth) / trav
				if u < 0.0 or u > 1.25: continue
				var uu := clampf(u, 0.0, 1.0)
				if u <= 1.0:
					for k in 5:   # comet tail
						var ut := maxf(uu - 0.035 * k, 0.0); var q := Gfx.bez(cp[0], cp[1], cp[2], cp[3], ut)
						Gfx.glow(self, q, (11.0 - k * 1.4), Color(col, 0.55 * (1.0 - k * 0.2)))
					var q0 := Gfx.bez(cp[0], cp[1], cp[2], cp[3], uu)
					Gfx.glow(self, q0, 17.0, Color(col, 0.55)); draw_circle(q0, 3.4, Color(1, 1, 1, 0.95)); draw_circle(q0, 2.2, col.lightened(0.35))
				else:   # arrival: a ring blooms on the postsynaptic disc
					var k2 := (u - 1.0) / 0.25
					draw_arc(cp[3], 6.0 + 12.0 * k2, 0, TAU, 24, Color(col, 0.7 * (1.0 - k2)), 2.0, true)
	# --- population outlines (n > 1): a rounded box around the group
	for p in pops:
		if p["n"] > 1 and p.has("box"):
			var bx: Rect2 = p["box"]
			Gfx.rounded_rect(self, bx, Color(Kit.PANEL, 0.35), Color(Kit.LINE, 0.95), minf(bx.size.x, bx.size.y) * 0.5 if bx.size.x < 90.0 or bx.size.y < 90.0 else 26.0, 1.4)
	# --- neurons
	var n_i := 0
	for n in ns:
		var gi := n_i; n_i += 1
		var pid: String = str(circuit["populations"][n["pi"]].get("id", ""))
		var kind: String = str(kd.get(pid, "E")); var col := _kind_col(kind)
		var p := to_screen(n["pos"])
		var f := 0.0
		if tele != null and gi < tele.flash.size(): f = tele.flash[gi]
		var isel: bool = sel.get("t", "") == "pop" and sel.get("id", "") == pid
		var ihov: bool = _hover.get("t", "") == "pop" and _hover.get("id", "") == pid
		var rr := _r * (1.0 + 0.16 * f)
		Gfx.glow(self, p, rr * (2.2 + 2.0 * f), Color(col, 0.24 + 0.76 * f + (0.1 if (isel or ihov) else 0.0)))
		draw_circle(p, rr + 1.5, Color(Kit.BENCH, 0.9))
		draw_circle(p, rr, Color(Kit.PANEL_HI, 1.0).lerp(col, 0.16 + 0.80 * f))
		draw_arc(p, rr - 1.0, 0, TAU, 40, Color(col, 0.55 + 0.45 * f), maxf(2.0, rr * 0.11), true)
		draw_circle(p, rr * 0.34, Color(col.lightened(0.5), 0.30 + 0.7 * f))
		if f > 0.05: Gfx.glow(self, p, rr * 1.1, Color(1, 1, 1, 0.5 * f))
		# membrane potential as a thin arc of the traced neurons
		if tele != null and tele.volt.has(gi):
			var a: PackedFloat32Array = tele.volt[gi]
			if a.size() > 0 and is_finite(a[a.size() - 1]):   # a model without a membrane (relay, generator) reports NaN
				var u := clampf((float(a[a.size() - 1]) + 80.0) / 50.0, 0.0, 1.0)
				draw_arc(p, rr + 5.0, -PI * 0.5, -PI * 0.5 + TAU * u, 40, Color(col.lightened(0.3), 0.85), 2.0, true)
		if isel: draw_arc(p, rr + 10.0, 0, TAU, 48, Color(1, 1, 1, 0.85), 1.6, true)
		if lesions.has("pop:" + pid):   # a silenced population: greyed out and struck through
			draw_circle(p, rr + 1.0, Color(Kit.BENCH, 0.7)); draw_arc(p, rr + 4.0, 0, TAU, 40, Color(Kit.RED, 0.95), 2.4, true)
			draw_line(p + Vector2(-rr, rr) * 0.75, p + Vector2(rr, -rr) * 0.75, Kit.RED, 3.0, true)
		if tele != null and tele.watched.has(gi):
			var idx := tele.watched.find(gi)
			draw_circle(p + Vector2(rr * 0.9, -rr * 0.9), 7.0, Color(Kit.BENCH, 0.9)); draw_circle(p + Vector2(rr * 0.9, -rr * 0.9), 5.0, col)
	# --- the selected axon names its weight and delay
	if sel.get("t", "") == "proj" and int(sel.get("i", -1)) < projs.size() and int(sel.get("i", -1)) >= 0:
		var cp: PackedVector2Array = geo[int(sel["i"])]
		if cp.size() == 4:
			var pr: Dictionary = projs[int(sel["i"])]; var w := Circuits.proj_weight(pr)
			var pi2 := Circuits.pop_index(circuit, Circuits.proj_pop(pr.get("post", ""))); var unit := "nS" if pi2 >= 0 and str(circuit["populations"][pi2].get("model", "")).contains("cond") else "pA"
			var txt := "%s%s %s   %s ms" % ["+" if w >= 0.0 else "-", str(snappedf(absf(w), 0.1)), unit, str(snappedf(Circuits.proj_delay(pr), 0.1))]
			var mid := Gfx.bez(cp[0], cp[1], cp[2], cp[3], 0.5); var tw := font.get_string_size(txt, HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x + 16.0
			var rect := Rect2(mid - Vector2(tw * 0.5, float(fz) * 0.9 + 4.0), Vector2(tw, float(fz) + 8.0))
			Gfx.rounded_rect(self, rect, Kit.PANEL_HI, _kind_col("I" if w < 0.0 else "E"), 6.0, 1.4)
			draw_string(font, rect.position + Vector2(8, float(fz) + 2.0), txt, HORIZONTAL_ALIGNMENT_LEFT, tw, fz, Kit.TEXT)
	# --- labels
	for p in pops:
		var c: Vector2 = p["c"]; var rr: float = p["rad"]
		if p.has("box"): rr = (p["box"] as Rect2).end.y - c.y
		var pi := Circuits.pop_index(circuit, p["id"]); var pm: Dictionary = circuit["populations"][pi]
		var txt := str(p["id"]) + ("  ×%d" % p["n"] if p["n"] > 1 else "")
		var isel: bool = sel.get("t", "") == "pop" and sel.get("id", "") == p["id"]
		draw_string(font, c + Vector2(-80, rr + float(fz) + 4.0), txt, HORIZONTAL_ALIGNMENT_CENTER, 160, fz, Kit.TEXT if isel else Kit.TEXT_SOFT)
		if isel: draw_string(font, c + Vector2(-90, rr + float(fz) * 2.1 + 6.0), str(pm.get("model", "")), HORIZONTAL_ALIGNMENT_CENTER, 180, fz, Kit.TEXT_SOFT)
	# --- cards
	for ch in ins: _draw_card(font, fz, "in", ch, in_sensor.has(str(ch.get("name", ""))), now)
	for ch in outs: _draw_card(font, fz, "out", ch, true, now)
	# --- connect rubber band
	if _drag.get("mode", "") == "connect":
		var a: Vector2 = _drag["from_pt"]; var b: Vector2 = _drag["to"]
		var tgt := _hit(b)
		var ok: bool = not tgt.is_empty() and ((tgt["t"] == "pop") or (tgt["t"] == "out" and _drag["src"]["t"] == "pop"))
		var pts := Gfx.bez_points(a, a + (b - a) * 0.4 + (b - a).orthogonal().normalized() * 14.0, a + (b - a) * 0.7 + (b - a).orthogonal().normalized() * 14.0, b, 20)
		var kc := Color(1, 1, 1, 0.9) if ok else Color(1, 1, 1, 0.45)
		Gfx.dashed(self, pts, kc, 2.4, 8.0, 6.0, now * 40.0); draw_circle(b, 5.0, kc)
		if ok: Gfx.glow(self, b, 26.0, Color(1, 1, 1, 0.25))
	elif tool == "connect" and not _connect_from.is_empty():
		var pd := _pop_dict(pops, str(_connect_from.get("id", "")))
		if not pd.is_empty(): draw_arc(pd["c"], float(pd["rad"]) + 12.0, 0, TAU, 48, Color(1, 1, 1, 0.8), 2.0, true)
	if circuit.get("populations", []).is_empty():
		var hw := maxf(size.x * 0.8, 160.0)
		draw_multiline_string(font, Vector2((size.x - hw) * 0.5, size.y * 0.16), "An empty bench. Press + EXCITATORY above to add a neuron, then drag from a sensor card to it and from it to a motor card.", HORIZONTAL_ALIGNMENT_CENTER, hw, Kit.fs(18), 4, Kit.TEXT_SOFT)

func _draw_card(font: Font, fz: int, side: String, ch: Dictionary, task_driven: bool, now: float) -> void:
	var name := str(ch.get("name", "")); var r: Rect2 = _cards[side + ":" + name]
	var col := Kit.GREEN if side == "in" else Gfx.VIOLET
	var isel: bool = sel.get("t", "") == side and sel.get("name", "") == name
	var act := 0.0
	if side == "in":
		act = 1.0 if float(ch.get("value", 0.0)) > 0.0 else 0.0
		if tele != null and tele.sim != null:
			for tg in ch.get("targets", []):
				var pi := Circuits.pop_index(circuit, Circuits.proj_pop(tg))
				if pi >= 0:
					var f0 := Circuits.pop_first(circuit, pi)
					for k in Circuits.pop_n(circuit["populations"][pi]): act = maxf(act, tele.flash[f0 + k] if f0 + k < tele.flash.size() else 0.0)
	elif tele != null: act = clampf(float(tele.rates.get(name, 0.0)) / 60.0, 0.0, 1.0)
	Gfx.glow(self, r.get_center(), r.size.x * 0.75, Color(col, 0.05 + 0.28 * act))
	Gfx.rounded_rect(self, r, Kit.PANEL, Color(col, 0.9 if isel else 0.65), 9.0, 1.6)
	var kicker := ("SENSOR" if task_driven else "INPUT") if side == "in" else "MOTOR"
	draw_string(font, r.position + Vector2(10, float(fz) + 3.0), kicker, HORIZONTAL_ALIGNMENT_LEFT, r.size.x - 14.0, fz, Color(col, 0.9))
	draw_string(font, r.position + Vector2(10, r.size.y - 9.0), name, HORIZONTAL_ALIGNMENT_LEFT, r.size.x - 14.0, fz, Kit.TEXT)
	# activity: a row of LEDs along the top edge's right side
	for k in 5:
		var lit := float(k) < act * 5.0
		draw_circle(Vector2(r.end.x - 12.0 - k * 9.0, r.position.y + 12.0), 3.0, Color(col, 0.95 if lit else 0.16))
	if isel: draw_rect(r.grow(5.0), Color(1, 1, 1, 0.6), false, 1.4)
	if lesions.has(side + ":" + name):   # a cut sensor
		draw_rect(r, Color(Kit.BENCH, 0.55)); draw_rect(r.grow(2.0), Kit.RED, false, 2.2)
		draw_string(font, r.position + Vector2(r.size.x - 52.0, float(fz) + 3.0), "CUT", HORIZONTAL_ALIGNMENT_RIGHT, 46.0, fz, Kit.RED)

# ---------- hit testing & input ----------
func _hit(p: Vector2) -> Dictionary:
	if circuit.is_empty(): return {}
	var ns := _neurons(); var pops := _pops(ns)
	for key in _cards:
		if (_cards[key] as Rect2).grow(4.0).has_point(p):
			var parts: PackedStringArray = str(key).split(":", true, 1); return {"t": parts[0], "name": parts[1]}
	for pd in pops:
		for q in (pd["pts"] as PackedVector2Array):
			if q.distance_to(p) <= _r + 5.0: return {"t": "pop", "id": pd["id"], "rim": q.distance_to(p) > _r * 0.62, "at": q}
	# axons
	var best := {}; var bd := 11.0
	var projs: Array = circuit.get("projections", [])
	for pj in projs.size():
		var pa := _pop_dict(pops, Circuits.proj_pop(projs[pj].get("pre", ""))); var pb := _pop_dict(pops, Circuits.proj_pop(projs[pj].get("post", "")))
		if pa.is_empty() or pb.is_empty(): continue
		var cp := _axon(pa, pb)
		for k in 25:
			var d := Gfx.bez(cp[0], cp[1], cp[2], cp[3], float(k) / 24.0).distance_to(p)
			if d < bd: bd = d; best = {"t": "proj", "i": pj}
	return best

func _gui_input(e: InputEvent) -> void:
	if circuit.is_empty(): return
	if e is InputEventMouseMotion:
		var h := _hit(e.position)
		if h != _hover: _hover = h; queue_redraw()
		if _drag.get("mode", "") == "move":
			var d: Vector2 = (e.position - _drag["last"]) / view_scale; _drag["last"] = e.position
			if editable: _move_pop(str(_drag["id"]), d); _drag["moved"] = true
			queue_redraw()
		elif _drag.get("mode", "") == "connect": _drag["to"] = e.position; queue_redraw()
		mouse_default_cursor_shape = Control.CURSOR_POINTING_HAND if not h.is_empty() else Control.CURSOR_ARROW
	elif e is InputEventMouseButton:
		if e.button_index == MOUSE_BUTTON_LEFT:
			if e.pressed: _press(e.position)
			else: _release(e.position)
		elif e.button_index == MOUSE_BUTTON_RIGHT and e.pressed:
			var h := _hit(e.position)
			if not h.is_empty(): select(h); delete_selected()
	elif e is InputEventKey and e.pressed and (e.keycode == KEY_DELETE or e.keycode == KEY_BACKSPACE): delete_selected()

func _press(p: Vector2) -> void:
	if not Kit.pad_mode: grab_focus()   # so the Delete key reaches _gui_input
	var h := _hit(p)
	if lesion_mode:
		if not h.is_empty() and ["pop", "proj", "in"].has(str(h["t"])): lesion_clicked.emit(h)
		return
	if tool == "connect" and editable:
		if h.is_empty(): _connect_from = {}; select({}); return
		if _connect_from.is_empty():
			if h["t"] == "pop" or h["t"] == "in": _connect_from = h; select(h)
		else:
			_make_link(_connect_from, h); _connect_from = {}
		queue_redraw(); return
	select(h)
	if h.is_empty(): return
	if h["t"] == "pop" and editable:
		if bool(h.get("rim", false)): _drag = {"mode": "connect", "src": h, "from_pt": h["at"], "to": p}
		else: _drag = {"mode": "move", "id": h["id"], "last": p, "moved": false}
	elif h["t"] == "in" and editable and h["name"] != "":
		var r: Rect2 = _cards["in:" + str(h["name"])]
		if p.x > r.end.x - 26.0: _drag = {"mode": "connect", "src": h, "from_pt": Vector2(r.end.x, r.get_center().y), "to": p}

func _release(p: Vector2) -> void:
	if _drag.get("mode", "") == "connect":
		var tgt := _hit(p)
		if not tgt.is_empty(): _make_link(_drag["src"], tgt)
	elif _drag.get("mode", "") == "move" and bool(_drag.get("moved", false)): moved.emit()
	_drag = {}; queue_redraw()

func _make_link(src: Dictionary, dst: Dictionary) -> void:
	if src.is_empty() or dst.is_empty(): return
	if src["t"] == "pop" and dst["t"] == "pop":
		var before: int = circuit["projections"].size(); var idx := Circuits.add_projection(circuit, str(src["id"]), str(dst["id"]))
		select({"t": "proj", "i": idx})
		if circuit["projections"].size() != before: structure_changed.emit()
	elif src["t"] == "in" and dst["t"] == "pop":
		for ch in circuit["inputs"]:
			if str(ch.get("name", "")) == str(src["name"]):
				var tg: Array = ch.get("targets", [])
				for x in tg:
					if Circuits.proj_pop(x) == str(dst["id"]): return
				if tg.is_empty():   # first connection: a weight that makes that neuron model fire (not a silent 120 pA onto a 20 nS cell)
					var pi := Circuits.pop_index(circuit, str(dst["id"]))
					if pi >= 0: ch["weight"] = Circuits.input_weight(str(circuit["populations"][pi].get("model", "iaf_psc_alpha")), str(ch.get("kind", "poisson")))
				tg.append(str(dst["id"])); ch["targets"] = tg; structure_changed.emit(); return
	elif src["t"] == "pop" and dst["t"] == "out":
		for ch in circuit["outputs"]:
			if str(ch.get("name", "")) == str(dst["name"]):
				var tg: Array = ch.get("neurons", [])
				for x in tg:
					if Circuits.proj_pop(x) == str(src["id"]): return
				tg.append(str(src["id"])); ch["neurons"] = tg; structure_changed.emit(); return

func _move_pop(id: String, d: Vector2) -> void:
	var pi := Circuits.pop_index(circuit, id); if pi < 0: return
	for q in circuit["populations"][pi]["pos"]: q[0] = float(q[0]) + d.x; q[1] = float(q[1]) + d.y

func select(h: Dictionary) -> void:
	var s := {}
	if not h.is_empty():
		s = {"t": h["t"]}
		for k in ["id", "i", "name"]:
			if h.has(k): s[k] = h[k]
	if s == sel: return
	sel = s; selection_changed.emit(sel); queue_redraw()

func delete_selected() -> void:
	if not editable or sel.is_empty(): return
	match sel["t"]:
		"pop": Circuits.remove_population(circuit, str(sel["id"]))
		"proj": circuit["projections"].remove_at(int(sel["i"]))
		"in":
			if not is_free_input(str(sel["name"])): return   # a task's sensors stay
			var keep := []
			for ch in circuit["inputs"]:
				if str(ch.get("name", "")) != str(sel["name"]): keep.append(ch)
			circuit["inputs"] = keep
		_: return
	sel = {}; selection_changed.emit(sel); structure_changed.emit(); queue_redraw()

## True for an input the player added (noise): not one of the task's sensors.
func is_free_input(name: String) -> bool:
	if tele != null and tele.task != null and (tele.task.sensor_channels() as PackedStringArray).has(name): return false
	return true

## A free spot right of the existing neurons for a newly added one.
func free_spot() -> Vector2:
	var ns := _neurons(); if ns.is_empty(): return Vector2(300, 240)
	var bb := _bbox(ns); var best := Vector2(bb.end.x + 110.0, bb.get_center().y)
	for k in 40:
		var cand := Vector2(bb.position.x + 70.0 + (k % 6) * 110.0, bb.end.y + 100.0 + (k / 6) * 80.0) if k > 0 else best
		var ok := true
		for n in ns:
			if (n["pos"] as Vector2).distance_to(cand) < 80.0: ok = false; break
		if ok: return cand
	return best
