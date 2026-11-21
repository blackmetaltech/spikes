class_name UiGraphTest
extends RefCounted
## Walks godot/tests/interaction_graph.json (docs/INTERACTION-GRAPH.md): the game's UI as nodes (screens, tabs, panels, modals, toasts) and edges (a user action
## on a named control that moves between them). Run by scripts/ui_graph_test.sh (ctest `ui_graph`):
##   SP_UIGRAPH=1 godot --headless --path godot            prints "UIGRAPH OK ..." or "UIGRAPH FAIL ..." and quits (exit 1 on failure)
## From the title it visits every edge: sets the guard's state (mode, passed, unlock all, missed runs) instead of playing levels, goes to the edge's source,
## presses the control with a real mouse click (so a covered or dead control fails), and asserts that the target node is what the UI now shows, that at most one
## modal is open, and, for an edge marked "returns", that the target's Back / Close control and Escape both return to the source. At every node it checks
## completeness: every visible interactive control (button, stepper, slider, text box, the circuit editor) must be listed by an active node of the graph or
## match the graph's explicit ignore list. Finally every node must have been reached. An edge or node marked "known_bug" in the JSON is expected to fail:
## it is listed as XFAIL and the run still passes; if it starts working the run fails until the marker is removed.

const GRAPH := "res://tests/interaction_graph.json"
const MODALS := ["MoreMenu", "PartsModal", "EndCard", "TestCard", "HistoCard"]   # roots in the lab's overlay layer (FileDlg is a Window)

var app: Node
var vp: Viewport
var g := {}
var nodes := {}
var edges: Array = []
var fails: Array = []
var xfails: Array = []
var seen_nodes := {}
var seen_edges := {}
var checked_complete := {}
var back_tick := {}
var ctx := ""                      # what is being tested, for messages
var cur_known := ""                # the known_bug text of the edge being walked ("" = none)
var cur_failed := false
var pos_node := ""                 # where the UI is now (clean position), "" = unknown
var pos_sig := ""
var synthetic := 0                 # presses that had to be emitted because the click did not arrive
var clicks := 0
var world_resets := 0

static func run(app_: Node) -> void:
	var t := UiGraphTest.new()
	await t._run(app_)

func _fail(msg: String) -> void:
	cur_failed = true
	var m := "%s: %s" % [ctx, msg]
	if cur_known != "": xfails.append("%s  [known_bug: %s]" % [m, cur_known])
	else: fails.append(m)

func _frames(n := 2) -> void:
	for i in n: await app.get_tree().process_frame

func _wait(cond: Callable, secs: float) -> bool:
	var t0 := Time.get_ticks_msec()
	while Time.get_ticks_msec() - t0 < secs * 1000.0:
		if cond.call(): return true
		await _frames(1)
	return cond.call()

# ---------------------------------------------------------------------------------------------------- the run
func _run(app_: Node) -> void:
	app = app_; vp = app.get_viewport()
	var t0 := Time.get_ticks_msec()
	var txt := FileAccess.get_file_as_string(GRAPH)
	var parsed = JSON.parse_string(txt)
	if not (parsed is Dictionary): print("UIGRAPH FAIL: %s is not valid JSON" % GRAPH); app.get_tree().quit(1); return
	g = parsed; nodes = g["nodes"]; edges = g["edges"]
	OS.set_environment("SP_NOAUTOSAVE", "1"); Settings.no_save = true; Kit.instant = true; Engine.max_fps = 0
	app.get_window().size = Vector2i(1920, 1080); await _frames(3)   # a desktop-sized window: the lab keeps its wide layout
	Settings.layout = str(g.get("layout", "desktop")); app.relayout(); await _frames(2)
	print("UIGRAPH window ", app.get_window().size, " viewport ", vp.get_visible_rect().size)
	ctx = "graph"; _static_checks()
	var only := OS.get_environment("SP_UIGRAPH_ONLY")   # dev: walk only the edges whose id contains this
	for e in edges:
		if only != "" and not str(e["id"]).contains(only): seen_edges[e["id"]] = true; continue
		await _walk_edge(e)
	for n in g.get("negatives", []): await _negative(n)
	_coverage()
	var secs := (Time.get_ticks_msec() - t0) / 1000.0
	print("UIGRAPH walked %d edges, %d nodes seen of %d, %d negatives, %d world resets, %d clicks (%d emitted instead), %.0f s" % [seen_edges.size(), seen_nodes.size(), nodes.size(), g.get("negatives", []).size(), world_resets, clicks, synthetic, secs])
	for x in xfails: print("UIGRAPH XFAIL ", x)
	for f in fails: printerr("UIGRAPH FAIL: ", f)
	_reset_env()
	if fails.is_empty(): print("UIGRAPH OK edges=%d nodes=%d xfail=%d" % [seen_edges.size(), seen_nodes.size(), xfails.size()]); app.get_tree().quit(0)
	else: print("UIGRAPH FAILED: %d problem(s)" % fails.size()); app.get_tree().quit(1)

