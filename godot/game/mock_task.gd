class_name MockTask
extends RefCounted
## Synthetic stand-in for the body-system tasks the engine does not have yet (SP_MOCK=1, dev only). It answers the same questions as SpikeTask
## (title, chapter, view, par_neurons, timeline, bands, modulators, body().readouts, score) with readouts that are plain functions of time, so
## the body views, the perturbation timeline, the dashboard and the lesion lab can be designed and looked at against the contract.
## It runs on the reflex reference circuit (the engine only has to step something). It is never offered unless SP_MOCK is set.
##
## Readout names the views understand (the real tasks should use them, or the view falls back to a generic read-out list):
##   limb       angle_deg target_deg ag_act ant_act load        heart      hr_bpm beat (0..1 phase) map_mmhg stroke_ml
##   lungs      volume (0..1) rr_per_min co2_mmhg               eye        pupil_mm light (0..1)
##   gut        bolus (0..1) seg0..seg7 (0..1 contraction)      dashboard  any numbers; bands() name the gauges
const IDS := ["mock_limb", "mock_heart", "mock_lungs", "mock_eye", "mock_gut", "mock_dash"]
const REF := "reflex_arc.json"
const DEF := {
	"mock_limb": {"title": "Posture (mock)", "chapter": "spinal", "view": "limb", "dur": 30000.0, "par": 7, "mods": [],
		"events": [[6000.0, 600.0, "push"], [14000.0, 600.0, "push"], [22000.0, 900.0, "heavy push"]],
		"bands": [["error_deg", "deg", -6.0, 6.0]]},
	"mock_heart": {"title": "Autonomic heart rate (mock)", "chapter": "heart", "view": "heart", "dur": 40000.0, "par": 8, "mods": ["NE", "ACh"],
		"events": [[10000.0, 12000.0, "exercise"], [30000.0, 6000.0, "recovery"]],
		"bands": [["hr_bpm", "bpm", 60.0, 80.0], ["map_mmhg", "mmHg", 80.0, 100.0]]},
	"mock_lungs": {"title": "Breathing (mock)", "chapter": "breathing", "view": "lungs", "dur": 45000.0, "par": 12, "mods": [],
		"events": [[18000.0, 14000.0, "CO2 rising"]],
		"bands": [["rr_per_min", "/min", 12.0, 20.0], ["co2_mmhg", "mmHg", 35.0, 45.0]]},
	"mock_eye": {"title": "Pupil light reflex (mock)", "chapter": "autonomic", "view": "eye", "dur": 30000.0, "par": 9, "mods": [],
		"events": [[5000.0, 4000.0, "light flash"], [16000.0, 9000.0, "darkness"]],
		"bands": [["pupil_mm", "mm", 2.0, 8.0]]},
	"mock_gut": {"title": "Peristalsis (mock)", "chapter": "autonomic", "view": "gut", "dur": 40000.0, "par": 14, "mods": ["ACh"],
		"events": [[4000.0, 2000.0, "bolus enters"]],
		"bands": [["bolus", "frac", 0.0, 1.0]]},
	"mock_dash": {"title": "Fight or flight (mock)", "chapter": "autonomic", "view": "dashboard", "dur": 60000.0, "par": 20, "mods": ["NE", "ACh", "DA"],
		"events": [[12000.0, 20000.0, "threat cue"], [40000.0, 12000.0, "safe again"]],
		"bands": [["hr_bpm", "bpm", 60.0, 100.0], ["map_mmhg", "mmHg", 80.0, 110.0], ["pupil_mm", "mm", 2.5, 6.0], ["gut_motility", "%", 40.0, 100.0]]}}

var _id := ""
var _t := 0.0
var _seed := 1
var _beat := 0.0
var _ok_ms := 0.0     # time spent inside the band, summed over the judged traces
var _all_ms := 0.0
var _ro := {}

static func has(id: String) -> bool: return DEF.has(id) and OS.get_environment("SP_MOCK") != ""

func make(id: String) -> bool:
	if not DEF.has(id): return false
	_id = id; reset(1); return true

func reset(seed: int) -> void:
	_seed = seed; _t = 0.0; _beat = 0.0; _ok_ms = 0.0; _all_ms = 0.0; _ro = _readouts(0.0)

func step_ok(_sim) -> bool: return true

func step(sim, ms: float) -> void:
	if sim != null: sim.step(ms)
	_t += ms; _ro = _readouts(_t)
	if _ro.has("hr_bpm"): _beat = fposmod(_beat + float(_ro["hr_bpm"]) / 60000.0 * ms, 1.0); _ro["beat"] = _beat
	for b in bands():
		var v := float(_ro.get(b["name"], 0.0)); _all_ms += ms
		if v >= float(b["lo"]) and v <= float(b["hi"]): _ok_ms += ms

