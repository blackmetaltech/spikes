#!/usr/bin/env python3
"""Hostile-circuit fuzz test (ctest: fuzz_circuits).

usage: run_fuzz.py <spike-run> [--ms 200] [--seconds 10] [--mb 1024]

Generates the corpus (gen_corpus.py: ~230 valid-JSON circuits that ask for absurd sizes, rates, delays, weights, nesting, unknown models ...) and runs each
through `spike-run <file> <ms>` with an address-space limit and a wall-clock limit. Every case must end with exit status 0 (it ran) or 1 (a clean error
message) within the limits: a signal (abort, segfault, out of memory), a timeout or any other status fails the test. The case named "ok" must run (exit 0).
"""
import concurrent.futures
import os
import resource
import subprocess
import sys
import tempfile
import time

here = os.path.dirname(os.path.abspath(__file__))
exe = os.path.abspath(sys.argv[1])
opt = dict(zip(sys.argv[2::2], sys.argv[3::2]))
ms = opt.get("--ms", "200")
limit_s = float(opt.get("--seconds", "10"))
limit_mb = int(opt.get("--mb", "1024"))


def limits():
    resource.setrlimit(resource.RLIMIT_AS, (limit_mb << 20, limit_mb << 20))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def run(path):
    t = time.time()
    p = subprocess.Popen([exe, path, ms], stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, preexec_fn=limits)
    try:
        _, err = p.communicate(timeout=limit_s)
        rc = p.returncode
    except subprocess.TimeoutExpired:
        p.kill()
        _, err = p.communicate()
        rc = "TIMEOUT"
    msg = err.decode(errors="replace").strip().splitlines()
    return os.path.basename(path)[:-5], rc, time.time() - t, msg[-1][:160] if msg else ""


with tempfile.TemporaryDirectory() as d:
    subprocess.check_call([sys.executable, os.path.join(here, "gen_corpus.py"), d], stdout=subprocess.DEVNULL)
    files = sorted(os.path.join(d, f) for f in os.listdir(d) if f.endswith(".json"))
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, min(4, os.cpu_count() or 1))) as ex:
        results = list(ex.map(run, files))
bad = []
for name, rc, secs, msg in results:
    ok = rc in (0, 1) and secs < limit_s
    if name == "ok" and rc != 0:
        ok = False
    if not ok:
        bad.append((name, rc, secs, msg))
print("%d cases, %d failing (limits: %g s, %d MB, exit status 0 or 1)" % (len(results), len(bad), limit_s, limit_mb))
for name, rc, secs, msg in bad:
    print("FAIL %-28s rc=%s %.1fs %s" % (name, rc, secs, msg))
sys.exit(1 if bad else 0)
