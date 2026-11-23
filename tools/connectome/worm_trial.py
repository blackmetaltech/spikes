#!/usr/bin/env python3
"""One touch trial of the full-wiring worm probe (worm_circuit.py). FEASIBILITY PROBE for iteration A2, not a level.

Runs `spike-run` in plain circuit mode (binary: $SPIKE_RUN, else build/engine/spike-run, else godot/bin/spike-run) for SIM_MS = 1200 ms of model time:
  [100, 500) baseline (the first 100 ms are start-up transient and not counted), touch [500, 900) (400 ms, 150 Hz Poisson per sensor, see worm_circuit.py),
  [900, 1200) the 300 ms after the touch.
Returns per trial: mean rate per cell in Hz (cells present only) of the A-type motor neurons (DA, VA) and the B-type (DB, VB) in the touch window (A_touch, B_touch), the
300 ms after (A_after, B_after), both together = [500, 1200) (A, B; these two decide `selective`), the baseline (A_base, B_base), the rate of each command class over [500, 1200)
(`ints`, None if the class is silenced) and its baseline (`ints_base`), and total spikes in the whole run.
`selective(head, tail)`: head gives A >= 5 Hz and A >= 2 B; tail gives B >= 5 Hz and B >= 2 A; baselines (A and B, both trials) below 1 Hz.
Assumption: the first neuron index of a population in the spike lines is its position in meta["cells"] (circuit-level index in population order, from 0).
"""
import json, os, random, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import worm_circuit as wc
BIN = os.environ.get("SPIKE_RUN") or next((b for b in (os.path.join(ROOT, "build", "engine", "spike-run"), os.path.join(ROOT, "godot", "bin", "spike-run")) if os.path.exists(b)), os.path.join(ROOT, "build", "engine", "spike-run"))
SIM_MS = 1200
BASE, TOUCH, AFTER, RESP = (100.0, 500.0), (500.0, 900.0), (900.0, 1200.0), (500.0, 1200.0)
DATA = wc.load()
CLASS_OF = {n["name"]: n["class"] for n in DATA["neurons"]}


def run(c, ms=SIM_MS):
    path = os.path.join(tempfile.gettempdir(), "wtrial_%d_%d.json" % (os.getpid(), random.randrange(1 << 30)))
    json.dump(c, open(path, "w"))
    try:
        p = subprocess.run([BIN, path, str(int(ms))], capture_output=True, text=True)
    finally:
        os.remove(path)
    if p.returncode != 0:
        raise RuntimeError("spike-run failed: " + p.stderr[-500:])
    sp = []
    for l in p.stdout.splitlines():
        if l.startswith("{"):
            d = json.loads(l); sp.append((d["t"], d["neuron"]))
    return sp


def trial(params, where, seed=7, ms=SIM_MS):
    """where = "head" or "tail"; seed = circuit seed. The touch trains are drawn from `seed` too."""
    q = dict(params); q["touch"] = where; q["touch_seed"] = seed * 1000 + (1 if where == "head" else 2)
    c = wc.build(q); c["seed"] = seed
    cells = c["meta"]["cells"]; cl = [CLASS_OF[n] for n in cells]
    sp = run(c, ms)
    n_of = {}
    for k in cl: n_of[k] = n_of.get(k, 0) + 1

    def rate(classes, w):
        n = sum(n_of.get(k, 0) for k in classes)
        if n == 0: return None
        return sum(1 for t, i in sp if w[0] <= t < w[1] and cl[i] in classes) / n / ((w[1] - w[0]) / 1000.0)

    out = {"where": where, "seed": seed, "spikes": len(sp)}
    for tag, cs in (("A", wc.A_CLASSES), ("B", wc.B_CLASSES)):
        out[tag + "_touch"] = rate(cs, TOUCH); out[tag + "_after"] = rate(cs, AFTER); out[tag] = rate(cs, RESP); out[tag + "_base"] = rate(cs, BASE)
    out["A_late"] = rate(wc.A_CLASSES, (ms - 200.0, float(ms)))   # last 200 ms (meaningful when ms > SIM_MS)
    la = [t for t, i in sp if t >= TOUCH[0] and cl[i] in wc.A_CLASSES]
    out["lat_A"] = round(min(la) - TOUCH[0], 1) if la else None   # first A-type spike after touch onset, ms
    out["ints"] = {k: rate((k,), RESP) for k in wc.COMMAND}
    out["ints_base"] = {k: rate((k,), BASE) for k in wc.COMMAND}
    return out


def _v(x): return 0.0 if x is None else x


def head_ok(t): return _v(t["A"]) >= 5 and _v(t["A"]) >= 2 * _v(t["B"]) and _v(t["A_base"]) < 1 and _v(t["B_base"]) < 1
def tail_ok(t): return _v(t["B"]) >= 5 and _v(t["B"]) >= 2 * _v(t["A"]) and _v(t["A_base"]) < 1 and _v(t["B_base"]) < 1
def selective(head_trial, tail_trial): return head_ok(head_trial) and tail_ok(tail_trial)


if __name__ == "__main__":
    p = {}
    for kv in sys.argv[1:]:
        k, v = kv.split("="); p[k] = json.loads(v) if k == "silence" else type(wc.DEFAULTS[k])(v)
    h, t = trial(p, "head"), trial(p, "tail")
    print("head", h); print("tail", t); print("selective", selective(h, t))