func _reset_env() -> void:
	OS.unset_environment("SP_UNLOCK_ALL")

# ---------------------------------------------------------------------------------------------------- static checks
## The nodes that are active together with `nid`: the ones it names in "co" and, with `with_parent`, the screen it is a state of.
func _implied(nid: String, with_parent := true) -> Array:
	var out := [nid]; var i := 0
	while i < out.size():
		var n: Dictionary = nodes.get(out[i], {})
		var more: Array = (n.get("co", []) as Array).duplicate()
		if n.has("state_of") and with_parent: more.append(n["state_of"])
		for m in more:
			if not out.has(m): out.append(m)
		i += 1
	return out

func _static_checks() -> void:
	for eid in edges.size():
		var e: Dictionary = edges[eid]; ctx = "edge " + str(e.get("id", eid))
		for k in ["from", "to"]:
			if not nodes.has(str(e.get(k, ""))): _fail("%s node '%s' is not in the graph" % [k, e.get(k, "")])
	ids_unique()
	# reachability from the entry over the edges (a node is reached when an edge leads to it or to a node that implies it)
	var reach := {}; var queue: Array = [str(g["entry"])]
	for q in _implied(queue[0]): reach[q] = true
	var i := 0
	while i < queue.size():
		var here: Array = _implied(queue[i]); i += 1
		for e in edges:
			if here.has(str(e["from"])) and not reach.has(str(e["to"])):
				for q in _implied(str(e["to"])): reach[q] = true
				queue.append(str(e["to"]))
	ctx = "graph"
	for nid in nodes:
		if not reach.has(nid) and not bool(nodes[nid].get("abstract", false)): _fail("node %s is not reachable from the entry by any edge" % nid)
	# the code's screens, chapters and tabs are all in the graph
	var screens := {}
	for nid in nodes:
		var d: Dictionary = nodes[nid]["detect"]
		if d.has("screen"): screens[d["screen"]] = true
	for s in app.SCREENS: if not screens.has(s): _fail("screen '%s' (main.gd SCREENS) has no node" % s)
	for c in Campaign.CHAPTERS:
		var found := false
		for nid in nodes: if str(nodes[nid]["detect"].get("chapter", "")) == str(c["id"]): found = true
		if not found: _fail("campaign chapter '%s' has no tab node" % c["id"])
	for pair in [["settings", SettingsScreen.TABS], ["notebook", NotebookScreen.TABS]]:
		for t in pair[1]:
			var f2 := false
			for nid in nodes: if str(nodes[nid]["detect"].get("screen", "")) == pair[0] and str(nodes[nid]["detect"].get("tab", "")) == str(t): f2 = true
			if not f2: _fail("%s tab '%s' has no node" % [pair[0], t])
	for t in AnalysisScreen.TABS:
		var f3 := false
		for nid in nodes: if str(nodes[nid]["detect"].get("screen", "")) == "analysis" and nodes[nid]["detect"].has("tab") and int(nodes[nid]["detect"]["tab"]) == int(t[0]): f3 = true
		if not f3: _fail("analysis tab '%s' has no node" % t[1])

func ids_unique() -> void:
	var seen := {}
	for e in edges:
		if seen.has(e["id"]): _fail("edge id '%s' is used twice" % e["id"])
		seen[e["id"]] = true

func _coverage() -> void:
	ctx = "coverage"; cur_known = ""
	for nid in nodes:
		if bool(nodes[nid].get("abstract", false)): continue
		if not seen_nodes.has(nid):
			if nodes[nid].has("known_bug"): xfails.append("node %s was never reached  [known_bug: %s]" % [nid, nodes[nid]["known_bug"]])
			else: _fail("node %s was never reached by the walk" % nid)
	for e in edges:
		if not seen_edges.has(e["id"]) and not e.has("known_bug"): _fail("edge %s was never walked" % e["id"])

# ---------------------------------------------------------------------------------------------------- state of the world
var _level_cache := {}

func _level(tok: String) -> String:
	if tok == "" or not tok.begins_with("$"): return tok
	if _level_cache.has(tok): return _level_cache[tok]
	var out := ""
	match tok:
		"$first": out = str(Campaign.order()[0])
		"$locked":
			for id in Campaign.order():
				if not Campaign.unlocked(str(id)): out = str(id); break
		"$dx":
			for c in Campaign.CHAPTERS:
				for n in Campaign.nodes(str(c["id"])):
					if out == "" and bool(n["installed"]) and str(n["kind"]) == "diagnosis": out = str(n["id"])
	_level_cache[tok] = out
	return out

func _sig(setup: Dictionary) -> String: return JSON.stringify(setup, "", true)

