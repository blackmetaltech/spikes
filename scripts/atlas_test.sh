#!/usr/bin/env bash
# The Atlas worm chapter's game side (godot/tests/test_atlas.gd): the wing and its gating, the glossary, Analysis open in the chapter, the silencing control's rules,
# the bet panel against an ordinary case, and a right and a wrong bet run through the engine. Needs godot and the built GDExtension. Exit 77 (ctest SKIP) without them.
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd); cd "$root"
GODOT=${GODOT:-godot}
command -v "$GODOT" > /dev/null 2>&1 || { echo "SKIP atlas: godot not found"; exit 77; }
[ -f godot/bin/libspike_gd_debug.so ] || [ -f godot/bin/libspike_gd_release.so ] || { echo "SKIP atlas: the GDExtension is not built (scripts/build-gd.sh)"; exit 77; }
SP_NOAUTOSAVE=1 "$GODOT" --headless --path godot --import > /dev/null 2>&1
out=$(SP_NOAUTOSAVE=1 timeout 900 "$GODOT" --headless --path godot -s tests/test_atlas.gd 2>&1)
echo "$out" | grep -E "^ATLAS|SCRIPT ERROR|Parse Error|^ERROR:"
echo "$out" | grep -q "^ATLAS OK" || { echo "$out" | tail -30; exit 1; }
