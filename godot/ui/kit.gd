class_name Kit
## Shared UI toolkit: the "Cajal Plate" visual language (docs/IDENTITY.md) and the builders every screen uses.
## A 19th-century anatomical plate: aged paper, India-ink outlines and type, Golgi-stain sienna = excitatory, Prussian blue = inhibitory,
## verdigris sensors, madder-plum motors; Fraunces display serif, Atkinson Hyperlegible body, IBM Plex Mono readouts.
## Token NAMES are kept from 0.1-0.3 so every screen keeps compiling; their VALUES are the new identity. Prefer the semantic aliases below.
## Palette values are constants; sizes are logical px at 1080p (the window stretch scales them).
##
## ---------- helpers for canvas painters (gfx, anatomy, views, map...) ----------
## Colours      Kit.PAPER CARD CARD_HI INK INK_SOFT RULE GRID EXC INH SENSOR MOTOR GAMBOGE OCHRE SUCCESS RED; Kit.exc() / Kit.inh() honour the
##              colour-blind setting. Kit.legible(col) darkens any colour until it reads at 4.5:1 on paper: use it for SMALL TEXT drawn in a
##              hue (sienna, verdigris, viridian, ochre, gamboge are all below 4.5:1 as they are). Kit.EXC_INK is the legible sienna.
##              Kit.on(fill) is the text colour to letter ON a filled shape. Kit.contrast(a, b) is the WCAG ratio.
## Fonts        Fonts.display (Fraunces 600) / display_med / display_heavy, Fonts.italic / italic_bold (captions), Fonts.mono / mono_med (readouts),
##              Fonts.regular / bold (Atkinson body). Kit.fs(n) is the size floor; never draw text below it.
## Plates       Kit.draw_plate(ci, rect, fill := CARD, shadow := RULE)  ink outline + hard 5 px offset shadow (a card drawn in _draw)
##              Kit.draw_graph_paper(ci, rect, cell := 24.0)        blue engineering graph paper (minor + every-5th major lines)
##              Kit.draw_fig(ci, pos, n, title, width, right := "", size := 19) -> height   "Fig. N." + italic caption + mono readout at right
##              Kit.draw_wash(ci, centre, radius, col, amount, seed := 0)  a watercolour stain blot (firing neurons); amount 0..1
##              Kit.draw_stamp(ci, centre, radius, big, top, bottom, col, angle := -0.14)  a rubber stamp (scores); ink texture is seeded
##              Kit.draw_leader(ci, a, b, col := RULE)               a dotted leader line (menus, call-outs)
##              Kit.draw_hatch(ci, rect, col, step := 7.0)           diagonal hatching (meter fills, as in the mockup's motor bars)
##              Kit.draw_ink_text(ci, font, pos, text, size, col)   text that respects the size floor and legibility
## Specimens    Kit.Cajal.pyramidal(rng, origin, scale) / basket(...) / stellate(...) -> {strokes, axon, soma};
##              Kit.Cajal.draw(ci, cell, col, alpha := 1.0, offset := Vector2.ZERO, k := 1.0) inks it (crisp vector polylines, any resolution).
## Controls     Kit.ink_card(fill := CARD, pad := 20, shadow := RULE) -> PanelContainer   a plate as a container
##              Kit.fig_card(n, title, right := "", fill := CARD) -> [card, body_vbox]  a numbered "Fig. N" panel
##              Kit.fig_header(n, title, right := "", size := 21) -> HBoxContainer  the header row alone
##              Kit.fig_caption(n, text, size := 19) -> Control   "Fig. N." + italic caption (wraps)
##              Kit.legend_item(numeral, text, sub, cb, primary := false, size := 34) -> Button   numbered menu entry with leader dots
##              Kit.display(text, size, col) / Kit.italic(text, size, col) / Kit.mono(text, size, col) -> Label in that face
##              Kit.small_caps(text, size, col) -> Label   letter-spaced plate lettering ("PLATE I · ...")
##              Kit.stamp(big, top, bottom, col, size := 140.0) -> Control   a rubber-stamp score (lands with a thump unless reduced motion)

const BENCH := Color("#EFE6D4")        # the page: aged paper
const PANEL := Color("#F8F2E6")        # a card / plate on the page
const PANEL_HI := Color("#FFFBF2")     # raised / hovered card
const WELL := Color("#F4EDE0")         # recessed area (scopes sit on graph paper drawn over this)
const LINE := Color("#CDBFA3")         # rules and hairlines
const TEXT := Color("#231C17")         # India ink
const TEXT_SOFT := Color("#5E5246")    # soft ink (6:1 on paper)
const AMBER := Color("#B8461B")        # excitatory: Golgi sienna (name kept for compatibility)
const CYAN := Color("#1E4F8F")         # inhibitory: Prussian blue (name kept for compatibility)
const GREEN := Color("#2E7D4F")        # success: viridian
const RED := Color("#A3261C")          # error: vermilion ink
## Colour-blind setting: sienna / Prussian blue already stay apart under protan, deutan and tritan simulation (Machado 2009, dE2000 > 40);
## the setting also pulls them apart in lightness (a lighter orange, a deeper blue), so they differ even in greyscale.
const AMBER_CB := Color("#D0661E")
const CYAN_CB := Color("#163C6E")
# semantic aliases (new code should use these)
const PAPER := BENCH
const CARD := PANEL
const CARD_HI := PANEL_HI
const INK := TEXT
const INK_SOFT := TEXT_SOFT
const RULE := LINE
const GRID := Color("#D3DCE0")         # engineering graph-paper grid for the bench and scopes
const GRID_MAJOR := Color("#B9C8CF")
const EXC := AMBER
const INH := CYAN
const SENSOR := Color("#12808F")       # verdigris
const MOTOR := Color("#6E2A4E")        # madder plum
const GAMBOGE := Color("#C9971C")      # highlight / selection
const OCHRE := Color("#B7791F")        # warning
const SUCCESS := GREEN
const EXC_INK := Color("#9A3814")      # sienna dark enough for small text (5.6:1 on paper)
const EXC_TINT := Color("#E9B48F")     # sienna on ink (numerals of the primary menu entry, 9:1)
const SHADOW := 5.0                    # the hard letterpress offset of a card
const OUTLINE := 2                     # ink outline width

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
const BENCH_SHADER := preload("res://assets/shaders/paper.gdshader")
const PAPER_SHADER := BENCH_SHADER

# ---------- contrast ----------
static func _lum(c: Color) -> float:
	var ch := [c.r, c.g, c.b]
	for i in 3: ch[i] = ch[i] / 12.92 if ch[i] <= 0.03928 else pow((ch[i] + 0.055) / 1.055, 2.4)
	return 0.2126 * ch[0] + 0.7152 * ch[1] + 0.0722 * ch[2]
