#!/usr/bin/env python3
"""Render the campaign-map plates of the Bestiary and Theory wings into godot/assets/plates (committed output; rerun only
when editing the drawings).

usage: python3 tools/art/plates.py [--only id,id] [--no-write] [--contact PNG [--body PNG,PNG]]
(--contact renders a review sheet: each plate on the map's sheet and frame with mock pins, beside crops of body-plate screenshots.)

Writes godot/assets/plates/<chapter id>.png (2000 x 1250 RGBA, transparent paper: the map draws its own sheet and frame
under it) and plates.json (per plate: file, size, the pin positions the drawing was composed around, and keep-out rects for
the label plates). Needs pycairo, Pillow and numpy. Deterministic: same inputs, same pixels.

The drawings follow docs/IDENTITY.md: India ink and Golgi stain on paper, sienna for excitation, Prussian blue for
inhibition, verdigris for sensory, madder plum for motor, gamboge for modulators, lettering in the game's Fraunces italic
on translucent paper slips, line weights as in godot/ui/anatomy.gd. The image covers the whole map canvas, so a pin at
(fx, fy) of the canvas sits at (fx * 2000, fy * 1250) here; the composition leaves those spots (Campaign.PLANNED) quiet."""
import argparse
import json
import math
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ink import (BODY, BONE, EXC, EXC_INK, FISH, GAMBOGE, GRAY, GUT, H, INH, INK, MOTOR, NUC, ROOT, S, SENSOR,  # noqa: E402
                 SHEET, SOFT, TISSUE, W, WALL, WORMFILL, Plate, Spine, catmull, ellipse, hexc, lw, smoothstep)

OUT = ROOT / "godot" / "assets" / "plates"

# The three spine levels of every wing chapter sit on a gentle arc (Campaign.PLANNED in godot/game/campaign.gd).
PINS = [(0.2, 0.62), (0.5, 0.36), (0.8, 0.62)]
LEVELS = {
    "worm": ["w_latch", "w_switch", "worm_touch"], "leech": ["l_burst", "l_rebound", "leech_heart"],
    "aplysia": ["a_depress", "a_facilitate", "aplysia_gill"], "fly_compass": ["c_ring", "c_shift", "fly_compass"],
    "fly_mb": ["k_sparse", "k_depress", "fly_mb"], "crab": ["s_pacemaker", "s_phase", "crab_pyloric"],
    "dynamics": ["d_class", "d_phasic", "d_switch"], "rates": ["r_oscillate", "r_choose", "r_meanfield"],
    "memory": ["h_complete", "h_two", "h_learn"], "tuning": ["t_tuned", "t_sharpen", "t_decode"]}


def pin_px(i):
    return np.array([PINS[i][0] * W, PINS[i][1] * H])


def scale_bar(pl, x1, y, length, text):
    """An engraved scale bar ending at x1 with its length lettered under it."""
    pl.stroke([(x1 - length, y), (x1, y)], INK, 0.85, 1.6, cap=0)
    for x in (x1 - length, x1):
        pl.stroke([(x, y - 5 * S), (x, y + 5 * S)], INK, 0.85, 1.2)
    pl.label(text, (x1 - length / 2, y + 22 * S), SOFT, 13, "c", slip=0)


def fig(pl, x, y, num, text, size=13):
    """An italic "Fig. N." caption, as on the panels of the game."""
    pl.label(f"Fig. {num}.", (x, y), INK, size, slip=0, bold=True)
    from ink import text_size
    w, _ = text_size(f"Fig. {num}. ", size, bold=True)
    pl.label(text, (x + w, y), SOFT, size, slip=0)


def soma(pl, c, r, col, name=None, below=True, size=13, seed=0):
    """A stained soma for the little wiring figures: a wash of stain, the cell body, a few inked dendrites."""
    rng = np.random.default_rng(seed + 11)
    pl.wash(c, r * 2.0, col, 0.18)
    for k in range(5):
        a = rng.uniform(0, 2 * math.pi)
        ln = rng.uniform(1.25, 1.7) * r
        pl.stroke([c + np.array([math.cos(a), math.sin(a)]) * r * 0.9, c + np.array([math.cos(a + 0.2), math.sin(a + 0.2)]) * ln], INK, 0.55, 0.9)
    pl.circle(c, r, col, INK, 0.92, 0.8, 1.2)
    pl.circle(c - np.array([0.3, 0.35]) * r, r * 0.16, SHEET, None, 0.35)
    if name:
        pl.label(name, (c[0], c[1] + (r + 26 * S if below else -r - 10 * S)), INK, size, "c", slip=0.55)


def wire(pl, a, b, col, kind="arrow", bend=0.0, r0=0.0, r1=0.0, w=1.6, dash=None):
    """A synapse from a to b (cell radii r0, r1 trimmed off): arrowhead = excitatory, bar = inhibitory."""
    a, b = np.asarray(a, float), np.asarray(b, float)
    d = b - a
    L = np.hypot(*d)
    nn = np.array([-d[1], d[0]]) / L
    m = (a + b) / 2 + nn * bend * L
    pts = catmull([a, m, b], 24) if bend else np.array([a + (b - a) * t for t in np.linspace(0, 1, 40)])
    keep = np.array([np.hypot(*(q - a)) >= r0 and np.hypot(*(q - b)) >= r1 + lw(3) for q in pts])
    pts = pts[keep]
    pl.arrow(pts, col, 0.9, w, 9.0, dash=dash, kind=kind)


def trace(pl, x0, y0, w, h, v, col=INK, a=0.9, width=1.3, base=True):
    """A pen trace: v (array, roughly 0..1) drawn left to right in the box (x0, y0, w, h), y up."""
    v = np.asarray(v, float)
    xs = x0 + np.linspace(0, w, len(v))
    ys = y0 + h - v * h
    if base:
        pl.stroke([(x0, y0 + h), (x0 + w, y0 + h)], INK, 0.18, 0.8)
    pl.stroke(np.c_[xs, ys], col, a, width)


def burst_train(n, period, on, phase, nsp=9, seed=0, slow=0.22):
    """A bursting membrane trace, 0..1 over [0, 1]: a slow depolarised plateau during each burst, `nsp` spikes riding on it
    (period, on fraction and phase in units of the trace length / period)."""
    rng = np.random.default_rng(seed)
    t = np.linspace(0, 1, n)
    ph = ((t / period) - phase) % 1.0
    inb = ph < on
    v = 0.14 + np.where(inb, slow * np.sin(np.pi * np.clip(ph / on, 0, 1)) ** 0.5, -0.07 * np.sin(np.pi * np.clip((ph - on) / (1 - on), 0, 1)))
    k0 = math.floor(-phase - 1)
    while True:
        t0 = (k0 + phase) * period
        if t0 > 1:
            break
        for j in range(nsp):
            ts = t0 + period * on * (0.06 + 0.88 * (j / (nsp - 1)) ** 1.25) + rng.normal(0, period * 0.004)
            i = int(round(ts * (n - 1)))
            if 0 <= i < n:
                v[i] = 0.92 - 0.08 * rng.uniform()
                if i + 1 < n:
                    v[i + 1] = v[i + 1] - 0.05
        k0 += 1
    return v


# ================================================================ BESTIARY

