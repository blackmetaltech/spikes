#!/usr/bin/env bash
# Package the game for Windows x86_64 and smoke-test the result. The Windows counterpart of scripts/package.sh; run it in the MSYS2 UCRT64 shell
# after scripts/build-windows.sh (godot/bin/spike_gd_release.dll and godot/bin/spike-run.exe must exist).
#   scripts/package-windows.sh
# Output: build/spike-windows-x86_64.zip (+ .sha256), containing
#   Spike.exe, Spike.console.exe, Spike.pck   the exported game (the console wrapper prints to a terminal; res://circuits and res://catalog are in the pck)
#   spike_gd_release.dll                      the GDExtension (C++ runtime, winpthread and GSL linked statically: only Windows system DLLs)
#   spike-run.exe                             the headless runner behind CHECK, found next to the extension
#   circuits/, catalog/, README.md            loose copies for spike-run (reference circuits) and the histogram viewer
# Godot 4.7.2 (the version in the Dockerfile): the Windows editor and, out of the ~1 GB .tpz, only the two release templates. Env GODOT_CACHE (default
# build/godot-win) keeps the download between runs. The smoke test is the one of package.sh: the zip is extracted into an empty temp dir and run
# with a scrubbed environment (PATH is only the Windows system directories, so a DLL that is not in the zip shows up as a failure to start).
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd); cd "$root"
[ "${MSYSTEM:-}" = UCRT64 ] || { echo "run this in the MSYS2 UCRT64 shell" >&2; exit 2; }
GODOT_VERSION=$(sed -n 's/^ARG GODOT_VERSION=\(.*\)$/\1/p' Dockerfile | head -n1)
[ -n "$GODOT_VERSION" ] || { echo "no GODOT_VERSION in the Dockerfile" >&2; exit 1; }
name=spike-windows-x86_64
out=build/export/windows
zip_path=$root/build/$name.zip
for f in godot/bin/spike_gd_release.dll godot/bin/spike-run.exe; do [ -f "$f" ] || { echo "missing $f (scripts/build-windows.sh)" >&2; exit 1; }; done

# imports must be Windows system DLLs only
check_imports() {
  local bad=0 f
  for f in "$@"; do
    if objdump -p "$f" | grep "DLL Name" | grep -iE 'libgcc|libstdc\+\+|libwinpthread|libgsl|libgomp|msys-|libgmp'; then echo "FAIL: $f imports a MinGW/MSYS2 runtime DLL" >&2; bad=1; fi
  done
  return $bad
}
check_imports godot/bin/spike_gd_release.dll godot/bin/spike-run.exe

# ---------------- Godot editor + export templates ----------------
cache=${GODOT_CACHE:-$root/build/godot-win}; mkdir -p "$cache"
base=https://github.com/godotengine/godot-builds/releases/download/${GODOT_VERSION}-stable
godot=$cache/Godot_v${GODOT_VERSION}-stable_win64_console.exe
if [ ! -f "$godot" ]; then
  curl -fsSL --retry 3 -o "$cache/godot.zip" "$base/Godot_v${GODOT_VERSION}-stable_win64.exe.zip"
  unzip -q -o "$cache/godot.zip" -d "$cache"; rm -f "$cache/godot.zip"
fi
tpl=$(cygpath -u "${APPDATA:?APPDATA is not set}")/Godot/export_templates/${GODOT_VERSION}.stable
if [ ! -f "$tpl/windows_release_x86_64.exe" ] || [ ! -f "$tpl/windows_release_x86_64_console.exe" ] || [ ! -f "$tpl/version.txt" ]; then
  mkdir -p "$tpl"
  curl -fsSL --retry 3 -o "$cache/tpl.tpz" "$base/Godot_v${GODOT_VERSION}-stable_export_templates.tpz"
  unzip -q -j -o "$cache/tpl.tpz" templates/windows_release_x86_64.exe templates/windows_release_x86_64_console.exe templates/version.txt -d "$tpl"
  rm -f "$cache/tpl.tpz"
fi
"$godot" --version

# ---------------- export ----------------
rm -rf "$out"; mkdir -p "$out" build
# the build id the title shows (res://build_id.txt, packed by the export; gitignored)
{ git rev-parse --short HEAD 2>/dev/null || echo dev; } | tr -d '\n' > godot/build_id.txt
if ! git diff --quiet HEAD -- 2>/dev/null; then printf -- '-dirty' >> godot/build_id.txt; fi
# The Godot editor is a debug build and looks for the .debug library of spike.gdextension; without an extension its scripts (SpikeSim, SpikeTask) do not
# parse and the export fails. Only the editor uses this copy of the release DLL (the exported game takes the release entry and the release DLL, below).
cp -f godot/bin/spike_gd_release.dll godot/bin/spike_gd_debug.dll
"$godot" --headless --path godot --import >/dev/null 2>&1 || true
"$godot" --headless --path godot --export-release "Windows x86_64" "$(cygpath -m "$root/$out/Spike.exe")" 2>&1 | tee build/export.log | grep -E "ERROR|error" || true
[ -f "$out/Spike.exe" ] && [ -f "$out/Spike.pck" ] || { echo "FAIL: export produced no exe/pck (build/export.log)" >&2; exit 1; }
[ -f "$out/Spike.console.exe" ] || { echo "FAIL: export produced no Spike.console.exe (build/export.log)" >&2; exit 1; }

