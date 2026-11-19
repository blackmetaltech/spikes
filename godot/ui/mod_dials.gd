class_name ModDials
extends HBoxContainer
## Neuromodulator dials. In the sandbox they are sliders (NE, ACh, DA, 0 = none, 1 = strong) that call Telemetry.set_modulator, which reaches
## SpikeSim.set_modulator once the engine has it. In a task that declares modulators() they are read-only gauges of what the circuit sees.
## `Telemetry.modulators_available()` says whether the engine can take them; the lab hides the dials when it cannot (SP_MODDIALS=1 forces them for dev).
const COLORS := {"NE": "amber", "ACh": "cyan", "DA": "violet"}
const FULL := {"NE": "noradrenaline", "ACh": "acetylcholine", "DA": "dopamine"}
## Levels that use a channel for another transmitter than its engine name: the Aplysia levels drive "NE" as serotonin (5-HT). The channel id stays "NE" in the circuit and the
## engine; only what the dial says changes. task id -> {channel id -> [chip text, full name]}.
const TASK_NAMES := {"a_facilitate": {"NE": ["5-HT", "serotonin"]}, "aplysia_gill": {"NE": ["5-HT", "serotonin"]}, "dx_aplysia": {"NE": ["5-HT", "serotonin"]}}
var lab_tele: Callable          # () -> Telemetry
var editable := true
var _sl := {}
var _val := {}

static func available() -> bool: return Telemetry.modulators_available() or OS.get_environment("SP_MODDIALS") != ""

func setup(tele_getter: Callable, names: Array, can_edit: bool, task_id := "") -> ModDials:
	lab_tele = tele_getter; editable = can_edit
	add_theme_constant_override("separation", 16); size_flags_horizontal = Control.SIZE_EXPAND_FILL
	for n in names:
		var col := Kit.exc() if COLORS.get(n, "amber") == "amber" else (Kit.inh() if COLORS[n] == "cyan" else Gfx.VIOLET)
		var v := VBoxContainer.new(); v.size_flags_horizontal = Control.SIZE_EXPAND_FILL; v.add_theme_constant_override("separation", 2); add_child(v)
		var h := HBoxContainer.new(); h.add_theme_constant_override("separation", 8); v.add_child(h)
		var alias: Array = (TASK_NAMES.get(task_id, {}) as Dictionary).get(n, [str(n), str(FULL.get(n, ""))])
		h.add_child(Kit.chip(str(alias[0]), col, 16))
		var nm := Kit.label(str(alias[1]), 15, Kit.TEXT_SOFT); nm.size_flags_horizontal = Control.SIZE_EXPAND_FILL; nm.clip_text = true; h.add_child(nm)
		var val := Kit.label("0.00", 17, Kit.TEXT); h.add_child(val); _val[n] = val
		var s := HSlider.new(); s.min_value = 0.0; s.max_value = 1.0; s.step = 0.01; s.value = 0.0; s.editable = can_edit; s.focus_mode = Kit.focus_mode()
		s.custom_minimum_size = Vector2(0, maxf(30.0, Layout.tap())); s.size_flags_horizontal = Control.SIZE_EXPAND_FILL; v.add_child(s); _sl[n] = s
		s.value_changed.connect(func(x): val.text = "%.2f" % x)
	return self

func _process(_dt: float) -> void:
	if not is_visible_in_tree(): return
	var t: Telemetry = lab_tele.call()
	if t == null: return
	for n in _sl:
		var s: HSlider = _sl[n]
		if editable:
			if not t.mods.has(n) or not is_equal_approx(float(t.mods[n]), s.value): t.set_modulator(n, s.value)
		else:
			var lv := clampf(t.modulator_level(n), 0.0, 1.0)
			if not is_equal_approx(s.value, lv): s.set_value_no_signal(lv); (_val[n] as Label).text = "%.2f" % lv
