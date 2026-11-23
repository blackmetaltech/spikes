---
title: Settings and performance
section: Reference
order: 56
summary: Every setting in the game, what it does, where your data is saved, and how to keep the simulation running at the speed you ask for.
---

# Settings and performance

Open Settings from the title screen (**IV. Settings**). Four tabs: Display, Audio, Controls and Game. Every change applies and saves at once.

## Display

| Setting | Options | Notes |
|---|---|---|
| Window mode | Windowed, Fullscreen, Exclusive | Desktop only (absent on a phone and the web). Default **Fullscreen**. |
| V-sync | Off, On, Adaptive | Default On. Stops tearing. |
| Frame cap | None, 30, 60, 90, 120, 144, 240 | Default None. An unfocused window sleeps at 10 frames per second. After a while with nothing happening, the game drops to 30. |
| Layout | Auto, Desktop, Handheld, Phone | Auto picks from the device and window shape, and the row says which. |
| UI scale | Auto, 80%, 100%, 125%, 150%, 175%, 200% | Scales every control. |
| Text size | Normal, Large, Larger, Huge | Scales text only. UI scale and text size together are capped so the lab still fits a 1280 x 720 area. When the cap bites, the row gives the limit. |
| Reduced motion | On or off | No sliding or drifting. Menus fade instead. |
| Colour-blind safe colours | On or off | Sienna and Prussian blue already stay apart for every common colour vision type. This also separates them in lightness (an Okabe-Ito pair). |
| Credits and licences | Button | Engine, fonts and sources. |

### Layouts

| Layout | When Auto picks it |
|---|---|
| Desktop | A large window. The lab shows two columns. |
| Handheld | A handheld device (a Steam Deck is detected), or a window with a long side under 1500 px and a short side under 900 px. |
| Phone | A phone, or a window with a short side under 600 px, a width under 820 px, or a height more than 1.3 times its width. The lab uses tabs and a bottom sheet. |

The lab also folds to three tabs (Circuit, World, Scopes) when the window is narrower than 1180 px.

## Audio

| Slider | What it sets | Default |
|---|---|---|
| Master | Overall volume | 0.8 |
| Spike clicks | The click of every spike | 0.8 |
| Body and UI | The heartbeat, breath, muscle, gut and interface sounds | 0.9 |
| Ambience | A quiet room tone | 0.7 |

Every spike is a soft click: warm for excitatory neurons, cool for inhibitory ones, pitched by population. Dense firing thins into a crackle. Select a neuron group on the bench and only its spikes click. A script synthesises the sounds. None are recorded.

## Controls

A read-only list of keys, grouped as Keyboard, Gamepad, and Touch and mouse. It covers the lab shortcuts (run, Check, reset, Undo, Redo, copy, paste, duplicate, select all, delete, zoom, fit, pan) and the bench-cursor keys and buttons for building without a mouse. See [keyboard and controls](controls.html).

## Game

