#!/usr/bin/env python3
"""Seed catalog/histograms/<task>.json with SYNTHETIC post-solve distributions (docs/HISTOGRAMS.md).

For each non-diagnosis level: take the reference circuit, generate many varied circuits (resize free populations, scale weights, add redundant or
idle neurons, perturb neuron parameters inside the lab's slider ranges, add per-edge weight spread), score each with spike-run on seeds 1 and 2,
keep the ones that pass both, and record neurons, energy_hz (spikes / neuron / episode second) and the task's own quality metrics.

usage: gen_histograms.py [--tasks a,b,...] [--budget SECONDS] [--target N] [--min N] [--workers N<=4] [--seed N] [--bin path] [--probe]
Deterministic for a given --seed (candidate i of a level uses its own RNG; the variation strength adapts per batch from the pass rate, in index order).
"""
import argparse, copy, glob, json, math, os, random, subprocess, sys, tempfile, time
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT_DIR = os.path.join(ROOT, "catalog", "histograms")
SEEDS = (1, 2)
BATCH = 8

# Neuron parameter ranges the lab's sliders offer (godot/game/circuits.gd ADVANCED): key -> (min, max, step, NEST default)
ADV = {
    "iaf_psc_alpha": {"tau_m": (2, 60, .5, 10), "V_th": (-60, -40, .5, -55), "I_e": (-400, 800, 5, 0), "tau_syn_ex": (.2, 50, .1, 2), "tau_syn_in": (.2, 50, .1, 2)},
    "aeif_cond_alpha": {"a": (-2, 10, .1, 4), "b": (0, 200, 1, 80.5), "tau_w": (5, 400, 1, 144), "I_e": (-400, 1000, 5, 0), "tau_syn_ex": (.2, 50, .1, .2), "tau_syn_in": (.2, 50, .1, 2)},
    "izhikevich": {"a": (.01, .15, .005, .02), "b": (.1, .3, .005, .2), "c": (-70, -45, .5, -65), "d": (.5, 10, .5, 8), "I_e": (-10, 30, .5, 0)},
    "hh_psc_alpha": {"I_e": (-100, 600, 5, 0), "tau_syn_ex": (.2, 50, .1, 2), "tau_syn_in": (.2, 50, .1, 2)},
}
# iaf_cond_alpha has no slider table; borrow the iaf_psc_alpha-like keys with its own (loose) NEST defaults
ADV["iaf_cond_alpha"] = {"tau_syn_ex": (.2, 50, .1, .2), "tau_syn_in": (.2, 50, .1, 2), "I_e": (-400, 800, 5, 0)}
CLONABLE = ("iaf_psc_alpha", "aeif_cond_alpha", "iaf_cond_alpha")


def find_bin(given):
    for p in ([given] if given else []) + [os.path.join(ROOT, "build/gd-debug/engine/spike-run"), os.path.join(ROOT, "build/engine/spike-run")]:
        if p and os.path.exists(p):
            return p
    sys.exit("no spike-run binary found")


def levels():
    """task id -> reference circuit path, for every non-diagnosis level (circuits/*.json carry meta.task)."""
    out = {}
    for f in sorted(glob.glob(os.path.join(ROOT, "circuits", "*.json"))):
        t = json.load(open(f)).get("meta", {}).get("task", "")
        if t and not t.startswith("dx_"):
            out[t] = f
    return out


# ---------------------------------------------------------------- circuit surgery
def _sel_pop(s):
    return s if isinstance(s, str) else s["pop"]


def _sel_has_idx(s):
    return isinstance(s, dict) and "idx" in s


