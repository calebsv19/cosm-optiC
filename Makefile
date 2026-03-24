CC        := cc
CSTD      := -std=c11
SRC_DIR   := src
INC_DIR   := include
BUILD_DIR := build
TARGET    := Ray_anim
VK_RENDERER_DIR := ../shared/vk_renderer
CORE_BASE_DIR := ../shared/core/core_base
CORE_IO_DIR := ../shared/core/core_io
CORE_DATA_DIR := ../shared/core/core_data
CORE_PACK_DIR := ../shared/core/core_pack
CORE_TIME_DIR := ../shared/core/core_time
CORE_SCENE_DIR := ../shared/core/core_scene
CORE_TRACE_DIR := ../shared/core/core_trace
CORE_SPACE_DIR := ../shared/core/core_space
CORE_THEME_DIR := ../shared/core/core_theme
CORE_FONT_DIR := ../shared/core/core_font
KIT_VIZ_DIR := ../shared/kit/kit_viz
UNAME_S := $(shell uname -s)

SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS   := $(shell sdl2-config --libs)
SDL_PREFIX := $(shell sdl2-config --prefix 2>/dev/null)
SDL_EXTRA_INC := $(if $(SDL_PREFIX),-I$(SDL_PREFIX)/include,)

VULKAN_CFLAGS :=
VULKAN_LIBS :=

ifeq ($(UNAME_S),Linux)
    VULKAN_CFLAGS := $(shell pkg-config --cflags vulkan 2>/dev/null)
    VULKAN_LIBS := $(shell pkg-config --libs vulkan 2>/dev/null)
    ifeq ($(strip $(VULKAN_CFLAGS)$(VULKAN_LIBS)),)
        VULKAN_CFLAGS := -I/usr/include
        VULKAN_LIBS := -lvulkan
    endif
endif

ifeq ($(UNAME_S),Darwin)
    VULKAN_CFLAGS := $(shell pkg-config --cflags vulkan 2>/dev/null)
    VULKAN_LIBS := $(shell pkg-config --libs vulkan 2>/dev/null)
    ifeq ($(strip $(VULKAN_CFLAGS)$(VULKAN_LIBS)),)
        VULKAN_CFLAGS := -I/opt/homebrew/include
        VULKAN_LIBS := -L/opt/homebrew/lib -lvulkan
    endif
    VULKAN_LIBS += -framework Metal -framework QuartzCore -framework Cocoa -framework IOKit -framework CoreVideo
    CFLAGS += -DVK_USE_PLATFORM_METAL_EXT
endif

CFLAGS  := $(CSTD) -Wall -Wextra -Wpedantic -g $(SDL_CFLAGS) $(SDL_EXTRA_INC) -I$(INC_DIR) -Isrc -Isrc/tools -Isrc/tools/ShapeLib -DMAIN_DRIVER
LDFLAGS := $(SDL_LIBS) -lSDL2_ttf -ljson-c -lm

CFLAGS_RELEASE := $(CSTD) -Wall -Wextra -Wpedantic -O3 $(SDL_CFLAGS) $(SDL_EXTRA_INC) -I$(INC_DIR) -Isrc -Isrc/tools -Isrc/tools/ShapeLib -DMAIN_DRIVER -DNDEBUG \
	-ffast-math -fno-math-errno -march=native
REL_BUILD_DIR := build_release
REL_TARGET := Ray_anim_release

CLI_BIN_DIR := tools/cli/bin
CLI_TOOLS := $(CLI_BIN_DIR)/shape_asset_tool $(CLI_BIN_DIR)/shape_import_tool $(CLI_BIN_DIR)/shape_sanity_tool
RAY_TRACE_TOOL_SRC := $(SRC_DIR)/tools/cli/ray_trace_tool.c
RAY_TRACE_TOOL_BIN := ray_trace_tool

VIDEO_FRAMES_DIR ?= Animations/default
VIDEO_OUTPUT ?= Animations/Vids/output.mp4
VIDEO_FPS ?= 30

