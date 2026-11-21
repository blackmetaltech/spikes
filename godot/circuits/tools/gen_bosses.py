#!/usr/bin/env python3
"""Generates the boss reference circuits: circuits/boss_spinal_sprint.json (and, further down, boss_autonomic_exam.json).
Run from anywhere:  python3 circuits/tools/gen_bosses.py [boss_spinal_sprint boss_autonomic_exam]
Design rationale and citations: docs/circuits/boss_*.md.  Units are NEST units (mV, ms, pA, nS, Hz).  The spinal sprint reuses the cell
parameters and helper functions of gen_spinal.py (posture + Renshaw + reach generator in one circuit)."""
import json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gen_spinal as G  # noqa: E402

OUT = G.OUT


def sprint(P=None):
    """Posture circuit (Ia / II spindle reflex, reciprocal inhibition, gamma pool, tonic descending drive) + Renshaw cells on both pools +
    the triphasic burst generator of the reach (GO, TGT, B1-B3). Motoneurons carry the posture bias current I_e (just under rheobase)."""
    d = dict(
        nIa=4, nII=3, nTONIC=6, nGAM=4, nMN=14, nIN=2, nRC=3, nTGT=3, nGO=3, nB=4,
        w_Ia_MN=2.4, w_II_MN=1.6, w_Ia_IN=3.0, w_IN_MN=-8.0,
        w_T_F=18.5, w_T_E=5.0, w_T_G=6.0, tonic_w=200.0, I_e=650.0, I_e_G=650.0, spread=0.6,
        w_MN_RC=3.0, w_RC_MN=-3.0, w_RC_IN=-3.0, w_RC_RC=-3.0,
        # hold command and burst generator (as in reach)
        w_T_MNF=18.9, w_T_MNE=-16.0,
        w_T_B1=1.9, w_T_B2=1.32, w_T_B3=0.0, w_G_B1=24.0,
        w_B1_MNF=2.85, w_B1_MNE=-1.86, w_B1_B2=6.0, d_B1_B2=212.4,
        w_B2_MNE=7.0, w_B2_MNF=-5.13, w_B2_B1=-11.11, w_B2_B3=6.23, d_B2_B3=103.16,
        w_B3_MNF=2.32, w_B3_B2=-10.74,
    )
    d.update(P or {})
    mn = dict(G.MN, I_e=d["I_e"], **d.get("MN", {}))
    mn_e = dict(mn, I_e=d.get("I_e_E", d["I_e"]))
    gm = dict(G.MN, I_e=d["I_e_G"], **d.get("GM", {}))
    rc = dict(G.IN, **d.get("RC", {}))
    bn = dict(G.MN, **dict(dict(C_m=200.0, g_L=20.0, V_reset=-65.0, b=34.4, tau_w=91.5, tau_syn_ex=30.0, tau_syn_in=40.0), **d.get("BN", {})))
    pops, projs = [], []
    for s, y0 in (("F", 40), ("E", 420)):
        pops.append(G.pop(f"Ia_{s}", "iaf_psc_alpha", d["nIa"], dict(G.AFF), 60, y0, dy=30, role="sensor"))
        pops.append(G.pop(f"II_{s}", "iaf_psc_alpha", d["nII"], dict(G.AFF), 60, y0 + 140, dy=30, role="sensor"))
        pops.append(G.pop(f"IN_{s}", "iaf_cond_alpha", d["nIN"], dict(G.IN), 260, y0 + 20, dy=40))
        pops.append(G.pop(f"MN_{s}", "aeif_cond_alpha", d["nMN"], dict(mn if s == "F" else mn_e), 460, y0, dy=30, role="motor"))
        pops.append(G.pop(f"RC_{s}", "iaf_cond_alpha", d["nRC"], dict(rc), 660, y0 + 40, dy=40))
    pops.append(G.pop("TONIC", "iaf_psc_alpha", d["nTONIC"], dict(G.AFF), 260, 240, dy=24, role="sensor"))
    pops.append(G.pop("GAM", "aeif_cond_alpha", d["nGAM"], dict(gm), 560, 330, dy=20, role="motor"))
    pops.append(G.pop("TGT", "iaf_psc_alpha", d["nTGT"], dict(G.AFF), 60, 230, dy=30, role="sensor"))
    pops.append(G.pop("GO", "iaf_psc_alpha", d["nGO"], dict(G.AFF), 60, 340, dy=30, role="sensor"))
    pops.append(G.pop("B1", "aeif_cond_alpha", d["nB"], dict(bn), 260, 200, dy=30))
    pops.append(G.pop("B2", "aeif_cond_alpha", d["nB"], dict(bn), 360, 280, dy=30))
    pops.append(G.pop("B3", "aeif_cond_alpha", d["nB"], dict(bn), 460, 360, dy=30))
    for s, o in (("F", "E"), ("E", "F")):
        sfx = "" if s == "F" else "_E"
        projs.append(G.eproj(f"Ia_{s}", f"MN_{s}", G.edges_all(d["nIa"], d["nMN"], d.get("w_Ia_MN" + sfx, d["w_Ia_MN"]), 1.0, d["spread"], seed=7 + ord(s))))
        projs.append(G.eproj(f"II_{s}", f"MN_{s}", G.edges_all(d["nII"], d["nMN"], d.get("w_II_MN" + sfx, d["w_II_MN"]), 2.0, d["spread"], seed=9 + ord(s))))
        projs.append(G.proj(f"Ia_{s}", f"IN_{s}", "all_to_all", d["w_Ia_IN"], 1.0))
        projs.append(G.proj(f"IN_{s}", f"MN_{o}", "all_to_all", d["w_IN_MN"], 1.0))
        projs.append(G.proj(f"MN_{s}", f"RC_{s}", "all_to_all", d["w_MN_RC"], 1.0))
        projs.append(G.proj(f"RC_{s}", f"MN_{s}", "all_to_all", d["w_RC_MN"], 1.0))
        projs.append(G.proj(f"RC_{s}", f"IN_{s}", "all_to_all", d["w_RC_IN"], 1.0))
        projs.append(G.proj(f"RC_{s}", f"RC_{o}", "all_to_all", d["w_RC_RC"], 1.0))
    projs.append(G.eproj("TONIC", "MN_F", G.edges_all(d["nTONIC"], d["nMN"], d["w_T_F"], 1.0, d["spread"], seed=31)))
    projs.append(G.eproj("TONIC", "MN_E", G.edges_all(d["nTONIC"], d["nMN"], d["w_T_E"], 1.0, d["spread"], seed=32)))
    projs.append(G.proj("TONIC", "GAM", "all_to_all", d["w_T_G"], 1.0))
    projs.append(G.proj("TGT", "MN_F", "all_to_all", d["w_T_MNF"], 1.0))
    projs.append(G.proj("TGT", "MN_E", "all_to_all", d["w_T_MNE"], 1.0))
    projs.append(G.proj("TGT", "B1", "all_to_all", d["w_T_B1"], 1.0))
    projs.append(G.proj("TGT", "B2", "all_to_all", d["w_T_B2"], 1.0))
    if d["w_T_B3"]:
        projs.append(G.proj("TGT", "B3", "all_to_all", d["w_T_B3"], 1.0))
    projs.append(G.proj("GO", "B1", "all_to_all", d["w_G_B1"], 1.0))
    projs.append(G.proj("B1", "MN_F", "all_to_all", d["w_B1_MNF"], 1.0))
    projs.append(G.proj("B1", "MN_E", "all_to_all", d["w_B1_MNE"], 1.0))
    projs.append(G.proj("B1", "B2", "all_to_all", d["w_B1_B2"], d["d_B1_B2"]))
    projs.append(G.proj("B2", "MN_E", "all_to_all", d["w_B2_MNE"], 1.0))
    projs.append(G.proj("B2", "MN_F", "all_to_all", d["w_B2_MNF"], 1.0))
    projs.append(G.proj("B2", "B1", "all_to_all", d["w_B2_B1"], 1.0))
    projs.append(G.proj("B2", "B3", "all_to_all", d["w_B2_B3"], d["d_B2_B3"]))
    projs.append(G.proj("B3", "MN_F", "all_to_all", d["w_B3_MNF"], 1.0))
    if d["w_B3_B2"]:
        projs.append(G.proj("B3", "B2", "all_to_all", d["w_B3_B2"], 1.0))
    A = G.AFF_W
    return {
        "schema_version": 1, "name": "Spinal sprint: posture + Renshaw + reach generator", "resolution_ms": 0.1, "seed": 19,
        "populations": pops, "projections": projs,
        "inputs": [
            {"name": "ag_Ia", "kind": "dc", "targets": ["Ia_F"], "weight": A, "value": 10.0},
            {"name": "ant_Ia", "kind": "dc", "targets": ["Ia_E"], "weight": A, "value": 10.0},
            {"name": "ag_II", "kind": "dc", "targets": ["II_F"], "weight": A, "value": 10.0},
            {"name": "ant_II", "kind": "dc", "targets": ["II_E"], "weight": A, "value": 10.0},
            {"name": "drive", "kind": "dc", "targets": ["TONIC"], "weight": d["tonic_w"], "value": 1.0},
            {"name": "go", "kind": "dc", "targets": ["GO"], "weight": A, "value": 0.0},
            {"name": "target", "kind": "dc", "targets": ["TGT"], "weight": A, "value": 0.0},
        ],
        "outputs": [
            {"name": "ag_motor", "kind": "rate", "neurons": [G.allout("MN_F", d["nMN"])]},
            {"name": "ant_motor", "kind": "rate", "neurons": [G.allout("MN_E", d["nMN"])]},
            {"name": "gamma_motor", "kind": "rate", "neurons": [G.allout("GAM", d["nGAM"])]},
            {"name": "burst1", "kind": "rate", "neurons": [G.allout("B1", d["nB"])]},
            {"name": "burst2", "kind": "rate", "neurons": [G.allout("B2", d["nB"])]},
            {"name": "burst3", "kind": "rate", "neurons": [G.allout("B3", d["nB"])]},
        ],
        "record": {"spikes": "all"},
        "meta": {"author": "spike", "task": "boss_spinal_sprint", "notes": "docs/circuits/boss_spinal_sprint.md"},
    }


