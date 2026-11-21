class_name CircuitClip
extends RefCounted
## Pieces of a circuit for the bench's clipboard (Ctrl+C / Ctrl+V / Ctrl+D) and for More > Import from a passed level: a set of populations with the
## projections between them, their E / I marks and neuromodulator rows, and (for an import) their wiring to sensor and motor channels by name.
## Pasting renames ids that are taken and drops links to channels the circuit does not have (they are reported, never invented).

const KIND := "spike-fragment-1"
static var clip := {}         # the bench clipboard: one fragment, shared by every lab visit of the session
static var text := ""          # the last "Copy circuit as JSON" (used when the system clipboard is unavailable, as in a headless run)

## The populations `ids` of `c` as a fragment (deep copies). `channels`: also every link from an input channel to them and from them to an output channel.
static func fragment(c: Dictionary, ids: Array, channels := false) -> Dictionary:
	var keep := {}
	for id in ids: keep[str(id)] = true
	var pops := []; var lo := Vector2(INF, INF); var hi := Vector2(-INF, -INF)
	for p in c.get("populations", []):
		if not keep.has(str(p.get("id", ""))): continue
		pops.append((p as Dictionary).duplicate(true))
		for q in p.get("pos", []):
			if q is Array and (q as Array).size() >= 2:
				var v := Vector2(float(q[0]), float(q[1])); lo = lo.min(v); hi = hi.max(v)
	var projs := []
	for pr in c.get("projections", []):
		if keep.has(Circuits.proj_pop(pr.get("pre", ""))) and keep.has(Circuits.proj_pop(pr.get("post", ""))): projs.append((pr as Dictionary).duplicate(true))
	var ei := {}; var src_ei: Variant = ((c.get("meta", {}) as Dictionary).get("editor", {}) as Dictionary).get("ei", {}) if (c.get("meta", {}) as Dictionary).get("editor", {}) is Dictionary else {}
	if src_ei is Dictionary:
		for id in keep: if (src_ei as Dictionary).has(id): ei[id] = str(src_ei[id])
	var mods := []
	for row in c.get("modulation", []):
		if row is Dictionary and keep.has(Circuits.proj_pop((row as Dictionary).get("pop", ""))): mods.append((row as Dictionary).duplicate(true))
	var ins := []; var outs := []
	if channels:
		for ch in c.get("inputs", []):
			for x in ch.get("targets", []):
				if keep.has(Circuits.proj_pop(x)): ins.append([str(ch.get("name", "")), _dup(x), float(ch.get("weight", 0.0)), str(ch.get("kind", ""))])
		for ch in c.get("outputs", []):
			for x in ch.get("neurons", []):
				if keep.has(Circuits.proj_pop(x)): outs.append([str(ch.get("name", "")), _dup(x)])
	var mid := (lo + hi) * 0.5 if lo.x <= hi.x else Vector2.ZERO
	return {"kind": KIND, "pops": pops, "projections": projs, "ei": ei, "mods": mods, "in_links": ins, "out_links": outs, "center": [mid.x, mid.y]}

static func is_fragment(v: Variant) -> bool: return v is Dictionary and str((v as Dictionary).get("kind", "")) == KIND and (v as Dictionary).get("pops", null) is Array

static func _dup(x: Variant) -> Variant: return (x as Dictionary).duplicate(true) if x is Dictionary else x

static func _rename(x: Variant, ren: Dictionary) -> Variant:
	if x is Dictionary:
		var d: Dictionary = (x as Dictionary).duplicate(true); var old := str(d.get("pop", "")); d["pop"] = ren.get(old, old); return d
	return ren.get(str(x), str(x))

## An id for a pasted population: its own when free in `c`, else "<id>_2", "<id>_3" ...
static func free_id(c: Dictionary, id: String, taken: Dictionary) -> String:
	var cand := id; var n := 2
	while Circuits.pop_index(c, cand) >= 0 or Circuits._channel_exists(c, cand) or taken.has(cand) or cand.length() > 64:
		cand = "%s_%d" % [id.substr(0, 58), n]; n += 1
	return cand

## Paste fragment `f` into `c`, every position shifted by `offset` (model units). Returns {ids (the new population ids, in order), dropped (names of
## channels the fragment was wired to that `c` does not have), linked (channel links made)}.
static func paste(c: Dictionary, f: Dictionary, offset: Vector2) -> Dictionary:
	var ren := {}; var taken := {}; var ids := []
	for p in f.get("pops", []):
		var id := str(p.get("id", "")); var nid := free_id(c, id, taken); ren[id] = nid; taken[nid] = true
	for p in f.get("pops", []):
		var q: Dictionary = (p as Dictionary).duplicate(true); q["id"] = ren[str(p.get("id", ""))]
		var pos: Array = []
		for xy in q.get("pos", []):
			if xy is Array and (xy as Array).size() >= 2: pos.append([float(xy[0]) + offset.x, float(xy[1]) + offset.y])
		q["pos"] = pos
		c["populations"].append(q); ids.append(q["id"])
	var meta: Dictionary = c.get("meta", {}); var ed: Dictionary = meta.get("editor", {}) if meta.get("editor", {}) is Dictionary else {}
	var ei: Dictionary = ed.get("ei", {}) if ed.get("ei", {}) is Dictionary else {}
	for id in f.get("ei", {}): if ren.has(str(id)): ei[ren[str(id)]] = str(f["ei"][id])
	if not ei.is_empty(): ed["ei"] = ei; meta["editor"] = ed; c["meta"] = meta
	if not c.has("projections"): c["projections"] = []
	for pr in f.get("projections", []):
		var q2: Dictionary = (pr as Dictionary).duplicate(true); q2["pre"] = _rename(q2.get("pre", ""), ren); q2["post"] = _rename(q2.get("post", ""), ren)
		c["projections"].append(q2)
	if not (f.get("mods", []) as Array).is_empty():
		var rows: Array = c.get("modulation", []) if c.get("modulation", []) is Array else []
		for row in f["mods"]:
			var r2: Dictionary = (row as Dictionary).duplicate(true); r2["pop"] = _rename(r2.get("pop", ""), ren); rows.append(r2)
		c["modulation"] = rows
	var dropped := {}; var linked := 0
	for l in f.get("in_links", []):
		var ch := _channel(c, "inputs", str(l[0]))
		if ch.is_empty(): dropped[str(l[0])] = true; continue
		var tg: Array = ch.get("targets", [])
		if tg.is_empty() and float(l[2]) > 0.0 and (str(l[3]) == "" or str(l[3]) == str(ch.get("kind", ""))): ch["weight"] = float(l[2])   # the sensor weight the solved level used
		tg.append(_rename(l[1], ren)); ch["targets"] = tg; linked += 1
	for l in f.get("out_links", []):
		var ch2 := _channel(c, "outputs", str(l[0]))
		if ch2.is_empty(): dropped[str(l[0])] = true; continue
		var tg2: Array = ch2.get("neurons", []); tg2.append(_rename(l[1], ren)); ch2["neurons"] = tg2; linked += 1
	var dl := dropped.keys(); dl.sort()
	return {"ids": ids, "dropped": dl, "linked": linked}

static func _channel(c: Dictionary, key: String, name: String) -> Dictionary:
	for ch in c.get(key, []):
		if str(ch.get("name", "")) == name: return ch
	return {}

## The fragment's centre in model units.
static func center(f: Dictionary) -> Vector2:
	var m: Array = f.get("center", [0.0, 0.0])
	return Vector2(float(m[0]), float(m[1])) if m.size() >= 2 else Vector2.ZERO
