extends SceneTree
## Headless checks of the Analysis screen's wiring matrix, path finder and null-model bench (godot/ui/screens/wiring.gd, analysis.gd; roadmap A2):
##   godot --headless --path godot -s tests/test_wiring.gd        prints "WIRING OK" or "WIRING FAIL: ..." (exit 1)   (scripts/wiring_test.sh, ctest `godot_wiring`)
## Needs the GDExtension (the path finder and the null models are engine functions); skips with a message when it is not loaded.
## matrix: image dimensions, marks of known edges (position, sign), orders and levels; path finder: a chain, a silenced middle cell type, a silenced end, no path;
## null bench: the three models through the engine at both levels with the right model name, the words for the verdict; return to the lab with a cell type selected.

var fails := 0
var checks := 0

func _check(ok: bool, msg: String) -> void:
	checks += 1
	if not ok: fails += 1; print("WIRING FAIL: ", msg)

class StubApp extends RefCounted:
	var lab_select := ""
	var analysis_task := "worm_touch"
	var analysis_circuit := {}
	var analysis_tele = null
	var opened := ""
	func open_lab(t: String, _r := false, _b := false) -> void: opened = t
	func back() -> void: pass

## A chain A > B > C > D (3 hops) and a longer detour A > E > F > G > D (4 hops); X has cells of different out-degree into Y.
func _chain() -> Dictionary:
	var pops := []
	for id in ["A", "B", "C", "D", "E", "F", "G"]: pops.append({"id": id, "n": 1})
	pops.append({"id": "X", "n": 3}); pops.append({"id": "Y", "n": 2})
	var pr := []
	for pq in [["A", "B"], ["B", "C"], ["C", "D"], ["A", "E"], ["E", "F"], ["F", "G"], ["G", "D"]]:
		pr.append({"pre": pq[0], "post": pq[1], "edges": [[0, 0, 10.0, 1.0]], "synapse": {"synapse_model": "static_synapse"}})
	pr.append({"pre": "X", "post": "Y", "edges": [[2, 0, 5.0, 1.0], [2, 1, 5.0, 1.0], [0, 1, 5.0, 1.0]], "synapse": {"synapse_model": "static_synapse"}})
	return {"schema_version": 1, "name": "chain", "populations": pops, "projections": pr, "inputs": [], "outputs": []}

func _init() -> void:
	await process_frame
	if not ClassDB.class_exists("SpikeSim"): print("WIRING SKIP: the GDExtension is not loaded"); quit(0); return
	var worm: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://circuits/worm_touch.json"))
	_matrix(worm)
	_orders()
	_paths(worm)
	_nulls(worm)
	await _screen(worm)
	print("WIRING OK (%d checks)" % checks if fails == 0 else "WIRING FAILED: %d of %d" % [fails, checks])
	quit(1 if fails > 0 else 0)