def exam(P=None):
    """The whole autonomic system in one circuit. Baroreflex core (AFF, NTS, NA/VG vagal limb, CVLM -| RVLM, IML, SG: baroreflex.json),
    central command (exercise relay CC, hypothalamic defence area HYP) that excites RVLM, the pupil dilator pathway (SCG) and the ventral respiratory
    group (VRG) and inhibits the vagal (NA), depressor (CVLM) and Edinger-Westphal (EW) cells (baroreflex resetting, vagal withdrawal), the
    ciliary ganglion (CG) sphincter arm, and a chemoreceptor relay (CHEMO) onto VRG (the CO2 feedback of the breathing task)."""
    d = dict(
        w_baro=1600.0, w_AFF_NTS=280.0, w_NTS_NA=30.0, w_NTS_CVLM=200.0, w_CVLM_RVLM=-140.0,
        # central command
        w_HYP_RVLM=60.0, w_CC_RVLM=60.0, w_HYP_CVLM=-120.0, w_CC_CVLM=-120.0, w_HYP_NA=-150.0, w_CC_NA=-150.0,
        w_HYP_SCG=60.0, w_HYP_EW=-150.0, w_HYP_VRG=100.0, w_CC_VRG=60.0, w_CHEMO_VRG=30.0,
        w_exercise=800.0, w_threat=250.0, w_chemo=400.0,
    )
    d.update(P or {})
    ie = lambda **kw: dict(kw)
    pops = [
        G.pop("AFF", "iaf_psc_alpha", 6, {}, 40, 120, dy=30, role="sensor"),
        G.pop("NTS", "iaf_psc_alpha", 8, {}, 180, 120, dy=26),
        G.pop("NA", "iaf_psc_alpha", 4, {"I_e": 372.0, "tau_syn_in": 10.0}, 320, 40, dy=30),
        G.pop("VG", "iaf_psc_alpha", 4, {}, 460, 40, dy=30, role="motor"),
        G.pop("CVLM", "iaf_psc_alpha", 6, {}, 320, 200, dy=30),
        G.pop("RVLM", "iaf_psc_alpha", 6, {"I_e": 380.0}, 460, 200, dy=30),
        G.pop("IML", "iaf_psc_alpha", 6, {}, 600, 200, dy=30),
        G.pop("SG", "iaf_psc_alpha", 6, {}, 740, 200, dy=30, role="motor"),
        G.pop("SA", "sa_node_pacemaker", 1, {"V_m": -60.0}, 600, 60, role="motor"),
        G.pop("CC", "iaf_psc_alpha", 4, {}, 40, 400, dy=30, role="sensor"),
        G.pop("HYP", "iaf_psc_alpha", 4, {"I_e": 100.0, "tau_syn_in": 10.0}, 40, 560, dy=30, role="sensor"),
        G.pop("SCG", "iaf_psc_alpha", 4, {"I_e": 377.0}, 320, 560, dy=30, role="motor"),
        G.pop("EW", "iaf_psc_alpha", 4, {"I_e": 377.0, "tau_syn_in": 10.0}, 180, 640, dy=30),
        G.pop("CG", "iaf_psc_alpha", 4, {"I_e": 340.0}, 320, 680, dy=30, role="motor"),
        G.pop("CHEMO", "iaf_psc_alpha", 4, {}, 40, 760, dy=30, role="sensor"),
        G.pop("VRG", "iaf_psc_alpha", 4, {"I_e": 377.0}, 320, 800, dy=30, role="motor"),
    ]
    pr = G.proj
    projs = [
        pr("AFF", "NTS", "all_to_all", d["w_AFF_NTS"], 1.0), pr("NTS", "NA", "all_to_all", d["w_NTS_NA"], 1.0),
        pr("NA", "VG", "one_to_one", 1500.0, 1.0), pr("NTS", "CVLM", "all_to_all", d["w_NTS_CVLM"], 1.0),
        pr("CVLM", "RVLM", "all_to_all", d["w_CVLM_RVLM"], 1.0), pr("RVLM", "IML", "one_to_one", 1500.0, 1.0), pr("IML", "SG", "one_to_one", 1500.0, 1.0),
        pr("HYP", "RVLM", "all_to_all", d["w_HYP_RVLM"], 3.0), pr("CC", "RVLM", "all_to_all", d["w_CC_RVLM"], 3.0),
        pr("HYP", "CVLM", "all_to_all", d["w_HYP_CVLM"], 3.0), pr("CC", "CVLM", "all_to_all", d["w_CC_CVLM"], 3.0),
        pr("HYP", "NA", "all_to_all", d["w_HYP_NA"], 3.0), pr("CC", "NA", "all_to_all", d["w_CC_NA"], 3.0),
        pr("HYP", "SCG", "all_to_all", d["w_HYP_SCG"], 3.0),
        pr("EW", "CG", "all_to_all", 60.0, 3.0), pr("HYP", "EW", "all_to_all", d["w_HYP_EW"], 3.0),
        pr("HYP", "VRG", "all_to_all", d["w_HYP_VRG"], 3.0), pr("CC", "VRG", "all_to_all", d["w_CC_VRG"], 3.0), pr("CHEMO", "VRG", "all_to_all", d["w_CHEMO_VRG"], 3.0),
    ]
    return {
        "schema_version": 1, "name": "Autonomic exam: baroreflex + central command + pupil + breathing", "resolution_ms": 0.1, "seed": 37,
        "populations": pops, "projections": projs,
        "inputs": [
            {"name": "baroreceptor", "kind": "poisson", "targets": ["AFF"], "weight": d["w_baro"], "value": 41.0},
            {"name": "exercise_cue", "kind": "poisson", "targets": ["CC"], "weight": d["w_exercise"], "value": 0.0},
            {"name": "threat", "kind": "poisson", "targets": ["HYP"], "weight": d["w_threat"], "value": 0.0},
            {"name": "chemo", "kind": "poisson", "targets": ["CHEMO"], "weight": d["w_chemo"], "value": 50.0},
        ],
        "outputs": [
            {"name": "sa_beat", "kind": "rate", "neurons": [{"pop": "SA"}]},
            {"name": "vagal", "kind": "modulator", "modulator": "ACh", "neurons": [{"pop": "VG"}], "gain": 0.012, "tau_ms": 500.0},
            {"name": "sympathetic", "kind": "modulator", "modulator": "NE", "neurons": [{"pop": "SG"}], "gain": 0.0076, "tau_ms": 1000.0},
            {"name": "dilator", "kind": "rate", "neurons": [{"pop": "SCG"}]},
            {"name": "sphincter", "kind": "rate", "neurons": [{"pop": "CG"}]},
            {"name": "vent", "kind": "rate", "neurons": [{"pop": "VRG"}]},
        ],
        "modulation": [
            {"modulator": "ACh", "pop": "SA", "param": "ach", "gain": 1.0},
            {"modulator": "NE", "pop": "SA", "param": "ne", "gain": 0.25},
        ],
        "record": {"spikes": "all"},
        "meta": {"author": "spike", "task": "boss_autonomic_exam", "notes": "docs/circuits/boss_autonomic_exam.md"},
    }


TASKS = {"boss_spinal_sprint": lambda P=None: sprint(P), "boss_autonomic_exam": lambda P=None: exam(P)}

if __name__ == "__main__":
    which = sys.argv[1:] or list(TASKS)
    for w in which:
        G.write(f"{w}.json", TASKS[w]())