def analyse(c):
    """Resizable groups: populations of role 'neuron' that no selector pins by index, linked through one_to_one projections and not wired by a
    sparse `edges` projection; one_to_one partners resize together (and only if all of them may)."""
    pops = {p["id"]: p for p in c["populations"]}
    pinned = set()
    for sec, key in (("inputs", "targets"), ("outputs", "neurons")):
        for x in c.get(sec, []):
            for s in x.get(key, []):
                if _sel_has_idx(s):
                    pinned.add(_sel_pop(s))
    ok = {i for i, p in pops.items() if p.get("role", "neuron") == "neuron" and i not in pinned}
    parent = {i: i for i in pops}

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a
    for pr in c["projections"]:
        a, b = pr["pre"], pr["post"]
        if "edges" in pr:
            if a in pops and b in pops and len(pr["edges"]) != pops[a]["n"] * pops[b]["n"]:
                ok.discard(a); ok.discard(b)  # structured (sparse) wiring: sizes are part of the design
        elif pr.get("rule") == "one_to_one" and a != b:
            parent[find(a)] = find(b)
        if pr.get("synapse", {}).get("synapse_model", "static_synapse") != "static_synapse":
            ok.discard(a); ok.discard(b)
    groups = {}
    for i in pops:
        groups.setdefault(find(i), []).append(i)
    res = []
    for g in groups.values():
        if all(i in ok for i in g) and len({pops[i]["n"] for i in g}) == 1:
            res.append(g)
    return res


def set_pos(p, n):
    x, y = (p.get("pos") or [[100, 100]])[0]
    p["pos"] = [[x, y + 24 * k] for k in range(n)]


def resize(c, rng, t, comp):
    """Resize each free group by a log-normal factor (slightly biased to smaller). comp = exponent of the weight compensation for a bigger/smaller pre."""
    pops = {p["id"]: p for p in c["populations"]}
    factor = {}
    for g in analyse(c):
        if rng.random() > 0.65:
            continue
        n0 = pops[g[0]]["n"]
        n1 = max(1, int(round(n0 * math.exp(rng.gauss(-0.1 * t, 0.45 * t)))))
        if n0 == 1 and rng.random() < 0.5:
            n1 = 1 + int(rng.random() < 0.5 * t) + int(rng.random() < 0.25 * t)
        if n1 == n0:
            continue
        for i in g:
            factor[i] = (n0, n1)
    if not factor:
        return
    for i, (n0, n1) in factor.items():
        pops[i]["n"] = n1
        set_pos(pops[i], n1)
    for pr in c["projections"]:
        a, b = pr["pre"], pr["post"]
        if a not in factor and b not in factor:
            continue
        f_pre = factor[a][1] / factor[a][0] if a in factor else 1.0
        joint = pr.get("rule") == "one_to_one"
        k = 1.0 if joint else f_pre ** (-comp)
        if "edges" in pr:
            ws = [e[2] for e in pr["edges"]]; ds = [e[3] for e in pr["edges"]]
            na, nb = pops[a]["n"], pops[b]["n"]
            pr["edges"] = [[i, j, rng.choice(ws) * k, ds[0] if len(set(ds)) == 1 else rng.choice(ds)] for i in range(na) for j in range(nb)]
        else:
            w = pr.get("synapse", {}).get("weight")
            if isinstance(w, (int, float)):
                pr["synapse"]["weight"] = w * k


def scale_weights(c, rng, t):
    pops = {p["id"]: p for p in c["populations"]}
    g = math.exp(rng.gauss(0, 0.12 * t))
    for pr in c["projections"]:
        k = g * math.exp(rng.gauss(0, 0.25 * t))
        if "edges" in pr:
            for e in pr["edges"]:
                e[2] *= k
        else:
            w = pr.get("synapse", {}).get("weight")
            if isinstance(w, (int, float)):
                pr["synapse"]["weight"] = w * k
    # weight spread (CV) on a random share of the projections; rule projections become explicit edges (what the lab's segment wiring produces)
    if rng.random() < 0.5:
        for pr in c["projections"]:
            if rng.random() > 0.6:
                continue
            cv = rng.uniform(0.05, 0.45) * min(1.0, 0.5 + t)
            if "edges" not in pr:
                syn = pr.get("synapse", {})
                ok = (not (set(syn) - {"synapse_model", "weight", "delay"}) and syn.get("synapse_model", "static_synapse") == "static_synapse"
                      and isinstance(syn.get("weight"), (int, float)) and pr.get("rule") in ("all_to_all", "one_to_one")
                      and not (set(pr) - {"pre", "post", "rule", "synapse", "allow_autapses"}))
                if not ok:
                    continue
                na, nb = pops[pr["pre"]]["n"], pops[pr["post"]]["n"]
                auto = pr.get("allow_autapses", True)
                if pr["rule"] == "all_to_all":
                    pairs = [(i, j) for i in range(na) for j in range(nb) if auto or pr["pre"] != pr["post"] or i != j]
                elif na == nb:
                    pairs = [(i, i) for i in range(na)]
                else:
                    continue
                w, d = syn["weight"], syn.get("delay", 1.0)
                pre, post = pr["pre"], pr["post"]
                pr.clear()
                pr.update({"pre": pre, "post": post, "edges": [[i, j, w, d] for i, j in pairs]})
            if pr["edges"]:
                pr.pop("weight_sd", None)
                pr["weight_cv"] = cv


