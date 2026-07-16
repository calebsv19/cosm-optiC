.PHONY: add-disney-flag
add-disney-flag:
	$(eval CFLAGS += -DCAMERA_INTEGRATOR_DISNEY_AVAILABLE)

clang-build:
	@$(MAKE) clean BUILD_TOOLCHAIN=clang
	@$(MAKE) BUILD_TOOLCHAIN=clang all

fisics-build:
	@$(MAKE) clean BUILD_TOOLCHAIN=fisics
	@$(MAKE) BUILD_TOOLCHAIN=fisics all

toolchain-contract:
	@echo "Program root:    $(CURDIR)"
	@echo "clang build:     $(CLANG_CC) $(CFLAGS) ..."
	@echo "fisiCs build:    $(FISICS_BIN) --overlay=$(FISICS_OVERLAY) $(CFLAGS) ..."
	@echo "units sema srcs: src/import/runtime_scene_bridge.c"
	@echo "                 src/import/runtime_scene_bridge_authoring.c"
	@echo "                 src/app/animation_fluid_scene.c"
	@echo "                 src/render/runtime_light_emitter_3d.c"
	@echo "                 src/render/runtime_volume_3d_sampling.c"
	@echo "                 src/render/runtime_native_3d_sampling.c"
	@echo "                 src/render/runtime_volume_3d.c"
	@echo "                 src/render/runtime_volume_3d_integrate.c"
	@echo "                 src/render/runtime_volume_3d_scatter.c"
	@echo "                 src/render/runtime_direct_light_3d.c"
	@echo "                 src/render/runtime_visibility_3d.c"
	@echo "                 src/render/runtime_camera_3d_rays.c"
	@echo "                 src/render/runtime_ray_3d.c"
	@echo "units output:    $(RAY_TRACING_UNITS_SEMA_OUTPUT)"
	@echo "                 $(RAY_TRACING_AUTHORING_UNITS_SEMA_OUTPUT)"
	@echo "                 $(RAY_TRACING_FLUID_SCENE_UNITS_SEMA_OUTPUT)"
	@echo "                 $(RAY_TRACING_LIGHT_EMITTER_UNITS_SEMA_OUTPUT)"
	@echo "                 $(RAY_TRACING_VOLUME_SAMPLING_UNITS_SEMA_OUTPUT)"
	@echo "                 $(RAY_TRACING_NATIVE_SAMPLING_UNITS_SEMA_OUTPUT)"
	@echo "                 $(RAY_TRACING_VOLUME_UNITS_SEMA_OUTPUT)"
	@echo "                 $(RAY_TRACING_VOLUME_INTEGRATE_UNITS_SEMA_OUTPUT)"
	@echo "                 $(RAY_TRACING_VOLUME_SCATTER_UNITS_SEMA_OUTPUT)"
	@echo "                 $(RAY_TRACING_DIRECT_LIGHT_UNITS_SEMA_OUTPUT)"
	@echo "                 $(RAY_TRACING_VISIBILITY_UNITS_SEMA_OUTPUT)"
	@echo "                 $(RAY_TRACING_CAMERA_RAYS_UNITS_SEMA_OUTPUT)"
	@echo "                 $(RAY_TRACING_RAY3D_UNITS_SEMA_OUTPUT)"

dump-sema-runtime-scene-bridge:
	@mkdir -p "$(call program_build_dir_for,fisics)"
	$(FISICS_ENV) $(FISICS_BIN) --overlay=$(FISICS_OVERLAY) $(CFLAGS) --dump-sema -c "src/import/runtime_scene_bridge.c" -o "$(call program_build_dir_for,fisics)/runtime_scene_bridge.o" > "$(RAY_TRACING_UNITS_SEMA_OUTPUT)" 2>&1
	@echo "Wrote semantic dump to $(RAY_TRACING_UNITS_SEMA_OUTPUT)"

dump-sema-runtime-scene-bridge-authoring:
	@mkdir -p "$(call program_build_dir_for,fisics)"
	$(FISICS_ENV) $(FISICS_BIN) --overlay=$(FISICS_OVERLAY) $(CFLAGS) --dump-sema -c "src/import/runtime_scene_bridge_authoring.c" -o "$(call program_build_dir_for,fisics)/runtime_scene_bridge_authoring.o" > "$(RAY_TRACING_AUTHORING_UNITS_SEMA_OUTPUT)" 2>&1
	@echo "Wrote semantic dump to $(RAY_TRACING_AUTHORING_UNITS_SEMA_OUTPUT)"

