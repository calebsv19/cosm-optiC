# BR4 optional, explicitly source-bound development adoption. Public clones retain
# a refusal stub until core_scene's reviewed result API is imported normally.
RIGID_SCENE_CORE_DIR ?= $(if $(wildcard $(CORE_SCENE_DIR)/include/core_scene_rigid_result.h),$(CORE_SCENE_DIR),$(CODEWORK_WORKSPACE_ROOT)/shared/core/core_scene)
RIGID_RESULT_ENABLED := $(if $(wildcard $(RIGID_SCENE_CORE_DIR)/include/core_scene_rigid_result.h),1,0)
RIGID_RESULT_APP_OBJS := $(BUILD_DIR)/import/runtime_rigid_sequence.o $(BUILD_DIR)/import/runtime_rigid_result.o $(BUILD_DIR)/import/runtime_rigid_bundle.o $(BUILD_DIR)/import/runtime_rigid_bundle_v2.o $(BUILD_DIR)/import/runtime_rigid_asset_metadata.o
ifeq ($(RIGID_RESULT_ENABLED),1)
RIGID_RESULT_INPUTS ?= tools/rigid_result_shared_inputs.json
.PHONY: rigid-result-inputs-check
rigid-result-inputs-check:
	PYTHONDONTWRITEBYTECODE=1 python3 tools/verify_rigid_result_inputs.py --manifest $(RIGID_RESULT_INPUTS) --core-scene $(RIGID_SCENE_CORE_DIR) --cjson $(TIMER_HUD_DIR)/external/cJSON.c

CFLAGS += -DRAY_RIGID_RESULT_ENABLED -I$(RIGID_SCENE_CORE_DIR)/include
RIGID_RESULT_CORE_OBJS := $(BUILD_DIR)/rigid_core/core_scene_rigid.o $(BUILD_DIR)/rigid_core/core_scene_rigid_json.o $(BUILD_DIR)/rigid_core/core_scene_rigid_result.o
$(RIGID_RESULT_CORE_OBJS): | rigid-result-inputs-check
$(BUILD_DIR)/rigid_core/%.o: $(RIGID_SCENE_CORE_DIR)/src/%.c $(wildcard $(RIGID_SCENE_CORE_DIR)/include/core_scene_rigid*.h)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) $(CFLAGS) -I$(TIMER_HUD_DIR)/external -MMD -MP -c $< -o $@
endif
OBJ += $(RIGID_RESULT_CORE_OBJS)
TEST_DEPS += $(RIGID_RESULT_CORE_OBJS)
NATIVE3D_AUDIT_DEPS += $(RIGID_RESULT_APP_OBJS) $(RIGID_RESULT_CORE_OBJS)
DEP += $(RIGID_RESULT_CORE_OBJS:.o=.d)
.PHONY: rigid-result-consumer-probe
rigid-result-consumer-probe: $(BUILD_DIR)/tests/rigid_result_consumer_probe
$(BUILD_DIR)/tests/rigid_result_consumer_probe: $(BUILD_DIR)/tests/rigid_result_consumer_probe.o $(RAY_TRACING_RENDER_HEADLESS_DEPS)
	@mkdir -p $(dir $@)
	$(CC) $(sort $^) -o $@ $(LDFLAGS)
.PHONY: rigid-result-profile-probe
.PHONY: rigid-contact-pixel-probe
rigid-contact-pixel-probe: $(BUILD_DIR)/tests/rigid_contact_pixel_probe
$(BUILD_DIR)/tests/rigid_contact_pixel_probe: $(BUILD_DIR)/tests/rigid_contact_pixel_probe.o $(RAY_TRACING_RENDER_HEADLESS_DEPS)
	@mkdir -p $(dir $@)
	$(CC) $(sort $^) -o $@ $(LDFLAGS)

rigid-result-profile-probe: $(BUILD_DIR)/tests/rigid_result_profile_probe
$(BUILD_DIR)/tests/rigid_result_profile_probe: tests/rigid_result_profile_probe.c $(RIGID_RESULT_CORE_OBJS) $(BUILD_DIR)/core_scene_compile/core_scene_compile_digest.o $(BUILD_DIR)/timer_hud_external/cJSON.o
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) $(CFLAGS) $^ -o $@ $(LDFLAGS)
# Instrument every new/consumed rigid module and the native integration probe.
# Existing surrounding renderer objects are reused; this is not a whole-app
# sanitizer qualification or a numerical solver qualification.
RIGID_ASAN_FLAGS := -fsanitize=address,undefined -fno-omit-frame-pointer
RIGID_ASAN_CORE := $(patsubst $(BUILD_DIR)/rigid_core/%.o,$(BUILD_DIR)/rigid_asan/core/%.o,$(RIGID_RESULT_CORE_OBJS))
RIGID_ASAN_APP := $(BUILD_DIR)/rigid_asan/import/runtime_rigid_sequence.o $(BUILD_DIR)/rigid_asan/import/runtime_rigid_result.o $(BUILD_DIR)/rigid_asan/import/runtime_rigid_bundle.o $(BUILD_DIR)/rigid_asan/import/runtime_rigid_bundle_v2.o $(BUILD_DIR)/rigid_asan/import/runtime_rigid_asset_metadata.o
RIGID_CLOCK_APP_NAMES := scene_atmosphere_sequence scene_atmosphere_attachment scene_atmosphere_runtime scene_atmosphere_prepare evaluated_scene_service
RIGID_ASAN_APP += $(addprefix $(BUILD_DIR)/rigid_asan/app/,$(addsuffix .o,$(RIGID_CLOCK_APP_NAMES))) $(BUILD_DIR)/rigid_asan/animation/evaluated_scene_snapshot.o
RIGID_CLOCK_NORMAL_OBJS := $(addprefix $(BUILD_DIR)/app/,$(addsuffix .o,$(RIGID_CLOCK_APP_NAMES))) $(BUILD_DIR)/animation/evaluated_scene_snapshot.o
$(BUILD_DIR)/rigid_asan/app/%.o: src/app/%.c include/app/scene_atmosphere_attachment.h include/app/scene_atmosphere_sequence.h include/animation/evaluated_scene_snapshot.h
	@mkdir -p $(dir $@)
	$(CLANG_CC) $(CSTD) $(CFLAGS) $(RIGID_ASAN_FLAGS) -c $< -o $@
