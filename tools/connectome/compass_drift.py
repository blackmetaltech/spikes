#!/usr/bin/env python3
"""Drift test for the compass probe (research/atlas-a.md section 4c, step 1). Does the late bump angle follow the cue, or does every cue end at the same place?

  python3 tools/connectome/compass_drift.py find N SEED K OUT.jsonl     re-create the draws of compass_search.py (pairs=1, rng seed SEED, N draws), run one trial each (cue at position 10,
                                                                         3,500 ms), keep the K draws with the strongest late bump inside the rate band (2 to 60 Hz), and run each at
                                                                         all 16 cues for 6,000 ms; one JSON line per draw
  python3 tools/connectome/compass_drift.py report DIR [DIR ...]         print the table from the .jsonl files found under the directories
Windows (ms): cue 300-500, 700-1000, 1500-2000, 3000-3500, 5500-6000. Workers: $JOBS.
"""
import glob, json, math, os, random, sys
from multiprocessing import Pool
import compass_trial as ct
import compass_search as cs

WIN = {"cue": (300, 500), "w700": (700, 1000), "w1500": (1500, 2000), "w3000": (3000, 3500), "w5500": (5500, 6000)}


def drift_trial(a):
    p, cue, ms, seed = a
    sp, err = ct.run(ct.circuit(p, cue, 0.0, seed), ms)
    out = {}
    for k, (x, y) in WIN.items():
        if y <= ms:
            r = ct.analyse(ct.profile(sp, x, y)); out[k] = {"angle": None if r["angle"] is None else round(r["angle"], 1), "strength": round(r["strength"], 3), "mean_hz": round(r["mean_hz"], 2), "near_peak": round(r["near_peak"], 3)}
    return out


def first(p):
    return p, ct.trial(p, 10, 7)


def find(n, seed, k, path):
    rng = random.Random(seed)
    ps = [cs.draw_pairs(rng, {}) for _ in range(n)]
    with Pool(int(os.environ.get("JOBS", os.cpu_count() or 4))) as pool:
        r = pool.map(first, ps, chunksize=4)
        band = [(t["late"]["strength"], t["late"]["near_peak"], p, t) for p, t in r if 2 < t["late"]["mean_hz"] < 60 and t["late"]["angle"] is not None]
        print("seed %d: %d draws, %d in the rate band" % (seed, n, len(band)), flush=True)
        band.sort(key=lambda x: (x[0], x[1]), reverse=True)
        with open(path, "w") as f:
            for st, npk, p, t in band[:k]:
                rows = pool.map(drift_trial, [(p, c, 6000, 7) for c in range(16)])
                f.write(json.dumps({"seed": seed, "params": p, "find_cue10_late": {"angle": t["late"]["angle"], "strength": st}, "trials": rows}) + "\n"); f.flush()


def cmean(angles):
    if not angles: return None, 0.0
    x = sum(math.cos(math.radians(a)) for a in angles); y = sum(math.sin(math.radians(a)) for a in angles)
    return math.degrees(math.atan2(y, x)) % 360, math.hypot(x, y) / len(angles)


