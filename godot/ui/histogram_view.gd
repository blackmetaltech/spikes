class_name HistogramView
extends VBoxContainer
## One post-solve histogram (docs/DESIGN-0.3.md section 6, Zachtronics style): the distribution of an axis over other solutions of the same task, your point marked,
## the median drawn, and a "pin a target" row for personal goals. Three of these make the post-solve card: neurons, energy (spikes/s, the ATP bill) and the
## task's own quality metric.
##
## Distributions come from catalog/histograms/<task>.json (written offline by the solution sweeps; later joined by opt-in uploads). Shape:
##   {"task": "reflex", "metrics": {"<name>": {"unit": "spikes/s", "lower_is_better": true,
##                                              "values": [3, 4, 4, 5, ...]}            raw samples, or
##                                              "lo": 0, "hi": 100, "counts": [..]}}    pre-binned (equal-width bins from lo to hi)
## `<name>` is a Task::metrics() key ("neurons", "energy_hz" = mean spikes per second per neuron, and the task's quality metric). A missing file or metric shows
## a placeholder message instead of a distribution; the player's own point and the pin row work regardless.

const GOALS_PATH := "user://goals.cfg"
const BINS := 14

signal goal_changed

var metric := ""
var label := ""
var unit := ""
var value := 0.0
var lower_better := true
var dist := {}                 # normalised distribution, {} when there is none
var goal := NAN                # the pinned personal target (NAN = none)
var task_id := ""
var _plot: _Plot
var _pin_row: HBoxContainer

# ---------- data ----------
## The task's distributions file as {metric: raw dictionary}; {} when it is not there.
static func load_file(task: String) -> Dictionary:
	for base in [ProjectSettings.globalize_path("res://").path_join("../catalog"), ProjectSettings.globalize_path("res://catalog"), OS.get_executable_path().get_base_dir().path_join("catalog")]:
		var p := String(base).path_join("histograms").path_join(task + ".json")
		if FileAccess.file_exists(p):
			var j: Variant = JSON.parse_string(FileAccess.get_file_as_string(p))
			if j is Dictionary and (j as Dictionary).get("metrics", null) is Dictionary: return (j as Dictionary)["metrics"]
	return {}

## {lo, hi, counts, n, median, integer} from a raw metric entry (samples or pre-binned counts); {} when it holds nothing usable.
static func normalise(m: Dictionary) -> Dictionary:
	var counts := []; var lo := 0.0; var hi := 1.0; var n := 0; var median := NAN; var integer := false
	if m.get("values", null) is Array and not (m["values"] as Array).is_empty():
		var vals: Array = (m["values"] as Array).map(func(x): return float(x)); vals.sort()
		lo = float(vals[0]); hi = float(vals[vals.size() - 1]); n = vals.size()
		median = float(vals[n / 2]) if n % 2 == 1 else 0.5 * (float(vals[n / 2 - 1]) + float(vals[n / 2]))
		integer = vals.all(func(x): return is_equal_approx(x, roundf(x)))
		var nb := BINS
		if integer and hi - lo <= 24.0: nb = int(hi - lo) + 1; lo -= 0.5; hi += 0.5
		elif hi - lo < 1e-9: lo -= 0.5; hi += 0.5
		counts.resize(nb); counts.fill(0)
		for x in vals: counts[clampi(int((float(x) - lo) / (hi - lo) * float(nb)), 0, nb - 1)] += 1
	elif m.get("counts", null) is Array and not (m["counts"] as Array).is_empty():
		counts = (m["counts"] as Array).map(func(x): return int(x)); lo = float(m.get("lo", 0.0)); hi = float(m.get("hi", float(counts.size())))
		for c in counts: n += int(c)
		if n <= 0: return {}
		var acc := 0; var w := (hi - lo) / float(counts.size())
		for i in counts.size():
			if acc + int(counts[i]) >= float(n) * 0.5: median = lo + w * (float(i) + (float(n) * 0.5 - float(acc)) / maxf(float(counts[i]), 1.0)); break
			acc += int(counts[i])
	else: return {}
	return {"lo": lo, "hi": hi, "counts": counts, "n": n, "median": median, "integer": integer}

