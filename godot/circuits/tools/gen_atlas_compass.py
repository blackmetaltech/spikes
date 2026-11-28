#!/usr/bin/env python3
"""The circuits of the compass chapter of the Atlas wing (chapter a_compass; research/atlas-a.md 4c and 4d; engine/src/tasks/atlas_compass.h).
Run from anywhere:  python3 godot/circuits/tools/gen_atlas_compass.py

Writes, next to this script's circuits/ directory:
  a_signs_given.json   the given circuit: 148 cells of the fly compass (E-PG 46, P-EN a 20, P-EN b 22, P-EG 18, Delta7 42), 9,487 measured connections, every cell and projection locked,
                       the fitted gains baked into the stored weights; four cell-type groups: E-PG, P-EG and Delta7 with a FREE sign (given excitatory, which is wrong), P-EN with nothing free
  a_signs.json         its reference: E-PG and P-EG excitatory, Delta7 inhibitory
  a_places_given.json  = a_places.json: the same wiring with the reference signs, nothing to edit (a bet: the level runs the cues itself)
  a_gain_given.json    the same wiring with the reference signs fixed and ONE free number: the scale of the Delta7 group (given GAIN_GIVEN)
  a_gain.json          its reference (scale GAIN_REF)
  dx_a_compass.json    the patient of the diagnosis case: the same circuit, not locked, no groups, the outputs of one cell type (the answer) at weight 0

The model (what the circuits hold; "in this model", never "in the fly"): leaky integrate-and-fire cells (iaf_psc_alpha), weight = sign x synapse count x one gain per cell-type pair (the
fitted setting data/connectome/hemibrain/compass_setting.json, made by tools/connectome/compass_circuit.py), a background current per type, delay 2 ms. The 40 pA that the study's two turn
sensors gave the P-EN cells all the time is baked into their background current. The data are data/connectome/hemibrain/compass.json (hemibrain v1.2.1, Scheffer et al. 2020; CC BY 4.0).
The stored weight of a projection out of a group is a magnitude in pA; a group applies |w| x scale x sign at load. Sensors: cue0 to cue15 (Poisson, 300 Hz, weight 400 pA, on the E-PG
cells of heading k-1, k and k+1) and "clear" (a negative current on every cell, the level's own reset between two trials); motors: h0 to h15, the E-PG cells of heading k.
"""
import json, math, os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, ".."))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "connectome"))
import compass_circuit as cc

SETTING = json.load(open(os.path.join(ROOT, "data", "connectome", "hemibrain", "compass_setting.json")))["params"]
SOURCE = ("Adult fly hemibrain v1.2.1, Scheffer LK et al. 2020, eLife 9:e57443, doi:10.7554/eLife.57443, published under CC BY 4.0; extraction: tools/connectome/hemibrain.py; "
          "data/connectome/hemibrain/PROVENANCE.md; setting: data/connectome/hemibrain/compass_setting.json (research/atlas-a.md 4c)")
PEN_TONIC = 40.0       # pA: what the two turn sensors gave every P-EN cell (BASE 40 units x turn_w 1.0 pA); baked into I_e
CUE_HZ, CUE_W, CLEAR_W = 300.0, 400.0, -1000.0
GAIN_GIVEN, GAIN_REF, GAIN_RANGE = 0.35, 1.0, [0.1, 3.0]    # a_gain: the Delta7 scale (set from research/atlas-a.md 4d)
ANSWER_DX = "D7"       # the cell type whose outputs carry nothing in the patient of dx_a_compass (set from research/atlas-a.md 4d)
CUE_SEQUENCE = {}
LABEL = {"EPG": "E-PG cells (compass ring)", "PEN": "P-EN cells (shifters; their sign barely matters in this model, so it is fixed)", "PEG": "P-EG cells", "D7": "Delta7 cells (ring-wide)"}
GROUP_OF = {"EPG": "EPG", "PENa": "PEN", "PENb": "PEN", "PEG": "PEG", "D7": "D7"}
CIRCLE = {"D7": (215, 0), "EPG": (300, 0), "PEG": (130, 0), "PENa": (390, 0), "PENb": (450, 0)}   # radius, angle offset
CENTRE = (520, 520)


def positions(d, pid_of, local_of):
    """Editor positions: one ring per cell type, each cell at the angle of its heading (the Delta7 cells have none: evenly spread)."""
    pos = {}
    ns = d["neurons"]
    for pid in CIRCLE:
        idx = [i for i in range(len(ns)) if pid_of[i] == pid]
        if pid == "D7":
            ang = {i: 360.0 * k / len(idx) for k, i in enumerate(idx)}
        else:
            ang, seen = {}, {}
            for i in idx:
                h = ns[i]["heading"]
                k = seen.get(h, 0); seen[h] = k + 1
                ang[i] = 22.5 * h + 6.0 * k
        r = CIRCLE[pid][0]
        for i in idx:
            a = math.radians(ang[i] - 90.0)
            pos[i] = [round(CENTRE[0] + r * math.cos(a)), round(CENTRE[1] + r * math.sin(a))]
    return pos


