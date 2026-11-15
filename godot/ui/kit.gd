class_name Kit
## Shared UI toolkit: the dark lab-bench visual language and the builders every screen uses.
## Deep navy bench, slightly lighter instrument panels with thin rules, warm amber = excitatory, cool cyan = inhibitory, Atkinson Hyperlegible type.
## Palette values are constants; sizes are logical px at 1080p (the window stretch scales them).

const BENCH := Color("#070b13")        # the bench: near-black navy
const PANEL := Color("#0e1726")        # instrument panel
const PANEL_HI := Color("#16233a")     # raised / hovered panel
const WELL := Color("#0a111d")         # recessed scope background
const LINE := Color("#27395a")         # hairlines and borders
const TEXT := Color("#e6edf7")
const TEXT_SOFT := Color("#98a8c2")    # muted body text (7:1 on PANEL)
const AMBER := Color("#ffb347")        # excitatory
const CYAN := Color("#4fd1ff")         # inhibitory
const GREEN := Color("#6be3a0")
const RED := Color("#ff6b6b")
const AMBER_CB := Color("#e69f00")     # Okabe-Ito orange
const CYAN_CB := Color("#56b4e9")      # Okabe-Ito sky blue

## Excitatory / inhibitory colours, honouring the colour-blind setting.
static func exc() -> Color: return AMBER_CB if Settings.colorblind else AMBER
static func inh() -> Color: return CYAN_CB if Settings.colorblind else CYAN

## Smallest font on screen, in PHYSICAL px, asserted by the smoke test (logical size x Kit.ppl).
const MIN_FONT := 14
## Physical px per logical px for text (Layout.text_scale: below 1 on phones, where the logical canvas is shrunk). Set by the app shell.
static var ppl := 1.0
## Smallest logical font size that still lands on MIN_FONT physical px.
static func min_font() -> int: return ceili(float(MIN_FONT) / maxf(ppl, 0.05) - 0.001)
static func fs(n: int) -> int: return maxi(min_font(), n)

static var pad_mode := false       # set by Pad: true while the player navigates with a pad or keys instead of the mouse
static var vh := 1080.0            # logical viewport height, set by the shell on every layout change
static func f() -> float: return clampf(vh / 1080.0, 0.5, 1.0)
static var instant := false        # smoke tests: no tweens
static func focus_mode() -> int: return Control.FOCUS_ALL if pad_mode else Control.FOCUS_NONE
static func motion() -> bool: return not Settings.reduced_motion and not instant

static var _tex_cache := {}
const BENCH_SHADER := preload("res://assets/shaders/bench.gdshader")

# ---------- theme ----------
## The root Theme: default fonts and sizes, scroll bars and tooltips.
static func make_theme() -> Theme:
	var t := Theme.new()
	t.default_font = Fonts.regular; t.default_font_size = fs(18)
	t.set_font("normal_font", "RichTextLabel", Fonts.regular); t.set_font("bold_font", "RichTextLabel", Fonts.bold)
	t.set_color("default_color", "RichTextLabel", TEXT)
	t.set_color("font_color", "Label", TEXT)
	for bar in ["VScrollBar", "HScrollBar"]:
		var track := sb(WELL, WELL, 0, 6, 0, false); track.content_margin_left = 5; track.content_margin_right = 5
		var grab := sb(LINE, LINE, 0, 6, 0, false)
		var grab_hi := sb(inh(), inh(), 0, 6, 0, false)
		t.set_stylebox("scroll", bar, track); t.set_stylebox("scroll_focus", bar, track)
		t.set_stylebox("grabber", bar, grab); t.set_stylebox("grabber_highlight", bar, grab_hi); t.set_stylebox("grabber_pressed", bar, grab_hi)
	t.set_stylebox("panel", "TooltipPanel", sb(PANEL_HI, LINE, 1, 6, 8, false)); t.set_color("font_color", "TooltipLabel", TEXT); t.set_font_size("font_size", "TooltipLabel", fs(16))
	var grabber := _dot(26, AMBER, BENCH, 3); var grabber_hi := _dot(26, TEXT, AMBER, 3)
	for n in ["grabber", "grabber_disabled"]: t.set_icon(n, "HSlider", grabber)
	t.set_icon("grabber_highlight", "HSlider", grabber_hi)
	var rail := sb(WELL, LINE, 1, 8, 0, false); rail.content_margin_top = 5; rail.content_margin_bottom = 5
	var fill := sb(Color(AMBER, 0.55), Color(AMBER, 0.0), 0, 8, 0, false); fill.content_margin_top = 5; fill.content_margin_bottom = 5
	t.set_stylebox("slider", "HSlider", rail); t.set_stylebox("grabber_area", "HSlider", fill); t.set_stylebox("grabber_area_highlight", "HSlider", fill)
	t.set_constant("center_grabber", "HSlider", 0)
	t.set_stylebox("focus", "HSlider", ring_sb())
	return t

