class_name Wiring
extends RefCounted
## The wiring of a circuit as a matrix, a path finder and a null-model bench (Atlas, roadmap A2). Pure logic plus two drawing controls; the Analysis screen
## (analysis.gd, tab "wiring" and the 3-node tab) hosts them. docs/ENGINE-STRUCTURE.md "The Analysis screen: wiring".
##   Wiring.of(circuit)                  the cell-level adjacency of the circuit as the file states it (edges lists, all_to_all, one_to_one, gap-junction mirrors);
##                                       connections a rule draws at random (fixed_indegree and the like) are counted per cell type only, not per cell
##   layout(order, level)                rows and columns in display order, population spans
##   render(layout)                      the matrix as an Image, one block of pixels per mark (not one draw call per mark)
##   path_query(from, to, without)       shortest path between two cell types through the engine's graph_path_json_edges
##   null_run(circuit, level, model, ...) the 3-node census against one of three null models (engine motif_null_json)
## Weights are the effective ones the engine runs (Circuits.proj_weight_eff: |stored| x scale x sign for a cell type in a group).

const MAX_CELLS := 2500       # above this the cell-level matrix is not built (n x n memory); the cell-type level always is
const ORDERS := [[0, "by cell type"], [1, "by out-degree"], [2, "by in-degree"]]
const LEVELS := [[0, "cells"], [1, "cell types"]]
const NULLS := [[0, "same number of edges"], [1, "same degrees"], [2, "same degrees and cell-type blocks"]]
const NULL_PHRASE := ["random graphs with the same number of edges", "degree-preserving rewiring", "rewiring that keeps the degrees and the cell-type blocks"]
## Engine order of the 13 connected 3-node classes (engine/src/analysis/analysis.cpp kMotifs).
const MOTIFS := ["fan out", "chain", "mutual pair driving", "fan in", "feed forward loop", "mutual pair co-driving", "mutual pair fed", "two mutual pairs",
	"feedback loop", "mutual pair in loop", "mutual pair co-driven", "mutual pair double", "fully connected"]
const DEFAULT_MOTIF := 4

var circuit := {}
var n := 0                    # cells
var np := 0                   # cell types (populations)
var ids: Array = []           # population ids
var first := PackedInt32Array()   # first cell of each population, np + 1 entries
var pop_of := PackedInt32Array()  # population of each cell (cell level only)
var cells := false            # the cell-level arrays exist (n <= MAX_CELLS)
var cnt := PackedInt32Array() # connections per (pre cell, post cell), index pre * n + post
var w := PackedFloat64Array() # summed effective weight per cell pair
var tp := PackedInt32Array()  # distinct cell pairs, pre
var tq := PackedInt32Array()  # distinct cell pairs, post
var pcnt := PackedInt32Array()    # connections per (pre type, post type), index pre * np + post
var pw := PackedFloat64Array()    # summed effective weight per type pair
var skipped := 0              # connections drawn at random by the engine: in pcnt, not in the cell arrays
var unknown := 0              # projections with a rule this view cannot read
var _grouped := PackedByteArray()
var _fac := PackedFloat64Array()
var _outd := PackedInt32Array()
var _ind := PackedInt32Array()

static func of(c: Dictionary) -> Wiring:
	var g := Wiring.new(); g._build(c); return g

func _build(c: Dictionary) -> void:
	circuit = c
	var pops: Array = c.get("populations", [])
	np = pops.size(); first.resize(np + 1)
	for i in np:
		first[i] = n; ids.append(str(pops[i].get("id", i))); n += Circuits.pop_n(pops[i])
	first[np] = n
	cells = n <= MAX_CELLS
	pcnt.resize(np * np); pw.resize(np * np)
	if cells:
		cnt.resize(n * n); w.resize(n * n); pop_of.resize(n)
		for i in np:
			for k in range(first[i], first[i + 1]): pop_of[k] = i
	_grouped.resize(np); _fac.resize(np)
	for i in np:
		var gd := Circuits.group_of_pop(c, ids[i])
		_grouped[i] = 0 if gd.is_empty() else 1
		_fac[i] = Circuits.pop_factor(c, ids[i])
	for pr in c.get("projections", []):
		_projection(pr as Dictionary)

func _eff(pa: int, wv: float) -> float:
	return absf(wv) * _fac[pa] if _grouped[pa] == 1 else wv

