---
title: Getting started
section: Start here
order: 1
summary: What Spike is, how to download and run it, what your computer needs, and what you see on first launch.
---

# Getting started

Spike is a game where you build small networks of spiking neurons and watch them run a body. You wire sensor cells to interneurons to motor cells, press Run, and a leg kicks, a lamprey swims or a heart speeds up. The neurons are real models from the NEST simulator, so their names and units (millivolts, milliseconds, picoamps, nanosiemens, hertz) are the ones a neuroscientist uses.

It is a game and a research tool at once. The puzzles teach one idea at a time. The same circuits can be saved as plain files and run again in Python.

> **Note:** Spike is in alpha. The foot of the title screen shows the version (v0.5.1), the build and the Godot version. It plays on Linux and on macOS (Apple Silicon). Some corners are rough. The [changelog](changelog.html) lists known gaps, and the [FAQ](faq.html) answers the common questions.

## What you do in the game

| You | What it means |
|---|---|
| Build | Start from an empty bench with the level's sensor cards on the left and motor cards on the right. Add neurons, drag connections, set weights. |
| Run | A live spike raster, voltage traces, rate gauges and the simulated body move as the circuit runs. |
| Check | A headless score on five random worlds, in the background. The verdict says what the circuit did and which part of the mechanism is missing. It never gives the answer. |
| Learn | One new idea per level, a three-tier hint ladder, a recipe (printed in Guided mode, or opened at the cost of the third star in Standard), and a lab notebook of cited findings. |
| Diagnose | A patient circuit with a hidden dead connection. You probe it and name the broken wire. |
| Dissect | A lesion lab that cuts parts and re-runs on several seeds, plus motif, burst, heart-rate-variability, phase-response, energy and reverse-correlation tools. |

A neuron here is a model of a nerve cell. A spike is the short electrical pulse it fires. A circuit is a set of neurons and the connections between them.

## System requirements

Spike ships for 64-bit Linux and for macOS on Apple Silicon. These are the facts the repository states.

| Item | Requirement |
|---|---|
| Operating system | Linux x86_64 with glibc 2.39 or newer (Ubuntu 24.04 or newer is the reference). |
| macOS | Apple Silicon (M1 or newer). The app is not notarized: on first launch open System Settings, Privacy & Security, and choose Open Anyway, or run `xattr -dr com.apple.quarantine Spike.app`. Intel Macs are not built. |
| Windows | Not available yet. The port is in progress: NEST has no official Windows build. |
| Graphics | The game is a Godot 4.7 project. It uses the Forward+ renderer on desktop and the OpenGL compatibility renderer on mobile. A graphics driver that supports Vulkan is the safe choice on desktop. |
| CPU | NEST runs on one thread. A modern single core is what matters. The biggest levels (the bosses, the mushroom body, the mean-field level) are the slowest. |

The game also runs on a handheld or phone-sized window. See [settings and performance](settings-and-performance.html).

## Download and run

1. Open the [Releases page](https://github.com/blackmetaltech/spikes/releases) and download `spike-linux-x86_64.zip`.
2. Extract it to a folder you own.
3. Run `Spike.x86_64` from that folder. If your file manager did not keep the execute bit, run `chmod +x Spike.x86_64` first.

Keep the files from the zip together. The zip holds the game (`Spike.x86_64` and `Spike.pck`), the engine library (`libspike_gd_release.so`), the headless scorer (`spike-run`) that the Check button uses, and loose `circuits/` and `catalog/` folders. If `spike-run` is missing next to the game, Check cannot score.

> **Note:** If no release is listed yet, build from source. That takes a while the first time. See [building from source](building-from-source.html).

## First launch

By default the game opens fullscreen. Esc goes back one screen from any sub-screen. Change the window mode in Settings, Display tab (desktop only).

The title screen is a plate in the style of a 19th-century histology atlas: ink and Golgi stain on paper, sienna for excitation and Prussian blue for inhibition. It has six entries.

| Entry | What it opens |
|---|---|
| I. Play | The first time, the tutorial (Plate 0). After that, the campaign map. |
| II. Laboratory | The sandbox: a free bench with no task and every part unlocked. See [the sandbox](sandbox.html). |
| III. What is real | A screen that says what is NEST, what was tuned and what is illustrative. See [what is real](what-is-real.html). |
| IV. Settings | Display, Audio, Controls and Game tabs. The Game tab holds the save buttons. |
| V. Credits | Engine, fonts, sources and licences. |
| VI. Quit | Leaves the game (not shown on a phone or on the web). |

Choose Play. On a fresh save the game starts **Plate 0: the first neuron**, an interactive tutorial that has you build your first circuit. Every step can be skipped (Skip tutorial, or Esc) and it can be replayed from the campaign header (Tutorial). At the end it asks how much help you want (Guided, Standard or Researcher, see [modes](modes.html)). After that, Play opens the campaign map on the first chapter that has an unfinished level, and the **Continue** button above the chapter tabs opens the next level you have not passed. Pick the first level, read its brief on the right, and press Build. [Your first hour](first-hour.html) walks through the tutorial and the first four levels.

## Where your progress lives

The game saves automatically, to Godot's per-user data folder for "Spike" (on Linux, normally under `~/.local/share/godot/app_userdata/Spike/`). Settings, Game shows the exact folder.

| What | File |
|---|---|
| Stars, best scores, difficulty mode, hints and variants done, and which tutorial steps and pointers you have seen | `progress.cfg` |
| Display, audio and accessibility settings | `settings.cfg` |
| The targets you pinned on the Results card | `goals.cfg` |
| The circuit you last built on each level (the sandbox is `last_free.json`) | `circuits/last_<level>.json` |
| The solution slots A, B and C of a level | `circuits/<level>/slot_A.json` (and B, C) |
| The benches a button replaced, newest ten per level | `circuits/<level>/history/` |
| Lesion tables you saved | `lesions/` |
| A log of each game start, and the notices the game raised | `logs/spike.log` (and the two before it, `spike.1.log`, `spike.2.log`) |
| Your play log (only if you turn it on) | `playlog.json`; exported on demand as a dated JSON file |

Every file is written safely (to a temporary file first, then renamed, so a crash cannot leave half a file) and keeps up to three backups (`.bak1` to `.bak3`). If a file is damaged, the game uses the newest good backup and keeps the damaged one beside it (`.corrupt-<time>`). The Settings, Game tab can export the whole save as one zip, import one, open the log folder and reset your progress. See [settings and performance](settings-and-performance.html#saves-backups-and-logs).

The game never sends anything anywhere. The optional play log stays on your device.

## Where to go next

- [Your first hour](first-hour.html): the first four levels, step by step.
- [How to read the lab](how-to-read-the-lab.html): every panel on the building screen.
- [The core loop](core-loop.html): build, run, check, verdict, stars, hints.
- [Modes](modes.html): Guided, Standard and Researcher.
- [Glossary](glossary.html): every term the game uses.