| Setting | What it does |
|---|---|
| Mode | Guided, Standard or Researcher. Part of your save. Switch any time, free of penalty. Only the star labels say which mode earned a result. See [modes](modes.html). |
| Record my play | Off by default. Keeps, on this device only, the time per level, runs, hint tiers, quits and edits before your first pass. Nothing is sent. |
| EXPORT JSON | Writes the play log to a dated file (`spike-playlog-<time>.json`) in your Documents folder (the game's data folder if there is none) and says where. |
| CLEAR | Asks first, then deletes the play log. Your progress is not touched. |
| Export save | Packs your whole save into `spike-save-<time>.zip` in your Documents folder (or the data folder) and says where. |
| Import save | Opens a file dialog for such a zip. |
| Open log folder | Opens the folder with `spike.log` in your file manager. |
| Reset progress | Asks first, then restarts your progress from nothing. |

## Saves, backups and logs

The game writes to Godot's per-user data folder for "Spike". On Linux that is normally under `~/.local/share/godot/app_userdata/Spike/`. The Game tab prints the exact folder.

| File or folder | Content |
|---|---|
| `progress.cfg` | Best scores, stars, mode, hint tiers used, recipes used, variants done, diagnosis state, and which tutorial steps and pointers you have seen. |
| `settings.cfg` | Everything on the Display and Audio tabs and the play-log switch. |
| `goals.cfg` | The targets you pinned on the Results card. |
| `playlog.json` | The play log, only if you turned it on. |
| `circuits/last_<level>.json` | Your latest circuit on each level, saved after every edit (`last_free.json` is the sandbox). |
| `circuits/<level>/slot_A.json` (also B, C) | The level's solution slots. |
| `circuits/<level>/history/` | The newest ten benches that a button replaced (More, Restore previous bench). |
| `lesions/` | Lesion tables saved with Save files. |
| `logs/spike.log` | The log of the last start: version, build, operating system, graphics adapter, window and layout, and every notice raised. The two starts before it are `spike.1.log` and `spike.2.log`. Attach `spike.log` to a bug report. |

**How files are written.** Every file is written to a temporary file, read back and renamed over the old one. A crash or power cut leaves the old file or the new one, never half of one. Each file keeps three older copies (`.bak1`, `.bak2`, `.bak3`). Circuit autosaves make a new backup every couple of minutes at most.

**A damaged file.** If a save file will not parse, is empty or is cut short, the game moves it aside as `<name>.corrupt-<time>` (never deleted), uses the newest good backup and tells you: "... was damaged (...). The last good backup (N saves ago) was used. The damaged file is kept at ...". The message shows as a banner and on the title screen. If no backup is usable either, the damaged file stays in quarantine, saving to that file is switched off for the session so it cannot be overwritten, and the message says where the file is so you can copy it to safety. A damaged circuit autosave is renamed `last_<level>.json.bad` and the level opens on an empty bench.

**A newer save.** The game reads a file written by a newer version but never writes it, and a notice says so. The newer game can still use it. **A disk that will not take a write** (read-only, full) raises one notice per file, naming the path.

**Export and import.** Export save packs `progress.cfg`, `settings.cfg`, `goals.cfg`, the play log, every circuit autosave and every solution slot into one zip with a small manifest. Import save checks every file in the zip before replacing anything. It must be a Spike save of this version or older, and every file must be sound. Otherwise nothing is imported and the message says why ("not a zip file", "not a Spike save (no spike-save.json)", "this save was made by a newer version of the game", "<file> is not usable (...)", "unexpected file in the save: ..."). The files it replaces keep their backups. A confirm card appears first.

**Reset progress.** Every star, score and unlock goes, and the mode returns to Standard. Your circuits stay. The old progress file is renamed `progress.cfg.reset-<time>` beside the new one, not deleted, so you can put it back by hand while the game is closed. The tutorial counts as unseen afterwards.

**The log folder.** Open log folder opens `logs/` in your file manager. If it does not open, look in the data folder named above.

## Performance

The simulation is NEST, running inside the game. It steps your circuit 1 ms of model time at a time, and steps the body too. Three things decide how fast it feels.

**The frame budget.** At 1x speed the game spends about 9 ms of each frame stepping the simulation. Above 1.5x, on Skip, or when the run falls behind, it may spend up to 70% of a frame, up to 60 ms. You get the speed you ask for only if the engine keeps up. The game measures its throughput, and speeds above what it can deliver do not count as faster.

**The circuit size.** Cost grows with the number of cells and connections. Most levels run far faster than real time. The big ones are the bosses (about 75 to 80 cells and a long episode), the fly mushroom body (up to a few hundred cells) and the mean-field level (250 cells).

**One thread.** NEST runs on one thread in the shipped game. A fast single core beats many cores.

### If the game feels slow

- Lower the speed if the picture stutters. At high speed the traces show fewer samples per second of model time. That is normal.
- Set a frame cap (60) and keep V-sync on. A capped frame leaves more time for each step.
- Use **Check** instead of watching a long episode. It runs in a separate process (the bundled `spike-run`) on the level's quick episode, so it does not slow the live view. Its five worlds take longer on the big levels.
- Close trace strips you do not need. Each open trace reads a cell's voltage every step.
- Keep the circuit lean. A level's challenge usually asks for fewer neurons anyway.

## Related pages

- [Keyboard and controls](controls.html)
- [FAQ](faq.html)
- [Building from source](building-from-source.html)
