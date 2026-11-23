#!/usr/bin/env python3
"""Full measured touch-withdrawal wiring of C. elegans -> a circuit JSON for `spike-run` (plain circuit mode).

FEASIBILITY PROBE for iteration A2, not a level. Nothing in circuits/ or the levels is changed.

Data: data/connectome/worm/touch.json (Cook et al. 2019, 54 cells, 308 chemical and 161 electrical cell-pair connections with synapse counts; see PROVENANCE.md).
Nothing is pruned (unlike worm.py, which keeps 17 model cells and only the Chalfie pathway): every cell and every connection with at least `min_count` synapses is built.

Question asked of it: with weights proportional to synapse counts, do head touch (ALM, AVM) and tail touch (PLM) drive the A-type motor neurons (DA, VA = backward)
and the B-type (DB, VB = forward) differentially, under which sign assumptions, and which interneuron class abolishes each response when silenced?

ASSUMPTIONS (all are parameters in DEFAULTS, or fixed here and said so)
  - Cell model: iaf_psc_alpha (NEST defaults: rest/reset -70 mV, threshold -55 mV, C_m 250 pF, tau_m 10 ms, t_ref 2 ms; rheobase about 375 pA), identical for all cells.
    Unlike worm.py there is NO per-class tuning of V_th or t_ref: the probe asks whether the counts alone are enough. The real cells signal mostly with graded potentials.
  - Background current I_e (same for all cells; stands in for everything the data do not hold). It must stay below the rheobase so cells are silent at rest.
  - Weight of one chemical synapse = scale_chem pA (one global scale, as worm.py SCALE_PA = 25). weight of a cell-pair connection = sign * count * scale.
  - Electrical connections (scale_elec pA per contact, 0 = left out) can only be modelled as EXCITATORY links in both directions (spiking cells cannot be gap-coupled here, as worm.py says).
    Electrical self-couplings are dropped.
  - Signs of chemical outputs are NOT in the data. Sensory and motor neurons: excitatory. Command classes AVA, AVB, AVD, AVE, PVC: sign_<CLASS> (+1/-1), all outputs of the class.
    `cross`=1: chemical connections between the backward group {AVA, AVD, AVE} and the forward group {AVB, PVC} (both directions) are inhibitory whatever the class signs say;
    connections inside a group keep the class sign. Connections from sensors or motors keep their (excitatory) sign even if they end on a command cell.
  - One synaptic delay for all (`delay`). tau_syn_ex and tau_syn_in global.
  - Touch: each sensor of the touched end gets its own Poisson train (rate touch_hz, default 150 Hz as in the worm_touch task) for touch_ms (400 ms) from touch_t0 (500 ms),
    delivered by a spike_generator (explicit spike times, generated here from `touch_seed`), each spike weight touch_w pA (1000 pA: one spike fires a sensor, as in worm.py).
    head = ALML, ALMR, AVM; tail = PLML, PLMR.
  - `silence`: list of classes (ALM, AVM, PLM, AVA, AVB, AVD, AVE, PVC, or DA, VA, DB, VB) whose cells are removed from the circuit; connections to and from them vanish.

LAYOUT. One population per cell class (ALM, AVM, PLM, AVA, AVB, AVD, AVE, PVC, DA, VA, DB, VB; left and right cells are members of the class population), connections as explicit `edges`
([pre local index, post local index, weight, delay]), spikes of all neurons recorded. meta["cells"] lists the cell names in circuit-level neuron order (the order of the populations,
within a population the order of touch.json), so a spike's `neuron` index maps to a cell name.

Usage:  python3 tools/connectome/worm_circuit.py out.json [head|tail] [key=value ...]
"""
import json, os, random, sys

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.normpath(os.path.join(HERE, "..", "..", "data", "connectome", "worm", "touch.json"))
ORDER = ["ALM", "AVM", "PLM", "AVA", "AVB", "AVD", "AVE", "PVC", "DA", "VA", "DB", "VB"]
COMMAND = ["AVA", "AVB", "AVD", "AVE", "PVC"]
BACK, FWD = {"AVA", "AVD", "AVE"}, {"AVB", "PVC"}
A_CLASSES, B_CLASSES = ("DA", "VA"), ("DB", "VB")
HEAD, TAIL = ("ALML", "ALMR", "AVM"), ("PLML", "PLMR")
DEFAULTS = dict(scale_chem=25.0, scale_elec=0.0,
                sign_AVA=1, sign_AVB=1, sign_AVD=1, sign_AVE=1, sign_PVC=1, cross=0,
                I_e=250.0, tau_syn_ex=5.0, tau_syn_in=5.0, delay=2.0, min_count=1,
                silence=[],
                touch=None,            # None, "head" or "tail"
                touch_seed=1, touch_hz=150.0, touch_ms=400.0, touch_t0=500.0, touch_w=1000.0)
