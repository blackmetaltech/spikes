#!/usr/bin/env python3
"""Tables for research/atlas-a.md 4c, turn term / signs / places: python3 compass_summary3.py R6 R7 R8 R9   (directories of the downloaded runner artifacts)
R6: turn-term evolution (opt2_*), hemibrain check, signs and check of the best holding setting (setting 7 of compass_candidates.jsonl); R7: the two settings that held and turned (compass_turners.jsonl), their
checks and signs; R8: rotating-trial evolution; R9: checks of its final settings (compass_rot.jsonl)."""
import glob, json, os, re, sys
R6, R7, R8, R9 = sys.argv[1:5]
def cat(d, pat): return "".join(open(f).read() for f in sorted(glob.glob(os.path.join(d, "**", pat), recursive=True)))
print("TURN-TERM EVOLUTION (start: settings 2, 4, 7 of compass_candidates.jsonl, 3 per start; mu 4, lambda 16, 45 min; trials on seed 7 then 8: hold at cues 2, 6, 10, 14 and turn at (cue 2, +), (10, -), (10, +), (2, -))")
print("%-8s %-6s %5s %-17s %-10s %-10s %-26s %-9s %s" % ("run", "start", "gens", "best first->last", "hold", "turn", "shifts of best (headings)", "4+4 on 7", "4+4 on 7 and 8"))
for f in sorted(glob.glob(os.path.join(R6, "**", "opt2_*.jsonl"), recursive=True), key=lambda f: int(re.search(r"opt2_(\d+)", f).group(1))):
    i = int(re.search(r"opt2_(\d+)", f).group(1)); L = [json.loads(l) for l in open(f)]; b = L[-1]["top"][0]
    both = any(all(t["held7"]) and all(t["turn7"]) for g in L for t in g["top"]); p8 = any(t.get("turn8") and all(t["turn8"]) and all(t["held8"]) for g in L for t in g["top"])
    print("%-8s %-6d %5d %.3f -> %.3f %6s %-10.2f %-10.2f %-26s %-9s %s" % ("opt2_%d" % i, [2, 4, 7][i % 3], len(L), L[0]["best"], L[-1]["best"], "", b["hold_score"], b["turn_score"], b["x7"], "yes" if both else "no", "yes" if p8 else "no"))
print("\nBEST HOLDING SETTING (setting 7: norm 2, one scale per type pair, turn_w 1; no turn term), seeds 101 to 105:")
print(cat(R6, "check7.txt"))
print("SIGN ASSIGNMENTS of that setting (gains as fitted; cues 2, 6, 10, 14 x seeds 101..105; a = bit 1 EPG, 2 PEN_a, 4 PEN_b, 8 PEG, 16 Delta7; - = inhibitory); top eight rows, then the count with 18 or more of 20:")
rows = sorted(cat(R6, "signs7_*.txt").splitlines(), key=lambda l: -int(re.search(r"held\s+(\d+) of", l).group(1)))
print("\n".join(rows[:8])); print("assignments with >= 18 of 20:", sum(1 for l in rows if int(re.search(r"held\s+(\d+) of", l).group(1)) >= 18), "; with >= 1:", sum(1 for l in rows if int(re.search(r"held\s+(\d+) of", l).group(1)) >= 1), "of 32")
print("\nSETTINGS THAT HOLD AND TURN ON SEEDS 7 AND 8 (compass_turners.jsonl), checked on seeds 101 to 105:")
for k in (0, 1):
    print("setting %d:" % (k + 1)); print(cat(R7, "checkT%d.txt" % k))
    r = sorted(cat(R7, "signsT%d_*.txt" % k).splitlines(), key=lambda l: -int(re.search(r"held\s+(\d+) of", l).group(1)))
    print("sign assignments, top four:"); print("\n".join(r[:4])); print("with >= 18 of 20:", sum(1 for l in r if int(re.search(r"held\s+(\d+) of", l).group(1)) >= 18), "\n")
print("ROTATING-TRIAL EVOLUTION (start: the two settings above, 6 runs each, 45 min, a new random trial set per generation on seeds 7 or 8), final settings checked on seeds 101 to 105:")
print("%-4s %5s %-12s %-12s" % ("run", "gens", "hold of 80", "turn of 40 (right half, left half)"))
for i, f in enumerate(sorted(glob.glob(os.path.join(R8, "**", "opt3_*.jsonl"), recursive=True), key=lambda f: int(re.search(r"opt3_(\d+)", f).group(1)))): pass
src = [json.loads(l)["top"][0] for l in open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "compass_rot.jsonl"))]
for i in range(len(src)):
    t = cat(R9, "checkR%d.txt" % i)
    h = re.search(r"total (\d+) of 80", t).group(1); m = re.search(r"turn, .*total (\d+) of 40; passes right half \(towards higher\): (\d+) of 20, left half \(lower\): (\d+) of 20", t)
    print("%-4d %5s %-12s %s of 40 (%s, %s)" % (i + 1, "", h, m.group(1), m.group(2), m.group(3)))
print("\nHEMIBRAIN CHECK (runner, run 46116624744):"); print(cat(R6, "hemibrain_check.txt"))
