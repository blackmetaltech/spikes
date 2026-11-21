#!/usr/bin/env python3
"""Adds meta.dx_types to the diagnosis circuits circuits/dx_*.json: for every projection (file order) "E" or "I", the type it has in the HEALTHY reference circuit.
The dead connection is kept in the patient file at weight 0, and the game must not let that weight give it away (the lab draws and describes a patient's axon by this type
only, never by weight; godot/game/circuits.gd proj_weight_shown). The generators gen_boss_dx.py / gen_bestiary_a.py / gen_bestiary_b.py call annotate();
running this file re-annotates every dx circuit from its reference:   python3 circuits/tools/dx_types.py"""
import json, os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, ".."))

REFERENCE = {
    "dx_clonus": "renshaw.json", "dx_vagal": "heart_autonomic.json", "dx_orthostatic": "baroreflex.json", "dx_apnoea": "breathing.json", "dx_horner": "pupil.json",
    "dx_achalasia": "peristalsis.json", "dx_worm": "worm_touch.json", "dx_leech": "leech_heart.json", "dx_aplysia": "aplysia_gill.json", "dx_compass": "fly_compass.json",
    "dx_mb_apl": "fly_mb.json", "dx_mb_dan": "fly_mb.json", "dx_crab": "crab_pyloric_b.json",
}


def weight(p):
    if p.get("edges"):
        return p["edges"][0][2]
    w = p["synapse"]["weight"]
    return w if isinstance(w, (int, float)) else 0.0


def annotate(patient, ref):
    assert len(patient["projections"]) == len(ref["projections"])
    patient.setdefault("meta", {})["dx_types"] = ["I" if weight(p) < 0 else "E" for p in ref["projections"]]
    return patient


def main():
    for name, ref in REFERENCE.items():
        path = os.path.join(OUT, name + ".json")
        c = annotate(json.load(open(path)), json.load(open(os.path.join(OUT, ref))))
        with open(path, "w") as f:
            json.dump(c, f, indent=1)
            f.write("\n")
        print("annotated", name, "from", ref)


if __name__ == "__main__":
    main()
