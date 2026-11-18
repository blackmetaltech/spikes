class_name PartsScreen
extends Screen
## The parts bin as a screen: the neuron catalogue to browse, from the campaign map. Placing parts happens in the lab (the PARTS button there).
var back_btn: Button

func default_focus() -> Control: return back_btn

func build() -> void:
	var f := f(); var phone := Layout.phone
	add_child(Kit.bench())
	var margin := MarginContainer.new(); margin.set_anchors_preset(Control.PRESET_FULL_RECT)
	var mx := int(56 * f) if not phone else 16; var my := int(30 * f) if not phone else 8
	for side in ["left", "right"]: margin.add_theme_constant_override("margin_" + side, mx)
	margin.add_theme_constant_override("margin_top", my); margin.add_theme_constant_override("margin_bottom", my); add_child(margin)
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", int(12 * f) if not phone else 6); margin.add_child(v)
	var hb := Kit.page_head("Parts bin", func(): app.back(), phone); var head: HBoxContainer = hb[0]; back_btn = hb[1]; v.add_child(head)
	if not phone: head.add_child(Kit.running_head("Plate V  ·  catalogue of cells"))
	else: head.add_child(Kit.spacer(0, true))
	head.add_child(Kit.btn("Sandbox", func(): app.open_lab(""), "panel", int(22 * f) if not phone else 18))
	v.add_child(PartsBin.new().setup("browse", OS.get_environment("SP_UNLOCK_ALL") != ""))
