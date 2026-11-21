#!/usr/bin/env python3
"""Web fonts for the site, from the game's own fonts in godot/assets/fonts (all SIL Open Font License 1.1).

- Fraunces (roman and italic): pinned to SOFT=0, WONK=0, weight kept variable from 300 to 900, optical size kept,
  subset to Latin plus the punctuation, arrows and Greek letters the site uses, saved as WOFF2. Fraunces has no
  Reserved Font Name, so a subset may keep the name.
- Atkinson Hyperlegible: the game's WOFF2 files, copied unchanged.
- IBM Plex Mono: copied unchanged as TTF. Its licence reserves the name "Plex", so a subset or a converted file
  would need another name; the unmodified file is simplest.
The licence texts are copied next to the fonts. Needs fontTools and brotli.
"""
import io
import shutil
from pathlib import Path

from fontTools import subset
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "godot/assets/fonts"
OUT = ROOT / "site/assets/fonts"
UNICODES = ("U+0020-007E,U+00A0-00FF,U+0131,U+0152-0153,U+02C6,U+02DA,U+02DC,U+2009,U+200A,U+2010-2015,"
            "U+2018-201E,U+2022,U+2026,U+2030,U+2032-2033,U+2039-203A,U+2044,U+20AC,U+2116,U+2122,U+2190-2195,"
            "U+21A9,U+2212,U+2215,U+2219,U+2248,U+2260,U+2264-2265,U+22A3,U+00B5,U+0394,U+03B1-03C9")
FEATURES = ["kern", "liga", "lnum", "onum", "pnum", "tnum", "frac", "sups", "case", "calt", "ccmp", "mark", "mkmk"]


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    for name in ("Fraunces", "Fraunces-Italic"):
        inst = instancer.instantiateVariableFont(TTFont(SRC / f"{name}.ttf"), {"SOFT": 0, "WONK": 0, "wght": (300, 900)})
        buf = io.BytesIO()
        inst.save(buf)
        buf.seek(0)
        font = TTFont(buf)   # reload: the subsetter needs fully loaded tables, not the instancer's lazy ones
        opts = subset.Options()
        opts.flavor = "woff2"
        opts.layout_features = FEATURES
        opts.name_IDs = ["*"]
        opts.notdef_outline = True
        sub = subset.Subsetter(opts)
        sub.populate(unicodes=subset.parse_unicodes(UNICODES))
        sub.subset(font)
        font.flavor = "woff2"
        font.save(OUT / f"{name}.woff2")
    for f in ("AtkinsonHyperlegible-Regular.woff2", "AtkinsonHyperlegible-Bold.woff2", "IBMPlexMono-Regular.ttf",
              "OFL-Fraunces.txt", "OFL-AtkinsonHyperlegible.txt", "OFL-IBMPlexMono.txt"):
        shutil.copyfile(SRC / f, OUT / f)
    for f in sorted(OUT.iterdir()):
        print(f"{f.name:40s} {f.stat().st_size // 1024:5d} KB")


if __name__ == "__main__":
    main()
