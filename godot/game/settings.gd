class_name Settings
extends RefCounted
## User preferences (user://settings.cfg): volume, display, accessibility. Static: no autoload.

static var volume := 0.8
static var music_volume := 0.7
static var sfx_volume := 0.9
# --- audio (item H): spike click bus level ---
static var spike_volume := 0.8
# --- end audio ---
## UI scale on top of the automatic window stretch. 0 = auto (handhelds and phones are scaled up to a readable size).
static var ui_scale := 0.0
static var text_boost := 1.0
## Layout class: "auto" picks desktop, handheld or phone from the device; or force one.
static var layout := "auto"
## Okabe-Ito safe excitatory / inhibitory colours.
static var colorblind := false
static var reduced_motion := false
## Display: 0 windowed, 1 fullscreen (borderless), 2 exclusive fullscreen.
static var window_mode := 1
## 0 off, 1 on, 2 adaptive
static var vsync := 1
## 0 = uncapped
static var fps_cap := 0
## Local, opt-in play log (game/playlog.gd): time per level, runs, hint tiers, mid-level quits, edits before the first pass. Off by default, stored on the
## device, never sent anywhere; the player exports it as JSON from Settings > Game.
static var telemetry := false
const PATH := "user://settings.cfg"

## The difficulty mode (guided | standard | researcher). It is part of the save (Progress), not of the device settings; this is the settings screen's door to it.
static func mode() -> String: return Progress.mode()
static func set_mode(m: String) -> void: Progress.set_mode(m)

static var deck := false
static var compat := false   # not Forward+ (OpenGL compatibility or Forward Mobile)
static var adapter := ""

static func headless() -> bool: return DisplayServer.get_name() == "headless"

static func detect() -> void:
	adapter = RenderingServer.get_video_adapter_name()
	var low := adapter.to_lower()
	deck = OS.get_environment("SteamDeck") == "1" or "vangogh" in low or "van gogh" in low
	compat = RenderingServer.get_current_rendering_method() != "forward_plus"

static func load_all() -> void:
	detect()
	var c := ConfigFile.new()
	if c.load(PATH) != OK: return
	volume = clampf(float(c.get_value("a", "volume", 0.8)), 0.0, 1.0)
	music_volume = clampf(float(c.get_value("a", "music_volume", 0.7)), 0.0, 1.0); sfx_volume = clampf(float(c.get_value("a", "sfx_volume", 0.9)), 0.0, 1.0)
	spike_volume = clampf(float(c.get_value("a", "spike_volume", 0.8)), 0.0, 1.0)   # audio (item H)
	var us := float(c.get_value("a", "ui_scale", 0.0))
	ui_scale = 0.0 if us < 0.8 else clampf(us, 0.8, 2.0)
	text_boost = clampf(float(c.get_value("a", "text_boost", 1.0)), 1.0, 1.5)
	colorblind = bool(c.get_value("a", "colorblind", false)); reduced_motion = bool(c.get_value("a", "reduced_motion", false))
	layout = str(c.get_value("a", "layout", "auto")); if not layout in ["auto", "desktop", "handheld", "phone"]: layout = "auto"
	telemetry = bool(c.get_value("a", "telemetry", false))
	window_mode = clampi(int(c.get_value("a", "window_mode", 1)), 0, 2); vsync = clampi(int(c.get_value("a", "vsync", 1)), 0, 2); fps_cap = int(c.get_value("a", "fps_cap", 0))

## Writes user://settings.cfg (skipped with SP_NOAUTOSAVE, which smoke tests and screenshots set).
static var no_save := false   # set by the smoke test
static func save_all() -> void:
	if no_save or OS.has_environment("SP_NOAUTOSAVE"): return
	var c := ConfigFile.new()
	c.set_value("a", "volume", volume); c.set_value("a", "music_volume", music_volume); c.set_value("a", "sfx_volume", sfx_volume)
	c.set_value("a", "spike_volume", spike_volume)   # audio (item H)
	c.set_value("a", "ui_scale", ui_scale); c.set_value("a", "text_boost", text_boost)
	c.set_value("a", "colorblind", colorblind); c.set_value("a", "reduced_motion", reduced_motion)
	c.set_value("a", "layout", layout); c.set_value("a", "telemetry", telemetry)
	c.set_value("a", "window_mode", window_mode); c.set_value("a", "vsync", vsync); c.set_value("a", "fps_cap", fps_cap)
	c.save(PATH)

## UI scale actually used: the explicit choice, or auto (undo the window stretch on small windows so text stays a readable size).
static func effective_ui_scale(window: Window) -> float:
	if ui_scale > 0.0: return ui_scale * text_boost
	Layout.update(window)
	var sz := Vector2(window.size)
	var s0 := maxf(minf(sz.x / 1920.0, sz.y / 1080.0), 0.01)   # the project's canvas_items stretch scale
	return clampf(Layout.total_scale(window) / s0, 0.3, 6.0) * text_boost

## Push the current values into the engine (call after load and after every change).
static func apply(window: Window) -> void:
	AudioServer.set_bus_volume_db(0, linear_to_db(maxf(volume, 0.0001)))
	GameAudio.apply_volumes()   # audio (item H): Music / SFX / Spikes buses
	window.content_scale_factor = effective_ui_scale(window)
	if not OS.has_environment("SP_NODISPLAY") and not headless():
		var wm: int = [Window.MODE_WINDOWED, Window.MODE_FULLSCREEN, Window.MODE_EXCLUSIVE_FULLSCREEN][window_mode]
		if window.mode != wm and not OS.has_environment("SP_SHOT") and not OS.has_environment("SP_SMOKE"): window.mode = wm
		DisplayServer.window_set_vsync_mode([DisplayServer.VSYNC_DISABLED, DisplayServer.VSYNC_ENABLED, DisplayServer.VSYNC_ADAPTIVE][vsync])
	apply_fps()

static var idle := false        # set by the app shell: nothing happening for a while
static var focused := true      # set by the app shell on focus in/out
static func apply_fps() -> void:
	if OS.has_environment("SP_NOTHROTTLE"): Engine.max_fps = 0; return   # benchmarks and tests
	if OS.get_environment("SP_FPS") != "": Engine.max_fps = int(OS.get_environment("SP_FPS")); return   # pacing measurements: a fixed frame rate (60 = a GPU at vsync)
	var cap: int = fps_cap
	if cap >= 1000: cap = 0
	if not focused: cap = 10 if cap == 0 or cap > 10 else cap          # unfocused window: nearly asleep
	elif idle: cap = 30 if cap == 0 or cap > 30 else cap
	Engine.max_fps = cap

## Written to user://logs/spike.log at start.
static func diagnostics(window: Window) -> String:
	var v := str(ProjectSettings.get_setting("application/config/version", "dev"))
	return "SPIKE %s\nOS: %s %s\nRenderer: %s (%s)\nAdapter: %s\nWindow: %s  scale=%.2f  ui=%.2f  layout=%s\nGodot: %s" % [
		v, OS.get_name(), OS.get_version(), RenderingServer.get_current_rendering_method(), RenderingServer.get_current_rendering_driver_name(), adapter,
		str(window.size), window.content_scale_factor, effective_ui_scale(window), Layout.kind, Engine.get_version_info().string]

static func write_log(window: Window) -> void:
	if no_save or OS.has_environment("SP_NOAUTOSAVE"): return
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("user://logs"))
	var f := FileAccess.open("user://logs/spike.log", FileAccess.WRITE)
	if f != null: f.store_string(diagnostics(window) + "\n"); f.close()
