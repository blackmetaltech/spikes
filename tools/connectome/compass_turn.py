#!/usr/bin/env python3
"""Turn-gain sweep for one candidate (research/atlas-a.md 4c). The turn sensors are constant currents of turn_w pA per unit onto the P-EN cells of one half of the bridge each
(compass_trial.circuit: both get 40 units, asym units are added to one half and removed from the other). For each turn_w the bump is formed by a cue at heading 2, 6, 10, 14
(circuit seed 101, 6,000 ms) with asym -20, 0, +20 (positive = turn_right = the left P-EN cells, which wire towards higher heading). The late angle (5,500-6,000 ms) minus the angle with asym 0
is the turn effect in degrees; right direction = positive for +20 and negative for -20.

  python3 tools/connectome/compass_turn.py IN.jsonl LINE OUT.json     (LINE is 1-based, a line of a candidates file)
"""
import json, os, sys
from multiprocessing import Pool
import compass_trial as ct

GAINS = (1.0, 3.0, 10.0, 30.0, 100.0)


def one(a):
    p, g, cue, asym = a
    q = dict(p, turn_w=g)
    sp, _ = ct.run(ct.circuit(q, cue, asym, 101), 6000)
    r = ct.analyse(ct.profile(sp, 5500, 6000))
    e = ct.analyse(ct.profile(sp, 1500, 2000))
    return {"turn_w": g, "cue": cue, "asym": asym, "early": e["angle"], "late": r["angle"], "strength": r["strength"], "mean_hz": r["mean_hz"]}


if __name__ == "__main__":
    p = json.loads(open(sys.argv[1]).read().splitlines()[int(sys.argv[2]) - 1])["top"][0]["params"]
    with Pool(int(os.environ.get("JOBS", os.cpu_count() or 4))) as pool:
        rows = pool.map(one, [(p, g, c, a) for g in GAINS for c in (2, 6, 10, 14) for a in (-20.0, 0.0, 20.0)], chunksize=1)
    res = []
    print("%-7s %-30s %-30s %s" % ("turn_w", "effect of -20 by cue 2,6,10,14 (deg)", "effect of +20 by cue 2,6,10,14 (deg)", "late strength / rate at asym 0, mean"))
    for g in GAINS:
        eff = {}
        for c in (2, 6, 10, 14):
            d = {r["asym"]: r for r in rows if r["turn_w"] == g and r["cue"] == c}
            eff[c] = [None if d[a]["late"] is None or d[0.0]["late"] is None else round((d[a]["late"] - d[0.0]["late"] + 180) % 360 - 180) for a in (-20.0, 20.0)]
        z = [r for r in rows if r["turn_w"] == g and r["asym"] == 0.0]
        print("%-7g %-30s %-30s %.2f / %.1f Hz" % (g, [eff[c][0] for c in eff], [eff[c][1] for c in eff], sum(r["strength"] for r in z) / 4, sum(r["mean_hz"] for r in z) / 4))
    json.dump(rows, open(sys.argv[3], "w"))