## WCAG contrast ratio of two opaque colours.
static func contrast(a: Color, b: Color) -> float:
	var la := _lum(a); var lb := _lum(b)
	return (maxf(la, lb) + 0.05) / (minf(la, lb) + 0.05)
static var _legible := {}
## The colour, darkened just enough to read at 4.5:1 on paper (and so on every card, which is lighter). Colours that pass are returned as is.
static func legible(c: Color, ratio := 4.6) -> Color:
	var k := c.to_html()
	if _legible.has(k): return _legible[k]
	var o := Color(c, 1.0); var n := 0
	while contrast(o, PAPER) < ratio and n < 40:
		o = o.darkened(0.04); n += 1
	o.a = c.a; _legible[k] = o; return o
## Text colour to put ON a filled colour: card white on dark fills, ink on light ones.
static func on(fill: Color) -> Color: return PANEL if contrast(PANEL, fill) >= contrast(TEXT, fill) else TEXT

# ---------- theme ----------
## The root Theme: default fonts and sizes, scroll bars and tooltips.
static func make_theme() -> Theme:
	var t := Theme.new()
	t.default_font = Fonts.regular; t.default_font_size = fs(18)
	t.set_font("normal_font", "RichTextLabel", Fonts.regular); t.set_font("bold_font", "RichTextLabel", Fonts.bold)
	t.set_font("italics_font", "RichTextLabel", Fonts.italic); t.set_font("mono_font", "RichTextLabel", Fonts.mono)
	t.set_color("default_color", "RichTextLabel", TEXT)
	t.set_color("font_color", "Label", TEXT)
	t.set_font("font", "Button", Fonts.display)
	for bar in ["VScrollBar", "HScrollBar"]:
		var track := sb(Color(LINE, 0.35), Color(LINE, 0.0), 0, 0, 0, false); track.content_margin_left = 4; track.content_margin_right = 4; track.content_margin_top = 4; track.content_margin_bottom = 4
		var grab := sb(TEXT_SOFT, TEXT_SOFT, 0, 0, 0, false)
		var grab_hi := sb(TEXT, TEXT, 0, 0, 0, false)
		t.set_stylebox("scroll", bar, track); t.set_stylebox("scroll_focus", bar, track)
		t.set_stylebox("grabber", bar, grab); t.set_stylebox("grabber_highlight", bar, grab_hi); t.set_stylebox("grabber_pressed", bar, grab_hi)
	var tip := sb(PANEL_HI, TEXT, OUTLINE, 2, 10, true); t.set_stylebox("panel", "TooltipPanel", tip)
	t.set_color("font_color", "TooltipLabel", TEXT); t.set_font_size("font_size", "TooltipLabel", fs(16)); t.set_font("font", "TooltipLabel", Fonts.regular)
	var grabber := _dot(26, exc(), TEXT, 3); var grabber_hi := _dot(26, TEXT, exc(), 3)
	for n in ["grabber", "grabber_disabled"]: t.set_icon(n, "HSlider", grabber)
	t.set_icon("grabber_highlight", "HSlider", grabber_hi)
	var rail := sb(PANEL_HI, TEXT, OUTLINE, 0, 0, false); rail.content_margin_top = 4; rail.content_margin_bottom = 4
	var fill := StyleBoxFlat.new(); fill.bg_color = exc(); fill.content_margin_top = 4; fill.content_margin_bottom = 4
	fill.border_color = TEXT; fill.border_width_top = OUTLINE; fill.border_width_bottom = OUTLINE; fill.border_width_left = OUTLINE
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
			var e := clampf(d - (r - bw) + 0.5, 0.0, 1.0)
			var col := fill.lerp(edge, e)
			img.set_pixel(x, y, Color(col.r, col.g, col.b, a))
	var tex := ImageTexture.create_from_image(img); _tex_cache[key] = tex; return tex

# ---------- styleboxes ----------
## The base box: a flat fill, a thin border and square-ish corners (radius is capped at 4: plates are cut, not moulded).
## shadow = true makes it a PLATE: the border turns to ink (if it was the hairline rule) and it gets the hard 5 px letterpress offset.
static func sb(fill: Color = PANEL, border: Color = LINE, bw := 1, r := 10, pad := 14, shadow := false) -> StyleBoxFlat:
	var s := StyleBoxFlat.new(); s.bg_color = fill; s.border_color = border; s.set_border_width_all(bw); s.set_corner_radius_all(mini(r, 4))
	s.content_margin_left = pad; s.content_margin_right = pad; s.content_margin_top = pad * 0.7; s.content_margin_bottom = pad * 0.7
	s.anti_aliasing = true; s.anti_aliasing_size = 0.6
	if shadow:
		if border == LINE: s.border_color = TEXT; s.set_border_width_all(maxi(bw, OUTLINE))
		_hard_shadow(s, LINE)
	return s

static func _hard_shadow(s: StyleBoxFlat, col: Color, off := SHADOW) -> void:
	s.shadow_color = col; s.shadow_size = 1; s.shadow_offset = Vector2(off - 1.0, off - 1.0)

## A plate stylebox: ink outline, hard offset shadow (in `shadow` colour; a transparent colour means none).
static func plate_sb(fill: Color = PANEL, pad := 20, shadow: Color = LINE, border: Color = TEXT, off := SHADOW) -> StyleBoxFlat:
	var s := sb(fill, border, OUTLINE, 3, pad, false)
	if shadow.a > 0.0: _hard_shadow(s, shadow, off)
	return s

## Focus ring: an ink line hugging the control and a gamboge line outside it, so it reads on paper, cards and ink-filled buttons (pad play).
class RingBox extends StyleBox:
	var inner := StyleBoxFlat.new()
	var outer := StyleBoxFlat.new()
	func _init() -> void:
		for s in [inner, outer]:
			s.draw_center = false; s.anti_aliasing = true
		inner.border_color = Kit.TEXT; inner.set_border_width_all(3); inner.set_corner_radius_all(4)
		outer.border_color = Kit.GAMBOGE; outer.set_border_width_all(3); outer.set_corner_radius_all(6)
	func _draw(to_canvas_item: RID, rect: Rect2) -> void:
		outer.draw(to_canvas_item, rect.grow(9)); inner.draw(to_canvas_item, rect.grow(6))
	func _get_draw_rect(rect: Rect2) -> Rect2: return rect.grow(10)

static var _ring := RingBox.new()
static func ring_sb() -> StyleBox: return _ring

## [fill, text, border, offset-shadow colour] of a button kind.
static func kind_colors(kind: String) -> Array:
	match kind:
		"cyan": return [inh(), PANEL, TEXT, TEXT]
		"panel": return [PANEL, TEXT, TEXT, LINE]
		"ghost": return [Color(PANEL, 0.0), TEXT, LINE, Color(0, 0, 0, 0)]
		"danger": return [RED, PANEL, TEXT, TEXT]
		"green": return [GREEN, PANEL, TEXT, TEXT]
	return [TEXT, PANEL, TEXT, exc()]   # "amber": the primary, ink-filled with a sienna offset