## Start over: no progress, Standard mode, nothing open, the title shown, then the guard's state.
func _reset_world(setup: Dictionary) -> void:
	world_resets += 1
	app.cur = ""; app.origin = {}; app.lab_keep = {}; app.lab_reference = false; app.campaign_chapter = ""; app.campaign_node = ""; app.real_back = "title"
	Progress._best = {}; Progress._loaded = true; Progress._mode = "standard"
	LabScreen._fails = {}; LabScreen._hint_tier = {}; LabScreen._ref_loaded = {}; LabScreen._fail_fp = {}
	Campaign.pending_variant = ""
	Settings.reduced_motion = false; Settings.telemetry = false
	if bool(setup.get("unlock_all", false)): OS.set_environment("SP_UNLOCK_ALL", "1")
	else: OS.unset_environment("SP_UNLOCK_ALL")
	LabUnlocks.refresh(); Campaign.refresh()
	if setup.has("mode"): Progress._mode = str(setup["mode"])
	var tid := _level(str(setup.get("level", "$first")))
	if bool(setup.get("passed", false)) and tid != "":
		Progress._best[tid] = {"value": 100.0, "passed": true, "stars": 1, "modes": {Progress._mode: {"value": 100.0, "passed": true, "stars": 1}}}
	if setup.has("misses"): LabScreen._fails[tid] = int(setup["misses"])
	Campaign.refresh()
	app.show_screen("title", false, true)
	await _frames(2)

# ---------------------------------------------------------------------------------------------------- finding controls
func _all_controls(root: Node, out: Array) -> void:
	for c in root.get_children():
		if c.is_queued_for_deletion(): continue
		if c is Control: out.append(c)
		_all_controls(c, out)

func _raw_id(c: Node) -> String:
	if c.has_meta("sp"): return str(c.get_meta("sp"))
	return ""

## The graph id of a control: its own, or that of the row / stepper it is part of (the arrows of a stepper, the slider of a settings row).
func _cid(c: Control) -> String:
	var id := _raw_id(c)
	if id in ["StepLeft", "StepRight", "Slider"] and c.get_parent() is Control: return _cid(c.get_parent())
	return id

func _isa(n: Node, cls: String) -> bool:
	match cls:
		"Stepper": return n is Kit.Stepper
		"ModDials": return n is ModDials
		"PartsBin": return n is PartsBin
	return n.is_class(cls)

func _interactive(c: Control) -> bool:
	if c.mouse_filter == Control.MOUSE_FILTER_IGNORE: return false
	return c is BaseButton or c is Kit.Stepper or c is HSlider or c is LineEdit or c is CircuitEditor

func _match_id(pat: String, id: String) -> bool:
	if pat.ends_with("*"): return id.begins_with(pat.trim_suffix("*"))
	return pat == id

func _root_for(nid: String) -> Node:
	var n: Dictionary = nodes.get(nid, {})
	var sc := str(n.get("scope", ""))
	if sc == "": return app.screen
	if sc == "FileDlg": return _lab()._file_dlg if _lab() != null and _lab()._file_dlg.visible else null
	var all: Array = []; _all_controls(app.screen, all)
	for c in all:
		if _raw_id(c) == sc and c.is_visible_in_tree(): return c
	return null

func _find(id: String, root: Node = null, need_visible := true) -> Control:
	if root == null: root = app.screen
	var all: Array = []
	if root is Control: all.append(root)
	_all_controls(root, all)
	for c in all:
		var cid := _raw_id(c)
		if cid == "" or not _match_id(id, cid): continue
		if need_visible and not c.is_visible_in_tree(): continue
		return c
	return null

func _find_all(id: String, root: Node = null) -> Array:
	if root == null: root = app.screen
	var all: Array = []; _all_controls(root, all)
	return all.filter(func(c): return _raw_id(c) != "" and _match_id(id, _raw_id(c)) and c.is_visible_in_tree())

func _lab() -> LabScreen: return app.screen as LabScreen

## The control to act on for an id, looked up in the scope of the node it belongs to: the open modal's own root, else the screen.
func _control(id: String, nid: String) -> Control:
	var root: Node = _root_for(nid)
	var c: Control = _find(id, root) if root != null else null
	if c == null: c = _find(id, _top_root())
	return c

func _open_modals() -> Array:
	var out := []
	var lab := _lab()
	if lab != null and lab.ov != null:
		for c in lab.ov.get_children():
			if not c.is_queued_for_deletion() and c is Control and _raw_id(c) in MODALS: out.append(c)
	if lab != null and lab._file_dlg != null and lab._file_dlg.visible: out.append(lab._file_dlg)
	return out

func _top_root() -> Node:
	var m := _open_modals()
	return m.back() if not m.is_empty() else app.screen

