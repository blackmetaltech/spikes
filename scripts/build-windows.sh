#!/usr/bin/env bash
# Build Spike for Windows x86_64 with MinGW-w64 (MSYS2 UCRT64): NEST (static, pinned, patched for MinGW), the NESTML models, godot-cpp, the engine,
#   godot/bin/spike_gd_release.dll   the GDExtension       godot/bin/spike-run.exe   the headless runner behind CHECK
# Run it inside the MSYS2 UCRT64 shell (the workflow .github/workflows/build-windows.yml does). It does not touch the Linux scripts: the pins are read out of them.
#   NEST_REF       scripts/build-nest.sh       NESTML_VERSION  scripts/build-nestml.sh       GODOT_CPP_TAG  scripts/build-gd.sh
# Env: NESTML_PYTHON  a (native) Python with nestml==$NESTML_VERSION installed; otherwise a venv of the MSYS2 python is created and nestml pip-installed.
#      SKIP_NEST=1    reuse third_party/nest-build as it is (the workflow restores it from the cache and skips the step when it is complete).
# NEST does not support native Windows; scripts/patches/*.patch (applied here only) are the fixes it needs. Binaries are linked statically: they may
# import only Windows system DLLs (asserted at the end).
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd); cd "$root"
[ "${MSYSTEM:-}" = UCRT64 ] || { echo "run this in the MSYS2 UCRT64 shell (MSYSTEM=${MSYSTEM:-unset})" >&2; exit 2; }
shopt -s nullglob
unset Python_ROOT_DIR Python2_ROOT_DIR Python3_ROOT_DIR pythonLocation PKG_CONFIG_PATH   # set by actions/setup-python for the native Python: not for MSYS2 CMake
pin() { sed -n "$2" "scripts/$1" | head -n1; }
NEST_REF=$(pin build-nest.sh 's/.*NEST_REF:-\([0-9a-f]\{40\}\)}.*/\1/p')
NESTML_VERSION=$(pin build-nestml.sh 's/.*NESTML_VERSION:-\([0-9.]*\)}.*/\1/p')
GODOT_CPP_TAG=$(pin build-gd.sh 's/.*GODOT_CPP_TAG:-\([^}]*\)}.*/\1/p')
[ -n "$NEST_REF" ] && [ -n "$NESTML_VERSION" ] && [ -n "$GODOT_CPP_TAG" ] || { echo "could not read the pins out of scripts/build-{nest,nestml,gd}.sh" >&2; exit 1; }
echo "pins: NEST $NEST_REF  NESTML $NESTML_VERSION  godot-cpp $GODOT_CPP_TAG"
export CXXFLAGS="${CXXFLAGS:-} -mno-ms-bitfields"   # MinGW lays out bit-fields like MSVC; NEST's SpikeData / TargetData size checks need the GCC layout (the engine gets it from engine/CMakeLists.txt)
export CC=gcc CXX=g++   # NEST rejects -DCMAKE_C_COMPILER (its unknown-option check)
jobs=$(nproc)
step() { echo; echo "=== $*"; }

# ---------------- NEST ----------------
SRC=$root/third_party/nest-simulator; BUILD=$root/third_party/nest-build
step "NEST static libs (MinGW)"
if [ "${SKIP_NEST:-0}" = 1 ] && [ -f "$BUILD/models/libmodels.a" ] && [ -f "$BUILD/nestkernel/libnestkernel.a" ] && [ -f "$BUILD/libnestutil/libnestutil.a" ]; then
  echo "NEST: reusing $BUILD"