# ---------- text ----------
static var _ls_cache := {}
## Label settings in Atkinson (heavy = bold). Small text in a hue is darkened by legible() so it always reads at 4.5:1.
static func ls(size: int, col: Color = TEXT, heavy := false) -> LabelSettings:
	return ls_font(Fonts.bold if heavy else null, size, col)

## Label settings in any face (null = the theme's Atkinson regular).
static func ls_font(font: Font, size: int, col: Color = TEXT, spacing := 0) -> LabelSettings:
	size = fs(size)
	var bold := font == Fonts.bold or font == Fonts.display or font == Fonts.display_heavy or font == Fonts.italic_bold
	var large := size >= (19 if bold else 24)
	if not large and col.a > 0.5 and col.get_luminance() < 0.55: col = legible(col)   # light colours are lettering on ink fills: left alone
	var k := "%d|%s|%s|%d|%s|%s" % [size, col.to_html(), str(font.get_instance_id() if font != null else 0), spacing, str(Settings.colorblind), str(Settings.text_boost)]
	if _ls_cache.has(k): return _ls_cache[k]
	var s := LabelSettings.new(); s.font_size = size; s.font_color = col
	if font != null:
		if spacing != 0:
			var v := FontVariation.new(); v.base_font = font; v.spacing_glyph = spacing; s.font = v
		else: s.font = font
	_ls_cache[k] = s; return s

static func label(t: String, size := 18, col: Color = TEXT, wrap := false) -> Label:
	var l := Label.new(); l.text = t; l.label_settings = ls(size, col)
	if wrap: l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART; l.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	return l

## Heading text: Fraunces at 22 px and up (the plate's lettering), Atkinson bold below that (small labels stay hyperlegible).
static func shout(t: String, size := 48, col: Color = TEXT) -> Label:
	var l := Label.new(); l.text = t; l.label_settings = ls_font(Fonts.display if size >= 22 else Fonts.bold, size, col); return l

static func display(t: String, size := 48, col: Color = TEXT, heavy := false) -> Label:
	var l := Label.new(); l.text = t; l.label_settings = ls_font(Fonts.display_heavy if heavy else Fonts.display, size, col); return l
static func italic(t: String, size := 20, col: Color = TEXT_SOFT, wrap := false) -> Label:
	var l := Label.new(); l.text = t; l.label_settings = ls_font(Fonts.italic, size, col)
	if wrap: l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART; l.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	return l
static func mono(t: String, size := 16, col: Color = TEXT_SOFT) -> Label:
	var l := Label.new(); l.text = t; l.label_settings = ls_font(Fonts.mono, size, col); return l
## Letter-spaced capitals in Fraunces: the running head of a plate ("PLATE I · HISTOLOGY ...").
static func small_caps(t: String, size := 15, col: Color = TEXT_SOFT) -> Label:
	var l := Label.new(); l.text = t.to_upper(); l.label_settings = ls_font(Fonts.display, size, col, maxi(1, int(round(fs(size) * 0.14)))); return l

## The page head every full screen shares: a ghost "← Back", the page title in Fraunces, then whatever the screen adds (returned row, back button).
static func page_head(title: String, back_cb: Callable, phone := false) -> Array:
	var h := HBoxContainer.new(); h.add_theme_constant_override("separation", 14 if not phone else 8)
	var b := btn("← Back", back_cb, "ghost", 22 if not phone else 18, Vector2(0, 0)); b.size_flags_vertical = Control.SIZE_SHRINK_CENTER; h.add_child(b)
	var t := display(title, int(50 * f()) if not phone else 28, TEXT); t.size_flags_vertical = Control.SIZE_SHRINK_CENTER; t.name = "PageTitle"; h.add_child(t)
	return [h, b]

## The plate's running head at the right of a page head ("Plate III · the instrument"): expands to push itself right.
static func running_head(t: String) -> Label:
	var run := small_caps(t, 16, TEXT_SOFT); run.size_flags_horizontal = Control.SIZE_EXPAND_FILL; run.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	run.size_flags_vertical = Control.SIZE_SHRINK_CENTER; return run

static func spacer(h := 0.0, expand := false) -> Control:
	var c := Control.new(); c.custom_minimum_size = Vector2(0, h); c.mouse_filter = Control.MOUSE_FILTER_IGNORE
	if expand: c.size_flags_vertical = Control.SIZE_EXPAND_FILL; c.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	return c

# ---------- buttons ----------
static var _btn_styles := {}
static func _btn_style_set(kind: String, pad: int) -> Dictionary:
	var key := "%s%d%s" % [kind, pad, str(Settings.colorblind)]
	if _btn_styles.has(key): return _btn_styles[key]
	var cols := kind_colors(kind); var fill: Color = cols[0]; var ink: Color = cols[1]; var edge: Color = cols[2]; var sh: Color = cols[3]
	var off := 4.0
	var normal := plate_sb(fill, pad, sh, edge, off)
	var hover: StyleBoxFlat
	if kind == "ghost": hover = plate_sb(PANEL, pad, Color(0, 0, 0, 0), TEXT, off)
	elif kind == "panel": hover = plate_sb(PANEL_HI, pad, TEXT_SOFT, edge, off)
	else: hover = plate_sb(fill.lightened(0.1) if fill.get_luminance() < 0.2 else fill.darkened(0.08), pad, sh, edge, off)
	var pressed := plate_sb(fill if kind != "ghost" else PANEL, pad, Color(0, 0, 0, 0), edge, off)
	pressed.expand_margin_left = -2.0; pressed.expand_margin_right = 2.0; pressed.expand_margin_top = -2.0; pressed.expand_margin_bottom = 2.0   # pressed into its shadow
	var disabled := plate_sb(Color(PANEL, 0.6), pad, Color(0, 0, 0, 0), LINE, off)
	var st := {"normal": normal, "hover": hover, "pressed": pressed, "disabled": disabled, "focus": ring_sb(), "ink": ink}
	_btn_styles[key] = st; return st

## A button: Fraunces lettering on a cut card. kind: amber (primary: ink-filled, sienna offset), panel (outlined card), ghost, cyan (Prussian blue),
## danger (vermilion), green (viridian). Labels in capitals get a little tracking, like engraved plate lettering.
static func btn(t: String, cb: Callable, kind := "amber", size := 26, min_size := Vector2.ZERO) -> Button:
	var b := Button.new(); b.text = label_case(t); b.focus_mode = focus_mode()
	_btn_font(b, b.text, size)
	skin(b, kind, int(size * 0.6))
	b.custom_minimum_size = Vector2(min_size.x, maxf(min_size.y, maxf(size * 1.9, Layout.tap())))
	b.pressed.connect(cb); return b