func _projection(pr: Dictionary) -> void:
	var pops: Array = circuit.get("populations", [])
	var pa := Circuits.pop_index(circuit, Circuits.proj_pop(pr.get("pre", "")))
	var pb := Circuits.pop_index(circuit, Circuits.proj_pop(pr.get("post", "")))
	if pa < 0 or pb < 0: return
	var na := Circuits.pop_n(pops[pa]); var nb := Circuits.pop_n(pops[pb])
	var gap := str((pr.get("synapse", {}) as Dictionary).get("synapse_model", "")) == "gap_junction"
	if pr.has("edges"):
		for e in pr["edges"]:
			if not (e is Array) or (e as Array).size() < 3: continue
			var i := int(e[0]); var j := int(e[1])
			if i < 0 or i >= na or j < 0 or j >= nb: continue
			_edge(pa, pb, i, j, _eff(pa, float(e[2])), gap)
		return
	var rule := str(pr.get("rule", ""))
	var syn: Variant = (pr.get("synapse", {}) as Dictionary).get("weight", 1.0)
	if not (syn is float or syn is int): unknown += 1; return   # a weight drawn from a distribution
	var wv := Circuits.proj_weight_eff(circuit, pr)
	match rule:
		"all_to_all":
			var auto := bool(pr.get("allow_autapses", not gap))
			if cells:
				for i in na:
					for j in nb:
						if pa == pb and i == j and not auto: continue
						_add(first[pa] + i, first[pb] + j, wv, pa, pb)
			else:
				_bulk(pa, pb, na * nb - (mini(na, nb) if pa == pb and not auto else 0), wv)
		"one_to_one":
			for i in mini(na, nb): _edge(pa, pb, i, i, wv, gap)
		"fixed_indegree": _random(pa, pb, maxi(0, int(pr.get("indegree", 0))) * nb, wv)
		"fixed_outdegree": _random(pa, pb, maxi(0, int(pr.get("outdegree", 0))) * na, wv)
		"fixed_total_number": _random(pa, pb, maxi(0, int(pr.get("N", 0))), wv)
		"pairwise_bernoulli": _random(pa, pb, roundi(clampf(float(pr.get("p", 0.0)), 0.0, 1.0) * float(na) * float(nb)), wv)
		_: unknown += 1

func _edge(pa: int, pb: int, i: int, j: int, wv: float, gap: bool) -> void:
	if cells:
		_add(first[pa] + i, first[pb] + j, wv, pa, pb)
		if gap: _add(first[pb] + j, first[pa] + i, wv, pb, pa)
	else:
		_bulk(pa, pb, 1, wv)
		if gap: _bulk(pb, pa, 1, wv)

func _add(a: int, b: int, wv: float, pa: int, pb: int) -> void:
	if absf(wv) < 1e-12: return   # a cut connection (weight 0) is no wiring
	var pi := pa * np + pb
	pcnt[pi] += 1; pw[pi] += wv
	var idx := a * n + b
	if cnt[idx] == 0: tp.append(a); tq.append(b)
	cnt[idx] += 1; w[idx] += wv

func _bulk(pa: int, pb: int, k: int, wv: float) -> void:
	if k <= 0 or absf(wv) < 1e-12: return
	var pi := pa * np + pb
	pcnt[pi] += k; pw[pi] += float(k) * wv

func _random(pa: int, pb: int, k: int, wv: float) -> void:
	if k <= 0 or absf(wv) < 1e-12: return
	_bulk(pa, pb, k, wv)
	if cells: skipped += k

## True when the path finder runs over cells (every connection is known per cell); otherwise over cell types.
func path_on_cells() -> bool: return cells and skipped == 0

func pop_name(pi: int) -> String: return str(ids[pi]) if pi >= 0 and pi < np else ""

## A cell as "AVD" (a cell type of one cell) or "AVD 2" (the second of several).
func cell_name(a: int) -> String:
	var pi := pop_of[a]
	return pop_name(pi) if first[pi + 1] - first[pi] == 1 else "%s %d" % [pop_name(pi), a - first[pi] + 1]

func out_degree(a: int) -> int:
	_degrees(); return _outd[a]
func in_degree(a: int) -> int:
	_degrees(); return _ind[a]
func _degrees() -> void:
	if _outd.size() == n or not cells: return
	_outd.resize(n); _ind.resize(n)
	for k in tp.size(): _outd[tp[k]] += 1; _ind[tq[k]] += 1

