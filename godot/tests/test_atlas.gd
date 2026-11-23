extends SceneTree
## Headless checks of the Atlas worm chapter's game side (roadmap A2):
##   godot --headless --path godot -s tests/test_atlas.gd        prints "ATLAS OK" or "ATLAS FAIL: ..." (exit 1)   (scripts/atlas_test.sh, ctest `godot_atlas`)
## Needs the GDExtension (the tasks are the engine's); skips with a message when it is not loaded.
## wing and chapter gating, the glossary, the Analysis screen open in the chapter, the silencing control's rules (circuits.gd), the bet panel (a bet offers no lesion
## lab, commits, shows a wrong bet and the result) beside an ordinary case that is unchanged, and a right and a wrong bet run through the engine.

var fails := 0
var checks := 0

func _check(ok: bool, msg: String) -> void:
	checks += 1
	if not ok: fails += 1; print("ATLAS FAIL: ", msg)

func _init() -> void:
	await process_frame
	if not ClassDB.class_exists("SpikeTask"): print("ATLAS SKIP: the GDExtension is not loaded"); quit(0); return
	_unlocks_fresh()
	_chapter()
	_gating()
	_concepts()
	_silence()
	_panel()
	_bets()
	print("ATLAS OK (%d checks)" % checks if fails == 0 else "ATLAS FAILED: %d of %d" % [fails, checks])
	quit(1 if fails > 0 else 0)

func _unlocks_fresh() -> void:
	_check(LabUnlocks.has("a_motif", "analysis") and LabUnlocks.has("a_path", "analysis") and LabUnlocks.has("dx_a_worm", "analysis"), "the Analysis screen is open in every level of the Atlas worm chapter from the start")
	_check(not LabUnlocks.has("reflex", "analysis"), "Analysis is still closed in the first chapter")

func _chapter() -> void:
	var cd := Campaign.chapter_def("a_worm")
	_check(str(cd.get("title", "")) == "Reading wiring" and str(cd.get("wing", "")) == "atlas", "chapter a_worm is 'Reading wiring' in the atlas wing")
	_check(Campaign.wing_of("a_worm") == "atlas" and Campaign.is_atlas("a_worm") and Campaign.is_wing("a_worm") and not Campaign.is_bestiary("a_worm") and not Campaign.is_theory("a_worm"), "a_worm is a wing chapter of the atlas wing only")
	_check(Campaign.wing_of("worm") == "bestiary" and Campaign.wing_of("dynamics") == "theory" and Campaign.wing_of("heart") == "body", "the other wings are unchanged")
	var wings := TasksScreen.WINGS.map(func(w): return w[0])
	_check(wings == ["body", "bestiary", "theory", "atlas"], "the map has four wing buttons (got %s)" % str(wings))
	_check(TasksScreen.ROW_SUB.has("atlas") and TasksScreen.SHORT.has("a_worm"), "the atlas row and its chapter tab have their labels")
	var ids := []; var spine := []; var parents := {}
	for n in Campaign.nodes("a_worm"):
		if not bool(n["installed"]): continue
		ids.append(str(n["id"])); if bool(n["spine"]): spine.append(str(n["id"]))
		parents[str(n["id"])] = str(n["parent"])
	_check(ids.has("a_path") and ids.has("a_motif") and ids.has("a_predict") and ids.has("dx_a_worm") and ids.size() == 4, "the chapter has its four levels (got %s)" % str(ids))
	_check(spine == ["a_path"], "a_path is the chapter's spine (got %s)" % str(spine))
	for b in ["a_motif", "a_predict", "dx_a_worm"]: _check(parents.get(b, "") == "a_path", "%s hangs off a_path" % b)
	_check(Campaign.is_bet("a_predict") and Campaign.is_bet("a_motif"), "a_predict and a_motif are bets")
	_check(not Campaign.is_bet("dx_a_worm") and not Campaign.is_bet("a_path") and not Campaign.is_bet("reflex"), "the case, the atlas level and an ordinary level are not bets")
	_check(Campaign.is_dev("atlas_probe") and not ids.has("atlas_probe"), "atlas_probe stays development-only")

