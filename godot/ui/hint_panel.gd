class_name HintPanel
extends PanelContainer
## The hint ladder card (docs/DESIGN-0.3.md section 5). Tier 1 names the concept, tier 2 points at the place (the lab outlines it in the editor), tier 3 gives a
## partial recipe. Free in Guided, unlocked after UNLOCK_FAILS failed runs in Standard, absent in Researcher. The card only draws; the lab owns the state
## (how many failed runs, which tier is revealed) and passes it to `show_state`.
const UNLOCK_FAILS := 2
const TIER_NAMES := {1: "THE IDEA", 2: "THE PLACE", 3: "A PARTIAL RECIPE"}

var max_h := 0.0   # > 0: the tiers sit in a ScrollContainer no taller than this (phone overlay)

signal next_pressed
signal close_pressed

## Is the ladder available now? Guided always; Standard after UNLOCK_FAILS failed runs; Researcher never.
static func available(mode: String, fails: int) -> bool:
	match mode:
		"guided": return true
		"researcher": return false
	return fails >= UNLOCK_FAILS

## One line for the button that opens the card: what the player can do about hints right now.
static func button_text(mode: String, fails: int, shown: int) -> String:
	if mode == "researcher": return ""
	if not available(mode, fails): return "Hint %d/%d" % [fails, UNLOCK_FAILS]
	return "Hint" if shown == 0 else "Hint %d" % shown

## `hints`: [{tier, text, target}] from Task::hints(); `shown`: highest tier revealed (0 = none).
func show_state(mode: String, hints: Array, shown: int, fails: int) -> HintPanel:
	for c in get_children(): remove_child(c); c.queue_free()
	add_theme_stylebox_override("panel", Kit.sb(Kit.PANEL, Kit.AMBER, 1, 12, 12 if Layout.phone else 16, true)); Kit.ident(self, "HintCard")
	var v := VBoxContainer.new(); v.add_theme_constant_override("separation", 6 if Layout.phone else 8); add_child(v)
	var hd := HBoxContainer.new(); hd.add_theme_constant_override("separation", 10); v.add_child(hd)
	var t := Kit.shout("Hints", 26 if not Layout.phone else 20, Kit.TEXT); t.size_flags_horizontal = Control.SIZE_EXPAND_FILL; hd.add_child(t)
	hd.add_child(Kit.chip(mode.to_upper(), Kit.AMBER if mode != "guided" else Kit.GREEN, 14))
	hd.add_child(Kit.ident(Kit.btn("X", func(): close_pressed.emit(), "panel", 16), "BtnHintClose"))
	var fs := 17 if not Layout.phone else 15
	if mode == "researcher":
		v.add_child(Kit.label("Researcher mode has no hints. Switch to Standard or Guided (the More menu, or the campaign header) to get them.", fs, Kit.TEXT_SOFT, true)); return self
	if not available(mode, fails):
		v.add_child(Kit.label("Hints open after %d failed runs in Standard mode (%d so far). Guided mode has them free from the start." % [UNLOCK_FAILS, fails], fs, Kit.TEXT_SOFT, true)); return self
	var by := {}
	var body: VBoxContainer = v
	if max_h > 0.0:
		var sc := ScrollContainer.new(); sc.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED; sc.size_flags_vertical = Control.SIZE_EXPAND_FILL
		sc.custom_minimum_size.y = minf(maxf(max_h - 150.0, 80.0), 400.0)
		v.add_child(sc); body = VBoxContainer.new(); body.add_theme_constant_override("separation", 6); body.size_flags_horizontal = Control.SIZE_EXPAND_FILL; sc.add_child(body)
	for h in hints:
		var tr := int(h.get("tier", 0))
		if not by.has(tr): by[tr] = h
	if by.is_empty():
		v.add_child(Kit.label("Run the circuit once so the hint can look at what went wrong, then ask again.", fs, Kit.TEXT_SOFT, true))
		v.add_child(Kit.btn("ASK AGAIN", func(): next_pressed.emit(), "amber", 18)); return self
	for tr in [1, 2, 3]:
		if tr > shown or not by.has(tr): continue
		var row := HBoxContainer.new(); row.add_theme_constant_override("separation", 10); body.add_child(row)
		var ch := Kit.chip("%d  %s" % [tr, TIER_NAMES[tr]], Kit.AMBER if tr < 3 else Kit.CYAN, 14); ch.size_flags_vertical = Control.SIZE_SHRINK_BEGIN; row.add_child(ch)
		var h: Dictionary = by[tr]; var txt := str(h.get("text", ""))
		if tr == 2 and str(h.get("target", "")) != "": txt += "  (outlined in the editor)" if txt != "" else "Look at the outlined part of the circuit."
		var l := Kit.label(txt, fs, Kit.TEXT, true); l.size_flags_horizontal = Control.SIZE_EXPAND_FILL; row.add_child(l)
	var nxt := shown + 1
	while nxt <= 3 and not by.has(nxt): nxt += 1
	if nxt <= 3: v.add_child(Kit.ident(Kit.btn("Show hint %d: %s" % [nxt, str(TIER_NAMES[nxt]).to_lower()], func(): next_pressed.emit(), "amber", 18), "BtnHintNext"))
	else: v.add_child(Kit.label("That is every hint. Hint use is recorded, never punished.", fs - 1, Kit.TEXT_SOFT, true))
	return self