def worm():
    """Caenorhabditis elegans, adult hermaphrodite, left side, head to the left, crawling in a sine on its side. Proportions
    after WormAtlas: about 1 mm long and 1/16 as wide; pharynx (procorpus, metacorpus, isthmus, terminal bulb with grinder) in
    the first eighth; nerve ring round the isthmus; intestine to the anus; vulva at mid-body with eggs in the uterus and the two
    reflexed gonad arms; ventral cord; the touch cells ALM, AVM (anterior field) and PLM, PVM (posterior)."""
    pl = Plate()
    # centreline: a crawling sine through the three pins (head left)
    xs = np.linspace(150, 1840, 120)
    ys = 612 + 163 * np.cos(2 * np.pi * (xs - 400) / 1200)
    ys += 30 * np.exp(-((xs - 1840) / 160) ** 2)   # the tail flicks down a little
    sp = Spine(np.c_[xs, ys], 900)
    R = 60.0   # half-width at mid-body: length / width about 16

    def prof(u):
        if u < 0.02:
            return R * (0.42 * math.sqrt(max(0.0, 1 - ((0.02 - u) / 0.02) ** 2)))
        r = R * (0.42 + 0.58 * smoothstep(0.02, 0.21, u) ** 0.8)
        if u > 0.80:
            r = R * (1 - 0.55 * smoothstep(0.80, 0.915, u))
        if u > 0.915:
            r = R * 0.45 * (1 - (u - 0.915) / 0.085) ** 1.6 + 0.6
        return r

    body = sp.outline(prof, 0, 1, 500)
    pl.fill(body, WORMFILL, 0.95)
    pl.hatch(body, 0.9, 7, BODY, 0.10, 0.8)
    # ventral = +normal side; at the head the normal points down-ish
    # intestine (granular) from behind the terminal bulb to the anus
    gut = sp.outline(lambda u: prof(u) * 0.50 * math.sqrt(smoothstep(0.148, 0.17, u) * (1 - smoothstep(0.89, 0.906, u))) + 0.5, 0.148, 0.906, 300)
    gut = np.array([q for q in gut])
    pl.shape(gut, GUT, BODY, 0.55, 0.45, 1.0)
    pl.stipple(gut, 1400, BODY, 0.35, 1.5, seed=3)
    pl.stroke(sp.line(0.16, 0.9, lambda u: prof(u) * 0.05, 200), BODY, 0.55, 1.0)   # the lumen
    # gonad arms: from the spermatheca along the ventral side outward, round the loop, back along the dorsal side
    for (ua, ub, uc) in ((0.40, 0.25, 0.47), (0.64, 0.79, 0.57)):
        ven = sp.line(ua, ub, lambda u: prof(u) * 0.40, 30)
        dor = sp.line(ub, uc, lambda u: -prof(u) * 0.46, 30)
        arm = catmull(np.vstack([ven[::3], dor[::3]]), 8)
        pl.stroke(arm, BODY, 0.55, 15.0)
        pl.stroke(arm, hexc("#E2CCA8"), 1.0, 12.0)
        pl.stipple(np.vstack([sp.line(ub, uc, lambda u: -prof(u) * 0.36, 30), sp.line(uc, ub, lambda u: -prof(u) * 0.56, 30)]), 60, NUC, 0.45, 2.4, seed=int(ua * 100))
        # oocytes stacked in the proximal arm (nearest the vulva): cross-walls
        for k in range(7):
            u = ua + (ub - ua) * (0.03 + 0.085 * k)
            pl.stroke([sp.at(u, prof(u) * 0.27), sp.at(u, prof(u) * 0.53)], BODY, 0.55, 1.0)
    # eggs in the uterus, either side of the vulva
    for k, u in enumerate([0.445, 0.475, 0.505, 0.545, 0.575, 0.605]):
        c = sp.at(u, prof(u) * (0.12 if k % 2 else -0.05))
        t = sp.tangent(u)
        e = ellipse(c, R * 0.52, R * 0.33, math.atan2(t[1], t[0]))
        pl.shape(e, hexc("#F3E7D2"), BODY, 1, 0.7, 1.2)
        pl.stipple(e, 26, BODY, 0.45, 2.2, seed=10 + k)
    vul = sp.at(0.525, prof(0.525))
    pl.stroke([sp.at(0.522, prof(0.522) * 0.55), vul], BODY, 0.8, 1.6)
    # pharynx: procorpus, metacorpus bulb, isthmus, terminal bulb with the grinder
    def phw(u):
        base = 0.20 - 0.08 * smoothstep(0.050, 0.070, u)     # procorpus, then the narrow isthmus
        bulb = lambda c, hl, r: r * math.sqrt(max(0.0, 1 - ((u - c) / hl) ** 2))
        w = max(base, bulb(0.052, 0.016, 0.40), bulb(0.121, 0.0155, 0.52))   # metacorpus and terminal bulb
        return R * w * math.sqrt(smoothstep(0.006, 0.02, u)) + 0.5
    ph = sp.outline(phw, 0.006, 0.1365, 260)
    pl.shape(ph, BODY, BODY, 0.30, 0.85, 1.4)
    pl.stroke(sp.line(0.004, 0.134, 0, 60), BODY, 0.75, 1.0)    # pharyngeal lumen
    g = sp.at(0.121, 0)
    t = sp.tangent(0.121)
    pl.fill(ellipse(g, R * 0.17, R * 0.07, math.atan2(t[1], t[0])), INK, 0.7)   # the grinder
    pl.circle(sp.at(0.052, 0), R * 0.08, BODY, None, 0.6)                      # metacorpus valve
    # nerve ring round the isthmus (seen edge-on: a narrow oblique band) and the head ganglia either side of it
    ring_u = 0.088
    band = catmull(np.array([sp.at(ring_u - 0.006, -prof(ring_u) * 0.93), sp.at(ring_u + 0.0, -prof(ring_u) * 0.95),
                             sp.at(ring_u + 0.007, prof(ring_u) * 0.95), sp.at(ring_u + 0.001, prof(ring_u) * 0.93)]), 8, closed=True)
    pl.wash(sp.at(ring_u, 0), R * 1.1, EXC, 0.22)
    pl.shape(band, EXC, EXC, 0.30, 0.9, 1.4)
    for k, (u0, u1) in enumerate(((0.060, 0.080), (0.097, 0.110))):
        reg = sp.outline(lambda u: prof(u) * 0.86, u0, u1, 40)
        pl.stipple(reg, 40, NUC, 0.5, 3.0, seed=40 + k)
    # ventral nerve cord with motor-neuron somata, dorsal cord
    vnc = sp.line(0.1, 0.9, lambda u: prof(u) * 0.84, 300)
    pl.stroke(vnc, MOTOR, 0.55, 1.6)
    for u in np.linspace(0.14, 0.86, 22):
        pl.circle(sp.at(u, prof(u) * 0.84), lw(2.6), MOTOR, None, 0.7)
    pl.stroke(sp.line(0.1, 0.9, lambda u: -prof(u) * 0.86, 300), MOTOR, 0.35, 1.0)
    # lumbar ganglion (PVC) in the tail
    pl.stipple(sp.outline(lambda u: prof(u) * 0.8, 0.905, 0.93, 20), 40, NUC, 0.55, 3.0, seed=7)
    # touch receptor neurons (verdigris): ALM (lateral, soma at about 0.43, process to the nose), AVM (ventral), PLM (tail,
    # process forward past mid-body), PVM
    def trn(us, ue, off, soma=True):
        ln = sp.line(us, ue, lambda u: prof(u) * off, 120)
        pl.stroke(ln, SENSOR, 0.8, 1.3)
        if soma:
            pl.circle(ln[0], lw(4.0), SENSOR, None, 0.95)
            pl.circle(ln[0], lw(1.6), SHEET, None, 0.9)
        return ln[0]
    alm = trn(0.43, 0.03, -0.30)
    avm = trn(0.33, 0.06, 0.62)
    plm = trn(0.935, 0.52, -0.20)
    pvm = trn(0.62, 0.50, 0.66)
    # command interneurons: AVA/AVD (backward), AVB (forward) in the lateral ganglia; PVC in the lumbar ganglion (sienna)
    for k, off in enumerate((-0.35, 0.25)):
        pl.circle(sp.at(0.100, prof(0.1) * off), lw(4.6), EXC, None, 0.95)
    pl.circle(sp.at(0.918, 0), lw(4.0), EXC, None, 0.95)
    pl.stroke(sp.line(0.103, 0.88, lambda u: prof(u) * 0.74, 300), EXC, 0.65, 1.1)
    # Fig. 2: the touch circuit, after Chalfie et al. 1985 (anterior touch -> AVD/AVA -> backward; posterior -> PVC -> AVB -> forward)
    ox, oy = 1190, 140
    fig(pl, ox, oy, 2, "The touch circuit.")
    rr = 13 * S
    cols = [ox + 60, ox + 250, ox + 440, ox + 620]
    r1, r2 = oy + 95, oy + 255
    for (x, y, nm, col) in ((cols[0], r1, "ALM, AVM", SENSOR), (cols[1], r1, "AVD", EXC), (cols[2], r1, "AVA", EXC),
                            (cols[0], r2, "PLM", SENSOR), (cols[1], r2, "PVC", EXC), (cols[2], r2, "AVB", EXC)):
        soma(pl, (x, y), rr, col, nm, seed=int(x + y))
    for y in (r1, r2):
        wire(pl, (cols[0], y), (cols[1], y), EXC, r0=rr, r1=rr)
        wire(pl, (cols[1], y), (cols[2], y), EXC, r0=rr, r1=rr)
    wire(pl, (cols[2], r1), (cols[3], r1), MOTOR, r0=rr, r1=0)
    wire(pl, (cols[2], r2), (cols[3], r2), MOTOR, r0=rr, r1=0)
    pl.label("backward", (cols[3] + 12, r1 + 8 * S), MOTOR, 13, slip=0)
    pl.label("forward", (cols[3] + 12, r2 + 8 * S), MOTOR, 13, slip=0)
    # Fig. 3: transverse section at mid-body
    cx, cy, cr = 1000.0, 985.0, 118.0
    fig(pl, cx - 150, cy - cr - 34, 3, "Section at mid-body.", 12)
    pl.circle((cx, cy), cr, WORMFILL, BODY, 0.95, 0.75, 1.8)
    pl.circle((cx, cy), cr - lw(5), None, BODY, ra=0.45, w=1.0)       # cuticle over the hypodermis
    for q, a0 in ((0, -math.pi * 0.86), (1, -math.pi * 0.36), (2, math.pi * 0.14), (3, math.pi * 0.64)):
        a1 = a0 + math.pi * 0.22   # four muscle quadrants, two dorsal and two ventral
        ang = np.linspace(a0, a1, 24)
        outer = np.c_[cx + np.cos(ang) * (cr - lw(7)), cy + np.sin(ang) * (cr - lw(7))]
        inner = np.c_[cx + np.cos(ang[::-1]) * (cr - lw(22)), cy + np.sin(ang[::-1]) * (cr - lw(22))]
        mq = np.vstack([outer, inner])
        pl.shape(mq, WALL, WALL, 0.30, 0.6, 1.0)
        pl.hatch(mq, a0 + 0.3, 3.5, WALL, 0.45, 0.8)
    pl.circle((cx, cy - cr + lw(9)), lw(5), MOTOR, None, 0.6)            # dorsal cord
    pl.circle((cx, cy + cr - lw(11)), lw(8), MOTOR, None, 0.75)           # ventral cord
    for sgn in (-1, 1):
        pl.fill(ellipse((cx + sgn * (cr - lw(9)), cy), lw(7), lw(13)), BODY, 0.35)   # lateral hypodermal cords
        pl.circle((cx + sgn * (cr - lw(11)), cy), lw(2.4), SENSOR, None, 0.9)            # the touch-cell process beside it
    gutc = ellipse((cx - lw(10), cy - lw(6)), lw(38), lw(30), 0.2)
    pl.shape(gutc, GUT, BODY, 0.6, 0.6, 1.0)
    pl.stipple(gutc, 90, BODY, 0.35, 1.6, seed=21)
    pl.stroke([(cx - lw(30), cy - lw(10)), (cx + lw(10), cy - lw(2))], BODY, 0.7, 1.0)
    for (gx, gy, gr) in ((cx + lw(40), cy + lw(26), 12), (cx - lw(30), cy + lw(38), 10)):
        pl.circle((gx, gy), lw(gr), hexc("#E2CCA8"), BODY, 1, 0.6, 1.0)
    pl.label("muscle", (cx + cr + 26, cy - 60), WALL, 12, leader=(cx + cr * 0.62, cy - cr * 0.62))
    pl.label("ventral cord", (cx + cr + 26, cy + cr - 6), MOTOR, 12, leader=(cx + lw(6), cy + cr - lw(11)))
    pl.label("intestine", (cx - cr - 26, cy - 20), SOFT, 12, "r", leader=(cx - lw(40), cy - lw(6)))
    # rim last
    pl.stroke(body, BODY, 0.6, 1.8, closed=True)
    anus = sp.at(0.905, prof(0.905))
    # lettering
    pl.label("Caenorhabditis elegans", (110, 150), INK, 17, slip=0)
    pl.label("adult hermaphrodite, left side, head to the left", (110, 150 + 30 * S), SOFT, 13, slip=0)
    head = sp.at(0.03, 0)
    pl.label("pharynx", (head[0] - 10, head[1] - 150), SOFT, 14, leader=tuple(sp.at(0.05, -prof(0.05) * 0.2)))
    pl.label("nerve ring", (330, 520), EXC_INK, 14, leader=tuple(sp.at(ring_u + 0.004, -prof(ring_u) * 0.92)))
    pl.label("AVA, AVB command cells", (130, 1000), EXC_INK, 14, leader=tuple(sp.at(0.100, prof(0.1) * 0.25)))
    pl.label("ALM, AVM touch cells", (590, 890), SENSOR, 14, leader=tuple(avm))
    pl.label("intestine", (690, 330), SOFT, 14, leader=tuple(sp.at(0.36, -prof(0.36) * 0.3)))
    pl.label("vulva", (vul[0] + 40, vul[1] + 70), SOFT, 14, leader=tuple(vul))
    pl.label("eggs", (1150, 380), SOFT, 14, leader=tuple(sp.at(0.47, -prof(0.47) * 0.35)))
    pl.label("gonad", (1330, 520), SOFT, 14, leader=tuple(sp.at(0.70, -prof(0.70) * 0.48)))
    pl.label("ventral cord", (1350, 1000), MOTOR, 14, leader=tuple(sp.at(0.76, prof(0.76) * 0.84)))
    pl.label("PLM, PVM touch cells", (1700, 1060), SENSOR, 14, "c", leader=tuple(plm))
    pl.label("anus", (anus[0] + 20, anus[1] + 80), SOFT, 14, leader=tuple(anus))
    _ = (alm, pvm)
    scale_bar(pl, 1880, 1150, sp.length * 0.1, "100 µm")
    pl.keep_rect(*body.min(0), *(body.max(0) - body.min(0)), 30)
    return pl


def leech():
    """Hirudo medicinalis, dorsal view, head to the left, at rest on an arc. About 1:7 wide; small oral sucker with five pairs of
    eyes, about a hundred annuli (five to a mid-body segment, sensilla on the first), the paired paramedial stripes of the
    medicinal leech, a large posterior sucker. Seen through the body: the ventral nerve cord (head ganglion, 21 mid-body
    ganglia, tail ganglion) and the two lateral heart tubes; the heart interneurons HN(3), HN(4) sit in ganglia 3 and 4."""
    pl = Plate()
    xs = np.linspace(200, 1690, 100)
    ys = 575 + 0.00052 * (xs - 1000) ** 2
    sp = Spine(np.c_[xs, ys], 900)
    R = 128.0

    def prof(u):
        r = R * (0.16 + 0.84 * smoothstep(0.0, 0.5, u) ** 0.65)
        if u > 0.80:
            r *= 1 - 0.50 * smoothstep(0.80, 1.0, u)
        if u < 0.012:
            r *= math.sqrt(max(0.0, 1 - ((0.012 - u) / 0.012) ** 2))
        return r + 0.5

    body = sp.outline(prof, 0, 1, 500)
    end = sp.at(1.0)
    tg = sp.tangent(1.0)
    suck = end + tg * R * 0.42
    # posterior sucker: a disc, radial striae
    pl.circle(suck, R * 0.66, hexc("#E4D9C4"), FISH, 1, 0.7, 1.8)
    for k in range(28):
        a = 2 * math.pi * k / 28
        pl.stroke([suck + np.array([math.cos(a), math.sin(a)]) * R * 0.2, suck + np.array([math.cos(a), math.sin(a)]) * R * 0.6], FISH, 0.35, 0.9)
    pl.circle(suck, R * 0.2, None, FISH, ra=0.5, w=1.0)
    pl.fill(body, hexc("#DCD8C6"), 0.97)
    pl.hatch(body, -0.6, 6, FISH, 0.10, 0.8)
    # colour pattern: paired paramedial stripes (rust, black-spotted) and lateral stripes
    for sg in (-1, 1):
        st = np.vstack([sp.line(0.04, 0.97, lambda u: sg * prof(u) * 0.16, 200), sp.line(0.97, 0.04, lambda u: sg * prof(u) * 0.34, 200)])
        pl.fill(st, EXC, 0.22)
        lat = np.vstack([sp.line(0.04, 0.97, lambda u: sg * prof(u) * 0.70, 200), sp.line(0.97, 0.04, lambda u: sg * prof(u) * 0.84, 200)])
        pl.fill(lat, GAMBOGE, 0.22)
    # annuli: about a hundred; segment borders (every fifth) heavier, sensilla on the first annulus of each segment
    nA = 102
    for k in range(1, nA):
        u = 0.015 + 0.965 * k / nA
        seg = (k % 5 == 0)
        ln = [sp.at(u, -prof(u) * 0.99), sp.at(u + 0.002, 0), sp.at(u, prof(u) * 0.99)]
        pl.stroke(catmull(ln, 6), FISH, 0.40 if seg else 0.20, 1.0 if seg else 0.7)
        if seg:
            for off in (-0.82, -0.56, -0.25, 0.25, 0.56, 0.82):
                pl.circle(sp.at(u + 0.0045, prof(u) * off), lw(1.7), INK, None, 0.55)
    # heart tubes: lateral vessels with segmental bulges
    for sg in (-1, 1):
        ht = sp.line(0.06, 0.92, lambda u: sg * prof(u) * 0.60, 300)
        pl.stroke(ht, WALL, 0.55, 4.0)
        for u in np.linspace(0.14, 0.86, 21):
            pl.circle(sp.at(u, sg * prof(u) * 0.60), lw(3.4), WALL, None, 0.6)
    # the ventral nerve cord, seen through the body: paired connectives, ganglia, segmental roots
    for off in (-0.035, 0.035):
        pl.stroke(sp.line(0.04, 0.95, lambda u: R * off, 300), INK, 0.55, 1.0)
    gang = list(np.linspace(0.115, 0.865, 21))
    for i, u in enumerate(gang):
        c = sp.at(u)
        t = sp.tangent(u)
        for sg in (-1, 1):
            for du in (-0.004, 0.004):
                pl.stroke([sp.at(u + du, sg * R * 0.06), sp.at(u + du * 2.2, sg * prof(u) * 0.52)], INK, 0.35, 0.8)
        e = ellipse(c, R * 0.085, R * 0.062, math.atan2(t[1], t[0]))
        hn = i in (2, 3)
        if hn:
            pl.wash(c, R * 0.30, INH, 0.30)
        pl.shape(e, INH if hn else hexc("#CFC6B4"), INK, 0.85 if hn else 1.0, 0.8, 1.2)
    hg = ellipse(sp.at(0.045), R * 0.11, R * 0.09, math.atan2(*sp.tangent(0.045)[::-1]))
    pl.shape(hg, hexc("#CFC6B4"), INK, 1, 0.8, 1.2)
    tgang = ellipse(sp.at(0.935), R * 0.12, R * 0.08, math.atan2(*sp.tangent(0.935)[::-1]))
    pl.shape(tgang, hexc("#CFC6B4"), INK, 1, 0.8, 1.2)
    # head: oral sucker rim and five pairs of eyes in a horseshoe
    for k in range(5):
        u = 0.008 + 0.0065 * k
        for sg in (-1, 1):
            pl.circle(sp.at(u, sg * prof(u) * 0.55), lw(2.6), INK, None, 0.85)
    pl.stroke(body, FISH, 0.75, 1.8, closed=True)
    # lettering
    pl.label("Hirudo medicinalis", (1290, 150), INK, 17, slip=0)
    pl.label("dorsal view; nerve cord and heart tubes seen through", (1290, 150 + 30 * S), SOFT, 13, slip=0)
    pl.label("anterior sucker and eyes", (130, 1110), SOFT, 14, leader=tuple(sp.at(0.01, 0)))
    pl.label("HN(3), HN(4): heart interneurons", (420, 1010), INH, 14, leader=tuple(sp.at(gang[3], 0)))
    pl.label("heart tube", (330, 560), WALL, 14, "r", leader=tuple(sp.at(0.16, -prof(0.16) * 0.60)))
    pl.label("ganglion 10", (1130, 840), SOFT, 14, leader=tuple(sp.at(gang[9], prof(gang[9]) * 0.08)))
    pl.label("segment: five annuli", (1240, 380), SOFT, 14, leader=tuple(sp.at(0.6, -prof(0.6) * 0.97)))
    pl.label("posterior sucker", (1700, 1060), SOFT, 14, "c", leader=tuple(suck + np.array([0, R * 0.5])))
    pl.label("tail ganglion", (1460, 960), SOFT, 14, "r", leader=tuple(sp.at(0.935, prof(0.935) * 0.1)))
    # Fig. 2: the half-centre oscillator of the heartbeat: two heart interneurons that inhibit each other and burst in turn
    ox, oy = 110, 130
    fig(pl, ox, oy, 2, "A half-centre: HN(L) and HN(R) of ganglion 4.")
    rr = 13 * S
    a, b = np.array([ox + 70, oy + 95]), np.array([ox + 70, oy + 245])
    soma(pl, a, rr, INH, None, seed=3)
    soma(pl, b, rr, INH, None, seed=4)
    pl.label("L", (a[0] - 40, a[1] + 8), INH, 13, "c", slip=0)
    pl.label("R", (b[0] - 40, b[1] + 8), INH, 13, "c", slip=0)
    wire(pl, a, b, INH, "bar", bend=0.22, r0=rr, r1=rr)
    wire(pl, b, a, INH, "bar", bend=0.22, r0=rr, r1=rr)
    va = burst_train(700, 0.42, 0.45, 0.0, seed=1)
    vb = burst_train(700, 0.42, 0.45, 0.5, seed=2)
    trace(pl, ox + 150, a[1] - 45, 560, 80, va, INK, 0.85, 1.0)
    trace(pl, ox + 150, b[1] - 45, 560, 80, vb, INK, 0.85, 1.0)
    pl.stroke([(ox + 150 + 560 - 160, b[1] + 60), (ox + 150 + 560, b[1] + 60)], INK, 0.8, 1.4)
    pl.label("1 s", (ox + 150 + 560 - 80, b[1] + 90), SOFT, 12, "c", slip=0)
    scale_bar(pl, 1880, 1160, 190, "1 cm")
    pl.keep_rect(*body.min(0), *(body.max(0) - body.min(0)), 25)
    return pl


