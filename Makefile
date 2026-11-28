# Play and build Spike.   make help
#
# `make play` needs no local build: it downloads the Linux build that CI made from the newest green commit on master
# (GitHub CLI `gh`, logged in) into build/play/ and starts it. Building from source is `make build` and takes a long time.

REPO     ?= blackmetaltech/spikes
BRANCH   ?= master
PLAY_DIR := build/play
GAME     := $(PLAY_DIR)/Spike.x86_64
STAMP    := $(PLAY_DIR)/.run-id

.PHONY: help play fetch update play-all play-fresh which play-dev build test clean-play

help:
	@echo "make play        start the game (downloads the newest CI build of $(BRANCH) the first time)"
	@echo "make update      download the newest CI build again, then 'make play'"
	@echo "make play-all    start with every level and part unlocked (your saves are not changed by the unlock)"
	@echo "make play-fresh  start with an empty, throw-away save (your real save is untouched)"
	@echo "make which       show which commit the downloaded build was made from"
	@echo "make play-dev    run from source with the Godot editor binary (needs 'make build' first)"
	@echo "make build       build everything from source on this machine (slow: NEST, models, extension)"
	@echo "make test        run the CI steps locally in Docker (slow)"
	@echo "make clean-play  remove the downloaded build"
	@echo "BRANCH=<name> picks another branch's CI build, for example: make update BRANCH=w-a3"

$(GAME):
	@$(MAKE) --no-print-directory fetch

fetch:
	@command -v gh >/dev/null || { echo "The GitHub CLI 'gh' is needed (https://cli.github.com), logged in with 'gh auth login'."; exit 1; }
	@id=$$(gh run list -R $(REPO) --workflow ci --branch $(BRANCH) --status success --limit 1 --json databaseId -q '.[0].databaseId'); \
	[ -n "$$id" ] || { echo "No green CI run found on $(BRANCH)."; exit 1; }; \
	echo "Downloading the build of CI run $$id ($(BRANCH))..."; \
	rm -rf $(PLAY_DIR) && mkdir -p $(PLAY_DIR)/.dl && \
	gh run download $$id -R $(REPO) -n spike-linux-x86_64 -D $(PLAY_DIR)/.dl && \
	unzip -q -o $(PLAY_DIR)/.dl/*.zip -d $(PLAY_DIR) && rm -rf $(PLAY_DIR)/.dl && \
	if [ ! -f $(GAME) ]; then d=$$(dirname "$$(find $(PLAY_DIR) -name Spike.x86_64 | head -1)"); [ -n "$$d" ] && mv "$$d"/* $(PLAY_DIR)/; fi; \
	chmod +x $(GAME) $(PLAY_DIR)/spike-run 2>/dev/null; \
	gh run view $$id -R $(REPO) --json headSha,createdAt -q '"run '$$id'  commit " + .headSha[0:7] + "  " + .createdAt' > $(STAMP); \
	echo "Ready: $$(cat $(STAMP))"

update: fetch

which:
	@cat $(STAMP) 2>/dev/null || echo "Nothing downloaded yet: make play"

play: $(GAME)
	@cat $(STAMP) 2>/dev/null; cd $(PLAY_DIR) && ./Spike.x86_64

play-all: $(GAME)
	@cd $(PLAY_DIR) && SP_UNLOCK_ALL=1 ./Spike.x86_64

play-fresh: $(GAME)
	@d=$$(mktemp -d); echo "Throw-away save in $$d"; cd $(PLAY_DIR) && XDG_DATA_HOME=$$d/data XDG_CONFIG_HOME=$$d/config ./Spike.x86_64

play-dev:
	@[ -f godot/bin/libspike_gd_debug.so ] || [ -f godot/bin/libspike_gd_release.so ] || { echo "No local build: run 'make build' or use 'make play'."; exit 1; }
	cd godot && godot --path .

build:
	scripts/build-all.sh --debug

test:
	scripts/ci-local.sh

clean-play:
	rm -rf $(PLAY_DIR)
