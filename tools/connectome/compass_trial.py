#!/usr/bin/env python3
"""Own trial harness for the hemibrain compass probe (research/atlas-a.md). Uses spike-run in plain circuit mode and reads every spike, so it can say how many cells
carry the bump (the task's 'strength' alone cannot tell one bump from one saturated cell).

A trial: a landmark cue at ring position `cue` for CUE_MS (spike_generator at 300 Hz onto the E-PG cells of that position), then nothing. Both turn sensors are constant
(BASE units each, times turn_w pA); `asym` units are added to one side and removed from the other for the whole trial.
Measures (E-PG spikes only, heading h at 22.5 h degrees):
  window means of the rate per ring position, the population-vector angle and strength, the share of spikes within +-2 positions of the peak, the number of active positions.
"""
import json, math, os, random, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import compass_circuit as cc
# the runner: $SPIKE_RUN, else the engine tree's build, else the game's copy (scripts/build-all.sh without --tests makes only that one)
BIN = os.environ.get("SPIKE_RUN") or next((b for b in (os.path.join(ROOT, "build", "engine", "spike-run"), os.path.join(ROOT, "godot", "bin", "spike-run")) if os.path.exists(b)), os.path.join(ROOT, "build", "engine", "spike-run"))
CUE_MS, BASE = 500.0, 40.0
DATA = cc.load()
EPG = [i for i, n in enumerate(DATA["neurons"]) if n["type"] == "EPG"]
RING_OF_EPG = [DATA["neurons"][i]["heading"] for i in EPG]   # heading of each E-PG cell (the variable name is from the bridge-coordinate probe)   # circuit-level neuron index of EPG k is k (EPG is the first population)


def circuit(params, cue, asym=0.0, seed=7, cue_rate=300.0):
    c = cc.build(params)
    c["seed"] = seed
    rnd = random.Random(seed * 100 + cue)
    for inp in c["inputs"]:
        if inp["name"].startswith("cue"):
            k = int(inp["name"][3:])
            if k == cue:
                t, ts = 0.0, []
                while True:
                    t += rnd.expovariate(cue_rate / 1000.0)
                    if t >= CUE_MS: break
                    ts.append(round(t, 1))
                inp["kind"] = "spike_generator"; inp["spike_times"] = sorted(set(max(0.1, x) for x in ts)); inp.pop("value", None)
            else:
                inp["targets"] = []
        elif inp["name"] == "turn_right":   # drives the left PEN cells (bump towards higher positions)
            inp["value"] = BASE + asym
        elif inp["name"] == "turn_left":
            inp["value"] = BASE - asym
    return c


def run(c, ms):
    path = os.path.join(tempfile.gettempdir(), "ctrial_%d_%d.json" % (os.getpid(), random.randrange(1 << 30)))
    json.dump(c, open(path, "w"))
    p = subprocess.run([BIN, path, str(int(ms))], capture_output=True, text=True)
    os.remove(path)
    spikes = []
    for l in p.stdout.splitlines():
        d = json.loads(l)
        if d["neuron"] < len(EPG): spikes.append((d["t"], d["neuron"]))
    return spikes, p.stderr


def profile(spikes, t0, t1):
    prof = [0.0] * 16
    for t, n in spikes:
        if t0 <= t < t1: prof[RING_OF_EPG[n]] += 1
    cnt = [sum(1 for r in RING_OF_EPG if r == k) for k in range(16)]
    return [prof[k] / cnt[k] / ((t1 - t0) / 1000.0) for k in range(16)]   # Hz per cell at each position


def analyse(prof):
    tot = sum(prof)
    if tot == 0: return {"angle": None, "strength": 0.0, "near_peak": 0.0, "active": 0, "mean_hz": 0.0, "peak_hz": 0.0}
    x = sum(p * math.cos(math.radians(22.5 * k)) for k, p in enumerate(prof)); y = sum(p * math.sin(math.radians(22.5 * k)) for k, p in enumerate(prof))
    pk = max(range(16), key=lambda k: prof[k])
    near = sum(prof[(pk + d) % 16] for d in range(-2, 3)) / tot
    return {"angle": math.degrees(math.atan2(y, x)) % 360, "strength": math.hypot(x, y) / tot, "near_peak": near,
            "active": sum(1 for p in prof if p > 0.3 * max(prof)), "mean_hz": tot / 16, "peak_hz": max(prof), "peak_pos": pk}


def wrap(d): return (d + 180) % 360 - 180


def trial(params, cue, seed=7, asym=0.0, ms=3500):
    sp, err = run(circuit(params, cue, asym, seed), ms)
    out = {"cue": cue, "seed": seed, "asym": asym, "cue_deg": 22.5 * cue}
    for name, (a, b) in {"cue": (300, 500), "early": (700, 1000), "hold": (1500, 2000), "late": (3000, 3500)}.items():
        r = analyse(profile(sp, a, b)); out[name] = r
        out[name]["err"] = abs(wrap(r["angle"] - 22.5 * cue)) if r["angle"] is not None else None
    out["profile_late"] = [round(x, 1) for x in profile(sp, 3000, 3500)]
    out["drift_deg_per_s"] = (wrap(out["late"]["angle"] - out["early"]["angle"]) / 2.3) if out["late"]["angle"] is not None and out["early"]["angle"] is not None else None
    return out


def held(t):
    h = t["late"]
    return h["angle"] is not None and h["err"] < 30 and h["strength"] > 0.5 and h["near_peak"] > 0.7 and h["mean_hz"] > 2 and h["mean_hz"] < 60


if __name__ == "__main__":
    p = {}
    for kv in sys.argv[1:]:
        k, v = kv.split("="); p[k] = type(cc.DEFAULTS[k])(v)
    for cue in (2, 6, 10, 14):
        t = trial(p, cue)
        print(cue, "held" if held(t) else "no", {k: (round(v, 2) if isinstance(v, float) else v) for k, v in t["late"].items()}, "cue-phase err", t["cue"]["err"] and round(t["cue"]["err"]))
        print("   ", t["profile_late"])
