#!/usr/bin/env bash
# Build NEST (pinned) as static libraries for embedding: no MPI, Python/Cython only needed at build time.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NEST_REF="${NEST_REF:-7e9111711d4a6ae3b3417d82a9783097ec26b49c}"
SRC="$ROOT/third_party/nest-simulator"; BUILD="$ROOT/third_party/nest-build"; PREFIX="$ROOT/third_party/nest-install"
OPENMP="${SPIKE_OPENMP:-OFF}"
if [ ! -d "$SRC/.git" ]; then git clone -q https://github.com/nest/nest-simulator.git "$SRC"; fi
git -C "$SRC" fetch -q --depth 1 origin "$NEST_REF" 2>/dev/null || true
git -C "$SRC" checkout -q "$NEST_REF"
mkdir -p "$BUILD"; cd "$BUILD"
cmake "$SRC" -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -Dwith-mpi=OFF -Dwith-openmp="$OPENMP" -Dwith-boost=OFF -Dwith-ltdl=OFF -Dwith-gsl=ON >/dev/null
make -j"$(nproc)" models nestkernel nestutil
echo "NEST static libs:"; find "$BUILD" -name '*.a'