TEST_DIR := tests
TEST_BIN := $(BUILD_DIR)/tests/test_runner
TEST_SRC := $(TEST_DIR)/test_runner.c
TEST_OBJ := $(BUILD_DIR)/tests/test_runner.o $(BUILD_DIR)/tests/test_stubs.o \
	$(BUILD_DIR)/tests/fluid_pack_import_test.o \
	$(BUILD_DIR)/tests/kit_viz_fluid_overlay_adapter_test.o \
	$(BUILD_DIR)/tests/render_metrics_dataset_test.o
TIMER_HUD_DIR := ../shared/timer_hud
TIMER_HUD_INCLUDE := -I$(TIMER_HUD_DIR)/include -I$(TIMER_HUD_DIR)/external

CFLAGS += $(TIMER_HUD_INCLUDE) -I$(VK_RENDERER_DIR)/include $(VULKAN_CFLAGS) \
	-I$(CORE_BASE_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_DATA_DIR)/include -I$(CORE_PACK_DIR)/include -I$(CORE_TIME_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_TRACE_DIR)/include -I$(CORE_SPACE_DIR)/include -I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include -I$(KIT_VIZ_DIR)/include \
	-DUSE_VULKAN=1 -DVK_RENDERER_SHADER_ROOT=\"$(abspath $(VK_RENDERER_DIR))\" \
	-include $(VK_RENDERER_DIR)/include/vk_renderer_sdl.h
LDFLAGS += $(VULKAN_LIBS)
CFLAGS_RELEASE += $(TIMER_HUD_INCLUDE) -I$(VK_RENDERER_DIR)/include $(VULKAN_CFLAGS) \
	-I$(CORE_BASE_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_DATA_DIR)/include -I$(CORE_PACK_DIR)/include -I$(CORE_TIME_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_TRACE_DIR)/include -I$(CORE_SPACE_DIR)/include -I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include -I$(KIT_VIZ_DIR)/include \
	-DUSE_VULKAN=1 -DVK_RENDERER_SHADER_ROOT=\"$(abspath $(VK_RENDERER_DIR))\" \
	-include $(VK_RENDERER_DIR)/include/vk_renderer_sdl.h
ifeq ($(UNAME_S),Darwin)
CORE_TIME_TEST_DEPS := $(BUILD_DIR)/core_time/core_time.o $(BUILD_DIR)/core_time/core_time_mac.o
else
CORE_TIME_TEST_DEPS := $(BUILD_DIR)/core_time/core_time.o $(BUILD_DIR)/core_time/core_time_posix.o
endif

