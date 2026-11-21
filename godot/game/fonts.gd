class_name Fonts
## The three faces of the "Cajal Plate" identity (docs/IDENTITY.md). All SIL OFL 1.1, vendored in assets/fonts with their licences:
##   regular / bold   Atkinson Hyperlegible (Braille Institute): body text and every small UI label (OFL-AtkinsonHyperlegible.txt)
##   display / display_med / display_heavy   Fraunces roman (Undercase Type, variable opsz/wght/SOFT/WONK): titles, buttons, the wordmark (OFL-Fraunces.txt)
##   italic / italic_bold   Fraunces italic: "Fig. N" captions, legend numerals, specimen letters
##   mono / mono_med   IBM Plex Mono (IBM): numeric readouts, times, units (OFL-IBMPlexMono.txt)
## Variable instances are FontVariations of one file; each falls back to Atkinson for glyphs the face lacks.
static var regular: Font = load("res://assets/fonts/AtkinsonHyperlegible-Regular.woff2")
static var bold: Font = load("res://assets/fonts/AtkinsonHyperlegible-Bold.woff2")
static var _roman: FontFile = load("res://assets/fonts/Fraunces.ttf")
static var _ital: FontFile = load("res://assets/fonts/Fraunces-Italic.ttf")
static var display: Font = _var(_roman, 600, 72, 50)        # headings and buttons
static var display_med: Font = _var(_roman, 500, 36, 50)    # menu entries, light headings
static var display_heavy: Font = _var(_roman, 720, 144, 60) # the wordmark and big numerals
static var italic: Font = _var(_ital, 420, 24, 100)         # captions
static var italic_bold: Font = _var(_ital, 600, 24, 100)
static var mono: Font = _fb(load("res://assets/fonts/IBMPlexMono-Regular.ttf"))
static var mono_med: Font = _fb(load("res://assets/fonts/IBMPlexMono-Medium.ttf"))

static func _tag(s: String) -> int: return TextServerManager.get_primary_interface().name_to_tag(s)

static func _var(base: FontFile, wght: int, opsz: int, soft: int) -> FontVariation:
	var v := FontVariation.new(); v.base_font = base
	v.variation_opentype = {_tag("wght"): wght, _tag("opsz"): opsz, _tag("SOFT"): soft, _tag("WONK"): 0}   # WONK 1 swaps italic h and b for leaning forms that read as "6" ("6pm" for "bpm")
	v.fallbacks = [regular]
	return v

static func _fb(f: FontFile) -> Font:
	f.fallbacks = [regular]; return f
