#!/usr/bin/env python3
"""Fix the model point of the worm chapter (iteration A2, step 1). Runs on a GitHub runner, never on a desktop.

  python3 tools/connectome/worm_point.py            grid below, shard $SHARD of $SHARDS, workers = every CPU
  python3 tools/connectome/worm_point.py k=v ...    one point, the given parameters (scale_chem, I_e, min_count, tau_syn_ex)

Circuit: worm_circuit.build with every synapse excitatory, weights = synapse count x scale_chem, no electrical coupling, head touch (150 Hz to each of ALML, ALMR, AVM for 400 ms from 500 ms).
Each trial runs 1500 ms. Windows: base [100, 500), response [500, 1200) (the probe's window), touch [500, 900), after [1000, 1500).
Circuit seeds SEEDS = 1, 2, 3, 101 ... 105. Conditions per point (stops at the first condition that fails, so most points cost little):
  intact       A >= 5 Hz, B < 1 Hz in the response window, A and B < 1 Hz in the baseline and in the window after the touch
  silence X    X in AVA, AVB, AVD, PVC: A < 1 Hz in the response window.   AVE: A >= 5 Hz.
  route alone  every shortest class-level route (touch cells to DA/VA, on the circuit with the synapse floor min_count) kept alone, all other command classes silenced: A reported (it carries the response if A >= 5 Hz).
Output: out/worm_point_SHARD.jsonl (one record per point) and a summary table on stdout (margins: lowest A intact, highest B intact, highest A with a class silenced, lowest A with AVE silenced).
"""
import itertools, json, os, sys
from multiprocessing import Pool
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import worm_circuit as wc
import worm_trial as wt

SEEDS = (1, 2, 3, 101, 102, 103, 104, 105)
MS = 1500
BASE, RESP, TOUCH, AFTER = (100.0, 500.0), (500.0, 1200.0), (500.0, 900.0), (1000.0, 1500.0)
NEED = ("AVA", "AVB", "AVD", "PVC")   # silencing each abolishes the response
GRID = dict(scale_chem=(14.6, 15.5, 16.5, 17.7), I_e=(110.0, 130.0, 150.0, 170.0, 190.0), min_count=(3, 5))


def routes(min_count):
    """All shortest class-level routes from the head touch cells (ALM, AVM) to the A-type motor neurons (DA, VA), on the chemical graph with at least min_count synapses.
    Returns (hops, sorted list of tuples of the interneuron classes on a route)."""
    d = wc.load(); ns = d["neurons"]
    g = {}
    for a, b, n, k in d["connections"]:
        if k == "chemical" and n >= min_count and ns[a]["class"] != ns[b]["class"]:
            g.setdefault(ns[a]["class"], set()).add(ns[b]["class"])
    src, dst = ("ALM", "AVM"), ("DA", "VA")
    dist = {s: 0 for s in src}; frontier = list(src)
    while frontier and not any(t in dist for t in dst):
        nxt = []
        for u in frontier:
            for v in sorted(g.get(u, ())):
                if v not in dist: dist[v] = dist[u] + 1; nxt.append(v)
        frontier = nxt
    best = min((dist[t] for t in dst if t in dist), default=None)
    if best is None: return None, []
    out = set()

    def walk(u, path):
        if len(path) - 1 == best:
            if u in dst: out.add(tuple(sorted(c for c in path if c in wc.COMMAND)))
            return
        for v in sorted(g.get(u, ())):
            walk(v, path + [v])

    for s in src: walk(s, [s])
    return best, sorted(out)


def run(p, silence, seed):
    q = dict(p); q["silence"] = silence; q["touch"] = "head"; q["touch_seed"] = seed * 1000 + 1
    c = wc.build(q); c["seed"] = seed
    cl = [wt.CLASS_OF[n] for n in c["meta"]["cells"]]
    sp = wt.run(c, MS)
    n_of = {}
    for k in cl: n_of[k] = n_of.get(k, 0) + 1

    def rate(classes, w):
        n = sum(n_of.get(k, 0) for k in classes)
        return 0.0 if n == 0 else sum(1 for t, i in sp if w[0] <= t < w[1] and cl[i] in classes) / n / ((w[1] - w[0]) / 1000.0)

    r = {}
    for tag, cs in (("A", wc.A_CLASSES), ("B", wc.B_CLASSES)):
        r[tag] = rate(cs, RESP); r[tag + "_touch"] = rate(cs, TOUCH); r[tag + "_base"] = rate(cs, BASE); r[tag + "_after"] = rate(cs, AFTER)
    return {k: round(v, 2) for k, v in r.items()}


def point(p):
    rec = {"params": p, "intact": [], "silence": {}, "routes": {}, "failed": None}
    for s in SEEDS:
        r = run(p, [], s); rec["intact"].append(r)
        if not (r["A"] >= 5 and r["B"] < 1 and max(r["A_base"], r["B_base"], r["A_after"], r["B_after"]) < 1):
            rec["failed"] = "intact seed %d" % s; return rec
    for k in NEED + ("AVE",):
        rec["silence"][k] = [run(p, [k], s) for s in SEEDS]
        a = [r["A"] for r in rec["silence"][k]]
        if (k in NEED and max(a) >= 1) or (k == "AVE" and min(a) < 5):
            rec["failed"] = "silence " + k; return rec
    hops, rts = routes(p["min_count"])
    rec["hops"] = hops
    for rt in rts:
        rec["routes"]["+".join(rt) or "none"] = [run(p, [c for c in wc.COMMAND if c not in rt], s) for s in SEEDS]
    return rec


def margins(rec):
    m = {"A_intact_min": min(r["A"] for r in rec["intact"]), "B_intact_max": max(r["B"] for r in rec["intact"])}
    if rec["silence"]:
        m["A_max_needed_silenced"] = max(r["A"] for k in NEED for r in rec["silence"].get(k, []))
        m["A_min_AVE_silenced"] = min(r["A"] for r in rec["silence"].get("AVE", [{"A": -1}]))
    return m


if __name__ == "__main__":
    shard, shards = int(os.environ.get("SHARD", 0)), int(os.environ.get("SHARDS", 1))
    if len(sys.argv) > 1:
        p = {"tau_syn_ex": 2.0}
        for kv in sys.argv[1:]:
            k, v = kv.split("="); p[k] = type(wc.DEFAULTS[k])(v)
        pts = [p]
    else:
        pts = [dict(scale_chem=a, I_e=b, min_count=c, tau_syn_ex=2.0) for a, b, c in itertools.product(*GRID.values())]
        pts = pts[shard::shards]
    os.makedirs("out", exist_ok=True)
    with Pool(int(os.environ.get("JOBS", os.cpu_count() or 2))) as pool:
        recs = pool.map(point, pts, chunksize=1)
    with open("out/worm_point_%d.jsonl" % shard, "w") as f:
        for r in recs: f.write(json.dumps(r) + "\n")
    for r in recs:
        p = r["params"]
        print("scale %.1f I_e %.0f min_count %d: %s" % (p["scale_chem"], p["I_e"], p["min_count"], "HOLDS" if not r["failed"] else "fails at " + r["failed"]), json.dumps(margins(r)) if not r["failed"] else "", flush=True)
        if not r["failed"]:
            print("   routes (%s hops):" % r.get("hops"), {k: [x["A"] for x in v] for k, v in r["routes"].items()})
