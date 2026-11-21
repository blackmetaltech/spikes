extends SceneTree
## Headless checks of the lab chrome's pure logic (no GDExtension needed):
##   godot --headless --path godot -s tests/test_polish.gd        prints "POLISH OK" or "POLISH FAIL: ..." (exit 1)
## time-axis labels never repeat; score-part labels never show a raw key; latch self-loops and scheduled stimuli are never "a rhythm by accident".

var fails := 0

func _check(ok: bool, msg: String) -> void:
	if not ok: fails += 1; print("POLISH FAIL: ", msg)

## Every engine score-part key the review saw on screen, plus the generated ones (r_meanfield's net_hz_l1, the bestiary windows <ch>_<win>_hz).
const KEYS := ["criteria_met", "criteria", "net_hz_l1", "mf_hz_l3", "forward_e4_hz", "reverse_base_hz", "mae_bpm", "latency_ms", "false_triggers",
	"burst_threshold", "worst_reach_ms", "map_min_mmHg", "ve_rest_l_min", "agreement_pct", "osc_rad", "spikes_turn_left", "seg2_ratio", "fire_ok",
	"silent_ok", "conditions_met", "b1_latency_ms_trial", "gap12_ms_trial", "eff_iv_ms", "rate_cv", "hr_bpm", "kc_spikes", "run_value"]

func _init() -> void:
	# ticks: labels from one window never repeat (playtest 0.5 item, ui finding 4, gameplay 15)
	for step in [10.0, 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0]:
		for mult in [1, 2, 3]:
			var st: float = step * mult; var seen := {}
			var t := -2000.0
			while t <= 30000.0:
				var s := TraceView.tick_text(t, st)
				_check(not seen.has(s), "tick label %s repeats at step %s ms" % [s, st]); seen[s] = true
				t += st
	_check(TraceView.tick_text(1250.0, 50.0) == "1.25 s", "50 ms ticks keep two decimals (%s)" % TraceView.tick_text(1250.0, 50.0))
	_check(TraceView.tick_text(3000.0, 1000.0) == "3 s", "whole seconds have none")
	_check(TraceView.tick_text(-0.001, 100.0) == "0.0 s", "no negative zero")
	# human labels for score parts
	for k in KEYS:
		var h := ScoreCard.human(k); var lab := str(h[0])
		_check(not lab.contains("_"), "%s -> '%s' keeps an underscore" % [k, lab])
		_check(not (" " + lab + " ").contains(" hz "), "%s -> '%s' has a bare hz" % [k, lab])
		_check(not (" " + lab + " ").contains(" l1 "), "%s -> '%s' has a raw l1" % [k, lab])
		_check(lab != k, "%s is shown as its raw key" % k)
		_check(lab != "" , "%s has an empty label" % k)
	_check(str(ScoreCard.human("net_hz_l1")[1]) == "Hz", "net_hz_l1 keeps its unit")
	_check(str(ScoreCard.human("criteria")[0]) == "conditions", "criteria reads as conditions")
	# surprise: a latch (excitatory self-loop) is no rhythm loop; a two-population loop or self-inhibition is
	var latch := {"populations": [{"id": "AVA", "n": 2}, {"id": "AVD", "n": 2}], "projections": [{"pre": "AVA", "post": "AVA", "synapse": {"weight": 300.0}}, {"pre": "AVD", "post": "AVD", "synapse": {"weight": 300.0}}, {"pre": "AVD", "post": "AVA", "synapse": {"weight": 200.0}}]}
	_check(not Surprise.has_rhythm_loop(latch), "latch self-loops count as a rhythm loop")
	var hc := {"populations": [{"id": "A", "n": 2}, {"id": "B", "n": 2}], "projections": [{"pre": "A", "post": "B", "synapse": {"weight": -300.0}}, {"pre": "B", "post": "A", "synapse": {"weight": -300.0}}]}
	_check(Surprise.has_rhythm_loop(hc), "a half-centre is a rhythm loop")
	var si := {"populations": [{"id": "A", "n": 4}], "projections": [{"pre": "A", "post": "A", "synapse": {"weight": -50.0}}]}
	_check(Surprise.has_rhythm_loop(si), "self-inhibition is a rhythm loop")
	# the worm_touch reference (the review's evidence: lab_wormtouch.png): its self-loops are latches; its bursts follow the scheduled touches
	var wt: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://circuits/worm_touch.json"))
	var no_latch := wt.duplicate(true); no_latch["projections"] = (wt["projections"] as Array).filter(func(pr): return str(pr["pre"]) != "AVA" or str(pr["post"]) != "AVB")
	no_latch["projections"] = (no_latch["projections"] as Array).filter(func(pr): return str(pr["pre"]) != "AVB" or str(pr["post"]) != "AVA")
	_check(not Surprise.has_rhythm_loop(no_latch), "worm_touch's AVA/AVB latches alone count as a rhythm loop")
	var tele := Telemetry.new()
	var touches := [{"t_ms": 500.0, "dur_ms": 300.0}, {"t_ms": 1700.0, "dur_ms": 300.0}, {"t_ms": 3100.0, "dur_ms": 300.0}]
	for e in touches:
		for k in 12: tele.sp_t.append(float(e["t_ms"]) + 20.0 * k); tele.sp_n.append(k % 4)
	tele.sp_t.append(1300.0); tele.sp_n.append(0)
	_check(Surprise.stimulus_locked(touches, tele, 0.0, 4000.0), "bursts inside the scheduled touches are not locked to them")
	var free := Telemetry.new()
	for k in 40: free.sp_t.append(100.0 * k); free.sp_n.append(0)
	_check(not Surprise.stimulus_locked([{"t_ms": 500.0, "dur_ms": 50.0}, {"t_ms": 2500.0, "dur_ms": 50.0}], free, 0.0, 4000.0), "a free-running rhythm is called stimulus-locked")
	# toast text quotes no digits
	var tx := Surprise.oscillation_text({"period_ms": 250.0, "pop": "AVA"})
	var re := RegEx.new(); re.compile("[0-9]")
	_check(re.search(str(tx["text"]) + str(tx["title"])) == null, "the rhythm toast quotes digits: %s" % tx["text"])
	print("POLISH OK" if fails == 0 else "POLISH FAILED: %d" % fails)
	quit(1 if fails > 0 else 0)
