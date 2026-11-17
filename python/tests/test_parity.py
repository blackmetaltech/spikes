"""Parity: the same circuit, seed and thread count give identical spikes in the C++ engine (spike-run) and in PyNEST.

Both must use the same NEST build (same commit, same OpenMP setting): the repo's third_party/nest-build (static, linked
into spike-run) and the PyNEST install of that same commit.
"""
import json
import os
import subprocess

import pytest

from conftest import requires_nest

pytestmark = requires_nest

CASES = [
    ("ei_net.json", 500.0),
    ("half_centre.json", 800.0),
    ("gap_junctions.json", 400.0),
    ("short_term_plasticity.json", 600.0),
    ("dopamine_stdp.json", 600.0),
    ("weight_spread.json", 600.0),  # projection weight_sd / weight_cv: seeded per-edge spread, same draws in both
    ("sa_clones.json", 4000.0),  # NESTML cells with the per-model V_m range (skips when the NESTML PyNEST library is not built)
]


def cpp_spikes(exe, path, ms, threads):
    out = subprocess.run([exe, str(path), str(ms), "--threads", str(threads)], capture_output=True, text=True, check=True)
    return sorted((j["step"], j["neuron"]) for j in map(json.loads, out.stdout.splitlines()))


@pytest.mark.parametrize("name,ms", CASES)
def test_spikes_identical(spike_run_exe, circuits_dir, name, ms):
    if spike_run_exe is None:
        pytest.skip("spike-run not built (cmake --build build --target spike-run)")
    import spike

    if name == "sa_clones.json":
        try:
            import spike_nestml  # noqa: F401  (models/nestml is put on sys.path by spike.circuit)
        except ImportError:
            pass
        spike.circuit._register_nestml()
        import nest

        if "sa_node_pacemaker" not in nest.node_models:
            if os.environ.get("SPIKE_REQUIRE_NEST", "") not in ("", "0"):
                pytest.fail("SPIKE_REQUIRE_NEST=1 but the NESTML PyNEST library is not built (scripts/build-nestml.sh --pynest)")
            pytest.skip("NESTML PyNEST library not built")
    path = circuits_dir / name
    res = json.loads(path.read_text()).get("resolution_ms", 0.1)
    cpp = cpp_spikes(spike_run_exe, path, ms, 1)
    py = spike.run(spike.build(path, threads=1), ms).steps(res)
    assert len(cpp) > 0, "fixture must produce spikes"
    assert cpp == py
