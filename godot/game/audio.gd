class_name GameAudio
extends Node
## Game audio: spike sonification, body sounds, UI cues and an optional lab ambience. Assets come from tools/audio/generate.py
## into res://assets/audio/ (catalog.json). Everything is synthesized offline; a missing file or bus is silent, nothing errors.
##
## Setup, once, in the shell (main.gd) after Settings.load_all():   add_child(GameAudio.new())
## Everything else goes through static helpers, so screens never hold a reference (no autoload); each is a no-op with no instance:
##   GameAudio.spike(pop_index, is_inhibitory)   one spike -> one click (warm for excitatory, cool for inhibitory, pitch by population)
##   GameAudio.event("run")                      UI cues: run pause pass star1 star2 star3 hint lesion_cut celebrate fail click back
##   GameAudio.body("heart", 1.0)                heart: a beat happened (value = strength 0..1): lub-dub
##   GameAudio.body("breath", lung_volume)       breath: call every frame with lung volume 0..1: inhale/exhale follow its rate of change
##   GameAudio.body("twitch", 0.8)               muscle twitch thud (value 0..1)
##   GameAudio.body("gut", 0.5)                  subtle gurgle (value 0..1 = chance, rate-limited)
##   GameAudio.set_spike_focus(pop or -1)        only that population clicks (-1 = all)
##   GameAudio.ambient(true)                     very quiet lab bed (off by default)
##   GameAudio.silence_body()                    stop breath and pending beats (call when a run ends or pauses)
##   GameAudio.apply_volumes()                   after the Settings screen changes a slider
## Buses Music, SFX and Spikes are created under Master. Settings.apply() owns Master (Settings.volume);
## Music = Settings.music_volume (ambient bed), SFX = Settings.sfx_volume (UI and body), Spikes = Settings.spike_volume.
##
## Crackle: the spike path is limited three ways so 1000 spikes/s is a texture, not noise: a per-population minimum gap (bursts of one
## population still roll at up to ~80/s), a global token bucket (SPIKE_RATE clicks/s), and a gain that falls as the recent density rises.

signal event_played(name: String)

static var current: GameAudio = null

const DIR := "res://assets/audio/"
const SPIKE_VOICES := 20
const UI_VOICES := 8
const PITCHES := 8
const POP_GAP_MS := 12.0            ## a population cannot click twice inside this (a roll tops out near 80/s)
const SPIKE_RATE := 110.0           ## global clicks per second (token bucket)
const SPIKE_BURST := 14.0           ## bucket size: how many clicks may arrive at once
const DENSITY_TAU_MS := 250.0       ## time constant of the recent-spike-rate estimate
const DENSITY_REF := 60.0           ## spikes/s above which each click gets quieter (-10 dB per decade)
const HEART_GAP_S := 0.30           ## lub -> dub
const GUT_GAP_S := 1.2
const TWITCH_GAP_S := 0.08
const BREATH_IDLE_S := 0.6          ## no body("breath") for this long -> the airflow fades out
const UI_CUE := {"run": "ui_run", "pause": "ui_pause", "pass": "ui_pass", "star1": "ui_star1", "star2": "ui_star2", "star3": "ui_star3",
		"hint": "ui_hint", "lesion_cut": "ui_lesion_cut", "celebrate": "ui_celebrate", "fail": "ui_fail", "click": "ui_click", "back": "ui_back",
		"ok": "ui_click", "cut": "ui_lesion_cut", "win": "ui_pass", "star": "ui_star1"}
const BODY_KINDS := ["heart", "breath", "twitch", "gut"]