static func _dot(size: int, fill: Color, edge: Color, bw: int) -> Texture2D:
	var key := "dot%d%s%s" % [size, fill.to_html(), edge.to_html()]
	if _tex_cache.has(key): return _tex_cache[key]
	var img := Image.create(size, size, false, Image.FORMAT_RGBA8); var c := (size - 1) * 0.5; var r := size * 0.5 - 0.5
	for y in size:
		for x in size:
			var d := Vector2(x - c, y - c).length()
			var a := clampf(r - d + 0.5, 0.0, 1.0)
			var col := edge if d > r - bw else fill
			img.set_pixel(x, y, Color(col.r, col.g, col.b, a))
	var tex := ImageTexture.create_from_image(img); _tex_cache[key] = tex; return tex

# ---------- styleboxes ----------
## The base box: solid fill, thin border, rounded corners, optional soft drop shadow.
static func sb(fill: Color = PANEL, border: Color = LINE, bw := 1, r := 10, pad := 14, shadow := false) -> StyleBoxFlat:
	var s := StyleBoxFlat.new(); s.bg_color = fill; s.border_color = border; s.set_border_width_all(bw); s.set_corner_radius_all(r)
	s.content_margin_left = pad; s.content_margin_right = pad; s.content_margin_top = pad * 0.7; s.content_margin_bottom = pad * 0.7
	s.anti_aliasing = true; s.anti_aliasing_size = 0.8
	if shadow: s.shadow_color = Color(0, 0, 0, 0.45); s.shadow_size = 12; s.shadow_offset = Vector2(0, 4)
	return s

## Focus ring: a bright inner line and a dark outer line, drawn outside the control, so it reads on amber, cyan and panels alike (pad play).
class RingBox extends StyleBox:
	var inner := StyleBoxFlat.new()
	var outer := StyleBoxFlat.new()
	func _init() -> void:
		for s in [inner, outer]:
			s.draw_center = false; s.anti_aliasing = true
		inner.border_color = Color.WHITE; inner.set_border_width_all(3); inner.set_corner_radius_all(14)
		outer.border_color = Kit.BENCH; outer.set_border_width_all(3); outer.set_corner_radius_all(18)
	func _draw(to_canvas_item: RID, rect: Rect2) -> void:
		outer.draw(to_canvas_item, rect.grow(8)); inner.draw(to_canvas_item, rect.grow(5))
	func _get_draw_rect(rect: Rect2) -> Rect2: return rect.grow(10)

static var _ring := RingBox.new()
static func ring_sb() -> StyleBox: return _ring

static func kind_colors(kind: String) -> Array:   # [fill, text, border]
	match kind:
		"cyan": return [inh(), BENCH, inh()]
		"panel": return [PANEL_HI, TEXT, LINE]
		"ghost": return [Color(PANEL, 0.0), TEXT, LINE]
		"danger": return [RED, BENCH, RED]
		"green": return [GREEN, BENCH, GREEN]
	return [exc(), BENCH, exc()]   # "amber": the primary

