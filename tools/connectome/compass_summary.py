#!/usr/bin/env python3
"""Tables for research/atlas-a.md section 4c from the runner outputs: python3 compass_summary.py DRIFT_DIR OPT_DIR
Prints (1) the angular move per draw of the drift test (from the cue window to 700-1000 ms, and from 700-1000 ms to 5500-6000 ms), (2) the evolution runs by norm, (3) the check runs."""
import glob, json, os, re, sys
def wrap(d): return (d + 180) % 360 - 180
d, o = sys.argv[1:3]
mv = []
for f in sorted(glob.glob(os.path.join(d, "**", "drift_*.jsonl"), recursive=True)):
    for l in open(f):
        r = json.loads(l); a = [t for t in r["trials"] if all(t[w]["angle"] is not None for w in ("cue", "w700", "w5500"))]
        mv.append((sum(abs(wrap(t["w700"]["angle"] - t["cue"]["angle"])) for t in a) / len(a), max(abs(wrap(t["w700"]["angle"] - t["cue"]["angle"])) for t in a), sum(abs(wrap(t["w5500"]["angle"] - t["w700"]["angle"])) for t in a) / len(a)))
n = len(mv)
print("drift test, %d draws x 16 cues: mean move cue window (300-500 ms) to 700-1000 ms: %.1f deg (largest single %.1f deg), over 0.45 s between window centres = %.0f deg/s; mean move 700-1000 ms to 5500-6000 ms: %.2f deg" % (n, sum(m[0] for m in mv) / n, max(m[1] for m in mv), sum(m[0] for m in mv) / n / 0.45, sum(m[2] for m in mv) / n))
best = {}; chk = {}
for f in sorted(glob.glob(os.path.join(o, "**", "opt_n*.jsonl"), recursive=True)):
    m = re.search(r"opt_n(\d)_(\d+)", f); k = int(m.group(1)); L = [json.loads(l) for l in open(f)]
    best.setdefault(k, []).append(([g["best"] for g in L]))
for f in sorted(glob.glob(os.path.join(o, "**", "check_n*.json"), recursive=True)):
    k = int(re.search(r"check_n(\d)", f).group(1)); chk.setdefault(k, []).extend(json.load(open(f)))
print("evolution runs (5 seeds per norm; (mu + lambda) = 8 + 32; 42 min each). best score by generation, then the check of the best three of each seed")
print("%-5s %5s %-28s %-28s %-8s %s" % ("norm", "seeds", "best score gen 0 (min-max)", "best score last gen (min-max)", "gens", "max cues held of 16 (one seed), best three of every seed"))
for k in sorted(best):
    b = best[k]
    mx = max(max(r["held_of_16_by_seed"].values()) for r in chk[k]); mean = sum(sum(r["held_of_16_by_seed"].values()) / 5 for r in chk[k]) / len(chk[k])
    print("%-5d %5d %.3f - %.3f%s %.3f - %.3f%s %d-%d%s %d (mean %.2f)" % (k, len(b), min(x[0] for x in b), max(x[0] for x in b), " " * 12, min(x[-1] for x in b), max(x[-1] for x in b), " " * 12, min(len(x) for x in b), max(len(x) for x in b), "  ", mx, mean))