# ---------- order ----------
## Rows and columns in display order. order: 0 by cell type (cell-type groups first when the circuit has them), 1 by out-degree, 2 by in-degree (within a cell type
## at the cell level, over the whole circuit at the cell-type level). level 0 = cells, 1 = cell types (forced when there are too many cells to draw).
## Returns {level, n, items (display index -> cell or type), pos (item -> display index), spans [{pop, id, a, b}], ps, pe (per type: first and end display index)}.
func layout(order: int, level: int) -> Dictionary:
	var lv := level if cells else 1
	var gcount := Circuits.groups(circuit).size()
	var pops_order: Array = range(np)
	var gi := PackedInt32Array(); gi.resize(np)
	for i in np:
		var gd := Circuits.group_of_pop(circuit, ids[i])
		gi[i] = Circuits.group_index(circuit, str(gd.get("id", ""))) if not gd.is_empty() else gcount + i
	pops_order.sort_custom(func(x, y): return gi[x] < gi[y] or (gi[x] == gi[y] and x < y))
	var items := PackedInt32Array(); var spans: Array = []
	if lv == 1:
		if order != 0:
			var deg := PackedInt32Array(); deg.resize(np)
			for a in np:
				for b in np: deg[a if order == 1 else b] += pcnt[a * np + b]
			pops_order.sort_custom(func(x, y): return deg[x] > deg[y] or (deg[x] == deg[y] and x < y))
		for k in pops_order.size():
			items.append(int(pops_order[k])); spans.append({"pop": int(pops_order[k]), "id": ids[pops_order[k]], "a": k, "b": k + 1})
	else:
		for pi in pops_order:
			var cs: Array = range(first[pi], first[pi + 1])
			if order != 0:
				_degrees(); cs.sort_custom(_by_out if order == 1 else _by_in)
			var a0 := items.size()
			for c in cs: items.append(int(c))
			spans.append({"pop": int(pi), "id": ids[pi], "a": a0, "b": items.size()})
	var pos := PackedInt32Array(); pos.resize(n if lv == 0 else np)
	for k in items.size(): pos[items[k]] = k
	var ps := PackedInt32Array(); var pe := PackedInt32Array(); ps.resize(np); pe.resize(np)
	for s in spans: ps[int(s["pop"])] = int(s["a"]); pe[int(s["pop"])] = int(s["b"])
	return {"level": lv, "n": items.size(), "items": items, "pos": pos, "spans": spans, "ps": ps, "pe": pe}

func _by_out(x: int, y: int) -> bool: return _outd[x] > _outd[y] or (_outd[x] == _outd[y] and x < y)
func _by_in(x: int, y: int) -> bool: return _ind[x] > _ind[y] or (_ind[x] == _ind[y] and x < y)

# ---------- the matrix ----------
## Count and summed weight of display row r (presynaptic) and column c (postsynaptic).
func at(lay: Dictionary, r: int, c: int) -> Vector2:
	var it: PackedInt32Array = lay["items"]
	if r < 0 or c < 0 or r >= it.size() or c >= it.size(): return Vector2.ZERO
	if int(lay["level"]) == 1: return Vector2(pcnt[it[r] * np + it[c]], pw[it[r] * np + it[c]])
	var idx := it[r] * n + it[c]
	return Vector2(cnt[idx], w[idx])

static func mark_color(wv: float, wmax: float) -> Color:
	var s := clampf(absf(wv) / maxf(wmax, 1e-9), 0.0, 1.0)
	var base: Color = Kit.exc() if wv > 0.0 else Kit.inh()
	return Kit.PANEL.lerp(base, 0.42 + 0.58 * s).lerp(Kit.TEXT, 0.4 * s)   # stronger = darker

## The matrix as an image: b x b pixels per mark (b = 1 for a very large circuit). Returns {img, b, side, m, wmax}. A mark of an inhibitory pair is split by a
## fine bar (the second code beside blue) when b >= 3; the cell-type level draws the arrowhead / bar glyphs over the marks (WiringView).
func render(lay: Dictionary, max_px := 640) -> Dictionary:
	var m: int = lay["n"]
	var b := clampi(max_px / maxi(m, 1), 1, 48)
	var side := maxi(m * b, 1)
	var img := Image.create(side, side, false, Image.FORMAT_RGBA8)
	img.fill(Kit.WELL)
	var pos: PackedInt32Array = lay["pos"]
	var wmax := 0.0
	var marks: Array = []   # [x, y, w]
	if int(lay["level"]) == 1:
		for a in np:
			for c in np:
				var v := pw[a * np + c]
				if pcnt[a * np + c] > 0: marks.append([pos[c], pos[a], v]); wmax = maxf(wmax, absf(v))
	else:
		for k in tp.size():
			var v := w[tp[k] * n + tq[k]]
			marks.append([pos[tq[k]], pos[tp[k]], v]); wmax = maxf(wmax, absf(v))
	for mk in marks:
		var col := mark_color(float(mk[2]), wmax)
		var x := int(mk[0]) * b; var y := int(mk[1]) * b
		if b == 1: img.set_pixel(x, y, col)
		else:
			img.fill_rect(Rect2i(x, y, b, b), col)
			if b >= 3 and float(mk[2]) < 0.0: img.fill_rect(Rect2i(x + b / 2, y, 1, b), Kit.PANEL.lerp(col, 0.25))
	return {"img": img, "b": b, "side": side, "m": m, "wmax": wmax}

