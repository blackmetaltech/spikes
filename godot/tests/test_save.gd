extends SceneTree
## Headless save-layer test: SaveIO (atomic write, backups, quarantine, future files, read-only disk), Progress and the damaged / truncated / garbage /
## future progress files, Circuits.validate on hostile circuit files, the Reference overwrite and the restart reference-lock repros, bench history,
## the play log's level_end, diagnosis persistence, save export / import, test hooks never saving.
##   scripts/save_test.sh   (ctest `save_io`): XDG_DATA_HOME points at a scratch folder so the real save is never touched
##   godot --headless --path godot -s tests/test_save.gd     prints "SAVE OK" or "SAVE FAIL: ..." (exit 1); refuses to run without XDG_DATA_HOME

const FIX := "res://tests/fixtures/save/"
const REF := "reflex_arc.json"   # the reference circuit of task "reflex" (the task table needs the extension, which this test does not load: the print is cached by hand)
var fails := 0
var checks := 0

func _check(ok: bool, msg: String) -> void:
	checks += 1
	if not ok:
		fails += 1
		print("SAVE FAIL: ", msg)

func _user(rel: String) -> String: return "user://" + rel
func _exists(rel: String) -> bool: return FileAccess.file_exists(_user(rel))
func _text(path: String) -> String: return FileAccess.get_file_as_string(path)
func _put(path: String, text: String) -> void:
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(path.get_base_dir()))
	var f := FileAccess.open(path, FileAccess.WRITE); f.store_string(text); f.close()
func _files_like(dir: String, prefix: String) -> Array:
	var out := []; var d := DirAccess.open(dir)
	if d == null: return out
	for f in d.get_files():
		if f.begins_with(prefix): out.append(f)
	return out

## Forget everything the game kept in memory and on disk (between scenarios).
func _wipe() -> void:
	for dir in ["user://", "user://circuits", "user://circuits/reflex", "user://circuits/reflex/history", "user://logs"]:
		var d := DirAccess.open(dir)
		if d == null: continue
		for f in d.get_files(): DirAccess.remove_absolute(ProjectSettings.globalize_path(dir.path_join(f)))
	SaveIO.forget_state(); Progress._loaded = false; Progress._best = {}; Progress._mode = "standard"
	Playlog._loaded = false; Playlog._levels = {}; Playlog._events = []; Playlog._cur = ""

func _reload_progress() -> void: Progress._loaded = false; Progress._best = {}; Progress._mode = "standard"

func _init() -> void:
	if OS.get_environment("XDG_DATA_HOME") == "":
		print("SAVE SKIP: XDG_DATA_HOME is not set (run scripts/save_test.sh); not touching the real save"); quit(77); return
	for h in SaveIO.HOOKS: OS.unset_environment(h)
	OS.unset_environment("SP_NOAUTOSAVE"); OS.unset_environment("SP_ALLOW_SAVE")
	Settings.no_save = false
	_wipe()
	_atomic_and_backups()
	_damaged_progress()
	_future_progress()
	_legacy_progress()
	_migrate_stars()
	_circuits_hostile()
	_reference_overwrite_and_lock()
	_history()
	_read_only_dir()
	_hooks()
	_playlog()
	_diagnosis()
	_zip()
	_scale()
	_wipe()
	if fails == 0: print("SAVE OK checks=%d" % checks)
	quit(0 if fails == 0 else 1)

# ------------------------------------------------------------------------------------------------
func _atomic_and_backups() -> void:
	var p := _user("t_atomic.cfg")
	for i in 6: _check(SaveIO.write_text(p, "v%d" % i), "write %d failed" % i)
	_check(_text(p) == "v5", "newest content not in place")
	_check(not FileAccess.file_exists(p + ".tmp"), "temp file left behind")
	_check(_text(p + ".bak1") == "v4" and _text(p + ".bak2") == "v3" and _text(p + ".bak3") == "v2", "three rotating backups are the last three states")
	_check(not FileAccess.file_exists(p + ".bak4"), "more than three backups")
	# a slow rotation interval keeps the newest backup
	SaveIO.write_text(p, "v6", 600.0)
	_check(_text(p + ".bak1") == "v4", "a backup younger than rotate_secs was rotated")

