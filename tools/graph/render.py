#!/usr/bin/env python3
"""Validate godot/tests/interaction_graph.json and render its mermaid diagrams into docs/INTERACTION-GRAPH.md.

  tools/graph/render.py           rewrite the generated block of docs/INTERACTION-GRAPH.md
  tools/graph/render.py --check   fail (exit 1) if the JSON is malformed or the committed diagram is stale (ctest: interaction_graph_static)
"""
import json, re, sys, pathlib

ROOT = pathlib.Path(__file__).resolve().parents[2]
GRAPH = ROOT / "godot/tests/interaction_graph.json"
DOC = ROOT / "docs/INTERACTION-GRAPH.md"
BEGIN, END = "<!-- BEGIN GENERATED (tools/graph/render.py) -->", "<!-- END GENERATED -->"


def mid(n):
    return re.sub(r"[^A-Za-z0-9]", "_", n)


def esc(s):
    return s.replace('"', "'")


def validate(g):
    errs = []
    nodes, edges = g["nodes"], g["edges"]
    if g["entry"] not in nodes: errs.append("entry is not a node")
    seen = set()
    for e in edges:
        if e["id"] in seen: errs.append("duplicate edge id " + e["id"])
        seen.add(e["id"])
        for k in ("from", "to"):
            if e[k] not in nodes: errs.append("edge %s: unknown node %s" % (e["id"], e[k]))
    for nid, n in nodes.items():
        for k in ("kind", "label", "detect"):
            if k not in n: errs.append("node %s lacks %s" % (nid, k))
        for k in ("co", "state_of"):
            for r in ([n[k]] if isinstance(n.get(k), str) else n.get(k, [])):
                if r not in nodes: errs.append("node %s: %s refers to unknown node %s" % (nid, k, r))
    return errs


def diagram(g, title, keep):
    nodes, edges = g["nodes"], g["edges"]
    ids = [n for n in nodes if keep(n)]
    out = ["```mermaid", "flowchart LR"]
    shape = {"screen": ('["', '"]'), "modal": ('[["', '"]]'), "tab": ('("', '")'), "panel": ('("', '")'), "overlay": ('{{"', '"}}'),
             "toast": ('>"', '"]'), "state": ('("', '")'), "frame": ('["', '"]')}
    for n in ids:
        a, b = shape.get(nodes[n]["kind"], ('["', '"]'))
        out.append("  %s%s%s%s" % (mid(n), a, esc(nodes[n]["label"]), b))
    chapter_tabs = [n for n in ids if n.startswith("campaign.chapter.")]
    done = set()
    for e in g["edges"]:
        if e["from"] not in ids or e["to"] not in ids: continue
        if e["to"] in chapter_tabs or e["from"] in chapter_tabs:
            key = (e["from"], "chapters")
            if key in done: continue
            done.add(key)
            out.append("  %s -->|Chapter tabs| %s" % (mid(e["from"]), mid(chapter_tabs[0])))
            continue
        label = e["control"] or e.get("action", "").replace("event:", "event: ")
        if e.get("guard"): label += " (" + e["guard"] + ")"
        arrow = "-.->" if e.get("action") == "terminal" else "-->"
        out.append("  %s %s|%s| %s" % (mid(e["from"]), arrow, esc(label), mid(e["to"])))
        if e.get("returns") and e["from"] != e["to"]:
            out.append("  %s -.->|Back or Esc| %s" % (mid(e.get("returns_to", e["from"]) if False else e["to"]), mid(e.get("returns_to", e["from"]))))
    out.append("```")
    return "\n".join(out)


def build(g):
    app = diagram(g, "screens", lambda n: not n.startswith("lab.") and n != "lab" and not n.startswith("analysis."))
    lab = diagram(g, "lab", lambda n: n.startswith("lab.") or n in ("campaign", "title", "analysis"))
    rows = ["| Node | Kind | What it is | Controls |", "|---|---|---|---|"]
    for nid, n in g["nodes"].items():
        ctl = ", ".join("`%s`" % c for c in n.get("controls", []) + [c + "?" for c in n.get("optional", [])])
        rows.append("| `%s` | %s | %s | %s |" % (nid, n["kind"], n["label"], ctl))
    neg = ["| Guard | Expectation |", "|---|---|"] + ["| `%s` on `%s`, %s | %s |" % (x["control"], x["node"], x["setup"], x["expect"] + ": " + x["why"]) for x in g.get("negatives", [])]
    return "\n\n".join([
        "### Screens, campaign and settings\n\n" + app,
        "### The lab, its modals and the analysis\n\n" + lab,
        "### Nodes (`?` = present only when its guard holds)\n\n" + "\n".join(rows),
        "### Guards that must hide or disable a control\n\n" + "\n".join(neg),
        "Counts: %d nodes, %d edges, %d guard checks." % (len(g["nodes"]), len(g["edges"]), len(g.get("negatives", []))),
    ])


def main():
    g = json.loads(GRAPH.read_text())
    errs = validate(g)
    if errs:
        print("\n".join("interaction_graph.json: " + e for e in errs)); return 1
    block = BEGIN + "\n" + build(g) + "\n" + END
    doc = DOC.read_text()
    if BEGIN not in doc or END not in doc:
        print("docs/INTERACTION-GRAPH.md lacks the generated-block markers"); return 1
    new = re.sub(re.escape(BEGIN) + ".*" + re.escape(END), lambda m: block, doc, flags=re.S)
    if "--check" in sys.argv:
        if new != doc:
            print("docs/INTERACTION-GRAPH.md is stale: run tools/graph/render.py and commit it with the JSON"); return 1
        print("interaction graph: JSON valid, diagram up to date (%d nodes, %d edges)" % (len(g["nodes"]), len(g["edges"]))); return 0
    DOC.write_text(new); print("wrote", DOC); return 0


sys.exit(main())