var catalog: Dictionary = {"sfx": {}}
var spike_focus := -1
var spikes_played := 0       ## clicks started (tests read it)
var spikes_dropped := 0      ## spikes limited away
var cues_played := 0
var clock_ms := -1.0         ## tests set this to drive the limiter deterministically; -1 = real clock
var _streams: Dictionary = {}
var _spike_voices: Array[AudioStreamPlayer] = []
var _ui_voices: Array[AudioStreamPlayer] = []
var _vi := 0
var _ui := 0
var _body: AudioStreamPlayer = null     ## one-shot body voice pool is _ui_voices; this is the breath loop
var _bed: AudioStreamPlayer = null
var _pop_last: Dictionary = {}          ## pop -> last click time (ms)
var _tokens := SPIKE_BURST
var _tok_t := 0.0
var _density := 0.0                     ## recent spikes per second (EMA)
var _den_t := 0.0
var _rng := RandomNumberGenerator.new()
var _pending: Array = []                ## [due_s, cue, db] scheduled one-shots (the dub of lub-dub)
var _clock_s := 0.0
var _last_gut := -10.0
var _last_twitch := -10.0
var _lung := -1.0
var _lung_s := 0.0
var _rate := 0.0                        ## smoothed lung volume rate (1/s), + inhale / - exhale
var _breath_s := -10.0
var _bed_on := false
var _tween: Tween = null


func _ready() -> void:
	current = self
	_rng.seed = 20260301
	_setup_buses()
	catalog = _load_catalog()
	for i in SPIKE_VOICES:
		_spike_voices.append(_mk("Spikes"))
	for i in UI_VOICES:
		_ui_voices.append(_mk("SFX"))
	_body = _mk("SFX")
	_body.stream = _loop_stream("breath_loop")
	_body.volume_db = -80.0
	_bed = _mk("Music")
	_bed.stream = _loop_stream("ambient_lab")
	_bed.volume_db = -80.0
	apply_volumes()


func _exit_tree() -> void:
	if current == self:
		current = null


func _mk(bus: String) -> AudioStreamPlayer:
	var p := AudioStreamPlayer.new()
	p.bus = bus
	add_child(p)
	return p


func _setup_buses() -> void:
	for n in ["Music", "SFX", "Spikes"]:
		if AudioServer.get_bus_index(n) == -1:
			AudioServer.add_bus()
			var i := AudioServer.get_bus_count() - 1
			AudioServer.set_bus_name(i, n)
			AudioServer.set_bus_send(i, "Master")


func volumes() -> Dictionary:
	return {"Music": Settings.music_volume, "SFX": Settings.sfx_volume, "Spikes": Settings.spike_volume}


func _apply_volumes() -> void:
	var v := volumes()
	for bus in v:
		var i := AudioServer.get_bus_index(bus)
		if i != -1:
			AudioServer.set_bus_volume_db(i, linear_to_db(maxf(float(v[bus]), 0.0001)))
			AudioServer.set_bus_mute(i, float(v[bus]) <= 0.0)


func _load_catalog() -> Dictionary:
	var out: Dictionary = {"sfx": {}}
	if FileAccess.file_exists(DIR + "catalog.json"):
		var j: Variant = JSON.parse_string(FileAccess.get_file_as_string(DIR + "catalog.json"))
		if j is Dictionary:
			out = j
	return out


func cue_names() -> Array:
	return (catalog.get("sfx", {}) as Dictionary).keys()


## The AudioStream for a cue (null when the file is missing). Cached.
func stream(cue: String) -> AudioStream:
	if not _streams.has(cue):
		var path := DIR + "sfx/%s.wav" % cue
		_streams[cue] = load(path) as AudioStream if ResourceLoader.exists(path) else null
	return _streams[cue]


func _loop_stream(cue: String) -> AudioStream:
	var s := stream(cue)
	if s is AudioStreamWAV:
		var w := (s as AudioStreamWAV).duplicate() as AudioStreamWAV
		w.loop_mode = AudioStreamWAV.LOOP_FORWARD
		w.loop_begin = 0
		w.loop_end = int(w.get_length() * w.mix_rate)
		return w
	return s


func _db(cue: String, fallback := -8.0) -> float:
	var m: Variant = (catalog.get("sfx", {}) as Dictionary).get(cue, {})
	return float((m as Dictionary).get("db", fallback)) if m is Dictionary else fallback