TEST_DEPS := \
	$(BUILD_DIR)/render/material_bsdf.o \
	$(BUILD_DIR)/material/material_manager.o \
	$(BUILD_DIR)/material/material.o \
	$(BUILD_DIR)/render/integrators/direct_light_integrator.o \
	$(BUILD_DIR)/render/integrators/forward_light_integrator.o \
	$(BUILD_DIR)/render/integrators/hybrid/integrator_energy.o \
	$(BUILD_DIR)/render/integrators/hybrid/integrator_cache.o \
	$(BUILD_DIR)/render/integrators/hybrid/integrator_direct.o \
	$(BUILD_DIR)/render/integrators/hybrid/integrator_indirect.o \
	$(BUILD_DIR)/render/integrators/hybrid/integrator_sampling.o \
	$(BUILD_DIR)/render/integrators/hybrid/integrator_tonemap.o \
	$(BUILD_DIR)/render/integrators/hybrid/integrator_visibility.o \
	$(BUILD_DIR)/render/integrators/hybrid/camera_path_integrator.o \
	$(BUILD_DIR)/render/integrators/camera_path_integrator_disney.o \
	$(BUILD_DIR)/render/integrators/integrator_common.o \
	$(BUILD_DIR)/render/irradiance_cache.o \
	$(BUILD_DIR)/render/uniform_grid.o \
	$(BUILD_DIR)/render/surface_mesh.o \
	$(BUILD_DIR)/render/render_helper.o \
	$(BUILD_DIR)/render/ray_tracing2.o \
	$(BUILD_DIR)/render/fluid_state.o \
	$(BUILD_DIR)/render/fluid_overlay.o \
	$(BUILD_DIR)/scene/object_manager.o \
	$(BUILD_DIR)/geo/shape_adapter.o \
	$(BUILD_DIR)/geo/geolib/shape_asset.o \
	$(BUILD_DIR)/geo/geolib/shape_library.o \
	$(BUILD_DIR)/import/shape_import.o \
	$(BUILD_DIR)/timer_hud_external/cJSON.o \
	$(BUILD_DIR)/camera/camera.o \
	$(BUILD_DIR)/config/config_manager.o \
	$(BUILD_DIR)/tools/ShapeLib/shape_core.o \
	$(BUILD_DIR)/tools/ShapeLib/shape_json.o \
	$(BUILD_DIR)/tools/ShapeLib/shape_flatten.o \
	$(BUILD_DIR)/import/fluid_import.o \
	$(BUILD_DIR)/import/fluid_pack_import.o \
	$(BUILD_DIR)/import/scene_bundle_import.o \
	$(BUILD_DIR)/render/kit_viz_fluid_overlay_adapter.o \
	$(BUILD_DIR)/core_base/core_base.o \
	$(BUILD_DIR)/core_io/core_io.o \
	$(BUILD_DIR)/core_data/core_data.o \
	$(BUILD_DIR)/core_pack/core_pack.o \
	$(CORE_TIME_TEST_DEPS) \
	$(BUILD_DIR)/core_scene/core_scene.o \
	$(BUILD_DIR)/core_space/core_space.o \
	$(BUILD_DIR)/export/render_metrics_dataset.o \
	$(BUILD_DIR)/core_theme/core_theme.o \
	$(BUILD_DIR)/core_font/core_font.o \
	$(BUILD_DIR)/kit_viz/kit_viz.o \
	$(BUILD_DIR)/vk_renderer/vk_renderer.o \
	$(BUILD_DIR)/vk_renderer/vk_renderer_commands.o \
	$(BUILD_DIR)/vk_renderer/vk_renderer_config.o \
	$(BUILD_DIR)/vk_renderer/vk_renderer_context.o \
	$(BUILD_DIR)/vk_renderer/vk_renderer_device.o \
	$(BUILD_DIR)/vk_renderer/vk_renderer_pipeline.o \
	$(BUILD_DIR)/vk_renderer/vk_renderer_textures.o \
	$(BUILD_DIR)/vk_renderer/vk_renderer_memory.o

SRC := $(shell find $(SRC_DIR) -name '*.c' \
	! -path '$(SRC_DIR)/tools/cli/*' \
	! -path '$(SRC_DIR)/render/integrators/camera_path_integrator_old_version.c' \
	! -path '$(SRC_DIR)/render/TimerHUD_legacy_backup/*')
VK_RENDERER_SRCS := $(shell find $(VK_RENDERER_DIR)/src -name '*.c')
OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC))
OBJ := $(filter-out $(BUILD_DIR)/render/integrators/camera_path_integrator_old_version.o,$(OBJ))

