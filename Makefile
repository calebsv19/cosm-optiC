CC        := cc
CSTD      := -std=c11
SRC_DIR   := src
INC_DIR   := include
BUILD_DIR := build
TARGET    := Ray_anim
.DEFAULT_GOAL := all
DIST_DIR := dist
PACKAGE_APP_NAME := optiC.app
PACKAGE_APP_DIR := $(DIST_DIR)/$(PACKAGE_APP_NAME)
PACKAGE_CONTENTS_DIR := $(PACKAGE_APP_DIR)/Contents
PACKAGE_MACOS_DIR := $(PACKAGE_CONTENTS_DIR)/MacOS
PACKAGE_RESOURCES_DIR := $(PACKAGE_CONTENTS_DIR)/Resources
PACKAGE_TOOLS_DIR := $(PACKAGE_RESOURCES_DIR)/bin
PACKAGE_FRAMEWORKS_DIR := $(PACKAGE_CONTENTS_DIR)/Frameworks
PACKAGE_INFO_PLIST_SRC := tools/packaging/macos/Info.plist
PACKAGE_LAUNCHER_SRC := tools/packaging/macos/raytracing-launcher
PACKAGE_DYLIB_BUNDLER := tools/packaging/macos/bundle-dylibs.sh
PACKAGE_LOCAL_ICON_DIR := tools/packaging/macos/local_app_icon
PACKAGE_APP_ICON_NAME := AppIcon
PACKAGE_APP_ICON_FILE := $(PACKAGE_APP_ICON_NAME).icns
PACKAGE_APP_ICON_SRC ?= $(PACKAGE_LOCAL_ICON_DIR)/$(PACKAGE_APP_ICON_FILE)
PACKAGE_APP_ICONSET_SRC ?= $(PACKAGE_LOCAL_ICON_DIR)/$(PACKAGE_APP_ICON_NAME).iconset
PACKAGE_BUNDLED_ICON_PATH := $(PACKAGE_RESOURCES_DIR)/$(PACKAGE_APP_ICON_FILE)
PACKAGE_FFMPEG_SRC ?= $(shell command -v ffmpeg 2>/dev/null)
DESKTOP_APP_DIR ?= $(HOME)/Desktop/$(PACKAGE_APP_NAME)
PACKAGE_ADHOC_SIGN_IDENTITY ?= -

# RL0 release contract.
RELEASE_VERSION_FILE ?= VERSION
RELEASE_VERSION ?= $(strip $(shell cat "$(RELEASE_VERSION_FILE)" 2>/dev/null))
ifeq ($(RELEASE_VERSION),)
RELEASE_VERSION := 0.1.0
endif
RELEASE_CHANNEL ?= stable
RELEASE_PRODUCT_NAME := optiC
RELEASE_PROGRAM_KEY := ray_tracing
RELEASE_BUNDLE_ID := com.cosm.optic
RELEASE_ARTIFACT_BASENAME = $(RELEASE_PRODUCT_NAME)-$(RELEASE_VERSION)-$(RELEASE_PLATFORM)-$(RELEASE_ARCH)-$(RELEASE_CHANNEL)
RELEASE_DIR := build/release
RELEASE_APP_ZIP = $(RELEASE_DIR)/$(RELEASE_ARTIFACT_BASENAME).zip
RELEASE_MANIFEST = $(RELEASE_DIR)/$(RELEASE_ARTIFACT_BASENAME).manifest.txt
RELEASE_CODESIGN_IDENTITY ?= $(if $(strip $(APPLE_SIGN_IDENTITY)),$(APPLE_SIGN_IDENTITY),$(PACKAGE_ADHOC_SIGN_IDENTITY))
APPLE_SIGN_IDENTITY ?=
APPLE_NOTARY_PROFILE ?=
APPLE_TEAM_ID ?=
STAPLE_MAX_ATTEMPTS ?= 6
STAPLE_RETRY_DELAY_SEC ?= 15
SHARED_ROOT ?= third_party/codework_shared
SHARED_ASSETS_DIR := $(SHARED_ROOT)/assets
VK_RENDERER_DIR := $(SHARED_ROOT)/vk_renderer
CORE_BASE_DIR := $(SHARED_ROOT)/core/core_base
CORE_IO_DIR := $(SHARED_ROOT)/core/core_io
CORE_DATA_DIR := $(SHARED_ROOT)/core/core_data
CORE_PACK_DIR := $(SHARED_ROOT)/core/core_pack
CORE_TIME_DIR := $(SHARED_ROOT)/core/core_time
CORE_SCENE_DIR := $(SHARED_ROOT)/core/core_scene
CORE_SCENE_COMPILE_DIR := $(SHARED_ROOT)/core/core_scene_compile
CORE_OBJECT_DIR := $(SHARED_ROOT)/core/core_object
CORE_UNITS_DIR := $(SHARED_ROOT)/core/core_units
CORE_TRACE_DIR := $(SHARED_ROOT)/core/core_trace
CORE_SPACE_DIR := $(SHARED_ROOT)/core/core_space
CORE_PANE_DIR := $(SHARED_ROOT)/core/core_pane
CORE_THEME_DIR := $(SHARED_ROOT)/core/core_theme
CORE_FONT_DIR := $(SHARED_ROOT)/core/core_font
KIT_RENDER_DIR := $(SHARED_ROOT)/kit/kit_render
KIT_PANE_DIR := $(SHARED_ROOT)/kit/kit_pane
KIT_VIZ_DIR := $(SHARED_ROOT)/kit/kit_viz
KIT_RUNTIME_DIAG_DIR := $(SHARED_ROOT)/kit/kit_runtime_diag
UNAME_S := $(shell uname -s)
PKG_CONFIG ?= pkg-config
TARGET_CONTRACT_HELPER ?= ../bin/desktop_release_target_contract.sh
HOST_ARCH := $(shell uname -m)
TARGET_ARCH ?= $(HOST_ARCH)
RELEASE_PLATFORM ?= $(UNAME_S)
RELEASE_ARCH ?= $(TARGET_ARCH)
TARGET_HOMEBREW_PREFIX :=
TARGET_ALT_HOMEBREW_PREFIX :=
TARGET_PKG_CONFIG_LIBDIR :=
TARGET_DEP_SEARCH_ROOTS :=
ARCH_FLAGS :=

ifeq ($(UNAME_S),Darwin)
HOST_ARCH := $(strip $(shell "$(TARGET_CONTRACT_HELPER)" get host_arch))
TARGET_ARCH_INPUT := $(TARGET_ARCH)
TARGET_ARCH := $(strip $(shell TARGET_ARCH="$(TARGET_ARCH_INPUT)" "$(TARGET_CONTRACT_HELPER)" get target_arch))
RELEASE_PLATFORM := $(strip $(shell TARGET_ARCH="$(TARGET_ARCH)" "$(TARGET_CONTRACT_HELPER)" get release_platform))
RELEASE_ARCH := $(strip $(shell TARGET_ARCH="$(TARGET_ARCH)" "$(TARGET_CONTRACT_HELPER)" get release_arch))
TARGET_HOMEBREW_PREFIX := $(strip $(shell TARGET_ARCH="$(TARGET_ARCH)" "$(TARGET_CONTRACT_HELPER)" get homebrew_prefix))
TARGET_ALT_HOMEBREW_PREFIX := $(strip $(shell TARGET_ARCH="$(TARGET_ARCH)" "$(TARGET_CONTRACT_HELPER)" get alt_homebrew_prefix))
TARGET_PKG_CONFIG_LIBDIR := $(TARGET_HOMEBREW_PREFIX)/lib/pkgconfig:$(TARGET_HOMEBREW_PREFIX)/share/pkgconfig
TARGET_DEP_SEARCH_ROOTS := $(TARGET_HOMEBREW_PREFIX):$(TARGET_ALT_HOMEBREW_PREFIX)
ARCH_FLAGS := -arch $(TARGET_ARCH)
endif