func _now_ms() -> float:
	return clock_ms if clock_ms >= 0.0 else float(Time.get_ticks_usec()) / 1000.0


func _process(delta: float) -> void:
	_clock_s += delta
	if not _pending.is_empty():
		var keep: Array = []
		for p in _pending:
			if _clock_s >= float(p[0]):
				_one_shot(String(p[1]), float(p[2]))
			else:
				keep.append(p)
		_pending = keep
	_update_breath(delta)


# ---------- spikes ----------

## One spike: a click. pop_index picks the pitch (pentatonic, so a crowd is consonant), is_inhibitory the timbre.
## Returns true if a click started (false when limited, filtered by the focus, or silent).
func spike_click(pop_index: int, is_inhibitory: bool) -> bool:
	return spike_click_at(pop_index, is_inhibitory, _now_ms())


## The same with an explicit clock in ms (the limiter's arithmetic is deterministic; tests drive it with this).
func spike_click_at(pop_index: int, is_inhibitory: bool, t_ms: float) -> bool:
	# recent density, counting every spike offered (dropped or not)
	_density = _density * exp(-(t_ms - _den_t) / DENSITY_TAU_MS) + 1000.0 / DENSITY_TAU_MS if t_ms >= _den_t else _density
	_den_t = maxf(_den_t, t_ms)
	if spike_focus >= 0 and pop_index != spike_focus:
		return false
	if t_ms - float(_pop_last.get(pop_index, -1000.0)) < POP_GAP_MS:
		spikes_dropped += 1
		return false
	_tokens = minf(SPIKE_BURST, _tokens + maxf(t_ms - _tok_t, 0.0) * SPIKE_RATE / 1000.0)
	_tok_t = maxf(_tok_t, t_ms)
	if _tokens < 1.0:
		spikes_dropped += 1
		return false
	_tokens -= 1.0
	_pop_last[pop_index] = t_ms
	spikes_played += 1
	if not is_inside_tree():
		return true
	var k := posmod(pop_index, PITCHES)
	var cue := "spike_%s_%d" % ["inh" if is_inhibitory else "exc", k]
	var s := stream(cue)
	if s == null:
		return true
	var p := _spike_voices[_vi]
	_vi = (_vi + 1) % _spike_voices.size()       # round robin: the oldest click is the one cut off
	var crowd := minf(0.0, -10.0 * log(maxf(_density / DENSITY_REF, 1.0)) / log(10.0))
	p.stream = s
	p.volume_db = _db(cue) + crowd + _rng.randf_range(-1.5, 1.0)
	p.pitch_scale = _rng.randf_range(0.97, 1.04)
	p.play()
	return true


# ---------- ui cues ----------

## Play a UI cue by name (see UI_CUE) or any catalog cue name. Unknown names do nothing. Returns whether one started.
func play_event(cue_name: String) -> bool:
	var cue := String(UI_CUE.get(cue_name, cue_name))
	if stream(cue) == null:
		return false
	_one_shot(cue, 0.0)
	cues_played += 1
	event_played.emit(cue_name)
	return true


func _one_shot(cue: String, extra_db: float, pitch := 1.0) -> void:
	var s := stream(cue)
	if s == null or not is_inside_tree():
		return
	var p := _ui_voices[_ui]
	_ui = (_ui + 1) % _ui_voices.size()
	p.stream = s
	p.volume_db = _db(cue) + extra_db
	var m: Variant = (catalog.get("sfx", {}) as Dictionary).get(cue, {})
	p.pitch_scale = pitch * (_rng.randf_range(0.96, 1.05) if m is Dictionary and bool((m as Dictionary).get("jitter", false)) else 1.0)
	p.play()


# ---------- body ----------

