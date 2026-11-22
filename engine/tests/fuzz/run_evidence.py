#!/usr/bin/env python3
"""Reviewer evidence circuits that used to abort or stall the process (ctest: evidence_circuits).

usage: run_evidence.py <spike-run> <fixtures-dir>

  rmf_negdrive.json       the r_meanfield reference with the mean-field input weight -1: GSL's error handler used to abort the process
  breath_chemo_pbc_*.json the breathing reference with `chemo` wired to the PBC cells: the NESTML burster's integrator used to stall for minutes

rmf_negdrive must end with exit status 1 and a message (a clean engine error); the chemo circuits must end with status 0 or 1 after under 30 s of CPU time (CPU, not wall: under a loaded machine the wall clock stretches while the work done does not; a stall burns CPU without end, the old behaviour was 40+ s of it), never a stall or a signal. A generous wall limit (240 s) still catches a process that hangs without using CPU (the dc input is clamped to the NESTML current cap, or the integrator guard raises a clean error)
"""
import os
import resource
import subprocess
import sys
import time

exe, fx = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
cases = [("r_meanfield", "rmf_negdrive.json", (1,)), ("breathing", "breath_chemo_pbc_20.json", (0, 1)), ("breathing", "breath_chemo_pbc_5.json", (0, 1))]
LIMIT_CPU_S = 30.0  # the references need 12-15 s of CPU alone; a stall used to burn 40+ s without end
bad = 0
for task, name, rcs in cases:
    t = time.time()
    ru0 = resource.getrusage(resource.RUSAGE_CHILDREN)
    p = subprocess.Popen([exe, "--task", task, "--circuit", os.path.join(fx, name), "--seed", "1", "--jobs", "1"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        out, err = p.communicate(timeout=240)
        rc = p.returncode
    except subprocess.TimeoutExpired:
        p.kill()
        out, err = p.communicate()
        rc = "TIMEOUT"
    wall = time.time() - t
    ru1 = resource.getrusage(resource.RUSAGE_CHILDREN)
    secs = (ru1.ru_utime - ru0.ru_utime) + (ru1.ru_stime - ru0.ru_stime)
    msg = err.decode(errors="replace").strip().splitlines()
    msg = msg[-1] if msg else ""
    ok = rc in rcs and secs < LIMIT_CPU_S and (rc == 0 or msg != "")
    print("%-26s rc=%s cpu %.1fs (wall %.1fs) %s %s" % (name, rc, secs, wall, "ok" if ok else "FAIL", msg[:140]))
    bad += 0 if ok else 1
sys.exit(1 if bad else 0)