# ---------------------------------------------------------------------------------------------------- detect
func _detect(nid: String) -> bool:
	var d: Dictionary = nodes[nid]["detect"]
	var scr = app.screen
	if d.has("screen") and app.cur != str(d["screen"]): return false
	if d.has("chapter") and str(scr.chapter) != str(d["chapter"]): return false
	if d.has("tab"):
		if typeof(d["tab"]) == TYPE_STRING:
			if str(scr.tab) != str(d["tab"]): return false
		elif int(scr.tab) != int(d["tab"]): return false
	var lab := _lab()
	if d.has("task"):
		if lab == null: return false
		match str(d["task"]):
			"none": if lab.task_id != "": return false
			"level": if lab.task_id == "" or lab._is_dx(): return false
			"dx": if not lab._is_dx(): return false
	if d.has("view"):
		if lab == null or lab.insp_body == null or str(lab.insp_body.get_meta("sp_view", "")) != str(d["view"]): return false
	if d.has("dx"):
		if lab == null: return false
		var st: Dictionary = lab._dx
		match str(d["dx"]):
			"wrong": if not (int(st.get("tries", 0)) > 0 and not bool(st.get("solved", false)) and not bool(st.get("gave_up", false))): return false
			"solved": if not bool(st.get("solved", false)): return false
			"revealed": if not bool(st.get("gave_up", false)): return false
	if d.has("find"):
		if lab == null: return false
		if str(d["find"]) == "FileDlg": return lab._file_dlg != null and lab._file_dlg.visible
		var c := _find(str(d["find"]), lab, false)
		if c == null or not c.is_visible_in_tree(): return false
	if d.has("toast"):
		if lab == null or lab.celebration == null or (lab.celebration._toasts.size() > 0) != bool(d["toast"]): return false
	if d.has("celebrating"):
		if lab == null or lab.celebration == null or lab.celebration.active() != bool(d["celebrating"]): return false
	return true

func _active_nodes() -> Array:
	var out := []
	for nid in nodes: if _detect(nid): out.append(nid)
	return out

# ---------------------------------------------------------------------------------------------------- completeness
func _ignored(c: Control) -> bool:
	for r in g.get("ignore", []):
		if r.has("class") and not _isa(c, str(r["class"])): continue
		if r.has("under"):
			var hit := false; var p := c.get_parent()
			while p != null and not hit: hit = p is Control and _raw_id(p) == str(r["under"]); p = p.get_parent()
			if not hit: continue
		if r.has("under_class"):
			var hit2 := false; var p2 := c.get_parent()
			while p2 != null and not hit2: hit2 = _isa(p2, str(r["under_class"])); p2 = p2.get_parent()
			if not hit2: continue
		return true
	return false

func _check_complete(nid: String) -> void:
	var active := _active_nodes()
	var allowed: Array = []
	var screens_listed := false
	for a0 in active:
		for a in _implied(a0):
			var n: Dictionary = nodes[a]
			allowed.append_array(n.get("controls", [])); allowed.append_array(n.get("optional", []))
			if str(n["detect"].get("screen", "")) == app.cur: screens_listed = true
	if not screens_listed: _fail("the screen '%s' is not a node of the graph" % app.cur)
	var all: Array = []; _all_controls(_top_root(), all)
	for c in all:
		if not _interactive(c) or not c.is_visible_in_tree() or _ignored(c): continue
		var id := _cid(c)
		if id == "": _fail("%s '%s' (%s) has no id (name it with Kit.ident, or list it in the graph's ignore rules); active nodes: %s" % [c.get_class(), c.name, app.screen.get_path_to(c), active]); continue
		var ok := false
		for pat in allowed: if _match_id(str(pat), id): ok = true; break
		if not ok: _fail("control '%s' (%s) is on screen but no active node lists it; active nodes: %s" % [id, c.get_class(), active])
	if _open_modals().size() > 1: _fail("more than one modal is open: %s" % str(_open_modals().map(func(m): return m.name)))

func _check_present(nid: String) -> void:
	for a in _implied(nid, false):
		var n: Dictionary = nodes[a]
		if bool(n.get("abstract", false)) and a != nid and not _detect(a): continue
		for id in n.get("controls", []):
			var root: Node = _root_for(a) if not bool(n.get("abstract", false)) else app.screen
			if root == null: _fail("node %s: its scope '%s' is not on screen" % [a, n.get("scope", "")]); continue
			if _find(str(id), root) == null: _fail("node %s: control '%s' is not on screen" % [a, id])

func _arrive(nid: String) -> void:
	seen_nodes[nid] = true
	for a in _implied(nid): if not bool(nodes[a].get("abstract", false)): seen_nodes[a] = true
	if not checked_complete.has(nid):
		checked_complete[nid] = true
		_check_present(nid); _check_complete(nid)

# ---------------------------------------------------------------------------------------------------- input
func _ensure_visible(c: Control) -> void:
	var p := c.get_parent()
	while p != null:
		if p is ScrollContainer: (p as ScrollContainer).ensure_control_visible(c)
		p = p.get_parent()

func _click_pos(c: Control) -> Vector2:
	var local := c.size * 0.5
	if "disc" in c: local = c.disc
	return c.get_global_transform_with_canvas() * local

