#!/usr/bin/env python3
"""Fly compass from the measured wiring: data/connectome/hemibrain/compass.json -> a circuit JSON the `fly_compass` task can run.

FEASIBILITY PROBE (Atlas A1), not a level. Nothing in circuits/ or the levels is changed. See research/atlas-a.md for what it found.

What is data: the cells (148 neurons: EPG 46, PEN_a 20, PEN_b 22, PEG 18, Delta7 42), their order around the ring, and every connection among them with its synapse count (9,487).
What the data do not give, and this script has to assume (all are parameters):
  - the sign of each cell type:  EPG, PEN_a, PEN_b, PEG excitatory (cholinergic), Delta7 inhibitory (glutamatergic, acting through an inhibitory receptor);
    sources in research/atlas-a.md. Delta7 sign is a literature assumption, not in the table.
  - the weight of one synapse, one scale per presynaptic type (pA per synapse): weight = sign * synapse_count * scale[type]
    (optional, used in the second search: a scale for one type pair, parameter w_<pre>_<post>, for example w_EPG_D7; and I_e_<pop> for one population's background current)
  - norm (0 default): homeostatic normalisation of the synapse counts before the scales are applied, see normalise() for exactly what is rescaled
  - the cell model and its constants (iaf_psc_alpha, tau_syn_ex / tau_syn_in), a background current I_e (a stand-in for everything the cells receive that the table does not hold)
  - how the task's input reaches the cells: the landmark (cue k) reaches the E-PG cells with heading k-1, k, k+1 (the `heading` field of compass.json, fitted to the wiring: see research/atlas-a.md 4c; both bridge halves, they interleave); the two turn sensors reach the P-EN cells of one side each
    (measured by tools/connectome/compass_heading.py: the P-EN cells of the left half of the bridge move the bump towards higher heading, +0.63 headings on average, the right half towards lower, -0.6). Gains are parameters.
  - the readout: 'heading' reads the 46 E-PG cells as a population vector, in heading order (the engine puts neuron k of 46 at 360 k / 46 degrees, while heading h is at 22.5 h degrees; the two differ by up to about 10 degrees).

Usage:  python3 tools/connectome/compass_circuit.py out.json [--scale-EPG 0.3 ...]      (see DEFAULTS)
"""
import argparse, json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.normpath(os.path.join(HERE, "..", "..", "data", "connectome", "hemibrain", "compass.json"))
POP = {"EPG": "EPG", "PEN_a(PEN1)": "PENa", "PEN_b(PEN2)": "PENb", "PEG": "PEG", "Delta7": "D7"}
SIGN = {"EPG": 1, "PENa": 1, "PENb": 1, "PEG": 1, "D7": -1}
DEFAULTS = dict(scale_EPG=0.3, scale_PENa=0.3, scale_PENb=0.3, scale_PEG=0.3, scale_D7=0.3,   # pA per synapse
                I_e=300.0, tau_syn_ex=5.0, tau_syn_in=5.0, delay=2.0,
                cue_w=400.0, cue_width=1,   # cue_width: the cue for heading k drives the E-PG cells with heading within +-cue_width of k (both bridge halves)
                 turn_w=3.0, turn_to="PEN",   # pA per spike (cue), pA per unit of turn sensor
                sign_D7=-1, sign_PEN=1, sign_PEG=1, sign_EPG=1, sign_PENa=0, sign_PENb=0,   # sign_PENa / sign_PENb: 0 = use sign_PEN, else +1 or -1 for that P-EN type alone
                norm=0)   # 0 none, 1 incoming totals equalised, 2 outgoing totals equalised, 3 both (see normalise)
WEDGES = 16


def load():
    return json.load(open(DATA))