def aplysia():
    """Aplysia californica from the right side, head to the right: oral tentacles and rolled rhinophores on the head, the
    eye in front of the rhinophore, the right parapodium cut away over the mantle cavity to show the gill (ctenidium), the
    exhalant siphon between the parapodia at the back, the foot and tapering tail. Inside: the abdominal ganglion with the
    siphon nerve (sensory) and the branchial nerve to the gill (motor), the connectives forward to the head ring."""
    pl = Plate()
    # dorsal outline: head, neck, the parapodial edge (gently frilled, highest in the posterior half), the siphon notch, the tail
    ctrl = [(1765, 770), (1742, 708), (1690, 668), (1610, 650), (1500, 618), (1400, 560), (1300, 492), (1190, 440), (1080, 412),
            (960, 398), (840, 392), (730, 404), (640, 432), (590, 470), (520, 522), (440, 584), (336, 676), (236, 790), (176, 864), (300, 900),
            (600, 926), (1000, 936), (1300, 926), (1550, 904), (1700, 862), (1756, 814)]
    body = catmull(ctrl, 14, closed=True)
    # far-side rhinophore and oral tentacle first (behind the body)
    def rhino(base, tip, wb, col_a):
        ax = Spine(catmull([base, (base[0] * 0.6 + tip[0] * 0.4 + 6, base[1] * 0.6 + tip[1] * 0.4), tip], 10), 60)
        o = ax.outline(lambda u: wb * (1 - 0.45 * u) * (1 - smoothstep(0.92, 1.0, u) * 0.6) + 0.5, 0, 1, 60)
        pl.shape(o, hexc("#D7BBA0"), BODY, col_a, 0.75, 1.6)
        pl.stroke(ax.line(0.12, 0.96, lambda u: wb * 0.25, 30), BODY, 0.6, 1.0)   # the rolled edge
        return ax
    rhino((1640, 664), (1606, 556), 22, 0.85)
    far_tent = catmull([(1748, 724), (1830, 700), (1900, 716), (1910, 744), (1850, 752), (1770, 760)], 10, closed=True)
    pl.shape(far_tent, hexc("#D7BBA0"), BODY, 0.8, 0.7, 1.4)
    # the body
    pl.fill(body, hexc("#E7D2BC"), 0.97)
    pl.hatch(body, 1.2, 7, BODY, 0.10, 0.8)
    pl.stipple(body, 900, BODY, 0.13, 4.0, seed=5)   # the mottled skin
    for k, (cx, cy, r) in enumerate([(560, 640, 26), (690, 800, 22), (1220, 760, 24), (1400, 660, 20), (1520, 760, 18), (840, 860, 16),
                                     (1120, 860, 15), (420, 760, 18), (1600, 700, 14)]):
        pl.circle((cx, cy), lw(r * 0.6), None, BODY, ra=0.35, w=1.0)   # pale ring blotches of A. californica
    # foot margin and sole
    foot = catmull([(220, 846), (420, 870), (800, 892), (1200, 892), (1500, 872), (1690, 830), (1745, 790)], 10)
    pl.stroke(foot, BODY, 0.55, 1.4)
    sole = np.vstack([foot, catmull([(1752, 808), (1700, 856), (1550, 900), (1300, 924), (1000, 934), (600, 924), (300, 898), (182, 858)], 10)])
    pl.hatch(sole, 0.15, 5, BODY, 0.18, 0.8)
    # the right parapodium: a flap rising from the side of the foot, its free edge the dorsal outline
    para = catmull([(1420, 572), (1300, 494), (1190, 442), (1080, 414), (960, 400), (840, 394), (730, 406), (640, 434), (590, 472),
                    (610, 620), (760, 742), (1000, 792), (1250, 760), (1400, 680)], 12, closed=True)
    pl.fill(para, hexc("#DFC4AA"), 0.55)
    pl.stroke(catmull([(590, 472), (610, 620), (760, 742), (1000, 792), (1250, 760), (1400, 680), (1450, 610)], 12), BODY, 0.35, 1.2)
    for k in range(6):   # folds of the flap
        t = 0.18 + 0.13 * k
        a = np.array(catmull([(1330, 510), (1000, 420), (640, 450)], 10))[int(t * 20)]
        pl.stroke(catmull([a, a + np.array([20, 160]), a + np.array([30, 300 - abs(k - 2.5) * 30])], 8), BODY, 0.22, 1.0)
    # the window cut in the parapodium over the mantle cavity, with the gill inside
    win = ellipse((950, 585), 215, 112, -0.06, 96)
    pl.shape(win, hexc("#F1E2CF"), None, 1.0)
    pl.hatch(win, 0.6, 5, BODY, 0.14, 0.8)
    # mantle shelf (the roof of the cavity) across the top of the window
    mantle = catmull([(760, 520), (880, 492), (1030, 488), (1150, 512)], 10)
    pl.stroke(mantle, BODY, 0.75, 2.0)
    # ctenidium: a plume on an axis, leaflets on both sides, tapering to the tip
    gax = Spine(catmull([(1120, 556), (1000, 580), (880, 602), (780, 630)], 10), 120)
    pl.clip(win)
    for k in range(34):
        u = 0.03 + 0.94 * k / 33
        c = gax.at(u)
        ln = 62 * (1 - 0.65 * u) * (0.85 + 0.15 * math.sin(k * 1.7))
        for sg in (-1, 1):
            tip = gax.at(min(u + 0.03, 1), sg * ln)
            leaf = catmull([c, (c + tip) / 2 + gax.tangent(u) * 7, tip], 6)
            pl.stroke(leaf, WALL, 0.55, 3.2)
            pl.stroke(leaf, hexc("#E7B9A8"), 0.9, 1.2)
    pl.stroke(gax.line(0, 1, 0, 40), WALL, 0.85, 3.0)
    pl.unclip()
    pl.stroke(win, BODY, 0.8, 1.6, closed=True)
    pl.stroke(ellipse((952, 590), 215, 112, -0.06, 96)[10:44], BODY, 0.45, 3.5)   # the cut thickness of the flap
    # siphon at the back, between the parapodia
    sax = Spine(catmull([(598, 482), (578, 456), (556, 436)], 10), 60)
    so = sax.outline(lambda u: 24 * (1 - 0.2 * u) + 5 * smoothstep(0.8, 1, u), 0, 1, 60)
    pl.shape(so, hexc("#DDBFA4"), BODY, 1, 0.8, 1.6)
    pl.shape(ellipse(sax.at(1.0), 28, 10, math.atan2(*sax.tangent(1.0)[::-1]) + math.pi / 2), hexc("#B99374"), BODY, 0.8, 0.8, 1.2)
    # the near oral tentacle and rhinophore, eye
    tent = catmull([(1742, 742), (1820, 734), (1896, 760), (1918, 800), (1880, 812), (1800, 806), (1750, 796)], 10, closed=True)
    pl.shape(tent, hexc("#DDBFA4"), BODY, 1, 0.8, 1.6)
    pl.stroke(catmull([(1764, 772), (1830, 770), (1890, 790)], 8), BODY, 0.55, 1.1)   # the groove of the rolled tentacle
    rh = rhino((1676, 668), (1652, 540), 24, 1.0)
    eye = np.array([1726, 702])
    pl.circle(eye, lw(3.2), INK, None, 0.9)
    pl.circle(eye, lw(5.4), None, BODY, ra=0.5, w=1.0)
    # nervous system seen through: head ring (cerebral, pleural, pedal), connectives, abdominal ganglion, siphon and branchial nerves
    ring = [np.array([1680, 744]), np.array([1650, 806]), np.array([1596, 824])]
    for c in ring:
        pl.shape(ellipse(c, 20, 15), hexc("#E9C9A0"), EXC, 0.95, 0.85, 1.2)
    abd = np.array([700, 760])
    con = catmull([ring[2], (1300, 850), (1000, 840), (800, 800), abd], 12)
    pl.stroke(con, EXC, 0.7, 1.4)
    pl.stroke(con + np.array([0, 9]), EXC, 0.45, 1.0)
    siph_n = catmull([abd, (660, 680), (620, 580), (595, 500), sax.at(0.6)], 12)
    pl.stroke(siph_n, SENSOR, 0.9, 1.8)
    bran_n = catmull([abd, (740, 700), (780, 650), gax.at(0.95)], 12)
    pl.stroke(bran_n, MOTOR, 0.9, 1.8)
    pl.wash(abd, 56, EXC, 0.28)
    ag = ellipse(abd, 30, 21, -0.3)
    pl.shape(ag, hexc("#E9C9A0"), EXC, 1, 0.9, 1.4)
    pl.stipple(ag, 30, NUC, 0.55, 3.2, seed=9)
    pl.stroke(body, BODY, 0.65, 1.8, closed=True)
    # lettering
    pl.label("Aplysia californica", (1300, 150), INK, 17, slip=0)
    pl.label("from the right side; the parapodium cut away over the gill", (1300, 150 + 30 * S), SOFT, 13, slip=0)
    pl.label("rhinophore", (1740, 470), SOFT, 14, leader=tuple(rh.at(0.85)))
    pl.label("oral tentacle", (1940, 900), SOFT, 14, "r", leader=(1880, 800))
    pl.label("eye", (1800, 640), SOFT, 14, leader=tuple(eye))
    pl.label("parapodium", (1240, 330), SOFT, 14, leader=(1200, 440))
    pl.label("gill", (1260, 610), WALL, 14, leader=tuple(gax.at(0.1)))
    pl.label("mantle", (860, 450), SOFT, 14, "r", leader=(880, 486))
    pl.label("siphon", (470, 470), SENSOR, 14, "r", leader=tuple(sax.at(0.9)))
    pl.label("siphon nerve", (430, 640), SENSOR, 14, "r", leader=tuple(siph_n[30]))
    pl.label("branchial nerve", (900, 720), MOTOR, 14, leader=tuple(bran_n[20]))
    pl.label("abdominal ganglion", (700, 1010), EXC_INK, 14, "c", leader=tuple(abd + np.array([0, 20])))
    pl.label("head ganglia", (1560, 1000), EXC_INK, 14, "c", leader=tuple(ring[1]))
    pl.label("tail", (140, 960), SOFT, 14, leader=(200, 860))
    pl.label("foot", (1100, 1000), SOFT, 14, "c", leader=(1100, 915))
    # Fig. 2: the gill-withdrawal circuit (Kandel): siphon sensory cell -> L7 motor cell; tail sensory -> L29 facilitator (5-HT)
    ox, oy = 110, 110
    fig(pl, ox, oy, 2, "The reflex and its facilitator.")
    rr = 12 * S
    sn, mn = np.array([ox + 60, oy + 85]), np.array([ox + 420, oy + 85])
    tn, fn = np.array([ox + 60, oy + 195]), np.array([ox + 240, oy + 195])
    soma(pl, sn, rr, SENSOR, None, seed=1)
    soma(pl, mn, rr, MOTOR, None, seed=2)
    soma(pl, tn, rr, SENSOR, None, seed=3)
    soma(pl, fn, rr, GAMBOGE, None, seed=4)
    wire(pl, sn, mn, EXC, r0=rr, r1=rr)
    wire(pl, tn, fn, EXC, r0=rr, r1=rr)
    mid = (sn + mn) / 2 + np.array([10, 0])
    pl.arrow(catmull([fn + np.array([0, -rr]), fn + np.array([20, -80]), mid + np.array([0, 14])], 12), GAMBOGE, 0.95, 1.6)
    wire(pl, mn, mn + np.array([150, 0]), MOTOR, r0=rr)
    pl.label("siphon", (sn[0], sn[1] + 44), SENSOR, 13, "c", slip=0)
    pl.label("L7", (mn[0], mn[1] + 44), MOTOR, 13, "c", slip=0)
    pl.label("gill", (mn[0] + 165, mn[1] + 8), MOTOR, 13, slip=0)
    pl.label("tail", (tn[0], tn[1] + 44), SENSOR, 13, "c", slip=0)
    pl.label("L29, serotonin", (fn[0] + 30, fn[1] + 44), hexc("#8A6410"), 13, slip=0)
    scale_bar(pl, 1880, 1160, 160, "2 cm")
    pl.keep_rect(*body.min(0), *(body.max(0) - body.min(0)), 25)
    return pl


