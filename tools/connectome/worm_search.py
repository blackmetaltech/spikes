#!/usr/bin/env python3
"""Random search over scales, signs and cell constants of the full-wiring worm (worm_circuit.py) for selective touch responses (worm_trial.selective), two stages.
FEASIBILITY PROBE for iteration A2, not a level.

  python3 tools/connectome/worm_search.py N SEED [key=value ...]    key=value fixes a parameter (for example cross=1, sign_AVA=-1, scale_elec=0). Workers: $JOBS, else every CPU.
Stage 1: N draws, one head and one tail trial each (circuit seed 7) -> out/worm_SEED.jsonl.stage1 (every draw in brief: parameters, A and B rates of both trials, head-selective,
tail-selective, selective).
Stage 2: for each selective draw: repeat on circuit seeds 8 and 9, then silence each command class in turn (AVA, AVB, AVD, AVE, PVC; seed 7): is the head response abolished
(A below 1 Hz), the tail response (B below 1 Hz), and the rates -> out/worm_SEED.jsonl. At the end a table per sign pattern (sign_AVA..PVC, cross): draws and selective ones.

Ranges (justification: worm.py uses 25 pA per synapse, tuned for 17 cells with per-class V_th; the plain iaf_psc_alpha needs 15 mV to fire, one 25 pA x 10-synapse input is a few mV):
  scale_chem  log-uniform 5 to 125 pA per synapse (a factor 5 either side of worm.py's 25)
  scale_elec  0 half the time, else uniform 0.25 to 1 times scale_chem
  sign_<CLASS> each +1 or -1 at random, but all +1 in a quarter of the draws;  cross 0 or 1
  I_e         uniform 100 to 330 pA (rheobase of the default cell is about 375 pA, so cells are silent at rest)
  tau_syn_ex, tau_syn_in from {2, 5, 15} ms;  min_count from {1, 2, 3, 5}
"""
import json, os, random, sys
from multiprocessing import Pool
import worm_trial as wt
import worm_circuit as wc

SIGNS = ["sign_" + c for c in wc.COMMAND]


def draw(rng, fixed):
    p = {"scale_chem": round(10 ** rng.uniform(0.699, 2.097), 2)}   # 5 .. 125
    p["scale_elec"] = 0.0 if rng.random() < 0.5 else round(p["scale_chem"] * rng.uniform(0.25, 1.0), 2)
    if rng.random() < 0.25:
        for k in SIGNS: p[k] = 1
    else:
        for k in SIGNS: p[k] = rng.choice([1, -1])
    p.update(cross=rng.choice([0, 1]), I_e=round(rng.uniform(100, 330), 1), tau_syn_ex=rng.choice([2.0, 5.0, 15.0]), tau_syn_in=rng.choice([2.0, 5.0, 15.0]),
             min_count=rng.choice([1, 2, 3, 5]))
    p.update(fixed)
    return p


def pattern(p): return tuple(p.get(k, wc.DEFAULTS[k]) for k in SIGNS) + (p.get("cross", wc.DEFAULTS["cross"]),)


def r(x): return None if x is None else round(x, 2)


def stage1(p):
    h, t = wt.trial(p, "head", 7), wt.trial(p, "tail", 7)
    row = {"params": p, "head": {"A": r(h["A"]), "B": r(h["B"])}, "tail": {"A": r(t["A"]), "B": r(t["B"])},
           "head_selective": wt.head_ok(h), "tail_selective": wt.tail_ok(t), "selective": wt.selective(h, t)}
    return row


def stage2(p):
    out = {"params": p, "seeds": {}, "ablation": {}}
    for s in (8, 9):
        h, t = wt.trial(p, "head", s), wt.trial(p, "tail", s)
        out["seeds"][s] = {"selective": wt.selective(h, t), "head": {"A": r(h["A"]), "B": r(h["B"])}, "tail": {"A": r(t["A"]), "B": r(t["B"])}}
    for k in wc.COMMAND:
        q = dict(p); q["silence"] = [k]
        h, t = wt.trial(q, "head", 7), wt.trial(q, "tail", 7)
        out["ablation"][k] = {"head_abolished": wt._v(h["A"]) < 1, "tail_abolished": wt._v(t["B"]) < 1,
                              "head": {"A": r(h["A"]), "B": r(h["B"])}, "tail": {"A": r(t["A"]), "B": r(t["B"])}}
    return out


if __name__ == "__main__":
    n, seed = int(sys.argv[1]), int(sys.argv[2])
    path = "out/worm_%d.jsonl" % seed
    os.makedirs("out", exist_ok=True)
    fixed = {}
    for kv in sys.argv[3:]:
        k, v = kv.split("="); fixed[k] = type(wc.DEFAULTS[k])(v)
    rng = random.Random(seed)
    ps = [draw(rng, fixed) for _ in range(n)]
    with Pool(int(os.environ.get("JOBS", os.cpu_count() or 4))) as pool:
        r1 = pool.map(stage1, ps, chunksize=4)
        with open(path + ".stage1", "w") as f:
            for row in r1: f.write(json.dumps(row) + "\n")
        good = [row["params"] for row in r1 if row["selective"]]
        print("stage 1: %d draws, selective in %d; head-selective in %d, tail-selective in %d" % (
            n, len(good), sum(row["head_selective"] for row in r1), sum(row["tail_selective"] for row in r1)), flush=True)
        n3 = 0
        with open(path, "w") as out:
            for row in pool.map(stage2, good):
                out.write(json.dumps(row) + "\n"); out.flush()
                ok = all(v["selective"] for v in row["seeds"].values()); n3 += ok
        print("stage 2: %d selective draws re-run; selective on seeds 8 and 9 as well in %d" % (len(good), n3), flush=True)
    tried, sel = {}, {}
    for row in r1:
        k = pattern(row["params"]); tried[k] = tried.get(k, 0) + 1; sel[k] = sel.get(k, 0) + row["selective"]
    print("AVA AVB AVD AVE PVC cross | tried selective")
    for k in sorted(tried):
        print("%+d  %+d  %+d  %+d  %+d  %d     | %d %d" % (k + (tried[k], sel[k])))
