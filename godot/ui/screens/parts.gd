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
	var head := HBoxContainer.new(); head.add_theme_constant_override("separation", 14); v.add_child(head)
	var bs := int(24 * f) if not phone else 20
	back_btn = Kit.btn("BACK", func(): app.back(), "panel", bs, Vector2(120 if not phone else 84, 0)); head.add_child(back_btn)
	var tt := Kit.shout("Parts bin", int(50 * f) if not phone else 30, Kit.TEXT); tt.size_flags_horizontal = Control.SIZE_EXPAND_FILL; tt.size_flags_vertical = Control.SIZE_SHRINK_CENTER; head.add_child(tt)
	head.add_child(Kit.btn("SANDBOX", func(): app.open_lab(""), "panel", bs))
	v.add_child(PartsBin.new().setup("browse", OS.get_environment("SP_UNLOCK_ALL") != ""))
