#!/usr/bin/env bash
# Package the game for macOS (Apple Silicon) and smoke-test the result. Runs on a Mac (CI: .github/workflows/build-macos.yml).
#   scripts/package-macos.sh              (needs godot/bin/{libspike_gd_release.dylib,spike-run}: scripts/build-macos.sh; SKIP_BUILD is implied)
# Output: build/spike-macos-arm64.zip (+ .sha256) holding Spike.app:
#   Contents/MacOS/Spike                the exported game;  Contents/Resources/Spike.pck the data
#   Contents/Frameworks/libspike_gd_release.dylib and spike-run   the extension and the headless runner behind CHECK (found next to the extension)
#   Contents/Resources/{circuits,catalog,README.md}               loose copies for spike-run (it looks in ../Resources/circuits from Frameworks)
# The app is ad-hoc signed (no Apple developer account), not notarized. Needs the Godot 4.7.2 macOS editor as `godot` on PATH (or $GODOT) with the macOS export
# template installed; GODOT_SETUP=1 downloads both from godot-builds (macos.zip and version.txt only out of the ~1 GB .tpz).
# The smoke test extracts the zip into an empty temp dir, scrubs the environment, and asserts what scripts/package.sh asserts for Linux.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd); cd "$root"
arch=${SPIKE_ARCH:-arm64}
name=spike-macos-$arch
out=build/export/macos
zip_path=$root/build/$name.zip
GODOT_VERSION=${GODOT_VERSION:-$(sed -n 's/^ARG GODOT_VERSION=//p' Dockerfile | head -1)}
base=https://github.com/godotengine/godot-builds/releases/download/${GODOT_VERSION}-stable

if [ "${GODOT_SETUP:-0}" = 1 ]; then
  d=$(mktemp -d)
  curl -fsSL -o "$d/godot.zip" "$base/Godot_v${GODOT_VERSION}-stable_macos.universal.zip"
  mkdir -p "$HOME/godot-editor"; unzip -q -o "$d/godot.zip" -d "$HOME/godot-editor"
  curl -fsSL -o "$d/tpl.tpz" "$base/Godot_v${GODOT_VERSION}-stable_export_templates.tpz"
  t="$HOME/Library/Application Support/Godot/export_templates/${GODOT_VERSION}.stable"; mkdir -p "$t"
  unzip -q -j "$d/tpl.tpz" 'templates/macos.zip' 'templates/version.txt' -d "$t"
  rm -rf "$d"
  export GODOT="$HOME/godot-editor/Godot.app/Contents/MacOS/Godot"
fi
GODOT=${GODOT:-godot}
"$GODOT" --version

for f in godot/bin/libspike_gd_release.dylib godot/bin/spike-run; do [ -f "$f" ] || { echo "missing $f (scripts/build-macos.sh)" >&2; exit 1; }; done
lipo -archs godot/bin/spike-run godot/bin/libspike_gd_release.dylib

# only system libraries (GSL is static; no Homebrew path): shared by the build tree check and the smoke test
check_libs() {  # check_libs file...
  local bad=0 f
  for f in "$@"; do
    # first line is the file name; a dylib's own id is the second; allow system libraries, the extension itself, @rpath/@loader_path siblings
    if otool -L "$f" | tail -n +2 | awk '{print $1}' | grep -vE '^(/usr/lib/|/System/Library/|@rpath/libspike_gd|@loader_path/libspike_gd|.*/libspike_gd_release\.dylib$)'; then
      echo "FAIL: $f links a non-system library (above)" >&2; bad=1
    fi
  done
  return $bad
}
check_libs godot/bin/libspike_gd_release.dylib godot/bin/spike-run

rm -rf "$out"; mkdir -p "$out"
{ git rev-parse --short HEAD 2>/dev/null || echo dev; } | tr -d '\n' > godot/build_id.txt
if ! git diff --quiet HEAD -- 2>/dev/null; then printf -- '-dirty' >> godot/build_id.txt; fi
"$GODOT" --headless --path godot --import >/dev/null 2>&1 || true
"$GODOT" --headless --path godot --export-release "macOS" "$root/$out/Spike.zip" 2>&1 | tee build/export-macos.log | grep -E "ERROR|error" || true
[ -f "$out/Spike.zip" ] || { echo "FAIL: export produced no zip (build/export-macos.log)" >&2; exit 1; }
ditto -x -k "$out/Spike.zip" "$out"; rm -f "$out/Spike.zip"
app=$out/Spike.app
[ -d "$app/Contents/MacOS" ] || { echo "FAIL: no Spike.app in the export" >&2; ls -la "$out" >&2; exit 1; }
echo "exported bundle:"; (cd "$app" && find . -type f | sort | head -60)
exe=$(ls "$app/Contents/MacOS" | head -1)

