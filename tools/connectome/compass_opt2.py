#!/usr/bin/env python3
"""Evolution strategy with a turn term (research/atlas-a.md 4c, step 1 of the second brief). Starts from settings that already hold (compass_candidates.jsonl).

  python3 compass_opt2.py evolve START_LINE SEED MINUTES OUT.jsonl [mu=4] [lam=16]     START_LINE: 1-based line of compass_candidates.jsonl

Turn trial (circuit seed s): a cue at heading `cue` for 500 ms, the bump settles, then for one second (1,500 to 2,500 ms) a spike train of 500 Hz at `pulse_w` pA per spike
goes onto the P-EN cells of one half of the bridge (the constant background current on both halves stays). Windows: before 1,200-1,500 ms, after 2,700-3,200 and 3,500-4,000 ms.
shift = signed angle between 'before' and the later window in headings (22.5 deg). Direction +1 = the left half (turn_right input; wiring predicts higher headings, see compass_heading.py),
-1 = the right half (lower headings). A turn trial passes when direction * shift is between 2 and 10 headings, the two 'after' angles differ by at most 1 heading (it stays),
the bump is still compact and in the rate band (strength above 0.5, near_peak above 0.7, 2 to 60 Hz mean rate). The hold trials are those of compass_trial.held (no pulse).
With rot=1 each generation uses a new random set of 4 hold cues, 4 (cue, direction) turn trials among the 32 (16 cues, both directions) and circuit seed 7 or 8 for all individuals (parents are re-evaluated on it); file=NAME takes the start from another candidates file.
Genes: the 19 pair scales, 5 population currents, turn_w (background gain, log10), pulse_w (log10). Score = 0.5 * hold + 0.5 * turn, with hold = mean of compass_opt.trial_score over
cues 2, 6, 10, 14, and turn = mean over (cue 2, +1), (cue 10, -1), (cue 10, +1), (cue 2, -1) of gate * f(x) * p: gate = min(1, strength / 0.5) times 1 in band (0.3 outside),
f = 0 for x <= 0, x / 2 up to 2, 1 up to 10, 1 - (x - 10) / 10 above (0 from 20), p = max(0.3, 1 - |after2 - after1| / 4). 1.0 is perfect. Workers: $JOBS.
"""
import json, math, os, random, sys, time
from multiprocessing import Pool
import compass_trial as ct
import compass_circuit as cc
import compass_search as cs
import compass_opt as co

HERE = os.path.dirname(os.path.abspath(__file__))
HOLD_CUES = (2, 6, 10, 14)
TURN_TRIALS = ((2, 1), (10, -1), (10, 1), (2, -1))
ON, OFF, MS = 1500, 2500, 4000
WB, WA1, WA2 = (1200, 1500), (2700, 3200), (3500, 4000)


def hd(a, b): return ((b - a + 180) % 360 - 180) / 22.5


def turn_circuit(p, cue, d, seed):
    c = ct.circuit(p, cue, 0.0, seed)
    src = next(i for i in c["inputs"] if i["name"] == ("turn_right" if d > 0 else "turn_left"))
    times = [round(ON + 2.0 * k, 1) for k in range(int((OFF - ON) / 2.0))]
    c["inputs"].append({"name": "pulse", "kind": "spike_generator", "targets": src["targets"], "weight": p["pulse_w"], "spike_times": times})
    return c


def turn_trial(p, cue, d, seed):
    sp, _ = ct.run(turn_circuit(p, cue, d, seed), MS)
    b = ct.analyse(ct.profile(sp, *WB)); a1 = ct.analyse(ct.profile(sp, *WA1)); a2 = ct.analyse(ct.profile(sp, *WA2))
    out = {"cue": cue, "dir": d, "seed": seed, "ok": False, "x": None, "stay": None, "strength": a2["strength"], "mean_hz": a2["mean_hz"]}
    if None in (b["angle"], a1["angle"], a2["angle"]): out["score"] = 0.0; return out
    x = d * hd(b["angle"], a2["angle"]); dd = abs(hd(a1["angle"], a2["angle"]))
    out.update(x=round(x, 2), stay=round(dd, 2), before=round(b["angle"], 1), after=round(a2["angle"], 1))
    out["ok"] = 2 <= x <= 10 and dd <= 1 and a2["strength"] > 0.5 and a2["near_peak"] > 0.7 and 2 < a2["mean_hz"] < 60
    gate = min(1.0, a2["strength"] / 0.5) * (1.0 if 2 <= a2["mean_hz"] <= 60 else 0.3)
    f = 0.0 if x <= 0 else x / 2 if x < 2 else 1.0 if x <= 10 else max(0.0, 1 - (x - 10) / 10)
    out["score"] = gate * f * max(0.3, 1 - dd / 4)
    return out


def make(g, base):
    p = dict(base)
    for k in cs.PAIRS: p["w_" + k] = round(10 ** g["w"][k], 5)
    for pid in co.POPS: p["I_e_" + pid] = round(g["I"][pid], 1)
    p["turn_w"] = round(10 ** g["tw"], 3); p["pulse_w"] = round(10 ** g["pw"], 2)
    return p


ALL_TURNS = tuple((c, d) for c in range(16) for d in (1, -1))