func _progress_with_two_levels() -> void:
	Progress.record("reflex", 88.0, true, 3, "standard")
	Progress.record("worm", 80.0, true, 2, "standard")

func _damaged_progress() -> void:
	# truncated file (what a kill during an in-place write left), no backup: quarantined, saving off, notice names the file, the data is kept in the quarantine
	_wipe()
	_put(_user("progress.cfg"), FileAccess.get_file_as_string(FIX + "trunc.cfg"))
	_check(not bool(Progress.best("reflex")["passed"]), "a truncated file gives empty progress")
	_check(SaveIO.status.get(_user("progress.cfg")) == "bad", "truncated file not flagged bad: %s" % str(SaveIO.status))
	_check(not _files_like("user://", "progress.cfg.corrupt").is_empty(), "the damaged file was not moved aside")
	_check(not SaveIO.notices.is_empty() and "progress.cfg" in str(SaveIO.notices[0]), "no notice naming the file: %s" % str(SaveIO.notices))
	Progress.record("reflex", 90.0, true, 1, "standard")
	_check(not _exists("progress.cfg"), "a pass overwrote / recreated the damaged file's place (saving must be off)")
	_check(_text("user://" + str(_files_like("user://", "progress.cfg.corrupt")[0])).length() == 60, "the quarantined file lost its content")
	# empty, garbage, and a cut-short file with a version but no end marker
	for case in [["empty", ""], ["garbage", "\u0001\u0002 not a config ÿ\n[[["], ["no_end", "[best]\n\nreflex={\n\"passed\": true\n}\n\n[meta]\n\nsave_version=1\nmode=\"standard\"\n"]]:
		_wipe(); _put(_user("progress.cfg"), case[1])
		Progress.best("reflex")
		_check(SaveIO.status.get(_user("progress.cfg")) == "bad", "%s file not flagged bad" % case[0])
		Progress.record("reflex", 90.0, true, 1, "standard")
		_check(not _exists("progress.cfg"), "%s file: saving was not switched off" % case[0])
	# a damaged primary with a good backup: the backup is used, nothing is lost
	_wipe(); _progress_with_two_levels(); Progress.record("reflex", 91.0, true, 3, "standard")
	_check(_exists("progress.cfg.bak1"), "no backup after the second save")
	var f := FileAccess.open(_user("progress.cfg"), FileAccess.WRITE); f.store_string("[best]\n\nreflex={\n"); f.close()   # truncate the primary
	_reload_progress()
	_check(bool(Progress.best("reflex")["passed"]) and bool(Progress.best("worm")["passed"]), "backup not used after truncation")
	_check(SaveIO.status.get(_user("progress.cfg")) == "recovered", "status is not recovered: %s" % str(SaveIO.status))
	Progress.record("reflex", 95.0, true, 3, "standard")
	_check(_exists("progress.cfg"), "saving does not continue after a recovery")
	_reload_progress()
	_check(bool(Progress.best("worm")["passed"]), "worm lost after a recovery and a save")

func _future_progress() -> void:
	_wipe()
	var future := "[best]\n\nreflex={\n\"value\": 77.0,\n\"passed\": true,\n\"stars\": 2\n}\n\n[meta]\n\nsave_version=99\nmode=\"guided\"\n\n[zz_end]\n\nok=true\n"
	_put(_user("progress.cfg"), future)
	_check(Progress.mode() == "guided" and bool(Progress.best("reflex")["passed"]), "a newer save is not read")
	_check(SaveIO.status.get(_user("progress.cfg")) == "future" and not SaveIO.notices.is_empty(), "no read-only notice for a newer save")
	Progress.record("worm", 80.0, true, 1, "guided")
	_check(_text(_user("progress.cfg")) == future, "a newer save was overwritten")