TIMER_HUD_SRCS := $(shell find $(TIMER_HUD_DIR)/src -name '*.c')
TIMER_HUD_EXTERNAL_SRCS := $(TIMER_HUD_DIR)/external/cJSON.c
TIMER_HUD_OBJS := $(patsubst $(TIMER_HUD_DIR)/src/%.c,$(BUILD_DIR)/timer_hud/%.o,$(TIMER_HUD_SRCS))
TIMER_HUD_EXTERNAL_OBJS := $(patsubst $(TIMER_HUD_DIR)/external/%.c,$(BUILD_DIR)/timer_hud_external/%.o,$(TIMER_HUD_EXTERNAL_SRCS))
CORE_BASE_SRCS := $(CORE_BASE_DIR)/src/core_base.c
CORE_IO_SRCS := $(CORE_IO_DIR)/src/core_io.c
CORE_DATA_SRCS := $(CORE_DATA_DIR)/src/core_data.c
CORE_PACK_SRCS := $(CORE_PACK_DIR)/src/core_pack.c
CORE_TIME_SRCS := $(CORE_TIME_DIR)/src/core_time.c
ifeq ($(UNAME_S),Darwin)
CORE_TIME_SRCS += $(CORE_TIME_DIR)/src/core_time_mac.c
else
CORE_TIME_SRCS += $(CORE_TIME_DIR)/src/core_time_posix.c
endif
CORE_SCENE_SRCS := $(CORE_SCENE_DIR)/src/core_scene.c
CORE_SPACE_SRCS := $(CORE_SPACE_DIR)/src/core_space.c
CORE_THEME_SRCS := $(CORE_THEME_DIR)/src/core_theme.c
CORE_FONT_SRCS := $(CORE_FONT_DIR)/src/core_font.c
KIT_VIZ_SRCS := $(KIT_VIZ_DIR)/src/kit_viz.c
CORE_BASE_OBJS := $(patsubst $(CORE_BASE_DIR)/src/%.c,$(BUILD_DIR)/core_base/%.o,$(CORE_BASE_SRCS))
CORE_IO_OBJS := $(patsubst $(CORE_IO_DIR)/src/%.c,$(BUILD_DIR)/core_io/%.o,$(CORE_IO_SRCS))
CORE_DATA_OBJS := $(patsubst $(CORE_DATA_DIR)/src/%.c,$(BUILD_DIR)/core_data/%.o,$(CORE_DATA_SRCS))
CORE_PACK_OBJS := $(patsubst $(CORE_PACK_DIR)/src/%.c,$(BUILD_DIR)/core_pack/%.o,$(CORE_PACK_SRCS))
CORE_TIME_OBJS := $(patsubst $(CORE_TIME_DIR)/src/%.c,$(BUILD_DIR)/core_time/%.o,$(CORE_TIME_SRCS))
CORE_SCENE_OBJS := $(patsubst $(CORE_SCENE_DIR)/src/%.c,$(BUILD_DIR)/core_scene/%.o,$(CORE_SCENE_SRCS))
CORE_SPACE_OBJS := $(patsubst $(CORE_SPACE_DIR)/src/%.c,$(BUILD_DIR)/core_space/%.o,$(CORE_SPACE_SRCS))
CORE_THEME_OBJS := $(patsubst $(CORE_THEME_DIR)/src/%.c,$(BUILD_DIR)/core_theme/%.o,$(CORE_THEME_SRCS))
CORE_FONT_OBJS := $(patsubst $(CORE_FONT_DIR)/src/%.c,$(BUILD_DIR)/core_font/%.o,$(CORE_FONT_SRCS))
KIT_VIZ_OBJS := $(patsubst $(KIT_VIZ_DIR)/src/%.c,$(BUILD_DIR)/kit_viz/%.o,$(KIT_VIZ_SRCS))

OBJ := $(OBJ) $(TIMER_HUD_OBJS) $(TIMER_HUD_EXTERNAL_OBJS) \
	$(patsubst $(VK_RENDERER_DIR)/src/%.c,$(BUILD_DIR)/vk_renderer/%.o,$(VK_RENDERER_SRCS)) \
	$(CORE_BASE_OBJS) $(CORE_IO_OBJS) $(CORE_DATA_OBJS) $(CORE_PACK_OBJS) $(CORE_TIME_OBJS) $(CORE_SCENE_OBJS) $(CORE_SPACE_OBJS) $(CORE_THEME_OBJS) $(CORE_FONT_OBJS) $(KIT_VIZ_OBJS)
DEP := $(OBJ:.o=.d)

.PHONY: add-disney-flag
add-disney-flag:
	$(eval CFLAGS += -DCAMERA_INTEGRATOR_DISNEY_AVAILABLE)

.PHONY: all clean run run-ide-theme run-daw-theme debug format video release relrun test test-shared-theme-font-adapter test-manifest-to-trace-export test-fluid-pack-contract-parity

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

