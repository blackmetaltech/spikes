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
