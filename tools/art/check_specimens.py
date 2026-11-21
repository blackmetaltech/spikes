#!/usr/bin/env python3
"""ctest art_specimens: the lab's specimen layers stay in step with the drawings they are cut from.

1. The map plates are untouched by specimen mode: rendering a plate after a specimen render gives the committed PNG, pixel for pixel.
2. Every layer the lab draws (godot/ui/specimens.gd) exists, is the size specimens.json says, and has the anchors of its live parts.
3. specimens.py is deterministic: a fresh render of each layer equals the committed one.
Exits 77 (skipped) when pycairo is missing."""
import json
import sys
from pathlib import Path

try:
    import numpy as np
    from PIL import Image
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import plates  # noqa: E402
    import specimens  # noqa: E402
except ImportError as e:   # no pycairo / Pillow / numpy here: nothing to check
    print("skip:", e)
    sys.exit(77)

ROOT = Path(__file__).resolve().parents[2]
NEED = {"worm": ["x0", "x1", "prof", "cells"], "leech": ["tubes", "bulges", "hn", "hn_r"], "aplysia": ["gill", "bran", "siph_n", "siphon_tip", "abd", "tail"],
        "crab": ["stg", "dvn"], "fly_mb": ["kc_c", "kc_r", "vl", "hl", "mbon", "ppl1", "ppl1_path", "pam", "pam_paths"], "fly": []}
bad = []
man = json.loads((ROOT / "godot/assets/specimens/specimens.json").read_text())
for lid, keys in NEED.items():
    m = man.get(lid)
    if m is None:
        bad.append(f"{lid}: not in specimens.json"); continue
    png = ROOT / "godot/assets/specimens" / f"{lid}.png"
    if not png.exists():
        bad.append(f"{lid}: {png} missing"); continue
    im = Image.open(png)
    if list(im.size) != m["size"]:
        bad.append(f"{lid}: size {im.size} != {m['size']}")
    for k in keys:
        if k not in m["anchors"]:
            bad.append(f"{lid}: anchor {k} missing")
    fresh, _ = specimens.render(lid)
    if not (np.asarray(fresh) == np.asarray(im.convert("RGBA"))).all():
        bad.append(f"{lid}: a fresh render differs from the committed layer (rerun tools/art/specimens.py)")
for pid in ["worm", "leech", "aplysia", "crab", "fly_mb"]:
    a = np.asarray(plates.PLATES[pid]().image())
    b = np.asarray(Image.open(ROOT / "godot/assets/plates" / f"{pid}.png").convert("RGBA"))
    if a.shape != b.shape or not (a == b).all():
        bad.append(f"plate {pid}: differs from the committed map plate after a specimen render")
for b in bad:
    print("FAIL", b)
print("art_specimens:", "FAIL" if bad else "OK")
sys.exit(1 if bad else 0)
