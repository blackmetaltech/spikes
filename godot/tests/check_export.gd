class_name CheckExport
extends RefCounted
## Export self-test (scripts/package.sh runs it from the exported binary): the instant CHECK works from a shipped build.
##   SP_CHECK_EXPORT=1 Spike.x86_64 --headless     prints "CHECK EXPORT OK" or "CHECK EXPORT FAIL: ..." and quits (exit 1 on failure)
## (release export templates ignore --script, hence the hook in main.gd). It proves, without the dev tree: the reference circuits are readable
## (pck or loose copy), the extension finds the bundled spike-run next to itself (not on PATH), and that runner scores a reference circuit headless.

static func _fail(app: Node, msg: String) -> void:
	print("CHECK EXPORT FAIL: ", msg)
	app.get_tree().quit(1)

static func run(app: Node) -> void:
	var tid := "reflex"
	var t = Campaign.make(tid)
	if t == null: _fail(app, "extension did not load or task %s is unknown" % tid); return
	var txt := Circuits.read_reference(str(Campaign.ref_path(tid)))
	if txt == "": _fail(app, "reference circuit of %s not readable" % tid); return
	var runner: String = SpikeTask.headless_runner_path()
	print("runner: ", runner)
	if runner == "": _fail(app, "no spike-run found"); return
	var exe_dir := OS.get_executable_path().get_base_dir()
	if OS.has_feature("template") and not runner.begins_with(exe_dir) and not (OS.get_name() == "macOS" and runner.begins_with(exe_dir.get_base_dir().path_join("Frameworks"))): _fail(app, "runner %s is not next to the executable %s" % [runner, exe_dir]); return
	var job: int = t.score_headless_async(txt, PackedInt64Array([1]), "standard", "")
	if job == 0: _fail(app, "score_headless_async refused the job"); return
	var t0 := Time.get_ticks_msec()
	while Time.get_ticks_msec() - t0 < 120000:
		var r: Dictionary = t.score_headless_poll(job)
		if bool(r.get("done", false)):
			if not bool(r.get("ok", false)): _fail(app, "headless run failed: %s" % str(r.get("error", ""))); return
			print("CHECK EXPORT OK: %s passed=%s value=%s (%d ms)" % [tid, str(r.get("passed")), str(r.get("value")), Time.get_ticks_msec() - t0])
			app.get_tree().quit(0 if bool(r.get("passed", false)) else 1); return
		await app.get_tree().create_timer(0.05).timeout
	_fail(app, "timeout")
