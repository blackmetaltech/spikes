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


RATE = {"tau": 10.0, "sigma": 0.0, "g": 1.0, "theta": 0.0, "alpha": 1.0}


def rproj(pre, post, w, rule="all_to_all", **extra):
    p = {"pre": pre, "post": post, "rule": rule, "synapse": {"synapse_model": "rate_connection_instantaneous", "weight": w}}
    if pre == post:
        p["allow_autapses"] = True
    p.update(extra)
    return p


def r_oscillate():
    e = dict(RATE); i = dict(RATE); i["tau"] = 20.0
    write("r_oscillate", circuit("Two populations that make a rhythm", "r_oscillate",
          [pop("E", "threshold_lin_rate_ipn", 1, e, (220, 120)), pop("I", "threshold_lin_rate_ipn", 1, i, (420, 120))],
          [rproj("E", "E", 5.0), rproj("E", "I", 0.8), rproj("I", "E", -6.0)],
          [inp("drive", "rate_drive", 1.0, ["E"], 0.2)],
          [out("e", ["E"], "rate_value"), out("i", ["I"], "rate_value")]))


def r_choose():
    write("r_choose", circuit("Two populations, one winner", "r_choose",
          [pop("A", "threshold_lin_rate_ipn", 1, dict(RATE), (220, 120)), pop("B", "threshold_lin_rate_ipn", 1, dict(RATE), (420, 120))],
          [rproj("A", "A", 1.25), rproj("B", "B", 1.25), rproj("A", "B", -1.0), rproj("B", "A", -1.0)],
          [inp("a_in", "rate_drive", 1.0, ["A"], 0.5), inp("b_in", "rate_drive", 1.0, ["B"], 0.5)],
          [out("a", ["A"], "rate_value"), out("b", ["B"], "rate_value")]))


def r_meanfield():
    neu = {"C_m": 1.0, "tau_m": 20.0, "t_ref": 2.0, "E_L": 0.0, "V_reset": 0.0, "V_th": 20.0}
    sg = {"tau_m": 20.0, "t_ref": 2.0, "theta": 20.0, "V_reset": 0.0, "tau": 5.0}
    J, JI, Jx, CE, CI = 0.3, -1.5, 0.2, 60, 20
    fi = lambda a, b, k, w: {"pre": a, "post": b, "rule": "fixed_indegree", "indegree": k, "synapse": {"synapse_model": "static_synapse", "weight": w, "delay": 1.5}}
    dc = lambda a, b, w, K: {"pre": a, "post": b, "rule": "all_to_all", "synapse": {"synapse_model": "diffusion_connection", "weight": w, "K": K}}
    write("r_meanfield", circuit("The network and its mean field", "r_meanfield",
          [pop("E", "iaf_psc_delta", 200, dict(neu), (60, 80)), pop("I", "iaf_psc_delta", 50, dict(neu), (60, 300)),
           pop("SE", "siegert_neuron", 1, dict(sg), (520, 80)), pop("SI", "siegert_neuron", 1, dict(sg), (520, 300)), pop("SD", "siegert_neuron", 1, {"mean": 0.0}, (420, 190))],
          [fi("E", "E", CE, J), fi("E", "I", CE, J), fi("I", "E", CI, JI), fi("I", "I", CI, JI),
           dc("SD", "SE", Jx, 1), dc("SD", "SI", Jx, 1), dc("SE", "SE", J, CE), dc("SE", "SI", J, CE), dc("SI", "SE", JI, CI), dc("SI", "SI", JI, CI)],
          [inp("ext", "poisson", Jx, ["E", "I"], 8000.0), inp("ext_mf", "rate_drive", 1.0, ["SD"], 8000.0)],
          [out("net", ["E"]), out("mf", ["SE"], "rate_value")]))


def h_complete():
    w = 400.0
    write("h_complete", circuit("Recalling a whole from a part", "h_complete",
          [pop("C", "iaf_psc_alpha", 3, {}, (180, 120)), pop("R", "iaf_psc_alpha", 5, {}, (180, 260))],
          [proj("C", "C", w), proj("C", "R", w), proj("R", "C", w), proj("R", "R", w)],
          [inp("cue", "poisson", 1500.0, ["C"], 0.0)], [out("memory", ["C", "R"])]))


def h_two():
    write("h_two", circuit("Two memories, no blends", "h_two",
          [pop("A", "iaf_psc_alpha", 4, {}, (160, 120)), pop("B", "iaf_psc_alpha", 4, {}, (160, 280))],
          [proj("A", "A", 375.0), proj("B", "B", 375.0), proj("A", "B", -500.0), proj("B", "A", -500.0)],
          [inp("cue_a", "poisson", 1500.0, ["A"], 0.0), inp("cue_b", "poisson", 1500.0, ["B"], 0.0)], [out("a", ["A"]), out("b", ["B"])]))


def h_learn():
    st = {"synapse_model": "stdp_synapse", "lambda": 0.0015, "alpha": 0.5, "Wmax": 800.0}
    def pj(a, b):
        p = proj(a, b, 80.0, 2.0, syn="stdp_synapse")
        p["synapse"].update({"lambda": 0.0015, "alpha": 0.5, "Wmax": 800.0, "tau_plus": 20.0})
        return p
    write("h_learn", circuit("Writing a memory with plasticity", "h_learn",
          [pop("C", "iaf_psc_alpha", 3, {}, (180, 120)), pop("R", "iaf_psc_alpha", 5, {}, (180, 260))],
          [pj("C", "C"), pj("C", "R"), pj("R", "C"), pj("R", "R")],
          [inp("cue", "poisson", 1500.0, ["C"], 0.0), inp("show", "poisson", 1500.0, ["C", "R"], 0.0)], [out("memory", ["C", "R"])]))


BUILD = {"d_class": d_class, "d_phasic": d_phasic, "d_switch": d_switch, "r_oscillate": r_oscillate, "r_choose": r_choose, "r_meanfield": r_meanfield, "h_complete": h_complete, "h_two": h_two, "h_learn": h_learn}

if __name__ == "__main__":
    for k in (sys.argv[1:] or list(BUILD)):
        BUILD[k]()
