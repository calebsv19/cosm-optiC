SRC := $(shell find $(SRC_DIR) -name '*.c' \
	! -path '$(SRC_DIR)/tools/cli/*' \
	! -path '$(SRC_DIR)/render/integrators/camera_path_integrator_old_version.c' \
	! -path '$(SRC_DIR)/render/TimerHUD_legacy_backup/*')
VK_RENDERER_SRCS := $(shell find $(VK_RENDERER_DIR)/src -name '*.c')
OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC))
OBJ := $(filter-out $(BUILD_DIR)/render/integrators/camera_path_integrator_old_version.o,$(OBJ))
OBJ := $(filter-out $(BUILD_DIR)/render/adapters/timer_hud_headless_stub.o,$(OBJ))

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
CORE_SIM_SRCS := $(CORE_SIM_DIR)/src/core_sim.c
CORE_SCENE_SRCS := $(CORE_SCENE_DIR)/src/core_scene.c
CORE_SCENE_VIEW_SRCS := $(CORE_SCENE_VIEW_DIR)/src/core_scene_view.c
CORE_AUTHORED_TEXTURE_SRCS := $(CORE_AUTHORED_TEXTURE_DIR)/src/core_authored_texture.c
CORE_SCENE_COMPILE_SRCS := $(CORE_SCENE_COMPILE_DIR)/src/core_scene_compile.c
CORE_MESH_ASSET_SRCS := \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset_authoring_document.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset_runtime_document.c
CORE_MESH_PREVIEW_SRCS := $(CORE_MESH_PREVIEW_DIR)/src/core_mesh_preview.c
CORE_OBJECT_SRCS := $(CORE_OBJECT_DIR)/src/core_object.c
CORE_UNITS_SRCS := $(CORE_UNITS_DIR)/src/core_units.c
CORE_SPACE_SRCS := $(CORE_SPACE_DIR)/src/core_space.c
CORE_PANE_SRCS := $(CORE_PANE_DIR)/src/core_pane.c
CORE_PANE_MODULE_SRCS := $(CORE_PANE_MODULE_DIR)/src/core_pane_module.c
CORE_THEME_SRCS := $(CORE_THEME_DIR)/src/core_theme.c
CORE_FONT_SRCS := $(CORE_FONT_DIR)/src/core_font.c
CORE_HEADLESS_JOB_SRCS := $(CORE_HEADLESS_JOB_DIR)/src/core_headless_job.c
KIT_RENDER_SRCS := \
	$(KIT_RENDER_DIR)/src/kit_render.c \
	$(KIT_RENDER_DIR)/src/kit_render_external_text.c \
	$(KIT_RENDER_DIR)/src/kit_render_backend_null.c \
	$(KIT_RENDER_DIR)/src/kit_render_backend_vk.c
KIT_PANE_SRCS := $(KIT_PANE_DIR)/src/kit_pane.c
KIT_VIZ_SRCS := $(KIT_VIZ_DIR)/src/kit_viz.c
KIT_RUNTIME_DIAG_SRCS := $(KIT_RUNTIME_DIAG_DIR)/src/kit_runtime_diag.c
KIT_WORKSPACE_AUTHORING_SRCS := \
	$(KIT_WORKSPACE_AUTHORING_DIR)/src/kit_workspace_authoring.c \
	$(KIT_WORKSPACE_AUTHORING_DIR)/src/ui/kit_workspace_authoring_ui_overlay.c \
	$(KIT_WORKSPACE_AUTHORING_DIR)/src/ui/kit_workspace_authoring_ui_font_theme.c