## Words kept in capitals by label_case (acronyms and units).
const KEEP_CAPS := ["E", "I", "JSON", "CSV", "HRV", "PRC", "ISI", "CV", "CPG", "ATP", "NEST", "ID", "UI", "OK", "SA", "AV", "EMG", "ECG"]
## A button label set in capitals by older code ("SHOW RECIPE") as the plate's sentence case ("Show recipe"). Words with digits, underscores or
## other marks (ids, "x5") and the acronyms in KEEP_CAPS stay as written. Labels already in mixed case are returned unchanged.
static func label_case(t: String) -> String:
	if t.length() < 2: return t
	var words := t.split(" "); var out := PackedStringArray(); var first_done := false
	for w in words:   # mixed case already (digits aside, as in "x5")?
		var has_digit := false
		for ch in w: has_digit = has_digit or (ch >= "0" and ch <= "9")
		if not has_digit and w != w.to_upper(): return t
	for w in words:
		var letters := true
		for ch in w:
			if not (ch >= "A" and ch <= "Z"): letters = false
		var keep := not letters or w in KEEP_CAPS or w.length() < 2
		var o: String = w if keep else w.to_lower()
		if not first_done and letters and o.length() > 0:
			o = o.substr(0, 1).to_upper() + o.substr(1); first_done = true
		elif not first_done and o.length() > 0 and o != "+" and o != "-": first_done = true
		out.append(o)
	return " ".join(out)

static var _caps_fonts := {}
static func _btn_font(b: Button, t: String, size: int) -> void:
	var caps := t == t.to_upper() and t.length() > 1
	var sp := clampi(int(round(fs(size) * 0.06)), 1, 3) if caps else 0
	if not _caps_fonts.has(sp):
		var v := FontVariation.new(); v.base_font = Fonts.display; v.spacing_glyph = sp; _caps_fonts[sp] = v
	b.add_theme_font_override("font", _caps_fonts[sp]); b.add_theme_font_size_override("font_size", fs(size))

static func skin(b: Button, kind: String, pad := 16) -> void:
	var st := _btn_style_set(kind, pad)
	for n in ["normal", "hover", "pressed", "disabled", "focus"]: b.add_theme_stylebox_override(n, st[n])
	for c in ["font_color", "font_hover_color", "font_pressed_color", "font_focus_color", "font_hover_pressed_color"]: b.add_theme_color_override(c, st.ink)
	b.add_theme_color_override("font_disabled_color", TEXT_SOFT)
	b.set_meta("kind", kind)

## Small label chip: an index-card tab outlined in its colour (a thick left edge), lettered in the legible shade of that colour.
## Pass a light colour (PANEL_HI...) for a neutral chip. The chip's first child is its Label.
static func chip(t: String, col: Color = AMBER, size := 16) -> PanelContainer:
	var p := PanelContainer.new()
	var light := col.get_luminance() > 0.7
	var edge := LINE if light else col
	var s := sb(PANEL_HI, edge, 2, 2, 8, false); s.border_width_left = 6
	s.content_margin_top = 2; s.content_margin_bottom = 2; s.content_margin_left = 10
	p.add_theme_stylebox_override("panel", s); p.mouse_filter = Control.MOUSE_FILTER_IGNORE
	p.add_child(shout(t, size, TEXT if light else col)); return p

# ---------- panels ----------
## A plate: card fill, ink outline, hard 5 px offset shadow. (Dark fills from older screens are mapped to the recessed paper tone.)
static func panel(fill: Color = PANEL, pad := 22) -> PanelContainer:
	if fill.get_luminance() < 0.25: fill = WELL
	var p := PanelContainer.new(); p.add_theme_stylebox_override("panel", plate_sb(fill, pad)); return p
static func ink_card(fill: Color = PANEL, pad := 20, shadow: Color = LINE) -> PanelContainer:
	var p := PanelContainer.new(); p.add_theme_stylebox_override("panel", plate_sb(fill, pad, shadow)); return p

## A section card inside a panel: a small-caps title over a short rule in the accent colour, body below. Returns [card, body_vbox].
static func card(title := "", fill: Color = PANEL_HI, accent: Color = AMBER) -> Array:
	var p := PanelContainer.new(); p.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	p.add_theme_stylebox_override("panel", sb(fill, LINE, 1, 2, 18, false))
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", 10); p.add_child(v)
	if title != "":
		var h := VBoxContainer.new(); h.add_theme_constant_override("separation", 4)
		h.add_child(small_caps(title, 16, TEXT))
		var bar := ColorRect.new(); bar.color = accent; bar.custom_minimum_size = Vector2(36, 3); bar.size_flags_horizontal = Control.SIZE_SHRINK_BEGIN; bar.mouse_filter = Control.MOUSE_FILTER_IGNORE; h.add_child(bar)
		v.add_child(h)
	return [p, v]

## "Fig. N." in upright Fraunces, the caption in italic, an optional mono readout at the right.
static func fig_header(n, title: String, right := "", size := 21) -> HBoxContainer:
	var h := HBoxContainer.new(); h.add_theme_constant_override("separation", int(size * 0.5)); h.mouse_filter = Control.MOUSE_FILTER_IGNORE
	if str(n) != "":
		var nl := display("Fig. %s." % str(n), size, TEXT); nl.size_flags_vertical = Control.SIZE_SHRINK_END; h.add_child(nl)
	var tl := italic(title, size, TEXT); tl.size_flags_horizontal = Control.SIZE_EXPAND_FILL; tl.size_flags_vertical = Control.SIZE_SHRINK_END
	tl.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS; tl.custom_minimum_size.x = 60; h.add_child(tl)
	if right != "":
		var rl := mono(right, maxi(15, size - 5), TEXT_SOFT); rl.size_flags_vertical = Control.SIZE_SHRINK_END; h.add_child(rl)
	return h

## A numbered plate panel: [card, body]. The header is fig_header(n, title, right).
static func fig_card(n, title: String, right := "", fill: Color = PANEL, pad := 20, size := 21) -> Array:
	var p := ink_card(fill, pad); p.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", 10); p.add_child(v)
	v.add_child(fig_header(n, title, right, size))
	return [p, v]

## "Fig. N." + an italic caption that wraps: the legend under a plate.
static func fig_caption(n, text: String, size := 19, col: Color = TEXT_SOFT) -> Control:
	var h := HBoxContainer.new(); h.add_theme_constant_override("separation", int(size * 0.5)); h.mouse_filter = Control.MOUSE_FILTER_IGNORE
	if str(n) != "":
		var nl := display("Fig. %s." % str(n), size, TEXT); nl.size_flags_vertical = Control.SIZE_SHRINK_BEGIN; h.add_child(nl)
	h.add_child(italic(text, size, col, true))
	return h

