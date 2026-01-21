CC        := cc
CSTD      := -std=c11
SRC_DIR   := src
INC_DIR   := include
BUILD_DIR := build
TARGET    := Ray_anim

SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS   := $(shell sdl2-config --libs)
SDL_PREFIX := $(shell sdl2-config --prefix 2>/dev/null)
SDL_EXTRA_INC := $(if $(SDL_PREFIX),-I$(SDL_PREFIX)/include,)

CFLAGS  := $(CSTD) -Wall -Wextra -Wpedantic -g $(SDL_CFLAGS) $(SDL_EXTRA_INC) -I$(INC_DIR) -Isrc -Isrc/tools -Isrc/tools/ShapeLib -DMAIN_DRIVER
LDFLAGS := $(SDL_LIBS) -lSDL2_ttf -ljson-c -lm

CFLAGS_RELEASE := $(CSTD) -Wall -Wextra -Wpedantic -O3 $(SDL_CFLAGS) $(SDL_EXTRA_INC) -I$(INC_DIR) -Isrc -Isrc/tools -Isrc/tools/ShapeLib -DMAIN_DRIVER -DNDEBUG \
	-ffast-math -fno-math-errno -march=native
REL_BUILD_DIR := build_release
REL_TARGET := Ray_anim_release

CLI_BIN_DIR := tools/cli/bin
CLI_TOOLS := $(CLI_BIN_DIR)/shape_asset_tool $(CLI_BIN_DIR)/shape_import_tool $(CLI_BIN_DIR)/shape_sanity_tool

VIDEO_FRAMES_DIR ?= Animations/default
VIDEO_OUTPUT ?= Animations/Vids/output.mp4
VIDEO_FPS ?= 30

TEST_DIR := tests
TEST_BIN := $(BUILD_DIR)/tests/test_runner
TEST_SRC := $(TEST_DIR)/test_runner.c
TEST_OBJ := $(BUILD_DIR)/tests/test_runner.o $(BUILD_DIR)/tests/test_stubs.o
TIMER_HUD_DIR := ../shared/timer_hud
TIMER_HUD_INCLUDE := -I$(TIMER_HUD_DIR)/include -I$(TIMER_HUD_DIR)/external

CFLAGS += $(TIMER_HUD_INCLUDE)
CFLAGS_RELEASE += $(TIMER_HUD_INCLUDE)

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
	$(BUILD_DIR)/scene/object_manager.o \
	$(BUILD_DIR)/geo/shape_adapter.o \
	$(BUILD_DIR)/geo/geolib/shape_asset.o \
	$(BUILD_DIR)/geo/geolib/shape_library.o \
	$(BUILD_DIR)/timer_hud_external/cJSON.o \
	$(BUILD_DIR)/camera/camera.o \
	$(BUILD_DIR)/config/config_manager.o \
	$(BUILD_DIR)/tools/ShapeLib/shape_core.o \
	$(BUILD_DIR)/tools/ShapeLib/shape_json.o \
	$(BUILD_DIR)/tools/ShapeLib/shape_flatten.o

SRC := $(shell find $(SRC_DIR) -name '*.c' \
	! -path '$(SRC_DIR)/tools/cli/*' \
	! -path '$(SRC_DIR)/render/integrators/camera_path_integrator_old_version.c' \
	! -path '$(SRC_DIR)/render/TimerHUD_legacy_backup/*')
OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC))
OBJ := $(filter-out $(BUILD_DIR)/render/integrators/camera_path_integrator_old_version.o,$(OBJ))

TIMER_HUD_SRCS := $(shell find $(TIMER_HUD_DIR)/src -name '*.c')
TIMER_HUD_EXTERNAL_SRCS := $(TIMER_HUD_DIR)/external/cJSON.c
TIMER_HUD_OBJS := $(patsubst $(TIMER_HUD_DIR)/src/%.c,$(BUILD_DIR)/timer_hud/%.o,$(TIMER_HUD_SRCS))
TIMER_HUD_EXTERNAL_OBJS := $(patsubst $(TIMER_HUD_DIR)/external/%.c,$(BUILD_DIR)/timer_hud_external/%.o,$(TIMER_HUD_EXTERNAL_SRCS))

OBJ := $(OBJ) $(TIMER_HUD_OBJS) $(TIMER_HUD_EXTERNAL_OBJS)
DEP := $(OBJ:.o=.d)

.PHONY: add-disney-flag
add-disney-flag:
	$(eval CFLAGS += -DCAMERA_INTEGRATOR_DISNEY_AVAILABLE)

.PHONY: all clean run debug format video release relrun test

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

cli-tools: $(CLI_TOOLS)

$(CLI_BIN_DIR)/shape_asset_tool: build/tools/cli/shape_asset_tool.o build/import/shape_import.o build/geo/geolib/shape_asset.o build/geo/geolib/shape_library.o build/tools/ShapeLib/shape_json.o build/tools/ShapeLib/shape_flatten.o build/tools/ShapeLib/shape_core.o build/timer_hud_external/cJSON.o
	@mkdir -p $(CLI_BIN_DIR)
	$(CC) $^ -o $@ $(LDFLAGS)

$(CLI_BIN_DIR)/shape_import_tool: build/tools/cli/shape_import_tool.o build/import/shape_import.o build/tools/ShapeLib/shape_json.o build/tools/ShapeLib/shape_flatten.o build/tools/ShapeLib/shape_core.o build/timer_hud_external/cJSON.o
	@mkdir -p $(CLI_BIN_DIR)
	$(CC) $^ -o $@ $(LDFLAGS)

$(CLI_BIN_DIR)/shape_sanity_tool: build/tools/cli/shape_sanity_tool.o
	@mkdir -p $(CLI_BIN_DIR)
	$(CC) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/timer_hud/%.o: $(TIMER_HUD_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/timer_hud_external/%.o: $(TIMER_HUD_DIR)/external/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/tests/%.o: $(TEST_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

run: $(TARGET)
	./$(TARGET)

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

clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(REL_BUILD_DIR) $(REL_TARGET)

-include $(DEP)
