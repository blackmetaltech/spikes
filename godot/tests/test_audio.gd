extends SceneTree
## Headless audio smoke: every catalog sound loads, every GameAudio API is called, the spike limiter is checked deterministically.
##   godot --headless --path godot -s tests/test_audio.gd        prints "AUDIO OK" or "AUDIO FAIL: ..." (exit 1)

var fails := 0

func _check(ok: bool, msg: String) -> void:
	if not ok:
		fails += 1
		print("AUDIO FAIL: ", msg)

func _init() -> void:
	Settings.no_save = true
	var a := GameAudio.new()
	root.add_child(a)
	await process_frame
	_check(GameAudio.current == a, "current not set")
	for bus in ["Music", "SFX", "Spikes"]:
		_check(AudioServer.get_bus_index(bus) != -1, "bus %s missing" % bus)
	# catalog: every entry has a file that loads as a non-empty stream
	var names := a.cue_names()
	_check(names.size() >= 30, "catalog too small: %d" % names.size())
	var total_ms := 0
	for n in names:
		var s := a.stream(n)
		_check(s != null and s.get_length() > 0.0, "sound %s did not load" % n)
		if s != null: total_ms += int(s.get_length() * 1000)
	# every UI cue name maps to a catalog sound
	for k in GameAudio.UI_CUE:
		_check(a.stream(String(GameAudio.UI_CUE[k])) != null, "UI cue %s has no sound" % k)
		GameAudio.event(k)
	GameAudio.event("no_such_cue")
	# spikes: all populations, both kinds, through the static API
	for p in 12:
		GameAudio.spike(p, p % 2 == 1)
	_check(a.spikes_played > 0, "no spike played")
	# body
	for kind in ["heart", "twitch", "gut"]:
		GameAudio.body(kind, 1.0)
	for i in 30:
		GameAudio.body("breath", 0.5 + 0.5 * sin(i * 0.2))
		await process_frame
	GameAudio.body("nonsense", 1.0)
	GameAudio.ambient(true)
	GameAudio.ambient(false)
	GameAudio.silence_body()
	# limiter, deterministic clock: 1000 spikes in one second from 40 populations is a crackle, not noise
	a.spikes_played = 0
	a.spikes_dropped = 0
	a.clock_ms = 100000.0
	var played := 0
	for i in 1000:
		if a.spike_click_at(i % 40, i % 5 == 0, 200000.0 + i):
			played += 1
	_check(played <= int(GameAudio.SPIKE_RATE + GameAudio.SPIKE_BURST) + 2, "1000 spikes/s not limited: %d clicks" % played)
	_check(played >= 60, "1000 spikes/s over-limited: %d clicks" % played)
	# a burst of one population rolls (at least ~50/s) but no faster than the per-population gap
	a._pop_last.clear(); a._tokens = GameAudio.SPIKE_BURST
	var roll := 0
	for i in 200:
		if a.spike_click_at(3, false, 300000.0 + i * 2.0):   # 500/s offered
			roll += 1
	_check(roll >= int(0.4 / (GameAudio.POP_GAP_MS / 1000.0) * 0.8) and roll <= 36, "burst roll count off: %d in 0.4 s" % roll)
	# focus: only the focused population clicks
	a._tokens = GameAudio.SPIKE_BURST; a._pop_last.clear()
	GameAudio.set_spike_focus(5)
	_check(not a.spike_click_at(4, false, 400000.0), "focus let another population through")
	_check(a.spike_click_at(5, false, 400000.0), "focus blocked the focused population")
	GameAudio.set_spike_focus(-1)
	# volumes reach the buses
	Settings.spike_volume = 0.0
	GameAudio.apply_volumes()
	_check(AudioServer.is_bus_mute(AudioServer.get_bus_index("Spikes")), "zero Spikes volume did not mute the bus")
	Settings.spike_volume = 0.5
	GameAudio.apply_volumes()
	_check(not AudioServer.is_bus_mute(AudioServer.get_bus_index("Spikes")), "Spikes bus stayed muted")
	a.queue_free()
	await process_frame
	GameAudio.spike(0, false)   # no instance: must be a no-op
	print("AUDIO sounds=%d total=%d ms played=%d roll=%d" % [names.size(), total_ms, played, roll])
	print("AUDIO OK" if fails == 0 else "AUDIO FAIL (%d)" % fails)
	quit(0 if fails == 0 else 1)