def crab():
    """Cancer borealis, dorsal view: the broad oval carapace (about 1.6 times as wide as long) with nine anterolateral teeth a
    side, the frontal teeth and the orbits with their stalked eyes, the heavy chelipeds folded in front, four pairs of walking
    legs. Drawn through the carapace: the foregut (cardiac sac and pylorus) and the stomatogastric nervous system on it,
    commissural and oesophageal ganglia, the stomatogastric nerve down to the STG, the dorsal ventricular nerve and its
    branches to the pyloric muscles."""
    pl = Plate()
    C = np.array([1000.0, 600.0])
    rx, ry = 440.0, 255.0

    def mirror(pts):
        pts = np.asarray(pts, float)
        return np.c_[2 * C[0] - pts[:, 0], pts[:, 1]]

    # right half of the carapace, front centre to back centre
    half = []
    # frontal margin: three small teeth between the orbits
    for x, y in ((1000, C[1] - ry - 8), (1022, C[1] - ry + 2), (1040, C[1] - ry - 6), (1060, C[1] - ry + 4)):
        half.append((x, y))
    # orbit: a notch with the eye in it
    half += [(1080, C[1] - ry + 14), (1096, C[1] - ry + 22), (1116, C[1] - ry + 10)]
    th0, th1 = math.radians(-74), math.radians(-4)
    for k in range(9 * 6 + 1):   # nine anterolateral teeth
        f = k / (9 * 6)
        th = th0 + (th1 - th0) * f
        bump = 1 + 0.035 * (math.sin(math.pi * ((k % 6) / 6.0)) ** 2 if k % 6 else 0.0) - 0.012
        bump = 1 + 0.03 * math.sin(math.pi * (k % 6) / 6.0) ** 1.5
        half.append((C[0] + rx * bump * math.cos(th), C[1] + ry * 1.03 * bump * math.sin(th)))
    half += [(C[0] + rx * 0.97, C[1] + 40), (C[0] + rx * 0.80, C[1] + 130), (C[0] + rx * 0.58, C[1] + 210), (C[0] + rx * 0.40, C[1] + 244),
             (C[0] + rx * 0.2, C[1] + 250), (C[0], C[1] + 252)]
    half = np.array(half)
    cara = np.vstack([half, mirror(half)[::-1][1:-1]])
    TAN = hexc("#E9CDB5")

    def limb(segs, col_fill=TAN, a=1.0):
        """A jointed limb from segment list [(p0, p1, w0, w1)], drawn proximal first; returns the tip."""
        for (p0, p1, w0, w1) in segs:
            sp = Spine(np.array([p0, p1]), 30)
            o = sp.outline(lambda u: (w0 + (w1 - w0) * u) * (0.85 + 0.15 * math.sin(math.pi * u)) + 0.5, 0, 1, 40)
            pl.shape(o, col_fill, BODY, a, 0.75, 1.5)
            pl.hatch(o, math.atan2(p1[1] - p0[1], p1[0] - p0[0]) + 1.2, 5, BODY, 0.16, 0.8)
            pl.circle(p0, w0 * 0.55, col_fill, BODY, a, 0.5, 1.0)
        return segs[-1][1]

    legs = []
    for side in (1, -1):
        def X(x):
            return C[0] + side * (x - C[0])
        # walking legs: base on the side of the carapace, merus outward, carpus-propodus angled back, dactyl to a point
        for i, (by, ang, lm, lp, ld) in enumerate(((C[1] + 10, -18, 250, 200, 120), (C[1] + 80, 2, 260, 205, 125),
                                                     (C[1] + 145, 24, 245, 195, 118), (C[1] + 200, 46, 210, 170, 105))):
            bx = C[0] + rx * (0.93 - 0.11 * i)
            a1 = math.radians(ang)
            p0 = np.array([bx - 40, by])
            p1 = p0 + np.array([math.cos(a1), math.sin(a1)]) * lm
            a2 = a1 + math.radians(58)
            p2 = p1 + np.array([math.cos(a2), math.sin(a2)]) * lp * 0.45
            p3 = p2 + np.array([math.cos(a2 + 0.12), math.sin(a2 + 0.12)]) * lp * 0.55
            a3 = a2 + math.radians(24)
            p4 = p3 + np.array([math.cos(a3), math.sin(a3)]) * ld
            pts = [p0, p1, p2, p3, p4]
            if side < 0:
                pts = [np.array([X(q[0]), q[1]]) for q in pts]
            tip = limb([(pts[0], pts[1], 30, 26), (pts[1], pts[2], 24, 22), (pts[2], pts[3], 21, 17), (pts[3], pts[4], 14, 2)])
            pl.fill(Spine(np.array([pts[3] + (pts[4] - pts[3]) * 0.7, pts[4]]), 10).outline(lambda u: 5 * (1 - u) + 0.5, 0, 1, 10), INK, 0.6)
            legs.append(tip)
        # cheliped: merus under the front corner, carpus, the heavy palm folded inward, fingers with dark tips
        q = [np.array([X(1250), C[1] - 190]), np.array([X(1345), C[1] - 270]), np.array([X(1262), C[1] - 318]),
             np.array([X(1112), C[1] - 318])]
        limb([(q[0], q[1], 34, 32), (q[1], q[2], 32, 36)])
        palm = Spine(np.array([q[2] + np.array([side * 10, 6]), q[3]]), 40).outline(lambda u: 30 + 14 * math.sin(math.pi * min(1, u * 1.3)), 0, 1, 40)
        pl.shape(palm, TAN, BODY, 1, 0.8, 1.6)
        pl.hatch(palm, 0.3 * side, 5, BODY, 0.18, 0.8)
        pl.stipple(palm, 120, BODY, 0.35, 2.4, seed=60 + side)
        f1 = catmull([q[3] + np.array([0, -16]), q[3] + np.array([side * -46, -20]), q[3] + np.array([side * -88, 2])], 8)
        f2 = catmull([q[3] + np.array([0, 16]), q[3] + np.array([side * -46, 18]), q[3] + np.array([side * -84, 8])], 8)
        for f, o in ((f1, 1), (f2, -1)):
            sp = Spine(f, 30)
            fo = sp.outline(lambda u: 11 * (1 - u) + 1, 0, 1, 30)
            pl.shape(fo, TAN, BODY, 1, 0.8, 1.4)
            pl.fill(sp.outline(lambda u: 11 * (1 - u) + 1, 0.62, 1, 12), INK, 0.7)
    # the carapace over the legs
    pl.fill(cara, TAN, 0.98)
    pl.stipple(cara, 2600, BODY, 0.30, 2.0, seed=31)   # the granular surface
    pl.hatch(cara, 0.5, 6, BODY, 0.10, 0.8)
    # regions: the H-shaped groove between the gastric and cardiac regions, branchial regions either side
    for sg in (1, -1):
        g = catmull([(C[0] + sg * 70, C[1] - 60), (C[0] + sg * 110, C[1] + 10), (C[0] + sg * 100, C[1] + 90), (C[0] + sg * 150, C[1] + 170)], 10)
        pl.stroke(g, BODY, 0.5, 1.4)
        pl.stroke(catmull([(C[0] + sg * 160, C[1] - 110), (C[0] + sg * 230, C[1] - 40), (C[0] + sg * 330, C[1] - 20)], 10), BODY, 0.35, 1.2)
    pl.stroke([(C[0] - 110, C[1] + 10), (C[0] + 110, C[1] + 10)], BODY, 0.35, 1.2)
    # eyes in the orbits
    for sg in (1, -1):
        e = np.array([C[0] + sg * 98, C[1] - ry + 6])
        pl.fill(ellipse(e, 16, 11, sg * 0.4), INK, 0.8)
        pl.circle(e + np.array([sg * -4, -3]), 3, SHEET, None, 0.6)
    # the foregut, drawn through the carapace: cardiac sac and pylorus
    card = ellipse(C + np.array([0, -120]), 175, 105)
    pyl = ellipse(C + np.array([0, 40]), 95, 62)
    for g in (card, pyl):
        pl.fill(g, SHEET, 0.45)
        pl.stroke(g, INK, 0.55, 1.2, closed=True, dash=[5, 4])
    # the stomatogastric nervous system (sienna): CoGs, OG, ion/son, stn, STG, dvn, lvn, pdn and pyn
    og = C + np.array([0, -232])
    cogs = [C + np.array([sg * 150, -205]) for sg in (1, -1)]
    stg = C + np.array([0, -60])
    for cg in cogs:
        pl.stroke(catmull([og, (og + cg) / 2 + np.array([0, -14]), cg], 8), EXC, 0.85, 1.6)
        pl.stroke(catmull([cg, (cg + stg) / 2 + np.array([0, -30]), stg + np.array([0, -150])], 8), EXC, 0.6, 1.2)
    stn = np.array([og, stg])
    pl.stroke(stn, EXC, 0.9, 2.0)
    dvn = catmull([stg, C + np.array([0, 20]), C + np.array([0, 60])], 8)
    pl.stroke(dvn, EXC, 0.9, 2.0)
    for sg in (1, -1):
        lvn = catmull([C + np.array([0, 60]), C + np.array([sg * 60, 80]), C + np.array([sg * 120, 70])], 8)
        pl.stroke(lvn, EXC, 0.85, 1.6)
        for k, (dx, dy) in enumerate(((150, 30), (160, 110), (90, 140))):
            pl.stroke([C + np.array([sg * 120, 70]), C + np.array([sg * dx, 70 + dy - 60])], EXC, 0.7, 1.1)
    for c, r in [(og, 13)] + [(cg, 17) for cg in cogs]:
        pl.circle(c, r, hexc("#E9C9A0"), EXC, 1, 0.9, 1.4)
    pl.wash(stg, 46, EXC, 0.35)
    pl.shape(ellipse(stg, 22, 16), hexc("#E9C9A0"), EXC, 1, 0.95, 1.6)
    pl.stipple(ellipse(stg, 20, 14), 20, NUC, 0.6, 3.2, seed=4)
    pl.stroke(cara, BODY, 0.75, 1.8, closed=True)
    # lettering
    pl.label("Cancer borealis", (1000, 120), INK, 17, "c", slip=0)
    pl.label("dorsal view; the stomatogastric nervous system drawn through the carapace", (1000, 120 + 30 * S), SOFT, 13, "c", slip=0)
    pl.label("STG", (stg[0] + 50, stg[1] + 10), EXC_INK, 15, leader=tuple(stg + np.array([22, 0])))
    pl.label("commissural ganglion", (cogs[1][0] - 60, cogs[1][1] - 40), EXC_INK, 13, "r", leader=tuple(cogs[1]))
    pl.label("oesophageal ganglion", (og[0] + 220, og[1] - 70), EXC_INK, 13, leader=tuple(og))
    pl.label("stomach", (C[0] - 230, C[1] - 40), SOFT, 13, "r", leader=tuple(C + np.array([-150, -90])))
    pl.label("pylorus", (C[0] - 200, C[1] + 130), SOFT, 13, "r", leader=tuple(C + np.array([-80, 60])))
    pl.label("pyloric nerves", (C[0] + 210, C[1] + 170), EXC_INK, 13, leader=tuple(C + np.array([150, 90])))
    pl.label("cheliped", (520, 330), SOFT, 14, "r", leader=(640, 250))
    pl.label("walking legs", (1830, 470), SOFT, 14, "c", leader=tuple(legs[0] + np.array([-40, -40])))
    pl.label("eye", (C[0] + 150, C[1] - ry - 40), SOFT, 13, leader=(C[0] + 106, C[1] - ry + 2))
    # Fig. 2: the pyloric circuit (after Marder and Bucher 2007): the AB/PD pacemaker kernel, LP, PY; chemical synapses inhibitory
    ox, oy = 90, 110
    fig(pl, ox, oy, 2, "The pyloric circuit.")
    rr = 12 * S
    AB, PD, LP, PY = np.array([ox + 60, oy + 90]), np.array([ox + 190, oy + 90]), np.array([ox + 60, oy + 250]), np.array([ox + 220, oy + 250])
    pl.stroke(ellipse((AB + PD) / 2, 118, 50), GAMBOGE, 0.8, 1.2, closed=True, dash=[4, 4])
    for c, nm in ((AB, "AB"), (PD, "PD"), (LP, "LP"), (PY, "PY")):
        soma(pl, c, rr, INH, None, seed=int(c[0]))
        pl.label(nm, (c[0] + (-34 if c[0] < ox + 100 else 34), c[1] + 8), INK, 13, "c", slip=0)
    zz = [AB + np.array([rr + 6, 0])]
    for k in range(7):   # electrical coupling: a resistor between AB and PD
        zz.append(AB + np.array([rr + 18 + k * 9, (-7 if k % 2 == 0 else 7) if 0 < k < 6 else 0]))
    zz.append(PD - np.array([rr + 6, 0]))
    pl.stroke(zz, INK, 0.8, 1.2)
    wire(pl, AB, LP, INH, "bar", bend=0.0, r0=rr, r1=rr)
    wire(pl, PD, PY, INH, "bar", bend=0.0, r0=rr, r1=rr)
    wire(pl, LP, PD, INH, "bar", bend=-0.15, r0=rr, r1=rr)
    wire(pl, LP, PY, INH, "bar", bend=0.18, r0=rr, r1=rr)
    wire(pl, PY, LP, INH, "bar", bend=0.18, r0=rr, r1=rr)
    pl.label("kernel", ((AB + PD)[0] / 2, AB[1] - 56), hexc("#8A6410"), 12, "c", slip=0)
    # Fig. 3: the triphasic rhythm, recorded on the nerves
    fx, fy = 1400, 110
    fig(pl, fx, fy, 3, "The triphasic rhythm.")
    for k, (nm, ph, on) in enumerate((("PD", 0.0, 0.30), ("LP", 0.40, 0.25), ("PY", 0.66, 0.30))):
        y = fy + 40 + 75 * k
        v = burst_train(600, 0.40, on, ph, nsp=7, seed=k + 5, slow=0.18)
        trace(pl, fx + 60, y, 440, 62, v, INK, 0.85, 1.0)
        pl.label(nm, (fx + 40, y + 50), INK, 13, "r", slip=0)
    scale_bar(pl, 1880, 1160, 190, "2 cm")
    pl.keep_rect(*cara.min(0), *(cara.max(0) - cara.min(0)), 25)
    return pl