def snap(v, lo, hi, step):
    v = min(hi, max(lo, v))
    return round(round(v / step) * step, 6)


def perturb_params(c, rng, t):
    for p in c["populations"]:
        if rng.random() > 0.5:
            continue
        adv = ADV.get(p["model"])
        if adv:
            keys = rng.sample(sorted(adv), min(len(adv), rng.choice((1, 1, 2, 3))))
            for kx in keys:
                lo, hi, step, dflt = adv[kx]
                cur = p["params"].get(kx, dflt)
                sd = 0.04 * t * (hi - lo)
                if kx == "I_e":
                    sd = 0.015 * t * (hi - lo)  # a few pA matter near rheobase
                p["params"][kx] = snap(cur + rng.gauss(0, sd), lo, hi, step)
        else:
            for kx, v in list(p["params"].items()):
                if kx != "V_m" and isinstance(v, (int, float)) and v != 0 and rng.random() < 0.5:
                    p["params"][kx] = v * math.exp(rng.gauss(0, 0.06 * t))
    for o in c.get("outputs", []):  # modulator gain / tau are sliders too
        if o.get("kind") == "modulator":
            if "gain" in o:
                o["gain"] *= math.exp(rng.gauss(0, 0.12 * t))
            if "tau_ms" in o:
                o["tau_ms"] *= math.exp(rng.gauss(0, 0.2 * t))


def typical_weight(c, model):
    ws = []
    pops = {p["id"]: p for p in c["populations"]}
    for pr in c["projections"]:
        if pops[pr["post"]]["model"] == model:
            w = pr.get("synapse", {}).get("weight") if "edges" not in pr else (sum(e[2] for e in pr["edges"]) / len(pr["edges"]) if pr["edges"] else None)
            if isinstance(w, (int, float)) and w > 0:
                ws.append(w)
    if ws:
        return sum(ws) / len(ws)
    return {"iaf_psc_alpha": 150.0}.get(model)


