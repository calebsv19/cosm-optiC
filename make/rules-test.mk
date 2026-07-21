STABLE_TEST_TARGETS := \
	test \
	test-starter-scene-profile-contract \
	test-ray-tracing-triangle-topology-stability \
	test-runtime-scene-bridge-contract \
	test-runtime-mesh-asset-loader \
	test-scene-editor-mesh-preview-outline \
	test-runtime-mesh-asset-pack \
	test-runtime-mesh-asset-builder \
	test-smooth-mesh-reflection-fixtures \
	test-runtime-mesh-asset-headless-audit \
	test-runtime-triangle-bvh-3d \
	test-ray-tracing-core-sim-runtime-frame-contract \
	test-ray-tracing-runtime-host-lifecycle-contract \
	test-menu-pane-host-contract \
	test-scene-editor-pane-host-contract \
	test-scene-editor-viewport-nav-contract \
	test-scene-editor-viewport3d-bridge-contract \
	test-ray-tracing-render-headless-preflight \
	test-ray-tracing-render-headless-image-export \
	test-ray-tracing-render-headless-mesh-asset-spheres \
	test-ray-tracing-render-headless-mesh-asset-sphere-pressure \
	test-ray-tracing-render-headless-mesh-asset-sphere-pressure-mrt8 \
	test-ray-tracing-render-headless-mesh-asset-sphere-pressure-mrt10 \
	test-ray-tracing-render-headless-mesh-asset-sphere-pressure-mrt12-static-cache \
	test-ray-tracing-render-headless-tlas-blas-repeated-instance-stress \
	test-ray-tracing-material-preview-headless \
	test-ray-tracing-job-runner-smoke \
	test-ray-tracing-job-runner-bundle-smoke \
	test-ray-tracing-publish-helper-validation \
	test-ray-tracing-repo-doc-redaction \
	test-ray-tracing-linux-worker-package-validator \
	test-ray-tracing-release-contract-redaction \
	test-ray-tracing-job-runner-policy \
	test-ray-tracing-wtr66-preview-matrix-planner-dry-run \
	test-manifest-to-trace-export \
	test-fluid-pack-contract-parity \
	test-trio-scene-contract-diff \
	test-shared-theme-font-adapter \
	test-ray-tracing-workspace-authoring-host

LEGACY_TEST_TARGETS :=

STARTER_SCENE_PROFILE_TEST_BIN := $(BUILD_DIR)/tests/starter_scene_profile_test
STARTER_SCENE_PROFILE_TEST_SRCS := \
	$(TEST_DIR)/starter_scene_profile_test.c \
	$(SRC_DIR)/app/starter_scene_profile.c \
	$(SRC_DIR)/app/starter_scene_startup.c

