SHARED_ROOT ?= third_party/codework_shared
SHARED_WORKSPACE_DIR ?= ../shared
SHARED_ASSETS_DIR := $(SHARED_ROOT)/assets
VK_RENDERER_DIR := $(SHARED_ROOT)/vk_renderer
CORE_BASE_DIR := $(SHARED_ROOT)/core/core_base
CORE_IO_DIR := $(SHARED_ROOT)/core/core_io
CORE_DATA_DIR := $(SHARED_ROOT)/core/core_data
CORE_PACK_DIR := $(SHARED_ROOT)/core/core_pack
CORE_QUEUE_DIR := $(SHARED_ROOT)/core/core_queue
CORE_TIME_DIR := $(SHARED_ROOT)/core/core_time
CORE_WORKERS_DIR := $(SHARED_ROOT)/core/core_workers
CORE_SIM_DIR := $(SHARED_ROOT)/core/core_sim
CORE_SCENE_DIR := $(SHARED_ROOT)/core/core_scene
CORE_AUTHORED_TEXTURE_DIR := $(SHARED_ROOT)/core/core_authored_texture
CORE_SCENE_COMPILE_DIR := $(SHARED_ROOT)/core/core_scene_compile
CORE_MESH_ASSET_DIR := $(SHARED_ROOT)/core/core_mesh_asset
CORE_MESH_PREVIEW_DIR := $(SHARED_ROOT)/core/core_mesh_preview
CORE_OBJECT_DIR := $(SHARED_ROOT)/core/core_object
CORE_UNITS_DIR := $(SHARED_ROOT)/core/core_units
CORE_TRACE_DIR := $(SHARED_ROOT)/core/core_trace
CORE_SPACE_DIR := $(SHARED_ROOT)/core/core_space
CORE_PANE_DIR := $(SHARED_ROOT)/core/core_pane
CORE_THEME_DIR := $(SHARED_ROOT)/core/core_theme
CORE_FONT_DIR := $(SHARED_ROOT)/core/core_font
CORE_HEADLESS_JOB_DIR := $(SHARED_ROOT)/core/core_headless_job
KIT_RENDER_DIR := $(SHARED_ROOT)/kit/kit_render
KIT_PANE_DIR := $(SHARED_ROOT)/kit/kit_pane
KIT_VIZ_DIR := $(SHARED_ROOT)/kit/kit_viz
KIT_RUNTIME_DIAG_DIR := $(SHARED_ROOT)/kit/kit_runtime_diag
KIT_WORKSPACE_AUTHORING_DIR := $(SHARED_ROOT)/kit/kit_workspace_authoring
ifeq ($(wildcard $(CORE_AUTHORED_TEXTURE_DIR)/include/core_authored_texture.h),)
CORE_AUTHORED_TEXTURE_DIR := $(SHARED_WORKSPACE_DIR)/core/core_authored_texture
endif
ifeq ($(wildcard $(CORE_MESH_ASSET_DIR)/include/core_mesh_asset.h),)
CORE_MESH_ASSET_DIR := $(SHARED_WORKSPACE_DIR)/core/core_mesh_asset
endif
ifeq ($(wildcard $(CORE_MESH_PREVIEW_DIR)/include/core_mesh_preview.h),)
CORE_MESH_PREVIEW_DIR := $(SHARED_WORKSPACE_DIR)/core/core_mesh_preview
endif
ifeq ($(wildcard $(CORE_HEADLESS_JOB_DIR)/include/core_headless_job.h),)
CORE_HEADLESS_JOB_DIR := $(SHARED_WORKSPACE_DIR)/core/core_headless_job
endif

TIMER_HUD_DIR := $(SHARED_ROOT)/timer_hud
TIMER_HUD_INCLUDE := -I$(TIMER_HUD_DIR)/include -I$(TIMER_HUD_DIR)/external
