#!/usr/bin/env bash
# Walk the game's UI interaction graph headless (godot/tests/interaction_graph.json, godot/tests/ui_graph_test.gd; docs/INTERACTION-GRAPH.md).
#   scripts/ui_graph_test.sh
# Needs the godot binary and the built GDExtension (scripts/build-gd.sh [release]) with spike-run beside it. Exit 77 (ctest SKIP) when they are missing.
# The game is exported (release if the release extension is built, else debug) into a temporary directory and the exported binary runs the walk: the
# editor binary of some distributions (Arch) cannot load the extension (two C++ runtimes in one process), and the export is what ships anyway.
# Without export templates it falls back to `godot --path godot`.
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd); cd "$root"
GODOT=${GODOT:-godot}
command -v "$GODOT" > /dev/null 2>&1 || { echo "SKIP ui_graph: godot ($GODOT) not found"; exit 77; }
dbg=godot/bin/libspike_gd_debug.so; rel=godot/bin/libspike_gd_release.so
[ -f "$dbg" ] || [ -f "$rel" ] || { echo "SKIP ui_graph: the GDExtension is not built (scripts/build-gd.sh)"; exit 77; }
[ -x godot/bin/spike-run ] || { echo "SKIP ui_graph: godot/bin/spike-run is missing (scripts/build-gd.sh)"; exit 77; }
tmp=$(mktemp -d); log="$tmp/log"; trap 'rm -rf "$tmp"' EXIT
SP_NOAUTOSAVE=1 "$GODOT" --headless --path godot --import > "$log" 2>&1
if grep -qE "SCRIPT ERROR|Parse Error" "$log" && ! grep -q "Identifier \"Spike\(Sim\|Task\)\" not declared" "$log"; then cat "$log"; echo "FAIL ui_graph: the project does not load"; exit 1; fi
if [ -f "$rel" ]; then kind=release; else kind=debug; fi
run=()
if "$GODOT" --headless --path godot --export-$kind "Linux x86_64" "$tmp/Spike.x86_64" > "$tmp/export.log" 2>&1 && [ -x "$tmp/Spike.x86_64" ]; then
  chmod +x "$tmp"/spike-run 2> /dev/null; cp -r godot/circuits godot/catalog "$tmp"/   # spike-run (CHECK) reads them loose, as in the shipped zip
  run=("$tmp/Spike.x86_64" --headless)
else
  echo "ui_graph: no export templates ($kind); running through $GODOT --path godot"
  run=("$GODOT" --headless --path godot)
fi
( cd "$tmp" && [ "${run[0]}" != "$GODOT" ] || cd "$root"; SP_UIGRAPH=1 SP_NOAUTOSAVE=1 SP_NODISPLAY=1 timeout 1700 "${run[@]}" > "$log" 2>&1 )
status=$?
grep -E "^UIGRAPH|SCRIPT ERROR|Parse Error|^ERROR:" "$log"
if ! grep -q "^UIGRAPH OK" "$log"; then
  tail -30 "$log"; echo "FAIL ui_graph: no UIGRAPH OK line (exit $status)"; exit 1
fi
if grep -qE "SCRIPT ERROR|Parse Error" "$log"; then echo "FAIL ui_graph: script errors while walking the UI"; exit 1; fi
[ "$status" = 0 ] || { echo "FAIL ui_graph: exit status $status"; exit 1; }
echo "ui_graph: all good"