$(STARTER_SCENE_PROFILE_TEST_BIN): $(STARTER_SCENE_PROFILE_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Wno-unknown-attributes -Wno-c23-extensions -g \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-o $@ $(STARTER_SCENE_PROFILE_TEST_SRCS) $(JSON_LIBS)

test-starter-scene-profile-contract: $(STARTER_SCENE_PROFILE_TEST_BIN)
	@$(STARTER_SCENE_PROFILE_TEST_BIN)

SCENE_EDITOR_MESH_PREVIEW_OUTLINE_TEST_BIN := \
	$(BUILD_DIR)/tests/scene_editor_mesh_preview_outline_test
SCENE_EDITOR_MESH_PREVIEW_OUTLINE_TEST_SRCS := \
	$(TEST_DIR)/scene_editor_mesh_preview_outline_test.c \
	$(SRC_DIR)/editor/scene_editor_mesh_preview_outline.c \
	$(KIT_VIEWPORT3D_DIR)/src/kit_viewport3d.c

$(SCENE_EDITOR_MESH_PREVIEW_OUTLINE_TEST_BIN): $(SCENE_EDITOR_MESH_PREVIEW_OUTLINE_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Wno-unknown-attributes -Wno-c23-extensions -g \
		$(SDL_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) -I$(KIT_VIEWPORT3D_DIR)/include \
		-o $@ $(SCENE_EDITOR_MESH_PREVIEW_OUTLINE_TEST_SRCS) $(SDL_LIBS) -lm

test-scene-editor-mesh-preview-outline: $(SCENE_EDITOR_MESH_PREVIEW_OUTLINE_TEST_BIN)
	@$(SCENE_EDITOR_MESH_PREVIEW_OUTLINE_TEST_BIN)

run: $(APP_TARGET)
	./$(APP_TARGET)

run-ide-theme: $(APP_TARGET)
	RAY_TRACING_USE_SHARED_THEME_FONT=1 RAY_TRACING_USE_SHARED_THEME=1 RAY_TRACING_USE_SHARED_FONT=1 RAY_TRACING_THEME_PRESET=ide_gray RAY_TRACING_FONT_PRESET=ide ./$(APP_TARGET)

run-daw-theme: $(APP_TARGET)
	RAY_TRACING_USE_SHARED_THEME_FONT=1 RAY_TRACING_USE_SHARED_THEME=1 RAY_TRACING_USE_SHARED_FONT=1 RAY_TRACING_THEME_PRESET=daw_default RAY_TRACING_FONT_PRESET=daw_default ./$(APP_TARGET)

run-headless-smoke: all test-stable
	@echo "ray_tracing headless smoke passed (non-interactive)"

visual-harness: $(APP_TARGET)
	@echo "visual harness binary ready: $(APP_TARGET)"

visual-artifact: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	tests/integration/run_ray_tracing_visual_artifact.sh

test: $(APP_TARGET) $(TEST_BIN)
	./$(TEST_BIN)

test-ray-tracing-triangle-topology-stability: $(TEST_BIN)
	@TEST_RUNNER_GROUP=runtime_native_3d_denoise ./$(TEST_BIN)
	@TEST_RUNNER_GROUP=runtime_disney_v2_topology_stability ./$(TEST_BIN)
	@echo "ray tracing triangle topology stability lane passed"

RAY_TRACING_FOLDER_PICKER_TEST_BIN := $(BUILD_DIR)/tests/ray_tracing_folder_picker_test
RAY_TRACING_FOLDER_PICKER_TEST_SRCS := \
	$(TEST_DIR)/ray_tracing_folder_picker_test.c \
	$(SRC_DIR)/platform/ray_tracing_folder_picker.c
RAY_TRACING_FOLDER_PICKER_TEST_PLATFORM_FLAGS :=

ifeq ($(UNAME_S),Linux)
RAY_TRACING_FOLDER_PICKER_TEST_PLATFORM_FLAGS += -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700
endif

$(RAY_TRACING_FOLDER_PICKER_TEST_BIN): $(RAY_TRACING_FOLDER_PICKER_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Wno-unknown-attributes -Wno-c23-extensions -g \
		$(RAY_TRACING_FOLDER_PICKER_TEST_PLATFORM_FLAGS) \
		-DRAY_TRACING_FOLDER_PICKER_FORCE_LINUX -I$(INC_DIR) -I$(SRC_DIR) \
		-o $@ $(RAY_TRACING_FOLDER_PICKER_TEST_SRCS)

test-ray-tracing-folder-picker: $(RAY_TRACING_FOLDER_PICKER_TEST_BIN)
	@$(RAY_TRACING_FOLDER_PICKER_TEST_BIN) || (echo "ray tracing folder picker test failed."; exit 1)
	@echo "ray tracing folder picker lane passed"

RAY_TRACING_PATH_OPENER_TEST_BIN := $(BUILD_DIR)/tests/ray_tracing_path_opener_test
RAY_TRACING_PATH_OPENER_TEST_SRCS := \
	$(TEST_DIR)/ray_tracing_path_opener_test.c \
	$(SRC_DIR)/platform/ray_tracing_path_opener.c

$(RAY_TRACING_PATH_OPENER_TEST_BIN): $(RAY_TRACING_PATH_OPENER_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Wno-unknown-attributes -Wno-c23-extensions -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L -DRAY_TRACING_PATH_OPENER_FORCE_LINUX -I$(INC_DIR) -I$(SRC_DIR) \
		-o $@ $(RAY_TRACING_PATH_OPENER_TEST_SRCS)

test-ray-tracing-path-opener: $(RAY_TRACING_PATH_OPENER_TEST_BIN)
	@$(RAY_TRACING_PATH_OPENER_TEST_BIN) || (echo "ray tracing path opener test failed."; exit 1)
	@echo "ray tracing path opener lane passed"

test-runtime-scene-bridge-contract: $(APP_TARGET) $(TEST_BIN)
	@TEST_RUNNER_GROUP=runtime_scene_bridge_core ./$(TEST_BIN) || (echo "ray tracing runtime scene bridge core contract test failed."; exit 1)
	@TEST_RUNNER_GROUP=runtime_scene_bridge_writeback ./$(TEST_BIN) || (echo "ray tracing runtime scene bridge writeback contract test failed."; exit 1)
	@echo "ray tracing runtime scene bridge contract lane passed"

test-water-surface-import-contract: $(APP_TARGET) $(TEST_BIN)
	@TEST_RUNNER_GROUP=fluid_volume_import_3d ./$(TEST_BIN) || (echo "ray tracing water surface import contract test failed."; exit 1)
	@TEST_RUNNER_GROUP=water_surface_runtime ./$(TEST_BIN) || (echo "ray tracing water surface heightfield builder test failed."; exit 1)
	@echo "ray tracing water surface import and builder contract lane passed"

RUNTIME_MESH_ASSET_LOADER_TEST_BIN := $(BUILD_DIR)/tests/runtime_mesh_asset_loader_test
RUNTIME_MESH_ASSET_LOADER_TEST_SRCS := \
	$(TEST_DIR)/test_runtime_mesh_asset_loader.c \
	$(SRC_DIR)/import/runtime_mesh_asset_loader.c \
	$(SRC_DIR)/import/runtime_mesh_asset_loader_cache.c \
	$(SRC_DIR)/import/runtime_mesh_asset_loader_utils.c \
	$(SRC_DIR)/import/runtime_mesh_asset_pack.c \
	$(CORE_MESH_PREVIEW_DIR)/src/core_mesh_preview.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset_runtime_document.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_SCENE_DIR)/src/core_scene.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_MESH_ASSET_DIR)/../../shape/external/cjson/cJSON.c

$(RUNTIME_MESH_ASSET_LOADER_TEST_BIN): $(RUNTIME_MESH_ASSET_LOADER_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Wno-unknown-attributes -Wno-c23-extensions -g \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) -I$(CORE_MESH_ASSET_DIR)/include \
		-I$(CORE_MESH_PREVIEW_DIR)/include -I$(CORE_MESH_PREVIEW_DIR)/../../shape/external \
		-I$(CORE_IO_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_OBJECT_DIR)/include \
		-I$(CORE_UNITS_DIR)/include -I$(CORE_BASE_DIR)/include \
		-DRAY_TRACING_RUNTIME_MESH_ASSET_LOADER_STANDALONE \
		-o $@ $(RUNTIME_MESH_ASSET_LOADER_TEST_SRCS) $(JSON_LIBS) -lm

test-runtime-mesh-asset-loader: $(APP_TARGET) $(TEST_BIN)
	@TEST_RUNNER_GROUP=runtime_mesh_asset_loader ./$(TEST_BIN) || (echo "ray tracing runtime mesh asset loader test failed."; exit 1)
	@echo "ray tracing runtime mesh asset loader lane passed"

RUNTIME_MESH_ASSET_PACK_TEST_BIN := $(BUILD_DIR)/tests/runtime_mesh_asset_pack_test
RUNTIME_MESH_ASSET_PACK_TEST_SRCS := \
	$(TEST_DIR)/test_runtime_mesh_asset_pack.c \
	$(SRC_DIR)/import/runtime_mesh_asset_pack.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset_runtime_document.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_MESH_ASSET_DIR)/../../shape/external/cjson/cJSON.c

$(RUNTIME_MESH_ASSET_PACK_TEST_BIN): $(RUNTIME_MESH_ASSET_PACK_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Wno-unknown-attributes -Wno-c23-extensions -g \
		-I$(INC_DIR) -I$(SRC_DIR) -I$(CORE_MESH_ASSET_DIR)/include \
		-I$(CORE_IO_DIR)/include -I$(CORE_OBJECT_DIR)/include \
		-I$(CORE_UNITS_DIR)/include -I$(CORE_BASE_DIR)/include \
		-DRAY_TRACING_RUNTIME_MESH_ASSET_PACK_STANDALONE \
		-o $@ $(RUNTIME_MESH_ASSET_PACK_TEST_SRCS) -lm

test-runtime-mesh-asset-pack: $(RUNTIME_MESH_ASSET_PACK_TEST_BIN)
	@$(RUNTIME_MESH_ASSET_PACK_TEST_BIN) || (echo "ray tracing runtime mesh asset pack test failed."; exit 1)
	@echo "ray tracing runtime mesh asset pack lane passed"

RUNTIME_MESH_ASSET_BUILDER_TEST_BIN := $(BUILD_DIR)/tests/runtime_mesh_asset_builder_test
RUNTIME_MESH_ASSET_BUILDER_TEST_SRCS := \
	$(TEST_DIR)/test_runtime_mesh_asset_builder.c \
	$(TEST_DIR)/test_runtime_material_payload_stub.c \
	$(SRC_DIR)/import/runtime_mesh_asset_loader.c \
	$(SRC_DIR)/import/runtime_mesh_asset_loader_cache.c \
	$(SRC_DIR)/import/runtime_mesh_asset_loader_utils.c \
	$(SRC_DIR)/import/runtime_mesh_asset_pack.c \
	$(CORE_MESH_PREVIEW_DIR)/src/core_mesh_preview.c \
	$(SRC_DIR)/render/runtime_ray_3d.c \
	$(SRC_DIR)/render/runtime_scene_3d.c \
	$(SRC_DIR)/render/runtime_emissive_light_set_3d.c \
	$(SRC_DIR)/render/runtime_environment_3d.c \
	$(SRC_DIR)/render/runtime_light_set_3d.c \
	$(SRC_DIR)/render/runtime_dynamic_geometry_accel_3d.c \
	$(SRC_DIR)/render/runtime_mesh_accel_pack_3d.c \
	$(SRC_DIR)/render/runtime_mesh_blas_cache_3d.c \
	$(SRC_DIR)/render/runtime_scene_accel_3d_instances.c \
	$(SRC_DIR)/render/runtime_scene_accel_3d.c \
	$(SRC_DIR)/render/runtime_scene_3d_builder.c \
	$(SRC_DIR)/render/runtime_scene_3d_builder_geometry.c \
	$(SRC_DIR)/render/runtime_scene_3d_builder_mesh.c \
	$(SRC_DIR)/render/runtime_scene_3d_builder_shared.c \
	$(SRC_DIR)/render/runtime_triangle_bvh_3d.c \
	$(SRC_DIR)/render/runtime_triangle_bvh_cache_3d.c \
	$(SRC_DIR)/render/runtime_triangle_bvh_trace_3d.c \
	$(SRC_DIR)/render/runtime_volume_3d.c \
	$(TEST_DIR)/test_runtime_scene_motion_bridge_noop_stub.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset_runtime_document.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_SCENE_DIR)/src/core_scene.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_MESH_ASSET_DIR)/../../shape/external/cjson/cJSON.c

