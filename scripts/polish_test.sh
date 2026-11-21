#!/usr/bin/env bash
# Pure-logic checks of the lab chrome (godot/tests/test_polish.gd): time-axis labels never repeat, score parts never show a raw key,
# latch self-loops and scheduled stimuli are never "a rhythm by accident". Needs only the godot binary (no GDExtension). Exit 77 (ctest SKIP) without it.
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd); cd "$root"
GODOT=${GODOT:-godot}
command -v "$GODOT" > /dev/null 2>&1 || { echo "SKIP polish: godot not found"; exit 77; }
out=$(SP_NOAUTOSAVE=1 timeout 120 "$GODOT" --headless --path godot -s tests/test_polish.gd 2>&1)
echo "$out" | grep -E "^POLISH|SCRIPT ERROR|Parse Error"
echo "$out" | grep -q "^POLISH OK" || exit 1