## "pre > post, N connections, weight W" for the mark at display row r, column c.
func describe(lay: Dictionary, r: int, c: int) -> String:
	var it: PackedInt32Array = lay["items"]
	if r < 0 or c < 0 or r >= it.size() or c >= it.size(): return ""
	var a: String; var b: String
	if int(lay["level"]) == 1: a = pop_name(it[r]); b = pop_name(it[c])
	else: a = cell_name(it[r]); b = cell_name(it[c])
	var v := at(lay, r, c)
	if int(v.x) == 0: return "%s > %s, no connection" % [a, b]
	return "%s > %s, %d connection%s, %sweight %.1f" % [a, b, int(v.x), "" if int(v.x) == 1 else "s", "summed " if int(lay["level"]) == 1 and int(v.x) > 1 else "", v.y]

func notes() -> String:
	var s := "Paths run between cell types. Sensors and motors are not part of the graph."
	if skipped > 0: s += " %d connections that the engine draws at random are counted per cell type and are not shown per cell." % skipped
	if unknown > 0: s += " %d projections use a rule or a weight this view cannot read." % unknown
	if not cells: s += " Too many cells to draw one by one (%d): the matrix shows cell types." % n
	return s

# ---------- path finder ----------
## Shortest path from cell type `src` to `dst` that avoids cell type `ban` (-1 = none). Runs over cells when every connection is known per cell, over cell types
## otherwise. Returns {ok, reachable, text, names, hops, pairs (node pairs on shortest paths), cells (node space)}.
func path_query(src: int, dst: int, ban: int) -> Dictionary:
	var out := {"ok": false, "reachable": false, "text": "", "names": [], "hops": -1, "pairs": [], "cells": path_on_cells()}
	if src < 0 or dst < 0 or src >= np or dst >= np: out["text"] = "Pick two cell types."; return out
	var from_to := "%s to %s" % [pop_name(src), pop_name(dst)]
	var without := (" without " + pop_name(ban)) if ban >= 0 else ""
	if ban == src or ban == dst:
		out["text"] = "No path from %s%s: the silenced cell type is the start or the end." % [from_to, without]; out["ok"] = true; return out
	if not ClassDB.class_exists("SpikeSim"): out["text"] = "The spike-gd GDExtension is not loaded."; return out
	var on_cells := path_on_cells()
	var tr := PackedFloat64Array(); var nn := np
	var s := PackedInt32Array(); var t := PackedInt32Array(); var bn := PackedInt32Array()
	if on_cells:
		nn = n
		for k in tp.size(): tr.append(float(tp[k])); tr.append(float(tq[k])); tr.append(w[tp[k] * n + tq[k]])
		for k in range(first[src], first[src + 1]): s.append(k)
		for k in range(first[dst], first[dst + 1]): t.append(k)
		if ban >= 0:
			for k in range(first[ban], first[ban + 1]): bn.append(k)
	else:
		for a in np:
			for b in np:
				if a != b and pcnt[a * np + b] > 0: tr.append(float(a)); tr.append(float(b)); tr.append(pw[a * np + b])
		s.append(src); t.append(dst)
		if ban >= 0: bn.append(ban)
	var d := Circuits.parse(str(SpikeSim.graph_path_json_edges(tr, nn, s, t, bn)))
	if d.is_empty() or d.has("error"): out["text"] = "The path could not be computed: " + str(d.get("error", "no answer from the engine")); return out
	out["ok"] = true
	if not bool(d.get("reachable", false)):
		out["text"] = "No path from %s%s." % [from_to, without]; return out
	out["reachable"] = true
	var names: Array = []
	for v in d.get("path", []):
		var pn := pop_name(pop_of[int(v)] if on_cells else int(v))
		if names.is_empty() or names[-1] != pn: names.append(pn)
	out["names"] = names; out["hops"] = names.size() - 1
	var h := int(out["hops"])
	out["text"] = "%d hop%s%s: %s" % [h, "" if h == 1 else "s", without, " > ".join(PackedStringArray(names))]
	# the edges that lie on some shortest path: both ends on a shortest path and one hop further from the sources
	var on := {}
	for v in d.get("on_paths", []): on[int(v)] = true
	var hops: Array = d.get("hops", [])
	var pairs: Array = []
	for k in range(0, tr.size() - 2, 3):
		var u := int(tr[k]); var v := int(tr[k + 1])
		if on.has(u) and on.has(v) and int(hops[v]) == int(hops[u]) + 1: pairs.append(Vector2i(u, v))
	out["pairs"] = pairs
	return out

