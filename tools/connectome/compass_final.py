#!/usr/bin/env python3
"""Checks of the best settings on the heading-coordinate probe (research/atlas-a.md 4c).

  python3 tools/connectome/compass_final.py IN.jsonl K OUT.json

IN is a compass_search.py stage-2 file (rows {params, held, n_held}) or a compass_opt.py file (rows {top: [{params, score, ...}]}); the K best are taken (search: most held trials of 8; opt: highest score).
For each: (1) all 16 cues on circuit seeds 101 to 105 (3,500 ms), held at the late window 3,000 to 3,500 ms (compass_trial.held); (2) drift: 16 cues, circuit seed 101, 6,000 ms, signed
angular velocity between the windows 1,500-2,000 and 5,500-6,000 ms (deg/s, positive towards higher heading) for cues whose bump is active (mean rate above 0.3 Hz) in both, and the mean absolute velocity;
(3) turn test: cues at headings 2, 6, 10, 14, circuit seed 101, 3,000 ms, asym = -20, 0, +20 turn-sensor units for the whole trial (positive = turn_right, which drives the left P-EN cells);
velocity between the windows 1,500-2,000 and 2,500-3,000 ms; the turn effect = velocity(asym) - velocity(0). A working turn input gives a positive effect for +20 and a negative one for -20.
"""
import json, os, sys
from multiprocessing import Pool
import compass_trial as ct


def one(a):
    kind, p, cue, seed, asym, ms = a
    sp, _ = ct.run(ct.circuit(p, cue, asym, seed), ms)
    ws = {"held": [(3000, 3500)], "drift": [(1500, 2000), (5500, 6000)], "turn": [(1500, 2000), (2500, 3000)]}[kind]
    rs = [ct.analyse(ct.profile(sp, a_, b_)) for a_, b_ in ws]
    return {"kind": kind, "cue": cue, "seed": seed, "asym": asym, "win": [{k: r.get(k) for k in ("angle", "strength", "mean_hz", "near_peak")} for r in rs],
            "held": ct.held({"late": dict(rs[0], err=None if rs[0]["angle"] is None else abs(ct.wrap(rs[0]["angle"] - 22.5 * cue)))}) if kind == "held" else None}


def vel(w, dt):
    a, b = w[0], w[1]
    if a["angle"] is None or b["angle"] is None or a["mean_hz"] < 0.3 or b["mean_hz"] < 0.3: return None
    return ((b["angle"] - a["angle"] + 180) % 360 - 180) / dt


def load(path, k):
    rows = [json.loads(l) for l in open(path)]
    if rows and "top" in rows[0]:
        best = {}
        for r in rows:
            for t in r["top"]: best[json.dumps(t["params"], sort_keys=True)] = t
        return [{"params": t["params"], "rank": t["score"]} for t in sorted(best.values(), key=lambda t: -t["score"])[:k]]
    return [{"params": r["params"], "rank": r["n_held"]} for r in sorted(rows, key=lambda r: -r["n_held"])[:k]]


if __name__ == "__main__":
    fin = load(sys.argv[1], int(sys.argv[2]))
    res = []
    with Pool(int(os.environ.get("JOBS", os.cpu_count() or 4))) as pool:
        for f in fin:
            p = f["params"]
            held = pool.map(one, [("held", p, c, s, 0.0, 3500) for s in range(101, 106) for c in range(16)], chunksize=2)
            drift = pool.map(one, [("drift", p, c, 101, 0.0, 6000) for c in range(16)], chunksize=1)
            turn = pool.map(one, [("turn", p, c, 101, a, 3000) for c in (2, 6, 10, 14) for a in (-20.0, 0.0, 20.0)], chunksize=1)
            by_seed = {s: sum(1 for x in held if x["seed"] == s and x["held"]) for s in range(101, 106)}
            v = [x for x in (vel(d["win"], 4.0) for d in drift) if x is not None]
            tv = {}
            for c in (2, 6, 10, 14):
                vs = {t["asym"]: vel(t["win"], 1.0) for t in turn if t["cue"] == c}
                if all(x is not None for x in vs.values()): tv[c] = {"minus20": vs[-20.0] - vs[0.0], "plus20": vs[20.0] - vs[0.0], "v0": vs[0.0]}
            r = {"params": p, "rank": f["rank"], "held_of_16_by_seed": by_seed, "drift_signed_deg_s": (sum(v) / len(v)) if v else None, "drift_abs_deg_s": (sum(abs(x) for x in v) / len(v)) if v else None, "drift_cues_active": len(v), "turn_effect_deg_s": tv, "held": held, "drift": drift, "turn": turn}
            res.append(r)
            print("rank %.3f  held of 16 cues by seed 101..105: %s  drift signed %s abs %s deg/s (%d cues active)  turn effect (deg/s, -20 | +20): %s" % (f["rank"], by_seed, r["drift_signed_deg_s"] and round(r["drift_signed_deg_s"], 2), r["drift_abs_deg_s"] and round(r["drift_abs_deg_s"], 2), len(v), {c: (round(x["minus20"], 1), round(x["plus20"], 1)) for c, x in tv.items()}), flush=True)
    os.makedirs(os.path.dirname(sys.argv[3]) or ".", exist_ok=True)
    json.dump(res, open(sys.argv[3], "w"))
