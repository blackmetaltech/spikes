class_name LabScreen
extends Screen
## Lab (placeholder): a demo circuit running live in the Rust SpikeSim, shown on the raster and voltage scopes, with a drive slider and play / pause.
## The circuit editor, task arena and analysis panel (REQ-0.1 section E) replace this. Shows a notice when the GDExtension is missing.
var scope: SpikeScope
var sim = null
var back_btn: Button
var readout: Label
var play_btn: Button

func default_focus() -> Control: return back_btn

func build() -> void:
	var f := f(); var phone := Layout.phone
	add_child(Kit.bench())
	var margin := MarginContainer.new(); margin.set_anchors_preset(Control.PRESET_FULL_RECT)
	var mx := int(48 * f) if not phone else 28; var my := int(32 * f) if not phone else 12
	for side in ["left", "right"]: margin.add_theme_constant_override("margin_" + side, mx)
	margin.add_theme_constant_override("margin_top", my); margin.add_theme_constant_override("margin_bottom", my); add_child(margin)
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", int(16 * f)); margin.add_child(v)

	var head := HBoxContainer.new(); head.add_theme_constant_override("separation", 16); v.add_child(head)
	back_btn = Kit.btn("BACK", func(): app.back(), "panel", int(26 * f) if not phone else 22, Vector2(130 if not phone else 100, 0))
	head.add_child(back_btn)
	var title := Kit.shout("Lab", int(52 * f) if not phone else 36, Kit.TEXT); head.add_child(title)
	head.add_child(Kit.chip("PLACEHOLDER", Kit.exc(), 16 if not phone else 15))
	var sp := Control.new(); sp.size_flags_horizontal = Control.SIZE_EXPAND_FILL; head.add_child(sp)
	readout = Kit.label("", 20, Kit.TEXT_SOFT); head.add_child(readout)

	var cols := HBoxContainer.new(); cols.size_flags_vertical = Control.SIZE_EXPAND_FILL; cols.add_theme_constant_override("separation", int(20 * f)); v.add_child(cols)
	var side := VBoxContainer.new(); side.add_theme_constant_override("separation", int(14 * f))
	side.custom_minimum_size.x = (400 if not phone else 270) * (f if not phone else 1.0); cols.add_child(side)
	var c := Kit.card("Demo circuit", Kit.PANEL, Kit.exc()); side.add_child(c[0])
	c[1].add_child(Kit.label("Eight leaky integrate-and-fire units in a ring, with feedback inhibition. Real circuits, tasks and the editor come next.", 17 if not phone else 15, Kit.TEXT_SOFT, true))
	var leg := HBoxContainer.new(); leg.add_theme_constant_override("separation", 10)
	leg.add_child(Kit.chip("EXCITATORY", Kit.exc(), 15)); leg.add_child(Kit.chip("INHIBITORY", Kit.inh(), 15)); c[1].add_child(leg)
	var d := Kit.card("Drive", Kit.PANEL, Kit.inh()); side.add_child(d[0])
	d[1].add_child(Kit.slider(0.65, func(x): if sim != null: sim.set_input(0, x * 600.0), 220.0 if not phone else 150.0))
	play_btn = Kit.btn("PAUSE", func(): _toggle(), "amber", 24, Vector2(0, 0)); side.add_child(play_btn)

	scope = SpikeScope.new(); scope.size_flags_horizontal = Control.SIZE_EXPAND_FILL; scope.size_flags_vertical = Control.SIZE_EXPAND_FILL; cols.add_child(scope)
	if not ClassDB.class_exists("SpikeSim"):
		var w := Kit.label("The spike-gd GDExtension is not loaded. Run scripts/build-gd.sh.", 20, Kit.RED, true); w.set_anchors_preset(Control.PRESET_CENTER); scope.add_child(w); return
	sim = ClassDB.instantiate("SpikeSim")
	if not sim.load_circuit(DemoCircuit.json()): sim = null; return
	sim.set_input(0, 0.65 * 600.0); sim.set_input(1, 120.0)
	scope.attach(sim, DemoCircuit.KINDS)

func _toggle() -> void:
	scope.running = not scope.running; play_btn.text = "PAUSE" if scope.running else "PLAY"

func _process(_dt: float) -> void:
	if sim != null and readout != null: readout.text = "t = %d ms    spikes %d" % [int(sim.time_ms()), scope.total_spikes]