## A numbered legend entry (title menu): "I.  Play ........ the campaign". A real Button (focus, pad, smoke) with its text drawn by child labels.
## primary: ink-filled with a sienna offset; else a ruled line with leader dots.
static func legend_item(numeral: String, text: String, sub: String, cb: Callable, primary := false, size := 34) -> Button:
	var b := Button.new(); b.focus_mode = focus_mode(); b.text = ""
	b.set_meta("legend", text); b.name = "Legend" + text.replace(" ", "")
	var h := float(size) * (2.15 if primary else 1.7)
	b.custom_minimum_size = Vector2(0, maxf(h, Layout.tap())); b.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var pad := int(size * 0.8)
	if primary:
		var n := plate_sb(TEXT, 0, exc(), TEXT, SHADOW); var hv := plate_sb(Color("#3A2F27"), 0, exc(), TEXT, SHADOW)
		var pr := plate_sb(TEXT, 0, Color(0, 0, 0, 0), TEXT, SHADOW); pr.expand_margin_left = -3; pr.expand_margin_right = 3; pr.expand_margin_top = -3; pr.expand_margin_bottom = 3
		b.add_theme_stylebox_override("normal", n); b.add_theme_stylebox_override("hover", hv); b.add_theme_stylebox_override("pressed", pr)
	else:
		var n := StyleBoxFlat.new(); n.bg_color = Color(PANEL, 0.0); n.border_color = LINE; n.border_width_bottom = OUTLINE
		var hv := n.duplicate() as StyleBoxFlat; hv.bg_color = Color(PANEL_HI, 0.7); hv.border_color = TEXT
		for k in ["normal", "disabled"]: b.add_theme_stylebox_override(k, n)
		b.add_theme_stylebox_override("hover", hv); b.add_theme_stylebox_override("pressed", hv)
	b.add_theme_stylebox_override("focus", ring_sb())
	var row := HBoxContainer.new(); row.set_anchors_preset(Control.PRESET_FULL_RECT); row.mouse_filter = Control.MOUSE_FILTER_IGNORE
	row.offset_left = pad; row.offset_right = -pad; row.add_theme_constant_override("separation", int(size * 0.45)); b.add_child(row)
	var num := Label.new(); num.text = numeral + "."; num.label_settings = ls_font(Fonts.italic, int(size * 0.72), EXC_TINT if primary else TEXT_SOFT)
	num.custom_minimum_size.x = size * 1.6; num.size_flags_vertical = Control.SIZE_SHRINK_CENTER; num.mouse_filter = Control.MOUSE_FILTER_IGNORE; row.add_child(num)
	var nm := Label.new(); nm.text = text; nm.label_settings = ls_font(Fonts.display if primary else Fonts.display_med, size if primary else int(size * 0.88), PANEL if primary else TEXT)
	nm.size_flags_vertical = Control.SIZE_SHRINK_CENTER; nm.mouse_filter = Control.MOUSE_FILTER_IGNORE; row.add_child(nm)
	var lead := Leader.new(); lead.size_flags_horizontal = Control.SIZE_EXPAND_FILL; lead.visible_dots = not primary; row.add_child(lead)
	if sub != "":
		var s := Label.new(); s.text = sub; s.label_settings = ls_font(Fonts.italic, int(size * 0.66), Color("#DDD2BE") if primary else TEXT_SOFT)
		s.size_flags_vertical = Control.SIZE_SHRINK_CENTER; s.mouse_filter = Control.MOUSE_FILTER_IGNORE; row.add_child(s)
	if primary:
		var ar := Label.new(); ar.text = "→"; ar.label_settings = ls_font(Fonts.regular, int(size * 0.8), PANEL); ar.size_flags_vertical = Control.SIZE_SHRINK_CENTER; ar.mouse_filter = Control.MOUSE_FILTER_IGNORE; row.add_child(ar)
	b.pressed.connect(cb)
	return b

## The dotted leader between a menu entry and its note.
class Leader extends Control:
	var visible_dots := true
	func _init() -> void: mouse_filter = Control.MOUSE_FILTER_IGNORE
	func _draw() -> void:
		if visible_dots: Kit.draw_leader(self, Vector2(6, size.y * 0.62), Vector2(size.x - 6, size.y * 0.62))

## The paper backdrop: rag paper with long fibres, fine tooth and faint foxing. Resizes with its rect. (Named bench() for older screens.)
static func bench() -> ColorRect:
	var r := ColorRect.new(); r.set_anchors_preset(Control.PRESET_FULL_RECT); r.mouse_filter = Control.MOUSE_FILTER_IGNORE; r.color = BENCH
	var m := ShaderMaterial.new(); m.shader = BENCH_SHADER
	r.material = m
	var fit := func(): m.set_shader_parameter("area", r.size)
	r.resized.connect(fit); fit.call(); return r
static func paper() -> ColorRect: return bench()

# ---------- rows (settings) ----------
## label on the left, control on the right
static func row(text: String, ctl: Control, hint := "") -> Control:
	var h := HBoxContainer.new(); h.add_theme_constant_override("separation", 16); h.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var left := VBoxContainer.new(); left.size_flags_horizontal = Control.SIZE_EXPAND_FILL; left.add_theme_constant_override("separation", 0); left.alignment = BoxContainer.ALIGNMENT_CENTER
	left.add_child(label(text, 20, TEXT))
	if hint != "": left.add_child(label(hint, 15, TEXT_SOFT, true))
	h.add_child(left); h.add_child(ctl); return h

## A faint rule between rows.
static func hsep() -> HSeparator:
	var h := HSeparator.new(); var l := StyleBoxLine.new(); l.color = LINE; l.thickness = 1; h.add_theme_stylebox_override("separator", l); return h

## On / off switch: ink-filled "On" when on, an outlined "Off" when off.
static func toggle(on_: bool, cb: Callable) -> Button:
	var b := Button.new(); b.toggle_mode = true; b.button_pressed = on_; b.focus_mode = focus_mode(); b.custom_minimum_size = Vector2(120, maxf(46.0, Layout.tap()))
	_btn_font(b, "ON", 22)
	var paint := func(v: bool):
		b.text = "On" if v else "Off"; skin(b, "amber" if v else "panel", 10)
		b.add_theme_stylebox_override("pressed", b.get_theme_stylebox("normal")); b.add_theme_stylebox_override("hover_pressed", b.get_theme_stylebox("hover"))
	paint.call(on_)
	b.toggled.connect(func(v): paint.call(v); cb.call(v)); return b

## The control for a slider row: slider + percentage readout (Plex Mono). cb receives 0..1.
static func slider(value: float, cb: Callable, width := 320.0) -> Control:
	var h := HBoxContainer.new(); h.add_theme_constant_override("separation", 14)
	var s := HSlider.new(); s.min_value = 0.0; s.max_value = 1.0; s.step = 0.05; s.value = value; s.focus_mode = focus_mode()
	s.custom_minimum_size = Vector2(width, maxf(34.0, Layout.tap())); s.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	var v := mono("%d%%" % roundi(value * 100.0), 20, TEXT); v.custom_minimum_size.x = 66; v.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	s.value_changed.connect(func(x): v.text = "%d%%" % roundi(x * 100.0); cb.call(x))
	h.add_child(s); h.add_child(v); return h

