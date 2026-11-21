# Reproducible build of Spike on a clean Ubuntu: pinned NEST (static), pinned NESTML in a venv, pinned godot-cpp, Godot 4.7.2 headless + the
# Linux export template. The image holds the toolchain; the source is mounted or copied in and built by scripts/build-all.sh.
#
#   docker build -t spike-build .
#   docker run --rm -v "$PWD":/src spike-build scripts/build-all.sh --tests      # everything, incl. engine tests
#   docker run --rm -v "$PWD":/src spike-build scripts/package.sh                # -> build/spike-linux-x86_64.zip (+ headless smoke of the zip)
#
# The NEST/NESTML/godot-cpp trees are built into the image (see below); CI publishes it as ghcr.io/ixaxaar/spike-build:<hash> (docs/BUILD.md).
# Pins (also the single place CI reads them from): NEST commit in scripts/build-nest.sh, NESTML in scripts/build-nestml.sh, godot-cpp in
# scripts/build-gd.sh; Godot below.
FROM ubuntu:24.04

ARG GODOT_VERSION=4.7.2
ARG DEBIAN_FRONTEND=noninteractive
ENV LANG=C.UTF-8 TZ=UTC

RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential cmake ninja-build git ca-certificates curl unzip zip pkg-config \
      python3 python3-dev python3-venv python3-pip python3-numpy python3-pytest python3-networkx cython3 \
      libgsl-dev libltdl-dev libreadline-dev libtool bison flex \
      libfontconfig1 libx11-6 libxcursor1 libxi6 libxinerama1 libxrandr2 libgl1 libasound2t64 \
    && rm -rf /var/lib/apt/lists/*

# Godot (headless editor binary) and only the linux_release.x86_64 template out of the ~1 GB .tpz
RUN set -eux; base=https://github.com/godotengine/godot-builds/releases/download/${GODOT_VERSION}-stable; \
    curl -fsSL -o /tmp/godot.zip "$base/Godot_v${GODOT_VERSION}-stable_linux.x86_64.zip"; \
    unzip -q /tmp/godot.zip -d /opt/godot; ln -s /opt/godot/Godot_v${GODOT_VERSION}-stable_linux.x86_64 /usr/local/bin/godot; \
    curl -fsSL -o /tmp/tpl.tpz "$base/Godot_v${GODOT_VERSION}-stable_export_templates.tpz"; \
    d=/root/.local/share/godot/export_templates/${GODOT_VERSION}.stable; mkdir -p "$d"; \
    unzip -q -j /tmp/tpl.tpz 'templates/linux_release.x86_64' 'templates/version.txt' -d "$d"; \
    rm -f /tmp/godot.zip /tmp/tpl.tpz; godot --version

# The checkout is mounted from the host and owned by another uid; git refuses it as "dubious ownership" without this.
RUN git config --system --add safe.directory '*'

# The pinned third_party trees, built once here instead of on every CI run: NEST (static libs for the engine + PyNEST install), the NESTML venv,
# godot-cpp. Only the pin scripts are copied in, so this layer (the expensive one) is rebuilt only when a pin changes; CI tags the image with a hash of
# the Dockerfile and these scripts. SPIKE_OPENMP is part of the NEST build and must match what the engine and PyNEST use at run time.
ENV SPIKE_OPENMP=OFF
COPY scripts/prebuilt.sh scripts/build-nest.sh scripts/build-nestml.sh scripts/build-gd.sh /opt/spike-prebuilt/scripts/
RUN set -eux; cd /opt/spike-prebuilt; export SPIKE_STAMP=1; \
    scripts/build-nest.sh; scripts/build-nest.sh --pynest; rm -rf third_party/nest-build-py; \
    scripts/build-nestml.sh --venv-only; scripts/build-gd.sh fetch
# build-*.sh link these into /src/third_party when the pin matches, and build locally otherwise (so the same scripts work without the image).
ENV SPIKE_PREBUILT=/opt/spike-prebuilt
WORKDIR /src
CMD ["scripts/build-all.sh", "--tests"]