## Rectangles, in matrix units (x = column, y = row), to outline for the shortest-path edges of a path_query result under layout `lay`.
func outline_rects(lay: Dictionary, q: Dictionary) -> Array:
	var out: Array = []; var seen := {}
	var pos: PackedInt32Array = lay["pos"]; var ps: PackedInt32Array = lay["ps"]; var pe: PackedInt32Array = lay["pe"]
	for pr in q.get("pairs", []):
		var u := int(pr.x); var v := int(pr.y); var r: Rect2i
		if bool(q.get("cells", false)):
			if int(lay["level"]) == 0: r = Rect2i(pos[v], pos[u], 1, 1)
			else: r = Rect2i(ps[pop_of[v]], ps[pop_of[u]], 1, 1)
		else:
			r = Rect2i(ps[v], ps[u], pe[v] - ps[v], pe[u] - ps[u])
		var key := "%d,%d,%d,%d" % [r.position.x, r.position.y, r.size.x, r.size.y]
		if not seen.has(key): seen[key] = true; out.append(r)
	return out

# ---------- null models ----------
## One block id per cell (per_cell) or per cell type: the cell-type group when the circuit has groups, else the population.
static func blocks(c: Dictionary, per_cell: bool) -> PackedInt32Array:
	var pops: Array = c.get("populations", []); var gcount := Circuits.groups(c).size(); var out := PackedInt32Array()
	for i in pops.size():
		var id := str(pops[i].get("id", i)); var gd := Circuits.group_of_pop(c, id)
		var blk := Circuits.group_index(c, str(gd.get("id", ""))) if not gd.is_empty() else gcount + i
		if per_cell:
			for _k in Circuits.pop_n(pops[i]): out.append(blk)
		else: out.append(blk)
	return out

static func signed_triples(tr: PackedFloat64Array, sign_idx: int) -> PackedFloat64Array:
	if sign_idx == 0: return tr
	var out := PackedFloat64Array()
	for k in range(0, tr.size() - 2, 3):
		if (sign_idx == 1 and tr[k + 2] > 0.0) or (sign_idx == 2 and tr[k + 2] < 0.0): out.append(tr[k]); out.append(tr[k + 1]); out.append(tr[k + 2])
	return out

## The 3-node census against null `model` (0 edge count, 1 degrees, 2 degrees and cell-type blocks). level 0 = cell types as nodes, 1 = cells (the engine's own graph, read from
## NEST). Returns the engine's parsed JSON ({model, null_samples, motifs: [{count, null_mean, null_sd, z, significant, null_counts, ...}], ...}) or {"error": text}.
## sign_idx (0 all, 1 excitatory, 2 inhibitory) filters the edges at the cell-type level; the cell level reads every edge.
static func null_run(c: Dictionary, level: int, model: int, sign_idx: int, n_null: int) -> Dictionary:
	if not ClassDB.class_exists("SpikeSim"): return {"error": "The spike-gd GDExtension is not loaded."}
	var seed := int(c.get("seed", 1)); var txt := ""
	if level == 0:
		var g := AnalysisScreen.population_graph(c)
		if int(g["n"]) < 3: return {"error": "Fewer than 3 populations: wire at least three to count three-node motifs."}
		txt = str(SpikeSim.motif_null_json_edges(signed_triples(g["edges"], sign_idx), int(g["n"]), model, blocks(c, false), n_null, seed))
	else:
		var sim = ClassDB.instantiate("SpikeSim")
		if not sim.load_circuit(Circuits.to_json(c)): return {"error": "The engine rejected the circuit: " + str(sim.error())}
		txt = str(sim.motif_null_json(model, blocks(c, true), n_null, seed)); sim.stop()
	var d := Circuits.parse(txt)
	return d if not d.is_empty() else {"error": "The engine gave no answer."}

## Words for one motif class of a null-bench result. Never a bare number: the null model is named first.
static func null_text(m: Dictionary, model: int, n_null: int, degenerate: bool) -> String:
	var mn: String = MOTIFS[clampi(int(m.get("index", 0)), 0, 12)]
	var head := "Null model: %s (%d graphs). %s%s: real count %d" % [NULLS[clampi(model, 0, 2)][1], n_null, mn.substr(0, 1).to_upper(), mn.substr(1), int(m.get("count", 0))]
	var zv: Variant = m.get("z", null)
	if not bool(m.get("z_defined", zv != null)) or zv == null:
		return head + ". The null never varies for this class, so z is not defined."
	var mean := ", null mean %.1f, sd %.1f, z %+.1f." % [float(m.get("null_mean", 0.0)), float(m.get("null_sd", 0.0)), float(zv)]
	var verdict := "about as often as chance"
	if bool(m.get("significant", false)): verdict = "more often than chance" if float(zv) > 0.0 else "less often than chance"
	var s := head + mean + " This class occurs " + verdict + " under this null."
	if degenerate: s += " The null is too weak to rely on (under a fifth of the swaps were possible)."
	return s

