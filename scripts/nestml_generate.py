#!/usr/bin/env python3
"""Generate NEST C++ for every models/nestml/*.nestml with NESTML 9.x (called by scripts/build-nestml.sh).

NESTML 9.0.1 wants a Python `nest` module to detect the NEST version and to build/install the module; we have
neither (static, ltdl-less build, no dynamic loading), so version detection and the builder are patched out
and the target version is pinned. NEST 3.10-dev uses the NESTExtensionInterface module mechanism, which NESTML
selects for version "main" (post0.dev). We only keep the per-neuron sources; registration is done by
engine/src/modules/nestml_models.cpp from the two .inc files written here.

usage: nestml_generate.py <models-dir> <out-dir>
"""
import glob
import os
import re
import shutil
import sys

from pynestml.codegeneration.nest_builder import NESTBuilder
from pynestml.codegeneration.nest_tools import NESTTools
from pynestml.frontend.pynestml_frontend import generate_target

NESTTools.detect_nest_version = classmethod(lambda cls: "main")  # NEST 3.10.0-post0.dev0
NESTBuilder.build = lambda self: None                            # we compile statically ourselves

models_dir, out_dir = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
tmp = out_dir + ".tmp"
shutil.rmtree(tmp, ignore_errors=True)
os.makedirs(tmp)
failed = generate_target(input_path=models_dir, target_platform="NEST", target_path=tmp,
                         module_name="spike_nestml_module", logging_level="ERROR",
                         codegen_opts={"nest_version": "main", "nest_path": tmp})
if failed:
    sys.exit("NESTML reported errors")
names = sorted(os.path.basename(p)[:-4] for p in glob.glob(os.path.join(tmp, "*.cpp"))
               if not os.path.basename(p).startswith("spike_nestml_module"))
if not names:
    sys.exit("NESTML generated no neuron sources")
# NESTML headers #error out unless NEST has ltdl (they only need it for the dynamic-module path, which we do not use)
guard = '#ifndef HAVE_LIBLTDL\n#error "NEST was compiled without support for dynamic loading. Please install libltdl and recompile NEST."\n#endif\n'
for h in glob.glob(os.path.join(tmp, "*.h")):
    txt = open(h).read()
    if guard in txt:
        open(h, "w").write(txt.replace(guard, "// (ltdl guard removed by scripts/nestml_generate.py: static build)\n"))
# Stall guard (review P0-2): the generated adaptive-step loop `while ( __t < B_.__step )` has no minimum step, so a strong input drives the step size
# to ~0 and one update never finishes. Inject (1) a step-count and minimum-step guard that throws GSLSolverFailure (NEST reports it as a failed simulation,
# the engine turns it into a clean "faulted" error) and (2) a non-finite state check after the loop.
GUARD_PRE = "    unsigned __spike_guard_iters = 0;\n"
GUARD_IN = ("      if ( ++__spike_guard_iters > SPIKE_GUARD_MAX_ITERS or not( B_.__integration_step > SPIKE_GUARD_MIN_STEP_MS ) )\n"
            "      {\n        throw nest::GSLSolverFailure( get_name(), GSL_EFAILED );  // stalled integrator (scripts/nestml_generate.py)\n      }\n")
GUARD_POST = ("    for ( int __spike_i = 0; __spike_i < State_::STATE_VEC_SIZE; ++__spike_i )\n"
              "    {\n      if ( not std::isfinite( S_.ode_state[ __spike_i ] ) )\n      {\n"
              "        throw nest::GSLSolverFailure( get_name(), GSL_EFAILED );  // non-finite state (scripts/nestml_generate.py)\n      }\n    }\n")
DEFS = ("#ifndef SPIKE_GUARD_MAX_ITERS\n#define SPIKE_GUARD_MAX_ITERS 20000u   // integrator steps per simulation step\n#endif\n"
        "#ifndef SPIKE_GUARD_MIN_STEP_MS\n#define SPIKE_GUARD_MIN_STEP_MS 1e-9  // ms\n#endif\n")
loop_re = re.compile(r"(\n)([ \t]*)while \( __t < B_\.__step \)\n([ \t]*)\{\n")
for c in glob.glob(os.path.join(tmp, "*.cpp")):
    if os.path.basename(c).startswith("spike_nestml_module"):
        continue
    txt = open(c).read()
    m = loop_re.search(txt)
    if not m:
        sys.exit("stall guard: no integration loop found in " + c)
    end = txt.index("\n    }\n", m.end()) + len("\n    }\n")  # the loop body ends at the first closing brace at the loop's indentation
    body = txt[m.end():end]
    txt = txt[:m.start()] + "\n" + GUARD_PRE + m.group(2) + "while ( __t < B_.__step )\n" + m.group(3) + "{\n" + GUARD_IN + body + GUARD_POST + txt[end:]
    txt = txt.replace('#include "', DEFS + '#include <cmath>\n#include "', 1)
    open(c, "w").write(txt)
for f in ("CMakeLists.txt", "spike_nestml_module.cpp", "spike_nestml_module.h"):
    os.remove(os.path.join(tmp, f))
with open(os.path.join(tmp, "nestml_includes.inc"), "w") as f:
    f.writelines('#include "%s.h"\n' % n for n in names)
with open(os.path.join(tmp, "nestml_register.inc"), "w") as f:
    f.writelines('register_%s( "%s" );\n' % (n, n) for n in names)
with open(os.path.join(tmp, "nestml_names.inc"), "w") as f:
    f.writelines('"%s",\n' % n for n in names)
with open(os.path.join(tmp, "models.txt"), "w") as f:
    f.writelines(n + "\n" for n in names)
shutil.rmtree(out_dir, ignore_errors=True)
os.rename(tmp, out_dir)
print("NESTML models generated into", out_dir, ":", " ".join(names))