func _matrix(worm: Dictionary) -> void:
	var w := Wiring.of(worm)
	_check(w.n == 17 and w.np == 9, "worm_touch has 17 cells in 9 cell types (got %d in %d)" % [w.n, w.np])
	_check(w.cells and w.skipped == 0, "every connection of worm_touch is known per cell")
	var alm0 := 0; var avd1 := w.first[Circuits.pop_index(worm, "AVD")] + 1
	_check(w.cnt[alm0 * w.n + avd1] == 1 and is_equal_approx(w.w[alm0 * w.n + avd1], 75.0), "ALM cell 0 to AVD cell 1 is one connection of weight 75")
	var ava0 := w.first[Circuits.pop_index(worm, "AVA")]; var avb0 := w.first[Circuits.pop_index(worm, "AVB")]
	_check(is_equal_approx(w.w[ava0 * w.n + avb0], -4000.0), "AVA to AVB (all_to_all, inhibitory) has weight -4000")
	var lay := w.layout(0, 0); var r := w.render(lay)
	var img: Image = r["img"]
	_check(int(r["m"]) == 17 and img.get_width() == 17 * int(r["b"]) and img.get_height() == img.get_width(), "cell-level image is 17 marks square (%d px, %d per mark)" % [img.get_width(), int(r["b"])])
	var b := int(r["b"]); var pos: PackedInt32Array = lay["pos"]
	var px: Color = img.get_pixel(pos[avd1] * b + b / 2, pos[alm0] * b + b / 2)
	_check(not px.is_equal_approx(Kit.WELL) and px.r > px.b, "the ALM > AVD mark is drawn in the excitatory colour")
	var pi: Color = img.get_pixel(pos[avb0] * b + b / 4, pos[ava0] * b + b / 2)
	_check(not pi.is_equal_approx(Kit.WELL) and pi.b > pi.r, "the AVA > AVB mark is drawn in the inhibitory colour")
	_check(img.get_pixel(pos[alm0] * b + 2, pos[alm0] * b + 2).is_equal_approx(Kit.WELL), "a pair with no connection is left blank")
	var lay1 := w.layout(0, 1); var r1 := w.render(lay1)
	_check(int(lay1["level"]) == 1 and int(r1["m"]) == 9 and (r1["img"] as Image).get_width() == 9 * int(r1["b"]), "cell-type level is a 9 by 9 matrix")
	var ia := Circuits.pop_index(worm, "AVD"); var ib := Circuits.pop_index(worm, "AVA")
	var v := w.at(lay1, int(lay1["pos"][ia]), int(lay1["pos"][ib]))
	_check(int(v.x) == 4 and is_equal_approx(v.y, 925.0 + 925.0 + 1025.0 + 1300.0), "AVD > AVA at cell-type level: 4 connections, weights summed (got %s)" % str(v))
	_check(w.describe(lay1, int(lay1["pos"][ia]), int(lay1["pos"][ib])).begins_with("AVD > AVA, 4 connections"), "the caption reads 'pre > post, N connections, weight' (%s)" % w.describe(lay1, int(lay1["pos"][ia]), int(lay1["pos"][ib])))
	_check(w.describe(lay, 0, 0).contains("no connection"), "an empty pair says so")
	# a group scales and signs the weights the engine runs (effective weights)
	var gc := worm.duplicate(true); gc["groups"] = [{"id": "g", "pops": ["ALM"], "sign": -1, "scale": 2.0, "free": ["sign", "scale"]}]
	var wg := Wiring.of(gc)
	_check(is_equal_approx(wg.w[alm0 * wg.n + avd1], -150.0), "a group's sign and scale are applied: 75 becomes -150 (got %s)" % str(wg.w[alm0 * wg.n + avd1]))
	# the image stays small work at 2,000 cells: a ring of 2,000 cells in one type
	var big := {"populations": [{"id": "R", "n": 2000}], "projections": [{"pre": "R", "post": "R", "rule": "one_to_one", "synapse": {"synapse_model": "static_synapse", "weight": 5.0}}]}
	var t0 := Time.get_ticks_msec(); var wb := Wiring.of(big); var rb := wb.render(wb.layout(0, 0))
	_check((rb["img"] as Image).get_width() == 2000 and wb.cells, "a 2,000-cell matrix is one 2,000 px image")
	_check(Time.get_ticks_msec() - t0 < 20000, "a 2,000-cell matrix is built in a few seconds (%d ms)" % (Time.get_ticks_msec() - t0))

func _orders() -> void:
	var c := _chain(); var w := Wiring.of(c)
	var x := Circuits.pop_index(c, "X"); var fx := w.first[x]
	var l0 := w.layout(0, 0); var items0: PackedInt32Array = l0["items"]
	var ix := items0.find(fx)
	_check(items0[ix] == fx and items0[ix + 1] == fx + 1 and items0[ix + 2] == fx + 2, "by cell type keeps the cells of a type in file order")
	var l1 := w.layout(1, 0); var i1: PackedInt32Array = l1["items"]; var j := i1.find(fx)
	_check(i1[j] == fx + 2 and i1[j + 1] == fx and i1[j + 2] == fx + 1, "by out-degree puts the cell with two targets first (got %s)" % str([i1[j], i1[j + 1], i1[j + 2]]))
	var y := Circuits.pop_index(c, "Y"); var fy := w.first[y]
	var l2 := w.layout(2, 0); var i2: PackedInt32Array = l2["items"]; var k := i2.find(fy)
	_check(i2[k] == fy + 1 and i2[k + 1] == fy, "by in-degree puts the cell with two inputs first")
	_check(int(l1["spans"][0]["a"]) == 0 and int(l1["spans"][-1]["b"]) == w.n, "the type spans tile the matrix")
	var lt := w.layout(1, 1); var it: PackedInt32Array = lt["items"]
	_check(int(lt["level"]) == 1 and it.size() == w.np and str(w.ids[it[0]]) in ["A", "B", "C", "D", "E", "F", "G", "X"], "cell-type level by out-degree lists one row per type, busiest first")
	var lt0 := w.layout(0, 1); var ord0: PackedInt32Array = lt0["items"]
	_check(ord0[0] == 0 and ord0[w.np - 1] == w.np - 1, "cell-type level by cell type is the file order")

