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