cli-tools: $(CLI_TOOLS)

RAY_TRACE_TOOL_SRCS := \
	$(RAY_TRACE_TOOL_SRC) \
	$(SRC_DIR)/import/fluid_import.c \
	$(SRC_DIR)/import/fluid_pack_import.c \
	$(SRC_DIR)/import/scene_bundle_import.c \
	$(CORE_TRACE_DIR)/src/core_trace.c \
	$(CORE_PACK_DIR)/src/core_pack.c \
	$(CORE_SCENE_DIR)/src/core_scene.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(TIMER_HUD_DIR)/external/cJSON.c
RAY_TRACE_TOOL_INCS := \
	-I$(INC_DIR) -I$(SRC_DIR) \
	-I$(CORE_TRACE_DIR)/include -I$(CORE_PACK_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include \
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

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/timer_hud/%.o: $(TIMER_HUD_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/timer_hud_external/%.o: $(TIMER_HUD_DIR)/external/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/vk_renderer/%.o: $(VK_RENDERER_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_base/%.o: $(CORE_BASE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_io/%.o: $(CORE_IO_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_data/%.o: $(CORE_DATA_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_pack/%.o: $(CORE_PACK_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_time/%.o: $(CORE_TIME_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_scene/%.o: $(CORE_SCENE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_space/%.o: $(CORE_SPACE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_theme/%.o: $(CORE_THEME_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_font/%.o: $(CORE_FONT_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/kit_viz/%.o: $(KIT_VIZ_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@
$(BUILD_DIR)/tests/%.o: $(TEST_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

run: $(TARGET)
	./$(TARGET)

run-ide-theme: $(TARGET)
	RAY_TRACING_USE_SHARED_THEME_FONT=1 RAY_TRACING_USE_SHARED_THEME=1 RAY_TRACING_USE_SHARED_FONT=1 RAY_TRACING_THEME_PRESET=ide_gray RAY_TRACING_FONT_PRESET=ide ./$(TARGET)

run-daw-theme: $(TARGET)
	RAY_TRACING_USE_SHARED_THEME_FONT=1 RAY_TRACING_USE_SHARED_THEME=1 RAY_TRACING_USE_SHARED_FONT=1 RAY_TRACING_THEME_PRESET=daw_default RAY_TRACING_FONT_PRESET=daw_default ./$(TARGET)

debug: CFLAGS += -O0 -g3
debug: clean all

format:
	@command -v clang-format >/dev/null 2>&1 && clang-format -i $(SRC) $(shell find $(INC_DIR) -name '*.h') || echo "clang-format not found"

video:
	@python3 tools/make_video.py --frames "$(VIDEO_FRAMES_DIR)" --output "$(VIDEO_OUTPUT)" --fps $(VIDEO_FPS)

release:
	$(MAKE) BUILD_DIR=$(REL_BUILD_DIR) TARGET=$(REL_TARGET) CFLAGS="$(CFLAGS_RELEASE)" LDFLAGS="$(LDFLAGS)" all

relrun: release
	./$(REL_TARGET)

test: $(TARGET) $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_OBJ) $(TEST_DEPS)
	$(CC) $(TEST_OBJ) $(TEST_DEPS) -o $@ $(LDFLAGS)

SHARED_THEME_FONT_ADAPTER_TEST_BIN := $(BUILD_DIR)/tests/shared_theme_font_adapter_test
SHARED_THEME_FONT_ADAPTER_TEST_SRCS := \
	$(TEST_DIR)/shared_theme_font_adapter_test.c \
	$(SRC_DIR)/ui/shared_theme_font_adapter.c \
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

test-manifest-to-trace-export: ray_trace_tool
	tests/integration/run_manifest_to_trace_export.sh

test-fluid-pack-contract-parity:
	tests/integration/run_fluid_pack_contract_parity.sh

clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(REL_BUILD_DIR) $(REL_TARGET) $(RAY_TRACE_TOOL_BIN)

-include $(DEP)