## Share (0..1) of the distribution that you beat with `v` (strictly better; a tie counts half).
static func beats(d: Dictionary, v: float, lower_is_better: bool) -> float:
	if d.is_empty(): return NAN
	var counts: Array = d["counts"]; var lo := float(d["lo"]); var w := (float(d["hi"]) - lo) / float(counts.size()); var n := float(d["n"]); var worse := 0.0
	for i in counts.size():
		var a := lo + w * float(i); var b := a + w; var c := float(counts[i])
		var frac := 0.0   # share of this bin that is worse than v
		if lower_is_better: frac = clampf((b - v) / w, 0.0, 1.0)
		else: frac = clampf((v - a) / w, 0.0, 1.0)
		worse += c * frac
	return worse / maxf(n, 1.0)

static func beats_median(d: Dictionary, v: float, lower_is_better: bool) -> bool:
	if d.is_empty() or is_nan(float(d["median"])): return false
	return v < float(d["median"]) - 1e-9 if lower_is_better else v > float(d["median"]) + 1e-9

static func fmt(v: float) -> String:
	if absf(v) >= 100.0 or is_equal_approx(v, roundf(v)): return "%d" % int(roundf(v))
	return "%.1f" % v if absf(v) >= 1.0 else "%.2f" % v

# ---------- personal goals ----------
static var _goals := {}
static var _goals_loaded := false
static func _gload() -> void:
	if _goals_loaded: return
	_goals_loaded = true
	var c := ConfigFile.new()
	if c.load(GOALS_PATH) != OK: return
	for k in c.get_section_keys("goals"): _goals[k] = float(c.get_value("goals", k))
static func goal_of(task: String, m: String) -> float: _gload(); return float(_goals.get(task + ":" + m, NAN))
static func set_goal(task: String, m: String, v: float) -> void:
	_gload()
	if is_nan(v): _goals.erase(task + ":" + m)
	else: _goals[task + ":" + m] = v
	if Settings.no_save or OS.has_environment("SP_NOAUTOSAVE"): return
	var c := ConfigFile.new()
	for k in _goals: c.set_value("goals", k, _goals[k])
	c.save(GOALS_PATH)

# ---------- view ----------
## `raw`: the metric's entry of the distributions file ({} = none). unit / lower_is_better fall back to the file's, then to `unit_in` / `lower_in`.
func setup(task: String, metric_id: String, title: String, v: float, unit_in: String, lower_in: bool, raw: Dictionary) -> HistogramView:
	task_id = task; metric = metric_id; label = title; value = v
	unit = str(raw.get("unit", unit_in)) if not raw.is_empty() else unit_in
	lower_better = bool(raw.get("lower_is_better", lower_in)) if not raw.is_empty() else lower_in
	dist = normalise(raw) if not raw.is_empty() else {}
	goal = goal_of(task, metric_id)
	add_theme_constant_override("separation", 4)
	_plot = _Plot.new(); _plot.hv = self; _plot.custom_minimum_size = Vector2(240, 124 if Layout.phone else 132); add_child(_plot)
	_pin_row = HBoxContainer.new(); _pin_row.add_theme_constant_override("separation", 8); add_child(_pin_row)
	_build_pin()
	return self

func _step() -> float:
	if metric == "neurons": return 1.0
	if not dist.is_empty(): return maxf((float(dist["hi"]) - float(dist["lo"])) / 40.0, 0.01)
	return maxf(absf(value) * 0.05, 0.1)