## Save version 1 to 2 (Progress._migrate): Guided stars are capped at two, a Researcher pass gets its seal, a diagnosis case keeps two stars until it is solved again;
## passes, scores, hints and the recipe flag survive, and the migrated file is written with version 2 and kept as a backup.
func _migrate_stars() -> void:
	_wipe()
	var old := "[best]\n\nreflex={\n\"value\": 90.0,\n\"passed\": true,\n\"stars\": 3,\n\"recipe\": true,\n\"modes\": {\n\"guided\": {\"value\": 90.0, \"passed\": true, \"stars\": 3},\n\"researcher\": {\"value\": 80.0, \"passed\": true, \"stars\": 2}\n}\n}\n\ndx_worm={\n\"value\": 100.0,\n\"passed\": true,\n\"stars\": 3\n}\n\nworm={\n\"value\": 60.0,\n\"passed\": true,\n\"stars\": 3,\n\"modes\": {\n\"standard\": {\"value\": 60.0, \"passed\": true, \"stars\": 3}\n}\n}\n\n[meta]\n\nsave_version=1\nmode=\"standard\"\n\n[zz_end]\n\nok=true\n"
	_put(_user("progress.cfg"), old)
	var r := Progress.best("reflex")
	_check(bool(r["passed"]) and int(r["stars"]) == 2, "migration: guided 3 + researcher 2 becomes 2 stars, the pass is kept (%s)" % str(r["stars"]))
	_check(int(Progress.best_in("reflex", "guided")["stars"]) == 2 and bool(Progress.best_in("reflex", "researcher")["seal"]), "migration: guided capped at 2, the researcher pass has its seal")
	_check(bool(r.get("recipe", false)) and float(r["value"]) == 90.0, "migration keeps the recipe flag and the score")
	_check(int(Progress.best("dx_worm")["stars"]) == 2 and bool(Progress.best("dx_worm")["passed"]), "migration: a diagnosis case keeps two stars")
	_check(int(Progress.best("worm")["stars"]) == 3, "migration leaves a Standard 3-star alone")
	_check("save_version=2" in _text(_user("progress.cfg")) and _exists("progress.cfg.bak1"), "the migrated file is written as version 2 and the old one kept")
	_reload_progress()
	_check(int(Progress.best("reflex")["stars"]) == 2 and int(Progress.best("worm")["stars"]) == 3, "the migrated file reads back the same")
	# the new rules: Guided stops at 2, the challenge decides 3, a Researcher five-world pass gets the seal
	Progress.set_mode("guided")
	_check(Progress.stars_for("worm", true, true, 1) == 2, "guided never earns a third star")
	Progress.set_mode("standard")
	_check(Progress.stars_for("worm", true, false, 1) == 1 and Progress.stars_for("worm", true, true, 1) == 3 and Progress.stars_for("worm", true, true, 99) == 2, "star 2 is five worlds, star 3 the challenge (neuron par, no extension: the table)")
	# wing gating (Campaign.chapter_open): a fresh save opens chapters I and II only; the wings wait for the chapter I spine, in every mode
	for m in ["standard", "guided", "researcher"]:
		Progress.set_mode(m)
		_check(Campaign.chapter_open("invertebrates") and Campaign.chapter_open("spinal"), "the first two chapters are open from the start (%s)" % m)
		_check(not Campaign.chapter_open("leech") and not Campaign.chapter_open("crab") and not Campaign.chapter_open("dynamics") and not Campaign.chapter_open("fly_mb"), "the wings are closed before the chapter I spine (%s)" % m)
	_check(Campaign.chapter_lock_reason("leech").begins_with("Pass the Invertebrates spine"), "the wing lock says which spine opens it")
	Progress.set_mode("researcher")
	Progress.record_result("reflex", 85.0, {"passed": true, "five_seeds": true, "neurons": 5, "mode": "researcher"})
	_check(Progress.seal("reflex"), "a researcher five-world pass earns the seal")

