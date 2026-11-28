#!/usr/bin/env python3
"""Full check of one setting (research/atlas-a.md 4c): hold at all 16 cues and turn both ways, on circuit seeds 101 to 105; and the resting places of the bump.

  python3 compass_check.py PARAMS.json OUT.json         PARAMS.json: a parameter dict, or a line-per-setting file's first {"params": ...}; needs pulse_w for the turn trials

Hold: compass_trial.held at 3,000-3,500 ms for the 16 cues. Turn: compass_opt2.turn_trial for cues 2, 6, 10, 14 and both directions (pass: shift of 2 to 10 headings in the predicted direction, stays, bump compact).
Places: the late angle of every active trial to the nearest heading (22.5 deg); per resting heading the number of (cue, seed) trials; the mean absolute error between the cue heading and the resting heading, in headings
and degrees, over active trials (mean rate above 0.3 Hz); the same restricted to held trials.
"""
import collections, json, os, sys
from multiprocessing import Pool
import compass_trial as ct
import compass_opt2 as o2


def hold_one(a):
    p, cue, seed = a
    t = ct.trial(p, cue, seed)
    h = t["late"]
    return {"cue": cue, "seed": seed, "held": ct.held(t), "angle": h["angle"], "mean_hz": h["mean_hz"], "strength": h["strength"], "err": h["err"]}


def turn_one(a):
    p, cue, d, seed = a
    return o2.turn_trial(p, cue, d, seed)


def load(path):
    d = json.load(open(path)) if not path.endswith(".jsonl") else json.loads(open(path).readline())
    d = d.get("top", [d])[0] if "top" in d else d
    return d.get("params", d)


def circ(a, b): return abs((a - b + 8) % 16 - 8)


if __name__ == "__main__":
    p = load(sys.argv[1])
    with Pool(int(os.environ.get("JOBS", os.cpu_count() or 4))) as pool:
        hold = pool.map(hold_one, [(p, c, s) for s in range(101, 106) for c in range(16)], chunksize=2)
        turn = pool.map(turn_one, [(p, c, d, s) for s in range(101, 106) for c in (2, 6, 10, 14) for d in (1, -1)], chunksize=1) if "pulse_w" in p else []
    print("hold, 16 cues x seeds 101..105:", {s: sum(1 for x in hold if x["seed"] == s and x["held"]) for s in range(101, 106)}, "total", sum(x["held"] for x in hold), "of 80")
    if turn: print("turn, 4 cues x 2 directions x seeds 101..105: passes by seed", {s: sum(1 for x in turn if x["seed"] == s and x["ok"]) for s in range(101, 106)}, "total", sum(x["ok"] for x in turn), "of 40;",
          "passes right half (towards higher):", sum(1 for x in turn if x["dir"] == 1 and x["ok"]), "of 20, left half (lower):", sum(1 for x in turn if x["dir"] == -1 and x["ok"]), "of 20")
    sh = [x["x"] for x in turn if x["x"] is not None]
    if turn: print("turn shifts in the predicted direction, headings: mean %.2f, min %.2f, max %.2f (n=%d)" % (sum(sh) / len(sh), min(sh), max(sh), len(sh)) if sh else "no turn shifts")
    act = [x for x in hold if x["angle"] is not None and x["mean_hz"] >= 0.3]
    rest = lambda x: round(x["angle"] / 22.5) % 16
    cnt = collections.Counter(rest(x) for x in act)
    print("resting headings of the late bump (nearest heading) over %d active trials (16 cues x 5 seeds):" % len(act))
    print("%-8s %-7s %-34s %s" % ("heading", "trials", "cues that end here (cue: seeds)", "of which held"))
    for h, n in sorted(cnt.items()):
        cues = collections.Counter(x["cue"] for x in act if rest(x) == h)
        print("%-8d %-7d %-34s %d" % (h, n, " ".join("%d:%d" % (c, k) for c, k in sorted(cues.items())), sum(1 for x in act if rest(x) == h and x["held"])))
    e = [circ(x["cue"], rest(x)) for x in act]; eh = [circ(x["cue"], rest(x)) for x in act if x["held"]]
    print("mean |cue heading - resting heading|: %.2f headings (%.1f deg) over active trials, %.2f headings (%.1f deg) over held trials; largest %d headings" % (sum(e) / len(e), 22.5 * sum(e) / len(e), sum(eh) / max(1, len(eh)), 22.5 * sum(eh) / max(1, len(eh)), max(e)))
    print("distinct resting headings:", len(cnt))
    json.dump({"params": p, "hold": hold, "turn": turn}, open(sys.argv[2], "w"))
