#!/usr/bin/env python3
"""Fly compass from the measured wiring: data/connectome/hemibrain/compass.json -> a circuit JSON the `fly_compass` task can run.

FEASIBILITY PROBE (Atlas A1), not a level. Nothing in circuits/ or the levels is changed. See research/atlas-a.md for what it found.

What is data: the cells (148 neurons: EPG 46, PEN_a 20, PEN_b 22, PEG 18, Delta7 42), their order around the ring, and every connection among them with its synapse count (9,487).
What the data do not give, and this script has to assume (all are parameters):
  - the sign of each cell type:  EPG, PEN_a, PEN_b, PEG excitatory (cholinergic), Delta7 inhibitory (glutamatergic, acting through an inhibitory receptor);
    sources in research/atlas-a.md. Delta7 sign is a literature assumption, not in the table.
  - the weight of one synapse, one scale per presynaptic type (pA per synapse): weight = sign * synapse_count * scale[type]
  - the cell model and its constants (iaf_psc_alpha, tau_syn_ex / tau_syn_in), a background current I_e (a stand-in for everything the cells receive that the table does not hold)
  - how the task's input reaches the cells: the landmark (cue k) reaches the E-PG cells at ring position k; the two turn sensors reach the P-EN cells of one side each
    (the left bridge half pushes the bump towards higher ring positions: see "shift" in research/atlas-a.md). Gains are parameters.
  - the readout: 'heading' reads the 46 E-PG cells as a population vector, in ring order (the engine puts neuron k of 46 at 360 k / 46 degrees, while ring position r is at 22.5 r degrees; the two differ by up to about 10 degrees).

Usage:  python3 tools/connectome/compass_circuit.py out.json [--scale-EPG 0.3 ...]      (see DEFAULTS)
"""
import argparse, json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.normpath(os.path.join(HERE, "..", "..", "data", "connectome", "hemibrain", "compass.json"))
POP = {"EPG": "EPG", "PEN_a(PEN1)": "PENa", "PEN_b(PEN2)": "PENb", "PEG": "PEG", "Delta7": "D7"}
SIGN = {"EPG": 1, "PENa": 1, "PENb": 1, "PEG": 1, "D7": -1}
DEFAULTS = dict(scale_EPG=0.3, scale_PENa=0.3, scale_PENb=0.3, scale_PEG=0.3, scale_D7=0.3,   # pA per synapse
                I_e=300.0, tau_syn_ex=5.0, tau_syn_in=5.0, delay=2.0,
                cue_w=400.0, turn_w=3.0, turn_to="PEN",   # pA per spike (cue), pA per unit of turn sensor
                sign_D7=-1, sign_PEN=1, sign_PEG=1, sign_EPG=1)
WEDGES = 16


def load():
    return json.load(open(DATA))


def build(p=None, data=None):
    q = dict(DEFAULTS); q.update(p or {})
    d = data or load()
    ns = d["neurons"]
    ids = [POP[n["type"]] for n in ns]
    local, count = [], {}
    for pid in ids:
        local.append(count.get(pid, 0)); count[pid] = count.get(pid, 0) + 1
    order = ["EPG", "PENa", "PENb", "PEG", "D7"]
    sign = {"EPG": q["sign_EPG"], "PENa": q["sign_PEN"], "PENb": q["sign_PEN"], "PEG": q["sign_PEG"], "D7": q["sign_D7"]}
    pops = [{"id": pid, "model": "iaf_psc_alpha", "n": count[pid], "params": {"I_e": q["I_e"], "tau_syn_ex": q["tau_syn_ex"], "tau_syn_in": q["tau_syn_in"]}, "role": "neuron"} for pid in order]
    edges = {}
    for a, b, w in d["connections"]:
        pa, pb = ids[a], ids[b]
        edges.setdefault((pa, pb), []).append([local[a], local[b], round(sign[pa] * w * q["scale_" + pa], 4), q["delay"]])
    proj = [{"pre": a, "post": b, "edges": e, "synapse": {"synapse_model": "static_synapse"}} for (a, b), e in sorted(edges.items(), key=lambda kv: (order.index(kv[0][0]), order.index(kv[0][1])))]
    epg = [i for i, n in enumerate(ns) if n["type"] == "EPG"]
    inputs = []
    for k in range(WEDGES):
        idx = [local[i] for i in epg if ns[i]["ring"] == k]
        inputs.append({"name": "cue%d" % k, "kind": "poisson", "targets": [{"pop": "EPG", "idx": idx}], "weight": q["cue_w"], "value": 0.0})
    def side_targets(side):
        t = []
        for pid, ty in (("PENa", "PEN_a(PEN1)"), ("PENb", "PEN_b(PEN2)")):
            idx = [local[i] for i, n in enumerate(ns) if n["type"] == ty and n["side"] == side]
            t.append({"pop": pid, "idx": idx})
        return t
    # the left half of the bridge: PEN > EPG goes to higher ring positions (see research/atlas-a.md), so turn_right (bump towards higher positions) drives the left cells
    inputs.append({"name": "turn_left", "kind": "dc", "targets": side_targets("R"), "weight": q["turn_w"], "value": 0.0})
    inputs.append({"name": "turn_right", "kind": "dc", "targets": side_targets("L"), "weight": q["turn_w"], "value": 0.0})
    epg_local = [local[i] for i in epg]   # data order = ring order (sorted by ring, side, id)
    outputs = [{"name": "heading", "kind": "population_vector", "neurons": [{"pop": "EPG", "idx": epg_local}]}]
    return {"schema_version": 1, "name": "Fly compass from hemibrain v1.2 wiring (probe)", "resolution_ms": 0.1, "seed": 7, "populations": pops, "projections": proj,
            "inputs": inputs, "outputs": outputs, "record": {"spikes": "all"},
            "meta": {"author": "tools/connectome/compass_circuit.py", "task": "fly_compass", "notes": "feasibility probe, parameters: " + json.dumps(q, sort_keys=True)}}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    for k, v in DEFAULTS.items():
        ap.add_argument("--" + k.replace("_", "-"), type=type(v), default=v)
    a = ap.parse_args()
    c = build({k: getattr(a, k) for k in DEFAULTS})
    json.dump(c, open(a.out, "w"))
    print("wrote", a.out, sum(len(p["edges"]) for p in c["projections"]), "edges")


if __name__ == "__main__":
    main()
