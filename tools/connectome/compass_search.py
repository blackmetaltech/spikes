#!/usr/bin/env python3
"""Random search over the per-type scales for a held bump (compass_trial.held), two stages. Probe for research/atlas-a.md.

  python3 tools/connectome/compass_search.py N SEED [out.jsonl] [key=value ...]    key=value fixes a parameter (for example sign_D7=1, or tau_syn_ex=15)
Writes out/compass_SEED.jsonl by default (stage 2 rows) and the same name with .stage1 (every draw, in brief). Workers: $JOBS, else every CPU.
Stage 1: N draws, two trials each (cue at position 2 and 10, circuit seed 7). Stage 2: draws with both held are re-run on 4 cues x 2 circuit seeds (7, 8).
Ranges: scale_* log-uniform 0.005 to 1.0 pA per synapse (EPG, PEN_a, PEN_b, PEG, Delta7 each; PEN_b = PEN_a half the time), I_e 150 to 360 pA,
tau_syn_ex and tau_syn_in from {2, 5, 15} ms, turn_w from {0.5, 1, 3}.
"""
import json, os, random, sys
from multiprocessing import Pool
import compass_trial as ct
import compass_circuit as cc


def draw(rng, fixed):
    p = {"scale_" + t: round(10 ** rng.uniform(-2.3, 0), 4) for t in ("EPG", "PENa", "PENb", "PEG", "D7")}
    if rng.random() < 0.5: p["scale_PENb"] = p["scale_PENa"]
    p.update(I_e=round(rng.uniform(150, 360), 1), tau_syn_ex=rng.choice([2.0, 5.0, 15.0]), tau_syn_in=rng.choice([2.0, 5.0, 15.0]), turn_w=rng.choice([0.5, 1.0, 3.0]))
    p.update(fixed)
    return p


PAIRS = ["EPG_EPG", "EPG_PENa", "EPG_PENb", "EPG_D7", "EPG_PEG", "PENa_EPG", "PENb_EPG", "D7_EPG", "D7_PENa", "D7_PENb", "D7_D7", "D7_PEG", "PEG_EPG", "PEG_PENa", "PEG_PENb", "PENa_PENa", "PENb_PENb", "PENa_PENb", "PENb_PENa"]


def draw_pairs(rng, fixed):
    """Second search: one scale per type pair (19 of them), I_e per population."""
    p = draw(rng, {})
    for k in PAIRS: p["w_" + k] = round(10 ** rng.uniform(-2.3, 0), 4)
    for pid in ("EPG", "PENa", "PENb", "PEG", "D7"): p["I_e_" + pid] = round(rng.uniform(150, 360), 1)
    p.update(fixed)
    return p


def brief(t):
    """The numbers of one trial that say how near a miss it was (late window, and the bump just after the cue)."""
    h = t["late"]
    return {"cue": t["cue"], "held": ct.held(t), "err": h["err"], "strength": round(h["strength"], 3), "near_peak": round(h["near_peak"], 3), "mean_hz": round(h["mean_hz"], 2), "early_strength": round(t["early"]["strength"], 3)}


def stage1(p):
    ts = [ct.trial(p, c, 7) for c in (2, 10)]
    return p, [ct.held(t) for t in ts], [brief(t) for t in ts]


def stage2(p):
    return p, [ct.held(ct.trial(p, c, s)) for s in (7, 8) for c in (2, 6, 10, 14)]


if __name__ == "__main__":
    n, seed = int(sys.argv[1]), int(sys.argv[2])
    path = sys.argv[3] if len(sys.argv) > 3 and "=" not in sys.argv[3] else "out/compass_%d.jsonl" % seed
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    out = open(path, "w"); all1 = open(path + ".stage1", "w")   # .stage1: every draw with its two trials in brief, so a search that finds nothing still says how near it came
    fixed = {}
    for kv in sys.argv[3:]:
        if "=" in kv:
            k, v = kv.split("="); fixed[k] = 1 if k == 'pairs' else type(cc.DEFAULTS[k])(v)
    rng = random.Random(seed)
    dr = draw_pairs if fixed.pop('pairs', None) else draw
    ps = [dr(rng, fixed) for _ in range(n)]
    with Pool(int(os.environ.get("JOBS", os.cpu_count() or 4))) as pool:
        r1 = pool.map(stage1, ps, chunksize=4)
        for p, h, b in r1: all1.write(json.dumps({"params": p, "trials": b}) + "\n")
        all1.close()
        good = [p for p, h, b in r1 if all(h)]
        print("stage 1: %d draws, held at both cues in %d; held at one cue in %d" % (n, len(good), sum(1 for p, h, b in r1 if any(h))), flush=True)
        for p, h in pool.map(stage2, good):
            out.write(json.dumps({"params": p, "held": h, "n_held": sum(h)}) + "\n"); out.flush()
            print(sum(h), "of 8", p, flush=True)