def compass(name, signs=None, groups=None, locked=True, silent=(), gain=1.0):
    """signs: dict group -> +1/-1 (stored as the group's sign; weights stored as magnitudes); groups: {id: free list} or None for no groups (then weights keep their sign)."""
    c = cc.build(dict(SETTING))
    d = cc.load()
    ns = d["neurons"]
    ids = [cc.POP[n["type"]] for n in ns]
    local, count = [], {}
    for pid in ids:
        local.append(count.get(pid, 0)); count[pid] = count.get(pid, 0) + 1
    pos = positions(d, ids, local)
    first = {}
    for i, pid in enumerate(ids):
        first.setdefault(pid, []).append(i)
    for p in c["populations"]:
        p["role"] = "neuron"
        if locked:
            p["locked"] = True
        if p["id"] in ("PENa", "PENb"):
            p["params"]["I_e"] = round(p["params"]["I_e"] + PEN_TONIC, 4)
        p["pos"] = [pos[i] for i in first[p["id"]]]
    for pr in c["projections"]:
        if locked:
            pr["locked"] = True
        if groups is not None:
            for e in pr["edges"]:
                e[2] = abs(e[2])
        if pr["pre"] in silent:
            for e in pr["edges"]:
                e[2] = 0.0
    if groups is not None:
        c["groups"] = []
        for g, free in groups.items():
            gd = {"id": g, "label": LABEL[g], "pops": [k for k in ("EPG", "PENa", "PENb", "PEG", "D7") if GROUP_OF[k] == g], "sign": signs[g], "scale": gain if g == "D7" else 1.0, "free": list(free)}
            if "scale" in free:
                gd["ranges"] = {"scale": GAIN_RANGE}
            c["groups"].append(gd)
    cues = [i for i in c["inputs"] if i["name"].startswith("cue")]
    for i in cues:
        i["weight"] = CUE_W
        i["value"] = 0.0
    clear = {"name": "clear", "kind": "dc", "targets": [{"pop": k} for k in ("EPG", "PENa", "PENb", "PEG", "D7")], "weight": CLEAR_W, "value": 0.0}
    c["inputs"] = cues + [clear]
    epg = [i for i in range(len(ns)) if ids[i] == "EPG"]
    c["outputs"] = [{"name": "h%d" % k, "kind": "rate", "neurons": [{"pop": "EPG", "idx": [local[i] for i in epg if ns[i]["heading"] == k]}]} for k in range(16)]
    c["name"] = name
    c["seed"] = 7
    c["record"] = {"spikes": "all"}
    c["meta"] = {"author": "spike", "task": "a_compass", "source": SOURCE, "setting": SETTING,
                 "notes": "fly compass of the Atlas wing: leaky integrate-and-fire cells, weight = sign x synapse count x one gain per cell-type pair (the fitted setting), the 40 pA tonic input of "
                          "the P-EN cells in their background current; generated by circuits/tools/gen_atlas_compass.py"}
    return c


def put(name, c):
    text = json.dumps(c, indent=1)
    text = re.sub(r"\[\s*(-?[\d.eE+-]+(?:,\s*-?[\d.eE+-]+)*)\s*\]", lambda m: "[" + re.sub(r"\s+", " ", m.group(1)) + "]", text)   # one line per edge and per index list
    with open(os.path.join(OUT, name), "w") as f:
        f.write(text + "\n")


if __name__ == "__main__":
    wrong = {"EPG": 1, "PEN": 1, "PEG": 1, "D7": 1}
    right = {"EPG": 1, "PEN": 1, "PEG": 1, "D7": -1}
    free_signs = {"EPG": ["sign"], "PEN": [], "PEG": ["sign"], "D7": ["sign"]}
    put("a_signs_given.json", compass("Atlas: the given compass circuit of the fly", wrong, free_signs))
    put("a_signs.json", compass("Atlas: the compass with the signs that hold a bump (reference)", right, free_signs))
    places = compass("Atlas: the compass circuit of the fly, to count its resting places", locked=True)
    put("a_places_given.json", places)
    put("a_places.json", places)
    free_gain = {"EPG": [], "PEN": [], "PEG": [], "D7": ["scale"]}
    put("a_gain_given.json", compass("Atlas: the compass with one gain to set", right, free_gain, gain=GAIN_GIVEN))
    put("a_gain.json", compass("Atlas: the compass with the Delta7 gain that holds a bump (reference)", right, free_gain, gain=GAIN_REF))
    put("dx_a_compass.json", compass("Diagnosis: the compass that holds no bump", locked=False, silent=(ANSWER_DX,)))
    print("wrote a_signs(_given), a_places(_given), a_gain(_given), dx_a_compass in", OUT)