## A real mouse click at the control's centre. If it does not arrive: when something else covers the control that is a failure; otherwise the press is emitted.
func _press_control(c: Control, what: String) -> void:
	if c == null: _fail("%s: no such control" % what); return
	if c is BaseButton and (c as BaseButton).disabled: _fail("%s: the control is disabled" % what); return
	_ensure_visible(c); await _frames(1)
	var pos := _click_pos(c)
	var fired := [false]
	var cb := func(): fired[0] = true
	if c is BaseButton: (c as BaseButton).pressed.connect(cb)
	var mv := InputEventMouseMotion.new(); mv.position = pos; mv.global_position = pos; vp.push_input(mv, true)
	await _frames(1)
	var hov := vp.gui_get_hovered_control()
	var down := InputEventMouseButton.new(); down.position = pos; down.global_position = pos; down.button_index = MOUSE_BUTTON_LEFT; down.pressed = true; down.button_mask = MOUSE_BUTTON_MASK_LEFT
	vp.push_input(down, true)
	var up := InputEventMouseButton.new(); up.position = pos; up.global_position = pos; up.button_index = MOUSE_BUTTON_LEFT; up.pressed = false; up.button_mask = 0
	vp.push_input(up, true)
	await _frames(1)
	clicks += 1
	if not is_instance_valid(c): fired[0] = true   # the press replaced the screen: it worked
	elif c is BaseButton and (c as BaseButton).pressed.is_connected(cb): (c as BaseButton).pressed.disconnect(cb)
	if not fired[0] and c is BaseButton:
		if hov != null and hov != c and not c.is_ancestor_of(hov) and not hov.is_ancestor_of(c): _fail("%s: the click lands on %s '%s' instead: the control is covered" % [what, hov.get_class(), hov.name])
		else: synthetic += 1; (c as BaseButton).pressed.emit()
	await _frames(2)

func _key_escape() -> void:
	for down in [true, false]:
		var k := InputEventKey.new(); k.keycode = KEY_ESCAPE; k.physical_keycode = KEY_ESCAPE; k.pressed = down
		vp.push_input(k)
	await _frames(2)

func _press_id(id: String, nid: String, what: String) -> void:
	var c := _control(id, nid)
	if c == null: _fail("%s: control '%s' is not on screen (scope of %s)" % [what, id, nid]); return
	await _press_control(c, what + " (" + id + ")")

## A stepper is driven through its right arrow.
func _step(id: String, nid: String) -> void:
	var c := _control(id, nid)
	if c == null or not (c is Kit.Stepper): _fail("control '%s' is not a stepper" % id); return
	await _press_control((c as Kit.Stepper).right, "stepper " + id)

# ---------------------------------------------------------------------------------------------------- navigation
func _allowed_path_edge(e: Dictionary, need_ref: bool) -> bool:
	if bool(e.get("reference_entry", false)) and not need_ref: return false
	if bool(e.get("plain_entry", false)) and need_ref: return false
	if bool(e.get("sandbox_only", false)): return false
	if bool(e.get("dx_only", false)) and _walk_level != "$dx": return false
	if _walk_avoid.has(str(e["id"])): return false
	if bool(e.get("needs_reference", false)) and not need_ref: return false
	if bool(e.get("plain_only", false)) and need_ref: return false
	if str(e.get("action", "")) == "terminal": return false
	if e.has("known_bug"): return false
	return true

## The shortest list of edges from the entry to a state in which `nid` is active.
func _path_to(nid: String, need_ref: bool) -> Array:
	var start := str(g["entry"])
	if _implied(start).has(nid): return []
	var prev := {start: null}; var queue: Array = [start]; var i := 0
	while i < queue.size():
		var here := str(queue[i]); i += 1
		var imp := _implied(here)
		for e in edges:
			if not imp.has(str(e["from"])) or not _allowed_path_edge(e, need_ref): continue
			var to := str(e["to"])
			if prev.has(to): continue
			prev[to] = [here, e]; queue.append(to)
			if _implied(to).has(nid):
				var path := []; var cur := to
				while prev[cur] != null: path.push_front(prev[cur][1]); cur = prev[cur][0]
				return path
	return []

func _merge(a: Dictionary, b: Dictionary) -> Dictionary:
	var out := a.duplicate(); for k in b: out[k] = b[k]
	return out

## Go to a state where `nid` is active, with `setup` in force.
var _walk_avoid: Array = []
var _walk_level := "$first"

func _goto(nid: String, setup: Dictionary, need_ref: bool) -> bool:
	var sig := _sig(setup) + str(need_ref)
	if pos_node != "" and pos_sig == sig and _implied(pos_node).has(nid) and _detect(nid): return true
	var path := _path_to(nid, need_ref)
	if path.is_empty() and not _implied(str(g["entry"])).has(nid): _fail("no path of edges leads to node %s" % nid); return false
	var world := {}
	for pe in path: world = _merge(world, pe.get("setup", {}))
	world = _merge(world, setup)
	await _reset_world(world)
	pos_node = ""; pos_sig = ""
	for pe in path:
		var save_ctx := ctx; ctx += " (on the way: " + str(pe["id"]) + ")"
		await _trigger(pe, world)
		ctx = save_ctx
	if not _detect(nid): _fail("after %d edges the UI is not at %s" % [path.size(), nid]); return false
	pos_node = nid; pos_sig = sig
	return true

