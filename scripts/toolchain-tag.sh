#!/usr/bin/env bash
# Prints the tag of the toolchain image for this checkout: a hash of the Dockerfile and every script that carries a pin or is baked into the image.
# Used by .github/workflows/{toolchain,ci}.yml and scripts/ci-local.sh, so they all agree. Bump a pin -> the tag changes -> run the toolchain workflow.
set -euo pipefail
cd "$(dirname "$0")/.."
sha256sum Dockerfile scripts/prebuilt.sh scripts/build-nest.sh scripts/build-nestml.sh scripts/build-gd.sh | sha256sum | cut -c1-16
