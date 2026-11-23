#!/usr/bin/env python3
"""Directed search for a held bump: a (mu + lambda) evolution strategy with self-adapted log-normal steps, pure Python. Probe for research/atlas-a.md section 4c.

  python3 tools/connectome/compass_opt.py evolve SEED MINUTES OUT.jsonl [norm=N] [mu=8] [lam=32]    runs generations until MINUTES of wall time have passed
  python3 tools/connectome/compass_opt.py check IN.jsonl K OUT.json                                  the K best individuals of IN on circuit seeds 101 to 105 and all 16 cues

Genes: the 19 pair scales (w_<pre>_<post>, log10 pA per synapse, clipped to -2.3..0.5) and one background current per population (I_e_<pop>, pA, 100..450).
Fixed: synaptic time constants 5 ms, turn_w 1, the sign assumptions of compass_circuit.py, norm from the command line.
Score of an individual (circuit seed 7, cues at ring positions 2, 6, 10, 14, 3,500 ms; late window 3,000 to 3,500 ms), mean over the four cues of
  rate * (0.25 + 0.25 * strength + 0.25 * near_peak + 0.25 * (1 - min(err, 90) / 90))
with rate = max(0, 1 - d / 2), d = decades between the mean E-PG rate and the band 2 to 60 Hz (0 inside), strength and near_peak as in compass_trial.analyse, err the angular error of the late bump.
The start population is the best 32 of 128 random draws. The first term rewards being in the band, the others reward a bump of one place that sits at the cue. 1.0 is a perfect score. Held (compass_trial.held) is tested separately.
Individuals whose four trials all hold are re-run on circuit seed 8; the 'passes' field counts held trials of 8. Workers: $JOBS.
"""
import json, math, os, random, sys, time
from multiprocessing import Pool
import compass_trial as ct
import compass_circuit as cc
import compass_search as cs

CUES = (2, 6, 10, 14)
POPS = ("EPG", "PENa", "PENb", "PEG", "D7")


def trial_score(t):
    h = t["late"]
    if h["angle"] is None: return 0.0
    hz = max(h["mean_hz"], 1e-3)
    d = max(0.0, math.log10(2.0 / hz), math.log10(hz / 60.0))
    rate = max(0.0, 1.0 - d / 2.0)
    return rate * (0.25 + 0.25 * h["strength"] + 0.25 * h["near_peak"] + 0.25 * (1.0 - min(h["err"], 90.0) / 90.0))


def make(g, fixed):
    p = {"tau_syn_ex": 5.0, "tau_syn_in": 5.0, "turn_w": 1.0}
    for k in cs.PAIRS: p["w_" + k] = round(10 ** g["w"][k], 5)
    for pid in POPS: p["I_e_" + pid] = round(g["I"][pid], 1)
    p.update(fixed)
    return p


def evaluate(a):
    g, fixed = a
    p = make(g, fixed)
    ts = [ct.trial(p, c, 7) for c in CUES]
    held = [ct.held(t) for t in ts]
    out = {"genes": g, "params": p, "score": sum(trial_score(t) for t in ts) / len(ts), "held7": held, "late": [(round(t["late"]["err"], 1) if t["late"]["err"] is not None else None, round(t["late"]["strength"], 2), round(t["late"]["mean_hz"], 1)) for t in ts]}
    if all(held):
        out["held8"] = [ct.held(ct.trial(p, c, 8)) for c in CUES]
    return out


def clip(x, lo, hi): return max(lo, min(hi, x))


def random_genes(rng):
    return {"w": {k: rng.uniform(-2.3, 0.0) for k in cs.PAIRS}, "I": {pid: rng.uniform(150, 360) for pid in POPS}, "sigma": 0.3}