func _select_level(level: String) -> void:
	var tid := _level(level)
	if tid == "": _fail("no level for %s" % level); return
	var scr = app.screen
	if app.cur != "tasks": _fail("cannot pick level %s: the campaign is not open" % tid); return
	var ch := Campaign.chapter_of(tid)
	if str(scr.chapter) != ch: await _press_id("Chapter_" + ch, "campaign", "chapter tab")
	await _press_id("Pin_" + tid, "campaign", "pin")
	if str(app.campaign_node) != tid: _fail("pressing the pin of %s selected '%s'" % [tid, app.campaign_node])

# ---------------------------------------------------------------------------------------------------- triggers
func _trigger(e: Dictionary, world: Dictionary) -> void:
	var act := str(e.get("action", "press")); var ctl := str(e.get("control", "")); var from := str(e["from"])
	if act.begins_with("event:"): await _event(act.substr(6), e); return
	await _trigger_control(e, world, act, ctl, from)
	if e.has("wait"): await _wait(func(): return _detect(str(e["to"])), float(e["wait"]))

func _trigger_control(e: Dictionary, world: Dictionary, act: String, ctl: String, from: String) -> void:
	if (ctl == "BtnBuild" or ctl == "BtnReference") and app.cur == "tasks": await _select_level(str(world.get("level", "$first")))
	match act:
		"press", "toggle": await _press_id(ctl, from, "edge " + str(e["id"]))
		"step": await _step(ctl, from)
		"select":
			var pool := _find_all(ctl, _root_for(from))
			var pick: Control = null
			for c in pool:
				if c is Button and ctl.begins_with("Pin_") and str(app.campaign_node) == _raw_id(c).trim_prefix("Pin_"): continue
				if c is Button and (c as Button).disabled: continue
				if ctl.begins_with("Pin_") and not bool((c as TasksScreen.NodeCard).node.get("installed", false)): continue
				pick = c; break
			if pick == null: _fail("no control matches '%s'" % ctl)
			else: await _press_control(pick, "edge " + str(e["id"]) + " (" + _raw_id(pick) + ")")
		"terminal":
			var tc := _control(ctl, from)
			if tc == null: _fail("the %s entry is missing" % ctl)
			elif tc is BaseButton and (tc as BaseButton).disabled: _fail("the %s entry is disabled" % ctl)
		_:
			if act.begins_with("press_after:"): await _pre(act.substr(12), from); await _press_id(ctl, from, "edge " + str(e["id"]))
			elif act.begins_with("select:") or act == "link" or act == "lesion_pick" or act.begins_with("dx:"): await _editor_action(act)
			else: _fail("unknown action '%s'" % act)

func _pre(what: String, from: String) -> void:
	if what == "lesion_pick": await _editor_action("lesion_pick")
	else: await _press_id(what, from, "preparing")

func _first_pop() -> String:
	var lab := _lab()
	var ps: Array = lab.circuit.get("populations", [])
	return str(ps[0].get("id", "")) if not ps.is_empty() else ""

func _editor_action(act: String) -> void:
	var lab := _lab()
	if lab == null: _fail("not in the lab for '%s'" % act); return
	var ed := lab.editor
	match act:
		"select:none": ed.select({})
		"select:pop":
			if _first_pop() == "": lab._add_neuron("E"); ed.select({})
			ed.select({"t": "pop", "id": _first_pop()})
		"select:in":
			var ins: Array = lab.circuit.get("inputs", [])
			if ins.is_empty(): _fail("the circuit has no input channel to select")
			else: ed.select({"t": "in", "name": str(ins[0]["name"])})
		"select:out":
			var outs: Array = lab.circuit.get("outputs", [])
			if outs.is_empty(): _fail("the circuit has no output channel to select")
			else: ed.select({"t": "out", "name": str(outs[0]["name"])})
		"link":
			while (lab.circuit.get("populations", []) as Array).size() < 2: lab._add_neuron("E")
			var ps: Array = lab.circuit["populations"]
			ed._make_link({"t": "pop", "id": str(ps[0]["id"])}, {"t": "pop", "id": str(ps[1]["id"])})
		"lesion_pick": ed.lesion_clicked.emit({"t": "pop", "id": _first_pop()})
		"dx:right": await _dx_submit(true)
		"dx:wrong": await _dx_submit(false)
		"dx:reveal":
			for _i in DiagnosisPanel.REVEAL_AFTER: await _dx_submit(false)   # giving up is offered only after several misses
			await _press_id("BtnRevealAnswer", "lab.insp.dx", "reveal")
		_: _fail("unknown editor action '%s'" % act)
	await _frames(3)

func _dx_submit(right: bool) -> void:
	var lab := _lab()
	var ans := str(lab._dx_info.get("answer", ""))
	var cands: Array = lab._dx_info.get("candidates", [])
	var pick := ""
	for c in cands:
		if (str(c) == ans) == right: pick = str(c); break
	if pick == "": _fail("the case has no %s candidate" % ("right" if right else "wrong")); return
	await _press_id("Cand_" + pick, "lab.insp.dx", "candidate")
	await _press_id("BtnSubmitDiagnosis", "lab.insp.dx", "submit")

