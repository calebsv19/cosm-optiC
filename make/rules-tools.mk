cli-tools: $(CLI_TOOLS)

RAY_TRACE_TOOL_SRCS := \
	$(RAY_TRACE_TOOL_SRC) \
	$(SRC_DIR)/import/fluid_import.c \
	$(SRC_DIR)/import/fluid_pack_import.c \
	$(SRC_DIR)/import/scene_bundle_import.c \
	$(CORE_TRACE_DIR)/src/core_trace.c \
	$(CORE_PACK_DIR)/src/core_pack.c \
	$(CORE_SCENE_DIR)/src/core_scene.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(TIMER_HUD_DIR)/external/cJSON.c
RAY_TRACE_TOOL_INCS := \
	-I$(INC_DIR) -I$(SRC_DIR) \
	-I$(CORE_TRACE_DIR)/include -I$(CORE_PACK_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include \
	-I$(TIMER_HUD_DIR)/external

$(CLI_BIN_DIR)/shape_asset_tool: build/tools/cli/shape_asset_tool.o build/import/shape_import.o build/geo/geolib/shape_asset.o build/geo/geolib/shape_library.o build/tools/ShapeLib/shape_json.o build/tools/ShapeLib/shape_flatten.o build/tools/ShapeLib/shape_core.o build/timer_hud_external/cJSON.o
	@mkdir -p $(CLI_BIN_DIR)
	$(CC) $^ -o $@ $(LDFLAGS)

$(CLI_BIN_DIR)/shape_import_tool: build/tools/cli/shape_import_tool.o build/import/shape_import.o build/tools/ShapeLib/shape_json.o build/tools/ShapeLib/shape_flatten.o build/tools/ShapeLib/shape_core.o build/timer_hud_external/cJSON.o
	@mkdir -p $(CLI_BIN_DIR)
	$(CC) $^ -o $@ $(LDFLAGS)

$(CLI_BIN_DIR)/shape_sanity_tool: build/tools/cli/shape_sanity_tool.o
	@mkdir -p $(CLI_BIN_DIR)
	$(CC) $^ -o $@ $(LDFLAGS)

ray_trace_tool: $(RAY_TRACE_TOOL_SRCS)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -g $(RAY_TRACE_TOOL_INCS) -o $(RAY_TRACE_TOOL_BIN) $(RAY_TRACE_TOOL_SRCS) -lm

native3d-render-audit: $(NATIVE3D_AUDIT_BIN)
	@$(NATIVE3D_AUDIT_BIN)

$(NATIVE3D_AUDIT_BIN): $(NATIVE3D_AUDIT_OBJ) $(NATIVE3D_AUDIT_DEPS) $(BUILD_DIR)/render/adapters/timer_hud_headless_stub.o
	@mkdir -p $(dir $@)
	$(CC) $(NATIVE3D_AUDIT_OBJ) $(NATIVE3D_AUDIT_DEPS) $(BUILD_DIR)/render/adapters/timer_hud_headless_stub.o -o $@ $(LDFLAGS)

ray-tracing-render-headless: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	@echo "ray_tracing headless render CLI ready: $(RAY_TRACING_RENDER_HEADLESS_BIN)"

$(RAY_TRACING_RENDER_HEADLESS_BIN): $(RAY_TRACING_RENDER_HEADLESS_OBJ) $(RAY_TRACING_RENDER_HEADLESS_DEPS)
	@mkdir -p $(dir $@)
	$(CC) $(RAY_TRACING_RENDER_HEADLESS_OBJ) $(RAY_TRACING_RENDER_HEADLESS_DEPS) -o $@ $(LDFLAGS)

ray-tracing-job-runner: $(RAY_TRACING_JOB_RUNNER_BIN)
	@echo "ray_tracing detached job runner ready: $(RAY_TRACING_JOB_RUNNER_BIN)"

$(RAY_TRACING_JOB_RUNNER_BIN): $(RAY_TRACING_JOB_RUNNER_OBJ) $(RAY_TRACING_JOB_RUNNER_DEPS)
	@mkdir -p $(dir $@)
	$(CC) $(RAY_TRACING_JOB_RUNNER_OBJ) $(RAY_TRACING_JOB_RUNNER_DEPS) -o $@ $(LDFLAGS)

ray-tracing-material-preview-headless: $(RAY_TRACING_MATERIAL_PREVIEW_HEADLESS_BIN)
	@echo "ray_tracing material preview CLI ready: $(RAY_TRACING_MATERIAL_PREVIEW_HEADLESS_BIN)"

$(RAY_TRACING_MATERIAL_PREVIEW_HEADLESS_BIN): $(RAY_TRACING_MATERIAL_PREVIEW_HEADLESS_OBJ) $(RAY_TRACING_MATERIAL_PREVIEW_HEADLESS_DEPS)
	@mkdir -p $(dir $@)
	$(CC) $(RAY_TRACING_MATERIAL_PREVIEW_HEADLESS_OBJ) $(RAY_TRACING_MATERIAL_PREVIEW_HEADLESS_DEPS) -o $@ $(LDFLAGS)

manifest_to_trace: ray_trace_tool
	@if [ -z "$(MANIFEST)" ] || [ -z "$(TRACE)" ]; then \
		echo "usage: make manifest_to_trace MANIFEST=/path/source_manifest_or_bundle_or_frame TRACE=/path/output.trace.pack [BOUNCE=4]"; \
		exit 1; \
	fi
	@mkdir -p "$$(dirname "$(TRACE)")"
	@if [ -n "$(BOUNCE)" ]; then \
		./$(RAY_TRACE_TOOL_BIN) "$(MANIFEST)" "$(TRACE)" "$(BOUNCE)"; \
	else \
		./$(RAY_TRACE_TOOL_BIN) "$(MANIFEST)" "$(TRACE)"; \
	fi

video:
	@python3 tools/make_video.py --frames "$(VIDEO_FRAMES_DIR)" --output "$(VIDEO_OUTPUT)" --fps $(VIDEO_FPS)
SMOOTH_MESH_CORE_ROOT := third_party/codework_shared/core
SMOOTH_MESH_COMPILE_ROOT := $(SMOOTH_MESH_CORE_ROOT)/core_mesh_compile
SMOOTH_MESH_ASSET_ROOT := $(SMOOTH_MESH_CORE_ROOT)/core_mesh_asset
SMOOTH_MESH_RUNTIME_COMPILE_DEPS := \
	$(wildcard $(SMOOTH_MESH_COMPILE_ROOT)/src/*.c) \
	$(wildcard $(SMOOTH_MESH_COMPILE_ROOT)/src/*.h) \
	$(wildcard $(SMOOTH_MESH_COMPILE_ROOT)/include/*.h) \
	$(wildcard $(SMOOTH_MESH_ASSET_ROOT)/src/*.c) \
	$(wildcard $(SMOOTH_MESH_ASSET_ROOT)/include/*.h)

$(SMOOTH_MESH_RUNTIME_COMPILE_TOOL_BIN): tools/smooth_mesh_reflection/compile_runtime_fixture.c $(SMOOTH_MESH_RUNTIME_COMPILE_DEPS)
	$(MAKE) -C $(SMOOTH_MESH_COMPILE_ROOT)
	$(MAKE) -C $(SMOOTH_MESH_ASSET_ROOT)
	$(MAKE) -C $(SMOOTH_MESH_CORE_ROOT)/core_object
	$(MAKE) -C $(SMOOTH_MESH_CORE_ROOT)/core_units
	$(MAKE) -C $(SMOOTH_MESH_CORE_ROOT)/core_io
	$(MAKE) -C $(SMOOTH_MESH_CORE_ROOT)/core_base
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -g \
		-I$(SMOOTH_MESH_COMPILE_ROOT)/include \
		-I$(SMOOTH_MESH_ASSET_ROOT)/include \
		-I$(SMOOTH_MESH_CORE_ROOT)/core_base/include \
		-I$(SMOOTH_MESH_CORE_ROOT)/core_io/include \
		-I$(SMOOTH_MESH_CORE_ROOT)/core_object/include \
		-I$(SMOOTH_MESH_CORE_ROOT)/core_units/include \
		-Ithird_party/codework_shared/shape/external/cjson \
		-o $@ $< \
		$(SMOOTH_MESH_COMPILE_ROOT)/build/libcore_mesh_compile.a \
		$(SMOOTH_MESH_ASSET_ROOT)/build/libcore_mesh_asset.a \
		third_party/codework_shared/shape/external/cjson/cJSON.c \
		$(SMOOTH_MESH_CORE_ROOT)/core_object/build/libcore_object.a \
		$(SMOOTH_MESH_CORE_ROOT)/core_units/build/libcore_units.a \
		$(SMOOTH_MESH_CORE_ROOT)/core_io/build/libcore_io.a \
		$(SMOOTH_MESH_CORE_ROOT)/core_base/build/libcore_base.a -lm

smooth-mesh-runtime-compile-tool: $(SMOOTH_MESH_RUNTIME_COMPILE_TOOL_BIN)
	@echo "smooth mesh runtime compile tool ready: $(SMOOTH_MESH_RUNTIME_COMPILE_TOOL_BIN)"
