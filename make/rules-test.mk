STABLE_TEST_TARGETS := \
	test \
	test-ray-tracing-core-sim-runtime-frame-contract \
	test-scene-editor-pane-host-contract \
	test-manifest-to-trace-export \
	test-fluid-pack-contract-parity \
	test-trio-scene-contract-diff \
	test-shared-theme-font-adapter \
	test-ray-tracing-workspace-authoring-host

LEGACY_TEST_TARGETS :=

run: $(TARGET)
	./$(TARGET)

run-ide-theme: $(TARGET)
	RAY_TRACING_USE_SHARED_THEME_FONT=1 RAY_TRACING_USE_SHARED_THEME=1 RAY_TRACING_USE_SHARED_FONT=1 RAY_TRACING_THEME_PRESET=ide_gray RAY_TRACING_FONT_PRESET=ide ./$(TARGET)

run-daw-theme: $(TARGET)
	RAY_TRACING_USE_SHARED_THEME_FONT=1 RAY_TRACING_USE_SHARED_THEME=1 RAY_TRACING_USE_SHARED_FONT=1 RAY_TRACING_THEME_PRESET=daw_default RAY_TRACING_FONT_PRESET=daw_default ./$(TARGET)

run-headless-smoke: all test-stable
	@echo "ray_tracing headless smoke passed (non-interactive)"

visual-harness: $(TARGET)
	@echo "visual harness binary ready: $(TARGET)"

test: $(TARGET) $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_OBJ) $(TEST_DEPS)
	$(CC) $(TEST_OBJ) $(TEST_DEPS) -o $@ $(LDFLAGS)

SHARED_THEME_FONT_ADAPTER_TEST_BIN := $(BUILD_DIR)/tests/shared_theme_font_adapter_test
SHARED_THEME_FONT_ADAPTER_TEST_SRCS := \
	$(TEST_DIR)/shared_theme_font_adapter_test.c \
	$(SRC_DIR)/ui/menu/shared_theme_font_adapter.c \
	$(CORE_THEME_DIR)/src/core_theme.c \
	$(CORE_FONT_DIR)/src/core_font.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_BASE_DIR)/src/core_base.c

$(SHARED_THEME_FONT_ADAPTER_TEST_BIN): $(SHARED_THEME_FONT_ADAPTER_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include \
		$(SHARED_THEME_FONT_ADAPTER_TEST_SRCS) -o $@ $(LDFLAGS)

test-shared-theme-font-adapter: $(SHARED_THEME_FONT_ADAPTER_TEST_BIN)
	@$(SHARED_THEME_FONT_ADAPTER_TEST_BIN) || (echo "shared theme/font adapter test failed."; exit 1)

RAY_TRACING_WORKSPACE_AUTHORING_HOST_TEST_BIN := $(BUILD_DIR)/tests/ray_tracing_workspace_authoring_host_test
RAY_TRACING_WORKSPACE_AUTHORING_HOST_TEST_SRCS := \
	$(TEST_DIR)/ray_tracing_workspace_authoring_host_test.c \
	$(SRC_DIR)/ui/menu/workspace_authoring/ray_tracing_workspace_authoring_host.c \
	$(SRC_DIR)/ui/menu/shared_theme_font_adapter.c \
	$(KIT_WORKSPACE_AUTHORING_DIR)/src/kit_workspace_authoring.c \
	$(KIT_WORKSPACE_AUTHORING_DIR)/src/ui/kit_workspace_authoring_ui_overlay.c \
	$(KIT_WORKSPACE_AUTHORING_DIR)/src/ui/kit_workspace_authoring_ui_font_theme.c \
	$(CORE_THEME_DIR)/src/core_theme.c \
	$(CORE_FONT_DIR)/src/core_font.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_BASE_DIR)/src/core_base.c