SDL_CFLAGS :=
SDL_LIBS :=
SDL_PREFIX :=
SDL_EXTRA_INC :=
JSON_CFLAGS :=
JSON_LIBS :=

VULKAN_CFLAGS :=
VULKAN_LIBS :=

ifneq ($(UNAME_S),Darwin)
SDL_CONFIG := $(shell command -v sdl2-config 2>/dev/null)
SDL_CFLAGS := $(shell $(SDL_CONFIG) --cflags 2>/dev/null)
SDL_LIBS := $(shell $(SDL_CONFIG) --libs 2>/dev/null)
SDL_PREFIX := $(shell $(SDL_CONFIG) --prefix 2>/dev/null)
SDL_EXTRA_INC := $(if $(SDL_PREFIX),-I$(SDL_PREFIX)/include,)
JSON_CFLAGS := $(shell pkg-config --cflags json-c 2>/dev/null)
JSON_LIBS := $(shell pkg-config --libs json-c 2>/dev/null)
ifeq ($(strip $(JSON_LIBS)),)
JSON_LIBS := -ljson-c
endif
endif

ifeq ($(UNAME_S),Linux)
    VULKAN_CFLAGS := $(shell pkg-config --cflags vulkan 2>/dev/null)
    VULKAN_LIBS := $(shell pkg-config --libs vulkan 2>/dev/null)
    ifeq ($(strip $(VULKAN_CFLAGS)$(VULKAN_LIBS)),)
        VULKAN_CFLAGS := -I/usr/include
        VULKAN_LIBS := -lvulkan
    endif
endif

ifeq ($(UNAME_S),Darwin)
    SDL_CFLAGS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --cflags sdl2 2>/dev/null)
    SDL_LIBS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --libs sdl2 2>/dev/null)
    SDL_PREFIX := $(TARGET_HOMEBREW_PREFIX)
    SDL_EXTRA_INC := $(if $(SDL_PREFIX),-I$(SDL_PREFIX)/include,)
    JSON_CFLAGS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --cflags json-c 2>/dev/null)
    JSON_LIBS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --libs json-c 2>/dev/null)
    ifeq ($(strip $(JSON_LIBS)),)
        JSON_LIBS := -L$(TARGET_HOMEBREW_PREFIX)/lib -ljson-c
    endif
    ifeq ($(strip $(JSON_CFLAGS)),)
        JSON_CFLAGS := -I$(TARGET_HOMEBREW_PREFIX)/include
    endif
    VULKAN_CFLAGS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --cflags vulkan 2>/dev/null)
    VULKAN_LIBS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --libs vulkan 2>/dev/null)
    ifeq ($(strip $(VULKAN_CFLAGS)$(VULKAN_LIBS)),)
        VULKAN_CFLAGS := -I$(TARGET_HOMEBREW_PREFIX)/include
        VULKAN_LIBS := -L$(TARGET_HOMEBREW_PREFIX)/lib -lvulkan
    endif
    VULKAN_LIBS += -framework Metal -framework QuartzCore -framework Cocoa -framework IOKit -framework CoreVideo
    CFLAGS += -DVK_USE_PLATFORM_METAL_EXT
endif

CFLAGS  := $(CSTD) -Wall -Wextra -Wpedantic -g $(ARCH_FLAGS) $(SDL_CFLAGS) $(SDL_EXTRA_INC) $(JSON_CFLAGS) -I$(INC_DIR) -Isrc -Isrc/tools -Isrc/tools/ShapeLib -DMAIN_DRIVER
LDFLAGS := $(ARCH_FLAGS) $(SDL_LIBS) -lSDL2_ttf $(JSON_LIBS) -lm

CFLAGS_RELEASE := $(CSTD) -Wall -Wextra -Wpedantic -O3 $(ARCH_FLAGS) $(SDL_CFLAGS) $(SDL_EXTRA_INC) $(JSON_CFLAGS) -I$(INC_DIR) -Isrc -Isrc/tools -Isrc/tools/ShapeLib -DMAIN_DRIVER -DNDEBUG \
	-ffast-math -fno-math-errno -march=native
ifeq ($(UNAME_S),Darwin)
CFLAGS += -DVK_USE_PLATFORM_METAL_EXT
CFLAGS_RELEASE += -DVK_USE_PLATFORM_METAL_EXT
endif
REL_BUILD_DIR := build_release
REL_TARGET := Ray_anim_release