else
  if [ ! -d "$SRC/.git" ]; then mkdir -p "$SRC"; git -C "$SRC" init -q; git -C "$SRC" remote add origin https://github.com/nest/nest-simulator.git; fi
  git -C "$SRC" config core.autocrlf false; git -C "$SRC" config core.eol lf   # LF sources, so the patches apply and the objects hash the same everywhere
  git -C "$SRC" cat-file -e "$NEST_REF^{commit}" 2>/dev/null || git -C "$SRC" fetch -q --depth 1 origin "$NEST_REF"
  git -C "$SRC" checkout -q -f "$NEST_REF"          # reverts a previous patch run: only the files a patch touches get a new mtime
  for p in "$root"/scripts/patches/*.patch; do
    echo "patch: $(basename "$p")"; git -C "$SRC" apply --whitespace=nowarn "$p"
  done
  mkdir -p "$BUILD"; : > "$BUILD/nestkernel_api.cxx"   # PyNEST is not built: a placeholder keeps the configure from needing Cython
  cmake -S "$SRC" -B "$BUILD" -G Ninja -DCMAKE_INSTALL_PREFIX="$root/third_party/nest-install" \
    -DPython_EXECUTABLE="$(command -v python)" -DNESTKERNEL_API_CXX="$BUILD/nestkernel_api.cxx" \
    -Dwith-mpi=OFF -Dwith-openmp=OFF -Dwith-boost=OFF -Dwith-ltdl=OFF -Dwith-gsl=ON
  cmake --build "$BUILD" --target models nestkernel nestutil -j"$jobs" -- -k 0   # -k 0: report every failing file in one run
fi
for l in models/libmodels.a nestkernel/libnestkernel.a libnestutil/libnestutil.a; do [ -f "$BUILD/$l" ] || { echo "missing $BUILD/$l" >&2; exit 1; }; done

# ---------------- NESTML ----------------
step "NESTML $NESTML_VERSION models"
PY=${NESTML_PYTHON:-}
if [ -z "$PY" ]; then
  python -m venv third_party/nestml-venv
  PY=$root/third_party/nestml-venv/bin/python
  "$PY" -m pip install -q "nestml==$NESTML_VERSION"
fi
have=$("$PY" -c 'import importlib.metadata as m; print(m.version("nestml"))')
[ "$have" = "$NESTML_VERSION" ] || { echo "NESTML $have found, $NESTML_VERSION is pinned" >&2; exit 1; }
"$PY" scripts/nestml_generate.py models/nestml third_party/nestml-gen
touch third_party/nestml-gen/.stamp

# ---------------- godot-cpp, engine, extension ----------------
step "godot-cpp $GODOT_CPP_TAG"
if [ ! -f third_party/godot-cpp/CMakeLists.txt ]; then
  git clone -q --depth 1 --branch "$GODOT_CPP_TAG" https://github.com/godotengine/godot-cpp third_party/godot-cpp
fi
step "engine, spike-run.exe, spike_gd.dll"
bdir=build/gd-release
cmake -S . -B "$bdir" -G Ninja -DCMAKE_BUILD_TYPE=Release -DSPIKE_BUILD_TESTS=ON -DSPIKE_BUILD_GDEXT=ON -DGODOTCPP_TARGET=template_release
cmake --build "$bdir" -j"$jobs" --target spike_gd spike-run test_golden
mkdir -p godot/bin
cp -f "$bdir/gd-out/spike_gd.dll" godot/bin/spike_gd_release.dll
cp -f "$bdir/engine/spike-run.exe" godot/bin/spike-run.exe
strip --strip-unneeded godot/bin/spike_gd_release.dll godot/bin/spike-run.exe

# ---------------- only system DLLs ----------------
step "imports of the binaries"
bad=0
for f in godot/bin/spike_gd_release.dll godot/bin/spike-run.exe; do
  echo "$f imports:"; objdump -p "$f" | sed -n 's/^\s*DLL Name: /  /p'
  if objdump -p "$f" | grep "DLL Name" | grep -iE 'libgcc|libstdc\+\+|libwinpthread|libgsl|libgomp|msys-|libucrt|libgmp'; then echo "FAIL: $f imports a MinGW/MSYS2 runtime DLL" >&2; bad=1; fi
done
[ "$bad" = 0 ] || exit 1
echo "built godot/bin/spike_gd_release.dll and godot/bin/spike-run.exe; test_golden: $bdir/engine/test_golden.exe"