func _event(name: String, e: Dictionary) -> void:
	var lab := _lab()
	match name:
		"episode_end", "episode_end_failing":
			if name == "episode_end_failing":   # a circuit that cannot pass: the reference with every axon cut and every sensor unwired (the walk's shortcut for a hand-built failing circuit)
				lab.circuit["projections"] = []
				for ch in lab.circuit.get("inputs", []): ch["targets"] = []
				lab._on_structure(); await _wait(func(): return lab._dirty < 0.0, 10.0); await _frames(3)
			if lab == null or lab.tele == null or lab.tele.task == null or lab.tele.sim == null: _fail("no running episode to finish (the lab has no simulation)"); return
			lab.tele.running = true; lab.tele.skip_until(float(lab.tele.task.duration_ms()))
			if not await _wait(func(): return _detect("lab.endcard"), 120.0): _fail("the episode never ended: no end card after 120 s")
		"test_done":
			if not await _wait(func(): return _detect("lab.endcard"), 400.0): _fail("the 5-seed test never finished (400 s)")
		"check_done":
			if not await _wait(func(): return _detect("lab.toast"), 120.0): _fail("Check never produced its verdict toast (120 s)")
		"first_pass": lab._celebrate(); await _frames(3)
		"celebration_ends":
			if not await _wait(func(): return not lab.celebration.active(), 12.0): _fail("the celebration never ended")
			lab.celebration.clear_toasts(); await _frames(2)
		"toast_expires": lab.celebration.clear_toasts(); await _frames(3)
		_: _fail("unknown event '%s'" % name)

# ---------------------------------------------------------------------------------------------------- expectations
func _control_state_text(id: String) -> String:
	var c := _control(id, "lab")
	if c == null: c = _find(id, _top_root())
	return (c as Button).text if c is Button else ""

func _snapshot() -> Dictionary:
	var lab := _lab()
	return {"mode": Progress.mode(), "speed": lab.tele.speed if lab != null and lab.tele != null else 0.0, "rm": Settings.reduced_motion, "tel": Settings.telemetry, "node": str(app.campaign_node)}

func _check_expect(e: Dictionary, before: Dictionary) -> void:
	var ex: Dictionary = e.get("expect", {})
	for k in ex:
		match k:
			"text":
				for id in ex[k]:
					var t := _control_state_text(str(id))
					if t.to_lower() != str(ex[k][id]).to_lower(): _fail("the text of %s is '%s', expected '%s'" % [id, t, ex[k][id]])
			"mode_changes": if Progress.mode() == before["mode"]: _fail("the difficulty mode did not change")
			"speed_changes": if _lab() == null or _lab().tele.speed == before["speed"]: _fail("the run speed did not change")
			"setting":
				var key := str(ex[k]); var was: bool = before["rm"] if key == "reduced_motion" else before["tel"]
				if (Settings.reduced_motion if key == "reduced_motion" else Settings.telemetry) == was: _fail("the setting %s did not flip" % key)
			"editor_tool": if _lab().editor.tool != str(ex[k]): _fail("the editor tool is '%s', expected '%s'" % [_lab().editor.tool, ex[k]])
			"reference": if _lab()._is_reference() != bool(ex[k]): _fail("the bench is %sthe reference circuit" % ("not " if bool(ex[k]) else ""))
			"enabled":
				var c := _control(str(ex[k]), str(e["to"]))
				if c == null or (c as BaseButton).disabled: _fail("%s is not enabled" % ex[k])
			"disabled":
				var c2 := _control(str(ex[k]), str(e["to"]))
				if c2 != null and not (c2 as BaseButton).disabled: _fail("%s should be disabled" % ex[k])
			"visible":
				var wait_for := str(ex[k])
				if not await _wait(func(): return _control(wait_for, str(e["to"])) != null, 150.0 if bool(e.get("slow", false)) else 20.0): _fail("%s did not appear" % wait_for)
			"gone":
				if _control(str(ex[k]), str(e["to"])) != null: _fail("%s is still on screen" % ex[k])
			_: _fail("unknown expectation '%s'" % k)