$(RAY_TRACING_WORKSPACE_AUTHORING_HOST_TEST_BIN): $(RAY_TRACING_WORKSPACE_AUTHORING_HOST_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(KIT_WORKSPACE_AUTHORING_DIR)/include -I$(CORE_PANE_DIR)/include -I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include \
		$(RAY_TRACING_WORKSPACE_AUTHORING_HOST_TEST_SRCS) -o $@ $(LDFLAGS)

test-ray-tracing-workspace-authoring-host: $(RAY_TRACING_WORKSPACE_AUTHORING_HOST_TEST_BIN)
	@$(RAY_TRACING_WORKSPACE_AUTHORING_HOST_TEST_BIN) || (echo "ray tracing workspace authoring host test failed."; exit 1)

RAY_TRACING_CORE_SIM_RUNTIME_FRAME_TEST_BIN := $(BUILD_DIR)/tests/ray_tracing_core_sim_runtime_frame_contract_test
RAY_TRACING_CORE_SIM_RUNTIME_FRAME_TEST_SRCS := \
	$(TEST_DIR)/ray_tracing_core_sim_runtime_frame_contract_test.c \
	$(SRC_DIR)/app/ray_tracing_core_sim_runtime_frame.c \
	$(SRC_DIR)/app/ray_tracing_app_main.c \
	$(CORE_SIM_DIR)/src/core_sim.c

$(RAY_TRACING_CORE_SIM_RUNTIME_FRAME_TEST_BIN): $(RAY_TRACING_CORE_SIM_RUNTIME_FRAME_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -g \
		-I$(INC_DIR) -I$(SRC_DIR) -I$(CORE_SIM_DIR)/include \
		-o $@ $(RAY_TRACING_CORE_SIM_RUNTIME_FRAME_TEST_SRCS) -lm

test-ray-tracing-core-sim-runtime-frame-contract: $(RAY_TRACING_CORE_SIM_RUNTIME_FRAME_TEST_BIN)
	@$(RAY_TRACING_CORE_SIM_RUNTIME_FRAME_TEST_BIN) || (echo "ray tracing core_sim runtime frame contract test failed."; exit 1)

SCENE_EDITOR_PANE_HOST_TEST_BIN := $(BUILD_DIR)/tests/scene_editor_pane_host_contract_test
SCENE_EDITOR_PANE_HOST_TEST_SRCS := \
	$(TEST_DIR)/scene_editor_pane_host_contract_test.c \
	$(TEST_DIR)/kit_render_backend_vk_stub.c \
	$(SRC_DIR)/editor/scene_editor_pane_host.c \
	$(CORE_PANE_DIR)/src/core_pane.c \
	$(KIT_PANE_DIR)/src/kit_pane.c \
	$(KIT_RENDER_DIR)/src/kit_render.c \
	$(KIT_RENDER_DIR)/src/kit_render_backend_null.c \
	$(CORE_THEME_DIR)/src/core_theme.c \
	$(CORE_FONT_DIR)/src/core_font.c \
	$(CORE_BASE_DIR)/src/core_base.c

$(SCENE_EDITOR_PANE_HOST_TEST_BIN): $(SCENE_EDITOR_PANE_HOST_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -g -DKIT_RENDER_ENABLE_VK_BACKEND=0 \
		-I$(INC_DIR) -I$(SRC_DIR) -I$(CORE_PANE_DIR)/include -I$(KIT_PANE_DIR)/include -I$(KIT_RENDER_DIR)/include -I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include -I$(CORE_BASE_DIR)/include \
		-o $@ $(SCENE_EDITOR_PANE_HOST_TEST_SRCS) -lm

test-scene-editor-pane-host-contract: $(SCENE_EDITOR_PANE_HOST_TEST_BIN)
	@$(SCENE_EDITOR_PANE_HOST_TEST_BIN) || (echo "scene editor pane host contract test failed."; exit 1)

test-manifest-to-trace-export: ray_trace_tool
	tests/integration/run_manifest_to_trace_export.sh

test-fluid-pack-contract-parity:
	tests/integration/run_fluid_pack_contract_parity.sh

test-trio-scene-contract-diff:
	tests/integration/run_trio_scene_contract_diff.sh

test-stable:
	@$(MAKE) $(STABLE_TEST_TARGETS)
	@echo "ray_tracing stable test lane passed"

test-legacy:
	@if [ -z "$(strip $(LEGACY_TEST_TARGETS))" ]; then \
		echo "ray_tracing legacy test lane is empty"; \
		exit 0; \
	fi; \
	set +e; \
	fails=0; \
	for t in $(LEGACY_TEST_TARGETS); do \
		echo "[legacy] running $$t"; \
		$(MAKE) $$t || fails=1; \
	done; \
	if [ $$fails -ne 0 ]; then \
		echo "[legacy] one or more legacy tests failed"; \
		exit 1; \
	fi
