#!/usr/bin/env python3
"""Build the Spike wiki: site/wiki-src/*.md -> site/wiki/*.html, site/wiki/search.json, site/sitemap.xml.

    python3 tools/site/build.py            build
    python3 tools/site/build.py --check    build, then fail on broken internal links, anchors or images

Python standard library only. Each page is a Markdown file with front matter:

    ---
    title: The Bestiary
    order: 30            # position in the sidebar (sections are ordered by their smallest page order)
    section: The wings   # sidebar group
    summary: One line for the index, search results and the page's meta description.
    ---

Files whose names start with "_" or are in capitals (README.md, the authoring guide; IMAGES.md) are not pages. Other files under
site/wiki-src (images, say img/foo.webp) are copied to site/wiki with the same relative path.

The Markdown dialect is CommonMark-ish, with what the wiki needs (see site/wiki-src/README.md for the full list):
headings with anchors, tables with alignment, fenced code, images that become figures with captions, clips
(an image whose source is .webm/.mp4 becomes a looping video with its poster), callouts (> [!NOTE], > [!WARNING],
"!!! note", or ":::note ... :::"), and maths written plainly (underscores inside words such as V_m or tau_syn are
never emphasis). Links to other pages may be written as foo.md or foo.html; links to repository files outside
site/ (../../docs/WHAT-IS-REAL.md) become GitHub links. The comment <!-- pages --> expands to the list of all pages
by section.
"""
from __future__ import annotations

import html
import json
import re
import shutil
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "site/wiki-src"
OUT = ROOT / "site/wiki"
SITE = ROOT / "site"
TEMPLATE = Path(__file__).resolve().parent / "template.html"
BASE_URL = "https://spike.blackmetal.tech/"
REPO_BLOB = "https://github.com/blackmetaltech/spikes/blob/master/"
REPO_EDIT = "https://github.com/blackmetaltech/spikes/edit/master/"
ROMAN = ["I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X", "XI", "XII", "XIII", "XIV", "XV", "XVI"]


# ---------------------------------------------------------------- pages
@dataclass
class Page:
    path: Path
    slug: str
    title: str
    order: float
    section: str
    summary: str
    body: str
    meta: dict
    html: str = ""
    text: str = ""
    headings: list = field(default_factory=list)   # (level, id, text)


def parse_front_matter(raw: str) -> tuple[dict, str]:
    meta: dict = {}
    if raw.startswith("---"):
        end = re.search(r"^---\s*$", raw[3:], re.M)
        if end:
            block = raw[3:3 + end.start()]
            raw = raw[3 + end.end():].lstrip("\n")
            for line in block.splitlines():
                m = re.match(r"^\s*([A-Za-z_][\w-]*)\s*:\s*(.*?)\s*$", line)
                if m:
                    v = m.group(2)
                    if len(v) >= 2 and v[0] == v[-1] and v[0] in "\"'":
                        v = v[1:-1]
                    meta[m.group(1).lower()] = v
    return meta, raw


def load_pages() -> list[Page]:
    pages = []
    for p in sorted(SRC.glob("*.md")):
        if p.name.startswith("_") or p.stem.isupper():   # README.md, IMAGES.md and other notes in capitals are not pages
            continue
        meta, body = parse_front_matter(p.read_text(encoding="utf-8"))
        slug = p.stem
        try:
            order = float(meta.get("order", "1000"))
        except ValueError:
            order = 1000.0
        title = meta.get("title") or slug.replace("-", " ").capitalize()
        pages.append(Page(p, slug, title, order, meta.get("section", "Pages"), meta.get("summary", ""), body, meta))
    pages.sort(key=lambda q: (q.slug != "index", q.order, q.title.lower()))
    return pages


def sections(pages: list[Page]) -> list[tuple[str, list[Page]]]:
    groups: dict[str, list[Page]] = {}
    for p in pages:
        if p.slug == "index":
            continue
        groups.setdefault(p.section, []).append(p)
    return sorted(groups.items(), key=lambda kv: min(q.order for q in kv[1]))