# the exporter copies the extension and its [dependencies] (spike-run.exe) next to the exe; replace them with the stripped release builds
rm -f "$out/spike_gd_debug.dll"
cp -f godot/bin/spike_gd_release.dll godot/bin/spike-run.exe "$out/"
cp -rL godot/circuits godot/catalog "$out/"
cp -f README.md "$out/README.md"
rm -f "$zip_path" "$zip_path.sha256"; (cd "$out" && zip -qr -X "$zip_path" .)
(cd build && sha256sum "$name.zip" > "$name.zip.sha256")
echo "zip: $(du -h "$zip_path" | cut -f1)  $zip_path"

# ---------------- smoke test of the zip ----------------
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
mkdir "$tmp/game" "$tmp/home" "$tmp/tmp"; unzip -q "$zip_path" -d "$tmp/game"
winsys=$(cygpath -w "${SYSTEMROOT:-/c/Windows}")
# a scrubbed environment: the Windows system directories on PATH and the few variables Windows programs need, nothing of MSYS2 / MinGW / this repo
clean() {
  env -i SYSTEMROOT="$winsys" WINDIR="$winsys" PATH="$winsys\\System32;$winsys" TEMP="$(cygpath -w "$tmp/tmp")" TMP="$(cygpath -w "$tmp/tmp")" \
    USERPROFILE="$(cygpath -w "$tmp/home")" APPDATA="$(cygpath -w "$tmp/home")" LOCALAPPDATA="$(cygpath -w "$tmp/home")" "$@"
}
log=$tmp/smoke.log; fail=0
cd "$tmp/game"   # not the repo: nothing outside the zip is visible via ../
clean SP_SMOKE=1 SP_NOAUTOSAVE=1 timeout 900 ./Spike.console.exe --headless 2>&1 | tee "$log" || true
for pat in "SMOKE OK" "^EXTENSION OK" "^V02 OK" "^TASKS OK" "^FONTS below 14 px: 0"; do
  grep -q "$pat" "$log" || { echo "FAIL: exported smoke: no '$pat'"; fail=1; }
done
grep -E "SCRIPT ERROR|Parse Error|SMOKE FAIL" "$log" && { echo "FAIL: errors in the exported smoke log"; fail=1; }
clean SP_CHECK_EXPORT=1 SP_NOAUTOSAVE=1 timeout 300 ./Spike.console.exe --headless 2>&1 | tee "$tmp/check.log" || true
grep -q "^CHECK EXPORT OK" "$tmp/check.log" || { echo "FAIL: CHECK from the exported build"; fail=1; }
tasks=$(clean ./spike-run.exe --list-tasks | wc -l)
[ "$tasks" -ge 30 ] || { echo "FAIL: bundled spike-run lists only $tasks tasks"; fail=1; }
clean ./spike-run.exe --task reflex --seed 1 --require any >/dev/null || { echo "FAIL: bundled spike-run does not pass the reflex reference"; fail=1; }
check_imports spike_gd_release.dll spike-run.exe || fail=1
# Windows only: a circuit under a path with accents and spaces (a user name like that is common), and the seed workers (no fork) agree with the in-process run
accents="$tmp/tmp/$(printf 'Jos\xc3\xa9 \xc3\x85ngstr\xc3\xb6m')"; mkdir "$accents"; cp circuits/reflex_arc.json "$accents/c.json"
clean ./spike-run.exe --task reflex --circuit "$(cygpath -w "$accents/c.json")" --seed 1 --require any >/dev/null || { echo "FAIL: spike-run cannot read a circuit under a path with non-ASCII characters"; fail=1; }
clean ./spike-run.exe --task reflex --seeds 1,2,3 --jobs 3 > "$tmp/par.json" || { echo "FAIL: parallel seeds"; fail=1; }
clean ./spike-run.exe --task reflex --seeds 1,2,3 --jobs 1 > "$tmp/seq.json" || { echo "FAIL: sequential seeds"; fail=1; }
cmp -s "$tmp/par.json" "$tmp/seq.json" || { echo "FAIL: worker-process seeds differ from the in-process run"; fail=1; }
[ "$fail" = 0 ] || exit 1
echo "package-windows.sh: OK  $zip_path ($(du -h "$zip_path" | cut -f1)); unpacked $(du -sh "$tmp/game" | cut -f1)"
