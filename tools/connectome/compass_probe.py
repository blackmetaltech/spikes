#!/usr/bin/env python3
"""Runs the fly_compass task on circuits made by compass_circuit.py. Probe harness for research/atlas-a.md; no level is changed.

  python3 tools/connectome/compass_probe.py one [key=value ...]            one parameter set on seeds 101..105, quick episode, prints the task's measures
  python3 tools/connectome/compass_probe.py search N [seed]                random search over the per-type scales (N draws), prints the best by the task's score
"""
import json, os, random, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import compass_circuit as cc
BIN = os.path.join(ROOT, "build", "engine", "spike-run")
KEYS = ["error_cal_deg", "strength_cal", "error_hold_deg", "strength_hold", "error_k1_deg", "error_k2_deg", "bump_k1_deg", "bump_k2_deg", "wrong_way", "too_short", "too_long", "criteria_met", "heading_hold_hz"]


def run(params, seeds=(101, 102, 103, 104, 105), quick=True, tag="p", mode=None):
    c = cc.build(params)
    path = os.path.join(tempfile.gettempdir(), "compass_probe_%s_%d.json" % (tag, os.getpid()))
    json.dump(c, open(path, "w"))
    cmd = [BIN, "--task", "fly_compass", "--circuit", path, "--seeds", ",".join(map(str, seeds)), "--jobs", "5"] + (["--quick"] if quick else []) + (["--mode", mode] if mode else [])
    p = subprocess.run(cmd, capture_output=True, text=True)
    os.remove(path)
    if not p.stdout.strip():
        return None, p.stderr[-300:]
    r = json.loads(p.stdout.strip().splitlines()[-1])
    return r, None


def show(r):
    for x in r["runs"]:
        pa = x["parts"]
        print("  seed %d passed=%s value=%.0f rate %.1f Hz | " % (x["seed"], x["passed"], x["value"], x["rate_hz_per_neuron"]) + " ".join("%s=%.3g" % (k.replace("error_", "e_").replace("_deg", ""), pa[k]) for k in KEYS if k in pa))


if __name__ == "__main__":
    w = sys.argv[1]
    if w == "one":
        p = {}
        for kv in sys.argv[2:]:
            k, v = kv.split("="); p[k] = type(cc.DEFAULTS[k])(v)
        r, err = run(p)
        if r is None: print("ERR", err)
        else: print(r["passes"], "of", r["n"], "passed;", r["runs"][0]["verdict"][:90]); show(r)
    if w == "search":
        n = int(sys.argv[2]); rng = random.Random(int(sys.argv[3]) if len(sys.argv) > 3 else 1)
        out = open(os.path.join(os.environ.get("TMPDIR", "/tmp"), "compass_search_%s.jsonl" % (sys.argv[3] if len(sys.argv) > 3 else 1)), "w")
        for i in range(n):
            p = {"scale_" + t: round(10 ** rng.uniform(-2, 0), 4) for t in ("EPG", "PENa", "PENb", "PEG", "D7")}
            p["scale_PENb"] = p["scale_PENa"] if rng.random() < 0.5 else p["scale_PENb"]
            p.update(I_e=round(rng.uniform(150, 360), 1), tau_syn_ex=rng.choice([2.0, 5.0, 15.0]), tau_syn_in=rng.choice([2.0, 5.0, 15.0]), turn_w=rng.choice([0.5, 1.0, 3.0]))
            r, err = run(p, tag="s")
            if r is None: continue
            xs = [x["parts"] for x in r["runs"]]
            hold_ok = sum(1 for a in xs if a["strength_hold"] > 0.4 and a["error_hold_deg"] < 30 and a["strength_cal"] > 0.4)
            rec = {"params": p, "hold_ok": hold_ok, "strength_hold": [round(a["strength_hold"], 3) for a in xs], "strength_cal": [round(a["strength_cal"], 3) for a in xs],
                   "error_hold": [round(a["error_hold_deg"], 1) for a in xs], "rate": [round(x["rate_hz_per_neuron"], 1) for x in r["runs"]]}
            out.write(json.dumps(rec) + "\n"); out.flush()