# ---------- text ----------
static var _ls_cache := {}
static func ls(size: int, col: Color = TEXT, heavy := false) -> LabelSettings:
	size = fs(size)
	var k := "%d|%s|%s|%s|%s" % [size, col.to_html(), str(heavy), str(Settings.colorblind), str(Settings.text_boost)]
	if _ls_cache.has(k): return _ls_cache[k]
	var s := LabelSettings.new(); s.font_size = size; s.font_color = col
	if heavy: s.font = Fonts.bold
	_ls_cache[k] = s; return s

static func label(t: String, size := 18, col: Color = TEXT, wrap := false) -> Label:
	var l := Label.new(); l.text = t; l.label_settings = ls(size, col)
	if wrap: l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART; l.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	return l

## Bold heading text (as written; pass uppercase yourself for small labels).
static func shout(t: String, size := 48, col: Color = TEXT) -> Label:
	var l := Label.new(); l.text = t; l.label_settings = ls(size, col, true); return l

static func spacer(h := 0.0, expand := false) -> Control:
	var c := Control.new(); c.custom_minimum_size = Vector2(0, h); c.mouse_filter = Control.MOUSE_FILTER_IGNORE
	if expand: c.size_flags_vertical = Control.SIZE_EXPAND_FILL; c.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	return c

# ---------- buttons ----------
static var _btn_styles := {}
static func _btn_style_set(kind: String, pad: int) -> Dictionary:
	var key := "%s%d%s" % [kind, pad, str(Settings.colorblind)]
	if _btn_styles.has(key): return _btn_styles[key]
	var cols := kind_colors(kind); var fill: Color = cols[0]; var ink: Color = cols[1]; var edge: Color = cols[2]
	var st := {
		"normal": sb(fill, edge, 1, 10, pad, false), "hover": sb(fill.lightened(0.14), edge.lightened(0.2), 1, 10, pad, true),
		"pressed": sb(fill.darkened(0.14), edge, 1, 10, pad, false), "disabled": sb(PANEL, LINE, 1, 10, pad, false),
		"focus": ring_sb(), "ink": ink}
	_btn_styles[key] = st; return st

## A button. kind: amber (primary), cyan, panel, ghost, danger, green.
static func btn(t: String, cb: Callable, kind := "amber", size := 26, min_size := Vector2.ZERO) -> Button:
	var b := Button.new(); b.text = t; b.focus_mode = focus_mode()
	b.add_theme_font_override("font", Fonts.bold); b.add_theme_font_size_override("font_size", fs(size))
	skin(b, kind, int(size * 0.6))
	b.custom_minimum_size = Vector2(min_size.x, maxf(min_size.y, maxf(size * 1.9, Layout.tap())))
	b.pressed.connect(cb); return b

static func skin(b: Button, kind: String, pad := 16) -> void:
	var st := _btn_style_set(kind, pad)
	for n in ["normal", "hover", "pressed", "disabled", "focus"]: b.add_theme_stylebox_override(n, st[n])
	for c in ["font_color", "font_hover_color", "font_pressed_color", "font_focus_color", "font_hover_pressed_color"]: b.add_theme_color_override(c, st.ink)
	b.add_theme_color_override("font_disabled_color", TEXT_SOFT)
	b.set_meta("kind", kind)

## Small filled label chip (dark text on a colour), e.g. a tab title or E / I tag.
static func chip(t: String, col: Color = AMBER, size := 16) -> PanelContainer:
	var p := PanelContainer.new(); var s := sb(col, col, 0, 6, 8, false); s.content_margin_top = 2; s.content_margin_bottom = 2
	p.add_theme_stylebox_override("panel", s); p.mouse_filter = Control.MOUSE_FILTER_IGNORE
	p.add_child(shout(t, size, BENCH)); return p

