---
title: FAQ
section: Reference
order: 57
summary: Short answers to the questions players ask most, with links to the pages that explain them.
---

# FAQ

## The game

### Is this a real simulation?

The neurons and synapses are real NEST models. NEST is a research simulator, and a neuron in Spike runs the same equations as in NEST. The bodies (a limb, a heart, lungs, an eye, a gut) are simplified models, and the circuit sizes are small. See [what is real](what-is-real.html).

### Do I need to know neuroscience?

No. Each level teaches one idea, and every term has a line in [the glossary](glossary.html). The hint ladder helps when you are stuck. The notebook explains the biology after each pass.

### Which platforms does it run on?

Linux x86_64 with glibc 2.39 or newer. Windows is planned and not built yet. See [getting started](getting-started.html).

### How long does it take to finish?

The campaign has 79 levels in all: the body chapters (20 micro-puzzles, 14 tasks, 2 bosses and 6 diagnosis cases), the Bestiary and the Theory wing. A micro-puzzle takes minutes. A boss can take much longer. Stars give you a reason to return.

### What licence is it under?

The repository does not carry a licence file at the time of writing. The simulator, NEST, is GPL-2.0-or-later. The Credits screen lists the engine, fonts and sources and their licences. Ask in the project's issues if you need a definite answer.

### Does the game send my data anywhere?

No. The optional play log is off by default, stays on your device, and you can export or clear it. See [settings and performance](settings-and-performance.html).

## Playing

### Why can't I open a level?

A level opens when the one before it on its chapter's main path is passed. A chapter opens when the previous chapter's main path is passed, or the one two before it. The card says "Pass X first". The Bestiary and the Theory wing open when you pass the Invertebrates spine, and some Bestiary chapters wait for a level or chapter they build on (the card says which; Guided mode drops those waits). The red NEEDS chips on a card are advice about concepts you have not met, not locks. See [the core loop](core-loop.html).

### Why is a tool missing, such as inhibitory neurons or Analysis?

Tools open as the campaign needs them: inhibitory neurons, the parts bin and noise inputs at the first level that needs them, and Analysis, Load, Save, the JSON copy and paste and Import from a passed level at the Spinal cord chapter. The More menu lists what is still to come. The [sandbox](sandbox.html) has everything open.

### What is the difference between Run, Check and Test x5?

Run is live on the bench. Check scores headless on five worlds (seeds 101 to 105) with the quick episode, and passing all five earns the second star. Test x5 (on the bar once you have passed, and in More) does the same on the full episode, live, with a progress card. See [the core loop](core-loop.html).

### My circuit passed on Check but not on Test x5.

Check runs the level's quick episode and Test x5 the full one, on the same five worlds. A circuit that sits on an edge can pass one and fail the other, or fail on some seeds. Move weights toward the middle of the range that works. See [tips](tips.html).

### I used the recipe. Can I still get three stars?

Not in Standard mode. Pressing **Show recipe** caps that level at two stars. In Guided mode the recipe is free, but Guided stops at two stars anyway. Researcher mode has no recipe, and its passes also earn a seal.

### Why does my copy of the reference circuit give no stars?

A circuit that equals the level's reference file, or that the Reference button filled in, earns no stars and unlocks nothing. Build your own with **Empty bench**. Hand-built circuits are never flagged. See [stars, par and hints](stars-and-hints.html).

### Why is the third star so hard?

It needs all five worlds of Check and then the level's challenge, which is written on the card (by default, par neurons or fewer). Pass first, then remove neurons one at a time. The lesion lab shows which neurons do nothing. See [the lesion lab](lesion-lab-and-analysis.html).

### Are the Results histograms other players?

No. They are generated variants of the reference circuit. The card says so. See [the lesion lab and analysis tools](lesion-lab-and-analysis.html).

### The verdict does not give numbers. Why?

By design. Verdicts and hints name the mechanism that is missing, not the numbers to type. The numbers are in the recipe. See [contributing](contributing.html).

### I am stuck on a diagnosis case.

Probe with scopes first (three probes are free). A cut that changes nothing is a finding. A wrong guess costs 15 points and a probe beyond the free ones costs 3. The third star needs the right answer on the first try after a probe that tells the candidates apart. REVEAL ANSWER appears after three wrong guesses, and a revealed case can earn one star at most. See [diagnosis cases](diagnosis-cases.html).