FLYRED = hexc("#B23A2E")


def fly_dorsal(pl, c, L, seed=0):
    """Drosophila melanogaster, female, dorsal view, head up; L = body length in px. Proportions: head about a third of the
    body wide with the red compound eyes, thorax with its macrochaetae, banded abdomen, wings folded over it reaching past
    the tip, six legs."""
    c = np.asarray(c, float)
    s = L / 100.0
    P = lambda x, y: c + np.array([x, y]) * s
    # legs (behind)
    for side in (1, -1):
        for k, (bx, by, pts) in enumerate((( 9, -14, [(22, -22), (34, -30), (40, -42)]), (10, -6, [(26, -2), (40, 6), (50, 4)]),
                                           (8, 4, [(22, 16), (32, 30), (40, 44)]))):
            leg = [P(side * bx, by)] + [P(side * x, y) for x, y in pts]
            pl.stroke(leg, BODY, 0.8, 1.2)
    # wings folded over the abdomen, overlapping, with veins
    for side in (1, -1):
        base = P(side * 4, -4)
        wing = catmull([base, P(side * 18, 6), P(side * 22, 40), P(side * 14, 64), P(side * 4, 66), P(side * -2, 50), P(side * 0, 10)], 10, closed=True)
        pl.shape(wing, hexc("#EDE8DC"), GRAY, 0.55, 0.7, 1.0)
        for vx in (6, 11, 15):
            pl.stroke(catmull([base, P(side * vx, 30), P(side * (vx - 2), 60)], 8), GRAY, 0.5, 0.8)
    # abdomen with tergite bands
    ab = ellipse(P(0, 30), 15 * s, 25 * s, 0, 64)
    pl.shape(ab, hexc("#E4CFAF"), BODY, 0.75, 0.8, 1.2)
    for k in range(5):
        y = 14 + k * 8
        band = ellipse(P(0, y + 3), 14 * s * (1 - 0.1 * k), 2.6 * s, 0, 32)
        pl.fill(band, BONE, 0.30)
    # thorax with bristles
    th = ellipse(P(0, -12), 16 * s, 15 * s, 0, 64)
    pl.shape(th, hexc("#D9BE98"), BODY, 1, 0.85, 1.3)
    for x, y in ((-7, -20), (7, -20), (-9, -10), (9, -10), (-4, -2), (4, -2), (-12, -16), (12, -16)):
        q = P(x, y)
        pl.stroke([q, q + np.array([0, 6 * s])], INK, 0.7, 1.0)
    # head and the red compound eyes, antennae with aristae
    hd = ellipse(P(0, -32), 14 * s, 8.5 * s, 0, 48)
    pl.shape(hd, hexc("#D9BE98"), BODY, 1, 0.85, 1.3)
    for side in (1, -1):
        e = ellipse(P(side * 9, -32.5), 6.5 * s, 7.8 * s, side * 0.2, 40)
        pl.shape(e, FLYRED, BODY, 0.75, 0.8, 1.0)
        pl.stipple(e, 60, INK, 0.35, 1.6, seed=seed + side + 5)
        pl.stroke([P(side * 2.5, -39), P(side * 5, -45), P(side * 9, -49)], BODY, 0.8, 0.9)
    return P


def fly_brain(pl, c, sc, detail=True, fill=0.9):
    """The adult fly brain in frontal view (after the JRC2018 template): the central brain round the oesophageal foramen,
    the antennal lobes below the front, each optic lobe with lamina, medulla, lobula and lobula plate. Units: 1 = 1 px at
    sc = 1, where the brain is about 1440 px across (590 um). Returns a mapping function."""
    c = np.asarray(c, float)
    P = lambda x, y: c + np.array([x, y]) * sc
    cb = catmull([P(0, -330), P(150, -350), P(300, -320), P(400, -230), P(430, -60), P(410, 110), P(330, 230), P(210, 300),
                  P(120, 330), P(0, 340), P(-120, 330), P(-210, 300), P(-330, 230), P(-410, 110), P(-430, -60), P(-400, -230),
                  P(-300, -320), P(-150, -350)], 12, closed=True)
    for side in (1, -1):
        # lobula complex next to the central brain, the medulla outside it, the lamina a thin crescent outermost
        lob = ellipse(P(side * 462, 20), 62 * sc, 175 * sc, side * 0.12, 64)
        med = ellipse(P(side * 575, 0), 98 * sc, 262 * sc, side * 0.10, 96)
        ang = np.linspace(-1.25, 1.25, 40)
        outer = np.array([P(side * (575 + 150 * math.cos(t)), 290 * math.sin(t)) for t in ang])
        inner = np.array([P(side * (575 + 108 * math.cos(t)), 268 * math.sin(t)) for t in ang[::-1]])
        lam = np.vstack([outer, inner])
        pl.shape(lam, TISSUE, BONE, fill, 0.6, 1.2)
        if detail:
            pl.hatch(lam, 0.0, 3.5, BONE, 0.22, 0.7)
        pl.shape(med, TISSUE, BONE, fill, 0.65, 1.4)
        if detail:
            for k in range(1, 5):   # medulla layers
                pl.stroke(ellipse(P(side * (575 - 12 * k), 0), (98 - 18 * k) * sc, (262 - 14 * k) * sc, side * 0.10, 96), BONE, 0.16, 0.8, closed=True)
        pl.shape(lob, TISSUE, BONE, fill, 0.6, 1.2)
        if detail:
            pl.stipple(lob, 120, BONE, 0.2, 1.6, seed=3 + side)
    pl.shape(cb, TISSUE, BONE, fill, 0.75, 1.8)
    if detail:
        pl.stipple(cb, 1500, BONE, 0.12, 1.6, seed=8)
    # the oesophageal foramen and the antennal lobes
    pl.shape(ellipse(P(0, 175), 52 * sc, 62 * sc), hexc("#F6F0E2"), BONE, 1, 0.6, 1.2)
    for side in (1, -1):
        al = ellipse(P(side * 120, 170), 84 * sc, 78 * sc)
        pl.shape(al, hexc("#E9DCC6"), BONE, fill, 0.6, 1.2)
        if detail:
            rng = np.random.default_rng(5 + side)
            for k in range(26):
                a, r = rng.uniform(0, 2 * math.pi), math.sqrt(rng.uniform(0, 1)) * 64 * sc
                pl.circle(P(side * 120, 170) + np.array([math.cos(a), math.sin(a)]) * r, 12 * sc, None, BONE, ra=0.35, w=0.8)
    return P, cb


def fly_compass():
    """The Drosophila central complex, schematic frontal view (after Wolff et al. 2015, Hulse et al. 2021): the protocerebral
    bridge (a handlebar of 18 glomeruli, L9 to R9), the fan-shaped body, the ellipsoid body (a ring of 16 wedges, 8 tiles) and
    the paired noduli. E-PG cells carry the bump from an EB wedge to one glomerulus in each half of the bridge; P-EN cells
    return it to the neighbouring tile by way of the noduli; the inhibitory Delta7 cells span the bridge."""
    pl = Plate()
    cx = 1000.0
    # protocerebral bridge: a bow of 18 glomeruli
    pb_ax = Spine(catmull([(585, 345), (690, 268), (840, 228), (1000, 218), (1160, 228), (1310, 268), (1415, 345)], 16), 400)
    pbo = pb_ax.outline(lambda u: 26 * (0.75 + 0.25 * math.sin(math.pi * u)) + 0.5, 0, 1, 200)
    pl.shape(pbo, TISSUE, BONE, 1, 0.8, 1.6)
    pl.stipple(pbo, 500, BONE, 0.16, 1.6, seed=2)
    glom = []
    for k in range(18):
        u0, u1 = k / 18, (k + 1) / 18
        if k:
            pl.stroke([pb_ax.at(u0, -24), pb_ax.at(u0, 24)], BONE, 0.55, 1.0)
        glom.append(pb_ax.at((u0 + u1) / 2))
    # fan-shaped body: layers and columns
    fb = catmull([(760, 372), (870, 336), (1000, 326), (1130, 336), (1240, 372), (1200, 438), (1100, 470), (1000, 478), (900, 470), (800, 438)], 12, closed=True)
    pl.shape(fb, TISSUE, BONE, 1, 0.8, 1.6)
    pl.hatch(fb, 0.0, 9, BONE, 0.16, 0.8)
    for k in range(1, 9):
        f = k / 9
        top = np.array([760 + 480 * f, 372 - 46 * math.sin(math.pi * f)])
        pl.stroke([top, (cx + (top[0] - cx) * 0.80, 470 - 8 * math.sin(math.pi * f))], BONE, 0.30, 0.8)
    # ellipsoid body: a ring of 16 wedges, 8 tiles, with the canal in the middle
    E = np.array([cx, 672.0])
    ro, ri = 178.0, 70.0
    ring = np.vstack([ellipse(E, ro, ro, 0, 128), ellipse(E, ri, ri, 0, 128)[::-1]])
    pl.circle(E, ro, TISSUE, None, 1)
    bump = -math.pi * 0.30   # the bump: centred on wedges up and to the right
    for k in range(16):
        a0 = -math.pi / 2 + 2 * math.pi * k / 16
        a1 = a0 + 2 * math.pi / 16
        am = (a0 + a1) / 2
        d = math.atan2(math.sin(am - bump), math.cos(am - bump))
        act = math.exp(-(d / 0.42) ** 2)
        if act > 0.05:
            ang = np.linspace(a0, a1, 12)
            wedge = np.vstack([np.c_[E[0] + np.cos(ang) * ro, E[1] + np.sin(ang) * ro], np.c_[E[0] + np.cos(ang[::-1]) * ri, E[1] + np.sin(ang[::-1]) * ri]])
            pl.fill(wedge, EXC, 0.55 * act)
    pl.wash(E + np.array([math.cos(bump), math.sin(bump)]) * 125, 120, EXC, 0.25)
    pl.stipple(ring, 900, BONE, 0.16, 1.6, seed=6)
    for k in range(16):
        a = -math.pi / 2 + 2 * math.pi * k / 16
        pl.stroke([E + np.array([math.cos(a), math.sin(a)]) * ri, E + np.array([math.cos(a), math.sin(a)]) * ro], BONE, 0.75 if k % 2 == 0 else 0.30, 1.2 if k % 2 == 0 else 0.8)
    pl.circle(E, ro, None, BONE, ra=0.85, w=1.8)
    pl.circle(E, ri, SHEET, BONE, 0.9, 0.8, 1.4)
    pl.circle(E, (ro + ri) / 2, None, BONE, ra=0.25, w=0.8)
    # noduli
    nod = [np.array([cx + sg * 120, 918.0]) for sg in (-1, 1)]
    for n in nod:
        pl.shape(ellipse(n, 40, 26), TISSUE, BONE, 1, 0.8, 1.4)
        for k in range(1, 3):
            pl.stroke(ellipse(n, 40 - 11 * k, 26 - 7 * k)[8:28], BONE, 0.35, 0.8)
    # E-PG: a wedge of the bump to one glomerulus in each half of the bridge (the bump appears twice in the PB)
    gR, gL = glom[9 + 3], glom[8 - 4]
    for g, a, sg in ((gR, bump - 0.12, 1), (gL, bump + 0.12, -1)):
        st = E + np.array([math.cos(a), math.sin(a)]) * 125
        path = catmull([st, st + np.array([sg * 10, -150]), (cx + sg * 70, 400), g + np.array([0, 70]), g], 14)
        pl.stroke(path, EXC, 0.9, 1.8)
        for k in range(5):
            q = st + np.array([math.cos(a + 0.5 * (k - 2)), math.sin(a + 0.5 * (k - 2))]) * 22
            pl.stroke([st, q], EXC, 0.8, 1.0)
        pl.wash(g, 46, EXC, 0.35)
        pl.circle(g, 12, EXC, None, 0.75)
    # P-EN: from the bridge down through the nodulus to the neighbouring tile (dashed)
    for g, n, a in ((glom[9 + 4], nod[1], bump + 0.55), (glom[8 - 3], nod[0], bump - 0.55)):
        en = E + np.array([math.cos(a), math.sin(a)]) * 140
        sx = cx + (330 if n[0] > cx else -330)
        path = catmull([g, g + np.array([0, 60]), (sx, 520), (sx - (40 if n[0] > cx else -40), 840), n, (n[0] * 0.5 + en[0] * 0.5, 830), en], 14)
        pl.stroke(path, EXC, 0.7, 1.4, dash=[6, 5])
        pl.arrow(path[-6:], EXC, 0.7, 1.4)
    # Delta7: inhibitory, across the whole bridge
    d7 = pb_ax.line(0.02, 0.98, -36, 120)
    pl.stroke(d7, INH, 0.85, 1.6)
    for k in range(0, 18, 2):
        q = pb_ax.at((k + 0.5) / 18, -36)
        pl.stroke([q, pb_ax.at((k + 0.5) / 18, -14)], INH, 0.8, 1.2)
        pl.stroke([pb_ax.at((k + 0.5) / 18, -18) + np.array([-6, 0]), pb_ax.at((k + 0.5) / 18, -18) + np.array([6, 0])], INH, 0.8, 1.6)
    # lettering
    pl.label("protocerebral bridge", (540, 420), SOFT, 14, "r", leader=tuple(pb_ax.at(0.05, 10)))
    pl.label("L9", (pb_ax.at(0.02)[0] - 20, pb_ax.at(0.02)[1] + 50), SOFT, 12, "c", slip=0)
    pl.label("R9", (pb_ax.at(0.98)[0] + 20, pb_ax.at(0.98)[1] + 50), SOFT, 12, "c", slip=0)
    pl.label("L1 | R1", (cx, 196), SOFT, 12, "c", slip=0)
    pl.label("fan-shaped body", (1290, 420), SOFT, 14, leader=(1215, 405))
    pl.label("ellipsoid body: 16 wedges, 8 tiles", (1240, 700), SOFT, 14, leader=tuple(E + np.array([ro * 0.96, 30])))
    pl.label("the bump", (1300, 560), EXC_INK, 14, leader=tuple(E + np.array([math.cos(bump), math.sin(bump)]) * 120))
    pl.label("noduli", (cx, 990), SOFT, 14, "c", leader=(cx + 80, 925))
    pl.label("E-PG", (cx - 150, 560), EXC_INK, 14, "r", leader=tuple(catmull([E + np.array([math.cos(bump + 0.12), math.sin(bump + 0.12)]) * 125, E + np.array([math.cos(bump + 0.12), math.sin(bump + 0.12)]) * 125 + np.array([-10, -150]), (cx - 70, 400)], 14)[14]))
    pl.label("P-EN", (cx - 390, 760), EXC_INK, 14, "r", leader=(cx - 352, 720))
    pl.label("Delta7 cells, inhibitory", (560, 240), INH, 14, "r", leader=tuple(pb_ax.at(0.06, -36)))
    # Fig. 2: the fly; Fig. 3: where the central complex sits in the brain
    fig(pl, 90, 110, 2, "Drosophila melanogaster.")
    fly_dorsal(pl, (250, 330), 300)
    fig(pl, 1450, 110, 3, "The brain from the front.")
    Pb, _ = fly_brain(pl, (1660, 290), 0.27, detail=False)
    box = [Pb(-120, -230), Pb(120, -230), Pb(120, 10), Pb(-120, 10)]
    pl.stroke(box, INK, 0.8, 1.2, closed=True)
    pl.fill(ellipse(Pb(0, -95), 22, 18), EXC, 0.5)
    pl.stroke([Pb(-30, -185), Pb(30, -185)], EXC, 0.8, 2.0)
    # Fig. 4: heading and bump in the dark
    fx, fy = 1360, 960
    fig(pl, fx, fy, 4, "Bump and heading, in the dark.", 12)
    t = np.linspace(0, 1, 300)
    head = 0.5 + 0.32 * np.sin(2 * math.pi * 1.3 * t) * np.exp(-t * 0.6) + 0.1 * np.sin(2 * math.pi * 3.1 * t)
    bmp = head + 0.05 * np.sin(2 * math.pi * 0.7 * t) + 0.06 * t
    trace(pl, fx + 20, fy + 20, 500, 110, head, INK, 0.8, 1.2)
    trace(pl, fx + 20, fy + 20, 500, 110, bmp, EXC, 0.85, 1.6, base=False)
    pl.label("heading", (fx + 530, fy + 20 + 110 - head[-1] * 110 + 30), INK, 12, slip=0)
    pl.label("bump", (fx + 530, fy + 20 + 110 - bmp[-1] * 110 - 6), EXC_INK, 12, slip=0)
    scale_bar(pl, 1880, 1170, 125, "20 µm")
    pl.keep_rect(560, 190, 880, 760, 25)
    return pl