# ---------- panels ----------
## An instrument panel: slightly lighter than the bench, hairline border, soft shadow.
static func panel(fill: Color = PANEL, pad := 22) -> PanelContainer:
	var p := PanelContainer.new(); p.add_theme_stylebox_override("panel", sb(fill, LINE, 1, 14, pad, true)); return p

## A section card inside a panel: small coloured title, body below. Returns [card, body_vbox].
static func card(title := "", fill: Color = PANEL_HI, accent: Color = AMBER) -> Array:
	var p := PanelContainer.new(); p.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	p.add_theme_stylebox_override("panel", sb(fill, LINE, 1, 10, 16, false))
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", 10); p.add_child(v)
	if title != "":
		var h := HBoxContainer.new(); h.add_theme_constant_override("separation", 10)
		var bar := ColorRect.new(); bar.color = accent; bar.custom_minimum_size = Vector2(4, 0); bar.mouse_filter = Control.MOUSE_FILTER_IGNORE; h.add_child(bar)
		h.add_child(shout(title.to_upper(), 20, accent)); v.add_child(h)
	return [p, v]

## The bench backdrop: near-black navy with a faint grid and vignette. Resizes with its rect.
static func bench() -> ColorRect:
	var r := ColorRect.new(); r.set_anchors_preset(Control.PRESET_FULL_RECT); r.mouse_filter = Control.MOUSE_FILTER_IGNORE; r.color = BENCH
	var m := ShaderMaterial.new(); m.shader = BENCH_SHADER
	r.material = m
	var fit := func(): m.set_shader_parameter("area", r.size)
	r.resized.connect(fit); fit.call(); return r

# ---------- rows (settings) ----------
## label on the left, control on the right
static func row(text: String, ctl: Control, hint := "") -> Control:
	var h := HBoxContainer.new(); h.add_theme_constant_override("separation", 16); h.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var left := VBoxContainer.new(); left.size_flags_horizontal = Control.SIZE_EXPAND_FILL; left.add_theme_constant_override("separation", 0); left.alignment = BoxContainer.ALIGNMENT_CENTER
	left.add_child(label(text, 20, TEXT))
	if hint != "": left.add_child(label(hint, 15, TEXT_SOFT, true))
	h.add_child(left); h.add_child(ctl); return h

## A faint divider between rows.
static func hsep() -> HSeparator:
	var h := HSeparator.new(); var l := StyleBoxLine.new(); l.color = Color(LINE, 0.7); l.thickness = 1; h.add_theme_stylebox_override("separator", l); return h

static func toggle(on: bool, cb: Callable) -> Button:
	var b := Button.new(); b.toggle_mode = true; b.button_pressed = on; b.focus_mode = focus_mode(); b.custom_minimum_size = Vector2(110, maxf(46.0, Layout.tap()))
	b.add_theme_font_override("font", Fonts.bold); b.add_theme_font_size_override("font_size", fs(22))
	var paint := func(v: bool): b.text = "ON" if v else "OFF"; skin(b, "green" if v else "panel", 10)
	paint.call(on)
	b.toggled.connect(func(v): paint.call(v); cb.call(v)); return b

## The control for a slider row: slider + percentage readout. cb receives 0..1.
static func slider(value: float, cb: Callable, width := 320.0) -> Control:
	var h := HBoxContainer.new(); h.add_theme_constant_override("separation", 14)
	var s := HSlider.new(); s.min_value = 0.0; s.max_value = 1.0; s.step = 0.05; s.value = value; s.focus_mode = focus_mode()
	s.custom_minimum_size = Vector2(width, maxf(34.0, Layout.tap())); s.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	var v := label("%d%%" % roundi(value * 100.0), 20, TEXT); v.custom_minimum_size.x = 62; v.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	s.value_changed.connect(func(x): v.text = "%d%%" % roundi(x * 100.0); cb.call(x))
	h.add_child(s); h.add_child(v); return h

