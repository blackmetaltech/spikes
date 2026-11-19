#!/usr/bin/env python3
"""Generates the reference circuits of the Bestiary chapters A (worm micro-puzzles, leech, aplysia) and the diagnosis patients circuits/dx_*.json of those chapters.
The worm task's reference (worm_touch.json) comes from tools/connectome/worm.py (real synapse counts); this script reads it to make dx_worm.json.
Run from anywhere:  python3 circuits/tools/gen_bestiary_a.py [name ...]      Docs: docs/circuits/bestiary_a.md"""
import json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, ".."))


def pop(id_, model="iaf_psc_alpha", n=1, params=None, pos=None):
    p = {"id": id_, "model": model, "n": n, "params": params or {}, "role": "neuron"}
    p["pos"] = [[pos[0] + 28 * k, pos[1]] for k in range(n)] if pos else [[100 + 28 * k, 100] for k in range(n)]
    return p


def proj(pre, post, w, delay=2.0, rule="all_to_all", syn="static_synapse", **extra):
    s = {"synapse_model": syn, "weight": w, "delay": delay}
    s.update(extra)
    return {"pre": pre, "post": post, "rule": rule, "synapse": s}


def inp(name, kind, weight, targets, value=0.0):
    return {"name": name, "kind": kind, "targets": [{"pop": t} for t in targets], "weight": weight, "value": value}


def out(name, pops, kind="rate", **extra):
    o = {"name": name, "kind": kind, "neurons": [{"pop": p} for p in pops]}
    o.update(extra)
    return o


def circuit(name, task, pops, projs, inputs, outputs, seed=7, modulation=None, notes=None):
    c = {"schema_version": 1, "name": name, "resolution_ms": 0.1, "seed": seed, "populations": pops, "projections": projs, "inputs": inputs, "outputs": outputs}
    if modulation:
        c["modulation"] = modulation
    c["record"] = {"spikes": "all"}
    c["meta"] = {"author": "spike", "task": task, "notes": notes or "docs/circuits/bestiary_a.md"}
    return c


def write(name, c):
    with open(os.path.join(OUT, name + ".json"), "w") as f:
        json.dump(c, f, indent=1)
        f.write("\n")
    print("wrote circuits/%s.json" % name)


def lesion(c, lid):
    """Weight 0 on every projection named by `lid` ('pre>post', several joined with '+'); the projection stays in the file (see gen_boss_dx.py)."""
    hit = [tuple(x.split(">")) for x in lid.split("+")]
    n = 0
    for p in c["projections"]:
        if (p["pre"], p["post"]) not in hit:
            continue
        n += 1
        if "edges" in p:
            for e in p["edges"]:
                e[2] = 0.0
        else:
            p["synapse"]["weight"] = 0.0
    assert n >= len(hit), "lesion %s matched too few projections" % lid
    return c


def w_latch():
    pops = [pop("A", pos=(220, 120)), pop("B", pos=(420, 120)), pop("S", pos=(320, 260))]
    projs = [proj("A", "B", 1500.0, 4.0), proj("B", "A", 1500.0, 4.0), proj("S", "A", -1500.0), proj("S", "B", -1500.0)]
    ins = [inp("touch", "poisson", 1000.0, ["A"]), inp("stop", "poisson", 1000.0, ["S"])]
    outs = [out("reverse", ["B"])]
    write("w_latch", circuit("Worm: holding a reversal", "w_latch", pops, projs, ins, outs))


def w_switch():
    pops = [pop("R", pos=(220, 140)), pop("F", pos=(420, 140))]
    projs = [proj("R", "F", -4000.0), proj("F", "R", -4000.0)]
    ins = [inp("push_rev", "poisson", 1000.0, ["R"]), inp("push_fwd", "poisson", 1000.0, ["F"])]
    outs = [out("reverse", ["R"]), out("forward", ["F"])]
    write("w_switch", circuit("Worm: forward or backward", "w_switch", pops, projs, ins, outs))


def izh(a, b, c, d):
    return {"a": a, "b": b, "c": c, "d": d, "V_m": -65.0, "U_m": b * -65.0}


def l_burst():
    pops = [pop("HN", "izhikevich", 1, izh(0.02, 0.2, -50.0, 2.0), pos=(320, 140))]
    ins = [inp("drive", "dc", 5.0, ["HN"])]
    outs = [out("burst", ["HN"])]
    write("l_burst", circuit("Leech: a cell that bursts", "l_burst", pops, [], ins, outs))


def l_rebound():
    pops = [pop("S", pos=(220, 140)), pop("T", "aeif_cond_alpha", 1, {"a": 20.0, "b": 0.0, "tau_w": 200.0, "I_e": 580.0}, pos=(420, 140))]
    projs = [proj("S", "T", -400.0)]
    ins = [inp("inhibit", "poisson", 1000.0, ["S"])]
    outs = [out("rebound", ["T"])]
    write("l_rebound", circuit("Leech: rebound after inhibition", "l_rebound", pops, projs, ins, outs))