## Histogram of the null counts: {lo, hi, bins (PackedInt32Array), n}. The range always holds `count`, so the real value can be marked on it.
static func hist(null_counts: Array, count: int, max_bins := 24) -> Dictionary:
	var lo := count; var hi := count
	for v in null_counts: lo = mini(lo, int(v)); hi = maxi(hi, int(v))
	var nb := clampi(hi - lo + 1, 1, max_bins)
	var bins := PackedInt32Array(); bins.resize(nb)
	for v in null_counts: bins[mini(nb - 1, int(float(int(v) - lo) / float(hi - lo + 1) * float(nb)))] += 1
	return {"lo": lo, "hi": hi, "bins": bins, "n": null_counts.size()}

# ====================================================================================================================== drawing
## The matrix with names along both edges, thin rules at type boundaries, the shortest-path outlines, and hover / tap.
class View extends Control:
	var wiring: Wiring = null
	var lay := {}
	var rend := {}
	var tex: ImageTexture = null
	var rects: Array = []
	var message := ""
	var on_caption := Callable()    # (String)
	var on_pick := Callable()       # (population id)
	var hover := Vector2i(-1, -1)
	var _ml := 0.0
	const LEGEND := "Rows send, columns receive. Sienna excites (solid), blue inhibits (split or barred). Darker is stronger. Yellow outline: on a shortest path."
	var _mt := 0.0
	var _o := Vector2.ZERO
	var _s := 1.0
	func _ready() -> void:
		clip_contents = true; custom_minimum_size = Vector2(240, 200); mouse_filter = Control.MOUSE_FILTER_STOP
		texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	func min_text_px() -> float: return float(Kit.fs(14)) * Kit.ppl
	func set_model(w_: Wiring, lay_: Dictionary, rend_: Dictionary) -> void:
		wiring = w_; lay = lay_; rend = rend_; tex = ImageTexture.create_from_image(rend_["img"]); hover = Vector2i(-1, -1); queue_redraw()
	func _geometry() -> void:
		var fz := Kit.fs(16 if size.y > 450.0 else 14)
		_ml = minf(size.x * 0.28, float(fz) * (6.5 if size.y > 450.0 else 4.2)); _mt = minf(size.y * 0.28, float(fz) * (6.0 if size.y > 450.0 else 4.0))
		_s = maxf(minf(size.x - _ml - 18.0, size.y - _mt - 18.0), 40.0)
		_o = Vector2(_ml + 8.0, _mt + 8.0)
	func _cell_at(p: Vector2) -> Vector2i:
		var m: int = int(lay.get("n", 0))
		if m == 0: return Vector2i(-1, -1)
		var q := (p - _o) / _s
		if q.x < 0.0 or q.y < 0.0 or q.x >= 1.0 or q.y >= 1.0: return Vector2i(-1, -1)
		return Vector2i(int(q.x * m), int(q.y * m))   # (column, row)
	func _span_at(idx: int) -> Dictionary:
		for sp in lay.get("spans", []):
			if idx >= int(sp["a"]) and idx < int(sp["b"]): return sp
		return {}
	func _gui_input(e: InputEvent) -> void:
		if wiring == null or lay.is_empty(): return
		if e is InputEventMouseMotion:
			var c := _cell_at((e as InputEventMouseMotion).position)
			if c != hover:
				hover = c; queue_redraw()
				if on_caption.is_valid(): on_caption.call(wiring.describe(lay, c.y, c.x) if c.x >= 0 else "")
		elif e is InputEventMouseButton and (e as InputEventMouseButton).pressed and (e as InputEventMouseButton).button_index == MOUSE_BUTTON_LEFT:
			var p := (e as InputEventMouseButton).position
			var m: int = int(lay.get("n", 0))
			var c := _cell_at(p)
			if c.x >= 0:
				hover = c; queue_redraw()
				if on_caption.is_valid(): on_caption.call(wiring.describe(lay, c.y, c.x))
			elif m > 0 and p.x < _o.x and p.y >= _o.y and p.y < _o.y + _s: _pick(int((p.y - _o.y) / _s * m))   # a row name
			elif m > 0 and p.y < _o.y and p.x >= _o.x and p.x < _o.x + _s: _pick(int((p.x - _o.x) / _s * m))   # a column name
	func _pick(idx: int) -> void:
		var sp := _span_at(idx)
		if not sp.is_empty() and on_pick.is_valid(): on_pick.call(str(sp["id"]))
	func _notification(what: int) -> void:
		if what == NOTIFICATION_RESIZED and size.y > 60.0:   # as wide as it is tall: the matrix is square, the reading column takes the rest
			var want := size.y + (minf(size.y * 0.28, float(Kit.fs(16)) * 4.2) if size.y < 450.0 else minf(size.y * 0.28, float(Kit.fs(16)) * 6.5)) + 10.0
			if absf(custom_minimum_size.x - want) > 2.0: custom_minimum_size.x = want
		if what == NOTIFICATION_MOUSE_EXIT and hover.x >= 0:
			hover = Vector2i(-1, -1); queue_redraw()
			if on_caption.is_valid(): on_caption.call("")
	func _draw() -> void:
		var fz := Kit.fs(16 if size.y > 450.0 else 14); var f := Fonts.regular
		Kit.draw_plate(self, Rect2(Vector2.ZERO, size - Vector2(Kit.SHADOW, Kit.SHADOW)))
		if message != "" or wiring == null or lay.is_empty() or tex == null:
			draw_multiline_string(f, Vector2(16, 40), message if message != "" else "computing...", HORIZONTAL_ALIGNMENT_LEFT, size.x - 32, Kit.fs(18), -1, Kit.TEXT_SOFT); return
		_geometry()
		var m: int = int(lay["n"]); var unit := _s / float(m)
		draw_texture_rect(tex, Rect2(_o, Vector2(_s, _s)), false)
		var level := int(lay["level"])
		# thin rules at type boundaries, frame
		for sp in lay["spans"]:
			var a := int(sp["a"])
			if a > 0:
				draw_line(_o + Vector2(a * unit, 0), _o + Vector2(a * unit, _s), Color(Kit.TEXT, 0.45), 1.0)
				draw_line(_o + Vector2(0, a * unit), _o + Vector2(_s, a * unit), Color(Kit.TEXT, 0.45), 1.0)
		draw_rect(Rect2(_o, Vector2(_s, _s)), Kit.TEXT, false, 1.5)
		# the cell-type level: count and the second (shape) code on every mark
		if level == 1 and unit >= 30.0:
			for r in m:
				for c in m:
					var v := wiring.at(lay, r, c)
					if int(v.x) == 0: continue
					var wmax := float(rend.get("wmax", 1.0)); var col := Wiring.mark_color(v.y, wmax)
					var ink := Kit.TEXT if Kit.contrast(Kit.TEXT, col) >= Kit.contrast(Kit.PANEL, col) else Kit.PANEL
					var c0 := _o + Vector2(c * unit, r * unit)
					draw_string(Fonts.bold, c0 + Vector2(0, unit * 0.5 + float(fz) * 0.35), str(int(v.x)), HORIZONTAL_ALIGNMENT_CENTER, unit, fz, ink)
					var gs := unit * 0.16
					if v.y > 0.0: Gfx.arrow_head(self, c0 + Vector2(unit * 0.5 + gs * 1.5, unit * 0.17), Vector2.RIGHT, ink, gs * 1.4)
					else: Gfx.bar_head(self, c0 + Vector2(unit * 0.5 + gs, unit * 0.17), Vector2.RIGHT, ink, gs * 1.4, gs * 1.6)
		# the cells on a shortest path
		for rc in rects:
			var rr := Rect2(_o + Vector2(rc.position.x, rc.position.y) * unit, Vector2(rc.size.x, rc.size.y) * unit)
			if rr.size.x < 6.0 or rr.size.y < 6.0: rr = rr.grow(3.0 - minf(rr.size.x, rr.size.y) * 0.5)
			draw_rect(rr, Kit.GAMBOGE, false, 2.5)
		# the mark under the pointer
		if hover.x >= 0:
			var hr := Rect2(_o + Vector2(hover.x, hover.y) * unit, Vector2(unit, unit)).grow(maxf(1.0, 3.0 - unit * 0.5))
			draw_rect(hr, Kit.TEXT, false, 2.0)
			draw_rect(Rect2(_o + Vector2(0, hover.y * unit), Vector2(_s, unit)), Color(Kit.TEXT, 0.08))
			draw_rect(Rect2(_o + Vector2(hover.x * unit, 0), Vector2(unit, _s)), Color(Kit.TEXT, 0.08))
		# names along both edges (a name that would overlap the last one is left out; tapping its strip still works)
		var last_y := -1.0e9; var last_x := -1.0e9
		for sp in lay["spans"]:
			var mid := (float(sp["a"]) + float(sp["b"])) * 0.5 * unit; var nm := str(sp["id"])
			var span_px := float(int(sp["b"]) - int(sp["a"])) * unit
			if span_px < float(fz) * 0.8: continue
			if _o.y + mid - float(fz) * 0.5 > last_y:
				draw_string(f, Vector2(4, _o.y + mid + float(fz) * 0.35), nm, HORIZONTAL_ALIGNMENT_RIGHT, _o.x - 10.0, fz, Kit.TEXT); last_y = _o.y + mid + float(fz) * 0.5
			if _o.x + mid - float(fz) * 0.5 > last_x:
				draw_set_transform(Vector2(_o.x + mid + float(fz) * 0.35, _o.y - 6.0), -PI / 2.0, Vector2.ONE)
				draw_string(f, Vector2.ZERO, nm, HORIZONTAL_ALIGNMENT_LEFT, _o.y - 10.0, fz, Kit.TEXT)
				draw_set_transform(Vector2.ZERO, 0.0, Vector2.ONE); last_x = _o.x + mid + float(fz) * 0.5

