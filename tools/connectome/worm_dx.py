#!/usr/bin/env python3
"""The recording signature of the four lesions of dx_a_worm (iteration A2, step 3). Runs on a GitHub runner, never on a desktop.

Model point of worm_point.py's study (POINT below). The patient has every cell and every connection, but the OUTPUTS of one interneuron class (AVA, AVB, AVD or PVC) carry nothing
(weight 0), as in godot/circuits/dx_a_worm.json. Head touch as in the levels (150 Hz per head touch cell for 400 ms from 500 ms), circuit seeds 1 to 3 and 101 to 105, 1500 ms.
Prints, per condition (intact, each lesion), the mean rate in Hz per cell of every class over the touch and the 300 ms after it ([500, 1200)), the minimum and maximum over the seeds.
"""
import json, os, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import worm_circuit as wc
import worm_trial as wt

POINT = dict(scale_chem=15.5, I_e=170.0, min_count=3, tau_syn_ex=2.0)
SEEDS = (1, 2, 3, 101, 102, 103, 104, 105)
CLASSES = ["ALM", "AVM", "AVA", "AVB", "AVD", "AVE", "PVC", "DA", "VA", "DB", "VB"]


def one(arg):
    lesion, seed = arg
    q = dict(POINT); q["touch"] = "head"; q["touch_seed"] = seed * 1000 + 1
    c = wc.build(q); c["seed"] = seed
    if lesion:
        for pr in c["projections"]:
            if pr["pre"] == lesion:
                for e in pr["edges"]: e[2] = 0.0
    cl = [wt.CLASS_OF[n] for n in c["meta"]["cells"]]
    sp = wt.run(c, 1500)
    n_of = {}
    for k in cl: n_of[k] = n_of.get(k, 0) + 1
    return {k: round(sum(1 for t, i in sp if 500 <= t < 1200 and cl[i] == k) / n_of[k] / 0.7, 2) for k in CLASSES}


if __name__ == "__main__":
    conds = ["", "AVA", "AVB", "AVD", "PVC"]
    with Pool(os.cpu_count() or 2) as pool:
        res = pool.map(one, [(c, s) for c in conds for s in SEEDS])
    print("class rates in Hz per cell over [500, 1200): min..max over seeds %s" % (SEEDS,))
    print("%-8s" % "lesion" + "".join("%12s" % k for k in CLASSES))
    for i, c in enumerate(conds):
        rows = res[i * len(SEEDS):(i + 1) * len(SEEDS)]
        print("%-8s" % (c or "intact") + "".join("%12s" % ("%.1f..%.1f" % (min(r[k] for r in rows), max(r[k] for r in rows))) for k in CLASSES))
    os.makedirs("out", exist_ok=True)
    json.dump({"conds": conds, "seeds": SEEDS, "rows": res}, open("out/worm_dx.json", "w"))
