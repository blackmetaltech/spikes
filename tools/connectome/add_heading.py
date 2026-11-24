#!/usr/bin/env python3
"""One-off pass over the committed data/connectome/hemibrain/compass.json: adds `heading` (hemibrain.heading_index) to every cell that has a glomerulus, re-sorts the neurons
by (type, heading, side, id) as hemibrain.py now does, renumbers the connections, writes the file with hemibrain.dumps. Uses only fields already in the file (no cache).
Idempotent. `hemibrain.py --check` against the raw export has not been run on the result."""
import json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hemibrain as hb
path = os.path.join(hb.OUT, "compass.json")
d = json.load(open(path))
ns = d["neurons"]
for n in ns:
    if "side" in n: n["heading"] = hb.heading_index(n["side"], n["glomerulus"])
order = sorted(range(len(ns)), key=lambda i: (hb.COMPASS_TYPES.index(ns[i]["type"]), ns[i].get("heading", -1), ns[i].get("side", ""), ns[i]["id"]))
new = {old: k for k, old in enumerate(order)}
d["neurons"] = [ns[i] for i in order]
d["connections"] = sorted([[new[a], new[b], w] for a, b, w in d["connections"]])
d["format"] = "connections are [pre_index, post_index, synapse_count] into neurons; neurons sorted by (type, heading, side, id)"
out = {}
for k, v in d.items():
    out[k] = v
    if k == "ring_note":
        out[k] = v + "; it is NOT the ring of headings (see heading_note)"
        out["heading_note"] = "heading = 0..15 (22.5 deg each), fitted to the wiring: right glomerulus g -> 2g, left glomerulus g -> (3 - 2g), mod 16; the scan over 32 mappings is tools/connectome/compass_heading.py; not taken from a paper; mirror direction and zero are arbitrary; ring (position along the bridge) is kept"
open(path, "w").write(hb.dumps(out))
print("wrote", path, len(d["neurons"]), "neurons", len(d["connections"]), "connections")