def normalise(conns, ids, mode, iters=30):
    """Homeostatic normalisation of the synapse counts (a parameter-free assumption; the table does not say it). Returns [pre, post, count] rows, counts now real numbers.
    Done separately for each ordered pair of cell types (A -> B), on the counts of that pair only; who connects to whom (the set of rows) is unchanged.
      mode 1: every cell of type B that receives anything from type A has its incoming A-synapses multiplied by one factor so that its total equals the mean of that total over the cells of B that receive anything from A.
      mode 2: every cell of type A that sends anything to type B has its outgoing B-synapses multiplied by one factor so that its total equals the mean of that total over the cells of A that send anything to B.
      mode 3: modes 1 and 2 alternated `iters` times (iterative proportional fitting); the outgoing totals are then equal and the incoming ones nearly so (on the hemibrain table the largest incoming max/min ratio is 6.0 after 30 rounds, against 29.0 without; the pattern cannot satisfy both exactly).
    The sum over the whole pair is kept (the target is the mean over cells with a non-zero total, so a cell with no input of that kind stays without)."""
    if not mode: return [list(c) for c in conns]
    rows = {}
    for a, b, w in conns: rows.setdefault((ids[a], ids[b]), []).append([a, b, float(w)])
    for rs in rows.values():
        for step in range(iters if mode == 3 else 1):
            for axis in ((1,) if mode == 1 else (0,) if mode == 2 else (1, 0)):
                tot = {}
                for r in rs: tot[r[axis]] = tot.get(r[axis], 0.0) + r[2]
                mean = sum(tot.values()) / len(tot)
                for r in rs: r[2] *= mean / tot[r[axis]]
    return [r for rs in rows.values() for r in rs]


def build(p=None, data=None):
    q = dict(DEFAULTS); q.update(p or {})
    d = data or load()
    ns = d["neurons"]
    ids = [POP[n["type"]] for n in ns]
    local, count = [], {}
    for pid in ids:
        local.append(count.get(pid, 0)); count[pid] = count.get(pid, 0) + 1
    order = ["EPG", "PENa", "PENb", "PEG", "D7"]
    sign = {"EPG": q["sign_EPG"], "PENa": q["sign_PENa"] or q["sign_PEN"], "PENb": q["sign_PENb"] or q["sign_PEN"], "PEG": q["sign_PEG"], "D7": q["sign_D7"]}
    pops = [{"id": pid, "model": "iaf_psc_alpha", "n": count[pid], "params": {"I_e": q.get("I_e_" + pid, q["I_e"]), "tau_syn_ex": q["tau_syn_ex"], "tau_syn_in": q["tau_syn_in"]}, "role": "neuron"} for pid in order]
    edges = {}
    conns = normalise(d["connections"], ids, int(q["norm"]))
    for a, b, w in conns:
        pa, pb = ids[a], ids[b]
        edges.setdefault((pa, pb), []).append([local[a], local[b], round(sign[pa] * w * q.get("w_%s_%s" % (pa, pb), q["scale_" + pa]), 4), q["delay"]])
    proj = [{"pre": a, "post": b, "edges": e, "synapse": {"synapse_model": "static_synapse"}} for (a, b), e in sorted(edges.items(), key=lambda kv: (order.index(kv[0][0]), order.index(kv[0][1])))]
    epg = [i for i, n in enumerate(ns) if n["type"] == "EPG"]
    inputs = []
    for k in range(WEDGES):
        idx = [local[i] for i in epg if abs((ns[i]["heading"] - k + 8) % 16 - 8) <= q["cue_width"]]
        inputs.append({"name": "cue%d" % k, "kind": "poisson", "targets": [{"pop": "EPG", "idx": idx}], "weight": q["cue_w"], "value": 0.0})
    def side_targets(side):
        t = []
        for pid, ty in (("PENa", "PEN_a(PEN1)"), ("PENb", "PEN_b(PEN2)")):
            idx = [local[i] for i, n in enumerate(ns) if n["type"] == ty and n["side"] == side]
            t.append({"pop": pid, "idx": idx})
        return t
    # the left half of the bridge: PEN > EPG goes to higher headings (compass_heading.py), so turn_right (bump towards higher positions) drives the left cells
    inputs.append({"name": "turn_left", "kind": "dc", "targets": side_targets("R"), "weight": q["turn_w"], "value": 0.0})
    inputs.append({"name": "turn_right", "kind": "dc", "targets": side_targets("L"), "weight": q["turn_w"], "value": 0.0})
    epg_local = [local[i] for i in epg]   # data order = heading order (sorted by heading, side, id)
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
