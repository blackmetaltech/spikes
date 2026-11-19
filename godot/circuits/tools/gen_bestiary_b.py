#!/usr/bin/env python3
"""Generates the reference circuits of the Bestiary chapters B (fly compass, fly mushroom body, crab pyloric rhythm) and the diagnosis patients circuits/dx_*.json of those chapters.
Run from anywhere:  python3 circuits/tools/gen_bestiary_b.py [name ...]      Docs: docs/circuits/bestiary_b.md
The compass wiring is a ring-attractor model after the Turner-Evans / Hulse cell types, not synapse-level data."""
import json, math, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_bestiary_a import OUT, circuit, inp, lesion, out, pop, proj, write  # noqa: E402

N = 16  # ring cells = wedges (22.5 degrees each)


def ring_pos(r, cx=320.0, cy=200.0, n=N, phase=-math.pi / 2):
    return [[round(cx + r * math.cos(phase + 2 * math.pi * k / n)), round(cy + r * math.sin(phase + 2 * math.pi * k / n))] for k in range(n)]


def popp(id_, model, n, params, pos):
    return {"id": id_, "model": model, "n": n, "params": params, "role": "neuron", "pos": pos}


def edges(n, off, w, d=2.0):
    return [[k, (k + off) % n, w, d] for k in range(n)]


def eproj(pre, post, E):
    return {"pre": pre, "post": post, "edges": E, "synapse": {"synapse_model": "static_synapse"}}


def ring_ref(w1=100.0, Ie=320.0, tse=15.0, nD=4, wd=150.0, wi=-400.0, wc=400.0, shifters=True, wep=300.0, wt=3.0, wpe=110.0, name="Fly: the compass in the dark", task="fly_compass"):
    pops = [popp("EPG", "iaf_psc_alpha", N, {"I_e": Ie, "tau_syn_ex": tse}, ring_pos(80)),
            popp("DELTA", "iaf_psc_alpha", nD, {}, [[320 + 14 * k - 7 * (nD - 1), 200] for k in range(nD)])]
    projs = [eproj("EPG", "EPG", edges(N, -1, w1) + edges(N, 1, w1)), proj("EPG", "DELTA", wd), proj("DELTA", "EPG", wi)]
    ins = [{"name": "cue%d" % k, "kind": "poisson", "targets": [{"pop": "EPG", "idx": [k]}], "weight": wc, "value": 0.0} for k in range(N)]
    outs = [out("heading", ["EPG"], kind="population_vector")]
    outs[0]["neurons"] = [{"pop": "EPG", "idx": list(range(N))}]
    if shifters:
        pops += [popp("PENL", "iaf_psc_alpha", N, {}, ring_pos(130, phase=-math.pi / 2 - 0.06)), popp("PENR", "iaf_psc_alpha", N, {}, ring_pos(160, phase=-math.pi / 2 + 0.06))]
        projs += [proj("EPG", "PENL", wep, rule="one_to_one"), proj("EPG", "PENR", wep, rule="one_to_one"), eproj("PENL", "EPG", edges(N, -1, wpe)), eproj("PENR", "EPG", edges(N, 1, wpe))]
        ins += [inp("turn_left", "dc", wt, ["PENL"], 40.0), inp("turn_right", "dc", wt, ["PENR"], 40.0)]
    return circuit(name, task, pops, projs, ins, outs, notes="docs/circuits/bestiary_b.md")


def c_ring():
    write("c_ring", ring_ref(shifters=False, name="Fly: a bump that stays", task="c_ring"))


def c_shift():
    write("c_shift", ring_ref(name="Fly: moving the bump", task="c_shift"))


def fly_compass():
    write("fly_compass", ring_ref())


DX = {"dx_compass": ("fly_compass.json", "PENL>EPG")}


def write_dx(name):
    ref, lid = DX[name]
    c = json.load(open(os.path.join(OUT, ref)))
    c = lesion(c, lid)
    c["name"] = "Patient circuit: " + name[3:]
    c["meta"] = {"author": "spike", "task": name, "notes": "docs/circuits/bestiary_b.md"}
    with open(os.path.join(OUT, name + ".json"), "w") as f:
        json.dump(c, f, indent=1)
        f.write("\n")
    print("wrote circuits/%s.json from %s (lesion %s)" % (name, ref, lid))


ALL = {"c_ring": c_ring, "c_shift": c_shift, "fly_compass": fly_compass, "dx_compass": lambda: write_dx("dx_compass")}

