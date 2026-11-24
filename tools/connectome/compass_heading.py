#!/usr/bin/env python3
"""Which coordinate makes the measured compass wiring a ring? Reads data/connectome/hemibrain/compass.json (pure Python, no simulation).

  python3 tools/connectome/compass_heading.py            prints (1) the scan, (2) the turn shifts

(1) A mapping gives every E-PG, P-EN and PEG cell a heading index h in 0..15 from its bridge glomerulus (side, number g):  h = 2g (mod 16) for a right glomerulus,
    h = (s * 2g + o) mod 16 for a left one, s in {+1, -1}, o in 0..15: 32 mappings. Score of a class of connections = synapse-weighted mean of cos(22.5 deg * (h_post - h_pre)):
    1 if every synapse joined cells of the same heading, about 0 if headings were unrelated. Classes: E-PG > E-PG, E-PG <> PEG (both directions), E-PG > P-EN (both types).
    Rule: the mapping with the highest E-PG > E-PG plus E-PG <> PEG. The current `ring` field (the glomerulus position along the bridge) is scored the same way for comparison.
    Delta7 cells have two glomeruli and no heading of their own; they are left out.
(2) Turn shift of each P-EN group (type, side of the bridge): for each cell, the circular mean (synapse-weighted) heading of its E-PG inputs and of its E-PG targets;
    shift = target - input, in headings (1 heading = 22.5 deg), then the mean over the cells of the group (circular mean per cell, linear mean of the shifts over cells).
"""
import json, math, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.normpath(os.path.join(HERE, "..", "..", "data", "connectome", "hemibrain", "compass.json"))
EPG, PENA, PENB, PEG = "EPG", "PEN_a(PEN1)", "PEN_b(PEN2)", "PEG"


def mapping(s, o):
    return lambda side, g: (2 * g) % 16 if side == "R" else (s * 2 * g + o) % 16


def scores(d, hf):
    ns = d["neurons"]
    h = [hf(n) if "side" in n else None for n in ns]
    acc = {"EPG>EPG": [0.0, 0], "EPG<>PEG": [0.0, 0], "EPG>PEN": [0.0, 0]}
    for a, b, w in d["connections"]:
        ta, tb = ns[a]["type"], ns[b]["type"]
        k = "EPG>EPG" if (ta, tb) == (EPG, EPG) else "EPG<>PEG" if {ta, tb} == {EPG, PEG} else "EPG>PEN" if ta == EPG and tb in (PENA, PENB) else None
        if k:
            acc[k][0] += w * math.cos(math.radians(22.5 * (h[b] - h[a]))); acc[k][1] += w
    return {k: v[0] / v[1] for k, v in acc.items()}


def scan(d):
    rows = []
    print("%-3s %-3s %8s %9s %8s %9s" % ("s", "o", "EPG>EPG", "EPG<>PEG", "EPG>PEN", "rule"))
    for s in (1, -1):
        for o in range(16):
            sc = scores(d, (lambda hf: (lambda n: hf(n["side"], n["glomerulus"])))(mapping(s, o)))
            rows.append((sc["EPG>EPG"] + sc["EPG<>PEG"], s, o, sc))
            print("%+d  %-3d %8.3f %9.3f %8.3f %9.3f" % (s, o, sc["EPG>EPG"], sc["EPG<>PEG"], sc["EPG>PEN"], rows[-1][0]))
    sc = scores(d, lambda n: n["ring"])
    print("current `ring` (position along the bridge): EPG>EPG %.3f  EPG<>PEG %.3f  EPG>PEN %.3f  rule %.3f" % (sc["EPG>EPG"], sc["EPG<>PEG"], sc["EPG>PEN"], sc["EPG>EPG"] + sc["EPG<>PEG"]))
    rows.sort(key=lambda r: -r[0])
    print("best by the rule: s=%+d o=%d (%.3f); next: s=%+d o=%d (%.3f); best with s=+1: o=%d (%.3f)" % (rows[0][1], rows[0][2], rows[0][0], rows[1][1], rows[1][2], rows[1][0], *[(r[2], r[0]) for r in rows if r[1] == 1][0]))
    return rows[0][1], rows[0][2]


def cmean(items):
    x = sum(w * math.cos(math.radians(22.5 * h)) for h, w in items); y = sum(w * math.sin(math.radians(22.5 * h)) for h, w in items)
    return math.degrees(math.atan2(y, x)) / 22.5


def turn_shifts(d, hf):
    ns = d["neurons"]
    h = [hf(n) if "side" in n else None for n in ns]
    inp, out = {}, {}
    for a, b, w in d["connections"]:
        if ns[a]["type"] == EPG and ns[b]["type"] in (PENA, PENB): inp.setdefault(b, []).append((h[a], w))
        if ns[a]["type"] in (PENA, PENB) and ns[b]["type"] == EPG: out.setdefault(a, []).append((h[b], w))
    groups = {}
    for i in inp:
        if i in out:
            sh = (cmean(out[i]) - cmean(inp[i]) + 8) % 16 - 8
            groups.setdefault((ns[i]["type"], ns[i]["side"]), []).append(sh)
    print("%-12s %-5s %5s %12s %10s" % ("type", "side", "cells", "mean shift", "range"))
    for k in sorted(groups):
        v = groups[k]
        print("%-12s %-5s %5d %+9.2f hd %+5.1f..%+.1f" % (k[0], k[1], len(v), sum(v) / len(v), min(v), max(v)))
    return {k: sum(v) / len(v) for k, v in groups.items()}


if __name__ == "__main__":
    d = json.load(open(DATA))
    s, o = scan(d)
    print()
    print("turn shifts under the chosen mapping (s=%+d, o=%d); a positive shift moves the bump to higher heading indices" % (s, o))
    hf = mapping(s, o)
    turn_shifts(d, lambda n: hf(n["side"], n["glomerulus"]))
