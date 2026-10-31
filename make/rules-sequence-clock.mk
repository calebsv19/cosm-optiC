# Camera framework gates. Build output follows the caller-selected BUILD_DIR_BASE.
.PHONY: test-sequence-clock test-sequence-clock-sanitize
SEQUENCE_CLOCK_SRC := src/animation/sequence_time.c src/animation/sequence_clock.c src/animation/timeline_clock.c
SEQUENCE_CLOCK_HEADERS := include/animation/sequence_time.h include/animation/sequence_clock.h include/animation/timeline_clock.h src/animation/sequence_time_internal.h
SEQUENCE_CLOCK_FLAGS := -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -pedantic -Iinclude
$(BUILD_DIR)/tests/sequence_clock_test: tests/sequence_clock_test.c $(SEQUENCE_CLOCK_SRC) $(SEQUENCE_CLOCK_HEADERS)
	@mkdir -p $(dir $@)
	$(CLANG_CC) $(SEQUENCE_CLOCK_FLAGS) tests/sequence_clock_test.c $(SEQUENCE_CLOCK_SRC) -lm -o $@
$(BUILD_DIR)/tests/sequence_clock_probe: tests/sequence_clock_probe.c $(SEQUENCE_CLOCK_SRC) $(SEQUENCE_CLOCK_HEADERS)
	@mkdir -p $(dir $@)
	$(CLANG_CC) $(SEQUENCE_CLOCK_FLAGS) tests/sequence_clock_probe.c $(SEQUENCE_CLOCK_SRC) -lm -o $@
test-sequence-clock: $(BUILD_DIR)/tests/sequence_clock_test $(BUILD_DIR)/tests/sequence_clock_probe
	$(BUILD_DIR)/tests/sequence_clock_test
	python3 tests/integration/check_sequence_clock.py --probe $(BUILD_DIR)/tests/sequence_clock_probe
test-sequence-clock-sanitize:
	@mkdir -p $(BUILD_DIR)/tests
	$(CLANG_CC) $(SEQUENCE_CLOCK_FLAGS) -g -fsanitize=address,undefined -fno-omit-frame-pointer tests/sequence_clock_test.c $(SEQUENCE_CLOCK_SRC) -lm -o $(BUILD_DIR)/tests/sequence_clock_sanitize
	$(BUILD_DIR)/tests/sequence_clock_sanitize
.PHONY: test-sequence-clock-json test-sequence-clock-json-sanitize
SEQUENCE_JSON_FLAGS := $(SEQUENCE_CLOCK_FLAGS) $(shell pkg-config --cflags json-c)
SEQUENCE_JSON_SRC := tests/sequence_clock_json_test.c src/import/sequence_clock_json.c $(SEQUENCE_CLOCK_SRC)
test-sequence-clock-json:
	@mkdir -p $(BUILD_DIR)/tests
	$(CLANG_CC) $(SEQUENCE_JSON_FLAGS) $(SEQUENCE_JSON_SRC) $(shell pkg-config --libs json-c) -lm -o $(BUILD_DIR)/tests/sequence_clock_json_test
	$(BUILD_DIR)/tests/sequence_clock_json_test