def report(dirs):
    rows = []
    for d in dirs:
        for f in sorted(glob.glob(os.path.join(d, "**", "drift_*.jsonl"), recursive=True)):
            rows += [json.loads(l) for l in open(f)]
    print("%d draws, each at 16 cues. angle = population-vector angle of the E-PG cells in the window (degrees; '-' = silent, below 0.3 Hz mean)." % len(rows))
    print("R = length of the mean vector of the angles over the cues (1 = every cue ends at one place, about 0.25 = spread evenly). follow = cues whose angle is within 30 degrees of the cue.")
    print("%-3s %-5s | %s | %s" % ("id", "seed", "late (5500-6000) angle by cue 0..15", "R and follow per window: cue, 700, 1500, 3000, 5500   | late mean-angle  late-strength"))
    tot = {w: [] for w in WIN}
    for i, r in enumerate(rows):
        line = []
        stats = []
        for w in WIN:
            an = [t[w]["angle"] for t in r["trials"] if t[w]["angle"] is not None and t[w]["mean_hz"] >= 0.3]
            m, R = cmean(an)
            fol = sum(1 for k, t in enumerate(r["trials"]) if t[w]["angle"] is not None and t[w]["mean_hz"] >= 0.3 and abs((t[w]["angle"] - 22.5 * k + 180) % 360 - 180) < 30)
            stats.append("%.2f/%d" % (R, fol)); tot[w].append((R, fol))
        late = ["%3d" % round(t["w5500"]["angle"]) if t["w5500"]["angle"] is not None and t["w5500"]["mean_hz"] >= 0.3 else "  -" for t in r["trials"]]
        an = [t["w5500"]["angle"] for t in r["trials"] if t["w5500"]["angle"] is not None and t["w5500"]["mean_hz"] >= 0.3]
        m, R = cmean(an)
        ms = sum(t["w5500"]["strength"] for t in r["trials"]) / 16
        print("%-3d %-5d | %s | %s | %s %.2f" % (i, r["seed"], " ".join(late), " ".join(stats), "-" if m is None else "%3d" % round(m), ms))
    print("mean over draws, per window, of R and of the number of cues followed (of 16):")
    for w in WIN:
        if tot[w]: print("  %-6s R %.2f  follow %.1f" % (w, sum(x for x, _ in tot[w]) / len(tot[w]), sum(y for _, y in tot[w]) / len(tot[w])))
    # places: how many distinct ring positions (of 16) hold the late angle across cues, and the share of the most common one
    print("late angles binned to the 16 ring positions: distinct places used and share of the commonest, per draw")
    for i, r in enumerate(rows):
        b = [int(round(t["w5500"]["angle"] / 22.5)) % 16 for t in r["trials"] if t["w5500"]["angle"] is not None and t["w5500"]["mean_hz"] >= 0.3]
        if b:
            c = max(set(b), key=b.count)
            print("  draw %2d: %2d active cues, %2d places, commonest position %2d holds %.2f" % (i, len(b), len(set(b)), c, b.count(c) / len(b)))
        else: print("  draw %2d: silent at every cue" % i)


def stage1_table(dirs):
    """Summary of compass_search .stage1 files by name prefix (norm1, norm2, norm3): trials silent / in band / saturated, and near misses."""
    print("%-6s %6s %8s %8s %8s %8s %12s %10s %10s %9s" % ("norm", "draws", "trials", "silent", "in band", "satur.", "band,str>.5", "band,err<30", "held both", "held one"))
    for pre in ("norm1", "norm2", "norm3"):
        fs = [f for d in dirs for f in glob.glob(os.path.join(d, "**", pre + "_*.jsonl.stage1"), recursive=True)]
        rows = [json.loads(l) for f in fs for l in open(f)]
        tr = [t for r in rows for t in r["trials"]]
        if not tr: continue
        sil = sum(1 for t in tr if t["mean_hz"] < 2); sat = sum(1 for t in tr if t["mean_hz"] > 60); band = [t for t in tr if 2 <= t["mean_hz"] <= 60]
        both = sum(1 for r in rows if all(t["held"] for t in r["trials"]))
        print("%-6s %6d %8d %8.4f %8.4f %8.4f %12d %10d %10d %9d" % (pre, len(rows), len(tr), sil / len(tr), len(band) / len(tr), sat / len(tr), sum(1 for t in band if t["strength"] > 0.5), sum(1 for t in band if t["err"] is not None and t["err"] < 30), both, sum(1 for r in rows if any(t["held"] for t in r["trials"]))))


if __name__ == "__main__":
    if sys.argv[1] == "s1":
        stage1_table(sys.argv[2:])
    elif sys.argv[1] == "find":
        os.makedirs(os.path.dirname(sys.argv[5]) or ".", exist_ok=True)
        find(int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]), sys.argv[5])
    else:
        report(sys.argv[2:])
