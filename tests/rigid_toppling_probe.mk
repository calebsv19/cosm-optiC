# Supplemental makefile for Ball Bounce's existing Main Edit build. Read after
# its Makefile; links a test observer without changing the native engine sources.
RIGID_TOPPLING_PROBE_SOURCE := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/rigid_toppling_momentum_probe.c)
.PHONY: rigid-toppling-momentum-probe
rigid-toppling-momentum-probe: $(CONTRACT_BUILD_DIR)/toppling_momentum_probe
$(CONTRACT_BUILD_DIR)/toppling_momentum_probe: $(RIGID_TOPPLING_PROBE_SOURCE) $(RIGID_NATIVE_SRC) $(CONTRACT_BUILD_DIR)/rigid-scene-id.txt | $(CONTRACT_BUILD_DIR)
	$(CLANG) $(SCENE_FLAGS) -DBALL_SCENE_ENGINE_SHA256=\"$(SCENE_CLANG_ID)\" -DBALL_RIGID_SCENE_ENGINE_SHA256=\"$(RIGID_NATIVE_ID)\" "$(RIGID_TOPPLING_PROBE_SOURCE)" $(RIGID_NATIVE_SRC) -o "$@" -lm