CLI_BIN_DIR := tools/cli/bin
CLI_TOOLS := $(CLI_BIN_DIR)/shape_asset_tool $(CLI_BIN_DIR)/shape_import_tool $(CLI_BIN_DIR)/shape_sanity_tool
RAY_TRACE_TOOL_SRC := $(SRC_DIR)/tools/cli/ray_trace_tool.c
RAY_TRACE_TOOL_BIN := ray_trace_tool
NATIVE3D_AUDIT_SRC := $(SRC_DIR)/tools/cli/native3d_render_audit.c
NATIVE3D_AUDIT_BIN := $(BUILD_DIR)/tools/cli/native3d_render_audit
NATIVE3D_AUDIT_OBJ := $(BUILD_DIR)/tools/cli/native3d_render_audit.o
NATIVE3D_AUDIT_DEPS = \
	$(BUILD_DIR)/render/materials/material_bsdf.o \
	$(BUILD_DIR)/material/material_manager.o \
	$(BUILD_DIR)/material/material.o \
	$(BUILD_DIR)/render/integrators/integrator_common.o \
	$(BUILD_DIR)/render/integrators/hybrid/integrator_tonemap.o \
	$(BUILD_DIR)/render/adapters/space_mode_adapter.o \
	$(BUILD_DIR)/render/helpers/ray_tracing_integrator_catalog.o \
	$(BUILD_DIR)/render/backend/ray_tracing_mode_backend.o \
	$(BUILD_DIR)/render/runtime_camera_3d_rays.o \
	$(BUILD_DIR)/render/runtime_direct_light_3d.o \
	$(BUILD_DIR)/render/runtime_diffuse_bounce_3d.o \
	$(BUILD_DIR)/render/runtime_emissive_direct_3d.o \
	$(BUILD_DIR)/render/runtime_light_emitter_3d.o \
	$(BUILD_DIR)/render/runtime_disney_3d.o \
	$(BUILD_DIR)/render/runtime_emission_transparency_3d.o \
	$(BUILD_DIR)/render/runtime_native_3d_feature_buffer.o \
	$(BUILD_DIR)/render/runtime_native_3d_denoise.o \
	$(BUILD_DIR)/render/runtime_native_3d_blue_noise.o \
	$(BUILD_DIR)/render/runtime_native_3d_sampling.o \
	$(BUILD_DIR)/render/materials/runtime_material_payload_3d.o \
	$(BUILD_DIR)/render/materials/runtime_material_response_3d.o \
	$(BUILD_DIR)/render/runtime_native_3d_adaptive_sampling.o \
	$(BUILD_DIR)/render/runtime_native_3d_render.o \
	$(BUILD_DIR)/render/runtime_native_3d_render_shading.o \
	$(BUILD_DIR)/render/runtime_native_3d_resolution.o \
	$(BUILD_DIR)/render/runtime_native_3d_temporal_accum.o \
	$(BUILD_DIR)/render/runtime_native_3d_tile_occupancy.o \
	$(BUILD_DIR)/render/runtime_ray_3d.o \
	$(BUILD_DIR)/render/runtime_scene_3d.o \
	$(BUILD_DIR)/render/runtime_volume_3d.o \
	$(BUILD_DIR)/render/runtime_volume_3d_sampling.o \
	$(BUILD_DIR)/render/runtime_volume_3d_integrate.o \
	$(BUILD_DIR)/render/runtime_volume_3d_scatter.o \
	$(BUILD_DIR)/render/runtime_volume_3d_debug.o \
	$(BUILD_DIR)/render/runtime_scene_3d_samples.o \
	$(BUILD_DIR)/render/runtime_scene_3d_builder.o \
	$(BUILD_DIR)/render/runtime_visibility_3d.o \
	$(BUILD_DIR)/scene/object_manager.o \
	$(BUILD_DIR)/geo/shape_adapter.o \
	$(BUILD_DIR)/geo/geolib/shape_asset.o \
	$(BUILD_DIR)/geo/geolib/shape_library.o \
	$(BUILD_DIR)/import/shape_import.o \
	$(BUILD_DIR)/import/fluid_import.o \
	$(BUILD_DIR)/import/fluid_volume_import_3d.o \
	$(BUILD_DIR)/import/fluid_volume_source_import_3d.o \
	$(BUILD_DIR)/import/fluid_volume_pack_import_3d.o \
	$(BUILD_DIR)/import/fluid_pack_import.o \
	$(BUILD_DIR)/import/scene_bundle_import.o \
	$(BUILD_DIR)/import/runtime_scene_bridge_json_utils.o \
	$(BUILD_DIR)/import/runtime_scene_bridge_authoring.o \
	$(BUILD_DIR)/import/runtime_scene_bridge.o \
	$(BUILD_DIR)/path/path_system.o \
	$(BUILD_DIR)/path/path_arc_length.o \
	$(BUILD_DIR)/camera/camera.o \
	$(BUILD_DIR)/camera/camera_path_3d.o \
	$(BUILD_DIR)/app/animation_fluid_scene.o \
	$(BUILD_DIR)/app/data_paths.o \
	$(BUILD_DIR)/config/core/config_runtime_paths.o \
	$(BUILD_DIR)/config/core/config_animation_persistence.o \
	$(BUILD_DIR)/config/io/config_file_io.o \
	$(BUILD_DIR)/config/scene/config_scene_path_io.o \
	$(BUILD_DIR)/config/core/config_manager.o \
	$(BUILD_DIR)/render/fluid/fluid_state.o \
	$(BUILD_DIR)/render/pipeline/ray_tracing2_native3d_overlay.o \
	$(BUILD_DIR)/render/pipeline/ray_tracing2_preview_present.o \
	$(BUILD_DIR)/tools/ShapeLib/shape_core.o \
	$(BUILD_DIR)/tools/ShapeLib/shape_json.o \
	$(BUILD_DIR)/tools/ShapeLib/shape_flatten.o \
	$(BUILD_DIR)/timer_hud_external/cJSON.o \
	$(CORE_BASE_OBJS) \
	$(CORE_IO_OBJS) \
	$(CORE_DATA_OBJS) \
	$(CORE_PACK_OBJS) \
	$(CORE_TIME_OBJS) \
	$(CORE_SCENE_OBJS) \
	$(CORE_SCENE_COMPILE_OBJS) \
	$(CORE_OBJECT_OBJS) \
	$(CORE_UNITS_OBJS) \
	$(CORE_SPACE_OBJS) \
	$(CORE_THEME_OBJS) \
	$(CORE_FONT_OBJS) \
	$(patsubst $(VK_RENDERER_DIR)/src/%.c,$(BUILD_DIR)/vk_renderer/%.o,$(VK_RENDERER_SRCS)) \
	$(KIT_VIZ_OBJS) \
	$(KIT_RUNTIME_DIAG_OBJS)

VIDEO_FRAMES_DIR ?= data/runtime/frames/default
VIDEO_OUTPUT ?= data/runtime/videos/output.mp4
VIDEO_FPS ?= 30

TEST_DIR := tests
TEST_BIN := $(BUILD_DIR)/tests/test_runner
TEST_SRC := $(TEST_DIR)/test_runner.c
TEST_OBJ := $(BUILD_DIR)/tests/test_runner.o $(BUILD_DIR)/tests/test_runner_registry.o \
	$(BUILD_DIR)/tests/test_support.o $(BUILD_DIR)/tests/test_config_animation.o \
	$(BUILD_DIR)/tests/test_ui_menu_contracts.o \
	$(BUILD_DIR)/tests/test_runtime_scene_bridge_core.o \
	$(BUILD_DIR)/tests/test_runtime_scene_bridge_writeback.o \
	$(BUILD_DIR)/tests/test_runtime_scene_3d_geometry.o \
	$(BUILD_DIR)/tests/test_runtime_volume_3d.o \
	$(BUILD_DIR)/tests/test_runtime_lighting_materials.o \
	$(BUILD_DIR)/tests/test_runtime_lighting_materials_payload_suite.o \
	$(BUILD_DIR)/tests/test_runtime_lighting_materials_direct_light_suite.o \
	$(BUILD_DIR)/tests/test_runtime_lighting_materials_transport_suite.o \
	$(BUILD_DIR)/tests/test_runtime_diffuse_temporal.o \
	$(BUILD_DIR)/tests/test_runtime_emission_transparency.o \
	$(BUILD_DIR)/tests/test_runtime_native_3d_denoise.o \
	$(BUILD_DIR)/tests/test_runtime_native_3d_render.o \
	$(BUILD_DIR)/tests/test_runtime_native_3d_render_live_suite.o \
	$(BUILD_DIR)/tests/test_runtime_native_3d_render_prepared_suite.o \
	$(BUILD_DIR)/tests/test_runtime_render_metrics_export.o \
	$(BUILD_DIR)/tests/test_runtime_preview_editor.o \
	$(BUILD_DIR)/tests/test_runtime_scene_editor.o \
	$(BUILD_DIR)/tests/test_runtime_path_policy.o \
	$(BUILD_DIR)/tests/test_runtime_mode_backend_policy.o \
	$(BUILD_DIR)/tests/test_fluid_volume_import_3d.o \
	$(BUILD_DIR)/tests/test_fluid_volume_pack_import_3d.o \
	$(BUILD_DIR)/tests/test_stubs.o \
	$(BUILD_DIR)/tests/fluid_pack_import_test.o \
	$(BUILD_DIR)/tests/kit_viz_fluid_overlay_adapter_test.o \
	$(BUILD_DIR)/tests/render_metrics_dataset_test.o
TIMER_HUD_DIR := $(SHARED_ROOT)/timer_hud
TIMER_HUD_INCLUDE := -I$(TIMER_HUD_DIR)/include -I$(TIMER_HUD_DIR)/external