# ---------------------------------------------------------------- images
def image_size(path: Path) -> tuple[int, int] | None:
    """Width and height of a PNG, GIF, JPEG or WebP file, from its header."""
    try:
        with open(path, "rb") as f:
            head = f.read(64)
            if head[:8] == b"\x89PNG\r\n\x1a\n":
                return struct.unpack(">II", head[16:24])
            if head[:6] in (b"GIF87a", b"GIF89a"):
                return struct.unpack("<HH", head[6:10])
            if head[:4] == b"RIFF" and head[8:12] == b"WEBP":
                kind = head[12:16]
                if kind == b"VP8 ":
                    w, h = struct.unpack("<HH", head[26:30])
                    return w & 0x3FFF, h & 0x3FFF
                if kind == b"VP8L":
                    b = head[21:25]
                    w = 1 + (((b[1] & 0x3F) << 8) | b[0])
                    h = 1 + (((b[3] & 0x0F) << 10) | (b[2] << 2) | ((b[1] & 0xC0) >> 6))
                    return w, h
                if kind == b"VP8X":
                    w = 1 + int.from_bytes(head[24:27], "little")
                    h = 1 + int.from_bytes(head[27:30], "little")
                    return w, h
            if head[:2] == b"\xff\xd8":
                f.seek(2)
                while True:
                    marker = f.read(2)
                    if len(marker) < 2 or marker[0] != 0xFF:
                        return None
                    if 0xC0 <= marker[1] <= 0xCF and marker[1] not in (0xC4, 0xC8, 0xCC):
                        f.read(3)
                        h, w = struct.unpack(">HH", f.read(4))
                        return w, h
                    seg = struct.unpack(">H", f.read(2))[0]
                    f.seek(seg - 2, 1)
    except OSError:
        return None
    return None


# ---------------------------------------------------------------- Markdown
def slugify(text: str) -> str:
    s = re.sub(r"<[^>]+>", "", text).lower()
    s = re.sub(r"&[a-z]+;|&#\d+;", "", s)
    s = re.sub(r"[^\w\s-]", "", s, flags=re.U).strip()
    return re.sub(r"[\s_]+", "-", s) or "section"


INLINE_TAGS = r"(?:kbd|sub|sup|br|abbr|span|em|strong|code|small|mark|b|i|u|s|q|cite|var|samp|wbr|del|ins|dfn)"
BLOCK_TAGS = r"(?:div|figure|figcaption|table|thead|tbody|tr|td|th|details|summary|video|audio|source|iframe|aside|section|p|ul|ol|li|dl|dt|dd|picture|img|br|hr|h[1-6]|blockquote|pre|nav|svg|!--)"
CALLOUT_KINDS = {"note": "note", "info": "note", "tip": "tip", "hint": "tip", "important": "warn", "warning": "warn",
                 "warn": "warn", "caution": "warn", "danger": "warn", "example": "note", "abstract": "note"}
CALLOUT_LABEL = {"note": "Note", "tip": "Tip", "warn": "Warning"}