dump-sema-animation-fluid-scene:
	@mkdir -p "$(call program_build_dir_for,fisics)"
	$(FISICS_ENV) $(FISICS_BIN) --overlay=$(FISICS_OVERLAY) $(CFLAGS) --dump-sema -c "src/app/animation_fluid_scene.c" -o "$(call program_build_dir_for,fisics)/animation_fluid_scene.o" > "$(RAY_TRACING_FLUID_SCENE_UNITS_SEMA_OUTPUT)" 2>&1
	@echo "Wrote semantic dump to $(RAY_TRACING_FLUID_SCENE_UNITS_SEMA_OUTPUT)"

dump-sema-runtime-light-emitter-3d:
	@mkdir -p "$(call program_build_dir_for,fisics)"
	$(FISICS_ENV) $(FISICS_BIN) --overlay=$(FISICS_OVERLAY) $(CFLAGS) --dump-sema -c "src/render/runtime_light_emitter_3d.c" -o "$(call program_build_dir_for,fisics)/runtime_light_emitter_3d.o" > "$(RAY_TRACING_LIGHT_EMITTER_UNITS_SEMA_OUTPUT)" 2>&1
	@echo "Wrote semantic dump to $(RAY_TRACING_LIGHT_EMITTER_UNITS_SEMA_OUTPUT)"

dump-sema-runtime-volume-3d-sampling:
	@mkdir -p "$(call program_build_dir_for,fisics)"
	$(FISICS_ENV) $(FISICS_BIN) --overlay=$(FISICS_OVERLAY) $(CFLAGS) --dump-sema -c "src/render/runtime_volume_3d_sampling.c" -o "$(call program_build_dir_for,fisics)/runtime_volume_3d_sampling.o" > "$(RAY_TRACING_VOLUME_SAMPLING_UNITS_SEMA_OUTPUT)" 2>&1
	@echo "Wrote semantic dump to $(RAY_TRACING_VOLUME_SAMPLING_UNITS_SEMA_OUTPUT)"

dump-sema-runtime-native-3d-sampling:
	@mkdir -p "$(call program_build_dir_for,fisics)"
	$(FISICS_ENV) $(FISICS_BIN) --overlay=$(FISICS_OVERLAY) $(CFLAGS) --dump-sema -c "src/render/runtime_native_3d_sampling.c" -o "$(call program_build_dir_for,fisics)/runtime_native_3d_sampling.o" > "$(RAY_TRACING_NATIVE_SAMPLING_UNITS_SEMA_OUTPUT)" 2>&1
	@echo "Wrote semantic dump to $(RAY_TRACING_NATIVE_SAMPLING_UNITS_SEMA_OUTPUT)"

dump-sema-runtime-volume-3d:
	@mkdir -p "$(call program_build_dir_for,fisics)"
	$(FISICS_ENV) $(FISICS_BIN) --overlay=$(FISICS_OVERLAY) $(CFLAGS) --dump-sema -c "src/render/runtime_volume_3d.c" -o "$(call program_build_dir_for,fisics)/runtime_volume_3d.o" > "$(RAY_TRACING_VOLUME_UNITS_SEMA_OUTPUT)" 2>&1
	@echo "Wrote semantic dump to $(RAY_TRACING_VOLUME_UNITS_SEMA_OUTPUT)"

dump-sema-runtime-volume-3d-integrate:
	@mkdir -p "$(call program_build_dir_for,fisics)"
	$(FISICS_ENV) $(FISICS_BIN) --overlay=$(FISICS_OVERLAY) $(CFLAGS) --dump-sema -c "src/render/runtime_volume_3d_integrate.c" -o "$(call program_build_dir_for,fisics)/runtime_volume_3d_integrate.o" > "$(RAY_TRACING_VOLUME_INTEGRATE_UNITS_SEMA_OUTPUT)" 2>&1
	@echo "Wrote semantic dump to $(RAY_TRACING_VOLUME_INTEGRATE_UNITS_SEMA_OUTPUT)"

dump-sema-runtime-volume-3d-scatter:
	@mkdir -p "$(call program_build_dir_for,fisics)"
	$(FISICS_ENV) $(FISICS_BIN) --overlay=$(FISICS_OVERLAY) $(CFLAGS) --dump-sema -c "src/render/runtime_volume_3d_scatter.c" -o "$(call program_build_dir_for,fisics)/runtime_volume_3d_scatter.o" > "$(RAY_TRACING_VOLUME_SCATTER_UNITS_SEMA_OUTPUT)" 2>&1
	@echo "Wrote semantic dump to $(RAY_TRACING_VOLUME_SCATTER_UNITS_SEMA_OUTPUT)"

