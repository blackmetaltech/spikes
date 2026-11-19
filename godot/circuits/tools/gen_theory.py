#!/usr/bin/env python3
"""Generates the reference circuits of the Theory wing (dynamics, rates, memory, tuning chapters): circuits/<id>.json.
Run from anywhere:  python3 circuits/tools/gen_theory.py [name ...]      Docs: docs/circuits/theory.md, docs/DESIGN-THEORY.md"""
import json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, ".."))


def pop(id_, model="iaf_psc_alpha", n=1, params=None, pos=None):
    p = {"id": id_, "model": model, "n": n, "params": params or {}, "role": "neuron"}
    p["pos"] = [[pos[0] + 28 * (k % 8), pos[1] + 28 * (k // 8)] for k in range(n)] if pos else [[100 + 28 * (k % 8), 100 + 28 * (k // 8)] for k in range(n)]
    return p


def proj(pre, post, w, delay=2.0, rule="all_to_all", syn="static_synapse", **extra):
    s = {"synapse_model": syn, "weight": w}
    if delay is not None:
        s["delay"] = delay
    s.update({k: v for k, v in extra.items() if k in ("tau_plus", "tau_minus", "lambda", "alpha", "mu_plus", "mu_minus", "Wmax", "K")})
    p = {"pre": pre, "post": post, "rule": rule, "synapse": s}
    p.update({k: v for k, v in extra.items() if k not in s and k not in ("tau_plus", "tau_minus", "lambda", "alpha", "mu_plus", "mu_minus", "Wmax", "K")})
    return p


def inp(name, kind, weight, targets, value=0.0, **extra):
    d = {"name": name, "kind": kind, "targets": [{"pop": t} if isinstance(t, str) else t for t in targets], "weight": weight, "value": value}
    d.update(extra)
    return d


def out(name, pops, kind="rate", **extra):
    o = {"name": name, "kind": kind, "neurons": [{"pop": p} if isinstance(p, str) else p for p in pops]}
    o.update(extra)
    return o


def circuit(name, task, pops, projs, inputs, outputs, seed=7, modulation=None, notes=None, v_m=True):
    c = {"schema_version": 1, "name": name, "resolution_ms": 0.1, "seed": seed, "populations": pops, "projections": projs, "inputs": inputs, "outputs": outputs}
    if modulation:
        c["modulation"] = modulation
    c["record"] = {"spikes": "all"}
    c["meta"] = {"author": "spike", "task": task, "notes": notes or "docs/circuits/theory.md"}
    return c


def write(name, c):
    with open(os.path.join(OUT, name + ".json"), "w") as f:
        json.dump(c, f, indent=1)
        f.write("\n")
    print("wrote circuits/%s.json" % name)


IZH_RES = {"a": 0.1, "b": 0.26, "c": -65.0, "d": 2.0}


def d_class():
    write("d_class", circuit("Where firing starts", "d_class",
          [pop("N", "izhikevich", 1, dict(IZH_RES), (220, 140))], [],
          [inp("current", "dc", 1.0, ["N"])], [out("spikes", ["N"])]))


def d_phasic():
    write("d_phasic", circuit("Only the change counts", "d_phasic",
          [pop("N", "izhikevich", 1, {"a": 0.02, "b": 0.25, "c": -65.0, "d": 6.0}, (220, 140))], [],
          [inp("current", "dc", 0.5, ["N"])], [out("spikes", ["N"])]))


def d_switch():
    write("d_switch", circuit("A neuron that remembers a pulse", "d_switch",
          [pop("N", "izhikevich", 1, {"a": 0.02, "b": 0.2, "c": -50.0, "d": 0.0}, (220, 140))], [],
          [inp("hold", "dc", 2.5, ["N"]), inp("set", "dc", 10.0, ["N"]), inp("reset", "dc", 10.0, ["N"])], [out("spikes", ["N"])]))


BUILD = {"d_class": d_class, "d_phasic": d_phasic, "d_switch": d_switch}

if __name__ == "__main__":
    for k in (sys.argv[1:] or list(BUILD)):
        BUILD[k]()