CFLAGS += $(TIMER_HUD_INCLUDE) -I$(VK_RENDERER_DIR)/include $(VULKAN_CFLAGS) \
	-I$(CORE_BASE_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_DATA_DIR)/include -I$(CORE_PACK_DIR)/include -I$(CORE_TIME_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_SCENE_COMPILE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include -I$(CORE_TRACE_DIR)/include -I$(CORE_SPACE_DIR)/include -I$(CORE_PANE_DIR)/include -I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include -I$(KIT_RENDER_DIR)/include -I$(KIT_PANE_DIR)/include -I$(KIT_VIZ_DIR)/include -I$(KIT_RUNTIME_DIAG_DIR)/include \
	-DKIT_RENDER_ENABLE_VK_BACKEND=0 \
	-DUSE_VULKAN=1 -DVK_RENDERER_SHADER_ROOT=\"$(abspath $(VK_RENDERER_DIR))\" \
	-include $(VK_RENDERER_DIR)/include/vk_renderer_sdl.h
LDFLAGS += $(VULKAN_LIBS)
CFLAGS_RELEASE += $(TIMER_HUD_INCLUDE) -I$(VK_RENDERER_DIR)/include $(VULKAN_CFLAGS) \
	-I$(CORE_BASE_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_DATA_DIR)/include -I$(CORE_PACK_DIR)/include -I$(CORE_TIME_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_SCENE_COMPILE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include -I$(CORE_TRACE_DIR)/include -I$(CORE_SPACE_DIR)/include -I$(CORE_PANE_DIR)/include -I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include -I$(KIT_RENDER_DIR)/include -I$(KIT_PANE_DIR)/include -I$(KIT_VIZ_DIR)/include -I$(KIT_RUNTIME_DIAG_DIR)/include \
	-DKIT_RENDER_ENABLE_VK_BACKEND=0 \
	-DUSE_VULKAN=1 -DVK_RENDERER_SHADER_ROOT=\"$(abspath $(VK_RENDERER_DIR))\" \
	-include $(VK_RENDERER_DIR)/include/vk_renderer_sdl.h
ifeq ($(UNAME_S),Darwin)
CORE_TIME_TEST_DEPS := $(BUILD_DIR)/core_time/core_time.o $(BUILD_DIR)/core_time/core_time_mac.o
else
CORE_TIME_TEST_DEPS := $(BUILD_DIR)/core_time/core_time.o $(BUILD_DIR)/core_time/core_time_posix.o
endif

TEST_DEPS := \
	$(BUILD_DIR)/render/materials/material_bsdf.o \
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
	$(BUILD_DIR)/render/accel/irradiance_cache.o \
	$(BUILD_DIR)/render/adapters/space_mode_adapter.o \
	$(BUILD_DIR)/render/helpers/ray_tracing_integrator_catalog.o \
	$(BUILD_DIR)/render/backend/ray_tracing_mode_backend.o \
		$(BUILD_DIR)/render/runtime_camera_3d_rays.o \
		$(BUILD_DIR)/render/runtime_direct_light_3d.o \
		$(BUILD_DIR)/render/runtime_diffuse_bounce_3d.o \
		$(BUILD_DIR)/render/runtime_emissive_direct_3d.o \
		$(BUILD_DIR)/render/runtime_light_emitter_3d.o \
		$(BUILD_DIR)/render/runtime_disney_3d.o \
		$(BUILD_DIR)/render/runtime_emission_transparency_3d.o \
	$(BUILD_DIR)/render/runtime_native_3d_feature_buffer.o \
	$(BUILD_DIR)/render/runtime_native_3d_denoise.o \
	$(BUILD_DIR)/render/runtime_native_3d_blue_noise.o \
	$(BUILD_DIR)/render/runtime_native_3d_sampling.o \
	$(BUILD_DIR)/render/materials/runtime_material_payload_3d.o \
	$(BUILD_DIR)/render/materials/runtime_material_response_3d.o \
	$(BUILD_DIR)/render/runtime_native_3d_adaptive_sampling.o \
	$(BUILD_DIR)/render/runtime_native_3d_render.o \
	$(BUILD_DIR)/render/runtime_native_3d_render_shading.o \
	$(BUILD_DIR)/render/runtime_native_3d_resolution.o \
	$(BUILD_DIR)/render/runtime_native_3d_temporal_accum.o \
	$(BUILD_DIR)/render/runtime_native_3d_tile_occupancy.o \
	$(BUILD_DIR)/render/runtime_ray_3d.o \
	$(BUILD_DIR)/render/runtime_scene_3d.o \
	$(BUILD_DIR)/render/runtime_volume_3d.o \
	$(BUILD_DIR)/render/runtime_volume_3d_sampling.o \
	$(BUILD_DIR)/render/runtime_volume_3d_integrate.o \
	$(BUILD_DIR)/render/runtime_volume_3d_scatter.o \
	$(BUILD_DIR)/render/runtime_volume_3d_debug.o \
	$(BUILD_DIR)/render/runtime_scene_3d_samples.o \
	$(BUILD_DIR)/render/runtime_scene_3d_builder.o \
	$(BUILD_DIR)/render/runtime_visibility_3d.o \
	$(BUILD_DIR)/editor/editor_mode_router.o \
	$(BUILD_DIR)/editor/object_editor_object_ops.o \
	$(BUILD_DIR)/editor/scene_editor_control_surface.o \
	$(BUILD_DIR)/editor/scene_editor_tool_state.o \
	$(BUILD_DIR)/editor/scene_editor_runtime_scene_persistence.o \
	$(BUILD_DIR)/path/path_system.o \
	$(BUILD_DIR)/path/path_arc_length.o \
		$(BUILD_DIR)/render/accel/uniform_grid.o \
		$(BUILD_DIR)/render/accel/surface_mesh.o \
	$(BUILD_DIR)/render/helpers/render_helper.o \
	$(BUILD_DIR)/render/font_bridge.o \
	$(BUILD_DIR)/render/text_font_cache.o \
	$(BUILD_DIR)/render/text_draw.o \
	$(BUILD_DIR)/render/text_upload_policy.o \
		$(BUILD_DIR)/render/pipeline/ray_tracing2_native3d_overlay.o \
		$(BUILD_DIR)/render/pipeline/ray_tracing2_preview.o \
		$(BUILD_DIR)/render/pipeline/ray_tracing2_preview_present.o \
		$(BUILD_DIR)/render/pipeline/ray_tracing2.o \
	$(BUILD_DIR)/render/fluid/fluid_state.o \
	$(BUILD_DIR)/render/fluid/fluid_overlay.o \
	$(BUILD_DIR)/scene/object_manager.o \
	$(BUILD_DIR)/geo/shape_adapter.o \
	$(BUILD_DIR)/geo/geolib/shape_asset.o \
	$(BUILD_DIR)/geo/geolib/shape_library.o \
	$(BUILD_DIR)/import/shape_import.o \
	$(BUILD_DIR)/timer_hud_external/cJSON.o \
	$(BUILD_DIR)/camera/camera.o \
	$(BUILD_DIR)/camera/camera_path_3d.o \
	$(BUILD_DIR)/app/preview_mode_route.o \
	$(BUILD_DIR)/app/preview_playback.o \
	$(BUILD_DIR)/app/preview_camera_sample.o \
	$(BUILD_DIR)/app/preview_camera_projector.o \
	$(BUILD_DIR)/app/preview_retained_scene_renderer.o \
	$(BUILD_DIR)/app/animation_output.o \
	$(BUILD_DIR)/app/render_export_batch.o \
	$(BUILD_DIR)/app/animation_fluid_scene.o \
	$(BUILD_DIR)/app/data_paths.o \
	$(BUILD_DIR)/config/core/config_runtime_paths.o \
	$(BUILD_DIR)/config/core/config_animation_persistence.o \
		$(BUILD_DIR)/config/io/config_file_io.o \
	$(BUILD_DIR)/config/scene/config_scene_path_io.o \
	$(BUILD_DIR)/config/core/config_manager.o \
	$(BUILD_DIR)/ui/menu/scene_source_ui_labels.o \
	$(BUILD_DIR)/ui/menu/volume_source_ui_labels.o \
	$(BUILD_DIR)/ui/menu/shared_theme_font_adapter.o \
	$(BUILD_DIR)/tools/make_video.o \
	$(BUILD_DIR)/tools/ShapeLib/shape_core.o \
	$(BUILD_DIR)/tools/ShapeLib/shape_json.o \
	$(BUILD_DIR)/tools/ShapeLib/shape_flatten.o \
	$(BUILD_DIR)/import/fluid_import.o \
	$(BUILD_DIR)/import/fluid_volume_import_3d.o \
	$(BUILD_DIR)/import/fluid_volume_source_import_3d.o \
	$(BUILD_DIR)/import/fluid_volume_pack_import_3d.o \
	$(BUILD_DIR)/import/fluid_pack_import.o \
	$(BUILD_DIR)/import/scene_bundle_import.o \
	$(BUILD_DIR)/import/runtime_scene_bridge_json_utils.o \
	$(BUILD_DIR)/import/runtime_scene_bridge_authoring.o \
	$(BUILD_DIR)/import/runtime_scene_volume_defaults.o \
	$(BUILD_DIR)/import/runtime_scene_bridge.o \
	$(BUILD_DIR)/ui/menu/sdl_menu_render.o \
	$(BUILD_DIR)/ui/menu/sdl_menu_render_manifest.o \
	$(BUILD_DIR)/ui/menu/sdl_menu_render_volume.o \
	$(BUILD_DIR)/ui/menu/sdl_menu_render_sliders.o \
	$(BUILD_DIR)/ui/menu/menu_layout.o \
	$(BUILD_DIR)/ui/menu/menu_panel_chrome.o \
	$(BUILD_DIR)/ui/menu/menu_batch_panel.o \
	$(BUILD_DIR)/ui/menu/scene_source_catalog.o \
	$(BUILD_DIR)/ui/menu/volume_source_catalog.o \
	$(BUILD_DIR)/ui/menu/sdl_menu_state.o \
	$(BUILD_DIR)/render/adapters/kit_viz_fluid_overlay_adapter.o \
	$(BUILD_DIR)/core_base/core_base.o \
	$(BUILD_DIR)/core_io/core_io.o \
	$(BUILD_DIR)/core_data/core_data.o \
	$(BUILD_DIR)/core_pack/core_pack.o \
	$(CORE_TIME_TEST_DEPS) \
	$(BUILD_DIR)/core_scene/core_scene.o \
	$(BUILD_DIR)/core_scene_compile/core_scene_compile.o \
	$(BUILD_DIR)/core_object/core_object.o \
	$(BUILD_DIR)/core_units/core_units.o \
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
CORE_SCENE_COMPILE_SRCS := $(CORE_SCENE_COMPILE_DIR)/src/core_scene_compile.c
CORE_OBJECT_SRCS := $(CORE_OBJECT_DIR)/src/core_object.c
CORE_UNITS_SRCS := $(CORE_UNITS_DIR)/src/core_units.c
CORE_SPACE_SRCS := $(CORE_SPACE_DIR)/src/core_space.c
CORE_PANE_SRCS := $(CORE_PANE_DIR)/src/core_pane.c
CORE_THEME_SRCS := $(CORE_THEME_DIR)/src/core_theme.c
CORE_FONT_SRCS := $(CORE_FONT_DIR)/src/core_font.c
KIT_RENDER_SRCS := \
	$(KIT_RENDER_DIR)/src/kit_render.c \
	$(KIT_RENDER_DIR)/src/kit_render_external_text.c \
	$(KIT_RENDER_DIR)/src/kit_render_backend_null.c \
	$(KIT_RENDER_DIR)/src/kit_render_backend_vk.c
KIT_PANE_SRCS := $(KIT_PANE_DIR)/src/kit_pane.c
KIT_VIZ_SRCS := $(KIT_VIZ_DIR)/src/kit_viz.c
KIT_RUNTIME_DIAG_SRCS := $(KIT_RUNTIME_DIAG_DIR)/src/kit_runtime_diag.c
CORE_BASE_OBJS := $(patsubst $(CORE_BASE_DIR)/src/%.c,$(BUILD_DIR)/core_base/%.o,$(CORE_BASE_SRCS))
CORE_IO_OBJS := $(patsubst $(CORE_IO_DIR)/src/%.c,$(BUILD_DIR)/core_io/%.o,$(CORE_IO_SRCS))
CORE_DATA_OBJS := $(patsubst $(CORE_DATA_DIR)/src/%.c,$(BUILD_DIR)/core_data/%.o,$(CORE_DATA_SRCS))
CORE_PACK_OBJS := $(patsubst $(CORE_PACK_DIR)/src/%.c,$(BUILD_DIR)/core_pack/%.o,$(CORE_PACK_SRCS))
CORE_TIME_OBJS := $(patsubst $(CORE_TIME_DIR)/src/%.c,$(BUILD_DIR)/core_time/%.o,$(CORE_TIME_SRCS))
CORE_SCENE_OBJS := $(patsubst $(CORE_SCENE_DIR)/src/%.c,$(BUILD_DIR)/core_scene/%.o,$(CORE_SCENE_SRCS))
CORE_SCENE_COMPILE_OBJS := $(patsubst $(CORE_SCENE_COMPILE_DIR)/src/%.c,$(BUILD_DIR)/core_scene_compile/%.o,$(CORE_SCENE_COMPILE_SRCS))
CORE_OBJECT_OBJS := $(patsubst $(CORE_OBJECT_DIR)/src/%.c,$(BUILD_DIR)/core_object/%.o,$(CORE_OBJECT_SRCS))
CORE_UNITS_OBJS := $(patsubst $(CORE_UNITS_DIR)/src/%.c,$(BUILD_DIR)/core_units/%.o,$(CORE_UNITS_SRCS))
CORE_SPACE_OBJS := $(patsubst $(CORE_SPACE_DIR)/src/%.c,$(BUILD_DIR)/core_space/%.o,$(CORE_SPACE_SRCS))
CORE_PANE_OBJS := $(patsubst $(CORE_PANE_DIR)/src/%.c,$(BUILD_DIR)/core_pane/%.o,$(CORE_PANE_SRCS))
CORE_THEME_OBJS := $(patsubst $(CORE_THEME_DIR)/src/%.c,$(BUILD_DIR)/core_theme/%.o,$(CORE_THEME_SRCS))
CORE_FONT_OBJS := $(patsubst $(CORE_FONT_DIR)/src/%.c,$(BUILD_DIR)/core_font/%.o,$(CORE_FONT_SRCS))
KIT_RENDER_OBJS := $(patsubst $(KIT_RENDER_DIR)/src/%.c,$(BUILD_DIR)/kit_render/%.o,$(KIT_RENDER_SRCS))
KIT_PANE_OBJS := $(patsubst $(KIT_PANE_DIR)/src/%.c,$(BUILD_DIR)/kit_pane/%.o,$(KIT_PANE_SRCS))
KIT_VIZ_OBJS := $(patsubst $(KIT_VIZ_DIR)/src/%.c,$(BUILD_DIR)/kit_viz/%.o,$(KIT_VIZ_SRCS))
KIT_RUNTIME_DIAG_OBJS := $(patsubst $(KIT_RUNTIME_DIAG_DIR)/src/%.c,$(BUILD_DIR)/kit_runtime_diag/%.o,$(KIT_RUNTIME_DIAG_SRCS))

TEST_DEPS += $(KIT_RENDER_OBJS)

OBJ := $(OBJ) $(TIMER_HUD_OBJS) $(TIMER_HUD_EXTERNAL_OBJS) \
	$(patsubst $(VK_RENDERER_DIR)/src/%.c,$(BUILD_DIR)/vk_renderer/%.o,$(VK_RENDERER_SRCS)) \
	$(CORE_BASE_OBJS) $(CORE_IO_OBJS) $(CORE_DATA_OBJS) $(CORE_PACK_OBJS) $(CORE_TIME_OBJS) $(CORE_SCENE_OBJS) $(CORE_SCENE_COMPILE_OBJS) $(CORE_OBJECT_OBJS) $(CORE_UNITS_OBJS) $(CORE_SPACE_OBJS) $(CORE_PANE_OBJS) $(CORE_THEME_OBJS) $(CORE_FONT_OBJS) $(KIT_RENDER_OBJS) $(KIT_PANE_OBJS) $(KIT_VIZ_OBJS) $(KIT_RUNTIME_DIAG_OBJS)
DEP := $(OBJ:.o=.d)

.PHONY: add-disney-flag
add-disney-flag:
	$(eval CFLAGS += -DCAMERA_INTEGRATOR_DISNEY_AVAILABLE)

STABLE_TEST_TARGETS := \
	test \
	test-scene-editor-pane-host-contract \
	test-manifest-to-trace-export \
	test-fluid-pack-contract-parity \
	test-trio-scene-contract-diff \
	test-shared-theme-font-adapter

LEGACY_TEST_TARGETS :=

.PHONY: all clean run run-ide-theme run-daw-theme run-headless-smoke visual-harness package-desktop package-desktop-smoke package-desktop-self-test package-desktop-copy-desktop package-desktop-sync package-desktop-open package-desktop-remove package-desktop-refresh release-contract release-clean release-build release-bundle-audit release-sign release-verify release-verify-signed release-notarize release-staple release-verify-notarized release-artifact release-distribute release-desktop-refresh debug format video release relrun test test-stable test-legacy test-shared-theme-font-adapter test-scene-editor-pane-host-contract test-manifest-to-trace-export test-fluid-pack-contract-parity test-trio-scene-contract-diff native3d-render-audit

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

$(NATIVE3D_AUDIT_BIN): $(NATIVE3D_AUDIT_OBJ) $(NATIVE3D_AUDIT_DEPS)
	@mkdir -p $(dir $@)
	$(CC) $(NATIVE3D_AUDIT_OBJ) $(NATIVE3D_AUDIT_DEPS) -o $@ $(LDFLAGS)

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

$(BUILD_DIR)/core_scene_compile/%.o: $(CORE_SCENE_COMPILE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_object/%.o: $(CORE_OBJECT_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_units/%.o: $(CORE_UNITS_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_space/%.o: $(CORE_SPACE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_pane/%.o: $(CORE_PANE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_theme/%.o: $(CORE_THEME_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_font/%.o: $(CORE_FONT_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/kit_render/%.o: $(KIT_RENDER_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/kit_pane/%.o: $(KIT_PANE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/kit_viz/%.o: $(KIT_VIZ_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/kit_runtime_diag/%.o: $(KIT_RUNTIME_DIAG_DIR)/src/%.c
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

run-headless-smoke: all test-stable
	@echo "ray_tracing headless smoke passed (non-interactive)"

visual-harness: $(TARGET)
	@echo "visual harness binary ready: $(TARGET)"

package-desktop: all
	@echo "Preparing desktop package..."
	@rm -rf "$(PACKAGE_APP_DIR)"
	@mkdir -p "$(PACKAGE_MACOS_DIR)" "$(PACKAGE_RESOURCES_DIR)" "$(PACKAGE_FRAMEWORKS_DIR)" "$(PACKAGE_TOOLS_DIR)"
	@cp "$(PACKAGE_INFO_PLIST_SRC)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@cp "$(TARGET)" "$(PACKAGE_MACOS_DIR)/raytracing-bin"
	@cp "$(PACKAGE_LAUNCHER_SRC)" "$(PACKAGE_MACOS_DIR)/raytracing-launcher"
	@PACKAGE_DEP_SEARCH_ROOTS="$(TARGET_DEP_SEARCH_ROOTS)" "$(PACKAGE_DYLIB_BUNDLER)" "$(PACKAGE_MACOS_DIR)/raytracing-bin" "$(PACKAGE_FRAMEWORKS_DIR)"
	@chmod +x "$(PACKAGE_MACOS_DIR)/raytracing-bin" "$(PACKAGE_MACOS_DIR)/raytracing-launcher"
	@if [ -f "$(PACKAGE_APP_ICON_SRC)" ]; then \
		cp "$(PACKAGE_APP_ICON_SRC)" "$(PACKAGE_BUNDLED_ICON_PATH)"; \
		echo "Bundled app icon from $(PACKAGE_APP_ICON_SRC)"; \
	elif [ -d "$(PACKAGE_APP_ICONSET_SRC)" ]; then \
		/usr/bin/iconutil -c icns -o "$(PACKAGE_BUNDLED_ICON_PATH)" "$(PACKAGE_APP_ICONSET_SRC)" || exit 1; \
		echo "Bundled app icon from $(PACKAGE_APP_ICONSET_SRC)"; \
	else \
		echo "warning: no app icon source found at $(PACKAGE_APP_ICON_SRC) or $(PACKAGE_APP_ICONSET_SRC)"; \
	fi
	@if [ -n "$(PACKAGE_FFMPEG_SRC)" ] && [ -x "$(PACKAGE_FFMPEG_SRC)" ] && \
		/usr/bin/lipo -archs "$(PACKAGE_FFMPEG_SRC)" 2>/dev/null | /usr/bin/grep -Eq '(^| )$(TARGET_ARCH)($| )'; then \
		cp "$(PACKAGE_FFMPEG_SRC)" "$(PACKAGE_TOOLS_DIR)/ffmpeg"; \
		chmod +x "$(PACKAGE_TOOLS_DIR)/ffmpeg"; \
	else \
		echo "Skipping bundled ffmpeg for TARGET_ARCH=$(TARGET_ARCH)"; \
	fi
	@cp -R config "$(PACKAGE_RESOURCES_DIR)/"
	@mkdir -p "$(PACKAGE_RESOURCES_DIR)/shared/assets/fonts"
	@cp -R "$(SHARED_ASSETS_DIR)/fonts/." "$(PACKAGE_RESOURCES_DIR)/shared/assets/fonts/"
	@mkdir -p "$(PACKAGE_RESOURCES_DIR)/data/runtime" "$(PACKAGE_RESOURCES_DIR)/data/runtime/frames" "$(PACKAGE_RESOURCES_DIR)/data/runtime/videos" "$(PACKAGE_RESOURCES_DIR)/data/snapshots"
	@mkdir -p "$(PACKAGE_RESOURCES_DIR)/vk_renderer" "$(PACKAGE_RESOURCES_DIR)/shaders"
	@cp -R "$(VK_RENDERER_DIR)/shaders" "$(PACKAGE_RESOURCES_DIR)/vk_renderer/"
	@cp -R "$(VK_RENDERER_DIR)/shaders/." "$(PACKAGE_RESOURCES_DIR)/shaders/"
	@for dylib in $$(/usr/bin/find "$(PACKAGE_FRAMEWORKS_DIR)" -type f -name '*.dylib' 2>/dev/null); do \
		/usr/bin/codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$$dylib"; \
	done
	@/usr/bin/codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/raytracing-bin"
	@/usr/bin/codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/raytracing-launcher"
	@if [ -x "$(PACKAGE_TOOLS_DIR)/ffmpeg" ]; then \
		/usr/bin/codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_TOOLS_DIR)/ffmpeg"; \
	fi
	@/usr/bin/codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_APP_DIR)"
	@echo "Desktop package ready: $(PACKAGE_APP_DIR)"

package-desktop-smoke: package-desktop
	@test -x "$(PACKAGE_MACOS_DIR)/raytracing-launcher" || (echo "Missing launcher"; exit 1)
	@test -x "$(PACKAGE_MACOS_DIR)/raytracing-bin" || (echo "Missing app binary"; exit 1)
	@test -f "$(PACKAGE_FRAMEWORKS_DIR)/libvulkan.1.dylib" || (echo "Missing bundled libvulkan.1.dylib"; exit 1)
	@test -f "$(PACKAGE_FRAMEWORKS_DIR)/libMoltenVK.dylib" || (echo "Missing bundled libMoltenVK.dylib"; exit 1)
	@test -f "$(PACKAGE_CONTENTS_DIR)/Info.plist" || (echo "Missing Info.plist"; exit 1)
	@if [ -f "$(PACKAGE_APP_ICON_SRC)" ] || [ -d "$(PACKAGE_APP_ICONSET_SRC)" ]; then \
		test -f "$(PACKAGE_BUNDLED_ICON_PATH)" || (echo "Missing bundled AppIcon.icns"; exit 1); \
	fi
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/animation_config.json" || (echo "Missing config/animation_config.json"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/scene_config.json" || (echo "Missing config/scene_config.json"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/default.ttf" || (echo "Missing config/default.ttf"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/shared/assets/fonts/Montserrat-Regular.ttf" || (echo "Missing shared packaged font"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/objects/Hexagon.asset.json" || (echo "Missing bundled shape assets"; exit 1)
	@test -d "$(PACKAGE_RESOURCES_DIR)/data/runtime" || (echo "Missing runtime dir"; exit 1)
	@test -d "$(PACKAGE_RESOURCES_DIR)/data/runtime/frames" || (echo "Missing runtime frames dir"; exit 1)
	@test -d "$(PACKAGE_RESOURCES_DIR)/data/runtime/videos" || (echo "Missing runtime videos dir"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/vk_renderer/shaders/textured.vert.spv" || (echo "Missing bundled vk_renderer shader"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/shaders/textured.vert.spv" || (echo "Missing bundled runtime shader"; exit 1)
	@echo "package-desktop-smoke passed."

package-desktop-self-test: package-desktop-smoke
	@"$(PACKAGE_MACOS_DIR)/raytracing-launcher" --self-test || (echo "package-desktop self-test failed."; exit 1)
	@echo "package-desktop-self-test passed."

package-desktop-copy-desktop: package-desktop
	@mkdir -p "$(dir $(DESKTOP_APP_DIR))"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@/usr/bin/ditto "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@echo "Copied $(PACKAGE_APP_NAME) to $(DESKTOP_APP_DIR)"

package-desktop-sync: package-desktop-copy-desktop
	@echo "Desktop package synchronized: $(DESKTOP_APP_DIR)"

package-desktop-open: package-desktop
	@open "$(PACKAGE_APP_DIR)"

package-desktop-remove:
	@rm -rf "$(PACKAGE_APP_DIR)"
	@echo "Removed desktop package: $(PACKAGE_APP_DIR)"

package-desktop-refresh: package-desktop
	@mkdir -p "$(dir $(DESKTOP_APP_DIR))"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@/usr/bin/ditto "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@echo "Refreshed $(PACKAGE_APP_NAME) at $(DESKTOP_APP_DIR)"

release-contract:
	@mkdir -p "$(RELEASE_DIR)"
	@echo "release-contract:"
	@echo "  product: $(RELEASE_PRODUCT_NAME)"
	@echo "  program: $(RELEASE_PROGRAM_KEY)"
	@echo "  version: $(RELEASE_VERSION)"
	@echo "  channel: $(RELEASE_CHANNEL)"
	@echo "  bundle_id: $(RELEASE_BUNDLE_ID)"
	@echo "  app_name: $(PACKAGE_APP_NAME)"
	@echo "  artifact_base: $(RELEASE_ARTIFACT_BASENAME)"
	@echo "  release_zip: $(RELEASE_APP_ZIP)"
	@echo "  signing_identity: $(RELEASE_CODESIGN_IDENTITY)"
	@echo "  notary_profile_set: $$( [ -n \"$(APPLE_NOTARY_PROFILE)\" ] && echo yes || echo no )"
	@echo "  team_id_set: $$( [ -n \"$(APPLE_TEAM_ID)\" ] && echo yes || echo no )"

release-clean:
	@rm -rf "$(RELEASE_DIR)"
	@echo "Removed release dir: $(RELEASE_DIR)"

release-build: all
	@echo "Release build complete: $(TARGET)"

release-bundle-audit: package-desktop-self-test
	@mkdir -p "$(RELEASE_DIR)"
	@/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$(PACKAGE_CONTENTS_DIR)/Info.plist" > "$(RELEASE_DIR)/bundle_id.txt"
	@test "$$(cat "$(RELEASE_DIR)/bundle_id.txt")" = "$(RELEASE_BUNDLE_ID)" || (echo "bundle id mismatch: expected $(RELEASE_BUNDLE_ID), got $$(cat "$(RELEASE_DIR)/bundle_id.txt")"; exit 1)
	@env -i HOME="$(HOME)" PATH="$(PATH)" "$(PACKAGE_MACOS_DIR)/raytracing-launcher" --print-config > "$(RELEASE_DIR)/print_config.txt"
	@runtime_dir="$$(/usr/bin/grep '^RAY_TRACING_RUNTIME_DIR=' "$(RELEASE_DIR)/print_config.txt" | /usr/bin/cut -d= -f2-)"; \
	if [ -z "$$runtime_dir" ]; then echo "runtime dir missing from print-config"; exit 1; fi; \
	case "$$runtime_dir" in *"/Contents/Resources"*) echo "runtime dir incorrectly points into app bundle: $$runtime_dir"; exit 1;; esac; \
	case "$$runtime_dir" in /tmp/*|/var/*|"$(HOME)"/*) ;; *) echo "runtime dir is not user-writable rooted: $$runtime_dir"; exit 1;; esac
	@dataset_path="$$(/usr/bin/grep '^RAY_TRACING_RENDER_METRICS_DATASET_PATH=' "$(RELEASE_DIR)/print_config.txt" | /usr/bin/cut -d= -f2-)"; \
	if [ -z "$$dataset_path" ]; then echo "render metrics dataset path missing from print-config"; exit 1; fi; \
	case "$$dataset_path" in *"/Contents/Resources"*) echo "render metrics dataset path incorrectly points into app bundle: $$dataset_path"; exit 1;; esac; \
	case "$$dataset_path" in /tmp/*|/var/*|"$(HOME)"/*) ;; *) echo "render metrics dataset path is not user-writable rooted: $$dataset_path"; exit 1;; esac
	@/usr/bin/grep -q '^VK_ICD_FILENAMES=' "$(RELEASE_DIR)/print_config.txt" || (echo "missing VK_ICD_FILENAMES in print-config"; exit 1)
	@/usr/bin/grep -q '^VK_DRIVER_FILES=' "$(RELEASE_DIR)/print_config.txt" || (echo "missing VK_DRIVER_FILES in print-config"; exit 1)
	@otool -L "$(PACKAGE_MACOS_DIR)/raytracing-bin" > "$(RELEASE_DIR)/otool_raytracing_bin.txt"
	@if /usr/bin/grep -Eq '/opt/homebrew|/usr/local/Cellar|/Users/.*/CodeWork' "$(RELEASE_DIR)/otool_raytracing_bin.txt"; then \
		echo "non-portable dylib dependency detected in $(PACKAGE_MACOS_DIR)/raytracing-bin"; \
		cat "$(RELEASE_DIR)/otool_raytracing_bin.txt"; \
		exit 1; \
	fi
	@for file in $$(/usr/bin/find "$(PACKAGE_FRAMEWORKS_DIR)" -type f -name '*.dylib' 2>/dev/null); do \
		base="$$(/usr/bin/basename "$$file")"; \
		otool -L "$$file" > "$(RELEASE_DIR)/otool_$$base.txt" || exit 1; \
		if /usr/bin/grep -Eq '/opt/homebrew|/usr/local/Cellar|/Users/.*/CodeWork' "$(RELEASE_DIR)/otool_$$base.txt"; then \
			echo "non-portable dylib dependency detected in $$file"; \
			cat "$(RELEASE_DIR)/otool_$$base.txt"; \
			exit 1; \
		fi; \
	done
	@echo "release-bundle-audit passed."

release-sign: release-bundle-audit
	@echo "Signing with identity: $(RELEASE_CODESIGN_IDENTITY)"
	@if [ "$(RELEASE_CODESIGN_IDENTITY)" = "-" ]; then \
		for dylib in $$(/usr/bin/find "$(PACKAGE_FRAMEWORKS_DIR)" -type f -name '*.dylib' 2>/dev/null); do \
			codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp=none "$$dylib"; \
		done; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/raytracing-bin"; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/raytracing-launcher"; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp=none "$(PACKAGE_APP_DIR)"; \
	else \
		for dylib in $$(/usr/bin/find "$(PACKAGE_FRAMEWORKS_DIR)" -type f -name '*.dylib' 2>/dev/null); do \
			codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp "$$dylib"; \
		done; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp --options runtime "$(PACKAGE_MACOS_DIR)/raytracing-bin"; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp --options runtime "$(PACKAGE_MACOS_DIR)/raytracing-launcher"; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp --options runtime "$(PACKAGE_APP_DIR)"; \
	fi
	@echo "release-sign complete."

release-verify: release-sign
	@codesign --verify --deep --strict "$(PACKAGE_APP_DIR)"
	@if [ "$(RELEASE_CODESIGN_IDENTITY)" = "-" ]; then \
		echo "release-verify note: ad-hoc identity in use; skipping spctl Gatekeeper assessment"; \
	else \
		spctl_output="$$(spctl --assess --type execute --verbose=2 "$(PACKAGE_APP_DIR)" 2>&1)"; \
		spctl_status=$$?; \
		if [ $$spctl_status -ne 0 ]; then \
			if printf '%s\n' "$$spctl_output" | /usr/bin/grep -qi "internal error in Code Signing subsystem"; then \
				echo "release-verify note: spctl internal subsystem error on this host; codesign verification remains authoritative"; \
			elif printf '%s\n' "$$spctl_output" | /usr/bin/grep -qi "Unnotarized Developer ID"; then \
				echo "release-verify note: app is Developer ID signed but not notarized yet"; \
			else \
				printf '%s\n' "$$spctl_output"; \
				exit $$spctl_status; \
			fi; \
		else \
			printf '%s\n' "$$spctl_output"; \
		fi; \
	fi
	@echo "release-verify passed."

release-verify-signed: release-sign release-verify
	@echo "release-verify-signed passed."

release-notarize: release-sign
	@if [ -z "$(APPLE_NOTARY_PROFILE)" ]; then \
		echo "APPLE_NOTARY_PROFILE is required for release-notarize"; \
		exit 1; \
	fi
	@if [ "$(RELEASE_CODESIGN_IDENTITY)" = "-" ]; then \
		echo "release-notarize requires a real Developer ID signing identity (APPLE_SIGN_IDENTITY)"; \
		exit 1; \
	fi
	@mkdir -p "$(RELEASE_DIR)"
	@/usr/bin/ditto -c -k --sequesterRsrc --keepParent "$(PACKAGE_APP_DIR)" "$(RELEASE_APP_ZIP)"
	@submission_json="$$(xcrun notarytool submit "$(RELEASE_APP_ZIP)" --keychain-profile "$(APPLE_NOTARY_PROFILE)" --wait --output-format json)"; \
	echo "$$submission_json" > "$(RELEASE_DIR)/notary_submit.json"; \
	status="$$(printf '%s\n' "$$submission_json" | /usr/bin/sed -n 's/.*\"status\"[[:space:]]*:[[:space:]]*\"\([^\"]*\)\".*/\1/p' | /usr/bin/tail -n 1)"; \
	if [ "$$status" != "Accepted" ]; then \
		submission_id="$$(printf '%s\n' "$$submission_json" | /usr/bin/sed -n 's/.*\"id\"[[:space:]]*:[[:space:]]*\"\([^\"]*\)\".*/\1/p' | /usr/bin/head -n 1)"; \
		echo "release-notarize failed: status=$$status id=$$submission_id"; \
		if [ -n "$$submission_id" ]; then \
			xcrun notarytool log "$$submission_id" --keychain-profile "$(APPLE_NOTARY_PROFILE)" > "$(RELEASE_DIR)/notary_log_$$submission_id.json" || true; \
			echo "notary log: $(RELEASE_DIR)/notary_log_$$submission_id.json"; \
		fi; \
		exit 1; \
	fi
	@echo "release-notarize passed."

release-staple:
	@attempt=1; \
	while [ $$attempt -le "$(STAPLE_MAX_ATTEMPTS)" ]; do \
		if xcrun stapler staple "$(PACKAGE_APP_DIR)"; then \
			break; \
		fi; \
		if [ $$attempt -eq "$(STAPLE_MAX_ATTEMPTS)" ]; then \
			echo "release-staple failed after $$attempt attempts"; \
			exit 1; \
		fi; \
		echo "release-staple retry $$attempt/$(STAPLE_MAX_ATTEMPTS) in $(STAPLE_RETRY_DELAY_SEC)s"; \
		sleep "$(STAPLE_RETRY_DELAY_SEC)"; \
		attempt=$$((attempt + 1)); \
	done
	@xcrun stapler validate "$(PACKAGE_APP_DIR)"
	@echo "release-staple passed."

release-verify-notarized: release-verify
	@xcrun stapler validate "$(PACKAGE_APP_DIR)"
	@echo "release-verify-notarized passed."

release-artifact: release-verify
	@mkdir -p "$(RELEASE_DIR)"
	@/usr/bin/ditto -c -k --sequesterRsrc --keepParent "$(PACKAGE_APP_DIR)" "$(RELEASE_APP_ZIP)"
	@shasum -a 256 "$(RELEASE_APP_ZIP)" > "$(RELEASE_APP_ZIP).sha256"
	@{ \
		echo "product=$(RELEASE_PRODUCT_NAME)"; \
		echo "program=$(RELEASE_PROGRAM_KEY)"; \
		echo "bundle_id=$(RELEASE_BUNDLE_ID)"; \
		echo "version=$(RELEASE_VERSION)"; \
		echo "channel=$(RELEASE_CHANNEL)"; \
		echo "platform=$(RELEASE_PLATFORM)"; \
		echo "arch=$(RELEASE_ARCH)"; \
		echo "artifact=$(RELEASE_APP_ZIP)"; \
		echo "sha256_file=$(RELEASE_APP_ZIP).sha256"; \
	} > "$(RELEASE_MANIFEST)"
	@echo "release-artifact complete: $(RELEASE_APP_ZIP)"

release-distribute: release-notarize release-staple release-verify-notarized release-artifact
	@echo "release-distribute passed."

release-desktop-refresh:
	@if [ ! -d "$(PACKAGE_APP_DIR)" ]; then \
		echo "release-desktop-refresh requires an existing built app at $(PACKAGE_APP_DIR)"; \
		echo "run release-distribute first"; \
		exit 1; \
	fi
	@mkdir -p "$$(dirname "$(DESKTOP_APP_DIR)")"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@cp -R "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@echo "Release app refreshed at $(DESKTOP_APP_DIR)"

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

clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(REL_BUILD_DIR) $(REL_TARGET) $(RAY_TRACE_TOOL_BIN) $(RAY_TRACE_TOOL_BIN).dSYM

-include $(DEP)