class Renderer:
    """A small Markdown renderer. One instance per page (it keeps the page's heading ids and link references)."""

    def __init__(self, page: Page, pages_by_slug: dict[str, Page]):
        self.page = page
        self.pages = pages_by_slug
        self.ids: set[str] = set()
        self.headings: list = []
        self.refs: dict[str, tuple[str, str]] = {}
        self.problems: list[str] = []
        self.links: list[str] = []     # internal hrefs, checked after the build
        self.images: list[str] = []
        self.fig = 0

    # ---- links and paths ----
    def href(self, url: str) -> str:
        url = url.strip()
        if re.match(r"^[a-z][a-z0-9+.-]*:|^#|^//", url, re.I):
            return url
        path, _, frag = url.partition("#")
        if not path:
            return url
        target = (SRC / path).resolve()
        if path.endswith(".md"):
            stem = Path(path).stem
            if target.parent == SRC.resolve() or not path.startswith(".."):
                if Path(path).name.lower() == "readme.md":
                    return REPO_BLOB + str((SRC / path).resolve().relative_to(ROOT)) + (("#" + frag) if frag else "")
                out = f"{stem}.html" + (("#" + frag) if frag else "")
                self.links.append(out)
                return out
        try:
            rel = target.relative_to(ROOT)
        except ValueError:
            return url
        if not str(rel).startswith("site/"):    # a repository file: link to it on GitHub
            return REPO_BLOB + str(rel) + (("#" + frag) if frag else "")
        if str(rel).startswith("site/wiki-src/"):
            out = path
        else:
            out = url
        self.links.append(out)
        return out

    # ---- inline ----
    def inline(self, s: str) -> str:
        stash: list[str] = []

        def keep(h: str) -> str:
            stash.append(h)
            return f"\x00{len(stash) - 1}\x00"

        # code spans first
        s = re.sub(r"(`+)(.+?)\1", lambda m: keep("<code>" + html.escape(m.group(2).strip(), quote=False) + "</code>"), s)
        # backslash escapes
        s = re.sub(r"\\([\\`*_{}\[\]()#+\-.!|~<>$])", lambda m: keep(html.escape(m.group(1))), s)
        # hard break: backslash or two spaces at line end
        s = re.sub(r"(?:\\| {2,})\n", lambda m: keep("<br>\n"), s)
        # raw inline html tags we allow
        s = re.sub(rf"</?{INLINE_TAGS}\b[^>]*>", lambda m: keep(m.group(0)), s, flags=re.I)
        # autolinks
        s = re.sub(r"<(https?://[^>\s]+)>", lambda m: keep(f'<a href="{html.escape(m.group(1))}">{html.escape(m.group(1))}</a>'), s)
        # images and links (one level of nested brackets in the text)
        s = re.sub(r"!\[((?:[^\[\]]|\[[^\]]*\])*)\]\(\s*<?([^)\s>]+)>?(?:\s+\"([^\"]*)\")?\s*\)", lambda m: keep(self.img_inline(m.group(1), m.group(2), m.group(3))), s)
        s = re.sub(r"\[((?:[^\[\]]|\[[^\]]*\])*)\]\(\s*<?([^)\s>]+)>?(?:\s+\"([^\"]*)\")?\s*\)", lambda m: keep(self.link(m.group(1), m.group(2), m.group(3))), s)
        s = re.sub(r"\[((?:[^\[\]]|\[[^\]]*\])*)\]\[([^\]]*)\]", lambda m: keep(self.ref_link(m.group(1), m.group(2) or m.group(1))), s)
        # bare URLs
        s = re.sub(r"(?<![\"'=(\w])(https?://[^\s<>()]+[^\s<>().,;:!?'\"])", lambda m: keep(f'<a href="{html.escape(m.group(1))}">{html.escape(m.group(1))}</a>'), s)
        s = html.escape(s, quote=False)
        # strong, emphasis, strike. Underscores only at word boundaries, so V_m and tau_syn_ex stay plain.
        s = re.sub(r"\*\*(?=\S)(.+?)(?<=\S)\*\*", r"<strong>\1</strong>", s)
        s = re.sub(r"(?<![\w\\])__(?=\S)(.+?)(?<=\S)__(?!\w)", r"<strong>\1</strong>", s)
        s = re.sub(r"(?<![\*\w])\*(?=[^\s*])(.+?)(?<=[^\s*])\*(?![\*\w])", r"<em>\1</em>", s)
        s = re.sub(r"(?<![\w\\])_(?=[^\s_])(.+?)(?<=[^\s_])_(?!\w)", r"<em>\1</em>", s)
        s = re.sub(r"~~(?=\S)(.+?)(?<=\S)~~", r"<del>\1</del>", s)
        # typographic arrows written plainly
        s = s.replace("--&gt;", "&rarr;").replace("&lt;--", "&larr;")
        for _ in range(3):
            s = re.sub(r"\x00(\d+)\x00", lambda m: stash[int(m.group(1))], s)
        return s

    def link(self, text: str, url: str, title: str | None) -> str:
        t = f' title="{html.escape(title)}"' if title else ""
        return f'<a href="{html.escape(self.href(url))}"{t}>{self.inline(text)}</a>'

    def ref_link(self, text: str, ref: str) -> str:
        r = self.refs.get(ref.strip().lower())
        if not r:
            return html.escape(f"[{text}][{ref}]")
        return self.link(text, r[0], r[1] or None)

    def resolve_image(self, src: str) -> str:
        """A local image that is missing as written but exists as .webp (or the other way round) is pointed at the file that exists."""
        if re.match(r"^[a-z]+:|^//", src, re.I) or (SRC / src).exists():
            return src
        stem, dot, ext = src.rpartition(".")
        for alt in ("webp", "png", "jpg", "gif"):
            if dot and alt != ext.lower() and (SRC / f"{stem}.{alt}").exists():
                return f"{stem}.{alt}"
        return src

    def img_attrs(self, src: str) -> str:
        src = self.resolve_image(src)
        href = self.href(src)
        attrs = f'src="{html.escape(href)}"'
        if not re.match(r"^[a-z]+:|^//", href, re.I):
            self.images.append(href)
            size = image_size((OUT / href).resolve()) or image_size((SRC / src).resolve())
            if size:
                attrs += f' width="{size[0]}" height="{size[1]}"'
        return attrs

    def img_inline(self, alt: str, src: str, title: str | None) -> str:
        t = f' title="{html.escape(title)}"' if title else ""
        return f'<img {self.img_attrs(src)} alt="{html.escape(alt)}"{t} loading="lazy" decoding="async">'

    def figure(self, alt: str, src: str, title: str | None, link: str | None = None) -> str:
        cap = title if title else alt
        self.fig += 1
        if re.search(r"\.(webm|mp4)$", src, re.I):
            return self.clip(alt, src, cap)
        src = self.resolve_image(src)
        img = f'<img {self.img_attrs(src)} alt="{html.escape(alt)}" loading="lazy" decoding="async">'
        if link:
            img = f'<a href="{html.escape(self.href(link))}">{img}</a>'
        else:
            img = f'<button class="zoom" type="button" data-zoom="{html.escape(self.href(src))}">{img}</button>'
        capt = f"<figcaption>{self.inline(cap)}</figcaption>" if cap else ""
        return f'<figure class="fig">{img}{capt}</figure>'

    def clip(self, alt: str, src: str, cap: str) -> str:
        base = re.sub(r"\.(webm|mp4)$", "", src, flags=re.I)
        sources = []
        for ext, typ in (("webm", "video/webm"), ("mp4", "video/mp4")):
            cand = f"{base}.{ext}"
            if (SRC / cand).resolve().exists() or cand == src:
                sources.append(f'<source src="{html.escape(self.href(cand))}" type="{typ}">')
                self.images.append(self.href(cand))
        poster = f"{base}-poster.webp"
        pattr, size = "", None
        if (SRC / poster).resolve().exists():
            pattr = f' poster="{html.escape(self.href(poster))}"'
            size = image_size((SRC / poster).resolve())
        dims = f' width="{size[0]}" height="{size[1]}"' if size else ""
        gif = f"{base}.gif"
        fallback = f'<img src="{html.escape(self.href(gif))}" alt="{html.escape(alt)}">' if (SRC / gif).resolve().exists() else html.escape(alt)
        capt = f"<figcaption>{self.inline(cap)}</figcaption>" if cap else ""
        return (f'<figure class="fig"><div class="clip" data-clip><video{dims} muted loop playsinline controls preload="none"{pattr} '
                f'aria-label="{html.escape(alt)}">{"".join(sources)}{fallback}</video>'
                f'<button class="play" type="button" aria-label="Play the clip"></button></div>{capt}</figure>')

    # ---- blocks ----
    def heading(self, level: int, text: str) -> str:
        m = re.search(r"\s*\{#([\w-]+)\}\s*$", text)
        if m:
            hid, text = m.group(1), text[:m.start()]
        else:
            hid = slugify(self.inline(text))
        base, n = hid, 2
        while hid in self.ids:
            hid = f"{base}-{n}"
            n += 1
        self.ids.add(hid)
        inner = self.inline(text.strip().rstrip("#").strip())
        plain = re.sub(r"<[^>]+>", "", inner)
        self.headings.append((level, hid, html.unescape(plain)))
        return f'<h{level} id="{hid}">{inner}<a class="hl" href="#{hid}"><span class="sr">Link to this section</span><span aria-hidden="true">&para;</span></a></h{level}>'

    def table(self, rows: list[str]) -> str:
        def cells(r: str) -> list[str]:
            r = r.strip()
            if r.startswith("|"):
                r = r[1:]
            if r.endswith("|") and not r.endswith("\\|"):
                r = r[:-1]
            out, cur, code = [], "", False
            i = 0
            while i < len(r):
                ch = r[i]
                if ch == "`":
                    code = not code
                if ch == "\\" and i + 1 < len(r) and r[i + 1] == "|":
                    cur += "|"
                    i += 2
                    continue
                if ch == "|" and not code:
                    out.append(cur.strip())
                    cur = ""
                else:
                    cur += ch
                i += 1
            out.append(cur.strip())
            return out
        head = cells(rows[0])
        aligns = []
        for c in cells(rows[1]):
            c = c.strip()
            aligns.append("center" if c.startswith(":") and c.endswith(":") else "right" if c.endswith(":") else "left" if c.startswith(":") else "")
        def al(i: int) -> str:
            a = aligns[i] if i < len(aligns) else ""
            return f' style="text-align:{a}"' if a else ""
        out = ['<div class="table"><table>', "<thead><tr>"]
        out += [f"<th{al(i)}>{self.inline(c)}</th>" for i, c in enumerate(head)]
        out.append("</tr></thead><tbody>")
        for r in rows[2:]:
            cs = cells(r)
            cs += [""] * (len(head) - len(cs))
            out.append("<tr>" + "".join(f"<td{al(i)}>{self.inline(c)}</td>" for i, c in enumerate(cs[:len(head)])) + "</tr>")
        out.append("</tbody></table></div>")
        return "".join(out)

    def callout(self, kind: str, title: str, body_lines: list[str]) -> str:
        k = CALLOUT_KINDS.get(kind.lower(), "note")
        label = title.strip() or CALLOUT_LABEL[k]
        return (f'<aside class="callout {k}" role="note"><p class="ctitle">{self.inline(label)}</p>'
                f"{self.blocks(body_lines)}</aside>")

    def blocks(self, lines: list[str]) -> str:
        out: list[str] = []
        i, n = 0, len(lines)
        para: list[str] = []

        def flush() -> None:
            if not para:
                return
            text = "\n".join(para).strip()
            para.clear()
            m = re.fullmatch(r"!\[((?:[^\[\]]|\[[^\]]*\])*)\]\(\s*<?([^)\s>]+)>?(?:\s+\"([^\"]*)\")?\s*\)", text)
            m2 = re.fullmatch(r"\[!\[((?:[^\[\]]|\[[^\]]*\])*)\]\(\s*<?([^)\s>]+)>?(?:\s+\"([^\"]*)\")?\s*\)\]\(([^)\s]+)\)", text)
            if m:
                out.append(self.figure(m.group(1), m.group(2), m.group(3)))
            elif m2:
                out.append(self.figure(m2.group(1), m2.group(2), m2.group(3), m2.group(4)))
            else:
                out.append(f"<p>{self.inline(text)}</p>")

        while i < n:
            line = lines[i]
            s = line.strip()
            # blank
            if not s:
                flush(); i += 1; continue
            # fenced code
            m = re.match(r"^(\s*)(`{3,}|~{3,})\s*([\w+-]*)", line)
            if m:
                flush()
                fence, lang, ind = m.group(2), m.group(3), len(m.group(1))
                j, code = i + 1, []
                while j < n and not re.match(rf"^\s*{re.escape(fence[0])}{{{len(fence)},}}\s*$", lines[j]):
                    code.append(lines[j][ind:] if lines[j][:ind].strip() == "" else lines[j])
                    j += 1
                cls = f' class="language-{lang}"' if lang else ""
                out.append(f"<pre><code{cls}>{html.escape(chr(10).join(code))}</code></pre>")
                i = j + 1
                continue
            # pages directive
            if s == "<!-- pages -->":
                flush(); out.append(self.page_list()); i += 1; continue
            # html block
            if re.match(rf"^<{BLOCK_TAGS}[\s>/]|^</{BLOCK_TAGS}>", s, re.I):
                flush()
                j = i
                while j < n and lines[j].strip():
                    out.append(lines[j]); j += 1
                i = j
                continue
            # ATX heading
            m = re.match(r"^(#{1,6})\s+(.*)$", s)
            if m:
                flush()
                level = len(m.group(1))
                if level == 1 and not self.headings and not out:
                    i += 1   # the page title is in the template
                    continue
                out.append(self.heading(max(level, 2), m.group(2)))
                i += 1
                continue
            # rule
            if re.fullmatch(r"(\*\s*){3,}|(-\s*){3,}|(_\s*){3,}", s) and not para:
                flush(); out.append('<hr class="hair">'); i += 1; continue
            # setext h2 under a paragraph line
            if para and re.fullmatch(r"-{3,}|={3,}", s):
                text = " ".join(para); para.clear()
                out.append(self.heading(2, text)); i += 1; continue
            # ::: callout
            m = re.match(r"^:{3,}\s*(\w+)\s*(.*)$", s)
            if m:
                flush()
                j, body = i + 1, []
                while j < n and not re.match(r"^\s*:{3,}\s*$", lines[j]):
                    body.append(lines[j]); j += 1
                out.append(self.callout(m.group(1), m.group(2).strip().strip('"'), body))
                i = j + 1
                continue
            # !!! admonition
            m = re.match(r"^!!!\s*(\w+)\s*(?:\"([^\"]*)\")?\s*$", s)
            if m:
                flush()
                j, body = i + 1, []
                while j < n and (not lines[j].strip() or lines[j].startswith(("    ", "\t"))):
                    body.append(lines[j][4:] if lines[j].startswith("    ") else lines[j].lstrip("\t"))
                    j += 1
                while body and not body[-1].strip():
                    body.pop()
                out.append(self.callout(m.group(1), m.group(2) or "", body))
                i = j
                continue
            # blockquote / GitHub callout
            if s.startswith(">"):
                flush()
                j, body = i, []
                while j < n and lines[j].strip().startswith(">"):
                    body.append(re.sub(r"^\s*>\s?", "", lines[j])); j += 1
                cm = re.match(r"^\[!(\w+)\]\s*(.*)$", body[0].strip()) if body else None
                if cm:
                    out.append(self.callout(cm.group(1), cm.group(2), body[1:]))
                else:
                    out.append(f"<blockquote>{self.blocks(body)}</blockquote>")
                i = j
                continue
            # table
            if "|" in s and i + 1 < n and re.match(r"^\s*\|?\s*:?-{2,}:?\s*(\|\s*:?-{2,}:?\s*)*\|?\s*$", lines[i + 1]):
                flush()
                j, rows = i, []
                while j < n and lines[j].strip() and "|" in lines[j]:
                    rows.append(lines[j]); j += 1
                out.append(self.table(rows))
                i = j
                continue
            # list
            m = re.match(r"^(\s*)([-*+]|\d{1,3}[.)])\s+", line)
            if m and not para:
                i = self.list_block(lines, i, out)
                continue
            # reference definition
            m = re.match(r"^\s*\[([^\]]+)\]:\s*<?(\S+?)>?(?:\s+\"([^\"]*)\")?\s*$", line)
            if m and not para:
                self.refs[m.group(1).strip().lower()] = (m.group(2), m.group(3) or "")
                i += 1
                continue
            para.append(line.rstrip("\n"))
            i += 1
        flush()
        return "\n".join(out)

    def list_block(self, lines: list[str], i: int, out: list[str]) -> int:
        m = re.match(r"^(\s*)([-*+]|\d{1,3}[.)])\s+", lines[i])
        base = len(m.group(1).expandtabs(4))
        ordered = m.group(2)[0].isdigit()
        start = int(re.match(r"\d+", m.group(2)).group(0)) if ordered else 1
        items: list[list[str]] = []
        loose = False
        n = len(lines)
        while i < n:
            line = lines[i]
            mm = re.match(r"^(\s*)([-*+]|\d{1,3}[.)])(\s+)(.*)$", line)
            if mm and len(mm.group(1).expandtabs(4)) == base and (mm.group(2)[0].isdigit()) == ordered:
                content_indent = len(mm.group(1)) + len(mm.group(2)) + len(mm.group(3))
                items.append([mm.group(4)])
                i += 1
                while i < n:
                    nxt = lines[i]
                    if not nxt.strip():
                        # a blank line: the list continues if the next non-blank line is indented or another item
                        k = i + 1
                        while k < n and not lines[k].strip():
                            k += 1
                        if k < n:
                            ind = len(lines[k]) - len(lines[k].lstrip())
                            m3 = re.match(r"^(\s*)([-*+]|\d{1,3}[.)])\s+", lines[k])
                            if ind >= content_indent or (m3 and len(m3.group(1).expandtabs(4)) == base):
                                if ind >= content_indent:
                                    items[-1].append("")
                                loose = True
                                i = k
                                if m3 and len(m3.group(1).expandtabs(4)) == base:
                                    break
                                continue
                        i = k
                        return self._emit_list(items, ordered, start, loose, out, i)
                    ind = len(nxt) - len(nxt.lstrip())
                    m3 = re.match(r"^(\s*)([-*+]|\d{1,3}[.)])\s+", nxt)
                    if m3 and len(m3.group(1).expandtabs(4)) <= base:
                        break
                    if ind >= content_indent or ind > base:
                        items[-1].append(nxt[min(ind, content_indent):])
                    else:
                        items[-1][-1] += "\n" + nxt.strip()   # lazy continuation
                    i += 1
            else:
                break
        return self._emit_list(items, ordered, start, loose, out, i)

    def _emit_list(self, items, ordered, start, loose, out, i) -> int:
        tag = "ol" if ordered else "ul"
        st = f' start="{start}"' if ordered and start != 1 else ""
        parts = [f"<{tag}{st}>"]
        for it in items:
            inner = self.blocks(it)
            if not loose and inner.startswith("<p>") and inner.count("<p>") == 1:
                inner = inner[3:].replace("</p>", "", 1)
            elif not loose:
                inner = re.sub(r"^<p>(.*?)</p>", r"\1", inner, count=1, flags=re.S)
            parts.append(f"<li>{inner}</li>")
        parts.append(f"</{tag}>")
        out.append("".join(parts))
        return i

    def page_list(self) -> str:
        out = ['<div class="pagelist">']
        for k, (name, ps) in enumerate(sections(list(self.pages.values()))):
            out.append(f'<section><h2 id="{slugify(name)}"><i>{ROMAN[k] if k < len(ROMAN) else k + 1}.</i> {html.escape(name)}</h2><ul>')
            self.ids.add(slugify(name))
            self.headings.append((2, slugify(name), name))
            for p in ps:
                out.append(f'<li><a href="{p.slug}.html">{html.escape(p.title)}</a>'
                           + (f"<span>{self.inline(p.summary)}</span>" if p.summary else "") + "</li>")
            out.append("</ul></section>")
        out.append("</div>")
        return "".join(out)