$(RUNTIME_MESH_ASSET_BUILDER_TEST_BIN): $(RUNTIME_MESH_ASSET_BUILDER_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Wno-unknown-attributes -Wno-c23-extensions -g \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) -I$(CORE_MESH_ASSET_DIR)/include \
		-I$(CORE_MESH_PREVIEW_DIR)/include -I$(CORE_MESH_PREVIEW_DIR)/../../shape/external \
		-I$(CORE_IO_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_OBJECT_DIR)/include \
		-I$(CORE_UNITS_DIR)/include -I$(CORE_BASE_DIR)/include \
		-o $@ $(RUNTIME_MESH_ASSET_BUILDER_TEST_SRCS) $(JSON_LIBS) -lm

test-runtime-mesh-asset-builder: $(RUNTIME_MESH_ASSET_BUILDER_TEST_BIN)
	@$(RUNTIME_MESH_ASSET_BUILDER_TEST_BIN) || (echo "ray tracing runtime mesh asset builder test failed."; exit 1)
	@echo "ray tracing runtime mesh asset builder lane passed"

test-smooth-mesh-reflection-fixtures: $(SMOOTH_MESH_RUNTIME_COMPILE_TOOL_BIN)
	bash tests/integration/run_smooth_mesh_reflection_fixture_smoke.sh