def mutate(g, rng):
    s = clip(g["sigma"] * math.exp(0.3 * rng.gauss(0, 1)), 0.02, 1.0)
    return {"w": {k: clip(v + s * rng.gauss(0, 1), -2.3, 0.5) for k, v in g["w"].items()}, "I": {k: clip(v + s * 100 * rng.gauss(0, 1), 100, 450) for k, v in g["I"].items()}, "sigma": s}


def evolve(seed, minutes, path, fixed, mu, lam):
    rng = random.Random(seed)
    t0 = time.time()
    out = open(path, "w")
    with Pool(int(os.environ.get("JOBS", os.cpu_count() or 4))) as pool:
        pop = pool.map(evaluate, [(random_genes(rng), fixed) for _ in range(4 * lam)], chunksize=1)   # four times lambda random draws, the best lambda start
        pop = sorted(pop, key=lambda r: r["score"], reverse=True)[:lam]
        gen = 0
        while True:
            pop.sort(key=lambda r: r["score"], reverse=True)
            passing = [r for r in pop if r.get("held8") and all(r["held8"])]
            held4 = sum(1 for r in pop if all(r["held7"]))
            print("gen %d  best %.4f  mean-of-top-mu %.4f  held at 4 cues (seed 7): %d  held at 8 trials: %d  %.0f s" % (gen, pop[0]["score"], sum(r["score"] for r in pop[:mu]) / mu, held4, len(passing), time.time() - t0), flush=True)
            out.write(json.dumps({"gen": gen, "best": pop[0]["score"], "top": [{k: r[k] for k in ("params", "score", "held7", "held8", "late") if k in r} | {"sigma": r["genes"]["sigma"]} for r in pop[:3]]}) + "\n"); out.flush()
            if time.time() - t0 > minutes * 60: break
            parents = [r["genes"] for r in pop[:mu]]
            kids = pool.map(evaluate, [(mutate(rng.choice(parents), rng), fixed) for _ in range(lam)], chunksize=1)
            pop = sorted(kids + pop[:mu], key=lambda r: r["score"], reverse=True)[:lam]   # (mu + lambda): the mu best parents compete with the children, so the best score never falls
            gen += 1
    return pop


def check_one(a):
    p, seed, cue = a
    t = ct.trial(p, cue, seed)
    return {"seed": seed, "cue": cue, "held": ct.held(t), "err": t["late"]["err"], "strength": t["late"]["strength"], "mean_hz": t["late"]["mean_hz"], "angle": t["late"]["angle"]}


def check(inp, k, path):
    best = {}
    for l in open(inp):
        for r in json.loads(l)["top"]: best[json.dumps(r["params"], sort_keys=True)] = r
    top = sorted(best.values(), key=lambda r: r["score"], reverse=True)[:k]
    res = []
    with Pool(int(os.environ.get("JOBS", os.cpu_count() or 4))) as pool:
        for r in top:
            rows = pool.map(check_one, [(r["params"], s, c) for s in range(101, 106) for c in range(16)], chunksize=2)
            by_seed = {s: sum(1 for x in rows if x["seed"] == s and x["held"]) for s in range(101, 106)}
            res.append({"params": r["params"], "score": r["score"], "held_of_16_by_seed": by_seed, "rows": rows})
            print("score %.4f  held of 16 cues on seeds 101..105: %s" % (r["score"], by_seed), flush=True)
    json.dump(res, open(path, "w"))


if __name__ == "__main__":
    if sys.argv[1] == "evolve":
        kv = dict(a.split("=") for a in sys.argv[5:])
        fixed = {"norm": int(kv.get("norm", 0))}
        os.makedirs(os.path.dirname(sys.argv[4]) or ".", exist_ok=True)
        evolve(int(sys.argv[2]), float(sys.argv[3]), sys.argv[4], fixed, int(kv.get("mu", 8)), int(kv.get("lam", 32)))
    else:
        os.makedirs(os.path.dirname(sys.argv[4]) or ".", exist_ok=True)
        check(sys.argv[2], int(sys.argv[3]), sys.argv[4])
