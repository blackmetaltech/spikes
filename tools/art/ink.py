"""Tiny engraving kit for the campaign-map plates: cairo for the line work, PIL for the lettering (the game's own Fraunces
italic, loaded from godot/assets/fonts so the plates letter exactly like the procedural ones). Everything is deterministic.

Units: image pixels on a W x H plate. The plate is drawn over the whole campaign-map canvas, which is about 1236 x 766 logical
px at 1080p, so one logical px of the procedural plates is S = W / 1236 image px here; widths and font sizes are given in
logical px and scaled by S, which keeps the line weights and the lettering the same as the hand-coded body plates."""
import math
from pathlib import Path

import cairo
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
FONTS = ROOT / "godot" / "assets" / "fonts"

W, H = 2000, 1250
S = W / 1236.0


def hexc(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4))


# palette (godot/ui/kit.gd, gfx.gd, anatomy.gd)
SHEET = hexc("#F6F0E2")
INK = hexc("#231C17")
SOFT = hexc("#5E5246")
EXC = hexc("#B8461B")       # Golgi sienna
EXC_INK = hexc("#9A3814")   # sienna dark enough for lettering
INH = hexc("#1E4F8F")       # Prussian blue
SENSOR = hexc("#12808F")    # verdigris
MOTOR = hexc("#6E2A4E")     # madder plum
GAMBOGE = hexc("#C9971C")
GREEN = hexc("#2E7D4F")
BODY = hexc("#8A6A4F")      # sepia specimen
FISH = hexc("#6F7F86")      # slate specimen
TISSUE = hexc("#EEDFCB")
WORMFILL = hexc("#E6D2B5")
GUT = hexc("#C2A27A")
WALL = hexc("#A2493F")
BONE = hexc("#5E4E3E")
NUC = hexc("#6E4E7E")
GRAY = hexc("#6E6458")


def lw(px):
    """A logical line width (as in the procedural plates) in image px."""
    return px * S


# ---------------------------------------------------------------- geometry

def catmull(pts, per=12, closed=False):
    """Centripetal-ish Catmull-Rom through pts (uniform), `per` samples per span."""
    P = [np.asarray(p, float) for p in pts]
    if closed:
        P = [P[-1]] + P + [P[0], P[1]]
    else:
        P = [2 * P[0] - P[1]] + P + [2 * P[-1] - P[-2]]
    out = []
    for i in range(1, len(P) - 2):
        p0, p1, p2, p3 = P[i - 1], P[i], P[i + 1], P[i + 2]
        for k in range(per):
            t = k / per
            t2, t3 = t * t, t * t * t
            out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3))
    if not closed:
        out.append(P[-2])
    return np.array(out)


def resample(pts, n):
    """n points evenly spaced by arc length along a polyline."""
    pts = np.asarray(pts, float)
    d = np.r_[0, np.cumsum(np.hypot(*np.diff(pts, axis=0).T))]
    s = np.linspace(0, d[-1], n)
    return np.c_[np.interp(s, d, pts[:, 0]), np.interp(s, d, pts[:, 1])]


def normals(pts):
    t = np.gradient(np.asarray(pts, float), axis=0)
    t /= np.maximum(np.hypot(t[:, 0], t[:, 1]), 1e-9)[:, None]
    return np.c_[-t[:, 1], t[:, 0]], t


class Spine:
    """A body axis: arc-length resampled centreline with normals; at(u, off) is the point at fraction u along it, offset
    `off` px along the normal (positive = the +normal side)."""

    def __init__(self, pts, n=600):
        self.p = resample(pts, n)
        self.n, self.t = normals(self.p)
        self.N = n
        self.length = float(np.sum(np.hypot(*np.diff(self.p, axis=0).T)))

    def _i(self, u):
        return min(max(u, 0.0), 1.0) * (self.N - 1)

    def at(self, u, off=0.0):
        f = self._i(u)
        i = int(min(f, self.N - 2))
        a = f - i
        p = self.p[i] * (1 - a) + self.p[i + 1] * a
        nn = self.n[i] * (1 - a) + self.n[i + 1] * a
        return p + nn * off

    def tangent(self, u):
        i = int(min(self._i(u), self.N - 2))
        return self.t[i]

    def line(self, u0, u1, off=0.0, n=80):
        """Polyline along the axis from u0 to u1, offset by off (a number or a function of u)."""
        us = np.linspace(u0, u1, n)
        return np.array([self.at(u, off(u) if callable(off) else off) for u in us])

    def outline(self, prof, u0=0.0, u1=1.0, n=240):
        """Closed body outline for half-width profile prof(u)."""
        us = np.linspace(u0, u1, n)
        left = [self.at(u, -prof(u)) for u in us]
        right = [self.at(u, prof(u)) for u in us[::-1]]
        return np.array(left + right)


