#!/usr/bin/env python3
"""All 32 sign assignments for one setting, gains as fitted (research/atlas-a.md 4c).

  python3 compass_signs.py PARAMS.json CHUNK NCHUNKS OUT.json     assignments a = CHUNK, CHUNK + NCHUNKS, ... of 0..31

Bit i of a is 1 = inhibitory (weights negated) for the types in the order EPG, PEN_a, PEN_b, PEG, Delta7. The fitted/assumed assignment (Delta7 inhibitory only) is a = 16.
Each assignment: cues 2, 6, 10, 14 on circuit seeds 101 to 105 (20 trials), held as compass_trial.held. Prints one row per assignment.
"""
import json, os, sys
from multiprocessing import Pool
import compass_trial as ct
import compass_check as cck

TYPES = ("EPG", "PENa", "PENb", "PEG", "D7")


def signs(a): return {"sign_EPG": -1 if a & 1 else 1, "sign_PENa": -1 if a & 2 else 1, "sign_PENb": -1 if a & 4 else 1, "sign_PEG": -1 if a & 8 else 1, "sign_D7": -1 if a & 16 else 1}


def one(x):
    p, a, cue, seed = x
    t = ct.trial(dict(p, **signs(a)), cue, seed)
    return {"a": a, "cue": cue, "seed": seed, "held": ct.held(t), "mean_hz": t["late"]["mean_hz"], "err": t["late"]["err"]}


if __name__ == "__main__":
    p = cck.load(sys.argv[1]); ch, n = int(sys.argv[2]), int(sys.argv[3])
    mine = [a for a in range(32) if a % n == ch]
    with Pool(int(os.environ.get("JOBS", os.cpu_count() or 4))) as pool:
        rows = pool.map(one, [(p, a, c, s) for a in mine for c in (2, 6, 10, 14) for s in range(101, 106)], chunksize=2)
    for a in mine:
        r = [x for x in rows if x["a"] == a]
        print("a=%2d %s held %2d of 20" % (a, " ".join("%s=%s" % (t, "-" if signs(a)["sign_" + t] < 0 else "+") for t in TYPES), sum(x["held"] for x in r)), flush=True)
    json.dump(rows, open(sys.argv[4], "w"))
