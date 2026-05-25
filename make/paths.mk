REL_BUILD_DIR_BASE ?= build_release
REL_TARGET := Ray_anim_release
BUILD_DIR ?= $(call program_build_dir_for,$(BUILD_TOOLCHAIN))
ifeq ($(UNAME_S),Darwin)
REL_BUILD_DIR ?= $(REL_BUILD_DIR_BASE)/$(TARGET_ARCH)
else
REL_BUILD_DIR ?= $(REL_BUILD_DIR_BASE)
endif

APP_TARGET := $(BUILD_DIR)/$(TARGET)
PACKAGE_SOURCE_BIN := $(call program_bin_for,$(PACKAGE_TOOLCHAIN))
RAY_TRACING_UNITS_SEMA_OUTPUT := $(call program_build_dir_for,fisics)/runtime_scene_bridge.sema.txt
RAY_TRACING_AUTHORING_UNITS_SEMA_OUTPUT := $(call program_build_dir_for,fisics)/runtime_scene_bridge_authoring.sema.txt
RAY_TRACING_FLUID_SCENE_UNITS_SEMA_OUTPUT := $(call program_build_dir_for,fisics)/animation_fluid_scene.sema.txt
RAY_TRACING_LIGHT_EMITTER_UNITS_SEMA_OUTPUT := $(call program_build_dir_for,fisics)/runtime_light_emitter_3d.sema.txt
RAY_TRACING_VOLUME_SAMPLING_UNITS_SEMA_OUTPUT := $(call program_build_dir_for,fisics)/runtime_volume_3d_sampling.sema.txt
RAY_TRACING_NATIVE_SAMPLING_UNITS_SEMA_OUTPUT := $(call program_build_dir_for,fisics)/runtime_native_3d_sampling.sema.txt
RAY_TRACING_VOLUME_UNITS_SEMA_OUTPUT := $(call program_build_dir_for,fisics)/runtime_volume_3d.sema.txt
RAY_TRACING_VOLUME_INTEGRATE_UNITS_SEMA_OUTPUT := $(call program_build_dir_for,fisics)/runtime_volume_3d_integrate.sema.txt
RAY_TRACING_VOLUME_SCATTER_UNITS_SEMA_OUTPUT := $(call program_build_dir_for,fisics)/runtime_volume_3d_scatter.sema.txt
RAY_TRACING_DIRECT_LIGHT_UNITS_SEMA_OUTPUT := $(call program_build_dir_for,fisics)/runtime_direct_light_3d.sema.txt
RAY_TRACING_VISIBILITY_UNITS_SEMA_OUTPUT := $(call program_build_dir_for,fisics)/runtime_visibility_3d.sema.txt
RAY_TRACING_CAMERA_RAYS_UNITS_SEMA_OUTPUT := $(call program_build_dir_for,fisics)/runtime_camera_3d_rays.sema.txt
RAY_TRACING_RAY3D_UNITS_SEMA_OUTPUT := $(call program_build_dir_for,fisics)/runtime_ray_3d.sema.txt

CLI_BIN_DIR := tools/cli/bin
CLI_TOOLS := $(CLI_BIN_DIR)/shape_asset_tool $(CLI_BIN_DIR)/shape_import_tool $(CLI_BIN_DIR)/shape_sanity_tool
RAY_TRACE_TOOL_SRC := $(SRC_DIR)/tools/cli/ray_trace_tool.c
RAY_TRACE_TOOL_BIN := ray_trace_tool
NATIVE3D_AUDIT_SRC := $(SRC_DIR)/tools/cli/native3d_render_audit.c
NATIVE3D_AUDIT_BIN := $(BUILD_DIR)/tools/cli/native3d_render_audit
NATIVE3D_AUDIT_OBJ := $(BUILD_DIR)/tools/cli/native3d_render_audit.o
RAY_TRACING_RENDER_HEADLESS_SRC := $(SRC_DIR)/tools/cli/ray_tracing_render_headless.c
RAY_TRACING_RENDER_HEADLESS_BIN := $(BUILD_DIR)/tools/cli/ray_tracing_render_headless
RAY_TRACING_RENDER_HEADLESS_OBJ := $(BUILD_DIR)/tools/cli/ray_tracing_render_headless.o
RAY_TRACING_JOB_RUNNER_SRC := $(SRC_DIR)/tools/cli/ray_tracing_job_runner.c
RAY_TRACING_JOB_RUNNER_BIN := $(BUILD_DIR)/tools/cli/ray_tracing_job_runner
RAY_TRACING_JOB_RUNNER_OBJ := $(BUILD_DIR)/tools/cli/ray_tracing_job_runner.o
RAY_TRACING_MATERIAL_PREVIEW_HEADLESS_SRC := $(SRC_DIR)/tools/cli/ray_tracing_material_preview_headless.c
RAY_TRACING_MATERIAL_PREVIEW_HEADLESS_BIN := $(BUILD_DIR)/tools/cli/ray_tracing_material_preview_headless
RAY_TRACING_MATERIAL_PREVIEW_HEADLESS_OBJ := $(BUILD_DIR)/tools/cli/ray_tracing_material_preview_headless.o

VIDEO_FRAMES_DIR ?= data/runtime/frames/default
VIDEO_OUTPUT ?= data/runtime/videos/output.mp4
VIDEO_FPS ?= 30

TEST_DIR := tests
TEST_BIN := $(BUILD_DIR)/tests/test_runner
TEST_SRC := $(TEST_DIR)/test_runner.c