func _build_pin() -> void:
	for c in _pin_row.get_children(): _pin_row.remove_child(c); c.queue_free()
	if is_nan(goal):
		_pin_row.add_child(Kit.btn("PIN A TARGET", func():
			var st := _step()
			goal = value - st if lower_better else value + st   # one step better than you are
			if not dist.is_empty() and not is_nan(float(dist["median"])) and not beats_median(dist, value, lower_better): goal = float(dist["median"]) - (st if lower_better else -st)
			if metric == "neurons": goal = maxf(1.0, roundf(goal))
			set_goal(task_id, metric, goal); _build_pin(); _plot.queue_redraw(); goal_changed.emit(), "ghost", 16))
	else:
		var reached := value <= goal + 1e-9 if lower_better else value >= goal - 1e-9
		_pin_row.add_child(Kit.btn("-", func(): _nudge(-1.0), "panel", 16, Vector2(40, 0)))
		var l := Kit.label("target %s %s" % [fmt(goal), unit], 16, Kit.GREEN if reached else Kit.CYAN); l.size_flags_vertical = Control.SIZE_SHRINK_CENTER; _pin_row.add_child(l)
		_pin_row.add_child(Kit.btn("+", func(): _nudge(1.0), "panel", 16, Vector2(40, 0)))
		var st := Kit.label("reached" if reached else "%s %s to go" % [fmt(absf(value - goal)), unit], 16, Kit.GREEN if reached else Kit.TEXT_SOFT); st.size_flags_vertical = Control.SIZE_SHRINK_CENTER; st.size_flags_horizontal = Control.SIZE_EXPAND_FILL; _pin_row.add_child(st)
		_pin_row.add_child(Kit.btn("UNPIN", func(): goal = NAN; set_goal(task_id, metric, NAN); _build_pin(); _plot.queue_redraw(); goal_changed.emit(), "ghost", 16))

func _nudge(dir: float) -> void:
	goal += dir * _step()
	if metric == "neurons": goal = maxf(1.0, roundf(goal))
	set_goal(task_id, metric, goal); _build_pin(); _plot.queue_redraw(); goal_changed.emit()