## Option stepper: one cut card "‹ | value | ›". Left/right (keys, pad D-pad), clicks on the arrows, or A/Enter (next) change it.
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
		add_theme_constant_override("separation", 0)
		left = Kit.btn("‹", func(): step(-1), "panel", fsize + 4, Vector2(aw, 0)); right = Kit.btn("›", func(): step(1), "panel", fsize + 4, Vector2(aw, 0))
		left.focus_mode = Control.FOCUS_NONE; right.focus_mode = Control.FOCUS_NONE
		_side(left, true); _side(right, false)
		var chipp := PanelContainer.new()
		var cs := StyleBoxFlat.new(); cs.bg_color = Kit.PANEL_HI; cs.border_color = Kit.TEXT; cs.border_width_top = Kit.OUTLINE; cs.border_width_bottom = Kit.OUTLINE
		cs.content_margin_left = 10; cs.content_margin_right = 10; Kit._hard_shadow(cs, Kit.LINE, 4.0)
		chipp.add_theme_stylebox_override("panel", cs); chipp.custom_minimum_size.x = width
		val = Kit.shout(Kit.label_case(str(opts[idx][1])), fsize, Kit.TEXT); val.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER; val.vertical_alignment = VERTICAL_ALIGNMENT_CENTER; val.size_flags_horizontal = Control.SIZE_EXPAND_FILL; chipp.add_child(val)
		chipp.mouse_filter = Control.MOUSE_FILTER_IGNORE
		add_child(left); add_child(chipp); add_child(right)
		focus_mode = Kit.focus_mode(); add_theme_stylebox_override("focus", Kit.ring_sb())
		focus_entered.connect(queue_redraw); focus_exited.connect(queue_redraw)
		return self
	## The arrow ends of the stepper: outlined on the outside, a fine rule where they meet the value.
	func _side(b: Button, is_left: bool) -> void:
		for n in ["normal", "hover", "pressed"]:
			var s := (b.get_theme_stylebox(n) as StyleBoxFlat).duplicate() as StyleBoxFlat
			if is_left: s.border_width_right = 1; s.corner_radius_top_right = 0; s.corner_radius_bottom_right = 0
			else: s.border_width_left = 1; s.corner_radius_top_left = 0; s.corner_radius_bottom_left = 0
			b.add_theme_stylebox_override(n, s)
	func step(d: int) -> void:
		if opts.is_empty(): return
		idx = posmod(idx + d, opts.size()); val.text = Kit.label_case(str(opts[idx][1])); cb.call(opts[idx][0])
	func _gui_input(e: InputEvent) -> void:
		if not has_focus(): return
		if e.is_action_pressed("ui_left"): step(-1); accept_event()
		elif e.is_action_pressed("ui_right") or e.is_action_pressed("ui_accept"): step(1); accept_event()
	func _draw() -> void:
		if has_focus(): draw_style_box(Kit.ring_sb(), Rect2(Vector2.ZERO, size))

static func stepper(options: Array, selected: String, cb: Callable, width := 240.0, fsize := 22, aw := 54.0) -> Stepper:
	return Stepper.new().setup(options, selected, cb, width, fsize, aw)

## Tab row; the current tab is ink-filled (the primary), the others outlined cards.
static func tabs(names: Array, current: String, cb: Callable) -> HBoxContainer:
	var h := HBoxContainer.new(); h.add_theme_constant_override("separation", 12); h.alignment = BoxContainer.ALIGNMENT_BEGIN
	for n in names:
		var on_: bool = n == current
		var b := btn(n, func(): cb.call(n), "amber" if on_ else "panel", 22 if Layout.phone else 24)
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

# ---------- stamp ----------
## A rubber-stamp score as a control: lands (from 1.35x down to 1x) unless reduced motion.
static func stamp(big: String, top := "", bottom := "", col: Color = AMBER, size := 140.0) -> Control:
	var s := Stamp.new(); s.big = big; s.top = top; s.bottom = bottom; s.col = col; s.custom_minimum_size = Vector2(size, size)
	return s

class Stamp extends Control:
	var big := ""
	var top := ""
	var bottom := ""
	var col := Kit.AMBER
	var landed := 1.0:
		set(v): landed = v; queue_redraw()
	func _init() -> void: mouse_filter = Control.MOUSE_FILTER_IGNORE
	func _ready() -> void:
		if Kit.motion():
			landed = 0.0
			create_tween().tween_property(self, "landed", 1.0, 0.28).set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
	func _draw() -> void:
		var r := minf(size.x, size.y) * 0.5 - 4.0
		var k := lerpf(1.35, 1.0, landed)
		draw_set_transform(size * 0.5, 0.0, Vector2(k, k))
		Kit.draw_stamp(self, Vector2.ZERO, r, big, top, bottom, Color(col, clampf(landed * 1.4, 0.0, 1.0)))
		draw_set_transform(Vector2.ZERO, 0.0, Vector2.ONE)

# ---------- canvas helpers (any CanvasItem, inside its _draw) ----------
## A plate drawn by hand: hard offset shadow, card fill, ink outline.
static func draw_plate(ci: CanvasItem, rect: Rect2, fill: Color = PANEL, shadow: Color = LINE, outline: Color = TEXT, off := SHADOW) -> void:
	if shadow.a > 0.0: ci.draw_rect(Rect2(rect.position + Vector2(off, off), rect.size), shadow)
	ci.draw_rect(rect, fill)
	ci.draw_rect(rect.grow(-OUTLINE * 0.5), outline, false, OUTLINE)

## Blue engineering graph paper: fine lines every `cell`, heavier every fifth, within rect.
static func draw_graph_paper(ci: CanvasItem, rect: Rect2, cell := 24.0, fill: Color = PANEL_HI) -> void:
	if fill.a > 0.0: ci.draw_rect(rect, fill)
	var minor := PackedVector2Array(); var major := PackedVector2Array()
	var i := 0; var x := rect.position.x
	while x <= rect.end.x + 0.01:
		if i % 5 == 0: major.append(Vector2(x, rect.position.y)); major.append(Vector2(x, rect.end.y))
		else: minor.append(Vector2(x, rect.position.y)); minor.append(Vector2(x, rect.end.y))
		x += cell; i += 1
	i = 0; var y := rect.position.y
	while y <= rect.end.y + 0.01:
		if i % 5 == 0: major.append(Vector2(rect.position.x, y)); major.append(Vector2(rect.end.x, y))
		else: minor.append(Vector2(rect.position.x, y)); minor.append(Vector2(rect.end.x, y))
		y += cell; i += 1
	if minor.size() > 0: ci.draw_multiline(minor, GRID, 1.0)
	if major.size() > 0: ci.draw_multiline(major, GRID_MAJOR, 1.4)