func _legacy_progress() -> void:
	_wipe()
	_put(_user("progress.cfg"), "[best]\n\nreflex={\n\"value\": 66.0,\n\"passed\": true,\n\"stars\": 1\n}\n\n[meta]\n\nmode=\"researcher\"\n")
	_check(bool(Progress.best("reflex")["passed"]) and Progress.mode() == "researcher", "a file from before save_version is not read")
	_check(SaveIO.notices.is_empty(), "a legacy file produced a notice")
	Progress.record("worm", 70.0, true, 1, "researcher")
	var t := _text(_user("progress.cfg"))
	_check("save_version=2" in t and "[zz_end]" in t, "the upgraded file has no version / end marker")
	_check(_exists("progress.cfg.bak1"), "the legacy file was not kept as a backup")
	# settings and goals go through the same helper
	Settings.volume = 0.31; Settings.save_all(); Settings.volume = 0.8; Settings.load_all()
	_check(is_equal_approx(Settings.volume, 0.31), "settings round trip")
	_put(_user("settings.cfg"), "[a]\nvolume=")
	Settings.volume = 0.8; Settings.load_all()
	_check(SaveIO.status.get(_user("settings.cfg")) == "bad" and is_equal_approx(Settings.volume, 0.8), "a truncated settings file is not handled")

func _circuits_hostile() -> void:
	_wipe()
	var expect_bad := ["h_types", "h_hugen", "h_notjson", "h_deep", "h_weird"]
	for n in expect_bad:
		var r := Circuits.parse_checked(_text(FIX + n + ".json"))
		_check(str(r["error"]) != "", "%s: no error (a banner is needed)" % n)
		_check((r["circuit"] as Dictionary).is_empty(), "%s: a circuit came back with the error" % n)
	for n in ["h_big", "mine_reflex"]:
		var r := Circuits.parse_checked(_text(FIX + n + ".json"))
		_check(str(r["error"]) == "", "%s should load: %s" % [n, str(r["error"])])
	# the hostile autosave: renamed .bad, no circuit, the level opens empty
	var path := _user("circuits/last_reflex.json")
	for n in expect_bad:
		_wipe(); _put(path, _text(FIX + n + ".json"))
		var r := LabScreen.read_autosave(path)
		_check(str(r["error"]) not in ["", "none"], "%s: autosave not rejected" % n)
		_check(not FileAccess.file_exists(path) and FileAccess.file_exists(path + ".bad"), "%s: autosave not renamed .bad" % n)
	# limits
	var big := {"populations": [], "projections": []}
	for i in Circuits.max_neurons() / 400 + 1: (big["populations"] as Array).append({"id": "P%d" % i, "model": "iaf_psc_alpha", "n": 400})
	_check(Circuits.validate(big) != "", "more neurons in all than the engine allows accepted")
	(big["populations"] as Array).resize(6)
	_check(Circuits.validate(big) == "", "2400 neurons refused (the engine allows %d)" % Circuits.max_neurons())
	_check(Circuits.validate({"populations": [{"id": "A", "model": "m", "n": 1.5}]}) != "", "non-integer n accepted")
	_check(Circuits.validate({"populations": [{"id": "A", "model": "m", "n": 400}]}) == "", "n = 400 rejected")
	_check(Circuits.validate({"populations": [{"id": "A", "model": "m", "n": Circuits.max_neurons() + 1}]}) != "", "n above the engine limit accepted")
	_check(Circuits.validate({"populations": [{"id": "A", "model": "m", "n": 0}]}) != "", "n = 0 accepted")
	_check(Circuits.validate({"populations": [{"id": "A", "model": "m", "n": 2, "params": {"V_m": INF}}]}) != "", "infinite parameter accepted")
	# non-finite positions are replaced, not fatal
	var w := {"populations": [{"id": "A", "model": "m", "n": 3, "pos": [[1, 2], ["a", "b"], [INF, NAN]]}]}
	_check(Circuits.validate(w) == "", "positions are not a validation failure")
	_check(Circuits.repair_positions(w) == 2, "two damaged positions were not repaired")
	for q in w["populations"][0]["pos"]: _check(is_finite(float(q[0])) and is_finite(float(q[1])), "a position is still not finite")
	# every shipped reference circuit validates (the checks must not reject real circuits)
	var bad := []
	for d in ["res://circuits"]:
		for fn in DirAccess.get_files_at(d):
			if fn.ends_with(".json") and Circuits.validate(JSON.parse_string(_text(d + "/" + fn))) != "": bad.append(fn)
	_check(bad.is_empty(), "shipped circuits rejected: %s" % str(bad))

