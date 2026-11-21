---
title: Settings and performance
section: Reference
order: 56
summary: Every setting in the game, what it does, where your data is saved, and how to keep the simulation running at the speed you ask for.
---

# Settings and performance

Open Settings from the title screen (**IV. Settings**). It has four tabs: Display, Audio, Controls and Game. Every change is applied and saved at once.

## Display

| Setting | Options | Notes |
|---|---|---|
| Window mode | Windowed, Fullscreen, Exclusive | Desktop only. The default is **Fullscreen**. |
| V-sync | Off, On, Adaptive | Default On. Keeps the picture from tearing. |
| Frame cap | None, 30, 60, 90, 120, 144, 240 | Default None. When the window is not focused the app sleeps at 10 frames per second. |
| Layout | Auto, Desktop, Handheld, Phone | Auto picks from the device and the window shape. The row says what it picked. |
| UI scale | Auto, 80%, 100%, 125%, 150%, 175%, 200% | Scales every control. |
| Text size | Normal, Large, Larger, Huge | Scales text only. |
| Reduced motion | On or off | No sliding or drifting. Menus fade instead. |
| Colour-blind safe colours | On or off | Sienna and Prussian blue already stay apart for every common colour vision type. This also pulls them apart in lightness. |
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

Every spike is a soft click: warm for excitatory neurons, cool for inhibitory ones, with the pitch set by the population. Dense firing is thinned into a crackle. Select a neuron group on the bench and only its spikes click. The sounds are synthesised by a script and not recorded.

## Controls

A read-only list of the keys. See [keyboard and controls](controls.html).

## Game

| Setting | What it does |
|---|---|
| Mode | Guided, Standard or Researcher. Part of your save. Switch any time. See [modes](modes.html). |
| Record my play | Off by default. Keeps, on this device only, the time per level, runs, hint tiers, quits and edits before your first pass. Nothing is ever sent. |
| EXPORT JSON | Writes the play log to a file and tells you where. |
| CLEAR | Deletes the play log. |

## Where data is saved

The game writes to Godot's per-user data folder for "Spike". On Linux that is normally under `~/.local/share/godot/app_userdata/`.

| File | Content |
|---|---|
| `progress.cfg` | Best scores, stars, mode, hint tiers used, recipes used, variants done. |
| `settings.cfg` | Everything on this page except the mode. |
| `circuits/last_<level>.json` | Your latest circuit on each level, saved after every edit. |
| `logs/spike.log` | The log of the last start. Include it with a bug report. |

To reset your progress, delete `progress.cfg` while the game is closed. The game has no reset button.

## Performance

The simulation is NEST, running inside the game. It steps your circuit 1 ms of model time at a time and also steps the body. Three things decide how fast it feels.

**The frame budget.** At 1x speed the game spends about 9 ms of each frame stepping the simulation. When you ask for a speed above 1.5x, or Skip, or the run falls behind, it may spend up to 70% of a frame, up to 60 ms. The speed you ask for is delivered only if the engine can keep up. The game measures its throughput and the speeds above what it can deliver do not count as faster.

**The circuit size.** The cost grows with the number of cells and connections. Most levels run far faster than real time. The big ones are the bosses (about 75 to 80 cells and a long episode), the fly mushroom body (up to a few hundred cells), and the mean-field level (250 cells).

**One thread.** NEST runs on one thread in the shipped game. A fast single core helps more than many cores.

### If the game feels slow

- Lower the speed if the picture stutters. At high speed the traces show fewer samples per second of model time, which is normal.
- Set a frame cap (60) and keep V-sync on. A capped frame leaves more time for each step.
- Use **Check** instead of watching a long episode. It runs in a separate process, and when several seeds are scored (the command line) they run in parallel. Check takes under a second on the worm and lamprey levels and about five seconds on the bosses.
- Close the trace strips you do not need. Each open trace reads the voltage of a cell every step.
- Keep the circuit lean. Fewer neurons is also the par star.

## Related pages

- [Keyboard and controls](controls.html)
- [FAQ](faq.html)
- [Building from source](building-from-source.html)