## Option stepper: [<] value [>]. Left/right (keys, pad D-pad), clicks on the arrows, or A/Enter (next) change it.
class Stepper extends HBoxContainer:
	var opts: Array = []   # [[id, name], ...]
	var idx := 0
	var cb: Callable
	var val: Label
	var left: Button
	var right: Button
	func setup(options: Array, selected: String, on_change: Callable, width := 240.0, fsize := 22, aw := 54.0) -> Stepper:
		opts = options; cb = on_change
		for i in opts.size():
			if str(opts[i][0]) == selected: idx = i
		add_theme_constant_override("separation", 8)
		left = Kit.btn("<", func(): step(-1), "panel", fsize + 2, Vector2(aw, 0)); right = Kit.btn(">", func(): step(1), "panel", fsize + 2, Vector2(aw, 0))
		left.focus_mode = Control.FOCUS_NONE; right.focus_mode = Control.FOCUS_NONE
		var chipp := PanelContainer.new(); chipp.add_theme_stylebox_override("panel", Kit.sb(Kit.WELL, Kit.LINE, 1, 10, 10, false)); chipp.custom_minimum_size.x = width
		val = Kit.shout(str(opts[idx][1]), fsize, Kit.TEXT); val.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER; val.size_flags_horizontal = Control.SIZE_EXPAND_FILL; chipp.add_child(val)
		chipp.mouse_filter = Control.MOUSE_FILTER_IGNORE
		add_child(left); add_child(chipp); add_child(right)
		focus_mode = Kit.focus_mode(); add_theme_stylebox_override("focus", Kit.ring_sb())
		focus_entered.connect(queue_redraw); focus_exited.connect(queue_redraw)
		return self
	func step(d: int) -> void:
		if opts.is_empty(): return
		idx = posmod(idx + d, opts.size()); val.text = str(opts[idx][1]); cb.call(opts[idx][0])
	func _gui_input(e: InputEvent) -> void:
		if not has_focus(): return
		if e.is_action_pressed("ui_left"): step(-1); accept_event()
		elif e.is_action_pressed("ui_right") or e.is_action_pressed("ui_accept"): step(1); accept_event()
	func _draw() -> void:
		if has_focus(): draw_style_box(Kit.ring_sb(), Rect2(Vector2.ZERO, size))

static func stepper(options: Array, selected: String, cb: Callable, width := 240.0, fsize := 22, aw := 54.0) -> Stepper:
	return Stepper.new().setup(options, selected, cb, width, fsize, aw)

## Tab row; the current tab is amber.
static func tabs(names: Array, current: String, cb: Callable) -> HBoxContainer:
	var h := HBoxContainer.new(); h.add_theme_constant_override("separation", 10); h.alignment = BoxContainer.ALIGNMENT_BEGIN
	for n in names:
		var on: bool = n == current
		var b := btn(n, func(): cb.call(n), "amber" if on else "panel", 22 if Layout.phone else 24)
		b.set_meta("tab", n); h.add_child(b)
	return h

## Fade (+ slide) a control in. Reduced motion or smoke tests: no motion (reduced motion keeps a short fade).
static func appear(c: Control, from := Vector2(0, 0), dur := 0.22) -> void:
	if c == null or not c.is_inside_tree() or instant: return
	if c.has_meta("appear"):
		var old: Tween = c.get_meta("appear")
		if old != null and old.is_valid(): old.kill()
	c.modulate.a = 0.0
	var tw := c.create_tween(); c.set_meta("appear", tw)
	if Settings.reduced_motion or from == Vector2.ZERO:
		tw.tween_property(c, "modulate:a", 1.0, 0.08 if Settings.reduced_motion else dur); return
	tw.set_parallel(true)
	var p0 := c.position
	c.position = p0 + from
	tw.tween_property(c, "position", p0, dur).set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_OUT)
	tw.tween_property(c, "modulate:a", 1.0, dur * 0.7)