test-smooth-mesh-reflection-matrix: $(SMOOTH_MESH_RUNTIME_COMPILE_TOOL_BIN) $(RAY_TRACING_RENDER_HEADLESS_BIN)
	bash tests/integration/run_smooth_mesh_reflection_matrix.sh

RUNTIME_MESH_ASSET_HEADLESS_AUDIT_TEST_BIN := $(BUILD_DIR)/tests/runtime_mesh_asset_headless_audit_test
RUNTIME_MESH_ASSET_HEADLESS_AUDIT_TEST_SRCS := \
	$(TEST_DIR)/test_runtime_mesh_asset_headless_audit.c \
	$(TEST_DIR)/test_runtime_material_payload_stub.c \
	$(SRC_DIR)/import/runtime_mesh_asset_loader.c \
	$(SRC_DIR)/import/runtime_mesh_asset_loader_cache.c \
	$(SRC_DIR)/import/runtime_mesh_asset_loader_utils.c \
	$(SRC_DIR)/import/runtime_mesh_asset_pack.c \
	$(CORE_MESH_PREVIEW_DIR)/src/core_mesh_preview.c \
	$(SRC_DIR)/render/runtime_camera_3d_rays.c \
	$(SRC_DIR)/render/runtime_ray_3d.c \
	$(SRC_DIR)/render/runtime_scene_3d.c \
	$(SRC_DIR)/render/runtime_emissive_light_set_3d.c \
	$(SRC_DIR)/render/runtime_environment_3d.c \
	$(SRC_DIR)/render/runtime_light_set_3d.c \
	$(SRC_DIR)/render/runtime_dynamic_geometry_accel_3d.c \
	$(SRC_DIR)/render/runtime_mesh_accel_pack_3d.c \
	$(SRC_DIR)/render/runtime_mesh_blas_cache_3d.c \
	$(SRC_DIR)/render/runtime_scene_accel_3d_instances.c \
	$(SRC_DIR)/render/runtime_scene_accel_3d.c \
	$(SRC_DIR)/render/runtime_scene_3d_builder.c \
	$(SRC_DIR)/render/runtime_scene_3d_builder_geometry.c \
	$(SRC_DIR)/render/runtime_scene_3d_builder_mesh.c \
	$(SRC_DIR)/render/runtime_scene_3d_builder_shared.c \
	$(SRC_DIR)/render/runtime_triangle_bvh_3d.c \
	$(SRC_DIR)/render/runtime_triangle_bvh_cache_3d.c \
	$(SRC_DIR)/render/runtime_triangle_bvh_trace_3d.c \
	$(SRC_DIR)/render/runtime_volume_3d.c \
	$(TEST_DIR)/test_runtime_scene_motion_bridge_noop_stub.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset_runtime_document.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_SCENE_DIR)/src/core_scene.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_MESH_ASSET_DIR)/../../shape/external/cjson/cJSON.c