func _lab_for(task: String) -> LabScreen:
	var lab := LabScreen.new(); lab.task_id = task; return lab

func _reference_overwrite_and_lock() -> void:
	_wipe(); LabScreen._ref_loaded = {}
	Circuits._ref_prints["reflex"] = Circuits.fingerprint(Circuits.parse(Circuits.read_reference(REF)))
	var path := _user("circuits/last_reflex.json")
	var mine := _text(FIX + "mine_reflex.json")
	_put(path, mine)
	# the repro: Reference fills the bench, the lab rebuilds the side panel (-> _autosave): the player's file must stay
	var lab := _lab_for("reflex")
	lab.circuit = Circuits.parse(Circuits.read_reference(REF))
	_check(not lab.circuit.is_empty(), "the reflex reference is not readable (res://circuits/reflex_arc.json)")
	lab._set_ref_lock(true); lab._autosave()
	_check(_text(path) == mine, "Reference overwrote the player's autosave")
	_check(lab._is_reference(), "the reference bench is not locked")
	# an edit on the reference bench is saved WITH the lock
	var pr: Dictionary = lab.circuit["projections"][0]
	Circuits.set_proj_weight(pr, Circuits.proj_weight(pr) * 1.01)
	lab._autosave()
	_check(_text(path) != mine and "ref_lock" in _text(path), "an edited reference bench is not saved with the lock flag")
	# restart: a new lab reads the autosave; one weight at 1% off must still be locked (the h/ref2 repro)
	var r := LabScreen.read_autosave(path)
	_check(str(r["error"]) == "" and bool(r["ref_lock"]), "the lock flag did not come back from the file")
	LabScreen._ref_loaded = {}
	var lab2 := _lab_for("reflex"); lab2.circuit = r["circuit"]
	_check(not lab2._is_reference(), "(sanity) the fingerprint alone does not catch a 1% edit")
	LabScreen._ref_loaded["reflex"] = bool(r["ref_lock"])
	_check(lab2._is_reference(), "the reference lock did not survive the restart")
	# Empty bench clears the lock for good
	lab2._set_ref_lock(false); lab2.circuit = Circuits.empty("x")
	lab2._autosave()
	LabScreen._ref_loaded = {}
	var r2 := LabScreen.read_autosave(path)
	_check(not bool(r2["ref_lock"]), "the lock is still in the file after Empty bench")
	LabScreen._ref_loaded = {}
	lab.free(); lab2.free()

func _history() -> void:
	_wipe()
	Circuits._ref_prints["reflex"] = Circuits.fingerprint(Circuits.parse(Circuits.read_reference(REF)))
	var c := Circuits.parse(Circuits.read_reference(REF))
	_check(not Circuits.history_push("reflex", Circuits.empty("empty")), "an empty bench was kept in the history")
	for i in 13:
		(c as Dictionary)["name"] = "bench %d" % i
		Circuits.history_push("reflex", c)
		OS.delay_msec(3)
	_check(Circuits.history_list("reflex").size() == Circuits.HISTORY_KEEP, "history is not capped at %d (%d)" % [Circuits.HISTORY_KEEP, Circuits.history_list("reflex").size()])
	_check(str(Circuits.history_latest("reflex").get("name", "")) == "bench 12", "the newest bench is not the latest")
	Circuits.history_pop("reflex")
	_check(str(Circuits.history_latest("reflex").get("name", "")) == "bench 11", "pop did not remove the newest")
	# the lab's restore: swap the bench with the newest history entry
	var lab := _lab_for("reflex"); lab.circuit = c
	var before := Circuits.history_list("reflex").size()
	lab._keep_bench()
	_check(Circuits.history_list("reflex").size() >= before, "_keep_bench lost an entry")
	lab.free()

