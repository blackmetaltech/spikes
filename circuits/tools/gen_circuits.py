#!/usr/bin/env python3
"""Generates circuits/*.json (the three reference circuits) and circuits/broken/*.json.
Run from anywhere:  python3 circuits/tools/gen_circuits.py
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


def write(name, c):
    with open(os.path.join(OUT, name), "w") as f:
        json.dump(c, f, indent=1)
        f.write("\n")
    n = sum(p["n"] for p in c["populations"])
    print(f"wrote circuits/{name}: {n} neurons, {len(c['projections'])} projections")


# ---------------------------------------------------------------------------------------------------------------
LAMPREY = dict(
    E_n=4, CC_n=2, MN_n=3,
    # aeif_cond_alpha (Brette & Gerstner 2005): NEST defaults except the adaptation (a, b, tau_w), the synaptic time
    # constants (slow tau_syn_ex = NMDA-like, slow tau_syn_in = glycinergic) and the initial membrane potential.
    E_a=8.4, E_b=75.0, E_tau_w=320.0, E_tau_syn=12.0, E_tau_syn_in=20.0, CC_tau_syn=3.3, MN_tau_syn=4.4,
    drive=1050.0,                      # pA per unit of the "drive" input (the task sets drive = 1.0)
    w_EE=0.9, w_ECC=6.0, w_EMN=2.0, w_CCE=-19.0, w_CCMN=-28.0,   # nS
    V_L=-62.0, V_R=-70.6,              # initial V_m: breaks the L/R symmetry (the network has no noise)
)


def flat_to_P(task, s):
    return dict(s)


def expand(task, s):
    """Search helper: flat keys such as SN_tau_m / IN_tau_m / MN_tau_m become the *_extra dicts."""
    s = dict(s)
    for pre in ("SN", "IN", "MN"):
        ex = {k[len(pre) + 1:]: s.pop(k) for k in list(s) if k.startswith(pre + "_") and k not in ("SN_n", "IN_n", "MN_n")}
        if ex:
            s[pre + "_extra"] = ex
    return s


def lamprey(P=None):
    d = dict(LAMPREY)
    d.update(P or {})
    pops, projs = [], []
    for s, x0, v0 in (("L", 100, d["V_L"]), ("R", 560, d["V_R"])):
        ep = dict(a=d["E_a"], b=d["E_b"], tau_w=d["E_tau_w"], tau_syn_ex=d["E_tau_syn"], tau_syn_in=d["E_tau_syn_in"], V_m=v0)
        cp = dict(tau_syn_ex=d["CC_tau_syn"])
        mp = dict(tau_syn_ex=d["MN_tau_syn"])
        pops.append(pop(f"E_{s}", "aeif_cond_alpha", d["E_n"], ep, x0, 60))
        pops.append(pop(f"CC_{s}", "aeif_cond_alpha", d["CC_n"], cp, x0 + 120, 100, dy=50))
        pops.append(pop(f"MN_{s}", "iaf_cond_alpha", d["MN_n"], mp, x0 + 240, 60, dy=50, role="motor"))
    for s in "LR":
        o = "R" if s == "L" else "L"
        projs.append(proj(f"E_{s}", f"E_{s}", "all_to_all", d["w_EE"], 1.0, allow_autapses=False))
        projs.append(proj(f"E_{s}", f"CC_{s}", "all_to_all", d["w_ECC"], 1.0))
        projs.append(proj(f"E_{s}", f"MN_{s}", "all_to_all", d["w_EMN"], 1.0))
        projs.append(proj(f"CC_{s}", f"E_{o}", "all_to_all", d["w_CCE"], 2.0))
        projs.append(proj(f"CC_{s}", f"MN_{o}", "all_to_all", d["w_CCMN"], 2.0))
    c = {
        "schema_version": 1, "name": "Lamprey half-centre swim CPG", "resolution_ms": 0.1, "seed": 42,
        "populations": pops, "projections": projs,
        "inputs": [{"name": "drive", "kind": "dc", "targets": ["E_L", "E_R"], "weight": d["drive"], "value": 1.0}],
        "outputs": [{"name": "left_motor", "kind": "rate", "neurons": [{"pop": "MN_L", "idx": list(range(d["MN_n"]))}]},
                    {"name": "right_motor", "kind": "rate", "neurons": [{"pop": "MN_R", "idx": list(range(d["MN_n"]))}]}],
        "record": {"spikes": "all"},
        "meta": {"author": "spike", "task": "lamprey", "notes": "docs/circuits/lamprey.md"},
    }
    return c


# ---------------------------------------------------------------------------------------------------------------
REFLEX = dict(
    SN_n=8, IN_n=4, MN_n=3,
    w_touch=400.0,          # pA per Poisson spike, afferent input
    w_SNIN=270.0, w_INMN=380.0,   # pA per spike
    SN_extra=dict(), IN_extra=dict(), MN_extra=dict(),
    d_SNIN=1.0, d_INMN=1.0,
)


def reflex(P=None):
    d = dict(REFLEX)
    d.update(P or {})
    pops = [
        pop("SN", "iaf_psc_alpha", d["SN_n"], dict(d["SN_extra"]), 60, 40, dy=40, role="sensor"),
        pop("IN", "iaf_psc_alpha", d["IN_n"], dict(d["IN_extra"]), 260, 80, dy=50),
        pop("MN", "iaf_psc_alpha", d["MN_n"], dict(d["MN_extra"]), 460, 100, dy=50, role="motor"),
    ]
    projs = [
        proj("SN", "IN", "all_to_all", d["w_SNIN"], d["d_SNIN"]),
        proj("IN", "MN", "all_to_all", d["w_INMN"], d["d_INMN"]),
    ]
    return {
        "schema_version": 1, "name": "Withdrawal reflex arc", "resolution_ms": 0.1, "seed": 7,
        "populations": pops, "projections": projs,
        "inputs": [{"name": "touch", "kind": "poisson", "targets": ["SN"], "weight": d["w_touch"], "value": 15.0}],
        "outputs": [{"name": "withdraw", "kind": "rate", "neurons": [{"pop": "MN", "idx": list(range(d["MN_n"]))}]}],
        "record": {"spikes": "all"},
        "meta": {"author": "spike", "task": "reflex", "notes": "docs/circuits/reflex.md"},
    }


# ---------------------------------------------------------------------------------------------------------------
WORM = dict(
    n=2,
    w_sens=1000.0,                # pA per Poisson spike: one input spike -> one ASE spike
    w_ASEL_AIY=800.0, w_AIY_AIB=-1500.0, w_ASER_AIB=1000.0,
    w_AIB_TR=1000.0, w_AIY_TL=0.0, w_AIB_TL=0.0,
    AIB_Ie=450.0, TL_Ie=0.0,     # pA; the rheobase of iaf_psc_alpha is 375 pA
    extra=dict(),
)


def worm(P=None):
    d = dict(WORM)
    d.update(P or {})
    n = d["n"]
    mk = lambda pid, x, y, role="neuron", I_e=0.0: pop(pid, "iaf_psc_alpha", n, dict(d["extra"], I_e=I_e), x, y, dy=44, role=role)
    pops = [mk("ASEL", 60, 40, "sensor"), mk("ASER", 60, 220, "sensor"),
            mk("AIY", 250, 40), mk("AIB", 250, 220, I_e=d["AIB_Ie"]),
            mk("TL", 450, 40, "motor", I_e=d["TL_Ie"]), mk("TR", 450, 220, "motor")]
    projs = [
        proj("ASEL", "AIY", "all_to_all", d["w_ASEL_AIY"], 1.0),
        proj("AIY", "AIB", "all_to_all", d["w_AIY_AIB"], 1.0),
        proj("ASER", "AIB", "all_to_all", d["w_ASER_AIB"], 1.0),
        proj("AIB", "TR", "all_to_all", d["w_AIB_TR"], 1.0),
    ]
    if d["w_AIY_TL"]:
        projs.append(proj("AIY", "TL", "all_to_all", d["w_AIY_TL"], 1.0))
    if d["w_AIB_TL"]:
        projs.append(proj("AIB", "TL", "all_to_all", d["w_AIB_TL"], 1.0))
    idx = list(range(n))
    return {
        "schema_version": 1, "name": "Worm ASEL/ASER chemotaxis", "resolution_ms": 0.1, "seed": 11,
        "populations": pops, "projections": projs,
        "inputs": [{"name": "ASEL", "kind": "poisson", "targets": ["ASEL"], "weight": d["w_sens"], "value": 5.0},
                   {"name": "ASER", "kind": "poisson", "targets": ["ASER"], "weight": d["w_sens"], "value": 5.0}],
        "outputs": [{"name": "turn_left", "kind": "rate", "neurons": [{"pop": "TL", "idx": idx}]},
                    {"name": "turn_right", "kind": "rate", "neurons": [{"pop": "TR", "idx": idx}]}],
        "record": {"spikes": "all"},
        "meta": {"author": "spike", "task": "worm", "notes": "docs/circuits/worm.md"},
    }


if __name__ == "__main__":
    which = sys.argv[1:] or ["reflex", "lamprey", "worm"]
    if "reflex" in which:
        write("reflex_arc.json", reflex())
    if "lamprey" in which:
        write("lamprey_cpg.json", lamprey())
    if "worm" in which:
        write("worm_chemotaxis.json", worm())
