class_name SandboxPanel
extends PanelContainer
## The sandbox's world panel (the lab with no task): what the sandbox is, the neuromodulator dials (only when the engine can take them), the network firing
## rate over the last seconds, and the energy read-out (spikes per second and the ATP it costs). The scopes and the analysis are the lab's own.
var tele_get: Callable
var rate_label: Label
var atp_label: Label
var bars: RateBars
var dials: ModDials

func setup(tele_getter: Callable) -> SandboxPanel:
	tele_get = tele_getter
	var phone := Layout.phone; var f := Kit.f()
	add_theme_stylebox_override("panel", Kit.sb(Color("#081019"), Kit.LINE, 1, 12, 14 if not phone else 10, false))
	size_flags_vertical = Control.SIZE_EXPAND_FILL
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", int(10 * f) if not phone else 5); add_child(v)
	var hd := HBoxContainer.new(); hd.add_theme_constant_override("separation", 10); v.add_child(hd)
	hd.add_child(Kit.chip("SANDBOX", Kit.exc(), 16))
	var bl := Kit.label("No task, every part unlocked: build anything and read it off the scopes.", 17 if not phone else 15, Kit.TEXT_SOFT, true); hd.add_child(bl)
	if ModDials.available():
		v.add_child(Kit.label("NEUROMODULATORS", 15, Kit.TEXT_SOFT))
		dials = ModDials.new().setup(tele_getter, ["NE", "ACh", "DA"], true); v.add_child(dials)
	var er := HBoxContainer.new(); er.add_theme_constant_override("separation", 16); v.add_child(er)
	var left := VBoxContainer.new(); left.add_theme_constant_override("separation", 0); er.add_child(left)
	left.add_child(Kit.label("ENERGY", 15, Kit.TEXT_SOFT))
	rate_label = Kit.shout("0 spikes/s", 32 if not phone else 24, Kit.TEXT); left.add_child(rate_label)
	atp_label = Kit.label("", 16, Kit.exc(), true); atp_label.custom_minimum_size.x = 240; left.add_child(atp_label)
	var cite := Kit.label("Roughly 1e8 to 1e9 ATP per spike (Attwell and Laughlin 2001; Lennie 2003), so read it as an order of magnitude.", 14, Kit.TEXT_SOFT, true); cite.custom_minimum_size.x = 260; left.add_child(cite)
	bars = RateBars.new(); bars.size_flags_horizontal = Control.SIZE_EXPAND_FILL; bars.size_flags_vertical = Control.SIZE_EXPAND_FILL; bars.custom_minimum_size = Vector2(120, 60); bars.tele_get = tele_getter; er.add_child(bars)
	var pr := PopRates.new(); pr.tele_get = tele_getter; pr.size_flags_vertical = Control.SIZE_EXPAND_FILL; pr.custom_minimum_size = Vector2(0, 54); v.add_child(pr)
	return self

func _process(_dt: float) -> void:
	var t: Telemetry = tele_get.call()
	if t == null or not is_visible_in_tree(): return
	var hz := t.spike_rate_hz() if t.sim != null else 0.0
	rate_label.text = "%s spikes/s" % (str(int(roundf(hz))) if hz >= 10.0 else "%.1f" % hz)
	atp_label.text = Telemetry.atp_text(hz)