func _read_only_dir() -> void:
	_wipe()
	if OS.execute("id", ["-u"], []) == 0:
		var out := []; OS.execute("id", ["-u"], out)
		if str(out[0]).strip_edges() == "0": print("SAVE note: running as root, read-only directory scenario skipped"); return
	_progress_with_two_levels()
	var dir := ProjectSettings.globalize_path("user://")
	OS.execute("chmod", ["555", dir.trim_suffix("/")])
	SaveIO.forget_state()
	Progress.record("reflex", 99.0, true, 3, "standard")   # must not crash
	_check(SaveIO.write_errors >= 1, "a failed write was not counted")
	var n := SaveIO.notices.size()
	_check(n == 1 and dir.trim_suffix("/") in str(SaveIO.notices[0]), "no one-time warning with the path: %s" % str(SaveIO.notices))
	Progress.record("worm", 99.0, true, 3, "standard")
	_check(SaveIO.notices.size() == n, "the write failure was reported more than once")
	OS.execute("chmod", ["755", dir.trim_suffix("/")])
	_check(_exists("progress.cfg"), "the previous good file disappeared")

func _hooks() -> void:
	_wipe()
	_check(SaveIO.enabled(), "saving is off in a plain run")
	OS.set_environment("SP_TASK", "reflex"); _check(not SaveIO.enabled(), "SP_TASK did not turn saving off")
	OS.set_environment("SP_ALLOW_SAVE", "1"); _check(SaveIO.enabled(), "SP_ALLOW_SAVE=1 did not turn it back on")
	OS.unset_environment("SP_ALLOW_SAVE"); OS.unset_environment("SP_TASK")
	for quiet in ["SP_SCREEN", "SP_LAYOUT"]:
		OS.set_environment(quiet, "x"); _check(SaveIO.enabled(), "%s turned saving off" % quiet); OS.unset_environment(quiet)
	for h in ["SP_CIRCUIT", "SP_UNLOCK_ALL", "SP_PROGRESS", "SP_REF", "SP_SHOT"]:
		OS.set_environment(h, "1"); _check(not SaveIO.enabled(), "%s did not turn saving off" % h); OS.unset_environment(h)
	# an h/circ-style run (SP_CIRCUIT) leaves the save folder untouched
	OS.set_environment("SP_CIRCUIT", FIX + "mine_reflex.json")
	_put(_user("circuits/last_reflex.json"), "keep")
	var lab := _lab_for("reflex"); lab.circuit = Circuits.parse(_text(FIX + "mine_reflex.json")); lab._autosave()
	Progress.record("reflex", 50.0, true, 1, "standard")
	_check(_text(_user("circuits/last_reflex.json")) == "keep" and not _exists("progress.cfg"), "a hook run wrote into the real save")
	lab.free(); OS.unset_environment("SP_CIRCUIT")

func _playlog() -> void:
	_wipe(); Settings.telemetry = true
	Playlog.level_start("reflex")
	Playlog.level_end("worm")   # a lab closing late for another level
	_check(Playlog._cur == "reflex", "level_end(other task) ended the open visit")
	Playlog.level_end("reflex")
	_check(Playlog._cur == "", "level_end(task) did not end its visit")
	Playlog.event("hint", {"task": "reflex", "tier": 1}); Playlog.flush()
	_check(_exists("playlog.json"), "play log not written")
	Playlog._loaded = false; Playlog._levels = {}; Playlog._events = []
	_check(Playlog.count() > 0, "play log not read back")
	Playlog.clear(); _check(not _exists("playlog.json"), "clear left the file")
	Settings.telemetry = false

