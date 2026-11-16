"""Use the NESTML cell models (sa_node_pacemaker, butera_burster, icc_slow_wave) from PyNEST.

PyNEST here is built without ltdl, so nest.Install() cannot load modules. scripts/build-nestml.sh --pynest builds the
same generated C++ the engine links statically into libspike_nestml_py.so; this loader maps it into the process so the
models register with the same NEST kernel, giving spike-for-spike parity with the C++ engine.

    import nest, spike_nestml
    nest.ResetKernel(); spike_nestml.register()      # after EVERY ResetKernel and every change of threads
    n = nest.Create("sa_node_pacemaker")

Registration is dropped by ResetKernel() and by changing the thread count, so call register() after them
(the engine does the same, engine/src/modules/nestml_models.cpp).
"""
import ctypes
import glob
import os

_REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
_lib = None


def _find_lib():
    env = os.environ.get("SPIKE_NESTML_LIB")
    cands = [env] if env else [os.path.join(_REPO, "third_party", "nestml-py", "libspike_nestml_py.so")]
    for c in cands:
        if c and os.path.exists(c):
            return c
    raise FileNotFoundError("libspike_nestml_py.so not found: run scripts/build-nestml.sh --pynest (or set SPIKE_NESTML_LIB)")


def register():
    """Register the NESTML models with the running NEST kernel (idempotent)."""
    global _lib
    import nest
    if "sa_node_pacemaker" in nest.node_models:
        return
    if _lib is None:
        # promote PyNEST's kernel library to the global symbol scope so the module resolves nest:: symbols against it
        api = glob.glob(os.path.join(os.path.dirname(nest.__file__), "nestkernel_api*.so"))[0]
        ctypes.CDLL(api, mode=ctypes.RTLD_GLOBAL)
        _lib = ctypes.CDLL(_find_lib(), mode=ctypes.RTLD_GLOBAL)
    _lib.spike_nestml_register()