func id() -> String: return _id
func title() -> String: return str(DEF[_id]["title"])
func brief() -> String: return "Synthetic task for designing the %s view. The numbers are scripted, not simulated." % str(DEF[_id]["view"])
func chapter() -> String: return str(DEF[_id]["chapter"])
func view() -> String: return str(DEF[_id]["view"])
func par_neurons() -> int: return int(DEF[_id]["par"])
func duration_ms() -> float: return float(DEF[_id]["dur"])
func sensor_channels() -> PackedStringArray: return PackedStringArray(["touch"])
func motor_channels() -> PackedStringArray: return PackedStringArray(["withdraw"])
func modulators() -> PackedStringArray: return PackedStringArray(DEF[_id]["mods"])
func reference_circuit() -> String: return REF
func field(_x: float, _y: float) -> float: return 0.0
func bounds() -> Rect2: return Rect2(-1, -1, 2, 2)
func timeline() -> Array:
	var out := []
	for e in DEF[_id]["events"]: out.append({"t_ms": e[0], "dur_ms": e[1], "label": e[2]})
	return out
func bands() -> Array:
	var out := []
	for b in DEF[_id]["bands"]: out.append({"name": b[0], "unit": b[1], "lo": b[2], "hi": b[3]})
	return out

func body() -> Dictionary:
	return {"points": PackedVector2Array([Vector2(0, 0), Vector2(0.5, 0), Vector2(1, 0)]), "heading": 0.0, "readouts": _ro.duplicate()}

func score() -> Dictionary:
	var frac := _ok_ms / maxf(_all_ms, 1.0); var v := 100.0 * frac
	var done := _t >= duration_ms() - 1.0
	var passed := done and frac > 0.7
	return {"value": v, "passed": passed, "parts": {"in_band_pct": snappedf(v, 0.1)},
		"verdict": "Mock score: %d%% of the episode inside the tolerance bands." % int(v)}

## 0..1 bump over [t0, t0 + dur] with soft edges.
func _on(t: float, t0: float, dur: float, edge := 1500.0) -> float:
	return clampf((t - t0) / edge, 0.0, 1.0) * clampf((t0 + dur - t) / edge, 0.0, 1.0)

func _ev(t: float, label: String) -> float:
	for e in DEF[_id]["events"]:
		if e[2] == label: return _on(t, e[0], e[1])
	return 0.0

func _readouts(t: float) -> Dictionary:
	var r := {}; var s := t * 0.001
	match _id:
		"mock_limb":
			var push := 0.0
			for e in DEF[_id]["events"]: push += 26.0 * _on(t, e[0], e[1], 120.0) * (1.4 if e[2] == "heavy push" else 1.0) * exp(-maxf(t - e[0], 0.0) / 1400.0)
			var err := push * cos(s * 7.0) * 0.5 + sin(s * 3.1) * 2.0
			r = {"angle_deg": 90.0 + err, "target_deg": 90.0, "error_deg": err, "ag_act": clampf(0.3 + err / 40.0, 0, 1), "ant_act": clampf(0.3 - err / 40.0, 0, 1), "load": 0.4 + 0.25 * _on(t, 22000.0, 900.0)}
		"mock_heart":
			var ex := _ev(t, "exercise"); var rec := _ev(t, "recovery")
			var hr := 68.0 + 52.0 * ex + 14.0 * rec * 0.0 + 2.2 * sin(s * 0.9)
			if t > 22000.0 and t < 36000.0: hr = 68.0 + 52.0 * exp(-(t - 22000.0) / 5200.0) * (1.0 if t > 22000.0 else 0.0) + 2.0 * sin(s)
			r = {"hr_bpm": hr, "map_mmhg": 92.0 + 22.0 * ex + 3.0 * sin(s * 1.7), "stroke_ml": 70.0 + 15.0 * ex, "NE": 0.2 + 0.7 * ex, "ACh": 0.7 - 0.5 * ex}
		"mock_lungs":
			var co2 := 40.0 + 14.0 * _ev(t, "CO2 rising") + 1.0 * sin(s * 0.5)
			var rr := 14.0 + 0.55 * (co2 - 40.0) + 0.5 * sin(s)
			r = {"volume": 0.5 - 0.5 * cos(TAU * s * rr / 60.0), "rr_per_min": rr, "co2_mmhg": co2, "ventilation": 6.0 + 4.0 * (co2 - 40.0) / 14.0}
		"mock_eye":
			var light := 0.45 + 0.55 * _ev(t, "light flash") - 0.4 * _ev(t, "darkness")
			r = {"light": light, "pupil_mm": 7.5 - 5.4 * light + 0.1 * sin(s * 2.0)}
		"mock_gut":
			var bolus := clampf((s - 6.0) / 30.0, 0.0, 1.0)
			r = {"bolus": bolus}
			for k in 8: r["seg%d" % k] = clampf(1.0 - absf(bolus * 8.0 + 0.5 - float(k)) * 0.6, 0.0, 1.0) * (0.0 if s < 5.0 else 1.0)
		"mock_dash":
			var th := _ev(t, "threat cue"); var safe := _ev(t, "safe again")
			var sym := th * (1.0 - 0.6 * safe)
			r = {"hr_bpm": 66.0 + 48.0 * sym + 2.0 * sin(s), "map_mmhg": 90.0 + 26.0 * sym + 2.0 * sin(s * 1.3), "pupil_mm": 3.2 + 3.6 * sym, "gut_motility": 90.0 - 70.0 * sym,
				"sympathetic_hz": 4.0 + 30.0 * sym, "parasympathetic_hz": 18.0 - 14.0 * sym, "NE": 0.15 + 0.8 * sym, "ACh": 0.6 - 0.45 * sym, "DA": 0.3 + 0.1 * sin(s * 0.4)}
	return r