def fly_mb():
    """The Drosophila brain from the front with both mushroom bodies (after Aso et al. 2014): Kenyon-cell bodies over the
    calyx, the peduncle running forward and down to the heel, the vertical (alpha, alpha') and horizontal (beta, beta',
    gamma) lobes with their compartments; projection neurons from the antennal lobe to the calyx and the lateral horn; the
    inhibitory APL cell that spans the whole mushroom body; dopamine neurons (PPL1 on the vertical lobe, PAM on the
    horizontal lobe) and an output neuron."""
    pl = Plate()
    P, cb = fly_brain(pl, (1000, 690), 1.0)
    KCF = hexc("#EBC3A6")
    lab = {}
    for sg in (1, -1):
        X = lambda x, y: P(sg * x, y)
        # projection neurons: antennal lobe -> calyx -> lateral horn (verdigris)
        pn = catmull([X(120, 170), X(150, 60), X(230, -70), X(285, -180), X(340, -150), X(392, -110)], 12)
        pl.stroke(pn, SENSOR, 0.85, 2.0)
        lh = ellipse(X(392, -110), 46, 62, sg * 0.3)
        pl.shape(lh, SENSOR, SENSOR, 0.16, 0.7, 1.2)
        # Kenyon-cell bodies: a cap of small cells over the calyx
        rng = np.random.default_rng(20 + sg)
        for k in range(110):
            a, r = rng.uniform(0, 2 * math.pi), math.sqrt(rng.uniform(0, 1))
            q = X(290, -262) + np.array([math.cos(a) * 92 * r, math.sin(a) * 52 * r])
            pl.circle(q, 6.5, KCF, EXC, 0.9, 0.45, 0.8)
        # calyx
        cal = ellipse(X(285, -180), 78, 48, sg * -0.15)
        pl.shape(cal, EXC, EXC, 0.38, 0.85, 1.5)
        pl.stipple(cal, 160, EXC, 0.45, 2.4, seed=30 + sg)
        # peduncle, vertical lobe, horizontal lobes
        ped = Spine(catmull([X(272, -150), X(240, -50), X(195, 40)], 10), 60)
        po = ped.outline(lambda u: 15, 0, 1, 40)
        vl = Spine(catmull([X(192, 40), X(178, -80), X(168, -200), X(172, -262)], 10), 80)
        vo = vl.outline(lambda u: 19 * (1 - 0.15 * u) * math.sqrt(max(0.0, 1 - max(0.0, (u - 0.9) / 0.1) ** 2)) + 0.5, 0, 1, 60)
        hl = Spine(catmull([X(206, 48), X(120, 56), X(26, 58)], 10), 80)
        ho = hl.outline(lambda u: 24 * math.sqrt(max(0.0, 1 - max(0.0, (u - 0.92) / 0.08) ** 2)) + 0.5, 0, 1, 60)
        for o in (po, vo, ho):
            pl.shape(o, KCF, EXC, 0.95, 0.85, 1.5)
        pl.circle(X(196, 44), 24, KCF, None, 0.95)
        pl.stroke(vl.line(0.05, 0.95, 0, 30), EXC, 0.4, 0.8)
        pl.stroke(hl.line(0.05, 0.95, -8, 30), EXC, 0.4, 0.8)
        pl.stroke(hl.line(0.05, 0.95, 8, 30), EXC, 0.4, 0.8)
        for u in (0.36, 0.68):   # compartments
            pl.stroke([vl.at(u, -19), vl.at(u, 19)], EXC, 0.6, 1.0)
        for u in (0.25, 0.5, 0.75):
            pl.stroke([hl.at(u, -24), hl.at(u, 24)], EXC, 0.6, 1.0)
        # APL: one inhibitory cell over the whole mushroom body (Prussian blue)
        apl_s = X(372, 30)
        pl.stroke(catmull([apl_s, X(300, 10), ped.at(0.6, sg * 20), X(215, 58)], 10), INH, 0.85, 1.4)
        pl.stroke(vl.line(0.05, 0.9, sg * -24, 30), INH, 0.7, 1.0)
        pl.stroke(hl.line(0.05, 0.9, 29, 30), INH, 0.7, 1.0)
        pl.stroke(catmull([ped.at(0.2, sg * 20), X(330, -150), cal[10]], 8), INH, 0.6, 1.0)
        pl.circle(apl_s, 10, INH, INK, 0.9, 0.7, 1.0)
        lab[sg] = dict(cal=cal, vl=vl, hl=hl, ped=ped, lh=X(392, -110), apl=apl_s, pn=pn)
    # one side in more detail: three Kenyon cells, dopamine neurons, an output neuron
    R = lab[1]
    X = lambda x, y: P(x, y)
    for k, (sx, sy) in enumerate(((250, -290), (300, -278), (330, -258))):
        kc = catmull([X(sx, sy), X(290 - 10 * k, -190), R["ped"].at(0.5, -6 + 6 * k), X(196, 44)], 10)
        pl.stroke(kc, EXC, 0.9, 1.0)
        pl.stroke(R["vl"].line(0, 0.97, -8 + 7 * k, 30), EXC, 0.75, 0.9)
        pl.stroke(R["hl"].line(0, 0.97, -12 + 10 * k, 30), EXC, 0.75, 0.9)
        pl.circle(X(sx, sy), 7, EXC, INK, 1, 0.8, 1.0)
    ppl1 = X(250, -330)
    pl.stroke(catmull([ppl1, X(215, -300), R["vl"].at(0.84, 6)], 8), GAMBOGE, 0.95, 1.6)
    pam = [X(70 + 14 * k, 140 - 8 * k) for k in range(4)]
    for k, q in enumerate(pam):
        pl.stroke(catmull([q, q + np.array([0, -40]), R["hl"].at(0.35 + 0.15 * k, 20)], 8), GAMBOGE, 0.85, 1.2)
        pl.circle(q, 7.5, GAMBOGE, INK, 0.95, 0.7, 0.9)
    pl.circle(ppl1, 9, GAMBOGE, INK, 0.95, 0.7, 1.0)
    mbon = catmull([R["vl"].at(0.9, -10), X(130, -300), X(60, -320)], 8)
    pl.arrow(mbon, EXC, 0.9, 1.8)
    # lettering (right hemisphere)
    pl.label("Kenyon cells", (1500, 300), EXC_INK, 14, leader=tuple(X(360, -285)))
    pl.label("calyx", (1560, 390), EXC_INK, 14, leader=tuple(X(355, -185)))
    pl.label("lateral horn", (1600, 470), SENSOR, 14, leader=tuple(X(425, -120)))
    pl.label("projection neurons", (1500, 1060), SENSOR, 14, leader=tuple(R["pn"][10]))
    pl.label("antennal lobe", (1250, 1040), SOFT, 14, leader=tuple(X(170, 230)))
    pl.label("peduncle", (1300, 560), EXC_INK, 14, leader=tuple(R["ped"].at(0.5, 12)))
    pl.label("vertical lobe (alpha, alpha')", (1100, 280), EXC_INK, 14, leader=tuple(R["vl"].at(0.97)))
    pl.label("horizontal lobes (beta, beta', gamma)", (1110, 680), EXC_INK, 14, leader=tuple(R["hl"].at(0.3, 22)))
    pl.label("APL, inhibitory", (1330, 640), INH, 14, leader=tuple(R["apl"]))
    pl.label("dopamine cells: PPL1, PAM", (1110, 940), hexc("#8A6410"), 14, leader=tuple(pam[0]))
    pl.label("output neuron", (900, 330), EXC_INK, 14, "r", leader=tuple(X(60, -320)))
    pl.label("lobula", (1430, 1000), SOFT, 14, leader=tuple(X(470, 170)))
    pl.label("medulla", (1590, 960), SOFT, 14, leader=tuple(X(575, 220)))
    pl.label("lamina", (1760, 900), SOFT, 14, leader=tuple(X(700, 160)))
    pl.label("oesophagus", (1000, 905), SOFT, 12, "c", slip=0)
    # Fig. 2: the fly
    fig(pl, 90, 110, 2, "The fly, from above.")
    fly_dorsal(pl, (210, 290), 210)
    pl.label("Drosophila melanogaster", (1380, 150), INK, 17, slip=0)
    pl.label("brain from the front; both mushroom bodies stained", (1380, 150 + 30 * S), SOFT, 13, slip=0)
    scale_bar(pl, 1880, 1170, 244, "100 µm")
    pl.keep_rect(270, 330, 1460, 700, 20)
    return pl


# ================================================================ THEORY