$(RUNTIME_MESH_ASSET_HEADLESS_AUDIT_TEST_BIN): $(RUNTIME_MESH_ASSET_HEADLESS_AUDIT_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Wno-unknown-attributes -Wno-c23-extensions -g \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) -I$(CORE_MESH_ASSET_DIR)/include \
		-I$(CORE_MESH_PREVIEW_DIR)/include -I$(CORE_MESH_PREVIEW_DIR)/../../shape/external \
		-I$(CORE_IO_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_OBJECT_DIR)/include \
		-I$(CORE_UNITS_DIR)/include -I$(CORE_BASE_DIR)/include \
		-o $@ $(RUNTIME_MESH_ASSET_HEADLESS_AUDIT_TEST_SRCS) $(JSON_LIBS) -lm $(FISICS_MEMCHECK_LINK_LIBS)

test-runtime-mesh-asset-headless-audit: $(RUNTIME_MESH_ASSET_HEADLESS_AUDIT_TEST_BIN)
	@$(RUNTIME_MESH_ASSET_HEADLESS_AUDIT_TEST_BIN) || (echo "ray tracing runtime mesh asset headless audit test failed."; exit 1)
	@echo "ray tracing runtime mesh asset headless audit lane passed"

test-line-drawing-imported-mesh-runtime: $(RUNTIME_MESH_ASSET_HEADLESS_AUDIT_TEST_BIN)
	@$(MAKE) -C ../line_drawing imported_mesh_harness >/dev/null
	@rm -rf tmp/line_drawing_imported_mesh_runtime
	@../line_drawing/build/toolchains/clang/bin/imported_mesh_harness \
		--stl ../line_drawing/third_party/codework_shared/core/core_mesh_compile/tests/fixtures/imports/tetrahedron_ascii.stl \
		--out tmp/line_drawing_imported_mesh_runtime \
		--asset-id asset_imported_tetrahedron_line_harness \
		--scene-id scene_line_drawing_imported_tetrahedron_for_ray_tracing \
		--object-id obj_imported_tetrahedron_harness >/dev/null
	@RAY_TRACING_LINE_DRAWING_IMPORTED_MESH_SCENE=tmp/line_drawing_imported_mesh_runtime/scene_runtime.json \
		$(RUNTIME_MESH_ASSET_HEADLESS_AUDIT_TEST_BIN) || \
		(echo "ray tracing line drawing generated imported mesh runtime test failed."; exit 1)
	@echo "ray tracing line drawing generated imported mesh runtime lane passed"

test-ray-tracing-publish-helper-validation:
	tests/integration/run_ray_tracing_publish_helper_validation.sh

test-ray-tracing-repo-doc-redaction:
	tests/integration/run_ray_tracing_repo_doc_redaction.sh

test-ray-tracing-linux-worker-package-validator:
	python3 tests/integration/run_ray_tracing_linux_worker_package_validator.py

test-ray-tracing-caustic-probe-matrix: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tests/integration/run_ray_tracing_caustic_probe_matrix.py

test-ray-tracing-spatial-caustic-phase4-matrix: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tests/integration/run_ray_tracing_spatial_caustic_phase4_matrix.py

test-ray-tracing-spatial-caustic-phase6-surface-matrix: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tests/integration/run_ray_tracing_spatial_caustic_phase6_surface_matrix.py

test-ray-tracing-spatial-caustic-phase7-calibration-matrix: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tests/integration/run_ray_tracing_spatial_caustic_phase7_calibration_matrix.py

test-ray-tracing-spatial-caustic-phase8-receiver-policy-matrix: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tests/integration/run_ray_tracing_spatial_caustic_phase8_receiver_policy_matrix.py

test-ray-tracing-spatial-caustic-phase9-transmitted-receiver-matrix: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tests/integration/run_ray_tracing_spatial_caustic_phase9_transmitted_receiver_matrix.py

test-ray-tracing-spatial-caustic-phase10-tangent-receiver-matrix: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tests/integration/run_ray_tracing_spatial_caustic_phase10_tangent_receiver_matrix.py

test-ray-tracing-spatial-caustic-visual-sphere-mist-matrix: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tests/integration/run_ray_tracing_spatial_caustic_visual_sphere_mist_matrix.py

test-ray-tracing-spatial-caustic-funnel-fixture-matrix: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tests/integration/run_ray_tracing_spatial_caustic_funnel_fixture_matrix.py

