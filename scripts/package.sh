#!/usr/bin/env bash
# Package the game for Linux x86_64 and smoke-test the result.
#   scripts/package.sh              build the release extension + spike-run (scripts/build-gd.sh release), export, zip, smoke-test the zip
#   SKIP_BUILD=1 scripts/package.sh reuse godot/bin/{libspike_gd_release.so,spike-run} as they are
# Output: build/spike-linux-x86_64.zip, containing
#   Spike.x86_64, Spike.pck            the exported game (res://circuits and res://catalog are inside the pck)
#   libspike_gd_release.so             the GDExtension (GSL and libstdc++ linked statically; needs only glibc)
#   spike-run                          the headless runner behind CHECK, found next to the extension
#   circuits/, catalog/                loose copies for spike-run (reference circuits) and the histogram viewer
# Needs the Godot 4.7.2 editor binary on PATH (`godot`) with its export templates installed (the Dockerfile does both).
# The smoke test extracts the zip into an empty temp dir, scrubs the environment (PATH has no spike-run, no SPIKE_* variables), then
#   1. runs the exported binary with SP_SMOKE=1 (every screen, layout, task, the extension, headless),
#   2. runs the exported binary again with SP_CHECK_EXPORT=1 (tests/check_export.gd): CHECK scores a reference circuit through the bundled spike-run,
#   3. runs the bundled spike-run on its own against the bundled circuits, and asserts no libgsl dependency.
# Windows x86_64 is not produced here yet: see README "Windows".
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd); cd "$root"
name=spike-linux-x86_64
out=build/export/linux
zip_path=$root/build/$name.zip

[ "${SKIP_BUILD:-0}" = 1 ] || scripts/build-gd.sh release
for f in godot/bin/libspike_gd_release.so godot/bin/spike-run; do [ -f "$f" ] || { echo "missing $f (scripts/build-gd.sh release)" >&2; exit 1; }; done
# godot/bin/spike-run is whichever mode built last: make sure it is the release one
cp -f build/gd-release/engine/spike-run godot/bin/spike-run 2>/dev/null || true

# no dynamic GSL: the whole point of the static link
if ldd godot/bin/libspike_gd_release.so godot/bin/spike-run | grep -i gsl; then echo "FAIL: GSL is linked dynamically" >&2; exit 1; fi

rm -rf "$out"; mkdir -p "$out"
# the build id the title shows (res://build_id.txt, packed by the export; gitignored)
{ git rev-parse --short HEAD 2>/dev/null || echo dev; } | tr -d '\n' > godot/build_id.txt
if ! git diff --quiet HEAD -- 2>/dev/null; then printf -- '-dirty' >> godot/build_id.txt; fi
godot --headless --path godot --import >/dev/null 2>&1 || true
godot --headless --path godot --export-release "Linux x86_64" "$root/$out/Spike.x86_64" 2>&1 | tee build/export.log | grep -E "ERROR|error" || true
[ -x "$out/Spike.x86_64" ] && [ -f "$out/Spike.pck" ] || { echo "FAIL: export produced no binary/pck (build/export.log)" >&2; exit 1; }

# the exporter copies the extension and its [dependencies] (spike-run) next to the binary; replace them with stripped copies of the release builds
cp -f godot/bin/libspike_gd_release.so godot/bin/spike-run "$out/"
strip --strip-unneeded "$out/libspike_gd_release.so" "$out/spike-run"
cp -rL godot/circuits godot/catalog "$out/"
cp -f README.md "$out/README.md"

rm -f "$zip_path"; (cd "$out" && zip -qr "$zip_path" .)
echo "zip: $(du -h "$zip_path" | cut -f1)  $zip_path"

# ---------------- smoke test of the zip ----------------
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
mkdir "$tmp/game" "$tmp/home"; unzip -q "$zip_path" -d "$tmp/game"
# the executable bits must come out of the zip as they are (no chmod here: a zip that loses them is a broken zip)
for f in Spike.x86_64 spike-run; do
  unzip -Z "$zip_path" "$f" | grep -q '^-rwx' || { echo "FAIL: $f is not executable inside the zip" >&2; exit 1; }
  [ -x "$tmp/game/$f" ] || { echo "FAIL: $f is not executable after unzip" >&2; exit 1; }
done
clean() { env -i PATH=/usr/bin:/bin HOME="$tmp/home" LANG=C.UTF-8 "$@"; }
log=$tmp/smoke.log; fail=0
cd "$tmp/game"   # not the repo: nothing outside the zip is visible via ../
clean SP_SMOKE=1 SP_NOAUTOSAVE=1 timeout 900 ./Spike.x86_64 --headless 2>&1 | tee "$log" || true
for pat in "SMOKE OK" "^EXTENSION OK" "^V02 OK" "^TASKS OK" "^FONTS below 14 px: 0"; do
  grep -q "$pat" "$log" || { echo "FAIL: exported smoke: no '$pat'"; fail=1; }
done
grep -E "SCRIPT ERROR|Parse Error|SMOKE FAIL" "$log" && { echo "FAIL: errors in the exported smoke log"; fail=1; }
clean SP_CHECK_EXPORT=1 SP_NOAUTOSAVE=1 timeout 300 ./Spike.x86_64 --headless 2>&1 | tee "$tmp/check.log" || true
grep -q "^CHECK EXPORT OK" "$tmp/check.log" || { echo "FAIL: CHECK from the exported build"; fail=1; }
tasks=$(clean ./spike-run --list-tasks | wc -l)
[ "$tasks" -ge 30 ] || { echo "FAIL: bundled spike-run lists only $tasks tasks"; fail=1; }
clean ./spike-run --task reflex --seed 1 --require any >/dev/null || { echo "FAIL: bundled spike-run does not pass the reflex reference"; fail=1; }
if ldd libspike_gd_release.so spike-run | grep -i "not found"; then echo "FAIL: unresolved libraries"; fail=1; fi
[ "$fail" = 0 ] || exit 1
echo "package.sh: OK  $zip_path ($(du -h "$zip_path" | cut -f1)); unpacked $(du -sh "$tmp/game" | cut -f1)"