func _gating() -> void:
	_check(not Campaign.chapter_open("a_worm") and Campaign.chapter_lock_reason("a_worm").begins_with("Pass the"), "the wing is closed before chapter I is passed (%s)" % Campaign.chapter_lock_reason("a_worm"))
	for id in Campaign.spine_ids("invertebrates"): Progress.record(str(id), 100.0, true, 3)
	_check(Campaign.chapter_open("a_worm"), "the wing opens after the chapter I spine, as the Bestiary does")
	_check(Campaign.unlocked("a_path") and not Campaign.unlocked("a_predict") and not Campaign.unlocked("dx_a_worm"), "inside the chapter the branches wait for a_path")
	Progress.record("a_path", 100.0, true, 3)
	_check(Campaign.unlocked("a_predict") and Campaign.unlocked("a_motif") and Campaign.unlocked("dx_a_worm"), "passing a_path opens the three branches")

func _concepts() -> void:
	for c in ["pathway", "prediction", "null_model"]:
		_check(Campaign.concept_blurb(c) != "" and Campaign.concept_title(c) != c, "the glossary explains %s" % c)
		_check(not Campaign.concept_blurb(c).contains("!") and not Campaign.concept_blurb(c).contains("—"), "%s has no exclamation mark or em dash" % c)

func _given(task: String) -> Dictionary:
	return Circuits.parse(Circuits.read_reference(Campaign.ref_path(task)))

func _silence() -> void:
	var c := _given("a_path")
	_check(Circuits.validate(c) == "", "the given a_path circuit validates with its off groups")
	var gs := Circuits.groups(c)
	_check(gs.size() == 5 and gs.all(func(g): return Circuits.group_off(g)), "the five interneuron classes are groups that can be switched off")
	_check(Circuits.group_set_active(c, "AVA", true) == "" and is_equal_approx(float(Circuits.groups(c)[0]["scale"]), 1.0) and not Circuits.pop_silenced(c, "AVA"), "active: scale 1")
	_check(Circuits.group_set_active(c, "AVA", false) == "" and is_equal_approx(float(Circuits.groups(c)[0]["scale"]), 0.0) and Circuits.pop_silenced(c, "AVA"), "silenced: scale 0")
	var out_w := 1.0
	for pr in c["projections"]:
		if Circuits.proj_pop(pr["pre"]) == "AVA": out_w = Circuits.proj_weight_eff(c, pr)
	_check(is_zero_approx(out_w), "a silenced class sends nothing: its outgoing effective weight is 0 (got %s)" % str(out_w))
	_check(Circuits.group_set_scale(c, "AVA", 0.0) == "" and is_equal_approx(float(Circuits.groups(c)[0]["scale"]), 0.0), "an off group may reach scale 0 by the stepper rule")
	var plain := c.duplicate(true); (plain["groups"][0] as Dictionary)["off"] = false; (plain["groups"][0] as Dictionary)["scale"] = 1.0
	_check(Circuits.group_set_active(plain, "AVA", false) != "", "a group that is not off cannot be switched off")
	Circuits.group_set_scale(plain, "AVA", 0.0)
	_check(float(Circuits.groups(plain)[0]["scale"]) >= Circuits.SCALE_STEP - 1e-9, "a group that is not off never reaches scale 0 (got %s)" % str(Circuits.groups(plain)[0]["scale"]))
	var bad := c.duplicate(true); (bad["groups"][0] as Dictionary)["off"] = "yes"
	_check(Circuits.validate(bad) != "", "off must be true or false")
	var rt := Circuits.parse(Circuits.to_json(c))
	_check(bool((rt["groups"][0] as Dictionary).get("off", false)), "the off key survives a save and a load")