# the extension and the runner: stripped copies of the release builds, in Frameworks (where Godot loads GDExtensions from in an exported app)
fw=$app/Contents/Frameworks; res=$app/Contents/Resources
mkdir -p "$fw" "$res"
find "$app" \( -name 'libspike_gd*' -o -name spike-run \) -type f -delete
cp -f godot/bin/libspike_gd_release.dylib godot/bin/spike-run "$fw/"
strip -x "$fw/libspike_gd_release.dylib" "$fw/spike-run"
cp -rL godot/circuits godot/catalog "$res/"
cp -f README.md "$res/README.md"
chmod 755 "$app/Contents/MacOS/$exe" "$fw/spike-run" "$fw/libspike_gd_release.dylib"

# ad-hoc signature, inside out (a signature must be applied after the last change to the bundle)
codesign --force -s - "$fw/libspike_gd_release.dylib"
codesign --force -s - "$fw/spike-run"
codesign --force --deep -s - "$app"
codesign --verify --deep --strict --verbose=2 "$app"

rm -f "$zip_path"; ditto -c -k --keepParent "$app" "$zip_path"
(cd build && shasum -a 256 "$name.zip" > "$name.zip.sha256")
echo "zip: $(du -h "$zip_path" | cut -f1)  $zip_path"

# ---------------- smoke test of the zip ----------------
tmp=$(mktemp -d); tmp=$(cd -P "$tmp" && pwd -P); trap 'rm -rf "$tmp"' EXIT
mkdir "$tmp/game" "$tmp/home"; ditto -x -k "$zip_path" "$tmp/game"
g=$tmp/game/Spike.app
codesign --verify --deep --strict "$g" || { echo "FAIL: signature does not verify after unzip"; exit 1; }
for f in "Contents/MacOS/$exe" Contents/Frameworks/spike-run; do [ -x "$g/$f" ] || { echo "FAIL: $f is not executable after unzip" >&2; exit 1; }; done
# no coreutils `timeout` on macOS: perl's alarm (T=seconds, default 900)
clean() { perl -e 'alarm shift; exec @ARGV' "${T:-900}" env -i PATH=/usr/bin:/bin HOME="$tmp/home" LANG=en_US.UTF-8 "$@"; }
log=$tmp/smoke.log; fail=0
cd "$tmp/game"   # not the repo: nothing outside the zip is visible via ../
clean SP_SMOKE=1 SP_NOAUTOSAVE=1 "$g/Contents/MacOS/$exe" --headless 2>&1 | tee "$log" || true
for pat in "SMOKE OK" "^EXTENSION OK" "^V02 OK" "^TASKS OK" "^FONTS below 14 px: 0"; do
  grep -q "$pat" "$log" || { echo "FAIL: exported smoke: no '$pat'"; fail=1; }
done
grep -E "SCRIPT ERROR|Parse Error|SMOKE FAIL" "$log" && { echo "FAIL: errors in the exported smoke log"; fail=1; }
T=300 clean SP_CHECK_EXPORT=1 SP_NOAUTOSAVE=1 "$g/Contents/MacOS/$exe" --headless 2>&1 | tee "$tmp/check.log" || true
grep -q "^CHECK EXPORT OK" "$tmp/check.log" || { echo "FAIL: CHECK from the exported build"; fail=1; }
run=$g/Contents/Frameworks/spike-run
tasks=$(clean "$run" --list-tasks | wc -l | tr -d ' ')
[ "$tasks" -ge 30 ] || { echo "FAIL: bundled spike-run lists only $tasks tasks"; fail=1; }
clean "$run" --task reflex --seed 1 --require any | tee "$tmp/reflex.log" || { echo "FAIL: bundled spike-run does not pass the reflex reference"; fail=1; }
check_libs "$g/Contents/Frameworks/libspike_gd_release.dylib" "$run" || fail=1
[ "$fail" = 0 ] || exit 1
echo "package-macos.sh: OK  $zip_path ($(du -h "$zip_path" | cut -f1)); unpacked $(du -sh "$tmp/game" | cut -f1)"
