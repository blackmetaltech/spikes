#!/usr/bin/env python3
"""circuits/m_tonic.json: cue > B (inhibitory) > A (bias I_e) > spikes. Run from anywhere: python3 circuits/tools/gen_tonic.py"""
import json, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
IE = float(sys.argv[1]) if len(sys.argv) > 1 else 400.0
WIN = float(sys.argv[2]) if len(sys.argv) > 2 else -1000.0
c = {
    "schema_version": 1, "name": "Tonic drive: a humming cell that a cue can silence", "resolution_ms": 0.1, "seed": 7,
    "populations": [
        {"id": "N1", "model": "iaf_psc_alpha", "n": 1, "params": {"I_e": IE}, "role": "neuron", "pos": [[420, 160]]},
        {"id": "N2", "model": "iaf_psc_alpha", "n": 1, "params": {}, "role": "neuron", "pos": [[260, 160]]}],
    "projections": [{"pre": "N2", "post": "N1", "rule": "all_to_all", "synapse": {"synapse_model": "static_synapse", "weight": WIN, "delay": 2.0}}],
    "inputs": [{"name": "cue", "kind": "dc", "targets": [{"pop": "N2"}], "weight": 300.0, "value": 2.0}],
    "outputs": [{"name": "spikes", "kind": "rate", "neurons": [{"pop": "N1"}]}],
    "record": {"spikes": "all"},
    "meta": {"author": "spike", "task": "m_tonic", "notes": "docs/circuits/m_tonic.md"},
}
json.dump(c, open(os.path.join(HERE, "..", "m_tonic.json"), "w"), indent=1)