## "Fig. N." + italic caption (+ mono readout at the right edge of `width`). pos is the baseline-left. Returns the line height.
static func draw_fig(ci: CanvasItem, pos: Vector2, n, title: String, width: float, right := "", size := 19) -> float:
	var z := fs(size); var x := pos.x
	if str(n) != "":
		var s := "Fig. %s." % str(n)
		ci.draw_string(Fonts.display, Vector2(x, pos.y), s, HORIZONTAL_ALIGNMENT_LEFT, -1, z, TEXT)
		x += Fonts.display.get_string_size(s, HORIZONTAL_ALIGNMENT_LEFT, -1, z).x + z * 0.45
	var rw := 0.0
	if right != "":
		var rz := fs(maxi(15, size - 4)); rw = Fonts.mono.get_string_size(right, HORIZONTAL_ALIGNMENT_LEFT, -1, rz).x
		ci.draw_string(Fonts.mono, Vector2(pos.x + width - rw, pos.y), right, HORIZONTAL_ALIGNMENT_LEFT, -1, rz, TEXT_SOFT)
	ci.draw_string(Fonts.italic, Vector2(x, pos.y), title, HORIZONTAL_ALIGNMENT_LEFT, maxf(pos.x + width - rw - x - 12.0, 10.0), z, TEXT)
	return z * 1.4

## Text in a face, never below the size floor; small coloured text is made legible.
static func draw_ink_text(ci: CanvasItem, font: Font, pos: Vector2, text: String, size: int, col: Color = TEXT, width := -1.0, align := HORIZONTAL_ALIGNMENT_LEFT) -> void:
	var z := fs(size)
	ci.draw_string(font, pos, text, align, width, z, legible(col) if z < 24 else col)

## A watercolour wash of stain: a few overlapping, slightly offset translucent blots with a darker tide-mark rim. amount 0..1.
static func draw_wash(ci: CanvasItem, c: Vector2, radius: float, col: Color, amount := 1.0, seed := 0) -> void:
	if amount <= 0.01: return
	var rng := RandomNumberGenerator.new(); rng.seed = 7919 + seed
	for k in 5:
		var o := Vector2(rng.randf_range(-0.18, 0.18), rng.randf_range(-0.18, 0.18)) * radius
		var rr := radius * rng.randf_range(0.62, 1.0)
		var pts := PackedVector2Array()
		var ph := rng.randf() * TAU; var wob := rng.randf_range(0.05, 0.12)
		for i in 28:
			var a := TAU * float(i) / 28.0
			pts.append(c + o + Vector2.from_angle(a) * rr * (1.0 + wob * sin(a * 3.0 + ph) + wob * 0.5 * sin(a * 5.0 - ph)))
		ci.draw_colored_polygon(pts, Color(col, 0.07 * amount))
		if k == 0:
			pts.append(pts[0]); ci.draw_polyline(pts, Color(col, 0.16 * amount), 1.4, true)

## A rubber stamp: double ring, small-caps word on top, a big number, an italic line below; slightly rotated, with gaps in the ink.
static func draw_stamp(ci: CanvasItem, c: Vector2, r: float, big: String, top := "", bottom := "", col: Color = AMBER, angle := -0.14) -> void:
	var rng := RandomNumberGenerator.new(); rng.seed = hash(big + top)
	ci.draw_set_transform(c, angle, Vector2.ONE)
	ci.draw_arc(Vector2.ZERO, r, 0.0, TAU, 72, col, maxf(2.0, r * 0.035), true)
	ci.draw_arc(Vector2.ZERO, r * 0.86, 0.0, TAU, 72, col, maxf(1.0, r * 0.012), true)
	var bz := int(r * 0.78)
	var bw := Fonts.display_heavy.get_string_size(big, HORIZONTAL_ALIGNMENT_LEFT, -1, bz).x
	ci.draw_string(Fonts.display_heavy, Vector2(-bw * 0.5, bz * 0.34), big, HORIZONTAL_ALIGNMENT_LEFT, -1, bz, col)
	if top != "":
		var tz := maxi(fs(13), int(r * 0.17)); var tw := Fonts.display.get_string_size(top.to_upper(), HORIZONTAL_ALIGNMENT_LEFT, -1, tz).x
		ci.draw_string(Fonts.display, Vector2(-tw * 0.5, -r * 0.5), top.to_upper(), HORIZONTAL_ALIGNMENT_LEFT, -1, tz, col)
	if bottom != "":
		var oz := maxi(fs(13), int(r * 0.17)); var ow := Fonts.italic.get_string_size(bottom, HORIZONTAL_ALIGNMENT_LEFT, -1, oz).x
		ci.draw_string(Fonts.italic, Vector2(-ow * 0.5, r * 0.66), bottom, HORIZONTAL_ALIGNMENT_LEFT, -1, oz, col)
	for i in 14:   # pale specks where the stamp did not take
		var p := Vector2.from_angle(rng.randf() * TAU) * r * sqrt(rng.randf())
		ci.draw_circle(p, rng.randf_range(0.6, 1.8), Color(PANEL, 0.55))
	ci.draw_set_transform(Vector2.ZERO, 0.0, Vector2.ONE)

## A dotted leader line.
static func draw_leader(ci: CanvasItem, a: Vector2, b: Vector2, col: Color = LINE) -> void:
	var d := a.distance_to(b)
	if d < 4.0: return
	var u := (b - a) / d; var t := 0.0
	while t <= d:
		ci.draw_circle(a + u * t, 1.3, col); t += 6.0

## Diagonal hatching (fills of meters and bars: ink, not flat colour).
static func draw_hatch(ci: CanvasItem, rect: Rect2, col: Color, step := 7.0, width := 2.0) -> void:
	if rect.size.x <= 0.0 or rect.size.y <= 0.0: return
	var pts := PackedVector2Array(); var h := rect.size.y; var x := rect.position.x - h
	while x < rect.end.x:
		var a := Vector2(x, rect.end.y); var b := Vector2(x + h, rect.position.y)
		if a.x < rect.position.x: a = Vector2(rect.position.x, rect.end.y - (rect.position.x - a.x))
		if b.x > rect.end.x: b = Vector2(rect.end.x, rect.position.y + (b.x - rect.end.x))
		pts.append(a); pts.append(b); x += step
	if pts.size() > 0: ci.draw_multiline(pts, col, width, true)

