#!/usr/bin/env python3
"""Reference circuits of the micro-puzzles for concepts 11-19 (m_recruitment ... m_divergent), circuits/m_*.json.

Run from anywhere:  python3 circuits/tools/gen_micro_b.py   (rewrites the nine files; deterministic).
Every circuit is built the way the lab builds one (iaf_psc_alpha unless the puzzle needs another cell) with numbers a player can set
on the editor's sliders (weights snap to the square-law slider, biases to 5 pA steps).
"""
import json, os, sys

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
RHEO = 375.0


def pop(pid, n, model="iaf_psc_alpha", params=None, x=0, y=0, role="neuron"):
    return {"id": pid, "model": model, "n": n, "params": params or {}, "pos": [[x, y + 30 * k] for k in range(n)], "role": role}


def proj(pre, post, w, d=2.0, rule="all_to_all"):
    return {"pre": pre, "post": post, "rule": rule, "synapse": {"synapse_model": "static_synapse", "weight": w, "delay": d}}


def sensor(name, kind, targets, w, value=0.0):
    return {"name": name, "kind": kind, "targets": targets, "weight": w, "value": value}


def motor(name, pops):
    return {"name": name, "kind": "rate", "neurons": [{"pop": p} if isinstance(p, str) else p for p in pops]}


def circuit(name, pops, projs, ins, outs, task, notes, seed=11):
    return {"schema_version": 1, "name": name, "resolution_ms": 0.1, "seed": seed, "populations": pops, "projections": projs,
            "inputs": ins, "outputs": outs, "record": {"spikes": "all"},
            "meta": {"author": "spike", "task": task, "notes": notes}}


CIRCUITS = {}


def reg(fn):
    CIRCUITS[fn.__name__] = fn
    return fn


@reg
def m_recruitment():
    # three motor pools, one cell each; the same command, three thresholds (bias I_e): small +100, mid 0, large -350 pA
    pops = [pop("MU_S", 1, params={"I_e": 100.0}, x=300, y=40, role="motor"),
            pop("MU_M", 1, params={}, x=300, y=140, role="motor"),
            pop("MU_L", 1, params={"I_e": -350.0}, x=300, y=240, role="motor")]
    ins = [sensor("command", "dc", ["MU_S", "MU_M", "MU_L"], 300.0, 2.2)]
    outs = [motor("mu_small", ["MU_S"]), motor("mu_mid", ["MU_M"]), motor("mu_large", ["MU_L"])]
    return circuit("Recruitment: three thresholds", pops, [], ins, outs, "m_recruitment", "docs/circuits/m_recruitment.md")


@reg
def m_reciprocal():
    # tone on both motor pools; the Ia afferent excites its own motoneurons and an inhibitory interneuron that inhibits the antagonist's
    pops = [pop("MN_F", 2, x=300, y=40, role="motor"), pop("MN_E", 2, x=300, y=240, role="motor"), pop("IN", 2, x=160, y=140)]
    projs = [proj("IN", "MN_E", -160.0)]
    ins = [sensor("tone", "dc", ["MN_F", "MN_E"], 300.0, 1.4), sensor("ia_flexor", "dc", ["MN_F", "IN"], 300.0, 0.0)]
    outs = [motor("flexor", ["MN_F"]), motor("extensor", ["MN_E"])]
    return circuit("Reciprocal inhibition", pops, projs, ins, outs, "m_reciprocal", "docs/circuits/m_reciprocal.md")


@reg
def m_recurrent():
    # motoneurons drive a small inhibitory Renshaw pool that inhibits them back
    pops = [pop("MN", 4, x=200, y=40, role="motor"), pop("RC", 2, x=400, y=100)]
    projs = [proj("MN", "RC", 800.0), proj("RC", "MN", -800.0)]
    ins = [sensor("drive", "dc", ["MN"], 300.0, 1.5), sensor("kick", "dc", ["MN"], 300.0, 0.0)]
    outs = [motor("mn", ["MN"])]
    return circuit("Renshaw loop", pops, projs, ins, outs, "m_recurrent", "docs/circuits/m_recurrent.md")


@reg
def m_negative_feedback():
    # tone drives the effort cells; the level sensor drives inhibitory cells that inhibit them
    pops = [pop("EFF", 2, x=300, y=40, role="motor"), pop("INH", 3, x=160, y=160)]
    projs = [proj("INH", "EFF", -300.0)]
    ins = [sensor("tone", "dc", ["EFF"], 300.0, 2.0), sensor("level", "dc", ["INH"], 300.0, 1.5)]
    outs = [motor("effort", ["EFF"])]
    return circuit("Negative feedback loop", pops, projs, ins, outs, "m_negative_feedback", "docs/circuits/m_negative_feedback.md")


