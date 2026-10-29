# Test observer only; read after Ball Bounce's existing Makefile.
RIGID_UNEQUAL_PROBE_SOURCE := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/rigid_unequal_mass_probe.c)
.PHONY: rigid-unequal-mass-probe
rigid-unequal-mass-probe: $(CONTRACT_BUILD_DIR)/unequal_mass_probe
$(CONTRACT_BUILD_DIR)/unequal_mass_probe: $(RIGID_UNEQUAL_PROBE_SOURCE) $(RIGID_NATIVE_SRC) $(CONTRACT_BUILD_DIR)/rigid-scene-id.txt | $(CONTRACT_BUILD_DIR)
	$(CLANG) $(SCENE_FLAGS) -DBALL_SCENE_ENGINE_SHA256=\"$(SCENE_CLANG_ID)\" -DBALL_RIGID_SCENE_ENGINE_SHA256=\"$(RIGID_NATIVE_ID)\" "$(RIGID_UNEQUAL_PROBE_SOURCE)" $(RIGID_NATIVE_SRC) -o "$@" -lm