# ---------------------------------------------------------------- the mushroom body (research/bestiary-a.md section 5)
def grid(n, cols, x0, y0, dx=14, dy=14):
    return [[x0 + dx * (k % cols), y0 + dy * (k // cols)] for k in range(n)]


def glomeruli_inputs(rate=0.0):
    return [{"name": "pn%d" % k, "kind": "poisson", "targets": [{"pop": "PN", "idx": [k]}], "weight": 1000.0, "value": 0.0} for k in range(20)]


def stdp_dopa(w, wmax, a, src, tau_c=30.0, tau_n=200.0):
    """KC > MBON: NEST stdp_dopamine_synapse used as a depression (A_plus < 0, A_minus > 0: NEST subtracts A_minus for anti-causal pairs, so both make c negative)."""
    return {"synapse_model": "stdp_dopamine_synapse", "weight": w, "delay": 2.0, "A_plus": -a, "A_minus": a, "Wmin": 0.0, "Wmax": wmax, "tau_c": tau_c, "tau_n": tau_n, "b": 0.0, "tau_plus": 20.0}


def k_sparse(apl=True):
    pops = [popp("PN", "iaf_psc_alpha", 20, {}, grid(20, 20, 40, 40, 22, 0)), popp("KC", "iaf_psc_alpha", 200, {}, grid(200, 20, 40, 120)), popp("APL", "iaf_psc_alpha", 4, {}, [[160 + 24 * k, 300] for k in range(4)])]
    projs = [{"pre": "PN", "post": "KC", "rule": "fixed_indegree", "indegree": 4, "synapse": {"synapse_model": "static_synapse", "weight": 230.0, "delay": 2.0}}, proj("KC", "APL", 5000.0), proj("APL", "KC", -5000.0)]
    ins = glomeruli_inputs()
    outs = [out("kc", ["KC"])]
    c = circuit("Fly: a sparse odour code", "k_sparse", pops, projs, ins, outs, notes="docs/circuits/bestiary_b.md")
    return c


def mb_core(task, name):
    pops = [popp("PN", "iaf_psc_alpha", 20, {}, grid(20, 20, 40, 40, 22, 0)), popp("KC", "iaf_psc_alpha", 200, {}, grid(200, 20, 40, 120)),
            popp("APL", "iaf_psc_alpha", 4, {}, [[500 + 24 * k, 140] for k in range(4)]), popp("MBON", "iaf_psc_alpha", 4, {"I_e": 350.0}, grid(4, 4, 400, 340, 22, 22)),
            popp("TSN", "iaf_psc_alpha", 1, {}, [[60, 400]]), popp("DAN", "iaf_psc_alpha", 2, {}, [[160, 400], [188, 400]])]
    projs = [{"pre": "PN", "post": "KC", "rule": "fixed_indegree", "indegree": 4, "synapse": {"synapse_model": "static_synapse", "weight": 230.0, "delay": 2.0}},
             proj("KC", "APL", 2500.0), proj("APL", "KC", -5000.0),
             {"pre": "KC", "post": "MBON", "rule": "all_to_all", "dopamine_source": "DAN", "synapse": stdp_dopa(1500.0, 4500.0, 1000.0, "DAN")},
             proj("TSN", "DAN", 1000.0)]
    ins = glomeruli_inputs() + [inp("punish", "poisson", 1000.0, ["TSN"])]
    outs = [out("avoid", ["MBON"]), out("kc", ["KC"])]
    return circuit(name, task, pops, projs, ins, outs, notes="docs/circuits/bestiary_b.md")


def k_depress():
    pops = [popp("KCA", "iaf_psc_alpha", 6, {}, [[100 + 28 * k, 80] for k in range(6)]), popp("KCB", "iaf_psc_alpha", 6, {}, [[100 + 28 * k, 220] for k in range(6)]),
            popp("MBON", "iaf_psc_alpha", 4, {"I_e": 350.0}, [[400 + 28 * k, 150] for k in range(4)]), popp("DAN", "iaf_psc_alpha", 2, {}, [[400 + 28 * k, 300] for k in range(2)])]
    projs = [{"pre": k, "post": "MBON", "rule": "all_to_all", "dopamine_source": "DAN", "synapse": stdp_dopa(300.0, 900.0, 0.6, "DAN")} for k in ("KCA", "KCB")]
    ins = [inp("odor_a", "poisson", 1000.0, ["KCA"], 100.0), inp("odor_b", "poisson", 1000.0, ["KCB"], 100.0), inp("punish", "poisson", 1000.0, ["DAN"], 100.0)]
    outs = [out("avoid", ["MBON"])]
    write("k_depress", circuit("Fly: learning to avoid an odour", "k_depress", pops, projs, ins, outs, notes="docs/circuits/bestiary_b.md"))


def write_k_sparse():
    write("k_sparse", k_sparse())


def fly_mb():
    write("fly_mb", mb_core("fly_mb", "Fly: the mushroom body learns to avoid"))


DX.update({"dx_mb_apl": ("fly_mb.json", "APL>KC"), "dx_mb_dan": ("fly_mb.json", "TSN>DAN")})
ALL.update({"k_sparse": write_k_sparse, "k_depress": k_depress, "fly_mb": fly_mb, "dx_mb_apl": lambda: write_dx("dx_mb_apl"), "dx_mb_dan": lambda: write_dx("dx_mb_dan")})


# ---------------------------------------------------------------- the crab pyloric rhythm (research/bestiary-a.md section 4)
K = 628.0  # nS of a 1 mS/cm2 conductance in a 0.628e-3 cm2 cell (and pF of 1 uF/cm2)


def stg(gNa, gCaT, gCaS, gA, gKCa, gKd, gH, gL, Ie=0.0):
    """stg_cell parameters from maximal conductances in mS/cm2 (after the Prinz/Liu parameter ranges; game values, not the published sets)."""
    return {"g_Na": round(gNa * K), "g_CaT": round(gCaT * K, 1), "g_CaS": round(gCaS * K, 1), "g_A": round(gA * K), "g_KCa": round(gKCa * K, 1), "g_Kd": round(gKd * K), "g_H": round(gH * K, 2), "g_L": round(gL * K, 2), "I_e": Ie}


AB = dict(gNa=400, gCaT=2.5, gCaS=6, gA=50, gKCa=5, gKd=100, gH=0.01, gL=0.0)
PDC = dict(gNa=100, gCaT=2.5, gCaS=6, gA=50, gKCa=5, gKd=100, gH=0.01, gL=0.01, Ie=-50.0)
# two hand-picked parameter sets that all give the pyloric pattern (degeneracy, Prinz et al. 2004): found by a random search over the follower conductances and synapses (not a sweep shipped), then fixed
SETS = {
    "a": dict(lp=dict(gNa=100, gCaT=0, gCaS=2, gA=10, gKCa=0, gKd=50, gH=0.03, gL=0.01, Ie=0.0), py=dict(gNa=100, gCaT=4, gCaS=1, gA=80, gKCa=0, gKd=125, gH=0.1, gL=0.01, Ie=50.0),
              w=dict(ab_f=40.0, pd_f=20.0, lp_py=10.0, py_lp=30.0, lp_pd=10.0)),
    "b": dict(lp=dict(gNa=100, gCaT=0, gCaS=4, gA=40, gKCa=2, gKd=25, gH=0.1, gL=0.03, Ie=0.0), py=dict(gNa=100, gCaT=2.5, gCaS=1, gA=50, gKCa=0, gKd=50, gH=0.1, gL=0.03, Ie=150.0),
              w=dict(ab_f=20.0, pd_f=20.0, lp_py=60.0, py_lp=10.0, lp_pd=10.0)),
}


def crab(setname="a", task="crab_pyloric", name="Crab: the pyloric rhythm"):
    S = SETS[setname]
    w = S["w"]
    pops = [popp("AB", "stg_cell", 1, stg(**AB, Ie=-50.0), [[120, 120]]), popp("PD", "stg_cell", 2, stg(**PDC), [[120, 240], [150, 240]]),
            popp("LP", "stg_cell", 1, stg(**S["lp"]), [[360, 120]]), popp("PY", "stg_cell", 2, stg(**S["py"]), [[360, 240], [390, 240]])]
    projs = [proj("AB", "PD", 100.0), proj("PD", "AB", 20.0), proj("AB", "LP", -w["ab_f"]), proj("AB", "PY", -w["ab_f"]), proj("PD", "LP", -w["pd_f"]), proj("PD", "PY", -w["pd_f"]),
             proj("LP", "PD", -w["lp_pd"]), proj("LP", "PY", -w["lp_py"]), proj("PY", "LP", -w["py_lp"])]
    ins = [inp("modulator", "dc", 50.0, ["AB", "PD"], 1.0)]
    outs = [out("pd", ["PD"]), out("lp", ["LP"]), out("py", ["PY"])]
    return circuit(name, task, pops, projs, ins, outs, notes="docs/circuits/bestiary_b.md")


def s_pacemaker():
    pops = [popp("AB", "stg_cell", 1, stg(**AB, Ie=-50.0), [[160, 150]]), popp("PD", "stg_cell", 2, stg(**PDC), [[360, 120], [360, 180]])]
    projs = [proj("AB", "PD", 100.0), proj("PD", "AB", 20.0)]
    ins = [inp("modulator", "dc", 50.0, ["AB", "PD"], 1.0)]
    outs = [out("ab", ["AB"]), out("pd", ["PD"])]
    write("s_pacemaker", circuit("Crab: a pacemaker kernel", "s_pacemaker", pops, projs, ins, outs, notes="docs/circuits/bestiary_b.md"))


def s_phase():
    lp = SETS["a"]["lp"]
    pops = [popp("LP", "stg_cell", 1, stg(**lp), [[320, 150]])]
    ins = [inp("pd", "poisson", -20.0, ["LP"], 100.0)]
    outs = [out("lp", ["LP"])]
    write("s_phase", circuit("Crab: phase from inhibition and rebound", "s_phase", pops, [], ins, outs, notes="docs/circuits/bestiary_b.md"))


def crab_pyloric():
    write("crab_pyloric", crab("a"))
    write("crab_pyloric_b", crab("b", name="Crab: the pyloric rhythm (another parameter set)"))


DX.update({"dx_crab": ("crab_pyloric_b.json", "LP>PY")})
ALL.update({"s_pacemaker": s_pacemaker, "s_phase": s_phase, "crab_pyloric": crab_pyloric, "dx_crab": lambda: write_dx("dx_crab")})


if __name__ == "__main__":
    for k in (sys.argv[1:] or list(ALL)):
        ALL[k]()