def leech_heart(hn_params=None, w_inh=-50.0, w_he=-300.0, he_params=None):
    """The heartbeat timing network of the reference: two heart interneurons (HN) that inhibit each other, and the two heart motor neurons (HE) that each one inhibits."""
    hp = hn_params or {"a": 0.0, "b": 5.0, "tau_w": 1500.0, "V_reset": -58.0}
    ep = he_params or {"a": 0.0, "b": 0.0}
    pops = [pop("HN_L", "aeif_cond_alpha", 1, hp, pos=(220, 90)), pop("HN_R", "aeif_cond_alpha", 1, hp, pos=(420, 90)),
            pop("HE_L", "aeif_cond_alpha", 1, ep, pos=(220, 230)), pop("HE_R", "aeif_cond_alpha", 1, ep, pos=(420, 230))]
    projs = [proj("HN_L", "HN_R", w_inh), proj("HN_R", "HN_L", w_inh), proj("HN_L", "HE_L", w_he), proj("HN_R", "HE_R", w_he)]
    ins = [inp("drive", "dc", 480.0, ["HN_L", "HN_R", "HE_L", "HE_R"], 2.0)]
    outs = [out("left", ["HE_L"]), out("right", ["HE_R"])]
    return circuit("Leech: the heartbeat", "leech_heart", pops, projs, ins, outs)


def write_leech_heart():
    write("leech_heart", leech_heart())


def stp(U=0.5, tau_rec=1500.0):
    """tsodyks2 depression: u = U so that the first spike of a connection already has the efficacy of every later one (docs/CIRCUIT-FORMAT.md)."""
    return {"U": U, "u": U, "x": 1.0, "tau_rec": tau_rec, "tau_fac": 0.0}


def a_depress(w=8000.0, tau_rec=1500.0, U=0.2):
    pops = [pop("SN", pos=(220, 140)), pop("MN", pos=(420, 140))]
    projs = [proj("SN", "MN", w, syn="tsodyks2_synapse", **stp(U, tau_rec))]
    ins = [inp("touch", "poisson", 1000.0, ["SN"])]
    outs = [out("gill", ["MN"])]
    return circuit("Aplysia: getting used to a touch", "a_depress", pops, projs, ins, outs)


def write_a_depress():
    write("a_depress", a_depress())


def a_facilitate(w=450.0, k=4.0):
    pops = [pop("SN", pos=(120, 80)), pop("MN", pos=(320, 80)), pop("TSN", pos=(120, 240)), pop("FN", pos=(320, 240))]
    projs = [proj("SN", "MN", w), proj("TSN", "FN", 1000.0)]
    ins = [inp("touch", "poisson", 1000.0, ["SN"]), inp("shock", "poisson", 1000.0, ["TSN"])]
    outs = [out("gill", ["MN"]), out("serotonin", ["FN"], kind="modulator", modulator="NE", gain=0.05, tau_ms=1000.0)]
    mod = [{"modulator": "NE", "projection": 0, "param": "weight", "scale_per_unit": k}]
    return circuit("Aplysia: a shock to the tail", "a_facilitate", pops, projs, ins, outs, modulation=mod)


def write_a_facilitate():
    write("a_facilitate", a_facilitate())


def aplysia_gill(w=2500.0, tau_rec=1000.0, U=0.1, gain=1.5, w_tail=1500.0, n_sn=6):
    """The reflex arc: touch > SN > (depressing synapse) > MN; shock > TSN > FN (releases the modulator) and TSN > MN (the shock also moves the gill)."""
    pops = [pop("SN", n=n_sn, pos=(120, 80)), pop("MN", pos=(340, 80)), pop("TSN", pos=(120, 250)), pop("FN", pos=(340, 250))]
    projs = [proj("SN", "MN", w, syn="tsodyks2_synapse", **stp(U, tau_rec)), proj("TSN", "MN", w_tail), proj("TSN", "FN", 1000.0)]
    ins = [inp("touch", "poisson", 1000.0, ["SN"]), inp("shock", "poisson", 1000.0, ["TSN"])]
    outs = [out("gill", ["MN"]), out("serotonin", ["FN"], kind="modulator", modulator="NE", gain=0.05, tau_ms=1000.0)]
    mod = [{"modulator": "NE", "projection": 0, "param": "U", "gain": gain, "min": 0.0, "max": 1.0}]
    return circuit("Aplysia: the gill-withdrawal reflex", "aplysia_gill", pops, projs, ins, outs, modulation=mod)


def write_aplysia_gill():
    write("aplysia_gill", aplysia_gill())


DX = {
    "dx_worm": ("worm_touch.json", "AVA>MA"),
    "dx_leech": ("leech_heart.json", "HN_L>HN_R"),
    "dx_aplysia": ("aplysia_gill.json", "TSN>FN"),
}


def write_dx(name):
    ref, lid = DX[name]
    c = json.load(open(os.path.join(OUT, ref)))
    c = lesion(c, lid)
    c["name"] = "Patient circuit: " + name[3:]
    c["meta"] = {"author": "spike", "task": name, "notes": "docs/circuits/bestiary_a.md"}
    with open(os.path.join(OUT, name + ".json"), "w") as f:
        json.dump(c, f, indent=1)
        f.write("\n")
    print("wrote circuits/%s.json from %s (lesion %s)" % (name, ref, lid))


ALL = {"w_latch": w_latch, "w_switch": w_switch, "l_burst": l_burst, "l_rebound": l_rebound, "leech_heart": write_leech_heart, "a_depress": write_a_depress, "a_facilitate": write_a_facilitate, "aplysia_gill": write_aplysia_gill, "dx_worm": lambda: write_dx("dx_worm"), "dx_leech": lambda: write_dx("dx_leech"), "dx_aplysia": lambda: write_dx("dx_aplysia")}

if __name__ == "__main__":
    for k in (sys.argv[1:] or list(ALL)):
        ALL[k]()
