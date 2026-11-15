import json
import random
import subprocess

import networkx as nx
import pytest

from spike import motifs

# networkx triad code -> our class (via the MAN name, independent of our canonical-id code)
MAN = {m.man: m.index for m in motifs.MOTIFS}


def random_edges(n, p, seed, p_neg=0.0):
    rng = random.Random(seed)
    return [(a, b, -1.0 if rng.random() < p_neg else 1.0) for a in range(n) for b in range(n) if a != b and rng.random() < p]


def nx_census(edges, n):
    g = nx.DiGraph()
    g.add_nodes_from(range(n))
    g.add_edges_from((a, b) for a, b, *_ in edges)
    tc = (getattr(nx, "triadic_census", None) or nx.triads_census)(g)
    out = [0] * 13
    for name, v in tc.items():
        if name in MAN:
            out[MAN[name]] = v
    return out


def test_table():
    assert [m.milo_id for m in motifs.MOTIFS] == [6, 12, 14, 36, 38, 46, 74, 78, 98, 102, 108, 110, 238]
    assert motifs.BY_MILO_ID[38].name == "feed_forward_loop"
    assert motifs.BY_MILO_ID[98].name == "feedback_loop"
    assert motifs.BY_MILO_ID[36].name == "fan_in" and motifs.BY_MILO_ID[6].name == "fan_out"
    # every networkx connected triad class maps to a distinct class with the right edge count
    for name in MAN:
        g = nx.triad_graph(name)
        idx = {v: i for i, v in enumerate(g.nodes())}
        c = motifs.census([(idx[a], idx[b]) for a, b in g.edges()], 3)
        assert c[MAN[name]] == 1 and sum(c) == 1
        assert motifs.MOTIFS[MAN[name]].edges == g.number_of_edges()


@pytest.mark.parametrize("seed", range(12))
def test_census_vs_networkx(seed):
    n = 5 + seed * 3
    e = random_edges(n, 0.05 + 0.07 * (seed % 7), seed)
    assert motifs.census(e, n) == nx_census(e, n)


def test_rewire_preserves_degrees():
    g = motifs.make_graph(random_edges(50, 0.1, 3), 50)
    r = motifs.rewire(g, 9)
    assert [len(o) for o in r.out] == [len(o) for o in g.out]
    indeg = lambda G: sorted(sum(1 for o in G.out if b in o) for b in range(G.n))
    assert indeg(g) == indeg(r)
    assert all(a not in o for a, o in enumerate(r.out))
    assert r.out == motifs.rewire(g, 9).out and r.out != motifs.rewire(g, 10).out


def test_sign_filter():
    e = [(0, 1, 1.0), (1, 2, -1.0), (0, 2, 1.0)]
    assert motifs.census(e, 3)[motifs.BY_MAN["030T"].index] == 1
    assert sum(motifs.census(e, 3, "I")) == 0
    assert motifs.census(e, 3, "E")[motifs.BY_MAN["021D"].index] == 1


def test_planted_ffl_significant():
    e = random_edges(80, 0.03, 4)
    for t in range(40):
        x = 2 * t
        e += [(x, x + 1, 1.0), (x + 1, (x + 2) % 80, 1.0), (x, (x + 2) % 80, 1.0)]
    r = motifs.zscores(e, 80, n_null=20, seed=5)
    assert r.z[motifs.BY_MAN["030T"].index] > 5


@pytest.mark.parametrize("sign", ["any", "E", "I"])
def test_cpp_matches_python(analysis_test_exe, sign):
    """The C++ census / null / z-scores equal the Python ones exactly (same generator, same seed)."""
    if analysis_test_exe is None:
        pytest.skip("analysis_test not built yet (cmake --build build --target analysis_test)")
    n = 40
    e = random_edges(n, 0.12, 21, p_neg=0.3)
    text = f"{n} {len(e)}\n" + "\n".join(f"{a} {b} {w}" for a, b, w in e)
    out = subprocess.run([analysis_test_exe, "--census", sign, "6", "99"], input=text, capture_output=True, text=True, check=True).stdout
    cpp = json.loads(out)["motifs"]
    py = motifs.zscores(e, n, sign=sign, n_null=6, seed=99)
    assert [m["count"] for m in cpp] == py.counts
    assert [m["milo_id"] for m in cpp] == [m.milo_id for m in motifs.MOTIFS]
    for c, mean, sd, z in zip(cpp, py.null_mean, py.null_sd, py.z):
        assert c["null_mean"] == pytest.approx(mean, rel=1e-9, abs=1e-12)
        assert c["null_sd"] == pytest.approx(sd, rel=1e-9, abs=1e-12)
        if c["z"] is None:
            assert not (z == z) or z in (float("inf"), float("-inf"))
        else:
            assert c["z"] == pytest.approx(z, rel=1e-9, abs=1e-12)