@reg
def m_feedforward():
    # the same feedback loop plus the cue straight onto the effort cells (sensor weight 450: a little more anticipation than the lab's 300)
    pops = [pop("EFF", 2, x=300, y=40, role="motor"), pop("INH", 3, x=160, y=160)]
    projs = [proj("INH", "EFF", -300.0)]
    ins = [sensor("tone", "dc", ["EFF"], 300.0, 2.0), sensor("level", "dc", ["INH"], 300.0, 1.5), sensor("cue", "dc", ["EFF"], 450.0, 0.4)]
    outs = [motor("effort", ["EFF"])]
    return circuit("Feed-forward plus feedback", pops, projs, ins, outs, "m_feedforward", "docs/circuits/m_feedforward.md")


@reg
def m_timed_bursts():
    # a chain of three groups, 120 ms between links
    pops = [pop("G1", 4, x=120, y=60, role="motor"), pop("G2", 4, x=300, y=60, role="motor"), pop("G3", 4, x=480, y=60, role="motor")]
    projs = [proj("G1", "G2", 300.0, 120.0), proj("G2", "G3", 300.0, 120.0)]
    ins = [sensor("go", "dc", ["G1"], 300.0, 0.0)]
    outs = [motor("b1", ["G1"]), motor("b2", ["G2"]), motor("b3", ["G3"])]
    return circuit("Three timed bursts", pops, projs, ins, outs, "m_timed_bursts", "docs/circuits/m_timed_bursts.md")


@reg
def m_rhythm():
    # one Izhikevich chattering cell (the bursting preset); the chemo drive is 5 pA per unit of level
    chat = {"a": 0.02, "b": 0.2, "c": -50.0, "d": 2.0}
    pops = [pop("PBC", 1, model="izhikevich", params=chat, x=300, y=100, role="motor")]
    ins = [sensor("chemo", "dc", ["PBC"], 5.0, 1.0)]
    outs = [motor("phrenic", ["PBC"])]
    return circuit("A burster tuned by CO2", pops, [], ins, outs, "m_rhythm", "docs/circuits/m_rhythm.md")


@reg
def m_wave():
    # tone on all segments; dist1 excites the segment above and, through an inhibitory interneuron, inhibits the one below; dist2 excites its upper neighbour
    pops = [pop("SEG0", 2, x=300, y=40, role="motor"), pop("SEG1", 2, x=300, y=160, role="motor"), pop("SEG2", 2, x=300, y=280, role="motor"),
            pop("IN", 2, x=160, y=220)]
    projs = [proj("IN", "SEG2", -160.0)]
    ins = [sensor("tone", "dc", ["SEG0", "SEG1", "SEG2"], 300.0, 1.4), sensor("dist1", "dc", ["SEG0", "IN"], 300.0, 0.0),
           sensor("dist2", "dc", ["SEG1"], 300.0, 0.0)]
    outs = [motor("seg0", ["SEG0"]), motor("seg1", ["SEG1"]), motor("seg2", ["SEG2"])]
    return circuit("Law of the intestine", pops, projs, ins, outs, "m_wave", "docs/circuits/m_wave.md")


@reg
def m_divergent():
    # the alarm drives a command group that contacts each effector with its own weight, and an inhibitory cell onto the gut
    pops = [pop("CMD", 2, x=120, y=120), pop("HEART", 1, x=320, y=40, role="motor"), pop("PUPIL", 1, x=320, y=120, role="motor"),
            pop("SWEAT", 1, x=320, y=200, role="motor"), pop("GUT", 1, x=320, y=280, role="motor"), pop("INH", 2, x=160, y=280)]
    projs = [proj("CMD", "HEART", 600.0), proj("CMD", "PUPIL", 280.0), proj("CMD", "SWEAT", 280.0), proj("INH", "GUT", -160.0)]
    ins = [sensor("alarm", "dc", ["CMD", "INH"], 300.0, 3.0), sensor("tone", "dc", ["GUT"], 300.0, 1.4)]
    outs = [motor("heart", ["HEART"]), motor("pupil", ["PUPIL"]), motor("sweat", ["SWEAT"]), motor("gut", ["GUT"])]
    return circuit("Divergent alarm", pops, projs, ins, outs, "m_divergent", "docs/circuits/m_divergent.md")


if __name__ == "__main__":
    only = sys.argv[1:] or list(CIRCUITS)
    for k in only:
        with open(os.path.join(OUT, k + ".json"), "w") as f:
            json.dump(CIRCUITS[k](), f, indent=1)
            f.write("\n")
        print("wrote", k + ".json")