## Network spikes per 100 ms over the last 4 s, as bars.
class RateBars extends Control:
	var tele_get: Callable
	func _ready() -> void: clip_contents = true
	func min_text_px() -> float: return float(Kit.fs(14)) * Kit.ppl
	func _process(_dt: float) -> void:
		if is_visible_in_tree(): queue_redraw()
	func _draw() -> void:
		var fz := Kit.fs(14)
		draw_style_box(Kit.sb(Kit.WELL, Kit.LINE, 1, 8, 0, false), Rect2(Vector2.ZERO, size))
		draw_string(Fonts.regular, Vector2(8, float(fz) + 2.0), "network spikes per 100 ms", HORIZONTAL_ALIGNMENT_LEFT, size.x - 12, fz, Kit.TEXT_SOFT)
		var t: Telemetry = tele_get.call()
		if t == null or t.sim == null: return
		var nb := 40; var bins := PackedFloat32Array(); bins.resize(nb); var t1 := t.t_ms; var t0 := t1 - 4000.0; var mx := 1.0
		for j in t.sp_t.size():
			var u := (t.sp_t[j] - t0) / 4000.0
			if u >= 0.0 and u < 1.0: bins[int(u * nb)] += 1.0
		for b in bins: mx = maxf(mx, b)
		var inner := Rect2(8, float(fz) + 8.0, size.x - 16.0, size.y - float(fz) - 14.0); var bw := inner.size.x / float(nb)
		for i in nb:
			var h := inner.size.y * bins[i] / mx
			draw_rect(Rect2(inner.position.x + bw * float(i) + 1.0, inner.end.y - h, bw - 2.0, h), Color(Kit.exc(), 0.85))
		draw_string(Fonts.regular, Vector2(size.x - 70.0, float(fz) + 2.0), "max %d" % int(mx), HORIZONTAL_ALIGNMENT_RIGHT, 62, fz, Kit.TEXT_SOFT)

## Mean firing rate of each population over the last second, one bar per population (Hz per neuron), in the circuit's order.
class PopRates extends Control:
	var tele_get: Callable
	func _ready() -> void: clip_contents = true
	func min_text_px() -> float: return float(Kit.fs(14)) * Kit.ppl
	func _process(_dt: float) -> void:
		if is_visible_in_tree(): queue_redraw()
	func _draw() -> void:
		var fz := Kit.fs(14); var t: Telemetry = tele_get.call()
		draw_string(Fonts.regular, Vector2(0, float(fz)), "POPULATION RATES, last second (Hz per neuron)", HORIZONTAL_ALIGNMENT_LEFT, size.x, fz, Kit.TEXT_SOFT)
		if t == null or t.sim == null: return
		var pops: Array = t.circuit.get("populations", []); if pops.is_empty(): return
		var cnt := PackedFloat32Array(); cnt.resize(pops.size()); var t0 := t.t_ms - 1000.0
		for j in t.sp_t.size():
			if t.sp_t[j] >= t0 and t.sp_n[j] < t.pop_of.size(): cnt[t.pop_of[t.sp_n[j]]] += 1.0
		var hz := PackedFloat32Array(); var mx := 20.0
		for i in pops.size():
			hz.append(cnt[i] / (minf(1000.0, maxf(t.t_ms, 50.0)) * 0.001) / float(Circuits.pop_n(pops[i]))); mx = maxf(mx, hz[i])
		var top := float(fz) + 8.0; var rows := mini(pops.size(), maxi(1, int((size.y - top) / (float(fz) + 6.0)))); var rh := (size.y - top) / float(rows)
		var kd := Circuits.kinds(t.circuit); var lw := float(fz) * 5.0
		for i in rows:
			var y := top + rh * float(i); var col := Kit.inh() if str(kd.get(str(pops[i].get("id", "")), "E")) == "I" else Kit.exc()
			draw_string(Fonts.regular, Vector2(0, y + rh * 0.5 + float(fz) * 0.35), str(pops[i].get("id", "")), HORIZONTAL_ALIGNMENT_LEFT, lw, fz, Kit.TEXT)
			var bw := (size.x - lw - 70.0) * hz[i] / mx
			draw_rect(Rect2(lw, y + 3.0, size.x - lw - 70.0, rh - 6.0), Color(Kit.LINE, 0.4)); draw_rect(Rect2(lw, y + 3.0, bw, rh - 6.0), Color(col, 0.9))
			draw_string(Fonts.regular, Vector2(size.x - 62.0, y + rh * 0.5 + float(fz) * 0.35), "%.1f Hz" % hz[i], HORIZONTAL_ALIGNMENT_RIGHT, 60.0, fz, Kit.TEXT)