def ellipse(c, rx, ry, rot=0.0, n=64):
    a = np.linspace(0, 2 * np.pi, n, endpoint=False)
    x, y = np.cos(a) * rx, np.sin(a) * ry
    cs, sn = math.cos(rot), math.sin(rot)
    return np.c_[c[0] + x * cs - y * sn, c[1] + x * sn + y * cs]


def smoothstep(a, b, x):
    t = min(max((x - a) / (b - a), 0.0), 1.0)
    return t * t * (3 - 2 * t)


# ---------------------------------------------------------------- the plate

class Plate:
    def __init__(self, w=W, h=H):
        self.w, self.h = w, h
        self.surf = cairo.ImageSurface(cairo.FORMAT_ARGB32, w, h)
        self.c = cairo.Context(self.surf)
        self.c.set_line_join(cairo.LINE_JOIN_ROUND)
        self.c.set_line_cap(cairo.LINE_CAP_ROUND)
        self.labels = []   # deferred lettering: drawn last, over everything, like the procedural tags
        self.keep = []     # [x, y, w, h, weight] areas the label plates of the map should avoid (image px)

    # -- paths
    def _path(self, pts, closed=False):
        c = self.c
        c.new_path()
        pts = np.asarray(pts, float)
        c.move_to(*pts[0])
        for q in pts[1:]:
            c.line_to(*q)
        if closed:
            c.close_path()

    def _src(self, col, a):
        self.c.set_source_rgba(col[0], col[1], col[2], a)

    def fill(self, pts, col, a=1.0):
        self._path(pts, True)
        self._src(col, a)
        self.c.fill()

    def stroke(self, pts, col, a=1.0, w=1.6, closed=False, dash=None, cap=cairo.LINE_CAP_ROUND):
        self._path(pts, closed)
        self._src(col, a)
        self.c.set_line_width(lw(w))
        self.c.set_line_cap(cap)
        self.c.set_dash([lw(d) for d in dash] if dash else [])
        self.c.stroke()
        self.c.set_dash([])
        self.c.set_line_cap(cairo.LINE_CAP_ROUND)

    def shape(self, pts, fill, rim, fa=1.0, ra=1.0, w=1.8):
        if fill is not None:
            self.fill(pts, fill, fa)
        if rim is not None:
            self.stroke(pts, rim, ra, w, closed=True)

    def circle(self, c, r, fill=None, rim=None, fa=1.0, ra=1.0, w=1.4):
        self.c.new_path()
        self.c.arc(c[0], c[1], r, 0, 2 * math.pi)
        if fill is not None:
            self._src(fill, fa)
            self.c.fill_preserve() if rim is not None else self.c.fill()
        if rim is not None:
            self._src(rim, ra)
            self.c.set_line_width(lw(w))
            self.c.stroke()

    def arc(self, c, r, a0, a1, col, a=1.0, w=1.4, dash=None):
        self.c.new_path()
        self.c.arc(c[0], c[1], r, a0, a1)
        self._src(col, a)
        self.c.set_line_width(lw(w))
        self.c.set_dash([lw(d) for d in dash] if dash else [])
        self.c.stroke()
        self.c.set_dash([])

    def clip(self, pts):
        self.c.save()
        self._path(pts, True)
        self.c.clip()

    def unclip(self):
        self.c.restore()

    def hatch(self, poly, angle, gap, col, a=0.25, w=1.0):
        """Engraver's shade: parallel lines every `gap` logical px at `angle` (radians), clipped to poly."""
        poly = np.asarray(poly, float)
        lo, hi = poly.min(0), poly.max(0)
        ctr = (lo + hi) / 2
        rad = np.hypot(*(hi - lo)) / 2 + 4
        d = np.array([math.cos(angle), math.sin(angle)])
        nn = np.array([-d[1], d[0]])
        self.clip(poly)
        self._src(col, a)
        self.c.set_line_width(lw(w))
        k = -rad
        g = lw(gap)
        while k <= rad:
            p0 = ctr + nn * k - d * rad
            p1 = ctr + nn * k + d * rad
            self.c.move_to(*p0)
            self.c.line_to(*p1)
            k += g
        self.c.stroke()
        self.unclip()

    def stipple(self, poly, n, col, a=0.4, dot=1.4, seed=0):
        """Seeded ink dots inside poly (Nissl / granule texture)."""
        rng = np.random.default_rng(seed)
        poly = np.asarray(poly, float)
        lo, hi = poly.min(0), poly.max(0)
        self.clip(poly)
        self._src(col, a)
        pts = rng.uniform(lo, hi, size=(n, 2))
        r = lw(dot) * 0.5
        for q in pts:
            self.c.new_sub_path()
            self.c.arc(q[0], q[1], r, 0, 2 * math.pi)
        self.c.fill()
        self.unclip()

    def stipple_disc(self, c, r, n, col, a=0.4, dot=1.4, seed=0, bias=1.0):
        rng = np.random.default_rng(seed)
        ang = rng.uniform(0, 2 * math.pi, n)
        rr = rng.uniform(0, 1, n) ** (0.5 * bias) * r
        self._src(col, a)
        d = lw(dot) * 0.5
        for t, q in zip(ang, rr):
            self.c.new_sub_path()
            self.c.arc(c[0] + math.cos(t) * q, c[1] + math.sin(t) * q, d, 0, 2 * math.pi)
        self.c.fill()

    def wash(self, c, r, col, a=0.4):
        """A watercolour wash of stain (the firing-cell look): soft radial blot."""
        g = cairo.RadialGradient(c[0], c[1], 0, c[0], c[1], r)
        g.add_color_stop_rgba(0, col[0], col[1], col[2], a)
        g.add_color_stop_rgba(0.55, col[0], col[1], col[2], a * 0.55)
        g.add_color_stop_rgba(1, col[0], col[1], col[2], 0)
        self.c.new_path()
        self.c.arc(c[0], c[1], r, 0, 2 * math.pi)
        self.c.set_source(g)
        self.c.fill()

    def tube(self, spine_or_pts, prof, fill, rim=None, fa=1.0, ra=1.0, w=1.6, u0=0.0, u1=1.0):
        sp = spine_or_pts if isinstance(spine_or_pts, Spine) else Spine(spine_or_pts, 300)
        o = sp.outline(prof if callable(prof) else (lambda u: prof), u0, u1)
        self.shape(o, fill, rim, fa, ra, w)
        return o

    def arrow(self, pts, col, a=1.0, w=1.6, head=9.0, dash=None, kind="arrow"):
        """A line ending in an arrowhead (excitatory) or a bar (inhibitory, the ⊣ of the identity)."""
        pts = np.asarray(pts, float)
        self.stroke(pts, col, a, w, dash=dash)
        tip = pts[-1]
        d = pts[-1] - pts[-2]
        d = d / max(np.hypot(*d), 1e-9)
        nn = np.array([-d[1], d[0]])
        h = lw(head)
        if kind == "arrow":
            self.fill([tip, tip - d * h + nn * h * 0.45, tip - d * h * 0.72, tip - d * h - nn * h * 0.45], col, a)
        else:
            self.stroke([tip + nn * h * 0.62, tip - nn * h * 0.62], col, a, w * 1.6, cap=cairo.LINE_CAP_BUTT)

    def dot(self, p, r, col, a=1.0, halo=True):
        if halo:
            self.circle(p, r + lw(1.6), SHEET, fa=0.9)
        self.circle(p, r, col, fa=a)

    # -- lettering
    def label(self, text, p, col=SOFT, size=14, align="l", slip=0.55, leader=None, italic=True, bold=False, a=0.95):
        """A lettered call-out at baseline p (logical size `size`), on a translucent paper slip like Anatomy.tag, with an
        optional leader line to a landmark (drawn with a small dot on the landmark)."""
        self.labels.append(dict(text=text, p=tuple(map(float, p)), col=col, size=size, align=align, slip=slip, leader=leader,
                                italic=italic, bold=bold, a=a))

    def keep_rect(self, x, y, w, h, weight=40):
        self.keep.append([int(x), int(y), int(w), int(h), weight])

    # -- output
    def image(self):
        self.surf.flush()
        im = Image.frombuffer("RGBA", (self.w, self.h), bytes(self.surf.get_data()), "raw", "BGRa", 0, 1).copy()
        return letter(im, self.labels, self)


