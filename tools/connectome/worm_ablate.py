#!/usr/bin/env python3
"""Ablation study of the ALL-EXCITATORY full-wiring worm (every class sign +1, cross=0): the only setting that needs no sign assumption. FEASIBILITY PROBE for iteration A2.

  python3 tools/connectome/worm_ablate.py N SEED [key=value ...]     key=value fixes a parameter. Workers: $JOBS, else every CPU.
Draws N parameter sets (scale_chem log-uniform 5 to 125 pA, scale_elec 0 half the time else 0.25-1 x scale_chem, I_e 100-330 pA, tau_syn_ex from {2, 5, 15}, min_count from {1, 2, 3, 5};
as in worm_search.py; tau_syn_in has no effect, no inhibition). A draw is "usable" if, intact, head touch gives A >= 5 Hz in the response window [500, 1200) ms, baseline A and B < 1 Hz,
and activity stops (A < 1 Hz in the last 200 ms of a trial run for 1500 ms). Otherwise "silent" (A < 5 Hz or baseline-silent failure counted as silent only if A < 5 Hz) or "runaway"
(A >= 5 Hz but baseline >= 1 Hz or still active at the end).
For each usable draw, head touch on circuit seeds 7, 8, 9 (touch trains differ with the seed): intact (also latency), silence each of AVA AVB AVD AVE PVC (A, B, abolished = A < 1 Hz on all
three seeds, latency), keep each alone (A, arrives = A >= 5 Hz on all three seeds), silence each of the 10 pairs (abolished). Cost per usable draw: 3 (intact) + 15 + 15 + 30 = 63 trials
of 1200 ms, plus the screening trial of 1500 ms (every draw; it is the seed-7 intact trial of the usable ones, which then needs only 2 more intact trials: 62 more).
Output: out/worm_ablate_SEED.jsonl (usable draws), out/worm_ablate_SEED.jsonl.all (every draw in brief), tables at the end (all usable; scale_elec == 0; min_count >= 3).
"""
import itertools, json, os, random, sys
from multiprocessing import Pool
import worm_trial as wt
import worm_circuit as wc

CL = wc.COMMAND
SEEDS = (7, 8, 9)
PAIRS = list(itertools.combinations(CL, 2))


def draw(rng, fixed):
    p = {"scale_chem": round(10 ** rng.uniform(0.699, 2.097), 2)}
    p["scale_elec"] = 0.0 if rng.random() < 0.5 else round(p["scale_chem"] * rng.uniform(0.25, 1.0), 2)
    p.update(I_e=round(rng.uniform(100, 330), 1), tau_syn_ex=rng.choice([2.0, 5.0, 15.0]), min_count=rng.choice([1, 2, 3, 5]))
    p.update(fixed)
    return p


def r(x): return None if x is None else round(x, 2)
def v(x): return wt._v(x)


def screen(p):
    t = wt.trial(p, "head", 7, ms=1500)
    if v(t["A"]) < 5: why = "silent"
    elif v(t["A_base"]) >= 1 or v(t["B_base"]) >= 1 or v(t["A_late"]) >= 1: why = "runaway"
    else: why = "usable"
    return {"params": p, "why": why, "A": r(t["A"]), "B": r(t["B"]), "A_base": r(t["A_base"]), "A_late": r(t["A_late"])}, t


def three(p, silence):
    q = dict(p); q["silence"] = silence
    ts = [wt.trial(q, "head", s) for s in SEEDS]
    return ts


def study(args):
    p, brief, t7 = args
    out = {"params": p, "intact": {"A": r(t7["A"]), "B": r(t7["B"]), "lat_A": t7["lat_A"]}, "silence": {}, "keep": {}, "pairs": {}}
    inter = [t7] + [wt.trial(p, "head", s) for s in SEEDS[1:]]
    out["intact"]["lat_A_seeds"] = [t["lat_A"] for t in inter]
    for k in CL:
        ts = three(p, [k])
        out["silence"][k] = {"A": [r(t["A"]) for t in ts], "B": [r(t["B"]) for t in ts], "lat_A": [t["lat_A"] for t in ts], "abolished": all(v(t["A"]) < 1 for t in ts)}
        ts = three(p, [c for c in CL if c != k])
        out["keep"][k] = {"A": [r(t["A"]) for t in ts], "arrives": all(v(t["A"]) >= 5 for t in ts)}
    for a, b in PAIRS:
        ts = three(p, [a, b])
        out["pairs"][a + "+" + b] = {"A": [r(t["A"]) for t in ts], "abolished": all(v(t["A"]) < 1 for t in ts)}
    return out


def frac(rows, f):
    return "%d/%d = %.2f" % (sum(f(x) for x in rows), len(rows), sum(f(x) for x in rows) / len(rows)) if rows else "0/0"


def tables(rows, title):
    print("== %s (%d usable draws)" % (title, len(rows)))
    if not rows: return
    print(" silencing it abolishes the response:")
    for k in CL: print("   %s %s" % (k, frac(rows, lambda x: x["silence"][k]["abolished"])))
    print(" it alone carries the signal (the other four silenced, A >= 5 Hz):")
    for k in CL: print("   %s %s" % (k, frac(rows, lambda x: x["keep"][k]["arrives"])))
    print(" pair silenced, abolished:")
    for a, b in PAIRS: print("   %s+%s %s" % (a, b, frac(rows, lambda x: x["pairs"][a + "+" + b]["abolished"])))


if __name__ == "__main__":
    n, seed = int(sys.argv[1]), int(sys.argv[2])
    path = "out/worm_ablate_%d.jsonl" % seed
    os.makedirs("out", exist_ok=True)
    fixed = {}
    for kv in sys.argv[3:]:
        k, val = kv.split("="); fixed[k] = type(wc.DEFAULTS[k])(val)
    rng = random.Random(seed)
    ps = [draw(rng, fixed) for _ in range(n)]
    with Pool(int(os.environ.get("JOBS", os.cpu_count() or 4))) as pool:
        sc = pool.map(screen, ps, chunksize=4)
        with open(path + ".all", "w") as f:
            for b, t in sc: f.write(json.dumps(b) + "\n")
        cnt = {w: sum(1 for b, t in sc if b["why"] == w) for w in ("usable", "silent", "runaway")}
        print("screen: %d draws, usable %d, silent %d, runaway %d" % (n, cnt["usable"], cnt["silent"], cnt["runaway"]), flush=True)
        rows = []
        with open(path, "w") as out:
            for row in pool.map(study, [(b["params"], b, t) for b, t in sc if b["why"] == "usable"]):
                rows.append(row); out.write(json.dumps(row) + "\n"); out.flush()
    print("study: %d usable draws done" % len(rows), flush=True)
    tables(rows, "all")
    tables([x for x in rows if x["params"]["scale_elec"] == 0], "scale_elec == 0")
    tables([x for x in rows if x["params"]["min_count"] >= 3], "min_count >= 3")
