#!/usr/bin/env bash
# Headless checks of the circuit editor (godot/tests/test_editor.gd): undo / redo byte for byte, large-circuit layout (labels, cards, gaps), clipboard and import,
# the reference fingerprint, accessors and the keyboard cursor. Needs only the godot binary (no GDExtension). Exit 77 (ctest SKIP) without it.
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd); cd "$root"
GODOT=${GODOT:-godot}
command -v "$GODOT" > /dev/null 2>&1 || { echo "SKIP editor: godot not found"; exit 77; }
out=$(SP_NOAUTOSAVE=1 timeout 300 "$GODOT" --headless --path godot -s tests/test_editor.gd 2>&1)
echo "$out" | grep -E "^EDITOR|SCRIPT ERROR|Parse Error" | grep -v "SpikeSim\|SpikeTask"
echo "$out" | grep -q "^EDITOR OK" || exit 1
