# Sourced by the build scripts. A toolchain image (Dockerfile) ships the pinned, already-built third_party trees under $SPIKE_PREBUILT/third_party
# (NEST source + static build + PyNEST install, NESTML venv, godot-cpp). When one is present and is for the same pin, the script symlinks it into
# $ROOT/third_party instead of building; otherwise (a dev machine, a pin bumped since the image was built) it builds locally as before.
#   use_prebuilt <name>...   link third_party/<name> -> $SPIKE_PREBUILT/third_party/<name> unless it already exists; false if any is missing
use_prebuilt() {
  local p="${SPIKE_PREBUILT:-}" n
  [ -n "$p" ] && [ "$(cd "$p" 2>/dev/null && pwd)" != "$ROOT" ] || return 1
  for n in "$@"; do [ -e "$p/third_party/$n" ] || return 1; done
  mkdir -p "$ROOT/third_party"
  for n in "$@"; do
    [ -e "$ROOT/third_party/$n" ] || { rm -f "$ROOT/third_party/$n"; ln -s "$p/third_party/$n" "$ROOT/third_party/$n"; }
  done
}
