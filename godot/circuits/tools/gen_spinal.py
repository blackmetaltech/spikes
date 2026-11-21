#!/usr/bin/env python3
"""Generates the reference circuits of the "spinal" chapter: circuits/{stretch_reflex,renshaw,posture,reach}.json.
Run from anywhere:  python3 circuits/tools/gen_spinal.py [stretch_reflex renshaw posture reach]
Design rationale and citations: docs/circuits/<id>.md.  Units are NEST units (mV, ms, pA, nS, Hz)."""
import json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, ".."))


def pop(pid, model, n, params, x, y, dy=34, role="neuron"):
    return {"id": pid, "model": model, "n": n, "params": params,
            "pos": [[x, y + i * dy] for i in range(n)], "role": role}


def proj(pre, post, rule, w, d, **kw):
    p = {"pre": pre, "post": post, "rule": rule,
         "synapse": {"synapse_model": "static_synapse", "weight": w, "delay": d}}
    p.update(kw)
    return p


import random


def edges_all(npre, npost, w, delay, spread=0.5, seed=1, p=1.0):
    """Explicit all-to-all edges with weights w * U(1 - spread, 1 + spread) (Ia EPSP sizes vary a lot between motoneurons;
    Henneman & Mendell 1981). Different input strength per motoneuron also desynchronises a pool of identical cells."""
    r = random.Random(seed)
    return [[i, j, round(w * (1 - spread + 2 * spread * r.random()), 4), delay]
            for j in range(npost) for i in range(npre) if p >= 1.0 or r.random() < p]


def eproj(pre, post, edges):
    return {"pre": pre, "post": post, "edges": edges, "synapse": {"synapse_model": "static_synapse"}}


def write(name, c):
    with open(os.path.join(OUT, name), "w") as f:
        json.dump(c, f, indent=1)
        f.write("\n")
    n = sum(p["n"] for p in c["populations"])
    print(f"wrote circuits/{name}: {n} neurons, {len(c['projections'])} projections")


def allout(p, n):
    return {"pop": p, "idx": list(range(n))}


# ---------------------------------------------------------------------------------------------------------------
# Cell parameters (NEST names). Alpha motoneuron: aeif_cond_alpha as a motoneuron with an afterhyperpolarisation
# (spike-triggered adaptation b, tau_w ~ AHP time course; Eccles, Eccles & Lundberg 1957; Kernell 1965 for the f-I slope):
# rheobase ~ 800 pA, about 100 Hz per nA, tau_m 10 ms.
MN = dict(C_m=500.0, g_L=50.0, E_L=-70.0, V_th=-52.0, Delta_T=2.0, V_reset=-68.0, t_ref=3.0, a=2.0, b=40.0, tau_w=100.0,
          tau_syn_ex=1.5, tau_syn_in=4.0, E_ex=0.0, E_in=-80.0)
# Interneurons (Ia inhibitory interneuron, Renshaw cell): fast, non-adapting conductance LIF
IN = dict(C_m=100.0, g_L=10.0, E_L=-70.0, V_th=-55.0, V_reset=-70.0, t_ref=2.0,
          tau_syn_ex=1.5, tau_syn_in=3.0, E_ex=0.0, E_in=-80.0)


# Spindle afferent (Ia / II / Ib fibre): a nearly perfect integrator, so a dc "sensor" current of about 3.75 pA per Hz of the
# receptor's rate makes it fire regularly at that rate with random phases (real primary endings are very regular: CV ~ 0.1,
# Matthews 1972). tau_m = 100 ms keeps the f-I curve close to linear above ~40 Hz.
AFF = dict(C_m=250.0, tau_m=100.0, E_L=-70.0, V_th=-55.0, V_reset=-70.0, t_ref=2.0, tau_syn_ex=2.0)
AFF_W = 4.0   # pA per unit of the sensor channel (the task writes the receptor rate in Hz)


