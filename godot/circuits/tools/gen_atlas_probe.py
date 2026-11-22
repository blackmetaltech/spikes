#!/usr/bin/env python3
"""circuits/atlas_probe_given.json (the given, locked circuit) and circuits/atlas_probe.json (its reference: the given circuit with the free values set).
Development-only level of the Atlas wing (docs/CIRCUIT-FORMAT.md "Locked parts and groups"). Run from anywhere: python3 circuits/tools/gen_atlas_probe.py [w_pm w_ni w_im ref_scale]

Four cell types, wiring as explicit edges (data): plus -> P -> M, minus -> N -> I -> M, M -> 'diff'. P, N are fixed (excitatory, scale 1); I is the free type (sign and scale).
Stored weights are magnitudes in pA (iaf_psc_alpha): effective weight = magnitude x scale x sign of the presynaptic type."""
import json, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
W_PM = float(sys.argv[1]) if len(sys.argv) > 1 else 330.0
W_NI = float(sys.argv[2]) if len(sys.argv) > 2 else 600.0
W_IM = float(sys.argv[3]) if len(sys.argv) > 3 else 150.0
REF_SCALE = float(sys.argv[4]) if len(sys.argv) > 4 else 0.98   # off the editor's 0.05 stepper grid: a bench the player tunes by hand never equals the reference

def edges(pairs, w, d):
    return [[a, b, w, d] for a, b in pairs]

def circuit(sign, scale, name):
    return {
        "schema_version": 1, "name": name, "resolution_ms": 0.1, "seed": 7,
        "populations": [
            {"id": "P", "model": "iaf_psc_alpha", "n": 3, "params": {}, "role": "neuron", "locked": True, "pos": [[120, 110], [120, 170], [120, 230]]},
            {"id": "N", "model": "iaf_psc_alpha", "n": 3, "params": {}, "role": "neuron", "locked": True, "pos": [[120, 330], [120, 390], [120, 450]]},
            {"id": "I", "model": "iaf_psc_alpha", "n": 2, "params": {}, "role": "neuron", "locked": True, "pos": [[320, 360], [320, 420]]},
            {"id": "M", "model": "iaf_psc_alpha", "n": 2, "params": {}, "role": "neuron", "locked": True, "pos": [[520, 200], [520, 260]]}],
        "groups": [
            {"id": "P", "label": "Plus relay", "pops": ["P"], "sign": 1, "scale": 1.0, "free": []},
            {"id": "N", "label": "Minus relay", "pops": ["N"], "sign": 1, "scale": 1.0, "free": []},
            {"id": "I", "label": "Interneuron", "pops": ["I"], "sign": sign, "scale": scale, "free": ["sign", "scale"], "ranges": {"scale": [0.25, 2.0]}}],
        "projections": [
            {"pre": "P", "post": "M", "locked": True, "synapse": {"synapse_model": "static_synapse"}, "edges": edges([(0, 0), (1, 0), (1, 1), (2, 1)], W_PM, 2.0)},
            {"pre": "N", "post": "I", "locked": True, "synapse": {"synapse_model": "static_synapse"}, "edges": edges([(0, 0), (1, 0), (1, 1), (2, 1)], W_NI, 2.0)},
            {"pre": "I", "post": "M", "locked": True, "synapse": {"synapse_model": "static_synapse"}, "edges": edges([(0, 0), (0, 1), (1, 0), (1, 1)], W_IM, 2.0)}],
        "inputs": [
            {"name": "plus", "kind": "dc", "targets": [{"pop": "P"}], "weight": 300.0, "value": 0.0},
            {"name": "minus", "kind": "dc", "targets": [{"pop": "N"}], "weight": 300.0, "value": 0.0}],
        "outputs": [{"name": "diff", "kind": "rate", "neurons": [{"pop": "M"}]}],
        "record": {"spikes": "all"},
        "meta": {"author": "spike", "task": "atlas_probe", "notes": "development-only: the Atlas locked-wiring probe"},
    }

json.dump(circuit(1, 0.5, "Atlas probe: the given circuit"), open(os.path.join(HERE, "..", "atlas_probe_given.json"), "w"), indent=1)
json.dump(circuit(-1, REF_SCALE, "Atlas probe: reference"), open(os.path.join(HERE, "..", "atlas_probe.json"), "w"), indent=1)