def evaluate(a):
    g, base = a[:2]
    hc, tt, sd = a[2:] if len(a) > 2 else (HOLD_CUES, TURN_TRIALS, 7)
    p = make(g, base)
    hold = [ct.trial(p, c, sd) for c in hc]
    turns = [turn_trial(p, c, d, sd) for c, d in tt]
    out = {"genes": g, "params": p, "held7": [ct.held(t) for t in hold], "turn7": [t["ok"] for t in turns], "x7": [t["x"] for t in turns],
           "hold_score": sum(co.trial_score(t) for t in hold) / len(hold), "turn_score": sum(t["score"] for t in turns) / len(turns)}
    out["score"] = 0.5 * out["hold_score"] + 0.5 * out["turn_score"]
    if all(out["held7"]) and all(out["turn7"]):
        sd2 = 8 if sd == 7 else 7
        out["held8"] = [ct.held(ct.trial(p, c, sd2)) for c in hc]; out["turn8"] = [turn_trial(p, c, d, sd2)["ok"] for c, d in tt]
    return out


def genes_of(p, rng, sigma):
    g = {"w": {k: math.log10(p["w_" + k]) for k in cs.PAIRS}, "I": {k: p["I_e_" + k] for k in co.POPS}, "tw": math.log10(p.get("turn_w", 1.0)), "pw": rng.uniform(0.5, 2.5), "sigma": sigma}
    return g


def mutate(g, rng):
    s = co.clip(g["sigma"] * math.exp(0.3 * rng.gauss(0, 1)), 0.01, 0.6)
    return {"w": {k: co.clip(v + s * rng.gauss(0, 1), -2.3, 0.5) for k, v in g["w"].items()}, "I": {k: co.clip(v + s * 100 * rng.gauss(0, 1), 100, 450) for k, v in g["I"].items()},
            "tw": co.clip(g["tw"] + s * 2 * rng.gauss(0, 1), -0.7, 1.7), "pw": co.clip(g["pw"] + s * 3 * rng.gauss(0, 1), 0.0, 3.0), "sigma": s}


def evolve(line, seed, minutes, path, mu, lam, rot=False, fname="compass_candidates.jsonl"):
    start = json.loads(open(os.path.join(HERE, fname)).read().splitlines()[line - 1])["top"][0]["params"]
    base = {k: v for k, v in start.items() if not (k.startswith("w_") or k.startswith("I_e_") or k in ("turn_w", "pulse_w"))}
    rng = random.Random(seed); t0 = time.time(); out = open(path, "w")
    with Pool(int(os.environ.get("JOBS", os.cpu_count() or 4))) as pool:
        first = [genes_of(start, rng, 0.05)]
        pop = pool.map(evaluate, [(g if i == 0 else mutate(g, rng), base) for i, g in enumerate(first * 1 + [genes_of(start, rng, 0.05) for _ in range(2 * lam - 1)])], chunksize=1)
        pop = sorted(pop, key=lambda r: r["score"], reverse=True)[:lam]
        gen = 0
        while True:
            pop.sort(key=lambda r: r["score"], reverse=True)
            both = [r for r in pop if all(r["held7"]) and all(r["turn7"])]
            p8 = [r for r in pop if r.get("held8") and all(r["held8"]) and all(r["turn8"])]
            print("gen %d best %.4f (hold %.3f turn %.3f)  holds+turns 4/4 on seed 7: %d  and on seed 8: %d  turn shifts of best %s  %.0f s" % (gen, pop[0]["score"], pop[0]["hold_score"], pop[0]["turn_score"], len(both), len(p8), pop[0]["x7"], time.time() - t0), flush=True)
            out.write(json.dumps({"gen": gen, "best": pop[0]["score"], "top": [{k: r[k] for k in ("params", "score", "hold_score", "turn_score", "held7", "turn7", "x7", "held8", "turn8") if k in r} for r in pop[:3]]}) + "\n"); out.flush()
            if time.time() - t0 > minutes * 60: break
            if rot:   # a new random set of trials each generation, the same for everyone (parents are re-evaluated on it); circuit seeds 7 and 8 only, 101 to 105 stay unseen
                hc = tuple(rng.sample(range(16), 4)); tt = tuple(rng.sample(ALL_TURNS, 4)); sd = rng.choice((7, 8)); par = [r["genes"] for r in pop[:mu]]
                res = pool.map(evaluate, [(g_, base, hc, tt, sd) for g_ in par] + [(mutate(rng.choice(par), rng), base, hc, tt, sd) for _ in range(lam)], chunksize=1)
                pop = sorted(res, key=lambda r: r["score"], reverse=True)[:lam]
            else:
                kids = pool.map(evaluate, [(mutate(rng.choice([r["genes"] for r in pop[:mu]]), rng), base) for _ in range(lam)], chunksize=1)
                pop = sorted(kids + pop[:mu], key=lambda r: r["score"], reverse=True)[:lam]
            gen += 1


if __name__ == "__main__":
    if sys.argv[1] == "evolve":
        kv = dict(a.split("=") for a in sys.argv[6:])
        os.makedirs(os.path.dirname(sys.argv[5]) or ".", exist_ok=True)
        evolve(int(sys.argv[2]), int(sys.argv[3]), float(sys.argv[4]), sys.argv[5], int(kv.get("mu", 4)), int(kv.get("lam", 16)), kv.get("rot") == "1", kv.get("file", "compass_candidates.jsonl"))
