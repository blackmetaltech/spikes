---
title: The spike-run command line
section: Under the hood
order: 82
summary: Run a circuit file or score it on a level from a terminal, with the same loop, seeds and rules as the game's Check button.
---

# The spike-run command line

`spike-run` is a small program that ships with the game and is built with the engine. It does two jobs. It can run any [circuit file](circuit-is-data.html) for a number of milliseconds and print the spikes. And it can score a circuit on a level, headless (with no window), exactly as the game does. The game's Check button runs it for you in the background. See [the core loop](core-loop.html).

In the release zip it sits next to the game. From a source build it is `build/engine/spike-run`, and a copy goes to `godot/bin` when you build the extension.

## Mode 1: run a circuit

```sh
spike-run circuit.json 1000 [--threads n]
```

This loads `circuit.json`, runs 1000 ms of model time, and prints one JSON line per spike on standard output:

```json
{"neuron":0,"t":5.800000,"step":58}
```

`neuron` is the circuit-level cell index (0 to N-1 over the populations in file order), `t` is the time in ms, and `step` is the NEST time stamp in steps (here 0.1 ms each). Lines are sorted by step and then by cell. A summary line goes to standard error, so standard output holds spikes only:

```
spike-run: t=100.000 ms spikes=16 checksum=76468ca69ef4ff71 dropped=0
```

The checksum is a hash of every spike, used by the tests to check that a run is reproducible. A file made by the golden-trace tests (with a `circuit` field) also runs.

Development options for this mode, for people who build their own circuits and tasks:

| Option | What it does |
|---|---|
| `--threads n` | NEST threads. The default is 1. |
| `--sample LIST` | Print a JSON line of values at regular times instead of spikes. Items are comma-separated: `V_m@0` (a state variable of cell 0), `out:name` (the value of a `rate_value` output), `cnt:name` (an output spike count), `mod:name` (a neuromodulator level). |
| `--every ms` | The sampling interval for `--sample`. Default 10 ms. |
| `--set t_ms:channel=value` | Set an input channel to a value at that model time. Can be repeated. |
| `--phase neuron[:I]` | Print the fixed points and onset type of a two-variable cell (the phase-plane numbers behind the Theory wing's [dynamics levels](campaign-theory.html)), then run. |

Example: `spike-run c.json 1000 --sample V_m@0,out:rate --every 10 --set 300:cur=5`.

`spike-run --list-tasks` prints every level id, one per line, in campaign order. This build lists 78.

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
| `--seed n` or `--seeds 1,2,3` | The world seeds. The default is seed 1. Each seed gives a different random world (touch times, noise) and also reseeds the circuit. |
| `--circuit-seed n` or `--circuit-seeds a,b,c` | Pin the circuit seed instead of the reseed. Give one value for all seeds, or one per seed. The game uses this so the instant check scores the very circuit you watched. |
| `--mode` | Difficulty mode: `guided`, `standard` (the default) or `researcher`. See [modes](modes.html). |
| `--variant id` | A challenge variant of the level, such as `half_neurons`. |
| `--lesion pre>post*k` | Scale the weight of every projection from group `pre` to group `post` by `k`. `k` of 0 (or leaving out `*k`) deletes it. Repeatable. A warning on standard error says if nothing matched. |
| `--require any` or `all` | Set the exit status from the result (below). |
| `--quick` | Run the level's short "quick" episode, the one Check uses, instead of the full episode. |
| `--jobs n` | Run seeds in parallel child processes. The default is the number of cores; 1 runs in-process. |
| `--circuits-dir dir` | Where to look for reference circuits. Otherwise `$SPIKE_CIRCUITS_DIR`, then a `circuits` or `godot/circuits` folder in the working directory or up to six folders above it, then the same from the program's own folder. |
| `--threads n` | NEST threads. |

**The seed rule.** The circuit seed that a run uses is `1000 * seed + the circuit's own seed`. A circuit with seed 7 run on seed 1 reports `circuit_seed` 1007. This is the same rule as the game's Test x5, which is why a score from the command line agrees with the game.

### What it prints

One JSON object on standard output:

| Field | Meaning |
|---|---|
| `circuit`, `mode`, `quick` | What was run. |
| `n`, `passes`, `mean_value` | Number of seeds, how many passed, and the mean score. |
| `runs` | One entry per seed (below). |

Each entry of `runs` has `seed`, `circuit_seed`, `duration_ms`, `value` (the score, 0 to 100), `passed`, `verdict` (a plain sentence about what the circuit did), `parts` (the numbers behind the score, named for the level), `metrics`, `neurons`, `spikes`, `rate_hz_per_neuron` (the energy number behind the third star) and `hints`.

> **Warning:** The `hints` list holds all three tiers, including the partial recipe of tier 3, whatever the mode. The game only reveals them one at a time. If you want to play a level without spoilers, do not read this field. The text carries no numbers by rule, but tier 3 still describes the wiring.

### Exit status

| Code | Meaning |
|---|---|
| 0 | Success. |
| 1 | The circuit failed to load or the run failed. |
| 2 | Usage error: unknown option, unknown task, missing circuit, unreadable file. |
| 3 | The `--require` condition failed: `any` means at least one seed must pass, `all` means every seed must pass. |

CI and the smoke test use `--require any` to check that every level's reference circuit passes.

## Examples

Score the reference circuit of a level on three seeds and require all to pass:

```sh
spike-run --task reflex --seeds 1,2,3 --require all
```

Score your own circuit in Researcher mode, in quick form:

```sh
spike-run --task m_threshold --circuit mine.json --mode researcher --seeds 1,2,3,4,5 --quick
```

Try a lesion: cut the connection from group SN to group MN on a copy of a circuit and see what the verdict says:

```sh
spike-run --task reflex --circuit mine.json --lesion SN>MN
```

This is the same idea as the game's [lesion lab](lesion-lab-and-analysis.html), without the table.

## From the game

In Godot, `SpikeTask.score_headless_async(circuit_json, seeds, mode, variant, circuit_seed)` starts a child `spike-run` in a worker thread and returns a job number. If the program cannot be found or started it returns 0 and a warning lists the places searched. `score_headless_quick_async` runs the quick episode. The game polls the job each frame. A release build must keep `spike-run` next to the extension or Check cannot work. The circuit travels in a private temporary file that is deleted afterwards, and `headless_cancel(job)` kills the child process.

## Related pages

[Use your circuit in Python](python.html), [building from source](building-from-source.html), [architecture](architecture.html), [make it yours](make-it-yours.html).
