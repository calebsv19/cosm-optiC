# Detached numerical gate; explicit header prerequisites avoid stale test code.
.PHONY: test-scene-atmosphere-attachment test-scene-atmosphere-attachment-sanitize
ATMOSPHERE_ATTACHMENT_TEST_SRC := tests/scene_atmosphere_attachment_test.c src/app/scene_atmosphere_attachment.c src/app/scene_atmosphere_appearance.c src/animation/timeline_clock.c
ATMOSPHERE_ATTACHMENT_TEST_FLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude $(shell pkg-config --cflags --libs json-c)
$(BUILD_DIR)/tests/scene_atmosphere_attachment_test: $(ATMOSPHERE_ATTACHMENT_TEST_SRC) include/app/scene_atmosphere_appearance.h include/app/scene_atmosphere_attachment.h include/animation/timeline_clock.h include/animation/timeline_track.h
	@mkdir -p $(dir $@)
	$(CLANG_CC) $(ATMOSPHERE_ATTACHMENT_TEST_FLAGS) $(ATMOSPHERE_ATTACHMENT_TEST_SRC) -lm -o $@
test-scene-atmosphere-attachment: $(BUILD_DIR)/tests/scene_atmosphere_attachment_test
	@$<
test-scene-atmosphere-attachment-sanitize:
	@mkdir -p $(BUILD_DIR)/tests
	$(CLANG_CC) $(ATMOSPHERE_ATTACHMENT_TEST_FLAGS) -g -fsanitize=address,undefined -fno-omit-frame-pointer $(ATMOSPHERE_ATTACHMENT_TEST_SRC) -lm -o $(BUILD_DIR)/tests/scene_atmosphere_attachment_sanitize
	$(BUILD_DIR)/tests/scene_atmosphere_attachment_sanitize

.PHONY: test-atmosphere-caustic-placement-sanitize
ATMOSPHERE_CAUSTIC_PLACEMENT_TEST_SRC := tests/runtime_caustic_volume_cache_placement_main.c tests/test_runtime_caustic_volume_cache_3d.c src/render/runtime_volume_3d.c src/render/runtime_volume_3d_transform.c src/render/runtime_caustic_volume_cache_3d.c src/render/runtime_caustic_volume_cache_placement_3d.c src/render/runtime_caustic_photon_sparse_brick_cache_3d.c
test-atmosphere-caustic-placement-sanitize:
	@mkdir -p $(BUILD_DIR)/tests
	$(CLANG_CC) $(CFLAGS) -Itests -fsanitize=address,undefined -fno-omit-frame-pointer $(ATMOSPHERE_CAUSTIC_PLACEMENT_TEST_SRC) -lm -o $(BUILD_DIR)/tests/atmosphere_caustic_placement_sanitize
	$(BUILD_DIR)/tests/atmosphere_caustic_placement_sanitize
