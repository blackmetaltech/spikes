# Spike audio

Neuroscientists listen to spikes: an audio monitor turns each action potential into a click. Spike does the same, plus the body.
Everything is synthesized offline in Python (numpy only: sines, filtered noise, envelopes), so it is deterministic and licence-clean.

## Pipeline
`python3 tools/audio/generate.py` writes `godot/assets/audio/sfx/*.wav` (mono 16-bit, 44.1 kHz; the ambient loop is 22.05 kHz) and
`godot/assets/audio/catalog.json` (per sound: group, level `db`, jitter, length, loop). Two runs give byte-identical files. The output
is committed; rerun only when editing sounds. `tools/audio/synth.py` is the kit (seeded noise, periodic noise for seamless loops, bells).

| Group | Sounds |
|---|---|
| spike | `spike_exc_0..7` warm (damped sine + soft octave, ~25 ms), `spike_inh_0..7` cool (higher, glassy, inharmonic, ~15 ms). Pitches are C major pentatonic, so any crowd of populations is consonant |
| body | `heart_lub`, `heart_dub`, `twitch`, `gut_0..2` (quiet), `breath_loop` (seamless airflow) |
| ui | run, pause, click, back, pass, star1..3, hint, lesion_cut, celebrate, fail |
| ambient | `ambient_lab`: 8 s seamless room tone plus faint mains hum, very quiet, off by default |

## API (`GameAudio`, godot/game/audio.gd)
Added once in main.gd (marked block). All helpers are static and no-ops without an instance.

| Call | Meaning |
|---|---|
| `GameAudio.spike(pop_index, is_inhibitory)` | one spike -> one click; pitch = `pop_index % 8`, timbre by sign |
| `GameAudio.event(name)` | `run pause pass star1 star2 star3 hint lesion_cut celebrate fail click back` (aliases `ok cut win star`); any catalog name also works |
| `GameAudio.body("heart", strength)` | a beat happened (call on each real beat event): lub, then dub 0.30 s later |
| `GameAudio.body("breath", lung_volume)` | call every frame with 0..1; inhale (brighter) and exhale (lower) follow the rate of change; fades out 0.6 s after the last call |
| `GameAudio.body("twitch", v)` / `("gut", v)` | muscle thud; subtle gurgle (v = chance, at most one per 1.2 s) |
| `GameAudio.set_spike_focus(pop)` | only that population clicks; `-1` = all |
| `GameAudio.ambient(bool)` | lab bed fade in/out |
| `GameAudio.silence_body()` | cancel pending beats and breath (run ends or pauses) |
| `GameAudio.apply_volumes()` | re-read the settings (Settings.apply calls it) |

## Crackle, not noise
Three limits keep 1000 spikes/s pleasant: a per-population minimum gap of 12 ms (one population's burst still rolls at up to ~80/s), a global token bucket
of 110 clicks/s (burst 14), and a gain that drops 10 dB per decade of recent spike rate above 60/s. 20 pooled voices, round-robin; each click gets +-1.5 dB and +-3 % pitch jitter.
Tune the constants at the top of audio.gd.

## Buses and settings
Master (`Settings.volume`), Music = ambient (`music_volume`), SFX = body and UI (`sfx_volume`), Spikes (`spike_volume`, new). Created at runtime;
a zero slider mutes the bus. The Audio tab of the Settings screen has the four sliders; values save to settings.cfg.

## Tests
`godot --headless --path godot -s tests/test_audio.gd` (also run by scripts/smoke.sh): loads every catalog sound, calls every API,
checks the limiter with a deterministic clock (1000 spikes/s -> about 120 clicks), roll rate, focus filter and bus muting.
