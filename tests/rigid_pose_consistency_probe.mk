# Supplemental test-only observer, read after Ball Bounce's Makefile.
RIGID_POSE_PROBE_SOURCE := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/rigid_pose_consistency_probe.c)
.PHONY: rigid-pose-consistency-probe
rigid-pose-consistency-probe: $(CONTRACT_BUILD_DIR)/pose_consistency_probe
$(CONTRACT_BUILD_DIR)/pose_consistency_probe: $(RIGID_POSE_PROBE_SOURCE) $(RIGID_NATIVE_SRC) $(CONTRACT_BUILD_DIR)/rigid-scene-id.txt | $(CONTRACT_BUILD_DIR)
	$(CLANG) $(SCENE_FLAGS) -DBALL_SCENE_ENGINE_SHA256=\"$(SCENE_CLANG_ID)\" -DBALL_RIGID_SCENE_ENGINE_SHA256=\"$(RIGID_NATIVE_ID)\" "$(RIGID_POSE_PROBE_SOURCE)" $(RIGID_NATIVE_SRC) -o "$@" -lm
