REL_BUILD_DIR_BASE ?= build_release
REL_TARGET := Ray_anim_release
ifeq ($(UNAME_S),Darwin)
BUILD_DIR ?= $(BUILD_DIR_BASE)/$(TARGET_ARCH)
REL_BUILD_DIR ?= $(REL_BUILD_DIR_BASE)/$(TARGET_ARCH)
else
BUILD_DIR ?= $(BUILD_DIR_BASE)
REL_BUILD_DIR ?= $(REL_BUILD_DIR_BASE)
endif

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