class _Plot extends Control:
	var hv: HistogramView
	func _draw() -> void:
		var f := Fonts.regular; var fb := Fonts.bold; var fz := Kit.fs(14)
		draw_style_box(Kit.sb(Gfx.SHEET, Color(Kit.INK, 0.5), 1, 2, 0, false), Rect2(Vector2.ZERO, size))
		var d := hv.dist; var v := hv.value
		var head := hv.label.to_upper()
		var yours := "you: %s %s" % [HistogramView.fmt(v), hv.unit]
		var yw := minf(fb.get_string_size(yours, HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x, size.x * 0.68)
		draw_string(Gfx.serif_italic(), Vector2(10, float(fz) + 4.0), hv.label, HORIZONTAL_ALIGNMENT_LEFT, size.x - yw - 28.0, Kit.fs(15), Kit.TEXT)
		draw_string(fb, Vector2(size.x - 10.0 - yw, float(fz) + 4.0), yours, HORIZONTAL_ALIGNMENT_RIGHT, yw, fz, Kit.INK)
		var pl := Rect2(14, float(fz) * 2.0 + 10.0, size.x - 28.0, size.y - float(fz) * 3.6 - 16.0)
		if d.is_empty():
			draw_multiline_string(f, Vector2(14, pl.position.y + float(fz) + 4.0), "No distribution for this task yet: the offline solution sweep has not been run. Your point and your own targets still count.", HORIZONTAL_ALIGNMENT_LEFT, size.x - 28.0, fz, 4, Kit.TEXT_SOFT)
			return
		var counts: Array = d["counts"]; var lo := float(d["lo"]); var hi := float(d["hi"]); var nb := counts.size(); var mx := 1
		for c in counts: mx = maxi(mx, int(c))
		var span := hi - lo
		# the player's value and any target may lie outside the samples: widen the axis, keep the bars where they are
		var a_lo := lo; var a_hi := hi
		for x in [v, hv.goal]:
			if not is_nan(float(x)): a_lo = minf(a_lo, float(x) - span * 0.02); a_hi = maxf(a_hi, float(x) + span * 0.02)
		var X := func(val: float) -> float: return pl.position.x + (val - a_lo) / maxf(a_hi - a_lo, 1e-9) * pl.size.x
		var w := span / float(nb)
		var med := float(d["median"])
		for i in nb:
			var h := pl.size.y * float(counts[i]) / float(mx)
			var x0: float = X.call(lo + w * float(i)); var x1: float = X.call(lo + w * float(i + 1))
			var mid := lo + w * (float(i) + 0.5)
			var better := (mid < v) if hv.lower_better else (mid > v)   # bars worse than you are dim, bars better than you are bright
			var br := Rect2(x0 + 1.0, pl.end.y - h, maxf(x1 - x0 - 2.0, 1.0), h)   # an engraved bar: hatched in its stain, ruled in ink
			if h > 0.5:
				var bc := Kit.inh() if better else Kit.exc()
				draw_rect(br, Color(bc, 0.10)); Gfx.hatch(self, PackedVector2Array([br.position, Vector2(br.end.x, br.position.y), br.end, Vector2(br.position.x, br.end.y)]), -PI * 0.25 if better else PI * 0.25, 3.5 if not better else 5.0, Color(bc, 0.9), 1.0)
				draw_rect(br, Color(Kit.INK, 0.8), false, 1.0)
		draw_line(Vector2(pl.position.x, pl.end.y), Vector2(pl.end.x, pl.end.y), Kit.INK, 1.4)
		if not is_nan(med):
			var mx_: float = X.call(med); Gfx.dashed(self, PackedVector2Array([Vector2(mx_, pl.position.y), Vector2(mx_, pl.end.y)]), Color(Kit.TEXT, 0.7), 1.5, 5.0, 4.0)
			var mt := "median " + HistogramView.fmt(med); var mw := f.get_string_size(mt, HORIZONTAL_ALIGNMENT_LEFT, -1, fz).x + 8.0
			var mxl := mx_ + 3.0 if mx_ + 3.0 + mw < size.x - 4.0 else mx_ - 3.0 - mw
			draw_rect(Rect2(mxl, pl.position.y - 2.0, mw, float(fz) + 4.0), Color(Gfx.SHEET, 0.92))
			draw_string(f, Vector2(mxl + 4.0, pl.position.y + float(fz) - 2.0), mt, HORIZONTAL_ALIGNMENT_LEFT, mw, fz, Kit.TEXT)
		if not is_nan(hv.goal):
			var gx: float = X.call(hv.goal); Gfx.dashed(self, PackedVector2Array([Vector2(gx, pl.position.y), Vector2(gx, pl.end.y)]), Kit.CYAN, 2.0, 6.0, 4.0)
		var px: float = X.call(v)
		draw_line(Vector2(px, pl.position.y), Vector2(px, pl.end.y), Kit.INK, 2.2, true)   # you: an inked pointer
		draw_colored_polygon(PackedVector2Array([Vector2(px - 7.0, pl.end.y + 9.0), Vector2(px + 7.0, pl.end.y + 9.0), Vector2(px, pl.end.y)]), Kit.INK)
		var b := HistogramView.beats(d, v, hv.lower_better)
		var txt := "beats %d%%" % int(roundf(b * 100.0)) if size.x < 330.0 else "better than %d%% of %d solutions" % [int(roundf(b * 100.0)), int(d["n"])]
		if HistogramView.beats_median(d, v, hv.lower_better): txt += "  ·  beats median" if size.x < 330.0 else "  ·  beats the median"
		draw_string(f, Vector2(10, size.y - 6.0), txt, HORIZONTAL_ALIGNMENT_LEFT, size.x - 20.0, fz, Kit.GREEN if HistogramView.beats_median(d, v, hv.lower_better) else Kit.TEXT_SOFT)
