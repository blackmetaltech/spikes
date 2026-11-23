---
title: The spike-run command line
section: Under the hood
order: 82
summary: Run a circuit file or score it on a level from a terminal, with the same loop, seeds and rules as the game's Check button.
---

# The spike-run command line

`spike-run` ships with the game and is built with the engine. It runs any [circuit file](circuit-is-data.html) for a number of milliseconds and prints the spikes. It also scores a circuit on a level, headless (no window), exactly as the game does. The game's Check button runs it for you in the background. See [the core loop](core-loop.html).

In the release zip it sits next to the game. From a source build it is `build/engine/spike-run`, and a copy goes to `godot/bin` when you build the extension.

## Mode 1: run a circuit

```sh
spike-run circuit.json 1000 [--threads n]
```

This loads `circuit.json`, runs 1000 ms of model time, and prints one JSON line per spike on standard output:

```json
{"neuron":0,"t":5.800000,"step":58}
```

`neuron` is the circuit-level cell index (0 to N-1 over the populations in file order). `t` is the time in ms. `step` is the NEST time stamp in steps (here 0.1 ms each). Lines are sorted by step, then by cell. A summary line goes to standard error, so standard output holds spikes only:

```
spike-run: t=100.000 ms spikes=16 checksum=76468ca69ef4ff71 dropped=0
```

The checksum is a hash of every spike. Tests use it to check that a run is reproducible. `dropped` counts spikes the engine had to drop. A file made by the golden-trace tests (with a `circuit` field) also runs.

