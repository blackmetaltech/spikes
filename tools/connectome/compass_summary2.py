#!/usr/bin/env python3
"""Tables for research/atlas-a.md 4c, heading-coordinate runs: python3 compass_summary2.py SEARCH_EVOLVE_DIR FINAL_DIR TURN_DIR CANDIDATES.jsonl
(directories hold the downloaded runner artifacts). compass_summary.py is the one for the earlier bridge-coordinate runs."""
import glob, json, os, re, sys
S, F, T, C = sys.argv[1:5]
print("random searches (stage 1: cues 2 and 10, circuit seed 7)")
print("%-14s %6s %7s %7s %8s %7s %10s %9s" % ("search", "draws", "silent", "band", "satur.", "held 2", "held 1 cue", "stage 2"))
for pre, name in (("types", "scale per type"), ("pairs", "scale per pair")):
    rows = [json.loads(l) for f in glob.glob(os.path.join(S, "**", pre + "_*.jsonl.stage1"), recursive=True) for l in open(f)]
    tr = [t for r in rows for t in r["trials"]]
    s2 = sum(1 for f in glob.glob(os.path.join(S, "**", pre + "_*.jsonl"), recursive=True) for l in open(f))
    print("%-14s %6d %7.3f %7.3f %8.3f %7d %10d %9d" % (name, len(rows), sum(t["mean_hz"] < 2 for t in tr) / len(tr), sum(2 <= t["mean_hz"] <= 60 for t in tr) / len(tr), sum(t["mean_hz"] > 60 for t in tr) / len(tr), sum(all(t["held"] for t in r["trials"]) for r in rows), sum(any(t["held"] for t in r["trials"]) for r in rows), s2))
print()
print("evolution strategy (mu + lambda = 8 + 32, 30 min per run)")
print("%-14s %5s %-22s %10s %12s" % ("run", "gens", "best score first->last", "held 4 cues", "held 8 of 8"))
for f in sorted(glob.glob(os.path.join(S, "**", "opt_n*.jsonl"), recursive=True)):
    L = [json.loads(l) for l in open(f)]
    h4 = sum(1 for g in L for t in g["top"] if all(t["held7"])); h8 = any(t.get("held8") and all(t["held8"]) for g in L for t in g["top"])
    first = next((i for i, g in enumerate(L) if any(t.get("held8") and all(t["held8"]) for t in g["top"])), None)
    print("%-14s %5d %.3f -> %.3f%s %10s %12s" % (os.path.basename(f)[:-6], len(L), L[0]["best"], L[-1]["best"], " " * 6, "yes" if h4 else "no", ("gen %d" % first) if h8 else "no"))
print()
print("finalists: held of 16 cues on circuit seeds 101..105; places = distinct late angles (rounded to 10 deg, cue-independent bumps) over the 16 cues at seed 101; drift; turn effect (deg/s; right sign: -20 negative, +20 positive)")
src = [json.loads(l)["top"][0] for l in open(C)]
for i, c in enumerate(src):
    fs = glob.glob(os.path.join(F, "**", "final_%d.json" % i), recursive=True)
    if not fs: continue
    r = json.load(open(fs[0]))[0]
    angs = sorted({round(d["win"][1]["angle"] / 10) * 10 % 360 for d in r["drift"] if d["win"][1]["angle"] is not None})
    failed = sorted({x["cue"] for x in r["held"] if not x["held"]})
    print("%2d %-52s score %.3f  held %s (%d of 80)  failing cues %s  places %s  drift %.2f deg/s (mean |v| %.2f)" % (i + 1, c["source"], c["score"], list(r["held_of_16_by_seed"].values()), sum(r["held_of_16_by_seed"].values()), failed, angs, r["drift_signed_deg_s"], r["drift_abs_deg_s"]))
print()
print("turn-gain sweeps (compass_turn.py; effect of asym -20 / +20 on the late angle in degrees at cues 2, 6, 10, 14; right sign: negative / positive)")
for i in range(len(src)):
    fs = glob.glob(os.path.join(T, "**", "turn_%d.txt" % i), recursive=True)
    if fs: print("candidate %d (%s)\n%s" % (i + 1, src[i]["source"], open(fs[0]).read()))