# ---------- Cajal specimens ----------
## Procedural Golgi-stained cells (after Ramon y Cajal's plates): recursive tapering dendrites with a little curl, seeded so a cell is always the
## same drawing. Geometry is in px around `o` times `s`; strokes are [PackedVector2Array points, float width]. Spines are 2-point strokes.
class Cajal:
	static func _branch(rng: RandomNumberGenerator, p: Vector2, ang: float, length: float, width: float, depth: int, out: Array, spines: bool, curl := 0.25, minw := 0.6) -> void:
		var pts := PackedVector2Array([p]); var n := maxi(4, int(length / 9.0)); var a := ang; var w0 := width
		for i in n:
			a += rng.randfn(0.0, curl * 0.35)
			p += Vector2.from_angle(a) * (length / n)
			width = maxf(minw, width * 0.965)
			pts.append(p)
			if spines and width < 2.6 and rng.randf() < 0.55:
				var sa := a + (PI * 0.5) * (1.0 if rng.randf() < 0.5 else -1.0) + rng.randfn(0.0, 0.3)
				out.append([PackedVector2Array([p, p + Vector2.from_angle(sa) * rng.randf_range(2.5, 5.5)]), 0.55])
		out.append([pts, (w0 + width) * 0.5])
		if depth > 0:
			var k := 2 if rng.randf() < 0.8 else 3
			for j in k:
				var na := a + rng.randf_range(-0.75, 0.75) + (j - (k - 1) * 0.5) * 0.5
				_branch(rng, p, na, length * rng.randf_range(0.55, 0.8), width * 0.72, depth - 1, out, spines, curl, minw)

	## A pyramidal cell: apical dendrite with obliques, a skirt of basal dendrites, a descending axon with collaterals, a triangular soma.
	static func pyramidal(rng: RandomNumberGenerator, o: Vector2, s: float, spines := true) -> Dictionary:
		var st := []; var ax := []
		_branch(rng, o + Vector2(0, -14 * s), -PI * 0.5, 260 * s, 7 * s, 4, st, spines, 0.18)
		for k in 5:
			var yy := o.y - 30 * s - k * 42 * s
			_branch(rng, Vector2(o.x + rng.randfn(0, 2), yy), -PI * 0.5 + (1.0 if k % 2 else -1.0) * rng.randf_range(0.9, 1.3), 90 * s, 2.6 * s, 2, st, spines)
		for k in 6:
			var a := PI * 0.5 + (k - 2.5) * 0.5 + rng.randfn(0, 0.12)
			_branch(rng, o + Vector2(cos(a) * 10 * s, sin(a) * 8 * s), a, 110 * s, 4 * s, 3, st, spines, 0.3)
		var p := o + Vector2(0, 18 * s); var a2 := PI * 0.5 + 0.05; var pts := PackedVector2Array([p])
		for i in 60:
			a2 += rng.randfn(0, 0.05); p += Vector2.from_angle(a2) * 7 * s; pts.append(p)
		ax.append([pts, 1.6 * s])
		for i in [14, 26, 40, 52]:
			_branch(rng, pts[i], PI * 0.5 + (1.0 if i % 4 else -1.0) * 1.1, 70 * s, 1.2 * s, 1, ax, false, 0.2)
		var q := [o + Vector2(-17, 12) * s, o + Vector2(-4, -10) * s, o + Vector2(0, -26) * s, o + Vector2(4, -10) * s, o + Vector2(17, 12) * s, o + Vector2(0, 22) * s]
		return {"strokes": st, "axon": ax, "soma": _quad_loop(q)}

	## A basket cell (inhibitory interneuron): radiating smooth dendrites, a horizontal axon with pericellular baskets, a round soma.
	static func basket(rng: RandomNumberGenerator, o: Vector2, s: float) -> Dictionary:
		var st := []; var ax := []
		for k in 7:
			var a := k * TAU / 7.0 + rng.randfn(0, 0.2)
			_branch(rng, o, a, 95 * s, 3.2 * s, 2, st, false, 0.35)
		var p := o; var a2 := 0.2; var pts := PackedVector2Array([p])
		for i in 40:
			a2 += rng.randfn(0, 0.06); p += Vector2.from_angle(a2) * 7 * s; pts.append(p)
		ax.append([pts, 1.3 * s])
		for i in [12, 22, 32, 39]:
			for j in 4: _branch(rng, pts[i], rng.randf() * TAU, 26 * s, 1.0 * s, 1, ax, false, 0.9)
		return {"strokes": st, "axon": ax, "soma": _circle(o, 11 * s)}

	## A small stellate / granule cell: a star of short spiny dendrites, a round soma (for specimen sheets and atlas pages).
	static func stellate(rng: RandomNumberGenerator, o: Vector2, s: float) -> Dictionary:
		var st := []
		for k in 6:
			_branch(rng, o, k * TAU / 6.0 + rng.randfn(0, 0.25), 60 * s, 2.4 * s, 2, st, true, 0.4, 0.5)
		return {"strokes": st, "axon": [], "soma": _circle(o, 8 * s)}

	static func _circle(c: Vector2, r: float) -> PackedVector2Array:
		var out := PackedVector2Array()
		for i in 24: out.append(c + Vector2.from_angle(TAU * i / 24.0) * r)
		return out

	## A closed loop of quadratic curves through alternate control points (the soma outline of the mockup's SVG path).
	static func _quad_loop(q: Array) -> PackedVector2Array:
		var out := PackedVector2Array(); var n := q.size()
		for i in range(0, n, 2):
			var a: Vector2 = q[i]; var c: Vector2 = q[(i + 1) % n]; var b: Vector2 = q[(i + 2) % n]
			for k in 8:
				var t := k / 8.0
				out.append(a.lerp(c, t).lerp(c.lerp(b, t), t))
		return out

	## Ink a cell: dendrites, axon (a little lighter), soma. offset/k map the cell's px into the canvas (crisp: vector lines, AA).
	static func draw(ci: CanvasItem, cell: Dictionary, col: Color, alpha := 1.0, offset := Vector2.ZERO, k := 1.0) -> void:
		var c := Color(col, col.a * alpha)
		for st in cell["strokes"]: _stroke(ci, st, c, offset, k)
		var ca := Color(col, col.a * alpha * 0.88)
		for st in cell["axon"]: _stroke(ci, st, ca, offset, k)
		var poly := PackedVector2Array()
		for p in cell["soma"]: poly.append(offset + p * k)
		if poly.size() >= 3:
			ci.draw_colored_polygon(poly, c)
			poly.append(poly[0]); ci.draw_polyline(poly, c, 1.0, true)

	static func _stroke(ci: CanvasItem, st: Array, c: Color, offset: Vector2, k: float) -> void:
		var pts: PackedVector2Array = st[0]
		var w := maxf(float(st[1]) * k, 0.5)
		var tp := PackedVector2Array()
		for p in pts: tp.append(offset + p * k)
		if w < 1.0: ci.draw_polyline(tp, Color(c, c.a * w), 1.0, true)   # hairlines fade instead of thinning below a pixel
		else: ci.draw_polyline(tp, c, w, true)