def add_extras(c, rng, t):
    """Redundant / idle neurons: (a) isolated cells, (b) cells that listen to something and fire for nothing, (c) a parallel clone of a free
    population (same inputs, same targets, output weights split between original and clone)."""
    pops = {p["id"]: p for p in c["populations"]}
    free = {i for g in analyse(c) for i in g}
    models = [p["model"] for p in c["populations"] if p["model"] in CLONABLE] or ["iaf_psc_alpha"]
    for k in range(rng.choice((1, 1, 2, 3, 4))):
        kind = rng.choice("abbc")
        nid = f"X{len(c['populations'])}"
        if kind == "c":
            src = [i for i in free if pops[i]["model"] in CLONABLE and i in pops and not pops[i].get("_cloned")]
            if not src:
                kind = "a"
            else:
                s = pops[rng.choice(sorted(src))]
                cl = copy.deepcopy(s)
                cl["id"] = nid
                cl["pos"] = [[x + 40, y + 12] for x, y in s["pos"]]
                new_p = []
                for pr in c["projections"]:
                    if pr["pre"] == s["id"] and pr["post"] != s["id"]:  # split the output weight between the original and the clone
                        two = copy.deepcopy(pr); two["pre"] = nid
                        for q in (pr, two):
                            if "edges" in q:
                                for e in q["edges"]: e[2] *= 0.5
                            elif isinstance(q.get("synapse", {}).get("weight"), (int, float)):
                                q["synapse"]["weight"] *= 0.5
                        new_p.append(two)
                    elif pr["post"] == s["id"] and pr["pre"] != s["id"]:
                        two = copy.deepcopy(pr); two["post"] = nid
                        new_p.append(two)
                c["populations"].append(cl)
                c["projections"].extend(new_p)
                for x in c.get("inputs", []):
                    if any(isinstance(tg, str) and tg == s["id"] or isinstance(tg, dict) and tg["pop"] == s["id"] and "idx" not in tg for tg in x["targets"]):
                        x["targets"].append(nid)
                pops[nid] = cl
                cl["_cloned"] = True
                s["_cloned"] = True
                continue
        model = rng.choice(models)
        n = rng.choice((1, 1, 2, 3, 4, 6)) if rng.random() < 0.8 else rng.choice((8, 10, 12))
        p = {"id": nid, "model": model, "n": n, "params": {}, "pos": [[520, 40 + 24 * j] for j in range(n)], "role": "neuron"}
        c["populations"].append(p)
        pops[nid] = p
        if kind == "b":
            w = typical_weight(c, model)
            srcs = [q["id"] for q in c["populations"] if q["id"] != nid]
            if w and srcs:
                c["projections"].append({"pre": rng.choice(srcs), "post": nid, "rule": "all_to_all",
                                         "synapse": {"synapse_model": "static_synapse", "weight": w * rng.uniform(0.3, 1.5), "delay": 2.0}})
    for p in c["populations"]:
        p.pop("_cloned", None)


def variant(base, rng, strength, is_ref):
    c = copy.deepcopy(base)
    if is_ref:
        return c
    t = max(0.05, rng.random() * strength)
    if rng.random() < 0.7:
        resize(c, rng, t, comp=rng.uniform(0.6, 1.05))
    if rng.random() < 0.8:
        scale_weights(c, rng, t)
    if rng.random() < 0.7:
        perturb_params(c, rng, t)
    if rng.random() < 0.45:
        add_extras(c, rng, t)
    c["seed"] = rng.randrange(1, 900)
    return c


# ---------------------------------------------------------------- scoring
def run(binp, task, circ_path, seed, timeout=900):
    r = subprocess.run([binp, "--task", task, "--circuit", circ_path, "--seeds", str(seed), "--require", "any"], capture_output=True, text=True, timeout=timeout)
    if r.returncode not in (0, 3) or not r.stdout.strip():
        return None
    return json.loads(r.stdout)["runs"][0]


def evaluate(args):
    binp, task, circ, tmpdir, idx = args
    path = os.path.join(tmpdir, f"{task}_{idx}.json")
    json.dump(circ, open(path, "w"))
    try:
        runs = []
        for s in SEEDS:  # seed 1 first: a failure there saves the second simulation
            r = run(binp, task, path, s)
            if r is None:
                return {"idx": idx, "status": "invalid"}
            runs.append(r)
            if not r["passed"]:
                return {"idx": idx, "status": "fail", "value": r["value"]}
        rec = {"idx": idx, "status": "pass", "neurons": runs[0]["neurons"], "energy_hz": sum(r["rate_hz_per_neuron"] for r in runs) / len(runs),
               "score": sum(r["value"] for r in runs) / len(runs), "metrics": {}}
        for k, m in runs[0]["metrics"].items():
            vals = [r["metrics"][k]["value"] for r in runs if k in r["metrics"] and r["metrics"][k]["value"] is not None and math.isfinite(r["metrics"][k]["value"])]
            if len(vals) == len(runs):
                rec["metrics"][k] = {"unit": m["unit"], "lower_is_better": m["lower_is_better"], "value": sum(vals) / len(vals)}
        return rec
    finally:
        try: os.remove(path)
        except OSError: pass