$(BUILD_DIR)/rigid_asan/animation/%.o: src/animation/%.c include/animation/evaluated_scene_snapshot.h
	@mkdir -p $(dir $@)
	$(CLANG_CC) $(CSTD) $(CFLAGS) $(RIGID_ASAN_FLAGS) -c $< -o $@

$(BUILD_DIR)/rigid_asan/core/%.o: $(RIGID_SCENE_CORE_DIR)/src/%.c $(wildcard $(RIGID_SCENE_CORE_DIR)/include/core_scene_rigid*.h)
	@mkdir -p $(dir $@)
	$(CLANG_CC) $(CSTD) $(CFLAGS) -I$(TIMER_HUD_DIR)/external -Werror $(RIGID_ASAN_FLAGS) -c $< -o $@
$(BUILD_DIR)/rigid_asan/import/%.o: src/import/%.c include/import/runtime_rigid_result.h src/import/runtime_rigid_result_internal.h
	@mkdir -p $(dir $@)
	$(CLANG_CC) $(CSTD) $(CFLAGS) -I$(TIMER_HUD_DIR)/external -Werror $(RIGID_ASAN_FLAGS) -c $< -o $@
$(BUILD_DIR)/rigid_asan/tests/%.o: tests/%.c
	@mkdir -p $(dir $@)
	$(CLANG_CC) $(CSTD) $(CFLAGS) -Werror $(RIGID_ASAN_FLAGS) -c $< -o $@
.PHONY: rigid-result-consumer-sanitize
rigid-result-consumer-sanitize: $(BUILD_DIR)/rigid_asan/consumer $(BUILD_DIR)/rigid_asan/profile
$(BUILD_DIR)/rigid_asan/consumer: $(BUILD_DIR)/rigid_asan/tests/rigid_result_consumer_probe.o $(RIGID_ASAN_CORE) $(RIGID_ASAN_APP) $(filter-out $(RIGID_RESULT_CORE_OBJS) $(RIGID_RESULT_APP_OBJS) $(RIGID_CLOCK_NORMAL_OBJS),$(RAY_TRACING_RENDER_HEADLESS_DEPS))
	$(CLANG_CC) $(sort $^) -o $@ $(LDFLAGS) $(RIGID_ASAN_FLAGS)
$(BUILD_DIR)/rigid_asan/profile: $(BUILD_DIR)/rigid_asan/tests/rigid_result_profile_probe.o $(RIGID_ASAN_CORE) $(BUILD_DIR)/core_scene_compile/core_scene_compile_digest.o $(BUILD_DIR)/timer_hud_external/cJSON.o
	$(CLANG_CC) $(sort $^) -o $@ $(LDFLAGS) $(RIGID_ASAN_FLAGS)

ifeq ($(RIGID_RESULT_ENABLED),1)
$(RIGID_ASAN_CORE) $(RIGID_ASAN_APP): | rigid-result-inputs-check
endif

.PHONY: rigid-workflow-probe
rigid-workflow-probe: $(BUILD_DIR)/tests/rigid_workflow_probe
$(BUILD_DIR)/tests/rigid_workflow_probe: $(BUILD_DIR)/tests/rigid_workflow_probe.o $(TEST_DEPS) $(BUILD_DIR)/tests/test_support.o $(BUILD_DIR)/tests/test_stubs.o $(if $(filter Darwin,$(UNAME_S)),$(BUILD_DIR)/platform/ray_tracing_macos_file_panel.o)
	@mkdir -p $(dir $@)
	$(CC) $(sort $^) -o $@ $(LDFLAGS)

.PHONY: rigid-compound-metadata-contract
rigid-compound-metadata-contract: $(BUILD_DIR)/tests/rigid_compound_metadata_contract
	$<
$(BUILD_DIR)/tests/rigid_compound_metadata_contract: tests/rigid_compound_metadata_contract.c src/import/runtime_rigid_asset_metadata.c src/import/runtime_rigid_asset_metadata.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) $(CFLAGS) tests/rigid_compound_metadata_contract.c src/import/runtime_rigid_asset_metadata.c -o $@ $(LDFLAGS)

.PHONY: rigid-job-profile-probe
rigid-job-profile-probe: $(BUILD_DIR)/tests/rigid_job_profile_probe
$(BUILD_DIR)/tests/rigid_job_profile_probe: $(BUILD_DIR)/tests/rigid_job_profile_probe.o $(TEST_DEPS) $(BUILD_DIR)/tests/test_support.o $(BUILD_DIR)/tests/test_stubs.o $(if $(filter Darwin,$(UNAME_S)),$(BUILD_DIR)/platform/ray_tracing_macos_file_panel.o)
	@mkdir -p $(dir $@)
	$(CC) $(sort $^) -o $@ $(LDFLAGS)