def core(P=None, renshaw=False, noise=False, name="", task=""):
    """Reflex core shared by stretch_reflex, renshaw: Ia afferents -> alpha motoneurons (monosynaptic) + Ia inhibitory interneurons ->
    antagonist motoneurons, optionally Renshaw cells (MN collaterals -> RC -> MN, Ia-IN and the antagonist's RC) and a shared
    descending noise source (common synaptic input to both pools)."""
    d = dict(
        nIa=6, nMN=20, nIN=3, nRC=3, nDesc=4,
        w_Ia_MN=5.0, w_Ia_IN=2.5, w_IN_MN=-6.0, w_IN_IN=0.0,
        w_MN_RC=3.0, w_RC_MN=-2.0, w_RC_IN=-3.0, w_RC_RC=-3.0,
        w_desc_MN=1.0, noise_hz=400.0,
        drive_MN=1050.0, spread=0.6,
    )
    d.update(P or {})
    mn = dict(MN, **d.get("MN", {}))
    rc = dict(IN, **d.get("RC", {}))
    inn = dict(IN, **d.get("IN", {}))
    pops, projs = [], []
    for s, y0 in (("F", 40), ("E", 400)):
        pops.append(pop(f"Ia_{s}", "iaf_psc_alpha", d["nIa"], dict(AFF), 60, y0, dy=30, role="sensor"))
        pops.append(pop(f"IN_{s}", "iaf_cond_alpha", d["nIN"], dict(inn), 260, y0 + 20, dy=40))
        pops.append(pop(f"MN_{s}", "aeif_cond_alpha", d["nMN"], dict(mn), 460, y0, dy=30, role="motor"))
        if renshaw:
            pops.append(pop(f"RC_{s}", "iaf_cond_alpha", d["nRC"], dict(rc), 660, y0 + 40, dy=40))
    if noise:
        pops.append(pop("DESC", "parrot_neuron", d["nDesc"], {}, 60, 250, dy=30, role="sensor"))
    for s, o in (("F", "E"), ("E", "F")):
        projs.append(eproj(f"Ia_{s}", f"MN_{s}", edges_all(d["nIa"], d["nMN"], d["w_Ia_MN"], 1.0, d["spread"], seed=7 + ord(s))))
        projs.append(proj(f"Ia_{s}", f"IN_{s}", "all_to_all", d["w_Ia_IN"], 1.0))
        projs.append(proj(f"IN_{s}", f"MN_{o}", "all_to_all", d["w_IN_MN"], 1.0))
        if d["w_IN_IN"]:
            projs.append(proj(f"IN_{s}", f"IN_{o}", "all_to_all", d["w_IN_IN"], 1.0))
        if renshaw:
            projs.append(proj(f"MN_{s}", f"RC_{s}", "all_to_all", d["w_MN_RC"], 1.0))
            projs.append(proj(f"RC_{s}", f"MN_{s}", "all_to_all", d["w_RC_MN"], 1.0))
            projs.append(proj(f"RC_{s}", f"IN_{s}", "all_to_all", d["w_RC_IN"], 1.0))
            projs.append(proj(f"RC_{s}", f"RC_{o}", "all_to_all", d["w_RC_RC"], 1.0))
        if noise:
            projs.append(eproj("DESC", f"MN_{s}", edges_all(d["nDesc"], d["nMN"], d["w_desc_MN"], 1.0, 0.8, seed=21 + ord(s))))
    inputs = [
        {"name": "ag_Ia", "kind": "dc", "targets": ["Ia_F"], "weight": AFF_W, "value": 40.0},
        {"name": "ant_Ia", "kind": "dc", "targets": ["Ia_E"], "weight": AFF_W, "value": 40.0},
        {"name": "drive", "kind": "dc", "targets": ["MN_F", "MN_E"], "weight": d["drive_MN"], "value": 1.0},
    ]
    if noise:
        inputs.append({"name": "noise", "kind": "poisson", "targets": ["DESC"], "weight": 1.0, "value": d["noise_hz"]})
    return {
        "schema_version": 1, "name": name, "resolution_ms": 0.1, "seed": 11,
        "populations": pops, "projections": projs, "inputs": inputs,
        "outputs": [
            {"name": "ag_motor", "kind": "rate", "neurons": [allout("MN_F", d["nMN"])]},
            {"name": "ant_motor", "kind": "rate", "neurons": [allout("MN_E", d["nMN"])]},
        ],
        "record": {"spikes": "all"},
        "meta": {"author": "spike", "task": task, "notes": f"docs/circuits/{task}.md"},
    }


def stretch(P=None):
    return core(dict({"w_IN_MN": -5.2, "nIN": 4, "w_Ia_IN": 2.25}, **(P or {})), name="Stretch reflex with reciprocal Ia inhibition", task="stretch_reflex")