test-sequence-clock-json-sanitize:
	@mkdir -p $(BUILD_DIR)/tests
	$(CLANG_CC) $(SEQUENCE_JSON_FLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer $(SEQUENCE_JSON_SRC) $(shell pkg-config --libs json-c) -lm -o $(BUILD_DIR)/tests/sequence_clock_json_sanitize
	$(BUILD_DIR)/tests/sequence_clock_json_sanitize

.PHONY: camera-rig-probe
camera-rig-probe: $(BUILD_DIR)/tests/camera_rig_probe
$(BUILD_DIR)/tests/camera_rig_probe: $(BUILD_DIR)/tests/camera_rig_probe.o $(RAY_TRACING_RENDER_HEADLESS_DEPS)
	@mkdir -p $(dir $@)
	$(CC) $(sort $^) -o $@ $(LDFLAGS)

.PHONY: test-camera-sequence test-camera-sequence-sanitize
CAMERA_SEQUENCE_SRC := src/animation/camera_sequence.c $(SEQUENCE_CLOCK_SRC)
test-camera-sequence:
	@mkdir -p $(BUILD_DIR)/tests
	$(CLANG_CC) $(SEQUENCE_CLOCK_FLAGS) tests/camera_sequence_test.c $(CAMERA_SEQUENCE_SRC) -lm -o $(BUILD_DIR)/tests/camera_sequence_test
	$(BUILD_DIR)/tests/camera_sequence_test
test-camera-sequence-sanitize:
	@mkdir -p $(BUILD_DIR)/tests
	$(CLANG_CC) $(SEQUENCE_CLOCK_FLAGS) -g -fsanitize=address,undefined -fno-omit-frame-pointer tests/camera_sequence_test.c $(CAMERA_SEQUENCE_SRC) -lm -o $(BUILD_DIR)/tests/camera_sequence_sanitize
	$(BUILD_DIR)/tests/camera_sequence_sanitize
.PHONY: test-inspection-orbit test-inspection-orbit-sanitize
INSPECTION_ORBIT_SRC := tests/inspection_orbit_test.c src/animation/inspection_orbit.c src/motion/motion_route_schedule.c src/motion/motion_timing_schedule.c src/motion/motion_timing_duration.c src/motion/motion_timing_plan.c
test-inspection-orbit:
	@mkdir -p $(BUILD_DIR)/tests
	$(CLANG_CC) $(SEQUENCE_CLOCK_FLAGS) $(INSPECTION_ORBIT_SRC) -lm -o $(BUILD_DIR)/tests/inspection_orbit_test
	$(BUILD_DIR)/tests/inspection_orbit_test
test-inspection-orbit-sanitize:
	@mkdir -p $(BUILD_DIR)/tests
	$(CLANG_CC) $(SEQUENCE_CLOCK_FLAGS) -g -fsanitize=address,undefined -fno-omit-frame-pointer $(INSPECTION_ORBIT_SRC) -lm -o $(BUILD_DIR)/tests/inspection_orbit_sanitize
	$(BUILD_DIR)/tests/inspection_orbit_sanitize
.PHONY: inspection-subframe-probe
inspection-subframe-probe: $(BUILD_DIR)/tests/inspection_subframe_probe
$(BUILD_DIR)/tests/inspection_subframe_probe: $(BUILD_DIR)/tests/inspection_subframe_probe.o $(RAY_TRACING_RENDER_HEADLESS_DEPS)
	@mkdir -p $(dir $@)
	$(CC) $(sort $^) -o $@ $(LDFLAGS)

.PHONY: volume-sequence-probe
volume-sequence-probe: $(BUILD_DIR)/tests/volume_sequence_probe
$(BUILD_DIR)/tests/volume_sequence_probe: $(BUILD_DIR)/tests/volume_sequence_probe.o $(RAY_TRACING_RENDER_HEADLESS_DEPS)
	@mkdir -p $(dir $@)
	$(CC) $(sort $^) -o $@ $(LDFLAGS)

.PHONY: test-event-camera-geometry test-event-camera-geometry-sanitize
EVENT_CAMERA_GEOMETRY_SRC := tests/event_camera_geometry_test.c src/animation/event_camera_geometry.c src/animation/inspection_orbit.c src/motion/motion_frame.c src/motion/motion_route_schedule.c src/motion/motion_route_geometry.c src/motion/motion_timing_schedule.c src/motion/motion_timing_duration.c src/motion/motion_timing_plan.c
# The shared M5 geometry source has its own maintained dependencies in the app;
# standalone tests reuse the same analytic scheduling closure as orbit tests.
EVENT_CAMERA_GEOMETRY_SRC := $(filter-out src/motion/motion_route_geometry.c,$(EVENT_CAMERA_GEOMETRY_SRC))
test-event-camera-geometry:
	@mkdir -p $(BUILD_DIR)/tests
	$(CLANG_CC) $(SEQUENCE_CLOCK_FLAGS) $(EVENT_CAMERA_GEOMETRY_SRC) -lm -o $(BUILD_DIR)/tests/event_camera_geometry_test
	$(BUILD_DIR)/tests/event_camera_geometry_test
test-event-camera-geometry-sanitize:
	@mkdir -p $(BUILD_DIR)/tests
	$(CLANG_CC) $(SEQUENCE_CLOCK_FLAGS) -g -fsanitize=address,undefined -fno-omit-frame-pointer $(EVENT_CAMERA_GEOMETRY_SRC) -lm -o $(BUILD_DIR)/tests/event_camera_geometry_sanitize
	$(BUILD_DIR)/tests/event_camera_geometry_sanitize

.PHONY: event-camera-probe
event-camera-probe: $(BUILD_DIR)/tests/event_camera_probe
$(BUILD_DIR)/tests/event_camera_probe: $(BUILD_DIR)/tests/event_camera_probe.o $(RAY_TRACING_RENDER_HEADLESS_DEPS)
	@mkdir -p $(dir $@)
	$(CC) $(sort $^) -o $@ $(LDFLAGS)

.PHONY: test-event-camera-lead test-event-camera-lead-sanitize
EVENT_CAMERA_LEAD_SRC := tests/event_camera_lead_test.c src/animation/event_camera_lead.c $(filter-out tests/event_camera_geometry_test.c,$(EVENT_CAMERA_GEOMETRY_SRC))
test-event-camera-lead:
	@mkdir -p $(BUILD_DIR)/tests
	$(CLANG_CC) $(SEQUENCE_CLOCK_FLAGS) $(EVENT_CAMERA_LEAD_SRC) -lm -o $(BUILD_DIR)/tests/event_camera_lead_test
	$(BUILD_DIR)/tests/event_camera_lead_test
test-event-camera-lead-sanitize:
	@mkdir -p $(BUILD_DIR)/tests
	$(CLANG_CC) $(SEQUENCE_CLOCK_FLAGS) -g -fsanitize=address,undefined -fno-omit-frame-pointer $(EVENT_CAMERA_LEAD_SRC) -lm -o $(BUILD_DIR)/tests/event_camera_lead_sanitize
	$(BUILD_DIR)/tests/event_camera_lead_sanitize

# Small deterministic framework gate included in test-stable. Native source
# adapters and visual checks additionally require caller-owned recorded fixtures.
.PHONY: test-camera-framework test-camera-framework-sanitize
test-camera-framework: test-sequence-clock test-sequence-clock-json test-camera-sequence test-inspection-orbit test-event-camera-geometry test-event-camera-lead
	python3 -B tests/integration/test_sequence_catalog.py
test-camera-framework-sanitize: test-sequence-clock-sanitize test-sequence-clock-json-sanitize test-camera-sequence-sanitize test-inspection-orbit-sanitize test-event-camera-geometry-sanitize test-event-camera-lead-sanitize

.PHONY: test-event-camera-validation test-event-camera-validation-sanitize
EVENT_VALIDATION_SRC := tests/event_camera_validation_test.c src/import/event_camera_validation.c src/import/event_camera_json.c src/app/ray_tracing_sha256.c
test-event-camera-validation:
	@mkdir -p $(BUILD_DIR)/tests
	$(CC) $(CFLAGS) $(CPPFLAGS) $(EVENT_VALIDATION_SRC) -o $(BUILD_DIR)/tests/event_camera_validation_test $(LDFLAGS)
	$(BUILD_DIR)/tests/event_camera_validation_test
test-event-camera-validation-sanitize:
	@mkdir -p $(BUILD_DIR)/tests
	$(CC) $(CFLAGS) $(CPPFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer $(EVENT_VALIDATION_SRC) -o $(BUILD_DIR)/tests/event_camera_validation_sanitize $(LDFLAGS)
	$(BUILD_DIR)/tests/event_camera_validation_sanitize

test-camera-framework: test-event-camera-validation
test-camera-framework-sanitize: test-event-camera-validation-sanitize

.PHONY: test-sequence-encoder test-sequence-job-recovery
test-sequence-encoder:
	python3 -B tests/integration/check_sequence_encoder.py
test-sequence-job-recovery: ray-tracing-job-runner ray-tracing-worker-runtime ray-tracing-render-headless
	@test -n "$(SEQUENCE_RECOVERY_FIXTURE)" -a -n "$(SEQUENCE_RECOVERY_OUTPUT)" || (echo "Set SEQUENCE_RECOVERY_FIXTURE and a new SEQUENCE_RECOVERY_OUTPUT"; exit 2)
	python3 -B tests/integration/check_sequence_job_recovery.py --fixture "$(SEQUENCE_RECOVERY_FIXTURE)" --out "$(SEQUENCE_RECOVERY_OUTPUT)" --runner "$(RAY_TRACING_JOB_RUNNER_BIN)"

.PHONY: test-sequence-job-reporting
test-sequence-job-reporting:
	python3 -B tests/integration/check_sequence_job_reporting.py
test-camera-framework: test-sequence-job-reporting
