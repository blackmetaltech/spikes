"""Parity: the same circuit, seed and thread count give identical spikes in the C++ engine (spike-run) and in PyNEST.

Both must use the same NEST build (same commit, same OpenMP setting): the repo's third_party/nest-build (static, linked
into spike-run) and the PyNEST install of that same commit.
"""
import json
import subprocess

import pytest

from conftest import requires_nest

pytestmark = requires_nest

CASES = [("ei_net.json", 500.0), ("half_centre.json", 800.0)]


def cpp_spikes(exe, path, ms, threads):
    out = subprocess.run([exe, str(path), str(ms), "--threads", str(threads)], capture_output=True, text=True, check=True)
    return sorted((j["step"], j["neuron"]) for j in map(json.loads, out.stdout.splitlines()))


@pytest.mark.parametrize("name,ms", CASES)
def test_spikes_identical(spike_run_exe, circuits_dir, name, ms):
    if spike_run_exe is None:
        pytest.skip("spike-run not built (cmake --build build --target spike-run)")
    import spike

    path = circuits_dir / name
    res = json.loads(path.read_text()).get("resolution_ms", 0.1)
    cpp = cpp_spikes(spike_run_exe, path, ms, 1)
    py = spike.run(spike.build(path, threads=1), ms).steps(res)
    assert len(cpp) > 0, "fixture must produce spikes"
    assert cpp == py