test-ray-tracing-spatial-caustic-cylinder-lens-fixture: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tests/integration/run_ray_tracing_spatial_caustic_cylinder_lens_fixture.py --debug-export

test-ray-tracing-spatial-caustic-prism-lens-fixture: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tests/integration/run_ray_tracing_spatial_caustic_prism_lens_fixture.py --debug-export

test-ray-tracing-spatial-caustic-bowl-lens-fixture: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tests/integration/run_ray_tracing_spatial_caustic_bowl_lens_fixture.py --debug-export

test-ray-tracing-spatial-caustic-mesh-dielectric-lens-fixture: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tests/integration/run_ray_tracing_spatial_caustic_mesh_dielectric_lens_fixture.py --debug-export

test-ray-tracing-spatial-caustic-imported-lens-wall-preview: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tests/integration/run_ray_tracing_spatial_caustic_imported_lens_wall_preview.py --debug-export

test-ray-tracing-spatial-caustic-imported-lens-distance-matrix: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tests/integration/run_ray_tracing_spatial_caustic_imported_lens_distance_matrix.py --debug-export

test-ray-tracing-spatial-caustic-plano-convex-lens-distance-matrix: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tests/integration/run_ray_tracing_spatial_caustic_plano_convex_lens_distance_matrix.py --debug-export

test-ray-tracing-spatial-caustic-plano-convex-heatmap-diagnostic: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tests/integration/run_ray_tracing_spatial_caustic_plano_convex_heatmap_diagnostic.py --debug-export

test-ray-tracing-spatial-caustic-lens-shape-comparison: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tests/integration/run_ray_tracing_spatial_caustic_lens_shape_comparison.py --debug-export

test-ray-tracing-spatial-caustic-lens-focal-sweep-diagnostic: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tests/integration/run_ray_tracing_spatial_caustic_lens_focal_sweep_diagnostic.py --debug-export

test-ray-tracing-spatial-caustic-ball-lens-focal-crossing: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tests/integration/run_ray_tracing_spatial_caustic_ball_lens_focal_crossing.py --debug-export

test-ray-tracing-ppm10-product-ab-fixture: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tests/integration/run_ray_tracing_ppm10_product_ab_fixture.py

test-ray-tracing-spatial-caustic-authored-validation-matrix: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tests/integration/run_ray_tracing_spatial_caustic_authored_validation_matrix.py

test-ray-tracing-emissive-light-preview-matrix: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tests/integration/run_ray_tracing_emissive_light_preview_matrix.py

test-ray-tracing-release-contract-redaction:
	tests/integration/run_ray_tracing_release_contract_redaction.sh

RUNTIME_TRIANGLE_BVH_3D_TEST_BIN := $(BUILD_DIR)/tests/runtime_triangle_bvh_3d_test
RUNTIME_TRIANGLE_BVH_3D_TEST_SRCS := \
	$(TEST_DIR)/test_runtime_triangle_bvh_3d.c \
	$(TEST_DIR)/test_runtime_material_payload_stub.c \
	$(SRC_DIR)/render/runtime_ray_3d.c \
	$(SRC_DIR)/render/runtime_scene_3d.c \
	$(SRC_DIR)/render/runtime_emissive_light_set_3d.c \
	$(SRC_DIR)/render/runtime_light_set_3d.c \
	$(SRC_DIR)/render/runtime_environment_3d.c \
	$(SRC_DIR)/render/runtime_triangle_bvh_3d.c \
	$(SRC_DIR)/render/runtime_triangle_bvh_trace_3d.c \
	$(SRC_DIR)/render/runtime_volume_3d.c

