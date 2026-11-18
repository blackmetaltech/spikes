#!/usr/bin/env python3
"""Citation registry checker.

The registry is catalog/citations.json. Every reference the game shows (notebook notes, concept cards,
lesion hints) or the docs make must be in it, with a DOI where one exists.

Modes
  check.py                 offline: registry sanity + scan the repo for citations that are not in the
                           registry + fail on retired (known wrong) strings. Needs no network. Use in CI.
  check.py --online        also ask Crossref (api.crossref.org) about every DOI in the registry and compare
                           title, first author, year, volume and first page. Fails on any mismatch.
  check.py --online --fill also look up entries that have no DOI (journal articles only) by title and write
                           the DOI into the registry when title, first author and year agree.
  check.py --write-md      regenerate docs/CITATIONS.md from the registry.

Exit status is 0 when everything checked passes, 1 otherwise.
"""
import argparse, difflib, json, os, re, sys, time, unicodedata, urllib.error, urllib.parse, urllib.request

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
REG = os.path.join(ROOT, "catalog", "citations.json")
MD = os.path.join(ROOT, "docs", "CITATIONS.md")
SCAN = ["engine/src", "docs", "godot/game", "python", "README.md", "CHANGELOG.md"]
SKIP_FILES = ("REVIEW-HARSH", "CRITIQUE", "CITATIONS", "REQ-0.", "PLAN.md", "NOVICE")
EXT = (".cpp", ".h", ".md", ".gd", ".py", ".ipynb")
DOI_RE = re.compile(r"^10\.\d{4,9}/\S+$")
# "Surname I, Surname J (1999)", "Surname & Surname 1999", "Surname et al. 1999", "Surname (1999)"
CITE_RE = re.compile(
    r"\b([A-Z][A-Za-zÀ-ſ'-]{2,})(?: [A-Z]{1,3}(?![a-z]))?"
    r"(?:,? (?:&|and) [A-Z][A-Za-zÀ-ſ'-]+(?: [A-Z]{1,3}(?![a-z]))?|,? et al\.?)?,? ?\(?((?:18|19|20)\d\d)[a-z]?\b")
# words that look like "Name 2000" but are not citations
IGNORE = {"Stand", "Lie", "Version", "Seed", "Seeds", "Tier", "Level", "Day", "Year", "March", "April", "May", "June", "July",
          "August", "September", "October", "November", "December", "January", "February", "Spike", "Godot", "NEST", "Python",
          "Ubuntu", "Windows", "Linux", "Copyright", "Standard", "Sim", "Pass", "Fail", "Max", "Min", "Rev", "Ver",
          "CIEDE", "WJEP", "Machado", "GDC", "ESC", "Hz", "Total", "Run", "Step", "Time", "Cell", "Cells", "Pool", "Pop", "Table", "Figure", "Fig", "Mode", "Case"}


def load():
    with open(REG, encoding="utf-8") as f:
        return json.load(f)


def files():
    for s in SCAN:
        p = os.path.join(ROOT, s)
        if os.path.isfile(p):
            yield p
            continue
        for dp, dn, fn in os.walk(p):
            dn[:] = [d for d in dn if d not in ("__pycache__", ".godot", "build", "third_party")]
            for f in fn:
                if f.endswith(EXT) and not any(k in f for k in SKIP_FILES):
                    yield os.path.join(dp, f)


def norm(s):
    return re.sub(r"[^a-z0-9]+", " ", (s or "").lower()).strip()


def strip_accents(s):
    return "".join(ch for ch in unicodedata.normalize("NFKD", s) if not unicodedata.combining(ch))


def surname_key(s):
    return norm(strip_accents(re.split(r"[ ,]", s.strip())[0]))


def surname_tokens(a):
    """All words of an author's family name (drops trailing initials): 'van Rossum MCW' -> {van, rossum}."""
    return set(norm(strip_accents(w)) for w in re.findall(r"[A-Za-z\u00C0-\u017F'-]{2,}", re.sub(r"\s[A-Z]{1,4}$", "", a.strip())))


