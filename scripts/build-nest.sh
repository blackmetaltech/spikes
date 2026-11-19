#!/usr/bin/env bash
# Build NEST (pinned) as static libraries for embedding: no MPI, Python/Cython only needed at build time.
#   scripts/build-nest.sh            static libs for the engine (third_party/nest-build)
#   scripts/build-nest.sh --pynest   additionally build and install NEST with PyNEST into third_party/nest-install (this NEST always builds PyNEST; there is no with-python option)
#                                    (same pinned commit; needs python3, cython, numpy; python/tests find it there)
# Env: NEST_REF (commit), SPIKE_OPENMP=ON|OFF (use the same value for the engine and PyNEST for spike parity).
set -euo pipefail
PYNEST=0
for a in "$@"; do case "$a" in --pynest) PYNEST=1 ;; *) echo "usage: $0 [--pynest]" >&2; exit 2 ;; esac; done
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NEST_REF="${NEST_REF:-7e9111711d4a6ae3b3417d82a9783097ec26b49c}"
SRC="$ROOT/third_party/nest-simulator"; BUILD="$ROOT/third_party/nest-build"; PREFIX="$ROOT/third_party/nest-install"
OPENMP="${SPIKE_OPENMP:-OFF}"
if [ ! -d "$SRC/.git" ]; then git clone -q https://github.com/nest/nest-simulator.git "$SRC"; fi
git -C "$SRC" fetch -q --depth 1 origin "$NEST_REF" 2>/dev/null || true
git -C "$SRC" checkout -q "$NEST_REF"
if [ "$PYNEST" = 1 ]; then
  PYBUILD="$ROOT/third_party/nest-build-py"; mkdir -p "$PYBUILD"; cd "$PYBUILD"
  cmake "$SRC" -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_BUILD_TYPE=Release \
    -Dwith-mpi=OFF -Dwith-openmp="$OPENMP" -Dwith-boost=OFF -Dwith-ltdl=OFF -Dwith-gsl=ON >/dev/null
  make -j"$(nproc)" install
  echo "PyNEST installed under: $(ls -d "$PREFIX"/lib/python3*/site-packages 2>/dev/null)"
  echo "Use: export SPIKE_PYNEST=<that dir>  (python/tests also find it automatically)"
  exit 0
fi
mkdir -p "$BUILD"; cd "$BUILD"
cmake "$SRC" -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -Dwith-mpi=OFF -Dwith-openmp="$OPENMP" -Dwith-boost=OFF -Dwith-ltdl=OFF -Dwith-gsl=ON >/dev/null
make -j"$(nproc)" models nestkernel nestutil
echo "NEST static libs:"; find "$BUILD" -name '*.a'