$(RUNTIME_TRIANGLE_BVH_3D_TEST_BIN): $(RUNTIME_TRIANGLE_BVH_3D_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Wno-unknown-attributes -Wno-c23-extensions -g \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-o $@ $(RUNTIME_TRIANGLE_BVH_3D_TEST_SRCS) -lm

test-runtime-triangle-bvh-3d: $(RUNTIME_TRIANGLE_BVH_3D_TEST_BIN)
	@$(RUNTIME_TRIANGLE_BVH_3D_TEST_BIN) || (echo "ray tracing runtime triangle BVH test failed."; exit 1)
	@echo "ray tracing runtime triangle BVH lane passed"

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

RAY_TRACING_RUNTIME_HOST_LIFECYCLE_TEST_BIN := $(BUILD_DIR)/tests/ray_tracing_runtime_host_lifecycle_contract_test
RAY_TRACING_RUNTIME_HOST_LIFECYCLE_TEST_SRCS := \
	$(TEST_DIR)/ray_tracing_runtime_host_lifecycle_contract_test.c \
	$(SRC_DIR)/app/ray_tracing_runtime_host.c

$(RAY_TRACING_RUNTIME_HOST_LIFECYCLE_TEST_BIN): $(RAY_TRACING_RUNTIME_HOST_LIFECYCLE_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -g -DUSE_VULKAN=0 \
		$(SDL_CFLAGS) $(TIMER_HUD_INCLUDE) -I$(INC_DIR) -I$(SRC_DIR) -I$(VK_RENDERER_DIR)/include \
		-o $@ $(RAY_TRACING_RUNTIME_HOST_LIFECYCLE_TEST_SRCS) -lm

test-ray-tracing-runtime-host-lifecycle-contract: $(RAY_TRACING_RUNTIME_HOST_LIFECYCLE_TEST_BIN)
	@$(RAY_TRACING_RUNTIME_HOST_LIFECYCLE_TEST_BIN) || (echo "ray tracing runtime host lifecycle contract test failed."; exit 1)

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

SCENE_EDITOR_VIEWPORT_NAV_TEST_BIN := $(BUILD_DIR)/tests/scene_editor_viewport_nav_contract_test
SCENE_EDITOR_VIEWPORT_NAV_TEST_SRC := $(TEST_DIR)/scene_editor_viewport_nav_contract_test.c

$(SCENE_EDITOR_VIEWPORT_NAV_TEST_BIN): $(SCENE_EDITOR_VIEWPORT_NAV_TEST_SRC) $(INC_DIR)/editor/scene_editor_viewport_nav_math.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -g -I$(INC_DIR) \
		-o $@ $(SCENE_EDITOR_VIEWPORT_NAV_TEST_SRC) -lm

test-scene-editor-viewport-nav-contract: $(SCENE_EDITOR_VIEWPORT_NAV_TEST_BIN)
	@$(SCENE_EDITOR_VIEWPORT_NAV_TEST_BIN) || (echo "scene editor viewport navigation contract test failed."; exit 1)

SCENE_EDITOR_VIEWPORT3D_BRIDGE_TEST_BIN := $(BUILD_DIR)/tests/scene_editor_viewport3d_bridge_test
SCENE_EDITOR_VIEWPORT3D_BRIDGE_TEST_SRCS := \
	$(TEST_DIR)/scene_editor_viewport3d_bridge_test.c \
	$(SRC_DIR)/editor/scene_editor_viewport3d_bridge.c \
	$(CORE_VIEWPORT3D_DIR)/src/core_viewport3d.c \
	$(CORE_BASE_DIR)/src/core_base.c

$(SCENE_EDITOR_VIEWPORT3D_BRIDGE_TEST_BIN): $(SCENE_EDITOR_VIEWPORT3D_BRIDGE_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -g \
		-I$(INC_DIR) -I$(CORE_VIEWPORT3D_DIR)/include -I$(CORE_BASE_DIR)/include \
		-o $@ $(SCENE_EDITOR_VIEWPORT3D_BRIDGE_TEST_SRCS) -lm

test-scene-editor-viewport3d-bridge-contract: $(SCENE_EDITOR_VIEWPORT3D_BRIDGE_TEST_BIN)
	@$(SCENE_EDITOR_VIEWPORT3D_BRIDGE_TEST_BIN) || (echo "scene editor viewport3d bridge test failed."; exit 1)

MENU_PANE_HOST_TEST_BIN := $(BUILD_DIR)/tests/menu_pane_host_contract_test
MENU_PANE_HOST_TEST_SRCS := \
	$(TEST_DIR)/menu_pane_host_contract_test.c \
	$(TEST_DIR)/kit_render_backend_vk_stub.c \
	$(SRC_DIR)/ui/menu/menu_pane_host.c \
	$(CORE_PANE_DIR)/src/core_pane.c \
	$(KIT_PANE_DIR)/src/kit_pane.c \
	$(KIT_RENDER_DIR)/src/kit_render.c \
	$(KIT_RENDER_DIR)/src/kit_render_backend_null.c \
	$(CORE_THEME_DIR)/src/core_theme.c \
	$(CORE_FONT_DIR)/src/core_font.c \
	$(CORE_BASE_DIR)/src/core_base.c

$(MENU_PANE_HOST_TEST_BIN): $(MENU_PANE_HOST_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -g -DKIT_RENDER_ENABLE_VK_BACKEND=0 \
		-I$(INC_DIR) -I$(SRC_DIR) -I$(CORE_PANE_DIR)/include -I$(KIT_PANE_DIR)/include -I$(KIT_RENDER_DIR)/include -I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include -I$(CORE_BASE_DIR)/include \
		-o $@ $(MENU_PANE_HOST_TEST_SRCS) -lm

test-menu-pane-host-contract: $(MENU_PANE_HOST_TEST_BIN)
	@$(MENU_PANE_HOST_TEST_BIN) || (echo "menu pane host contract test failed."; exit 1)

test-ray-tracing-render-headless-preflight: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	tests/integration/run_ray_tracing_render_headless_preflight.sh

test-ray-tracing-render-headless-image-export: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	tests/integration/run_ray_tracing_render_headless_image_export.sh

test-ray-tracing-render-headless-mesh-asset-spheres: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	tests/integration/run_ray_tracing_render_headless_mesh_asset_spheres.sh

test-optic-build-week-showcase: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	tests/integration/run_optic_build_week_showcase.sh

test-ray-tracing-render-headless-mesh-asset-sphere-pressure: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	tests/integration/run_ray_tracing_render_headless_mesh_asset_sphere_pressure.sh

test-ray-tracing-render-headless-mesh-asset-sphere-pressure-mrt8: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	tests/integration/run_ray_tracing_render_headless_mesh_asset_sphere_pressure_mrt8.sh

test-ray-tracing-render-headless-mesh-asset-sphere-pressure-mrt10: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	tests/integration/run_ray_tracing_render_headless_mesh_asset_sphere_pressure_mrt10.sh

test-ray-tracing-render-headless-mesh-asset-sphere-pressure-mrt12-static-cache: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	tests/integration/run_ray_tracing_render_headless_mesh_asset_sphere_pressure_mrt12_static_cache.sh

test-ray-tracing-render-headless-tlas-blas-repeated-instance-stress: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tools/run_tlas_blas_repeated_instance_stress.py --output-root build/agent_runs/ray_tracing --run-id tlas_blas_repeated_instance_stress

test-ray-tracing-tile-adaptive-t5-matrix: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tests/integration/run_ray_tracing_tile_adaptive_visual_metric_matrix.py --keep-going

test-ray-tracing-render-headless-line-drawing-mesh-asset: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	tests/integration/run_ray_tracing_render_headless_line_drawing_mesh_asset.sh

test-ray-tracing-render-headless-volume-handoff: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	tests/integration/run_ray_tracing_render_headless_volume_handoff.sh

test-ray-tracing-render-headless-water-surface-handoff: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	tests/integration/run_ray_tracing_render_headless_water_surface_handoff.sh

test-ray-tracing-render-headless-water-optics-review: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	tests/integration/run_ray_tracing_render_headless_water_optics_review.sh

test-ray-tracing-render-headless-water-basin-surface-review: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	tests/integration/run_ray_tracing_render_headless_water_basin_surface_review.sh

test-ray-tracing-render-headless-water-moving-light-review: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	tests/integration/run_ray_tracing_render_headless_water_moving_light_review.sh

test-ray-tracing-render-headless-water-long-motion-review: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	tests/integration/run_ray_tracing_render_headless_water_long_motion_review.sh

test-ray-tracing-render-headless-water-object-coupling-review: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	bash tests/integration/run_ray_tracing_render_headless_water_object_coupling_review.sh

test-ray-tracing-render-headless-water-object-coupling-long-review: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	bash tests/integration/run_ray_tracing_render_headless_water_object_coupling_long_review.sh

test-ray-tracing-job-runner-smoke: $(RAY_TRACING_RENDER_HEADLESS_BIN) $(RAY_TRACING_JOB_RUNNER_BIN)
	tests/integration/run_ray_tracing_job_runner_smoke.sh

test-ray-tracing-job-runner-bundle-smoke: $(RAY_TRACING_RENDER_HEADLESS_BIN) $(RAY_TRACING_JOB_RUNNER_BIN)
	tests/integration/run_ray_tracing_job_runner_bundle_smoke.sh

test-ray-tracing-job-runner-policy: $(RAY_TRACING_RENDER_HEADLESS_BIN) $(RAY_TRACING_JOB_RUNNER_BIN)
	tests/integration/run_ray_tracing_job_runner_policy.sh

test-ray-tracing-wtr66-preview-matrix-planner-dry-run:
	bash tests/integration/run_ray_tracing_wtr66_preview_matrix_planner_dry_run.sh

test-ray-tracing-wtr66-preview-matrix-local-job-runner: $(RAY_TRACING_RENDER_HEADLESS_BIN) $(RAY_TRACING_JOB_RUNNER_BIN)
	bash tests/integration/run_ray_tracing_wtr66_preview_matrix_local_job_runner.sh

test-ray-tracing-material-preview-headless: $(RAY_TRACING_MATERIAL_PREVIEW_HEADLESS_BIN)
	tests/integration/run_ray_tracing_material_preview_headless.sh

test-ray-tracing-material-family-preview-grid: $(RAY_TRACING_MATERIAL_PREVIEW_HEADLESS_BIN)
	python3 tests/integration/run_ray_tracing_material_family_preview_grid.py

test-ray-tracing-material-layer-control-preview-grid: $(RAY_TRACING_MATERIAL_PREVIEW_HEADLESS_BIN)
	python3 tests/integration/run_ray_tracing_material_layer_control_preview_grid.py --publish-docs

test-ray-tracing-material-stack-structure-proof-grid:
	python3 tests/integration/run_ray_tracing_material_stack_structure_proof_grid.py --publish-docs

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