# --------------------------------------------------------------------------- offline
def offline(reg, verbose):
    bad = 0
    ids = set()
    required = ("id", "authors", "year", "title", "journal", "status", "verified_by")
    for c in reg["citations"]:
        for k in required:
            if k not in c or c[k] in (None, "", []):
                print("registry: %s missing %s" % (c.get("id", "?"), k)); bad += 1
        if c["id"] in ids:
            print("registry: duplicate id", c["id"]); bad += 1
        ids.add(c["id"])
        if c.get("doi") and not DOI_RE.match(c["doi"]):
            print("registry: %s has a malformed DOI %r" % (c["id"], c["doi"])); bad += 1
        if c["status"] not in ("verified", "verified-locus", "unverified"):
            print("registry: %s bad status %r" % (c["id"], c["status"])); bad += 1
        if c.get("match"):
            try:
                re.compile(c["match"])
            except re.error as e:
                print("registry: %s bad match regex: %s" % (c["id"], e)); bad += 1

    known = {}
    for c in reg["citations"]:
        for a in c["authors"][:1]:
            known.setdefault((surname_key(a), int(c["year"])), []).append(c["id"])
    # also accept any author in the list (second authors are often cited alone)
    anyauth = set()
    for c in reg["citations"]:
        for a in c["authors"]:
            for t in surname_tokens(a):
                anyauth.add((t, int(c["year"])))
    rx = [(c["id"], re.compile(c["match"])) for c in reg["citations"] if c.get("match")]
    retired = [(r, re.compile(r["pattern"])) for r in reg.get("retired", [])]

    unresolved = {}
    for p in files():
        text = open(p, encoding="utf-8", errors="ignore").read()
        rel = os.path.relpath(p, ROOT)
        for r, rr in retired:
            if rr.search(text):
                print("RETIRED  %s: %s  (%s)" % (rel, r["pattern"], r["reason"])); bad += 1
        for m in CITE_RE.finditer(text):
            name, year = re.sub(r"'s$", "", m.group(1)), int(m.group(2))
            if name in IGNORE or not (1850 <= year <= 2026):
                continue
            if (norm(strip_accents(name)), year) in anyauth:
                continue
            unresolved.setdefault("%s %d" % (name, year), set()).add(rel)
    if unresolved:
        print("\nCitation-like strings with no registry entry (add them, or add the word to IGNORE):")
        for k in sorted(unresolved):
            print("  %-28s %s" % (k, ", ".join(sorted(unresolved[k]))[:120]))
        bad += len(unresolved)
    stats = {}
    for c in reg["citations"]:
        stats[c["status"]] = stats.get(c["status"], 0) + 1
    print("\nregistry: %d entries %s" % (len(reg["citations"]), stats))
    return bad


# --------------------------------------------------------------------------- online
UA = "spike-citation-check/1.0 (open-source game; mailto set via CROSSREF_MAILTO env if you have one)"


def crossref(path, params=None):
    url = "https://api.crossref.org/" + path
    if params:
        url += "?" + urllib.parse.urlencode(params)
    mt = os.environ.get("CROSSREF_MAILTO")
    req = urllib.request.Request(url, headers={"User-Agent": UA + ("" if not mt else " mailto:" + mt)})
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.load(r)["message"]


def first_page(p):
    return re.split(r"[-–—]", str(p or ""))[0].strip().lower()


def compare(c, m):
    """Return a list of problems comparing a registry entry with a Crossref work."""
    out = []
    t = (m.get("title") or [""])[0]
    sim = difflib.SequenceMatcher(None, norm(t), norm(c["title"])).ratio()
    if c["title"] and not c["title"].startswith("(title not read") and sim < 0.85:
        out.append("title differs (%.2f): crossref %r" % (sim, t))
    au = m.get("author") or []
    if au and c["authors"]:
        fam = norm(au[0].get("family", ""))
        mine = surname_key(c["authors"][0])
        if fam and mine and mine not in fam and fam.split(" ")[-1] != mine:
            out.append("first author differs: crossref %r, registry %r" % (au[0].get("family"), c["authors"][0]))
    yr = None
    for k in ("issued", "published-print", "published-online"):
        if m.get(k, {}).get("date-parts", [[None]])[0][0]:
            yr = m[k]["date-parts"][0][0]; break
    if yr and abs(int(yr) - int(c["year"])) > 1:
        out.append("year differs: crossref %s, registry %s" % (yr, c["year"]))
    if c.get("volume") and m.get("volume") and norm(str(c["volume"])).split(" ")[0] != norm(str(m["volume"])).split(" ")[0]:
        out.append("volume differs: crossref %s, registry %s" % (m["volume"], c["volume"]))
    if c.get("pages") and m.get("page") and first_page(c["pages"]) != first_page(m["page"]):
        out.append("first page differs: crossref %s, registry %s" % (m["page"], c["pages"]))
    return out


