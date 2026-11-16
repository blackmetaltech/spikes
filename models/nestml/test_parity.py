#!/usr/bin/env python3
"""Spike-for-spike parity of the NESTML models: C++ engine (static module, spike-run) vs PyNEST (shared module).

usage: test_parity.py <spike-run> ; exit 77 (ctest SKIP) when PyNEST or libspike_nestml_py.so is missing.
"""
import glob
import json
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
for p in [os.environ.get("SPIKE_PYNEST", "")] + sorted(glob.glob(os.path.join(REPO, "third_party/nest-install/lib/python3*/site-packages"))) + sorted(glob.glob("/tmp/nest-install/lib/python3*/site-packages")):
    if p and os.path.isdir(p):
        sys.path.insert(0, p)
try:
    import nest
    import spike_nestml
    spike_nestml._find_lib()
except Exception as e:  # noqa: BLE001
    print("SKIP:", e)
    sys.exit(77)

CASES = [  # model, params, ms
    ("sa_node_pacemaker", {"V_m": -60.0, "ne": 0.5, "ach": 0.1}, 12000.0),
    ("butera_burster", {"V_m": -60.0, "g_tonic": 0.35}, 30000.0),
    ("icc_slow_wave", {"ach": 0.5}, 120000.0),
]
spike_run = sys.argv[1]
fails = 0
for model, params, ms in CASES:
    circuit = {"schema_version": 1, "name": "parity", "resolution_ms": 0.1, "seed": 1,
               "populations": [{"id": "N", "model": model, "n": 1, "params": params}], "inputs": [],
               "outputs": [{"name": "o", "kind": "rate", "neurons": [{"pop": "N", "idx": [0]}]}], "record": {"spikes": "all"}}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f:
        json.dump(circuit, f)
    out = subprocess.run([spike_run, f.name, str(ms)], capture_output=True, text=True, check=True).stdout
    os.unlink(f.name)
    eng = [json.loads(l)["step"] for l in out.splitlines() if l.strip()]

    nest.set_verbosity("M_ERROR")
    nest.ResetKernel()
    nest.resolution = 0.1
    nest.rng_seed = 1
    spike_nestml.register()
    n = nest.Create(model, params=params)
    sr = nest.Create("spike_recorder")
    nest.Connect(n, sr)
    nest.Simulate(ms)
    py = [int(round(t / 0.1)) for t in sr.events["times"]]
    ok = eng == py and len(eng) > 3
    print(f"{model}: engine {len(eng)} spikes, PyNEST {len(py)} spikes: {'identical' if ok else 'MISMATCH'}")
    fails += not ok
print("test_nestml_parity:", "FAILED" if fails else "ok")
sys.exit(1 if fails else 0)