def axes(pl, x0, y0, w, h, xl, yl, xt=(), yt=(), origin=None):
    """Engraved axes in the box (x0, y0, w, h): an L of ink with arrowheads, ticks, italic axis letters."""
    ox, oy = origin if origin is not None else (x0, y0 + h)
    pl.arrow([(x0, oy), (x0 + w, oy)], INK, 0.85, 1.4, 8)
    pl.arrow([(ox, y0 + h), (ox, y0)], INK, 0.85, 1.4, 8)
    pl.label(xl, (x0 + w - 4, oy + 34), INK, 15, "r", slip=0)
    pl.label(yl, (ox - 14, y0 + 14), INK, 15, "r", slip=0)
    for (x, t) in xt:
        pl.stroke([(x, oy), (x, oy + 8)], INK, 0.8, 1.0)
        if t:
            pl.label(t, (x, oy + 34), SOFT, 12, "c", slip=0)
    for (y, t) in yt:
        pl.stroke([(ox, y), (ox - 8, y)], INK, 0.8, 1.0)
        if t:
            pl.label(t, (ox - 14, y + 8), SOFT, 12, "r", slip=0)


def arrows_along(pl, pts, col, every=0.25, a=0.9, w=1.4, head=8.0):
    """Small arrowheads along a trajectory (the direction of flow)."""
    pts = np.asarray(pts, float)
    d = np.r_[0, np.cumsum(np.hypot(*np.diff(pts, axis=0).T))]
    for f in np.arange(every / 2, 1.0, every):
        i = int(np.searchsorted(d, f * d[-1]))
        i = min(max(i, 2), len(pts) - 1)
        pl.arrow(pts[i - 2:i + 1], col, a, w, head)


def fhn(I, v0, w0, T, dt=0.01, a=0.7, b=0.8, eps=0.25):
    v, w = v0, w0
    out = []
    for _ in range(int(T / dt)):
        def f(v, w):
            return v - v ** 3 / 3 - w + I, eps * (v + a - b * w)
        k1 = f(v, w)
        k2 = f(v + dt / 2 * k1[0], w + dt / 2 * k1[1])
        k3 = f(v + dt / 2 * k2[0], w + dt / 2 * k2[1])
        k4 = f(v + dt * k3[0], w + dt * k3[1])
        v += dt / 6 * (k1[0] + 2 * k2[0] + 2 * k3[0] + k4[0])
        w += dt / 6 * (k1[1] + 2 * k2[1] + 2 * k3[1] + k4[1])
        out.append((v, w))
    return np.array(out)


def fhn_fp(I, a=0.7, b=0.8):
    # v - v^3/3 - (v + a)/b + I = 0
    r = np.roots([-1 / 3, 0, 1 - 1 / b, I - a / b])
    v = float(np.real(r[np.argmin(np.abs(np.imag(r)))]))
    return v, (v + a) / b


def dynamics():
    """Two phase planes of the FitzHugh-Nagumo model (v' = v - v^3/3 - w + I, w' = 0.25 (v + 0.7 - 0.8 w)), computed: below the
    Hopf bifurcation (I = 0.30) a trajectory spirals into the stable fixed point; above it (I = 0.60) the fixed point is
    unstable and every trajectory winds onto the limit cycle. The cubic v-nullcline in sienna, the straight w-nullcline in
    Prussian blue, a light direction field. Under them, v(t) for both: a damped ring and a train of spikes."""
    pl = Plate()
    vr, wr = (-2.4, 2.4), (-0.9, 1.9)
    panels = ((0.30, 120, "Fig. 1.", "Below the bifurcation: a stable focus."), (0.60, 1100, "Fig. 2.", "Above it: a limit cycle."))
    traces = []
    for I, x0, fnum, cap in panels:
        y0, w, h = 150.0, 780.0, 520.0
        M = lambda v, ww: np.array([x0 + 40 + (v - vr[0]) / (vr[1] - vr[0]) * (w - 60), y0 + h - (ww - wr[0]) / (wr[1] - wr[0]) * h])
        pl.label(fnum, (x0, 118), INK, 13, slip=0, bold=True)
        pl.label(cap, (x0 + 62, 118), SOFT, 13, slip=0)
        # direction field
        for v in np.linspace(vr[0] + 0.2, vr[1] - 0.2, 17):
            for ww in np.linspace(wr[0] + 0.15, wr[1] - 0.15, 11):
                dv, dw = v - v ** 3 / 3 - ww + I, 0.25 * (v + 0.7 - 0.8 * ww)
                d = np.array([dv / (vr[1] - vr[0]), dw / (wr[1] - wr[0]) * 3])
                d = d / max(np.hypot(*d), 1e-9)
                p = M(v, ww)
                q = p + np.array([d[0], -d[1]]) * 16
                pl.stroke([p, q], INK, 0.22, 1.0)
                pl.circle(p, 1.6, INK, None, 0.25)
        vv = np.linspace(vr[0], vr[1], 200)
        cub = np.array([M(v, v - v ** 3 / 3 + I) for v in vv])
        cub = cub[(cub[:, 1] > y0) & (cub[:, 1] < y0 + h)]
        pl.stroke(cub, EXC, 0.9, 2.6)
        lin = np.array([M(v, (v + 0.7) / 0.8) for v in vv])
        lin = lin[(lin[:, 1] > y0) & (lin[:, 1] < y0 + h)]
        pl.stroke(lin, INH, 0.9, 2.4)
        fv, fw = fhn_fp(I)
        if I < 0.33:
            tr = fhn(I, fv + 0.62, fw + 0.32, 200)
            P = np.array([M(*q) for q in tr])
            pl.stroke(P[:6000], INK, 0.9, 1.6)
            arrows_along(pl, P[:2500], INK, 0.25)
            pl.circle(P[0], 5, INK, None, 0.9)
            pl.circle(M(fv, fw), 9, INK, None, 1)
            traces.append(tr[:, 0])
            pl.label("stable focus", tuple(M(fv, fw) + np.array([-24, 46])), INK, 13, "r")
        else:
            cyc = fhn(I, 2.0, 0.5, 120)[-4000:]
            P = np.array([M(*q) for q in cyc])
            pl.stroke(P, INK, 0.9, 2.2)
            arrows_along(pl, P, INK, 0.25)
            ins = fhn(I, fv + 0.05, fw, 60)
            Q = np.array([M(*q) for q in ins])
            pl.stroke(Q[:2000], INK, 0.6, 1.2, dash=[4, 4])
            pl.circle(M(fv, fw), 9, SHEET, INK, 1, 1, 1.6)
            traces.append(fhn(I, fv + 0.62, fw + 0.32, 200)[:, 0])
            pl.label("limit cycle", tuple(M(1.6, 1.5)), INK, 13)
            pl.label("unstable fixed point", tuple(M(fv, fw) + np.array([-24, 46])), INK, 13, "r")
        axes(pl, x0 + 40, y0, w - 40, h, "v", "w", origin=(M(0, 0)[0], M(0, wr[0])[1]))
        pl.label("v-nullcline", tuple(M(1.95, 1.95 - 1.95 ** 3 / 3 + I) + np.array([10, -18])), EXC_INK, 13, leader=tuple(M(1.8, 1.8 - 1.8 ** 3 / 3 + I)))
        pl.label("w-nullcline", tuple(M(1.0, 2.1) + np.array([24, 0])), INH, 13)
    # Fig. 3: v(t) for both, as the pen draws it
    bx, by, bw = 680, 850, 640
    pl.label("Fig. 3.", (bx - 40, by - 28), INK, 13, slip=0, bold=True)
    pl.label("The voltage v(t) in both.", (bx + 22, by - 28), SOFT, 13, slip=0)
    for k, v in enumerate(traces):
        v = np.asarray(v)[:8000:4]
        y = by + 10 + k * 120
        trace(pl, bx, y, bw, 95, (v + 2.2) / 4.4, INK, 0.9, 1.4)
        pl.label(f"I = {panels[k][0]:.2f}", (bx - 14, y + 70), SOFT, 12, "r", slip=0)
    pl.keep_rect(120, 100, 1760, 620, 25)
    pl.keep_rect(bx, by - 50, bw, 280, 25)
    return pl


def wc(P, T=40.0, dt=0.01, E0=0.0, I0=0.0):
    def S(x, a, th):
        return 1 / (1 + math.exp(-a * (x - th))) - 1 / (1 + math.exp(a * th))
    E, I = E0, I0
    out = []
    for _ in range(int(T / dt)):
        dE = -E + (1 - E) * S(16 * E - 12 * I + P, 1.3, 4)
        dI = -I + (1 - I) * S(15 * E - 3 * I, 2, 3.7)
        E += dt * dE
        I += dt * dI
        out.append((E, I))
    return np.array(out)


def rates():
    """The Wilson-Cowan excitatory-inhibitory pair (1972 parameters: c1..c4 = 16, 12, 15, 3; sigmoids 1.3/4 and 2/3.7),
    computed: with a strong drive (P = 2.5) the pair rings and settles, with P = 1.25 it oscillates without end. Below, the
    pair itself: E excites itself and I, I inhibits E and itself."""
    pl = Plate()
    for k, (P, x0, cap) in enumerate(((2.5, 120, "A damped oscillation (P = 2.5)."), (1.25, 1100, "A sustained oscillation (P = 1.25)."))):
        y0, w, h = 160.0, 780.0, 400.0
        pl.label(f"Fig. {k + 1}.", (x0, 118), INK, 13, slip=0, bold=True)
        pl.label(cap, (x0 + 62, 118), SOFT, 13, slip=0)
        o = wc(P, 40.0)
        E, I = o[::10, 0], o[::10, 1]
        top = 0.5
        xs = x0 + 40 + np.linspace(0, w - 60, len(E))
        for v, col, wd in ((I, INH, 2.2), (E, EXC, 2.6)):
            ys = y0 + h - v / top * h
            pl.stroke(np.c_[xs, ys], col, 0.92, wd)
        axes(pl, x0 + 40, y0, w - 40, h, "t", "rate", xt=[(x0 + 40 + (w - 60) * f, f"{int(40 * f)}") for f in (0, 0.25, 0.5, 0.75)],
             yt=[(y0 + h - h * f / top, f"{f:.1f}") for f in (0.25,)])
        iE = int(len(E) * 0.10)
        pl.label("E", (xs[iE] + 10, y0 + h - E[iE] / top * h - 16), EXC_INK, 15, slip=0.55)
        iI = int(len(I) * 0.16)
        pl.label("I", (xs[iI] + 10, y0 + h - I[iI] / top * h + 34), INH, 15, slip=0.55)
    # Fig. 3: the pair
    cE, cI = np.array([810.0, 880.0]), np.array([1190.0, 880.0])
    pl.label("Fig. 3.", (700, 700), INK, 13, slip=0, bold=True)
    pl.label("The excitatory-inhibitory pair.", (762, 700), SOFT, 13, slip=0)
    r = 64.0
    for c, col, nm in ((cE, EXC, "E"), (cI, INH, "I")):
        pl.wash(c, r * 2.0, col, 0.20)
        pl.circle(c, r, col, INK, 0.85, 0.85, 1.8)
        pl.circle(c - np.array([16, 18]), 8, SHEET, None, 0.35)
        pl.label(nm, (c[0], c[1] + 12), hexc("#FBF7EE"), 22, "c", slip=0, bold=True)
    wire(pl, cE, cI, EXC, "arrow", bend=0.28, r0=r, r1=r, w=2.2)
    wire(pl, cI, cE, INH, "bar", bend=0.28, r0=r, r1=r, w=2.2)
    for c, col, kind, sg in ((cE, EXC, "arrow", -1), (cI, INH, "bar", 1)):
        loop = catmull([c + np.array([sg * r * 0.7, -r * 0.7]), c + np.array([sg * r * 2.0, -r * 1.3]), c + np.array([sg * r * 2.3, 0]),
                        c + np.array([sg * r * 2.0, r * 1.3]), c + np.array([sg * (r + 8) * 0.72, r * 0.72])], 12)
        pl.arrow(loop, col, 0.9, 2.0, 10, kind=kind)
    pl.arrow([cE + np.array([-r * 2.3, -r * 2.2]), cE + np.array([-r * 0.75, -r * 0.75])], INK, 0.85, 1.6, 10)
    pl.label("drive P", tuple(cE + np.array([-r * 2.4, -r * 2.3])), INK, 13, "r", slip=0)
    pl.label("c1", tuple(cE + np.array([-r * 2.6, 8])), EXC_INK, 13, "r", slip=0)
    pl.label("c2", ((cE[0] + cI[0]) / 2, cE[1] + 128), INH, 13, "c", slip=0)
    pl.label("c3", ((cE[0] + cI[0]) / 2, cE[1] - 108), EXC_INK, 13, "c", slip=0)
    pl.label("c4", tuple(cI + np.array([r * 2.6, 8])), INH, 13, slip=0)
    pl.keep_rect(120, 100, 1760, 520, 25)
    pl.keep_rect(640, 660, 720, 380, 25)
    return pl


def _pat_ring(n=8):
    c = (n - 1) / 2
    return np.array([[1 if 1.9 <= math.hypot(i - c, j - c) <= 3.6 else 0 for j in range(n)] for i in range(n)])


def _pat_cross(n=8):
    return np.array([[1 if (i == j or i + j == n - 1) else 0 for j in range(n)] for i in range(n)])


def grid(pl, x0, y0, cell, pat, known=None, r=None):
    n = pat.shape[0]
    r = r or cell * 0.36
    for i in range(n):
        for j in range(n):
            c = (x0 + (j + 0.5) * cell, y0 + (i + 0.5) * cell)
            if known is not None and not known[i, j]:
                pl.circle(c, r, None, INK, ra=0.25, w=0.8)
                pl.stipple_disc(c, r * 0.8, 10, INK, 0.3, 1.4, seed=i * n + j)
            elif pat[i, j]:
                pl.wash(c, r * 1.8, EXC, 0.18)
                pl.circle(c, r, EXC, INK, 0.9, 0.7, 1.2)
            else:
                pl.circle(c, r, SHEET, INK, 0.9, 0.45, 1.0)