_fonts = {}


def font(px, italic=True, bold=False):
    key = (px, italic, bold)
    if key not in _fonts:
        if italic:
            f = ImageFont.truetype(str(FONTS / "Fraunces-Italic.ttf"), px)
            # Fonts.italic in godot/game/fonts.gd: opsz 24, wght 420 (600 bold), SOFT 100, WONK 1
            f.set_variation_by_axes([24, 600 if bold else 420, 100, 1])
        else:
            f = ImageFont.truetype(str(FONTS / "Fraunces.ttf"), px)
            ax = [a["name"] for a in f.get_variation_axes()]
            vals = {b"Optical Size": 24, b"Weight": 600 if bold else 420, b"Softness": 100, b"Wonky": 0}
            f.set_variation_by_axes([vals.get(n, 0) for n in ax])
        _fonts[key] = f
    return _fonts[key]


def text_size(text, size, italic=True, bold=False):
    f = font(round(size * S), italic, bold)
    return f.getlength(text), round(size * S)


def letter(im, labels, plate):
    """Draw the deferred labels: leader, paper slip, then the text, onto the RGBA image."""
    ctx = cairo.ImageSurface(cairo.FORMAT_ARGB32, im.width, im.height)
    c = cairo.Context(ctx)
    c.set_line_cap(cairo.LINE_CAP_ROUND)
    boxes = []
    for L in labels:
        px = round(L["size"] * S)
        f = font(px, L["italic"], L["bold"])
        w = f.getlength(L["text"])
        x, y = L["p"]
        if L["align"] == "c":
            x -= w / 2
        elif L["align"] == "r":
            x -= w
        r = (x - 4 * S, y - px * 0.92, w + 8 * S, px * 1.25)
        boxes.append((L, f, x, y, r))
        if L["leader"] is not None:
            lx, ly = L["leader"]
            # join the landmark to the nearest side of the slip
            tx = min(max(lx, r[0]), r[0] + r[2])
            ty = min(max(ly, r[1]), r[1] + r[3])
            col = L["col"]
            c.set_source_rgba(col[0], col[1], col[2], 0.55)
            c.set_line_width(lw(1.0))
            c.move_to(lx, ly)
            c.line_to(tx, ty)
            c.stroke()
            c.set_source_rgba(col[0], col[1], col[2], 0.9)
            c.arc(lx, ly, lw(2.2), 0, 2 * math.pi)
            c.fill()
        if L["slip"] > 0:
            rr = 3 * S
            x0, y0, ww, hh = r
            c.new_sub_path()
            c.arc(x0 + ww - rr, y0 + rr, rr, -math.pi / 2, 0)
            c.arc(x0 + ww - rr, y0 + hh - rr, rr, 0, math.pi / 2)
            c.arc(x0 + rr, y0 + hh - rr, rr, math.pi / 2, math.pi)
            c.arc(x0 + rr, y0 + rr, rr, math.pi, 1.5 * math.pi)
            c.close_path()
            c.set_source_rgba(*SHEET, L["slip"])
            c.fill()
        plate.keep.append([int(r[0]), int(r[1]), int(r[2]), int(r[3]), 120])
    ctx.flush()
    over = Image.frombuffer("RGBA", (im.width, im.height), bytes(ctx.get_data()), "raw", "BGRa", 0, 1)
    im = Image.alpha_composite(im, over)
    for L, f, x, y, r in boxes:
        col = tuple(int(round(v * 255)) for v in L["col"])
        asc = f.getmetrics()[0]
        bb = f.getbbox(L["text"])
        lay = Image.new("RGBA", (int(bb[2] + 8), int(asc + f.getmetrics()[1] + 8)), col + (0,))
        ImageDraw.Draw(lay).text((2, 2), L["text"], font=f, fill=col + (int(255 * L["a"]),))
        im.alpha_composite(lay, (int(round(x)) - 2, int(round(y - asc)) - 2))
    return im
