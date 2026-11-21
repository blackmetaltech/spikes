#!/usr/bin/env python3
"""Render the lab's specimen layers for the Bestiary wing into godot/assets/specimens (committed output; rerun after editing a
drawing in plates.py).

usage: python3 tools/art/specimens.py [--only id,id]

The lab draws each Bestiary level's body view as a living specimen (godot/ui/specimens.gd): a still engraving of the animal,
cut from the same drawing as its campaign-map plate, with the parts the simulation moves (heart tubes, gill, the STG, the
Kenyon cells, the dopamine and output cells) drawn live on top. This script runs the plate drawings of tools/art/plates.py in
specimen mode (plates.SPEC: no lettering, no insets, no animated parts; each drawing records the geometry of its live parts in
plates.ANCH), with the line weights scaled up for the size the lab shows the animal at, crops the animal and downsamples it.

Writes godot/assets/specimens/<id>.png (RGBA, transparent paper) and specimens.json: per layer the PNG, its pixel size, the
crop in plate px [x, y, w, h], and the anchors in plate px (a point p of the plate sits at (p - crop.xy) / crop.wh of the image).
Deterministic: same inputs, same pixels."""
import argparse
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ink  # noqa: E402
import plates  # noqa: E402

OUT = ink.ROOT / "godot" / "assets" / "specimens"

# id: (drawing, crop in plate px [x, y, w, h], the width the lab shows the crop at (logical px at 1080p), output width in px).
# The line-weight factor makes a logical px of the drawing one px at that display width, as on the map plates (the map shows
# the 2000 px plate at about 1236 px), so the engraving keeps the weight of the procedural body plates at the lab's size.
LAYERS = {
    "worm": (plates.worm, (130, 538, 1730, 148), 470, 1100),
    "leech": (plates.leech, (180, 430, 1660, 530), 430, 900),
    "aplysia": (plates.aplysia, (160, 395, 1780, 560), 440, 960),
    "crab": (plates.crab, (180, 230, 1640, 980), 300, 680),
    "fly_mb": (plates.fly_mb, (990, 300, 520, 720), 210, 460),
    "fly": (None, (170, 250, 660, 660), 90, 240),
}


def fly_plate():
    pl = ink.Plate()
    plates.fly_dorsal(pl, (500, 520), 560)
    return pl


def render(lid):
    fn, crop, disp, out_w = LAYERS[lid]
    x, y, w, h = crop
    s0 = ink.S
    ink.S = s0 * (w / disp) / (ink.W / 1236.0) * 1.0
    plates.SPEC = True
    k = ink.S / s0
    st, sd = ink.Plate.stipple, ink.Plate.stipple_disc
    # stipple grains grow only with the square root of the line factor: at the full factor the granular skins read as blotches
    ink.Plate.stipple = lambda self, poly, n, col, a=0.4, dot=1.4, seed=0: st(self, poly, n, col, a, dot / k ** 0.5, seed)
    ink.Plate.stipple_disc = lambda self, c, r, n, col, a=0.4, dot=1.4, seed=0, bias=1.0: sd(self, c, r, n, col, a, dot / k ** 0.5, seed, bias)
    try:
        pl = fn() if fn is not None else fly_plate()
        pl.labels = []
        im = pl.image()
    finally:
        plates.SPEC = False
        ink.S = s0
        ink.Plate.stipple, ink.Plate.stipple_disc = st, sd
    im = im.crop((x, y, x + w, y + h))
    out_h = round(out_w * h / w)
    im = im.resize((out_w, out_h), Image.LANCZOS)
    return im, plates.ANCH.get(lid, {})


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", default="")
    a = ap.parse_args()
    ids = [i for i in a.only.split(",") if i] or list(LAYERS)
    OUT.mkdir(parents=True, exist_ok=True)
    man_path = OUT / "specimens.json"
    man = json.loads(man_path.read_text()) if man_path.exists() else {}
    for lid in ids:
        im, anch = render(lid)
        im.save(OUT / f"{lid}.png", optimize=True)
        rnd = lambda v: [rnd(q) for q in v] if isinstance(v, (list, tuple)) else ({k: rnd(q) for k, q in v.items()} if isinstance(v, dict) else round(float(v), 2))
        man[lid] = {"file": f"res://assets/specimens/{lid}.png", "size": list(im.size), "crop": list(LAYERS[lid][1]), "anchors": rnd(anch)}
        print("wrote", OUT / f"{lid}.png", im.size)
    man = {k: man[k] for k in sorted(man)}
    man_path.write_text(json.dumps(man, separators=(",", ":")) + "\n")


if __name__ == "__main__":
    main()