def memory():
    """Attractor memory after Hopfield (1982): two patterns stored in one network of 64 cells (a ring and a cross), a cue that
    gives only half of the ring, and a cross-section of the energy landscape: each stored pattern is a well, the barrier
    between them keeps them apart, a state in a valley rolls down to the memory at its bottom."""
    pl = Plate()
    A, B = _pat_ring(), _pat_cross()
    cell = 44
    gx = [170, 2000 - 170 - 8 * cell]
    for k, (pat, x) in enumerate(((A, gx[0]), (B, gx[1]))):
        pl.label(f"Fig. {k + 1}.", (x, 118), INK, 13, slip=0, bold=True)
        pl.label(f"Stored pattern {k + 1}.", (x + 62, 118), SOFT, 13, slip=0)
        pl.stroke([(x - 10, 140), (x + 8 * cell + 10, 140), (x + 8 * cell + 10, 140 + 8 * cell + 20), (x - 10, 140 + 8 * cell + 20)], INK, 0.5, 1.0, closed=True)
        grid(pl, x, 150, cell, pat)
    # the cue: the top half of pattern 1
    known = np.zeros_like(A)
    known[:4, :] = 1
    cc = 30
    cx0 = 1000 - 4 * cc
    pl.label("Fig. 3.", (cx0 - 40, 118), INK, 13, slip=0, bold=True)
    pl.label("A cue: half of pattern 1.", (cx0 + 22, 118), SOFT, 13, slip=0)
    grid(pl, cx0, 150, cc, A, known)
    pl.arrow(catmull([(cx0 - 20, 260), (720, 250), (gx[0] + 8 * cell + 30, 280)], 12), INK, 0.7, 1.4, 10, dash=[6, 5])
    pl.label("recall", (720, 236), SOFT, 13, "c", slip=0)
    # Fig. 4: the energy landscape in cross-section
    X = np.linspace(110, 1890, 400)
    def Ef(x):
        u = (x - 1000) / 600.0   # wells at x = 400 and 1600, u = -1 and 1
        return 800 + 220 * (1 - (u ** 2 - 1) ** 2) * (abs(u) < 1.62) + 0 * u
    def Ey(x):
        u = (x - 1000) / 600.0
        return 1010 - 230 * (u ** 2 - 1) ** 2 / (1 + 0.15 * (u ** 2 - 1) ** 2) - 0 * u
    Y = np.array([Ey(x) for x in X])
    Y = np.minimum(Y, 1010)
    ground = np.vstack([np.c_[X, Y], [(1890, 1120), (110, 1120)]])
    pl.fill(ground, TISSUE, 0.9)
    pl.hatch(ground, 0.8, 7, BONE, 0.28, 0.9)
    pl.stroke(np.c_[X, Y], INK, 0.9, 2.0)
    pl.label("Fig. 4.", (110, 640), INK, 13, slip=0, bold=True)
    pl.label("Energy along a line through the two patterns.", (172, 640), SOFT, 13, slip=0)
    _ = Ef
    # the state as a ball rolling into the nearer well
    bx = 700.0
    by = Ey(bx)
    t = np.array([1.0, (Ey(bx + 1) - Ey(bx - 1)) / 2])
    t = t / np.hypot(*t)
    nn = np.array([t[1], -t[0]])
    ball = np.array([bx, by]) + nn * 22
    pl.circle(ball, 20, INK, None, 0.85)
    pl.circle(ball - np.array([6, 7]), 5, SHEET, None, 0.5)
    pl.arrow([ball + t * -26 + nn * 34, ball + t * -110 + nn * 34 + np.array([0, 30])], INK, 0.8, 1.6, 10)
    pl.label("the state", (bx + 40, by - 50), INK, 13, slip=0.55)
    for k, (x, pat) in enumerate(((400, A), (1600, B))):
        small = 15
        gx0 = x - 4 * small
        gy0 = Ey(x) + 34
        pl.fill([(gx0 - 12, gy0 - 12), (gx0 + 8 * small + 12, gy0 - 12), (gx0 + 8 * small + 12, gy0 + 8 * small + 12), (gx0 - 12, gy0 + 8 * small + 12)], SHEET, 1)
        pl.stroke([(gx0 - 12, gy0 - 12), (gx0 + 8 * small + 12, gy0 - 12), (gx0 + 8 * small + 12, gy0 + 8 * small + 12), (gx0 - 12, gy0 + 8 * small + 12)], INK, 0.6, 1.0, closed=True)
        grid(pl, gx0, gy0, small, pat, r=small * 0.34)
        pl.label(f"pattern {k + 1}", (x + 4 * small + 24, gy0 + 4 * small + 8), INK, 13, slip=0.7)
    pl.label("barrier", (1000, Ey(1000) - 26), SOFT, 13, "c")
    pl.arrow([(80, 1110), (80, 700)], INK, 0.85, 1.4, 8)
    pl.label("energy", (96, 716), INK, 15, slip=0)
    pl.keep_rect(110, 100, 1780, 420, 25)
    pl.keep_rect(110, 780, 1780, 420, 20)
    return pl


def tuning():
    """Orientation and direction tuning: eight cells with bell-shaped (von Mises, kappa 2.5) tuning curves, preferred
    directions every 45 degrees. On a line (Fig. 1), their spikes for one stimulus (Fig. 2), and around the circle (Fig. 3):
    each cell's vote along its preferred direction, weighted by its rate, and the population vector they add up to."""
    pl = Plate()
    N, kap = 8, 2.5
    pref = np.arange(N) * 2 * math.pi / N
    f = lambda th, k: np.exp(kap * (np.cos(th - pref[k]) - 1))
    stim = math.radians(70)
    # Fig. 1: tuning curves on a line
    x0, y0, w, h = 160.0, 160.0, 720.0, 360.0
    pl.label("Fig. 1.", (120, 118), INK, 13, slip=0, bold=True)
    pl.label("Eight tuning curves.", (182, 118), SOFT, 13, slip=0)
    th = np.linspace(0, 2 * math.pi, 300)
    for k in range(N):
        y = f(th, k)
        pts = np.c_[x0 + th / (2 * math.pi) * w, y0 + h - y * h * 0.9]
        hi = (k == 2)
        pl.stroke(pts, EXC if hi else INK, 0.95 if hi else 0.55, 2.2 if hi else 1.2)
    sx = x0 + stim / (2 * math.pi) * w
    pl.stroke([(sx, y0 + h), (sx, y0 - 10)], INK, 0.7, 1.2, dash=[5, 4])
    for k in range(N):
        pl.circle((sx, y0 + h - f(stim, k) * h * 0.9), 5, EXC if k == 2 else INK, None, 0.9)
    axes(pl, x0, y0, w + 30, h, "direction", "rate", xt=[(x0 + w * q, d) for q, d in ((0, "0"), (0.25, "90"), (0.5, "180"), (0.75, "270"), (1.0, "360"))])
    pl.label("stimulus", (sx + 8, y0 - 16), INK, 13, slip=0)
    # Fig. 2: spikes of the eight cells to that stimulus
    rx0, ry0, rw = 1180.0, 170.0, 680.0
    pl.label("Fig. 2.", (1120, 118), INK, 13, slip=0, bold=True)
    pl.label("Their spikes for the stimulus at 70 degrees.", (1182, 118), SOFT, 13, slip=0)
    rng = np.random.default_rng(4)
    for k in range(N):
        y = ry0 + k * 46
        n = rng.poisson(2 + 34 * f(stim, k))
        ts = np.sort(rng.uniform(0, 1, n))
        for t in ts:
            pl.stroke([(rx0 + t * rw, y), (rx0 + t * rw, y + 30)], EXC if k == 2 else INK, 0.9, 1.4)
        pl.label(f"{k * 45}", (rx0 - 16, y + 24), SOFT, 12, "r", slip=0)
    pl.stroke([(rx0, ry0 + N * 46), (rx0 + rw, ry0 + N * 46)], INK, 0.6, 1.0)
    # Fig. 3: around the circle, with the population vector
    C = np.array([1000.0, 840.0])
    R0, A = 120.0, 150.0
    pl.label("Fig. 3.", (C[0] - R0 - A - 330, 700), INK, 13, slip=0, bold=True)
    pl.label("Around the circle: each cell", (C[0] - R0 - A - 268, 700), SOFT, 13, slip=0)
    pl.label("votes for its preferred direction.", (C[0] - R0 - A - 268, 700 + 26 * S), SOFT, 13, slip=0)
    for k in range(N):
        th = np.linspace(0, 2 * math.pi, 240)
        rr = R0 + A * f(th, k)
        pts = np.c_[C[0] + np.cos(th) * rr, C[1] - np.sin(th) * rr]
        hi = (k == 2)
        lobe = np.c_[C[0] + np.cos(th) * rr, C[1] - np.sin(th) * rr]
        if hi:
            pl.fill(np.vstack([lobe, np.c_[C[0] + np.cos(th[::-1]) * R0, C[1] - np.sin(th[::-1]) * R0]]), EXC, 0.12)
        pl.stroke(pts, EXC if hi else INK, 0.9 if hi else 0.45, 2.0 if hi else 1.0, closed=True)
    pl.circle(C, R0, SHEET, INK, 0.92, 0.85, 1.6)
    # votes and their sum
    vec = np.zeros(2)
    for k in range(N):
        r = f(stim, k)
        d = np.array([math.cos(pref[k]), -math.sin(pref[k])])
        vec += r * d
        if r > 0.03:
            pl.arrow([C, C + d * r * R0 * 0.95], INK, 0.75, 1.3, 7)
        if k % 2 == 0:
            pl.label(f"{k * 45}", tuple(C + d * (R0 - 24) + np.array([0, 7])), SOFT, 11, "c", slip=0)
    sd = np.array([math.cos(stim), -math.sin(stim)])
    pl.stroke([C, C + sd * (R0 + A + 30)], INK, 0.6, 1.2, dash=[5, 4])
    pv = vec / np.hypot(*vec) * (R0 + A * 0.75)
    pl.arrow([C, C + pv], EXC, 0.95, 3.4, 16)
    pl.label("population vector", tuple(C + pv + np.array([22, 0])), EXC_INK, 13)
    pl.label("stimulus", tuple(C + sd * (R0 + A + 40) + np.array([-16, 0])), INK, 13, "r", slip=0.55)
    pl.keep_rect(120, 100, 1760, 460, 25)
    pl.keep_rect(C[0] - R0 - A - 20, 580, 2 * (R0 + A + 20), 560, 25)
    return pl


PLATES = {"worm": worm, "leech": leech, "aplysia": aplysia, "crab": crab, "fly_compass": fly_compass, "fly_mb": fly_mb, "dynamics": dynamics, "rates": rates, "memory": memory, "tuning": tuning}


# ================================================================ output

def write(ids):
    OUT.mkdir(parents=True, exist_ok=True)
    man_path = OUT / "plates.json"
    man = json.loads(man_path.read_text()) if man_path.exists() else {}
    for pid in ids:
        pl = PLATES[pid]()
        im = pl.image()
        im.save(OUT / f"{pid}.png", optimize=True)
        man[pid] = {"file": f"res://assets/plates/{pid}.png", "size": [W, H],
                    "pins": {lv: list(PINS[i]) for i, lv in enumerate(LEVELS[pid])},
                    "keep": [[round(k[0] / W, 4), round(k[1] / H, 4), round(k[2] / W, 4), round(k[3] / H, 4), k[4]] for k in pl.keep]}
        print("wrote", OUT / f"{pid}.png")
    man = {k: man[k] for k in sorted(man)}
    man_path.write_text(json.dumps(man, indent=1) + "\n")


def contact(path, ids, body):
    """Each plate on the map's sheet and frame at the 1080p canvas size, with mock pins at the planned spots, next to crops
    of the procedural body plates."""
    from PIL import Image, ImageDraw
    cw, chh = 1248, 777
    tiles = []
    for b in body:
        im = Image.open(b).convert("RGB").crop((55, 273, 55 + cw, 273 + chh))
        tiles.append(im)
    for pid in ids:
        art = Image.open(OUT / f"{pid}.png").resize((cw, chh), Image.LANCZOS)
        t = Image.new("RGBA", (cw, chh), tuple(int(v * 255) for v in SHEET) + (255,))
        t.alpha_composite(art)
        d = ImageDraw.Draw(t)
        d.rectangle([3, 3, cw - 4, chh - 4], outline=(35, 28, 23), width=1)
        d.rectangle([7, 7, cw - 8, chh - 8], outline=(150, 140, 130), width=1)
        for i, (fx, fy) in enumerate(PINS):
            x, y = fx * cw, fy * chh
            d.ellipse([x - 15, y - 15, x + 15, y + 15], fill=(248, 242, 230), outline=(184, 70, 27), width=2)
            d.ellipse([x - 4, y - 4, x + 4, y + 4], fill=(184, 70, 27))
            d.rectangle([x - 150, y + 22, x + 150, y + 70], fill=(251, 247, 238), outline=(120, 110, 100))
        d.text((14, chh - 30), f"Plate - {pid}", fill=(35, 28, 23))
        tiles.append(t.convert("RGB"))
    cols = 3
    rows = (len(tiles) + cols - 1) // cols
    g = 16
    sheet = Image.new("RGB", (cols * cw + (cols + 1) * g, rows * chh + (rows + 1) * g), (239, 230, 212))
    for i, t in enumerate(tiles):
        sheet.paste(t, (g + (i % cols) * (cw + g), g + (i // cols) * (chh + g)))
    sheet.save(path)
    print("wrote", path, sheet.size)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", default="")
    ap.add_argument("--contact", default="")
    ap.add_argument("--body", default="")
    ap.add_argument("--no-write", action="store_true")
    a = ap.parse_args()
    ids = [i for i in a.only.split(",") if i] or list(PLATES)
    if not a.no_write:
        write(ids)
    if a.contact:
        contact(a.contact, ids, [b for b in a.body.split(",") if b])


if __name__ == "__main__":
    main()