## The null distribution of one motif class as a histogram strip, the real count marked on it, and the verdict in words.
class Strip extends Control:
	var m := {}
	var counts: Array = []
	var model := 1
	var n_null := 0
	var degenerate := false
	var message := ""
	func _ready() -> void: clip_contents = true
	func min_text_px() -> float: return float(Kit.fs(14)) * Kit.ppl
	func _draw() -> void:
		var fz := Kit.fs(16 if size.x > 600.0 else 14); var f := Fonts.regular; var fb := Fonts.bold
		Kit.draw_plate(self, Rect2(Vector2.ZERO, size - Vector2(Kit.SHADOW, Kit.SHADOW)))
		Kit.draw_fig(self, Vector2(18, 28), "1b", "Null distribution: " + str(Wiring.NULLS[model][1]), size.x - 44, "", 17)
		if message != "" or m.is_empty():
			draw_multiline_string(f, Vector2(16, 58), message if message != "" else "computing...", HORIZONTAL_ALIGNMENT_LEFT, size.x - 32, fz, -1, Kit.TEXT_SOFT); return
		var count := int(m.get("count", 0))
		var txt := Wiring.null_text(m, model, n_null, degenerate)
		var th := f.get_multiline_string_size(txt, HORIZONTAL_ALIGNMENT_LEFT, size.x - 32.0, fz).y
		var gx := 18.0; var gw := size.x - 40.0; var gy := 66.0; var gh := maxf(size.y - gy - th - 28.0, 30.0)
		draw_line(Vector2(gx, gy + gh), Vector2(gx + gw, gy + gh), Kit.TEXT, 1.5)
		if counts.is_empty():
			draw_string(f, Vector2(gx, gy + gh * 0.5), "No histogram for one sign of edges at the cell level.", HORIZONTAL_ALIGNMENT_LEFT, gw, fz, Kit.TEXT_SOFT)
		else:
			var h := Wiring.hist(counts, count); var bins: PackedInt32Array = h["bins"]; var nb := bins.size()
			var top := 1
			for v in bins: top = maxi(top, v)
			var bw := gw / float(nb); var lo := float(h["lo"]); var span := float(int(h["hi"]) - int(h["lo"]) + 1)
			for i in nb:
				var bh := gh * float(bins[i]) / float(top)
				draw_rect(Rect2(gx + float(i) * bw + 1.0, gy + gh - bh, maxf(bw - 2.0, 1.0), bh), Color(Kit.TEXT_SOFT, 0.55))
			var mx := gx + gw * (float(m.get("null_mean", 0.0)) - lo + 0.5) / span
			if mx >= gx and mx <= gx + gw: Gfx.dashed(self, PackedVector2Array([Vector2(mx, gy), Vector2(mx, gy + gh)]), Kit.TEXT_SOFT, 1.5)
			var rx := gx + gw * (float(count) - lo + 0.5) / span
			draw_line(Vector2(rx, gy - 4.0), Vector2(rx, gy + gh), Kit.exc(), 3.0)
			draw_string(fb, Vector2(clampf(rx - 30.0, gx, gx + gw - 60.0), gy - 10.0), "real", HORIZONTAL_ALIGNMENT_CENTER, 60.0, fz, Kit.EXC_INK)
			draw_string(f, Vector2(gx, gy + gh + float(fz) + 2.0), str(int(h["lo"])), HORIZONTAL_ALIGNMENT_LEFT, 80.0, fz - 2, Kit.TEXT_SOFT)
			draw_string(f, Vector2(gx + gw - 80.0, gy + gh + float(fz) + 2.0), str(int(h["hi"])), HORIZONTAL_ALIGNMENT_RIGHT, 80.0, fz - 2, Kit.TEXT_SOFT)
		draw_multiline_string(f, Vector2(16, size.y - th - 4.0 + float(fz)), txt, HORIZONTAL_ALIGNMENT_LEFT, size.x - 32.0, fz, -1, Kit.TEXT)