dump-sema-runtime-direct-light-3d:
	@mkdir -p "$(call program_build_dir_for,fisics)"
	$(FISICS_ENV) $(FISICS_BIN) --overlay=$(FISICS_OVERLAY) $(CFLAGS) --dump-sema -c "src/render/runtime_direct_light_3d.c" -o "$(call program_build_dir_for,fisics)/runtime_direct_light_3d.o" > "$(RAY_TRACING_DIRECT_LIGHT_UNITS_SEMA_OUTPUT)" 2>&1
	@echo "Wrote semantic dump to $(RAY_TRACING_DIRECT_LIGHT_UNITS_SEMA_OUTPUT)"

dump-sema-runtime-visibility-3d:
	@mkdir -p "$(call program_build_dir_for,fisics)"
	$(FISICS_ENV) $(FISICS_BIN) --overlay=$(FISICS_OVERLAY) $(CFLAGS) --dump-sema -c "src/render/runtime_visibility_3d.c" -o "$(call program_build_dir_for,fisics)/runtime_visibility_3d.o" > "$(RAY_TRACING_VISIBILITY_UNITS_SEMA_OUTPUT)" 2>&1
	@echo "Wrote semantic dump to $(RAY_TRACING_VISIBILITY_UNITS_SEMA_OUTPUT)"

dump-sema-runtime-camera-3d-rays:
	@mkdir -p "$(call program_build_dir_for,fisics)"
	$(FISICS_ENV) $(FISICS_BIN) --overlay=$(FISICS_OVERLAY) $(CFLAGS) --dump-sema -c "src/render/runtime_camera_3d_rays.c" -o "$(call program_build_dir_for,fisics)/runtime_camera_3d_rays.o" > "$(RAY_TRACING_CAMERA_RAYS_UNITS_SEMA_OUTPUT)" 2>&1
	@echo "Wrote semantic dump to $(RAY_TRACING_CAMERA_RAYS_UNITS_SEMA_OUTPUT)"

dump-sema-runtime-ray-3d:
	@mkdir -p "$(call program_build_dir_for,fisics)"
	$(FISICS_ENV) $(FISICS_BIN) --overlay=$(FISICS_OVERLAY) $(CFLAGS) --dump-sema -c "src/render/runtime_ray_3d.c" -o "$(call program_build_dir_for,fisics)/runtime_ray_3d.o" > "$(RAY_TRACING_RAY3D_UNITS_SEMA_OUTPUT)" 2>&1
	@echo "Wrote semantic dump to $(RAY_TRACING_RAY3D_UNITS_SEMA_OUTPUT)"

all: $(APP_TARGET)

$(APP_TARGET): $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(OBJ) -o $@ $(LDFLAGS) $(FISICS_MEMCHECK_LINK_LIBS)


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

$(BUILD_DIR)/core_queue/%.o: $(CORE_QUEUE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_time/%.o: $(CORE_TIME_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_workers/%.o: $(CORE_WORKERS_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_sim/%.o: $(CORE_SIM_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_scene/%.o: $(CORE_SCENE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_scene_view/%.o: $(CORE_SCENE_VIEW_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_viewport3d/%.o: $(CORE_VIEWPORT3D_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_authored_texture/%.o: $(CORE_AUTHORED_TEXTURE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_scene_compile/%.o: $(CORE_SCENE_COMPILE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_mesh_asset/%.o: $(CORE_MESH_ASSET_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_mesh_preview/%.o: $(CORE_MESH_PREVIEW_DIR)/src/%.c
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

$(BUILD_DIR)/core_pane_module/%.o: $(CORE_PANE_MODULE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_theme/%.o: $(CORE_THEME_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_font/%.o: $(CORE_FONT_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_headless_job/%.o: $(CORE_HEADLESS_JOB_DIR)/src/%.c
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

$(BUILD_DIR)/kit_viewport3d/%.o: $(KIT_VIEWPORT3D_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/kit_workspace_authoring/%.o: $(KIT_WORKSPACE_AUTHORING_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/tests/%.o: $(TEST_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@


debug: CFLAGS += -O0 -g3
debug: clean all

format:
	@command -v clang-format >/dev/null 2>&1 && clang-format -i $(SRC) $(shell find $(INC_DIR) -name '*.h') || echo "clang-format not found"

release:
	$(MAKE) BUILD_DIR=$(REL_BUILD_DIR) APP_TARGET=$(REL_TARGET) CFLAGS="$(CFLAGS_RELEASE)" LDFLAGS="$(LDFLAGS)" all

relrun: release
	./$(REL_TARGET)

clean:
	rm -rf $(BUILD_DIR_BASE)/toolchains $(REL_BUILD_DIR) $(REL_TARGET) $(RAY_TRACE_TOOL_BIN) $(RAY_TRACE_TOOL_BIN).dSYM