def plain_text(h: str) -> str:
    h = re.sub(r"<(script|style)[^>]*>.*?</\1>", " ", h, flags=re.S | re.I)
    h = re.sub(r'<a class="hl".*?</a>', " ", h, flags=re.S)
    h = re.sub(r"<[^>]+>", " ", h)
    return re.sub(r"\s+", " ", html.unescape(h)).strip()


# ---------------------------------------------------------------- page assembly
def sidebar(pages: list[Page], cur: Page) -> str:
    def li(p: Page) -> str:
        cur_attr = ' aria-current="page"' if p.slug == cur.slug else ""
        return f'<li><a href="{p.slug}.html"{cur_attr}>{html.escape(p.title)}</a></li>'
    index = next((p for p in pages if p.slug == "index"), None)
    out = []
    if index:
        out.append(f'<ul class="top">{li(index)}</ul>')
    for k, (name, ps) in enumerate(sections(pages)):
        out.append(f'<p class="sec"><i>{ROMAN[k] if k < len(ROMAN) else k + 1}.</i> {html.escape(name)}</p><ul>' + "".join(li(p) for p in ps) + "</ul>")
    return "\n".join(out)


def toc(page: Page) -> str:
    hs = [h for h in page.headings if h[0] in (2, 3)]
    if len(hs) < 3:
        return ""
    items = "".join(f'<li class="l{lvl}"><a href="#{hid}">{html.escape(text)}</a></li>' for lvl, hid, text in hs)
    return f'<nav class="toc" aria-label="On this page"><p class="sec">On this page</p><ul>{items}</ul></nav>'