### A level says it is not installed.

The campaign map draws a dashed ghost for a planned level that this build does not have. Rebuild the extension, or update the game.

## Technical

### Can I use my circuit outside the game?

Yes. A circuit is a JSON file that the game, the C++ engine and the Python package all read. A circuit built in the game runs in PyNEST with identical spikes. See [a circuit is data](circuit-is-data.html), [Python](python.html) and [spike-run](spike-run.html).

### Can I add my own neuron model?

Yes, with NESTML. See [make it yours](make-it-yours.html).

### Can I add a level?

Yes. A level is a C++ file with a registration line, a reference circuit and a table of cheap circuits that must fail. See [make it yours](make-it-yours.html).

### Where is my save?

In Godot's per-user data folder for "Spike" (on Linux, `~/.local/share/godot/app_userdata/Spike/`). Settings, Game prints the folder and has **Export save**, **Import save** and **Open log folder**. See [settings and performance](settings-and-performance.html#saves-backups-and-logs).

### How do I reset my progress?

Settings, Game tab, **Reset progress**. A card asks first. Every star, score and unlock goes and the mode returns to Standard, but your circuits stay. The old progress file is kept beside the new one as `progress.cfg.reset-<time>`, so it can be put back by hand while the game is closed.

### How do I move my save to another computer?

Settings, Game tab, **Export save** writes one zip (`spike-save-<time>.zip`, in your Documents folder) with your progress, settings, pinned goals, play log, circuit autosaves and solution slots. On the other machine use **Import save** and pick the zip. Every file in it is checked first; if one is bad, nothing is imported and the message says why.

### The game says my save was damaged.

The game keeps three backups of each file and writes safely, so this is rare. It moved the damaged file aside as `<name>.corrupt-<time>`, used the newest good backup, and told you where the damaged file is. If no backup could be read, saving to that file is switched off for the session so nothing overwrites it; copy the file somewhere safe and ask in the issues. A damaged circuit for one level is renamed `last_<level>.json.bad` and that level starts on an empty bench.

### I lost my circuit after pressing Empty bench, Reference or Load.

More, **Restore previous bench** brings it back. The game keeps the newest ten benches a button replaced, per level. Undo (Ctrl+Z) goes back through your edits, and the solution slots (Save to A, B, C) keep circuits you choose.

### A banner says "The engine rejected the circuit".

A value is outside what the engine allows (for example a synaptic weight above 500 nS onto a conductance-based cell, a delay above 1000 ms) or the circuit is too large. The banner names the place and the limit. The game also refuses, before the engine, a pasted or loaded circuit with more than 2000 neurons, a group above 400 neurons, or damaged fields. See [how to read the lab](how-to-read-the-lab.html#when-the-lab-refuses-a-circuit) for the limits.

### My score stops at 49.

The circuit brings a stimulus of its own: an input the level does not drive. Delete it and wire the sensors the brief names. See [the core loop](core-loop.html).

### The game says the extension is not loaded.

The message reads "The spike-gd GDExtension is not loaded. Run scripts/build-gd.sh." It appears when you run from source without building the extension. The release zip includes it. See [building from source](building-from-source.html).

### Check does nothing or says it could not start.

Check needs the headless scorer, `spike-run`, next to the extension. Keep the files of the zip together. If the zip lost the executable bit, the game tries to set it and otherwise says "Check cannot run: ... is not executable" with the `chmod +x` command to use. A build run from source needs `scripts/build-gd.sh` to put `spike-run` in `godot/bin`. A toast reading "Check timed out" means a cell model stalled for 60 seconds, usually from a huge weight or current.

### Why does the game feel slow?

NEST runs on one thread. See the performance section of [settings and performance](settings-and-performance.html).

### I found a bug or a wrong fact.

Report it in [the issue tracker](https://github.com/ixaxaar/spikes/issues). Include `logs/spike.log` and the level. See [contributing](contributing.html).

## Related pages

- [Getting started](getting-started.html)
- [Tips and common mistakes](tips.html)
- [Glossary](glossary.html)