def build_level(binp, task, ref_path, pool, tmpdir, a, log):
    base = json.load(open(ref_path))
    t0 = time.time()
    strength, idx, samples = 1.0, 0, []
    n_fail = n_invalid = 0
    ref_pass = None
    while True:
        batch = [(binp, task, variant(base, random.Random(f"{a.seed}:{task}:{i}"), strength, i == 0), tmpdir, i) for i in range(idx, idx + BATCH)]
        res = list(pool.map(evaluate, batch))  # ordered: the adaptation below is deterministic
        idx += BATCH
        npass = 0
        for r in res:
            if r["status"] == "pass":
                samples.append(r); npass += 1
            elif r["status"] == "fail":
                n_fail += 1
            else:
                n_invalid += 1
        if ref_pass is None:
            ref_pass = res[0]["status"] == "pass"
        rate = npass / BATCH
        if rate < 0.25:
            strength = max(0.25, strength * 0.8)
        elif rate > 0.7:
            strength = min(2.0, strength * 1.15)
        el = time.time() - t0
        log(f"  {task}: {len(samples)} pass / {idx} tried, strength {strength:.2f}, {el:.0f}s")
        if len(samples) >= a.target:
            break
        if el > a.budget and len(samples) >= a.min:
            break
        if el > 3 * a.budget or idx >= a.max_tries:
            break
    return {"samples": samples, "tried": idx, "fail": n_fail, "invalid": n_invalid, "ref_pass": ref_pass, "secs": time.time() - t0, "strength": strength}


def write(task, lv):
    S = lv["samples"][:]
    S.sort(key=lambda r: r["idx"])
    metrics = {
        "neurons": {"unit": "neurons", "lower_is_better": True, "values": [r["neurons"] for r in S]},
        "energy_hz": {"unit": "spikes/s", "lower_is_better": True, "values": [round(r["energy_hz"], 3) for r in S]},
    }
    keys = []
    for r in S:
        for k in r["metrics"]:
            if k not in keys:
                keys.append(k)
    for k in keys:
        ms = [r["metrics"][k] for r in S if k in r["metrics"]]
        metrics[k] = {"unit": ms[0]["unit"], "lower_is_better": ms[0]["lower_is_better"], "values": [round(m["value"], 4) for m in ms]}
    out = {"task": task, "source": "synthetic: tools/histograms/gen_histograms.py (see docs/HISTOGRAMS.md)", "samples": len(S), "metrics": metrics}
    os.makedirs(OUT_DIR, exist_ok=True)
    with open(os.path.join(OUT_DIR, task + ".json"), "w") as f:
        json.dump(out, f, indent=1)
        f.write("\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tasks", default="")
    ap.add_argument("--budget", type=float, default=150, help="wall seconds per level before stopping (if --min is met)")
    ap.add_argument("--target", type=int, default=150)
    ap.add_argument("--min", type=int, default=40)
    ap.add_argument("--max-tries", dest="max_tries", type=int, default=1500)
    ap.add_argument("--workers", type=int, default=4)
    ap.add_argument("--seed", type=int, default=20260101)
    ap.add_argument("--bin", default="")
    ap.add_argument("--probe", action="store_true", help="only time the reference circuits")
    a = ap.parse_args()
    a.workers = min(4, a.workers)
    binp = find_bin(a.bin)
    lv = levels()
    todo = [t for t in (a.tasks.split(",") if a.tasks else lv) if t]
    log = lambda s: print(s, file=sys.stderr, flush=True)
    tmpdir = tempfile.mkdtemp(prefix="hist_")
    summary = {}
    T0 = time.time()
    if a.probe:
        for t in todo:
            s = time.time(); r = evaluate((binp, t, json.load(open(lv[t])), tmpdir, 0)); print(t, r["status"], f"{time.time()-s:.1f}s", flush=True)
        return
    with ThreadPoolExecutor(a.workers) as pool:
        for t in todo:
            res = build_level(binp, t, lv[t], pool, tmpdir, a, log)
            if res["samples"]:
                write(t, res)
            summary[t] = {k: v for k, v in res.items() if k != "samples"} | {"passed": len(res["samples"])}
            log(f"{t}: {len(res['samples'])} samples ({res['tried']} tried, ref_pass={res['ref_pass']}, {res['secs']:.0f}s)")
    print(json.dumps({"total_secs": time.time() - T0, "levels": summary}, indent=1))


if __name__ == "__main__":
    main()