# ---------------------------------------------------------------------------------------------------- one edge
func _walk_edge(e: Dictionary) -> void:
	ctx = "edge " + str(e["id"]); cur_known = str(e.get("known_bug", "")); cur_failed = false
	var from := str(e["from"]); var to := str(e["to"])
	var setup: Dictionary = e.get("setup", {})
	var need_ref := bool(e.get("needs_reference", false))
	_walk_avoid = e.get("path_avoid", []); _walk_level = str(setup.get("level", "$first"))
	var level_tok := str(setup.get("level", "$first"))
	if level_tok == "$dx" and _level("$dx") == "": _fail("the build has no diagnosis case"); return
	if not await _goto(from, setup, need_ref):
		pos_node = ""; _finish_edge(e); return
	var before := _snapshot()
	await _trigger(e, _merge({"level": level_tok}, setup))
	await _frames(2)
	pos_node = ""      # whatever the edge did, the next edge re-navigates unless the return below puts us back
	if e.has("may_finish_to") and not _detect(to) and _detect(str(e["may_finish_to"])): to = str(e["may_finish_to"])   # a fast engine finished the 5-seed test before the first look
	if not _detect(to): _fail("after the action, %s is not what the UI shows (screen %s%s)" % [to, app.cur, _hint_of_state()])
	else:
		var fd: Dictionary = nodes[from]["detect"]; var td: Dictionary = nodes[to]["detect"]
		if from != to and not _implied(to).has(from) and ((fd.has("screen") and td.has("screen") and fd["screen"] != td["screen"]) or str(nodes[from]["kind"]) in ["modal", "overlay", "toast", "tab"]):
			if _detect(from): _fail("%s is still active after the move to %s" % [from, to])
		_arrive(to)
		if _open_modals().size() > 1: _fail("more than one modal is open")
		await _check_expect(e, before)
	if bool(e.get("returns", false)) and not cur_failed: await _check_return(e)
	_finish_edge(e)

func _hint_of_state() -> String:
	var lab := _lab()
	if lab == null: return ""
	var s := ""
	if lab.tele != null: s += "; score passed %s verdict '%s'" % [lab.tele.score.get("passed", null), str(lab.tele.score.get("verdict", ""))]
	s += "; task '%s' view '%s' lesion_on %s" % [lab.task_id, str(lab.insp_body.get_meta("sp_view", "")), lab.lesion_on]
	if lab.banner != null and lab.banner.visible: s += "; banner: " + lab.banner.text
	if lab.celebration != null and lab.celebration._toasts.size() > 0: s += "; toast: " + " / ".join(((lab.celebration._toasts[0]["node"] as Node).find_children("*", "Label", true, false)).map(func(l): return (l as Label).text))
	return s

func _finish_edge(e: Dictionary) -> void:
	seen_edges[e["id"]] = true
	if cur_known != "" and not cur_failed: fails.append("edge %s is marked known_bug but it works: remove the marker" % e["id"])

## Back / Close and Escape must both return to the edge's source. First visit of a node uses the control, the second the key.
func _check_return(e: Dictionary) -> void:
	var to := str(e["to"]); var from := str(e.get("returns_to", e["from"]))
	if e.has("may_finish_to") and not _detect(to) and _detect(str(e["may_finish_to"])): to = str(e["may_finish_to"])
	var bk: Dictionary = nodes[to].get("back", {})
	if bk.is_empty():
		for a in _implied(to):
			if bk.is_empty() and nodes[a].has("back"): bk = nodes[a]["back"]
	if bk.is_empty(): _fail("the graph says the edge returns but node %s declares no back" % to); return
	var tick := int(back_tick.get(to, 0)); back_tick[to] = tick + 1
	var use_key := tick % 2 == 1 and bk.has("key")
	if bk.has("call"):
		if _lab() != null and _lab()._file_dlg != null: _lab()._file_dlg.hide()
		await _frames(2)
	elif use_key: await _key_escape()
	else: await _press_id(str(bk["control"]), to, "back from %s" % to)
	var how := "Escape" if use_key else "Back/Close"
	if e.has("may_finish_to") and _detect(str(e["may_finish_to"])): await _press_id("BtnClose", str(e["may_finish_to"]), "closing the finished test's card")   # Cancel lost the race with the fast engine
	if not _detect(from): _fail("%s from %s does not return to %s (the UI shows screen '%s')" % [how, to, from, app.cur])
	elif str(nodes[to]["kind"]) in ["modal", "screen", "panel"] and not _implied(to).has(from) and not _implied(from).has(to) and _detect(to):
		_fail("%s is still active after %s" % [to, how])
	if _open_modals().size() > 0 and str(nodes[from]["kind"]) != "modal": _fail("a modal is still open after %s: %s" % [how, str(_open_modals().map(func(m): return m.name))])
	if not cur_failed and from == str(e["from"]): pos_node = from; pos_sig = _sig(e.get("setup", {})) + str(bool(e.get("needs_reference", false)))

# ---------------------------------------------------------------------------------------------------- negatives
func _negative(n: Dictionary) -> void:
	ctx = "negative " + str(n["id"]); cur_known = ""; cur_failed = false
	var setup: Dictionary = n.get("setup", {})
	if not await _goto(str(n["node"]), setup, false): pos_node = ""; return
	if app.cur == "tasks" and setup.has("level"): await _select_level(str(setup["level"]))
	var c := _control(str(n["control"]), str(n["node"]))
	match str(n["expect"]):
		"absent": if c != null: _fail("%s is on screen but should be absent: %s" % [n["control"], n.get("why", "")])
		"disabled":
			if c == null: _fail("%s is not on screen" % n["control"])
			elif not (c as BaseButton).disabled: _fail("%s should be disabled: %s" % [n["control"], n.get("why", "")])
	pos_node = ""