RES = 0.1


def load():
    return json.load(open(DATA))


def build(params=None, data=None):
    q = dict(DEFAULTS); q.update(params or {})
    d = data or load()
    ns = d["neurons"]
    sil = set(q["silence"])
    keep = [i for i, n in enumerate(ns) if n["class"] not in sil]
    cls = {i: ns[i]["class"] for i in keep}
    local, count = {}, {}
    for i in keep:
        local[i] = count.get(cls[i], 0); count[cls[i]] = local[i] + 1
    order = [c for c in ORDER if c in count]
    offset, o = {}, 0
    for c in order:
        offset[c] = o; o += count[c]
    cells = [ns[i]["name"] for c in order for i in keep if cls[i] == c]
    pops = [{"id": c, "model": "iaf_psc_alpha", "n": count[c], "role": "neuron",
             "params": {"I_e": q["I_e"], "tau_syn_ex": q["tau_syn_ex"], "tau_syn_in": q["tau_syn_in"]}} for c in order]
    edges = {}

    def add(a, b, w):
        edges.setdefault((cls[a], cls[b]), []).append([local[a], local[b], round(w, 4), q["delay"]])

    for a, b, n, kind in d["connections"]:
        if a not in cls or b not in cls or n < q["min_count"]:
            continue
        if kind == "electrical":
            if q["scale_elec"] > 0 and a != b:
                add(a, b, n * q["scale_elec"]); add(b, a, n * q["scale_elec"])
            continue
        pa, pb = cls[a], cls[b]
        s = q["sign_" + pa] if pa in COMMAND else 1
        if q["cross"] and pa in COMMAND and pb in COMMAND and ((pa in BACK and pb in FWD) or (pa in FWD and pb in BACK)):
            s = -1
        add(a, b, s * n * q["scale_chem"])
    proj = [{"pre": a, "post": b, "edges": e, "synapse": {"synapse_model": "static_synapse"}}
            for (a, b), e in sorted(edges.items(), key=lambda kv: (ORDER.index(kv[0][0]), ORDER.index(kv[0][1])))]
    inputs = []
    if q["touch"]:
        rnd = random.Random(q["touch_seed"])
        for i in keep:
            if ns[i]["name"] in (HEAD if q["touch"] == "head" else TAIL):
                t, ks = 0.0, []
                while True:
                    t += rnd.expovariate(q["touch_hz"] / 1000.0)
                    if t >= q["touch_ms"]: break
                    k = int(round((q["touch_t0"] + t) / RES))
                    if ks and k <= ks[-1]: k = ks[-1] + 1   # strictly ascending: the engine refuses t < previous after snapping, so equal times (rounded to 0.1 ms) are nudged one step on
                    ks.append(k)
                ts = [round(k * RES, 1) for k in ks]
                inputs.append({"name": "touch_" + ns[i]["name"], "kind": "spike_generator", "spike_times": ts,
                               "targets": [{"pop": cls[i], "idx": [local[i]]}], "weight": q["touch_w"]})
    return {"schema_version": 1, "name": "C. elegans touch wiring, full measured (probe)", "resolution_ms": RES, "seed": 7,
            "populations": pops, "projections": proj, "inputs": inputs, "outputs": [], "record": {"spikes": "all"},
            "meta": {"author": "tools/connectome/worm_circuit.py", "notes": "feasibility probe A2, parameters: " + json.dumps(q, sort_keys=True), "cells": cells}}


if __name__ == "__main__":
    out = sys.argv[1]; p = {}
    for kv in sys.argv[2:]:
        if "=" in kv:
            k, v = kv.split("="); p[k] = json.loads(v) if k == "silence" else type(DEFAULTS[k])(v)
        else:
            p["touch"] = kv
    c = build(p)
    json.dump(c, open(out, "w"))
    print("wrote", out, len(c["meta"]["cells"]), "cells", sum(len(x["edges"]) for x in c["projections"]), "edges")