def online(reg, fill, sleep):
    bad = 0
    changed = False
    for c in reg["citations"]:
        if c.get("doi"):
            try:
                m = crossref("works/" + urllib.parse.quote(c["doi"]))
            except urllib.error.HTTPError as e:
                print("FAIL %-18s DOI %s does not resolve (HTTP %d)" % (c["id"], c["doi"], e.code)); bad += 1
                time.sleep(sleep); continue
            except Exception as e:
                print("ERROR %-17s %s" % (c["id"], e)); bad += 1
                continue
            probs = compare(c, m)
            if probs:
                bad += 1
                print("FAIL %-18s %s" % (c["id"], "; ".join(probs)))
            else:
                print("ok   %-18s %s" % (c["id"], c["doi"]))
        elif fill and c["journal"] and "(book)" not in c["journal"] and c["journal"] != "thesis" and c["authors"]:
            try:
                items = crossref("works", {"query.bibliographic": "%s %s %s" % (c["title"], c["authors"][0], c["year"]), "rows": 3})["items"]
            except Exception as e:
                print("ERROR %-17s %s" % (c["id"], e)); continue
            hit = next((m for m in items if not compare(c, m)), None)
            if hit:
                c["doi"] = hit["DOI"]; changed = True
                print("fill %-18s %s" % (c["id"], c["doi"]))
            else:
                print("none %-18s no Crossref record agrees on title, author and year" % c["id"])
        time.sleep(sleep)
    if changed:
        with open(REG, "w", encoding="utf-8") as f:
            json.dump(reg, f, indent=1, ensure_ascii=False)
        print("registry updated; re-run with --write-md")
    return bad


# --------------------------------------------------------------------------- markdown
def write_md(reg):
    lines = ["# Citations", "",
             "Generated from `catalog/citations.json` by `python3 tools/citations/check.py --write-md`. Do not edit by hand.", "",
             "Every reference in the notebooks, concept cards, lesion hints and docs is listed here. Status:", "",
             "- **verified**: title, authors, journal, volume, pages and DOI were read from the PubMed record.",
             "- **verified-locus**: a PubMed record exists at exactly this journal, year, volume, first page and first author. The title is as cited in the repo.",
             "- **unverified**: could not be checked (not indexed by PubMed, or a book). Kept because the work is real and classic, but marked so.", "",
             "Run `python3 tools/citations/check.py` (offline) or `--online` (Crossref DOI, title, author, year, volume and page check).", ""]
    st = {}
    for c in reg["citations"]:
        st[c["status"]] = st.get(c["status"], 0) + 1
    lines += ["Counts: %d entries; %s." % (len(reg["citations"]), ", ".join("%d %s" % (v, k) for k, v in sorted(st.items()))), ""]
    if reg.get("retired"):
        lines += ["## Corrections (wrong citations found and fixed)", "", "| Was | Problem | Now |", "|---|---|---|"]
        for r in reg["retired"]:
            lines.append("| %s | %s | %s |" % (r.get("was", r["pattern"]).replace("|", "\\|"), r["reason"], r["replaced_by"]))
        lines.append("")
    lines += ["## Registry", "", "| id | citation | DOI | status | used in |", "|---|---|---|---|---|"]
    for c in sorted(reg["citations"], key=lambda c: c["id"]):
        au = c["authors"]
        a = au[0] if len(au) == 1 else (au[0] + " & " + au[1] if len(au) == 2 else au[0] + " et al.")
        loc = " ".join(x for x in (c["journal"], str(c["volume"] or ""), ":" + c["pages"] if c.get("pages") else "") if x).replace(" :", ":")
        doi = "[%s](https://doi.org/%s)" % (c["doi"], c["doi"]) if c.get("doi") else ""
        ui = c.get("used_in", [])
        used = "<br>".join(ui[:4]) + (" (+%d)" % (len(ui) - 4) if len(ui) > 4 else "")
        title = c["title"].replace("|", "/")
        lines.append("| `%s` | %s (%s) %s. %s | %s | %s | %s |" % (c["id"], a, c["year"], title, loc, doi, c["status"], used))
    notes = [c for c in reg["citations"] if c.get("note")]
    lines += ["", "## Notes on individual entries", ""]
    for c in sorted(notes, key=lambda c: c["id"]):
        lines.append("- `%s`: %s" % (c["id"], c["note"]))
    with open(MD, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print("wrote", MD)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--online", action="store_true", help="check every DOI against Crossref")
    ap.add_argument("--fill", action="store_true", help="with --online: look up missing DOIs and write them")
    ap.add_argument("--write-md", action="store_true", help="regenerate docs/CITATIONS.md")
    ap.add_argument("--sleep", type=float, default=0.1, help="seconds between Crossref requests")
    a = ap.parse_args()
    reg = load()
    bad = offline(reg, True)
    if a.online:
        bad += online(reg, a.fill, a.sleep)
    if a.write_md:
        write_md(load())
    print("FAILED: %d problem(s)" % bad if bad else "OK")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