func _paths(worm: Dictionary) -> void:
	var c := _chain(); var w := Wiring.of(c)
	var pi := func(id: String) -> int: return Circuits.pop_index(c, id)
	var q := w.path_query(pi.call("A"), pi.call("D"), -1)
	_check(str(q["text"]) == "3 hops: A > B > C > D", "chain path text (got '%s')" % q["text"])
	var rects := w.outline_rects(w.layout(0, 0), q)
	_check(rects.size() == 3 and (rects[0] as Rect2i).size == Vector2i(1, 1), "the three links of the shortest path are outlined (got %d)" % rects.size())
	var lay := w.layout(0, 0); var pos: PackedInt32Array = lay["pos"]
	_check((rects[0] as Rect2i).position == Vector2i(pos[w.first[pi.call("B")]], pos[w.first[pi.call("A")]]), "the first outline is the A to B mark (column B, row A)")
	var qb := w.path_query(pi.call("A"), pi.call("D"), pi.call("B"))
	_check(str(qb["text"]) == "4 hops without B: A > E > F > G > D", "silencing B sends the path round by E, F, G (got '%s')" % qb["text"])
	var qe := w.path_query(pi.call("A"), pi.call("D"), pi.call("E"))
	_check(str(qe["text"]) == "3 hops without E: A > B > C > D", "silencing a cell type off the path changes nothing (got '%s')" % qe["text"])
	var qn := w.path_query(pi.call("D"), pi.call("A"), -1)
	_check(str(qn["text"]) == "No path from D to A." and not bool(qn["reachable"]), "no path is said in words (got '%s')" % qn["text"])
	var qd := w.path_query(pi.call("A"), pi.call("D"), pi.call("D"))
	_check(str(qd["text"]).begins_with("No path from A to D without D"), "silencing the end leaves no path (got '%s')" % qd["text"])
	# the real worm: the touch pathway, and what silencing AVD does to it
	var ww := Wiring.of(worm); var wi := func(id: String) -> int: return Circuits.pop_index(worm, id)
	var q1 := ww.path_query(wi.call("ALM"), wi.call("MA"), -1)
	_check(str(q1["text"]) == "3 hops: ALM > AVD > AVA > MA", "worm_touch: ALM to MA (got '%s')" % q1["text"])
	var q2 := ww.path_query(wi.call("ALM"), wi.call("MA"), wi.call("AVD"))
	_check(str(q2["text"]) == "No path from ALM to MA without AVD.", "worm_touch: without AVD there is no path (got '%s')" % q2["text"])
	var q3 := ww.path_query(wi.call("ALM"), wi.call("MB"), -1)
	_check(str(q3["text"]) == "4 hops: ALM > AVD > AVA > AVB > MB", "worm_touch: ALM to MB runs through the inhibitory AVA to AVB link (got '%s')" % q3["text"])

func _nulls(worm: Dictionary) -> void:
	var names := ["edge_count", "degree", "blocks"]
	for level in [0, 1]:
		for model in [0, 1, 2]:
			var d := Wiring.null_run(worm, level, model, 0, 20)
			_check(not d.has("error"), "null model %d at level %d returns without error (%s)" % [model, level, str(d.get("error", ""))])
			if d.has("error"): continue
			_check(str(d.get("model", "")) == names[model], "model %d reports its name '%s' (got '%s')" % [model, names[model], str(d.get("model", ""))])
			var ms: Array = d.get("motifs", [])
			_check(ms.size() == 13 and (ms[0] as Dictionary).has("null_counts") and ((ms[4] as Dictionary)["null_counts"] as Array).size() == 20, "model %d at level %d carries 13 classes with 20 null counts each" % [model, level])
	var blocks := Wiring.blocks(worm, true)
	_check(blocks.size() == 17 and Wiring.blocks(worm, false).size() == 9, "block ids: one per cell, one per cell type")
	var gc := worm.duplicate(true); gc["groups"] = [{"id": "sensory", "pops": ["ALM", "AVM", "PLM"], "sign": 1, "scale": 1.0, "free": []}]
	var bg := Wiring.blocks(gc, false)
	_check(bg[0] == bg[1] and bg[1] == bg[2] and bg[3] != bg[0], "cell types in one group share a block")
	# the words
	var base := {"index": 4, "count": 12, "null_mean": 4.0, "null_sd": 1.5, "z": 5.3, "z_defined": true, "significant": true}
	_check(Wiring.null_text(base, 1, 100, false).contains("more often than chance under this null") and Wiring.null_text(base, 1, 100, false).contains("same degrees"), "over-represented, with the null's name")
	var less := base.duplicate(); less["z"] = -4.0
	_check(Wiring.null_text(less, 2, 100, false).contains("less often than chance under this null") and Wiring.null_text(less, 2, 100, false).contains("same degrees and cell-type blocks"), "under-represented")
	var same := base.duplicate(); same["significant"] = false; same["z"] = 0.4
	_check(Wiring.null_text(same, 0, 100, false).contains("about as often as chance under this null") and Wiring.null_text(same, 0, 100, false).contains("same number of edges"), "about as often")
	var h := Wiring.hist([1, 2, 2, 3, 9], 5)
	var tot := 0
	for v in (h["bins"] as PackedInt32Array): tot += v
	_check(int(h["lo"]) == 1 and int(h["hi"]) == 9 and tot == 5, "the histogram holds every null count and the real one lies inside its range")