def renshaw(P=None):
    d = dict(nIa=5, nMN=16, nIN=2, nRC=3, nDesc=2, w_Ia_MN=6.0, w_Ia_IN=3.0, w_IN_MN=-9.0, w_desc_MN=2.3,
             drive_MN=1000.0, w_MN_RC=3.0, w_RC_MN=-10.0)
    d.update(P or {})
    return core(d, renshaw=True, noise=True, name="Stretch reflex with Renshaw recurrent inhibition", task="renshaw")


def posture(P=None):
    """Stretch reflex + spindle secondaries (II: length) + gamma motoneurons + tonic descending tone. The tone (TONIC cells, a regular
    reticulospinal / vestibulospinal-like source fed by the dc 'drive') excites the flexor pool more than the extensor pool (against gravity)
    and the gamma pool (spindle sensitivity). Motoneurons get a standing bias current I_e (persistent inward currents set by monoamines,
    Heckman et al. 2003) just under their rheobase, so the tone and the Ia / II feedback decide who fires."""
    d = dict(
        nIa=4, nII=3, nTONIC=6, nGAM=4, nMN=14, nIN=2,
        w_Ia_MN=2.4, w_II_MN=1.6, w_Ia_IN=3.0, w_IN_MN=-8.0,
        w_T_F=18.5, w_T_E=5.0, w_T_G=6.0, tonic_w=200.0, I_e=650.0, I_e_G=650.0, spread=0.6,
    )
    d.update(P or {})
    mn = dict(MN, I_e=d["I_e"], **d.get("MN", {}))
    mn_e = dict(mn, I_e=d.get("I_e_E", d["I_e"]))  # the extensor pool may sit lower under its rheobase (less standing co-contraction)
    gm = dict(MN, I_e=d["I_e_G"], **d.get("GM", {}))
    pops, projs = [], []
    for s, y0 in (("F", 40), ("E", 420)):
        pops.append(pop(f"Ia_{s}", "iaf_psc_alpha", d["nIa"], dict(AFF), 60, y0, dy=30, role="sensor"))
        pops.append(pop(f"II_{s}", "iaf_psc_alpha", d["nII"], dict(AFF), 60, y0 + 140, dy=30, role="sensor"))
        pops.append(pop(f"IN_{s}", "iaf_cond_alpha", d["nIN"], dict(inn), 260, y0 + 20, dy=40))
        pops.append(pop(f"MN_{s}", "aeif_cond_alpha", d["nMN"], dict(mn if s == "F" else mn_e), 460, y0, dy=30, role="motor"))
    pops.append(pop("TONIC", "iaf_psc_alpha", d["nTONIC"], dict(AFF), 260, 240, dy=24, role="sensor"))
    pops.append(pop("GAM", "aeif_cond_alpha", d["nGAM"], dict(gm), 560, 330, dy=20, role="motor"))
    for s, o in (("F", "E"), ("E", "F")):
        sfx = "" if s == "F" else "_E"
        projs.append(eproj(f"Ia_{s}", f"MN_{s}", edges_all(d["nIa"], d["nMN"], d.get("w_Ia_MN" + sfx, d["w_Ia_MN"]), 1.0, d["spread"], seed=7 + ord(s))))
        projs.append(eproj(f"II_{s}", f"MN_{s}", edges_all(d["nII"], d["nMN"], d.get("w_II_MN" + sfx, d["w_II_MN"]), 2.0, d["spread"], seed=9 + ord(s))))
        projs.append(proj(f"Ia_{s}", f"IN_{s}", "all_to_all", d["w_Ia_IN"], 1.0))
        projs.append(proj(f"IN_{s}", f"MN_{o}", "all_to_all", d["w_IN_MN"], 1.0))
    projs.append(eproj("TONIC", "MN_F", edges_all(d["nTONIC"], d["nMN"], d["w_T_F"], 1.0, d["spread"], seed=31)))
    projs.append(eproj("TONIC", "MN_E", edges_all(d["nTONIC"], d["nMN"], d["w_T_E"], 1.0, d["spread"], seed=32)))
    projs.append(proj("TONIC", "GAM", "all_to_all", d["w_T_G"], 1.0))
    return {
        "schema_version": 1, "name": "Posture: stretch reflex, gamma drive and tonic descending input", "resolution_ms": 0.1, "seed": 13,
        "populations": pops, "projections": projs,
        "inputs": [
            {"name": "ag_Ia", "kind": "dc", "targets": ["Ia_F"], "weight": AFF_W, "value": 10.0},
            {"name": "ant_Ia", "kind": "dc", "targets": ["Ia_E"], "weight": AFF_W, "value": 10.0},
            {"name": "ag_II", "kind": "dc", "targets": ["II_F"], "weight": AFF_W, "value": 10.0},
            {"name": "ant_II", "kind": "dc", "targets": ["II_E"], "weight": AFF_W, "value": 10.0},
            {"name": "drive", "kind": "dc", "targets": ["TONIC"], "weight": d["tonic_w"], "value": 1.0},
        ],
        "outputs": [
            {"name": "ag_motor", "kind": "rate", "neurons": [allout("MN_F", d["nMN"])]},
            {"name": "ant_motor", "kind": "rate", "neurons": [allout("MN_E", d["nMN"])]},
            {"name": "gamma_motor", "kind": "rate", "neurons": [allout("GAM", d["nGAM"])]},
        ],
        "record": {"spikes": "all"},
        "meta": {"author": "spike", "task": "posture", "notes": "docs/circuits/posture.md"},
    }