func _diagnosis() -> void:
	_wipe()
	Progress.dx_put("dx_x", {"tries": 2, "probes": 5, "probed": {"scope:A": true}, "wrong": ["a>b", "c>d"], "revealed": false})
	_reload_progress()
	var st := Progress.dx_state("dx_x")
	_check(int(st["tries"]) == 2 and int(st["probes"]) == 5 and (st["wrong"] as Array).size() == 2, "wrong picks / probes did not survive a restart")
	st["revealed"] = true; Progress.dx_put("dx_x", st); _reload_progress()
	_check(Progress.dx_revealed("dx_x"), "reveal did not survive a restart")
	Progress.dx_solved("dx_x"); _reload_progress()
	_check(int(Progress.dx_state("dx_x")["tries"]) == 0 and Progress.dx_revealed("dx_x"), "solving must clear the tally but keep the reveal")
	# a revealed case caps at one star
	_check(Progress.record_result("dx_x", 100.0, {"passed": true, "five_seeds": true, "neurons": 0, "max_stars": 1}) == 1, "a revealed case earned more than one star")
	# the panel orders answer-first candidates by a stable hash, and leaves a neutral order alone
	var dx := {"task_id": "dx_x", "answer": "a>b", "candidates": ["a>b", "c>d", "e>f", "g>h", "i>j"]}
	var o1 := DiagnosisPanel.ordered(dx); var o2 := DiagnosisPanel.ordered(dx)
	_check(o1 == o2 and str(o1[0]) != "a>b" and o1.size() == 5, "answer-first candidates were not reordered stably: %s" % str(o1))
	var neutral := {"task_id": "dx_x", "answer": "e>f", "candidates": ["c>d", "e>f", "a>b"]}
	_check(DiagnosisPanel.ordered(neutral) == ["c>d", "e>f", "a>b"], "the engine's neutral order was changed")

func _zip() -> void:
	_wipe(); _progress_with_two_levels()
	_put(_user("circuits/last_reflex.json"), _text(FIX + "mine_reflex.json"))
	_put(_user("circuits/reflex/slot_B.json"), _text(FIX + "mine_reflex.json"))   # a lab solution slot travels with the save
	var zp := ProjectSettings.globalize_path("user://t_save.zip")
	_check(SaveIO.export_zip(zp) == "", "export failed")
	Progress.reset(); _check(not bool(Progress.best("reflex")["passed"]), "reset left progress")
	_check(not _files_like("user://", "progress.cfg.reset").is_empty(), "reset did not keep the old file")
	DirAccess.remove_absolute(ProjectSettings.globalize_path("user://circuits/last_reflex.json"))
	DirAccess.remove_absolute(ProjectSettings.globalize_path("user://circuits/reflex/slot_B.json"))
	_check(SaveIO.import_zip(zp) == "", "import failed")
	Progress.reload()
	_check(bool(Progress.best("reflex")["passed"]) and bool(Progress.best("worm")["passed"]) and _exists("circuits/last_reflex.json"), "import did not restore the save")
	_check(_exists("circuits/reflex/slot_B.json"), "import did not restore the solution slot")
	# a zip with a hostile entry and one with a damaged circuit are refused whole
	for case in [["../evil.cfg", "x"], ["circuits/../progress/slot_A.json", _text(FIX + "mine_reflex.json")], ["circuits/reflex/slot_D.json", _text(FIX + "mine_reflex.json")], ["circuits/last_reflex.json", _text(FIX + "h_types.json")], ["progress.cfg", "[best]\n\nreflex={"]]:
		var z := ZIPPacker.new(); var bp := ProjectSettings.globalize_path("user://t_bad.zip"); z.open(bp)
		z.start_file(SaveIO.MANIFEST); z.write_file(JSON.stringify({"format": "spike-save-1", "save_version": 1}).to_utf8_buffer()); z.close_file()
		z.start_file(case[0]); z.write_file(str(case[1]).to_utf8_buffer()); z.close_file(); z.close()
		_check(SaveIO.import_zip(bp) != "", "a bad save zip (%s) was imported" % case[0])
	_check(SaveIO.import_zip(ProjectSettings.globalize_path("user://nothing.zip")) != "", "a missing zip was imported")

func _scale() -> void:
	var win := root
	var kind := Layout.kind; Layout.kind = "desktop"
	Settings.ui_scale = 2.0; Settings.text_boost = 1.5
	var eff := Settings.effective_ui_scale(win)
	_check(eff <= Settings.max_scale(win) + 0.001 and Settings.scale_capped(win), "UI scale x text size is not clamped (%.2f)" % eff)
	var sz := Vector2(win.size); var s0 := maxf(minf(sz.x / 1920.0, sz.y / 1080.0), 0.01)
	_check(sz.x / (s0 * eff) >= 1279.0 and sz.y / (s0 * eff) >= 719.0, "the clamped scale leaves less than 1280x720 logical")
	Settings.ui_scale = 0.0; Settings.text_boost = 1.0; Layout.kind = kind