func _screen(worm: Dictionary) -> void:
	var stub := StubApp.new()
	var s := AnalysisScreen.new(); s.app = stub; s.circuit = worm.duplicate(true); s.null_n = 20
	root.add_child(s); s.size = Vector2(1600, 900); s.build()
	await process_frame
	_check(s.result.has("motifs"), "the 3-node tab computes on open")
	_check(s.strip.m.has("count") and s.strip.counts.size() == 20, "the strip holds the selected class and its 20 null counts")
	# the null model stepper: three models, each named
	var names := ["edge_count", "degree", "blocks"]
	var nm := s.find_child("StepNullModel", true, false) as Kit.Stepper
	_check(nm != null, "StepNullModel is on the screen")
	for i in [2, 0, 1]:   # the stepper opens on "same degrees": next is blocks, then it wraps to the edge count, then back
		nm.step(1)
		_check(s.null_model == i and s.chart.model == i, "StepNullModel selects model %d" % i)
		if i != 1: _check(str(s.result.get("model", "")) == names[i], "model %d: the chart shows the engine's '%s' null (got '%s')" % [i, names[i], str(s.result.get("model", ""))])
		_check(s.result.has("motifs") and s.strip.counts.size() == 20, "model %d: the strip has its histogram data" % i)
	var mot := s.find_child("StepMotif", true, false) as Kit.Stepper
	mot.step(1)
	_check(s.motif_sel == Wiring.DEFAULT_MOTIF + 1 and int(s.strip.m.get("index", -1)) == s.motif_sel, "StepMotif moves the strip to the next class")
	# the wiring tab
	var tabs := s.find_child("StepTab", true, false) as Kit.Stepper
	for _i in 7: tabs.step(1)
	_check(s.tab == 7 and s.wbox.visible and not s.chart.visible, "the wiring tab shows the matrix and hides the census")
	_check(s.wlay.has("level") and int(s.wlay["level"]) == 0 and int(s.wview.rend["m"]) == 17, "the matrix opens at cell level, 17 cells")
	var order := s.find_child("StepOrder", true, false) as Kit.Stepper
	order.step(1)
	_check(s.wiring_order == 1, "StepOrder: by out-degree")
	order.step(1); order.step(1)
	_check(s.wiring_order == 0, "StepOrder wraps to by cell type")
	var lvl := s.find_child("StepWiringLevel", true, false) as Kit.Stepper
	lvl.step(1)
	_check(s.wiring_level == 1 and int(s.wlay["level"]) == 1 and int(s.wview.rend["m"]) == 9, "StepWiringLevel: cell types, a 9 by 9 matrix")
	lvl.step(1)
	_check(int(s.wlay["level"]) == 0, "StepWiringLevel: back to cells")
	# the path finder through its pickers
	var fr := s.find_child("StepFrom", true, false) as Kit.Stepper; var to := s.find_child("StepTo", true, false) as Kit.Stepper; var wo := s.find_child("StepWithout", true, false) as Kit.Stepper
	_check(fr != null and to != null and wo != null, "the three pickers are on the screen")
	s.w_from = Circuits.pop_index(worm, "ALM"); s.w_to = Circuits.pop_index(worm, "MA"); s.w_ban = -1; s._wiring_path()
	_check(s.wresult.text == "3 hops: ALM > AVD > AVA > MA", "the result line names the path (got '%s')" % s.wresult.text)
	_check(s.wview.rects.size() > 0, "the cells on the path are outlined in the matrix")
	s.w_ban = Circuits.pop_index(worm, "AVD"); s._wiring_path()
	_check(s.wresult.text == "No path from ALM to MA without AVD." and s.wview.rects.is_empty(), "silencing AVD: no path, nothing outlined (got '%s')" % s.wresult.text)
	wo.step(1)   # nothing > ALM
	_check(s.w_ban == Circuits.pop_index(worm, "ALM") and s.wresult.text.begins_with("No path"), "StepWithout steps through the cell types")
	# a population name tapped: back to the lab with it selected
	s.pick_pop("AVD")
	_check(stub.lab_select == "AVD" and stub.opened == "worm_touch", "tapping a name returns to the lab on the same task with that cell type selected")
	s.queue_free()