func _panel() -> void:
	var bet: Dictionary = Campaign.make("a_predict").diagnosis(); bet["task_id"] = "a_predict"
	_check(bool(bet.get("bet", false)) and int(bet.get("budget", 0)) > 0, "a_predict's diagnosis() is a bet with a budget")
	var p := DiagnosisPanel.new(); var st := {"picked": "", "tries": 0, "probes": 0}
	p.setup(bet, st, {}, false)
	_check(p.find_child("BtnLesionLab", true, false) == null, "the bet panel offers no lesion lab")
	_check(p.find_child("BtnSubmitDiagnosis", true, false) == null, "the bet panel has no Submit diagnosis")
	for c in bet["candidates"]: _check(p.find_child("Cand_" + str(c), true, false) != null, "candidate %s is offered" % str(c))
	var commit := p.find_child("BtnCommitTheBet", true, false) as Button
	_check(commit != null and commit.disabled, "Commit the bet is there and disabled until a candidate is picked")
	var seen := [0]; p.submit_pressed.connect(func(): seen[0] += 1)
	st["picked"] = "AVA"; p.setup(bet, st, {}, false)
	commit = p.find_child("BtnCommitTheBet", true, false) as Button
	_check(commit != null and not commit.disabled, "picking a candidate enables Commit the bet")
	commit.pressed.emit()
	_check(seen[0] == 1, "Commit the bet emits the commit")
	# running: nothing can change; a wrong bet: the verdict, that candidate closed, another bet possible
	p.setup(bet, {"picked": "AVA", "running": true, "tries": 0}, {}, false)
	_check((p.find_child("Cand_AVA", true, false) as Button).disabled and p.find_child("DxRunning", true, false) != null, "while the run goes the candidates are closed and the panel says so")
	p.setup(bet, {"picked": "", "tries": 1, "wrong": ["AVA"], "verdict": "Not AVA. With it silenced the worm does not back up."}, {}, false)
	_check((p.find_child("Cand_AVA", true, false) as Button).disabled and not (p.find_child("Cand_AVB", true, false) as Button).disabled, "a wrong bet closes that candidate and leaves the others")
	_check(p.find_child("DxResult", true, false) != null and (p.find_child("DxResult", true, false) as Label).text.begins_with("Not AVA"), "the worded verdict of the wrong bet is shown")
	_check(p.find_child("BtnCommitTheBet", true, false) != null, "another bet can be committed")
	p.setup(bet, {"solved": true, "tries": 1, "score": 85.0, "verdict": "Correct: with AVE silenced the worm still backs up."}, {}, false)
	_check(p.find_child("DxTally", true, false) != null and (p.find_child("DxTally", true, false) as Label).text.contains("Score 85"), "a right bet shows its score")
	_check(p.find_child("BtnCommitTheBet", true, false) == null, "a settled bet has no commit button")
	# an ordinary case is unchanged
	var dx: Dictionary = Campaign.make("dx_a_worm").diagnosis(); dx["task_id"] = "dx_a_worm"
	_check(not bool(dx.get("bet", false)), "dx_a_worm is an ordinary case")
	var q := DiagnosisPanel.new(); q.setup(dx, {"picked": "", "tries": 0, "probes": 0}, {}, false)
	_check(q.find_child("BtnLesionLab", true, false) != null and q.find_child("BtnCommitTheBet", true, false) == null, "an ordinary case keeps its lesion lab and has no commit button")
	_check(q.find_child("DxUsed", true, false) != null and (q.find_child("DxUsed", true, false) as Label).text.contains("the first 3 are free"), "an ordinary case keeps its probe rule text")

func _run(task: String, answer: String) -> Dictionary:
	var tele := Telemetry.new(); tele.audio_on = false; tele.variant = answer
	var c := _given(task); Circuits.ensure_pos(c)
	_check(tele.setup(c, task, true), "%s sets up (%s)" % [task, tele.error])
	tele.speed = 100.0; var guard := 0
	while not tele.finished and guard < 20000 and not tele.failed: tele.advance(0.1); guard += 1
	var sc: Dictionary = tele.task.score() if tele.task != null else {}
	if tele.sim != null: tele.sim.stop()
	_check(tele.finished, "%s with '%s' finishes its run" % [task, answer])
	return sc

func _bets() -> void:
	Telemetry.budget_us = 1000000
	var none := _run("a_predict", "")
	_check(not bool(none.get("passed", true)) and str(none.get("verdict", "")) != "", "no commit: not passed, and the verdict says why")
	var right := _run("a_predict", "AVE@0:0")
	_check(bool(right.get("passed", false)) and str(right.get("verdict", "")).begins_with("Correct"), "a right bet passes with a worded verdict (got '%s')" % str(right.get("verdict", "")))
	var wrong := _run("a_predict", "AVA@0:0")
	_check(not bool(wrong.get("passed", true)) and str(wrong.get("verdict", "")).begins_with("Not AVA"), "a wrong bet does not pass and the verdict names it (got '%s')" % str(wrong.get("verdict", "")))
	var again := _run("a_predict", "AVE@2:1")
	_check(bool(again.get("passed", false)) and float(again.get("value", 100.0)) < float(right.get("value", 0.0)), "a right bet after a wrong one costs more (%s against %s)" % [str(again.get("value")), str(right.get("value"))])
	Telemetry.budget_us = 9000