def pager(order: list[Page], i: int) -> str:
    prev = order[i - 1] if i > 0 else None
    nxt = order[i + 1] if i + 1 < len(order) else None
    a = f'<a class="prev" href="{prev.slug}.html" rel="prev"><small>Previous</small>{html.escape(prev.title)}</a>' if prev else "<span></span>"
    b = f'<a class="next" href="{nxt.slug}.html" rel="next"><small>Next</small>{html.escape(nxt.title)}</a>' if nxt else "<span></span>"
    return f'<nav class="pager" aria-label="Previous and next page">{a}{b}</nav>'


def fill(tpl: str, values: dict[str, str]) -> str:
    return re.sub(r"\{\{(\w+)\}\}", lambda m: values.get(m.group(1), ""), tpl)


def build(check: bool) -> int:
    if not SRC.is_dir():
        print(f"no {SRC}", file=sys.stderr)
        return 1
    OUT.mkdir(parents=True, exist_ok=True)
    tpl = TEMPLATE.read_text(encoding="utf-8")
    # copy assets that live next to the sources (images and such)
    for f in SRC.rglob("*"):
        if f.is_file() and f.suffix.lower() != ".md":
            dest = OUT / f.relative_to(SRC)
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(f, dest)
    pages = load_pages()
    if not pages:
        print("no pages in site/wiki-src", file=sys.stderr)
        return 1
    by_slug = {p.slug: p for p in pages}
    renderers = {}
    for p in pages:
        r = Renderer(p, by_slug)
        p.html = r.blocks(p.body.splitlines())
        p.headings = r.headings
        p.text = plain_text(p.html)
        renderers[p.slug] = r
    # reading order: the index, then section by section
    order = ([by_slug["index"]] if "index" in by_slug else []) + [q for _, ps in sections(pages) for q in ps]
    written = set()
    for i, p in enumerate(order):
        desc = p.summary or (p.text[:155] + ("…" if len(p.text) > 155 else ""))
        kicker = "Spike wiki" if p.slug == "index" else p.section
        page = fill(tpl, {
            "title": html.escape(p.title),
            "doc_title": html.escape(p.title if p.slug != "index" else "Spike wiki") + ("" if p.slug == "index" else " · Spike wiki"),
            "description": html.escape(desc),
            "canonical": f"{BASE_URL}wiki/{p.slug}.html" if p.slug != "index" else f"{BASE_URL}wiki/",
            "kicker": html.escape(kicker),
            "summary": f'<p class="lede">{renderers[p.slug].inline(p.summary)}</p>' if p.summary else "",
            "sidebar": sidebar(pages, p),
            "content": p.html,
            "toc": toc(p),
            "pager": pager(order, i),
            "edit": f"{REPO_EDIT}{p.path.relative_to(ROOT)}",
            "source": f"{REPO_BLOB}{p.path.relative_to(ROOT)}",
        })
        (OUT / f"{p.slug}.html").write_text(page, encoding="utf-8")
        written.add(f"{p.slug}.html")
    # pages that no longer have a source
    for f in OUT.glob("*.html"):
        if f.name not in written:
            f.unlink()
    # search index
    idx = [{"s": p.slug, "t": p.title, "c": p.section if p.slug != "index" else "", "u": p.summary,
            "h": [[hid, text] for lvl, hid, text in p.headings if lvl in (2, 3)], "x": p.text} for p in order]
    (OUT / "search.json").write_text(json.dumps(idx, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")
    # sitemap: the landing page and every wiki page
    urls = [BASE_URL, f"{BASE_URL}wiki/"] + [f"{BASE_URL}wiki/{p.slug}.html" for p in order if p.slug != "index"]
    sm = ['<?xml version="1.0" encoding="UTF-8"?>', '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">']
    sm += [f"  <url><loc>{u}</loc></url>" for u in urls]
    sm.append("</urlset>")
    (SITE / "sitemap.xml").write_text("\n".join(sm) + "\n", encoding="utf-8")
    print(f"wiki: {len(order)} pages, search.json {len((OUT / 'search.json').read_bytes()) // 1024} KB, sitemap {len(urls)} urls")

    if not check:
        return 0
    bad = 0
    for p in order:
        r = renderers[p.slug]
        for href in dict.fromkeys(r.links + r.images):
            path, _, frag = href.partition("#")
            target = (OUT / path).resolve() if path else OUT / f"{p.slug}.html"
            if path and not target.exists():
                print(f"{p.path.name}: broken link or image: {href}")
                bad += 1
                continue
            if frag and target.suffix == ".html":
                slug = target.stem
                ids = {h[1] for h in by_slug[slug].headings} if slug in by_slug else set()
                if slug in by_slug and frag not in ids:
                    print(f"{p.path.name}: missing anchor: {href}")
                    bad += 1
    print("links: ok" if not bad else f"links: {bad} problem(s)")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(build("--check" in sys.argv))
