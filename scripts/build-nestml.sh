#!/usr/bin/env bash
# Generate the custom NEST cell models (models/nestml/*.nestml) to C++ with NESTML (pip `nestml` 9.x).
#   scripts/build-nestml.sh            -> third_party/nestml-gen/   (engine CMake picks it up on the next configure)
#   scripts/build-nestml.sh --venv-only  only create the pinned venv (the toolchain image does this)
#   scripts/build-nestml.sh --pynest   additionally build libspike_nestml_py.so against third_party/nest-install
#                                      (PyNEST parity: see models/nestml/spike_nestml.py)
# Python: $NESTML_PYTHON, else a venv in third_party/nestml-venv (created, nestml==$NESTML_VERSION installed from PyPI, on first use).
# NESTML is pinned to the version that generated the committed model expectations; override only deliberately (NESTML_VERSION=...).
# Then: cmake -S . -B build && cmake --build build -j
set -euo pipefail
PYNEST=0; VENV_ONLY=0
for a in "$@"; do case "$a" in --pynest) PYNEST=1 ;; --venv-only) VENV_ONLY=1 ;; *) echo "usage: $0 [--pynest|--venv-only]" >&2; exit 2 ;; esac; done
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/scripts/prebuilt.sh"
GEN="$ROOT/third_party/nestml-gen"
NESTML_VERSION="${NESTML_VERSION:-9.0.1}"
PY="${NESTML_PYTHON:-}"
if [ -z "$PY" ]; then
  VENV="$ROOT/third_party/nestml-venv"
  [ -e "$VENV" ] || { [ -z "${SPIKE_NO_PREBUILT:-}" ] && use_prebuilt nestml-venv || true; }   # the image's venv, if it has this pin
  if [ ! -x "$VENV/bin/python" ]; then python3 -m venv "$VENV"; fi
  if [ "$("$VENV/bin/python" -c 'import importlib.metadata as m; print(m.version("nestml"))' 2>/dev/null)" != "$NESTML_VERSION" ]; then
    "$VENV/bin/pip" install -q "nestml==$NESTML_VERSION"
  fi
  PY="$VENV/bin/python"
fi
"$PY" -c 'import pynestml' 2>/dev/null || { echo "pynestml not importable with $PY (pip install nestml==$NESTML_VERSION)" >&2; exit 1; }
have=$("$PY" -c 'import importlib.metadata as m; print(m.version("nestml"))')
[ "$have" = "$NESTML_VERSION" ] || { echo "NESTML $have found, $NESTML_VERSION is pinned (set NESTML_VERSION to override)" >&2; exit 1; }
[ "$VENV_ONLY" = 0 ] || { echo "NESTML $have venv ready"; exit 0; }
"$PY" "$ROOT/scripts/nestml_generate.py" "$ROOT/models/nestml" "$GEN"
touch "$GEN/.stamp"

if [ "$PYNEST" = 1 ]; then
  # PyNEST is one monolithic nestkernel_api.so with the kernel linked in and no ltdl, so nest.Install() is unavailable.
  # Instead build the same generated sources into a small shared library whose NEST symbols are resolved at load time
  # from nestkernel_api.so (models/nestml/spike_nestml.py loads both RTLD_GLOBAL and calls spike_nestml_register()).
  # Headers come from the PyNEST *install* so they match the kernel it loaded.
  PREFIX="${NEST_INSTALL:-$ROOT/third_party/nest-install}"
  [ -d "$PREFIX/include/nest" ] || PREFIX=/tmp/nest-install
  [ -d "$PREFIX/include/nest" ] || { echo "no NEST install with headers (run scripts/build-nest.sh --pynest or set NEST_INSTALL)" >&2; exit 1; }
  OUT="$ROOT/third_party/nestml-py"; mkdir -p "$OUT"
  { cat "$GEN/nestml_includes.inc"; echo 'extern "C" void spike_nestml_register() {'; cat "$GEN/nestml_register.inc"; echo '}'; } > "$OUT/register.cpp"
  g++ -std=c++20 -O2 -fPIC -shared -w -I"$GEN" -I"$PREFIX/include/nest" -I"$PREFIX/include/nest/Random123" \
    "$GEN"/*.cpp "$OUT/register.cpp" -o "$OUT/libspike_nestml_py.so" -lgsl -lgslcblas
  echo "built $OUT/libspike_nestml_py.so (headers: $PREFIX/include/nest)"
fi