def reach(P=None):
    """Triphasic pattern generator + stretch reflex. 'go' starts AG1 (agonist burst 1; its spike-triggered adaptation ends the burst), which
    after a conduction/synaptic delay starts B2 (the antagonist burst, which also silences the agonist and AG1), which after another delay
    starts B3 (the small second agonist burst that clamps the limb). 'target' (100 x distance in rad) scales every burst (it drives AG1,
    B2 and B3 together with their triggers) and, held on, shifts the tonic balance of the two pools to the new angle (equilibrium-point
    command, Feldman 1986). The Ia reflex then holds the limb there."""
    d = dict(
        nIa=5, nIN=2, nMN=12, nTGT=3, nGO=3, nB=4,
        w_Ia_MN=5.0, w_Ia_IN=3.0, w_IN_MN=-8.0, spread=0.6, drive_MN=1000.0,
        # hold command: the target shifts the tonic balance (flexor excited, extensor inhibited)
        w_T_MNF=5.95, w_T_MNE=-10.0,
        # burst generator: target scales each burst (sub-threshold drive), go triggers B1, delays chain B1 -> B2 -> B3
        w_T_B1=2.185, w_T_B2=1.32, w_T_B3=0.0, w_G_B1=30.0,
        w_B1_MNF=2.85, w_B1_MNE=-1.86, w_B1_B2=6.0, d_B1_B2=212.4,
        w_B2_MNE=10.15, w_B2_MNF=-5.13, w_B2_B1=-11.11, w_B2_B3=6.23, d_B2_B3=103.16,
        w_B3_MNF=2.32, w_B3_B2=-10.74,
    )
    d.update(P or {})
    mn = dict(MN, **d.get("MN", {}))
    bn = dict(MN, **dict(dict(C_m=200.0, g_L=20.0, V_reset=-65.0, b=34.4, tau_w=91.5, tau_syn_ex=30.0, tau_syn_in=40.0), **d.get("BN", {})))
    pops, projs = [], []
    for s, y0 in (("F", 40), ("E", 420)):
        pops.append(pop(f"Ia_{s}", "iaf_psc_alpha", d["nIa"], dict(AFF), 60, y0, dy=30, role="sensor"))
        pops.append(pop(f"IN_{s}", "iaf_cond_alpha", d["nIN"], dict(inn), 260, y0 + 20, dy=40))
        pops.append(pop(f"MN_{s}", "aeif_cond_alpha", d["nMN"], dict(mn), 660, y0, dy=30, role="motor"))
    pops.append(pop("TGT", "iaf_psc_alpha", d["nTGT"], dict(AFF), 60, 230, dy=30, role="sensor"))
    pops.append(pop("GO", "iaf_psc_alpha", d["nGO"], dict(AFF), 60, 340, dy=30, role="sensor"))
    pops.append(pop("B1", "aeif_cond_alpha", d["nB"], dict(bn), 260, 200, dy=30))
    pops.append(pop("B2", "aeif_cond_alpha", d["nB"], dict(bn), 360, 280, dy=30))
    pops.append(pop("B3", "aeif_cond_alpha", d["nB"], dict(bn), 460, 360, dy=30))
    for s, o in (("F", "E"), ("E", "F")):
        projs.append(eproj(f"Ia_{s}", f"MN_{s}", edges_all(d["nIa"], d["nMN"], d["w_Ia_MN"], 1.0, d["spread"], seed=7 + ord(s))))
        projs.append(proj(f"Ia_{s}", f"IN_{s}", "all_to_all", d["w_Ia_IN"], 1.0))
        projs.append(proj(f"IN_{s}", f"MN_{o}", "all_to_all", d["w_IN_MN"], 1.0))
    projs.append(proj("TGT", "MN_F", "all_to_all", d["w_T_MNF"], 1.0))
    projs.append(proj("TGT", "MN_E", "all_to_all", d["w_T_MNE"], 1.0))
    projs.append(proj("TGT", "B1", "all_to_all", d["w_T_B1"], 1.0))
    projs.append(proj("TGT", "B2", "all_to_all", d["w_T_B2"], 1.0))
    projs.append(proj("TGT", "B3", "all_to_all", d["w_T_B3"], 1.0))
    projs.append(proj("GO", "B1", "all_to_all", d["w_G_B1"], 1.0))
    projs.append(proj("B1", "MN_F", "all_to_all", d["w_B1_MNF"], 1.0))
    projs.append(proj("B1", "MN_E", "all_to_all", d["w_B1_MNE"], 1.0))
    projs.append(proj("B1", "B2", "all_to_all", d["w_B1_B2"], d["d_B1_B2"]))
    projs.append(proj("B2", "MN_E", "all_to_all", d["w_B2_MNE"], 1.0))
    projs.append(proj("B2", "MN_F", "all_to_all", d["w_B2_MNF"], 1.0))
    projs.append(proj("B2", "B1", "all_to_all", d["w_B2_B1"], 1.0))
    projs.append(proj("B2", "B3", "all_to_all", d["w_B2_B3"], d["d_B2_B3"]))
    projs.append(proj("B3", "MN_F", "all_to_all", d["w_B3_MNF"], 1.0))
    if d["w_B3_B2"]:
        projs.append(proj("B3", "B2", "all_to_all", d["w_B3_B2"], 1.0))
    return {
        "schema_version": 1, "name": "Reach: triphasic burst generator + stretch reflex", "resolution_ms": 0.1, "seed": 17,
        "populations": pops, "projections": projs,
        "inputs": [
            {"name": "go", "kind": "dc", "targets": ["GO"], "weight": AFF_W, "value": 0.0},
            {"name": "target", "kind": "dc", "targets": ["TGT"], "weight": AFF_W, "value": 0.0},
            {"name": "ag_Ia", "kind": "dc", "targets": ["Ia_F"], "weight": AFF_W, "value": 40.0},
            {"name": "ant_Ia", "kind": "dc", "targets": ["Ia_E"], "weight": AFF_W, "value": 40.0},
            {"name": "drive", "kind": "dc", "targets": ["MN_F", "MN_E"], "weight": d["drive_MN"], "value": 1.0},
        ],
        "outputs": [
            {"name": "ag_motor", "kind": "rate", "neurons": [allout("MN_F", d["nMN"])]},
            {"name": "ant_motor", "kind": "rate", "neurons": [allout("MN_E", d["nMN"])]},
            {"name": "burst1", "kind": "rate", "neurons": [allout("B1", d["nB"])]},
            {"name": "burst2", "kind": "rate", "neurons": [allout("B2", d["nB"])]},
            {"name": "burst3", "kind": "rate", "neurons": [allout("B3", d["nB"])]},
        ],
        "record": {"spikes": "all"},
        "meta": {"author": "spike", "task": "reach", "notes": "docs/circuits/reach.md"},
    }


TASKS = {"stretch_reflex": lambda P=None: stretch(P), "renshaw": lambda P=None: renshaw(P), "posture": lambda P=None: posture(P), "reach": lambda P=None: reach(P)}

if __name__ == "__main__":
    which = sys.argv[1:] or list(TASKS)
    for w in which:
        write(f"{w}.json", TASKS[w]())
