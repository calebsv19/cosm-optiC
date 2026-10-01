# Detached numerical gate; explicit header prerequisites avoid stale test code.
.PHONY: test-motion-timing-plan test-motion-timing-plan-sanitize
MOTION_TIMING_TEST_SRC := tests/motion_timing_plan_test.c src/motion/motion_timing_plan.c src/animation/timeline_clock.c
MOTION_TIMING_TEST_HEADERS := include/motion/motion_timing_plan.h include/animation/timeline_clock.h
$(BUILD_DIR)/tests/motion_timing_plan_test: $(MOTION_TIMING_TEST_SRC) $(MOTION_TIMING_TEST_HEADERS)
	@mkdir -p $(dir $@)
	$(CLANG_CC) -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude $(MOTION_TIMING_TEST_SRC) -lm -o $@
test-motion-timing-plan: $(BUILD_DIR)/tests/motion_timing_plan_test
	@$<
test-motion-timing-plan-sanitize:
	@mkdir -p $(BUILD_DIR)/tests
	$(CLANG_CC) -std=c11 -Wall -Wextra -Werror -pedantic -g -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude $(MOTION_TIMING_TEST_SRC) -lm -o $(BUILD_DIR)/tests/motion_timing_plan_sanitize
	$(BUILD_DIR)/tests/motion_timing_plan_sanitize

.PHONY: test-motion-timing-schedule test-motion-timing-schedule-sanitize
MOTION_SCHEDULE_SRC := tests/motion_timing_schedule_test.c src/motion/motion_timing_plan.c src/motion/motion_timing_duration.c src/motion/motion_timing_schedule.c src/animation/timeline_clock.c
MOTION_SCHEDULE_HEADERS := $(MOTION_TIMING_TEST_HEADERS) include/motion/motion_timing_duration.h include/motion/motion_timing_schedule.h
$(BUILD_DIR)/tests/motion_timing_schedule_test: $(MOTION_SCHEDULE_SRC) $(MOTION_SCHEDULE_HEADERS)
	@mkdir -p $(dir $@)
	$(CLANG_CC) -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude $(MOTION_SCHEDULE_SRC) -lm -o $@
test-motion-timing-schedule: $(BUILD_DIR)/tests/motion_timing_schedule_test
	@$<
test-motion-timing-schedule-sanitize:
	@mkdir -p $(BUILD_DIR)/tests
	$(CLANG_CC) -std=c11 -Wall -Wextra -Werror -pedantic -g -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude $(MOTION_SCHEDULE_SRC) -lm -o $(BUILD_DIR)/tests/motion_timing_schedule_sanitize
	$(BUILD_DIR)/tests/motion_timing_schedule_sanitize

.PHONY: test-motion-route-geometry test-motion-route-geometry-sanitize
MOTION_GEOMETRY_SRC := tests/motion_route_geometry_test.c src/motion/motion_route_geometry.c
MOTION_GEOMETRY_FLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude $(shell pkg-config --cflags json-c)
$(BUILD_DIR)/tests/motion_route_geometry_test: $(MOTION_GEOMETRY_SRC) include/motion/motion_route_geometry.h include/motion/scene_motion_paths.h
	@mkdir -p $(dir $@)
	$(CLANG_CC) $(MOTION_GEOMETRY_FLAGS) $(MOTION_GEOMETRY_SRC) -lm -o $@
test-motion-route-geometry: $(BUILD_DIR)/tests/motion_route_geometry_test
	@$<
test-motion-route-geometry-sanitize:
	@mkdir -p $(BUILD_DIR)/tests
	$(CLANG_CC) $(MOTION_GEOMETRY_FLAGS) -g -fsanitize=address,undefined -fno-omit-frame-pointer $(MOTION_GEOMETRY_SRC) -lm -o $(BUILD_DIR)/tests/motion_route_geometry_sanitize
	$(BUILD_DIR)/tests/motion_route_geometry_sanitize

.PHONY: test-motion-route-schedule test-motion-route-schedule-sanitize
MOTION_ROUTE_SCHEDULE_SRC := tests/motion_route_schedule_test.c src/motion/motion_route_geometry.c src/motion/motion_route_schedule.c src/motion/motion_timing_plan.c src/motion/motion_timing_duration.c src/motion/motion_timing_schedule.c
$(BUILD_DIR)/tests/motion_route_schedule_test: $(MOTION_ROUTE_SCHEDULE_SRC) $(MOTION_SCHEDULE_HEADERS) include/motion/motion_route_geometry.h include/motion/motion_route_schedule.h
	@mkdir -p $(dir $@)
	$(CLANG_CC) $(MOTION_GEOMETRY_FLAGS) $(MOTION_ROUTE_SCHEDULE_SRC) -lm -o $@
test-motion-route-schedule: $(BUILD_DIR)/tests/motion_route_schedule_test
	@$<
test-motion-route-schedule-sanitize:
	@mkdir -p $(BUILD_DIR)/tests
	$(CLANG_CC) $(MOTION_GEOMETRY_FLAGS) -g -fsanitize=address,undefined -fno-omit-frame-pointer $(MOTION_ROUTE_SCHEDULE_SRC) -lm -o $(BUILD_DIR)/tests/motion_route_schedule_sanitize
	$(BUILD_DIR)/tests/motion_route_schedule_sanitize

MOTION_ORIENTATION_SRC := tests/motion_orientation_test.c src/motion/motion_frame.c src/motion/scene_motion_orientation.c
$(BUILD_DIR)/tests/motion_orientation_test: $(MOTION_ORIENTATION_SRC) include/motion/motion_frame.h include/motion/scene_motion_paths.h
	@mkdir -p $(dir $@)
	$(CLANG_CC) $(MOTION_GEOMETRY_FLAGS) $(MOTION_ORIENTATION_SRC) -lm -o $@
.PHONY: test-motion-orientation test-motion-orientation-sanitize
test-motion-orientation: $(BUILD_DIR)/tests/motion_orientation_test
	@$<
test-motion-orientation-sanitize:
	@mkdir -p $(BUILD_DIR)/tests
	$(CLANG_CC) $(MOTION_GEOMETRY_FLAGS) -g -fsanitize=address,undefined -fno-omit-frame-pointer $(MOTION_ORIENTATION_SRC) -lm -o $(BUILD_DIR)/tests/motion_orientation_sanitize
	$(BUILD_DIR)/tests/motion_orientation_sanitize