The command line is strict about numbers. `<ms>` must be above 0 and at most 100000000. `--threads` must be a whole number from 1 to 1024, and `--every` a number above 0. A value such as `12abc` is refused, not half read. Mistakes end with a one-line message and an exit status, never a crash: 2 for a command-line mistake or a file that cannot be opened, 1 for a circuit the engine refuses (see [a circuit is data](circuit-is-data.html#limits-and-errors)) or a run that failed.

Development options for this mode, for people who build their own circuits and tasks:

| Option | What it does |
|---|---|
| `--threads n` | NEST threads. The default is 1. |
| `--sample LIST` | Also print a JSON line of values at regular times, between the spike lines. Items are comma-separated: `V_m@0` (a state variable of cell 0), `out:name` (the value of a `rate_value` output), `cnt:name` (an output spike count), `mod:name` (a neuromodulator level). |
| `--every ms` | The sampling interval for `--sample`. Default 10 ms. |
| `--set t_ms:channel=value` | Set an input channel to a value at that model time. Repeatable. |
| `--phase neuron[:I]` | Print the fixed points and onset type of a two-variable cell (the phase-plane numbers behind the Theory wing's [dynamics levels](campaign-theory.html)) to standard error, then run. |

Example: `spike-run c.json 1000 --sample V_m@0,out:rate --every 10 --set 300:cur=5`.

## Listing levels and variants

```sh
spike-run --list-tasks
spike-run --list-variants <id>
```

`--list-tasks` prints every level id, one per line, in the order the engine registers them. This build lists 79: 20 micro-puzzles, 14 tasks, 2 bosses, 6 body diagnosis cases, the Bestiary (18 levels and 7 diagnosis cases) and the Theory wing (12). `--list-variants <id>` prints the ids that level accepts for `--variant`: its challenge variants and, for a diagnosis case, the candidate answers, because a case is answered through `--variant` too. It never says which candidate is right. An unknown level id gives status 2. Both must be the only arguments on the line.

## Mode 2: score a circuit on a level

```sh
spike-run --task <id> [--circuit file.json] [--seed n | --seeds 1,2,3]
          [--lesion pre>post*k]... [--mode guided|standard|researcher]
          [--variant id] [--require any|all] [--circuits-dir dir]
          [--threads n] [--quick] [--jobs n] [--circuit-seed(s) ...]
```

| Option | What it does |
|---|---|
| `--task id` | The level to run, for example `reflex` or `m_threshold`. Required. |
| `--circuit file` | The circuit to score. Without it, the level's reference circuit is used. |
| `--seed n` or `--seeds 1,2,3` | The world seeds, whole numbers from 1 to 4000000. The default is seed 1. Each seed gives a different random world (touch times, noise) and also reseeds the circuit. |
| `--circuit-seed n` or `--circuit-seeds a,b,c` | Pin the circuit seed instead of the reseed. A whole number from 1 to 4294967295. Give one value for all seeds, or one per seed. The game uses this so the instant check scores the very circuit you watched. |
| `--mode` | Difficulty mode: `guided`, `standard` (the default) or `researcher`. See [modes](modes.html). |
| `--variant id` | A challenge variant of the level, such as `half_neurons`, or a diagnosis answer. An id the level lacks is refused with a pointer to `--list-variants`. |
| `--lesion pre>post*k` | Scale the weight of every projection from group `pre` to group `post` by `k`. `k` of 0 (or no `*k`) deletes it. Repeatable. A warning on standard error says if nothing matched. |
| `--require any` or `all` | Set the exit status from the result (below). |
| `--quick` | Run the level's short "quick" episode, the one Check uses, instead of the full episode. |
| `--jobs n` | Run seeds in parallel child processes, at most one per seed. The default (0) is the number of cores. 1 runs in-process. Whole number up to 1024. |
| `--circuits-dir dir` | Where to look for reference circuits. Otherwise `$SPIKE_CIRCUITS_DIR`, then a `circuits` or `godot/circuits` folder in the working directory or up to six folders above it, then the same from the program's own folder. |
| `--threads n` | NEST threads, 1 to 1024. |

**The seed rule.** A run's circuit seed is `1000 * seed + the circuit's own seed`. A circuit with seed 7 run on seed 1 reports `circuit_seed` 1007. The circuit seed must stay within 1 to 4294967295 (NEST's own range), so a very large `--seed` is refused with a message. A circuit with no `seed` counts as seed 1. This is the rule behind the game's Test x5, so a command-line score agrees with the game.

### What it prints

One JSON object on standard output:

| Field | Meaning |
|---|---|
| `task`, `circuit`, `mode`, `variant`, `quick` | What was run. |
| `n`, `passes`, `mean_value` | Number of seeds, how many passed, and the mean score. |
| `runs` | One entry per seed (below). |

Each entry of `runs` has `seed`, `circuit_seed`, `duration_ms`, `value` (the score, 0 to 100), `passed`, `verdict` (a plain sentence about what the circuit did), `parts` (the numbers behind the score, named for the level), `metrics` (each with `value`, `unit` and `lower_is_better`), `neurons`, `spikes`, `rate_hz_per_neuron` (the energy number behind the third star) and `hints` (each with `tier`, `text` and `target`).

> **Warning:** The `hints` list holds all three tiers, including the partial recipe of tier 3, whatever the mode. The game reveals them one at a time. To play a level without spoilers, do not read this field. The text carries no numbers by rule, but tier 3 still describes the wiring.

### Exit status

| Code | Meaning |
|---|---|
| 0 | Success. |
| 1 | The engine refused the circuit or the run failed (for example a numerical blow-up). No result is printed. The reason goes to standard error. If a seed fails, later seeds are not run. |
| 2 | Usage error: unknown option, unknown task or variant, a number that is not a number or is out of range, missing circuit, unreadable or malformed file. |
| 3 | The `--require` condition failed: `any` means at least one seed must pass, `all` means every seed must pass. |

CI and the smoke test use `--require any` to check that every level's reference circuit passes. The packaging script also runs the bundled `spike-run` as the last check on the release zip.

## Examples

Score the reference circuit of a level on three seeds and require all to pass:

```sh
spike-run --task reflex --seeds 1,2,3 --require all
```

Score your own circuit in Researcher mode, in quick form:

```sh
spike-run --task m_threshold --circuit mine.json --mode researcher --seeds 1,2,3,4,5 --quick
```

Try a lesion: cut the connection from group SN to group MN on a copy of a circuit and read the verdict:

```sh
spike-run --task reflex --circuit mine.json --lesion SN>MN
```

This is the game's [lesion lab](lesion-lab-and-analysis.html) without the table.

## From the game

In Godot, `SpikeTask.score_headless_async(circuit_json, seeds, mode, variant, circuit_seed)` starts a child `spike-run` and returns a job number. `circuit_seed` is optional (-1 means the reseed rule). If the program cannot be found or started it returns 0 and a warning lists the places searched. `score_headless_quick_async` runs the quick episode, and `score_headless_poll(job)` is how the game polls each frame. A release build must keep `spike-run` next to the extension or Check cannot work. The circuit travels in a private temporary file (in `$TMPDIR`, default `/tmp`, readable by you only) that is deleted afterwards. `headless_cancel(job)` kills the child process and its children. On Windows the runner is `spike-run.exe`, beside `Spike.exe`, and the circuit file goes in your temp folder.

## Related pages

[Use your circuit in Python](python.html), [building from source](building-from-source.html), [architecture](architecture.html), [make it yours](make-it-yours.html).
