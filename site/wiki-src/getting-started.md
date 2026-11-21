---
title: Getting started
section: Start here
order: 1
summary: What Spike is, how to download and run it, what your computer needs, and what you see on first launch.
---

# Getting started

Spike is a game where you build small networks of spiking neurons and watch them run a body. You wire sensor cells to interneurons to motor cells, press Run, and a leg kicks, a lamprey swims or a heart speeds up. The neurons are real models from the NEST simulator, so their names and units (millivolts, milliseconds, picoamps, nanosiemens, hertz) are the ones a neuroscientist uses.

It is a game and a research tool at once. The puzzles teach one idea at a time. The same circuits can be saved as plain files and run again in Python.

> **Note:** Spike is in alpha. The version string in the game reads "0.5 in progress". It plays on desktop Linux. Some corners are rough. The [changelog](changelog.html) lists known gaps, and the [FAQ](faq.html) answers the common questions.

## What you do in the game

| You | What it means |
|---|---|
| Build | Start from an empty bench with the level's sensor cards on the left and motor cards on the right. Add neurons, drag connections, set weights. |
| Run | A live spike raster, voltage traces, rate gauges and the simulated body move as the circuit runs. |
| Check | An instant headless score. The verdict says what the circuit did and which part of the mechanism is missing. It never gives the answer. |
| Learn | One new idea per level, a three-tier hint ladder, a recipe you can open at the cost of a star, and a lab notebook of cited findings. |
| Diagnose | A patient circuit with a hidden dead connection. You probe it and name the broken wire. |
| Dissect | A lesion lab that cuts parts and re-runs on several seeds, plus motif, burst, heart-rate-variability, phase-response, energy and reverse-correlation tools. |

A neuron here is a model of a nerve cell. A spike is the short electrical pulse it fires. A circuit is a set of neurons and the connections between them.

## System requirements

Spike ships for 64-bit Linux only. These are the facts the repository states.

| Item | Requirement |
|---|---|
| Operating system | Linux x86_64 with glibc 2.39 or newer (Ubuntu 24.04 or newer is the reference). |
| Windows, macOS | Not available. A Windows build is planned. What is missing is a cross-build of NEST and a port of the headless runner. |
| Graphics | The game is a Godot 4.7 project. It uses the Forward+ renderer on desktop and the OpenGL compatibility renderer on mobile. A graphics driver that supports Vulkan is the safe choice on desktop. |
| CPU | NEST runs on one thread. A modern single core is what matters. The biggest levels (the bosses, the 250-cell mushroom body) are the slowest. |

The game also runs on a handheld or phone-sized window. See [settings and performance](settings-and-performance.html).

## Download and run

1. Open the [Releases page](https://github.com/ixaxaar/spikes/releases) and download `spike-linux-x86_64.zip`.
2. Extract it to a folder you own.
3. Run `Spike.x86_64` from that folder. If your file manager did not keep the execute bit, run `chmod +x Spike.x86_64` first.

Keep the files from the zip together. The zip holds the game (`Spike.x86_64` and `Spike.pck`), the engine library (`libspike_gd_release.so`), the headless scorer (`spike-run`) that the Check button uses, and loose `circuits/` and `catalog/` folders. If `spike-run` is missing next to the game, Check cannot score.

> **Note:** If no release is listed yet, build from source. That takes a while the first time. See [building from source](building-from-source.html).

## First launch

By default the game opens fullscreen. Press Escape on the title screen to go back out of any sub-screen. Change the window mode in Settings, Display tab.

The title screen is a plate in the style of a 19th-century histology atlas: ink and Golgi stain on paper, sienna for excitation and Prussian blue for inhibition. It has six entries.

| Entry | What it opens |
|---|---|
| I. Play | The campaign map. This is where you start. |
| II. Laboratory | The sandbox: a free bench with no task and every part unlocked. See [the sandbox](sandbox.html). |
| III. What is real | A screen that says what is NEST, what was tuned and what is illustrative. See [what is real](what-is-real.html). |
| IV. Settings | Display, Audio, Controls and Game tabs. |
| V. Credits | Engine, fonts, sources and licences. |
| VI. Quit | Leaves the game. |

Choose Play. The campaign map opens on the first chapter that has an unfinished level. Pick the first level, read its brief on the right, and press Build. [Your first hour](first-hour.html) walks through it.

## Where your progress lives

The game saves automatically.

| What | File (inside Godot's per-user data folder for "Spike") |
|---|---|
| Stars, best scores, difficulty mode, hints and variants done | `progress.cfg` |
| Display, audio and accessibility settings | `settings.cfg` |
| The circuit you last built on each level | `circuits/last_<level>.json` |
| A log of the game start | `logs/spike.log` |
| Your play log (only if you turn it on) | exported on demand as JSON |

On Linux the per-user data folder is normally under `~/.local/share/godot/app_userdata/`. The game never sends anything anywhere. The optional play log stays on your device. See [settings and performance](settings-and-performance.html).

## Where to go next

- [Your first hour](first-hour.html): the first four levels, step by step.
- [How to read the lab](how-to-read-the-lab.html): every panel on the building screen.
- [The core loop](core-loop.html): build, run, check, verdict, stars, hints.
- [Modes](modes.html): Guided, Standard and Researcher.
- [Glossary](glossary.html): every term the game uses.