## Body sounds. kind: heart | breath | twitch | gut; value 0..1 (see the header). Returns whether it did something audible.
func play_body(kind: String, value: float, rate_scale := 1.0) -> bool:
	value = clampf(value, 0.0, 1.0)
	match kind:
		"heart":
			var db := linear_to_db(maxf(0.35 + 0.65 * value, 0.01))
			_one_shot("heart_lub", db)
			_pending.append([_clock_s + HEART_GAP_S, "heart_dub", db])
			cues_played += 1
			return true
		"breath":
			var dt := _clock_s - _lung_s
			if _lung >= 0.0 and dt > 0.0:
				_rate = lerpf(_rate, (value - _lung) / dt / maxf(rate_scale, 1.0), clampf(dt * 6.0, 0.0, 1.0))   # rate_scale: the run's speed, so a 10x breath still sounds like a breath   # smooth, frame-rate independent
			_lung = value
			_lung_s = _clock_s
			_breath_s = _clock_s
			return true
		"twitch":
			if _clock_s - _last_twitch < TWITCH_GAP_S:
				return false
			_last_twitch = _clock_s
			_one_shot("twitch", linear_to_db(maxf(0.3 + 0.7 * value, 0.01)))
			cues_played += 1
			return true
		"gut":
			if _clock_s - _last_gut < GUT_GAP_S or _rng.randf() > value:
				return false
			_last_gut = _clock_s
			_one_shot("gut_%d" % _rng.randi_range(0, 2), 0.0, _rng.randf_range(0.85, 1.15))
			cues_played += 1
			return true
	return false


func _update_breath(delta: float) -> void:
	if _body == null:
		return
	var active := _clock_s - _breath_s < BREATH_IDLE_S
	if not active:
		_rate = lerpf(_rate, 0.0, clampf(delta * 6.0, 0.0, 1.0))
	# a full breath in ~3 s is rate ~0.35/s: that should sit near the loop's catalog level
	var flow := clampf(absf(_rate) / 0.35, 0.0, 1.4)
	var target := -80.0 if flow < 0.04 else _db("breath_loop") + linear_to_db(flow)
	_body.volume_db = lerpf(_body.volume_db, target, clampf(delta * 8.0, 0.0, 1.0))
	_body.pitch_scale = lerpf(_body.pitch_scale, 1.0 if _rate > 0.0 else 0.72, clampf(delta * 6.0, 0.0, 1.0))   # inhale brighter, exhale lower
	if flow >= 0.04 and not _body.playing and _body.stream != null:
		_body.play()
	elif flow < 0.04 and _body.volume_db < -70.0 and _body.playing:
		_body.stop()


func quiet_body() -> void:
	_pending.clear()
	_rate = 0.0
	_lung = -1.0
	_breath_s = -10.0


# ---------- ambient ----------

func set_ambient(on: bool, fade_s := 2.0) -> void:
	_bed_on = on
	if _bed == null or _bed.stream == null or not is_inside_tree():
		return
	if _tween != null and _tween.is_valid():
		_tween.kill()
	if on:
		if not _bed.playing:
			_bed.volume_db = -60.0
			_bed.play()
		_tween = create_tween()
		_tween.tween_property(_bed, "volume_db", _db("ambient_lab"), fade_s)
	elif _bed.playing:
		_tween = create_tween()
		_tween.tween_property(_bed, "volume_db", -60.0, fade_s)
		_tween.tween_callback(_bed.stop)


# ---------- static helpers (no reference needed) ----------

static func spike(pop_index: int, is_inhibitory: bool) -> void:
	if current != null:
		current.spike_click(pop_index, is_inhibitory)


static func event(cue_name: String) -> void:
	if current != null:
		current.play_event(cue_name)


static func body(kind: String, value := 1.0, rate_scale := 1.0) -> void:
	if current != null:
		current.play_body(kind, value, rate_scale)


static func set_spike_focus(pop_index: int) -> void:
	if current != null:
		current.spike_focus = pop_index


static func ambient(on: bool) -> void:
	if current != null:
		current.set_ambient(on)


static func silence_body() -> void:
	if current != null:
		current.quiet_body()


static func apply_volumes() -> void:
	if current != null:
		current._apply_volumes()
