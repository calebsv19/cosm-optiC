STABLE_TEST_TARGETS := \
	test \
	test-procedural-surface-recipe-contract \
	test-procedural-surface-field-contract \
	test-procedural-surface-plane-contract \
	test-procedural-surface-prism-contract \
	test-procedural-surface-material-contract \
	test-procedural-surface-derived-asset-contract \
	test-procedural-surface-graph-contract \
	test-procedural-surface-field-graph-contract \
	test-procedural-surface-feature-field-contract \
	test-procedural-surface-feature-curve-contract \
	test-procedural-surface-feature-selection-contract \
	test-procedural-surface-authoring-contract \
	test-procedural-surface-authoring-document-contract \
	test-procedural-surface-binding-contract \
	test-procedural-surface-terrain-contract \
	test-procedural-surface-selected-face-shell-contract \
	test-procedural-surface-feature-relief-shell-contract \
	test-procedural-surface-feature-relief-compiler-contract \
	test-procedural-surface-shell-contract \
	test-procedural-solid-contract \
	test-procedural-solid-authoring \
	test-procedural-solid-psg11 \
	test-procedural-solid-psg12 \
	test-procedural-solid-agent-flow \
	test-procedural-solid-psg11-flow \
	test-procedural-solid-psg12-flow \
	test-procedural-solid-psg13-flow \
	test-procedural-solid-psg14-flow \
	test-procedural-solid-material-graph \
	test-procedural-solid-psg15-flow \
	test-procedural-solid-material-runtime \
	test-procedural-solid-psg16b-visual-contract \
	test-procedural-solid-psg17-visual-contract \
	test-procedural-solid-psg14-visual-proof \
	test-procedural-solid-psg13-visual-proof \
	test-procedural-solid-psg12-visual-proof \
	test-procedural-surface-visual-proof \
	test-procedural-surface-field-preset-visual-proof \
	test-procedural-surface-binding-visual-proof \
	test-procedural-surface-terrain-visual-proof \
	test-procedural-surface-shell-visual-proof \
	test-procedural-surface-agent-iteration \
	test-starter-scene-profile-contract \
	test-ray-tracing-triangle-topology-stability \
	test-runtime-scene-bridge-contract \
	test-runtime-mesh-asset-loader \
	test-procedural-imported-surface-strands-psg23b \
	test-procedural-imported-surface-strands-psg23c \
	test-procedural-imported-surface-strands-psg23d \
	test-procedural-imported-surface-strands-psg23e \
	test-procedural-imported-surface-strands-psg23f \
	test-procedural-imported-surface-strands-psg23g \
	test-scene-editor-mesh-preview-outline \
	test-scene-editor-mesh-preview-shading \
	test-scene-editor-primitive-preview-geometry \
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
	test-ray-tracing-artifact-comparison \
	test-ray-tracing-render-headless-mesh-asset-spheres \
	test-ray-tracing-render-headless-mesh-asset-sphere-pressure \
	test-ray-tracing-render-headless-mesh-asset-sphere-pressure-mrt8 \
	test-ray-tracing-render-headless-mesh-asset-sphere-pressure-mrt10 \
	test-ray-tracing-render-headless-mesh-asset-sphere-pressure-mrt12-static-cache \
	test-ray-tracing-render-headless-tlas-blas-repeated-instance-stress \
	test-ray-tracing-material-preview-headless \
	test-ray-tracing-job-runner-smoke \
		test-ray-tracing-job-runner-bundle-smoke \
		test-ray-tracing-durable-frame-recovery \
		test-ray-tracing-recovery-authority \
		test-ray-tracing-worker-protocol \
		test-ray-tracing-worker-version-contract \
	test-ray-tracing-worker-protocol-phase-b \
		test-ray-tracing-temporal-checkpoint-phase-c \
		test-ray-tracing-tile-batch-checkpoint-phase-d \
		test-ray-tracing-fleet-recovery-phase-e \
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

PROCEDURAL_SOLID_TEST_BIN := \
	$(BUILD_DIR)/tests/procedural_solid_test
PROCEDURAL_SOLID_COMMON_SRCS := \
	$(SRC_DIR)/procedural/procedural_solid_graph.c \
	$(SRC_DIR)/procedural/procedural_solid_graph_json.c \
	$(SRC_DIR)/procedural/procedural_solid_graph_eval.c \
	$(SRC_DIR)/procedural/procedural_solid_field_query.c \
	$(SRC_DIR)/procedural/procedural_solid_source_accel.c \
	$(SRC_DIR)/procedural/procedural_solid_mesh.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset_runtime_document.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_MESH_ASSET_DIR)/../../shape/external/cjson/cJSON.c
PROCEDURAL_SOLID_TEST_SRCS := \
	$(TEST_DIR)/test_procedural_solid.c \
	$(SRC_DIR)/procedural/procedural_surface_shell.c \
	$(SRC_DIR)/procedural/procedural_surface_binding.c \
	$(SRC_DIR)/procedural/procedural_surface_field_graph.c \
	$(SRC_DIR)/procedural/procedural_surface_field_graph_json.c \
	$(SRC_DIR)/procedural/procedural_surface_field_graph_eval.c \
	$(SRC_DIR)/procedural/procedural_surface_field_graph_noise.c \
	$(PROCEDURAL_SOLID_COMMON_SRCS)

PROCEDURAL_SOLID_AUTHORING_TEST_BIN := \
	$(BUILD_DIR)/tests/procedural_solid_authoring_test
PROCEDURAL_SOLID_AUTHORING_TEST_SRCS := \
	$(TEST_DIR)/test_procedural_solid_authoring.c \
	$(SRC_DIR)/procedural/procedural_solid_authoring.c \
	$(SRC_DIR)/procedural/procedural_solid_remesh.c \
	$(PROCEDURAL_SOLID_COMMON_SRCS)

PROCEDURAL_SOLID_PSG11_TEST_BIN := \
	$(BUILD_DIR)/tests/procedural_solid_psg11_test
PROCEDURAL_SOLID_PSG11_TEST_SRCS := \
	$(TEST_DIR)/test_procedural_solid_psg11.c \
	$(SRC_DIR)/procedural/procedural_solid_feature.c \
	$(SRC_DIR)/procedural/procedural_solid_regions.c \
	$(SRC_DIR)/procedural/procedural_solid_local_remesh.c \
	$(SRC_DIR)/procedural/procedural_solid_crease.c \
	$(SRC_DIR)/procedural/procedural_solid_shading.c \
	$(SRC_DIR)/procedural/procedural_solid_quality.c \
	$(PROCEDURAL_SOLID_COMMON_SRCS)

PROCEDURAL_SOLID_PSG12_TEST_BIN := \
	$(BUILD_DIR)/tests/procedural_solid_psg12_test
PROCEDURAL_SOLID_PSG12_TEST_SRCS := \
	$(TEST_DIR)/test_procedural_solid_psg12.c \
	$(SRC_DIR)/procedural/procedural_solid_feature.c \
	$(SRC_DIR)/procedural/procedural_solid_regions.c \
	$(SRC_DIR)/procedural/procedural_solid_local_remesh.c \
	$(SRC_DIR)/procedural/procedural_solid_crease.c \
	$(SRC_DIR)/procedural/procedural_solid_shading.c \
	$(SRC_DIR)/procedural/procedural_solid_quality.c \
	$(PROCEDURAL_SOLID_COMMON_SRCS)

$(PROCEDURAL_SOLID_TEST_BIN): \
	$(PROCEDURAL_SOLID_TEST_SRCS) \
	$(INC_DIR)/procedural/procedural_solid_graph.h \
	$(INC_DIR)/procedural/procedural_solid_mesh.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror \
		-Wno-unknown-attributes -Wno-c23-extensions -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_MESH_ASSET_DIR)/include -I$(CORE_IO_DIR)/include \
		-I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-I$(CORE_BASE_DIR)/include \
		-o $@ $(PROCEDURAL_SOLID_TEST_SRCS) $(JSON_LIBS) -lm

test-procedural-solid-contract: $(PROCEDURAL_SOLID_TEST_BIN)
	@$(PROCEDURAL_SOLID_TEST_BIN)

$(PROCEDURAL_SOLID_PSG11_TEST_BIN): \
	$(PROCEDURAL_SOLID_PSG11_TEST_SRCS) \
	$(INC_DIR)/procedural/procedural_solid_local_remesh.h \
	$(INC_DIR)/procedural/procedural_solid_source_accel.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror \
		-Wno-unknown-attributes -Wno-c23-extensions -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_MESH_ASSET_DIR)/include -I$(CORE_IO_DIR)/include \
		-I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-I$(CORE_BASE_DIR)/include \
		-o $@ $(PROCEDURAL_SOLID_PSG11_TEST_SRCS) $(JSON_LIBS) -lm

test-procedural-solid-psg11: $(PROCEDURAL_SOLID_PSG11_TEST_BIN)
	@$(PROCEDURAL_SOLID_PSG11_TEST_BIN)

$(PROCEDURAL_SOLID_PSG12_TEST_BIN): \
	$(PROCEDURAL_SOLID_PSG12_TEST_SRCS) \
	$(INC_DIR)/procedural/procedural_solid_quality.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror \
		-Wno-unknown-attributes -Wno-c23-extensions -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-Ithird_party/codework_shared/core/core_mesh_asset/include \
		-Ithird_party/codework_shared/core/core_io/include \
		-Ithird_party/codework_shared/core/core_object/include \
		-Ithird_party/codework_shared/core/core_units/include \
		-Ithird_party/codework_shared/core/core_base/include \
		-o $@ $(PROCEDURAL_SOLID_PSG12_TEST_SRCS) $(JSON_LIBS) -lm

test-procedural-solid-psg12: $(PROCEDURAL_SOLID_PSG12_TEST_BIN)
	@$(PROCEDURAL_SOLID_PSG12_TEST_BIN)

$(PROCEDURAL_SOLID_AUTHORING_TEST_BIN): \
	$(PROCEDURAL_SOLID_AUTHORING_TEST_SRCS) \
	$(INC_DIR)/procedural/procedural_solid_authoring.h \
	$(INC_DIR)/procedural/procedural_solid_remesh.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror \
		-Wno-unknown-attributes -Wno-c23-extensions -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_MESH_ASSET_DIR)/include -I$(CORE_IO_DIR)/include \
		-I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-I$(CORE_BASE_DIR)/include \
		-o $@ $(PROCEDURAL_SOLID_AUTHORING_TEST_SRCS) $(JSON_LIBS) -lm

test-procedural-solid-authoring: $(PROCEDURAL_SOLID_AUTHORING_TEST_BIN)
	@$(PROCEDURAL_SOLID_AUTHORING_TEST_BIN)

PROCEDURAL_SURFACE_RECIPE_TEST_BIN := \
	$(BUILD_DIR)/tests/procedural_surface_recipe_test
PROCEDURAL_SURFACE_RECIPE_TEST_SRCS := \
	$(TEST_DIR)/test_procedural_surface_recipe.c \
	$(SRC_DIR)/procedural/procedural_surface_recipe.c \
	$(SRC_DIR)/procedural/procedural_surface_topology_contract.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c

$(PROCEDURAL_SURFACE_RECIPE_TEST_BIN): \
	$(PROCEDURAL_SURFACE_RECIPE_TEST_SRCS) \
	$(INC_DIR)/procedural/procedural_surface_recipe.h \
	$(INC_DIR)/procedural/procedural_surface_topology_contract.h \
	$(INC_DIR)/app/ray_tracing_sha256.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-o $@ $(PROCEDURAL_SURFACE_RECIPE_TEST_SRCS) $(JSON_LIBS) -lm

test-procedural-surface-recipe-contract: \
	$(PROCEDURAL_SURFACE_RECIPE_TEST_BIN)
	@$(PROCEDURAL_SURFACE_RECIPE_TEST_BIN)

PROCEDURAL_SURFACE_FIELD_TEST_BIN := \
	$(BUILD_DIR)/tests/procedural_surface_field_test
PROCEDURAL_SURFACE_FIELD_TEST_SRCS := \
	$(TEST_DIR)/test_procedural_surface_field_3d.c \
	$(SRC_DIR)/procedural/procedural_surface_field_3d.c \
	$(SRC_DIR)/procedural/procedural_surface_recipe.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c

$(PROCEDURAL_SURFACE_FIELD_TEST_BIN): \
	$(PROCEDURAL_SURFACE_FIELD_TEST_SRCS) \
	$(INC_DIR)/procedural/procedural_surface_field_3d.h \
	$(INC_DIR)/procedural/procedural_surface_recipe.h \
	$(INC_DIR)/app/ray_tracing_sha256.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-o $@ $(PROCEDURAL_SURFACE_FIELD_TEST_SRCS) $(JSON_LIBS) -lm

test-procedural-surface-field-contract: \
	$(PROCEDURAL_SURFACE_FIELD_TEST_BIN)
	@$(PROCEDURAL_SURFACE_FIELD_TEST_BIN)

PROCEDURAL_SURFACE_PLANE_TEST_BIN := \
	$(BUILD_DIR)/tests/procedural_surface_plane_test
PROCEDURAL_SURFACE_PLANE_TEST_SRCS := \
	$(TEST_DIR)/test_procedural_surface_plane_mesh.c \
	$(SRC_DIR)/procedural/procedural_surface_plane_mesh.c \
	$(SRC_DIR)/procedural/procedural_surface_field_3d.c \
	$(SRC_DIR)/procedural/procedural_surface_recipe.c \
	$(SRC_DIR)/procedural/procedural_surface_topology_contract.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c

$(PROCEDURAL_SURFACE_PLANE_TEST_BIN): \
	$(PROCEDURAL_SURFACE_PLANE_TEST_SRCS) \
	$(INC_DIR)/procedural/procedural_surface_plane_mesh.h \
	$(INC_DIR)/procedural/procedural_surface_field_3d.h \
	$(INC_DIR)/procedural/procedural_surface_recipe.h \
	$(INC_DIR)/procedural/procedural_surface_topology_contract.h \
	$(INC_DIR)/app/ray_tracing_sha256.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-o $@ $(PROCEDURAL_SURFACE_PLANE_TEST_SRCS) $(JSON_LIBS) -lm

test-procedural-surface-plane-contract: \
	$(PROCEDURAL_SURFACE_PLANE_TEST_BIN)
	@$(PROCEDURAL_SURFACE_PLANE_TEST_BIN)

PROCEDURAL_SURFACE_PRISM_TEST_BIN := \
	$(BUILD_DIR)/tests/procedural_surface_prism_test
PROCEDURAL_SURFACE_PRISM_TEST_SRCS := \
	$(TEST_DIR)/test_procedural_surface_prism_mesh.c \
	$(SRC_DIR)/procedural/procedural_surface_prism_mesh.c \
	$(SRC_DIR)/procedural/procedural_surface_mesh_asset_adapter.c \
	$(SRC_DIR)/procedural/procedural_surface_field_3d.c \
	$(SRC_DIR)/procedural/procedural_surface_recipe.c \
	$(SRC_DIR)/procedural/procedural_surface_topology_contract.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset_runtime_document.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_MESH_ASSET_DIR)/../../shape/external/cjson/cJSON.c

$(PROCEDURAL_SURFACE_PRISM_TEST_BIN): \
	$(PROCEDURAL_SURFACE_PRISM_TEST_SRCS) \
	$(INC_DIR)/procedural/procedural_surface_prism_mesh.h \
	$(INC_DIR)/procedural/procedural_surface_mesh_asset_adapter.h
	$(INC_DIR)/procedural/procedural_solid_mesh.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror \
		-Wno-unknown-attributes -Wno-c23-extensions -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		-DPROCEDURAL_SURFACE_FIXTURE_ROOT=\"$(CURDIR)/tests/fixtures/procedural_surface_rock_prism_psg0\" \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_MESH_ASSET_DIR)/include -I$(CORE_IO_DIR)/include \
		-I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-I$(CORE_BASE_DIR)/include \
		-o $@ $(PROCEDURAL_SURFACE_PRISM_TEST_SRCS) $(JSON_LIBS) -lm

test-procedural-surface-prism-contract: \
	$(PROCEDURAL_SURFACE_PRISM_TEST_BIN)
	@$(PROCEDURAL_SURFACE_PRISM_TEST_BIN)

PROCEDURAL_SURFACE_MATERIAL_TEST_BIN := \
	$(BUILD_DIR)/tests/procedural_surface_material_test
PROCEDURAL_SURFACE_MATERIAL_TEST_SRCS := \
	$(TEST_DIR)/test_procedural_surface_material.c \
	$(SRC_DIR)/procedural/procedural_surface_material.c \
	$(SRC_DIR)/procedural/procedural_surface_material_runtime_adapter.c \
	$(SRC_DIR)/procedural/procedural_surface_material_payload_adapter.c \
	$(SRC_DIR)/procedural/procedural_surface_prism_mesh.c \
	$(SRC_DIR)/procedural/procedural_surface_field_3d.c \
	$(SRC_DIR)/procedural/procedural_surface_recipe.c \
	$(SRC_DIR)/procedural/procedural_surface_topology_contract.c \
	$(SRC_DIR)/render/materials/runtime_material_payload_surface_eval_3d.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c

$(PROCEDURAL_SURFACE_MATERIAL_TEST_BIN): \
	$(PROCEDURAL_SURFACE_MATERIAL_TEST_SRCS) \
	$(INC_DIR)/procedural/procedural_surface_material.h \
	$(INC_DIR)/procedural/procedural_surface_material_runtime_adapter.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror \
		-Wno-unknown-attributes -Wno-c23-extensions -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		-DPROCEDURAL_SURFACE_FIXTURE_ROOT=\"$(CURDIR)/tests/fixtures/procedural_surface_rock_prism_psg0\" \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-o $@ $(PROCEDURAL_SURFACE_MATERIAL_TEST_SRCS) $(JSON_LIBS) -lm

test-procedural-surface-material-contract: \
	$(PROCEDURAL_SURFACE_MATERIAL_TEST_BIN)
	@$(PROCEDURAL_SURFACE_MATERIAL_TEST_BIN)

PROCEDURAL_SURFACE_DERIVED_ASSET_TEST_BIN := \
	$(BUILD_DIR)/tests/procedural_surface_derived_asset_test
PROCEDURAL_SURFACE_DERIVED_ASSET_TEST_SRCS := \
	$(TEST_DIR)/test_procedural_surface_derived_asset.c \
	$(SRC_DIR)/procedural/procedural_surface_derived_asset.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c

$(PROCEDURAL_SURFACE_DERIVED_ASSET_TEST_BIN): \
	$(PROCEDURAL_SURFACE_DERIVED_ASSET_TEST_SRCS) \
	$(INC_DIR)/procedural/procedural_surface_derived_asset.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-o $@ $(PROCEDURAL_SURFACE_DERIVED_ASSET_TEST_SRCS) \
		$(JSON_LIBS) -lm

test-procedural-surface-derived-asset-contract: \
	$(PROCEDURAL_SURFACE_DERIVED_ASSET_TEST_BIN)
	@$(PROCEDURAL_SURFACE_DERIVED_ASSET_TEST_BIN)

PROCEDURAL_SURFACE_GRAPH_TEST_BIN := \
	$(BUILD_DIR)/tests/procedural_surface_graph_test
PROCEDURAL_SURFACE_GRAPH_TEST_SRCS := \
	$(TEST_DIR)/test_procedural_surface_graph.c \
	$(SRC_DIR)/procedural/procedural_surface_graph.c \
	$(SRC_DIR)/procedural/procedural_surface_graph_json.c \
	$(SRC_DIR)/procedural/procedural_surface_graph_compile.c \
	$(SRC_DIR)/procedural/procedural_surface_derived_asset.c \
	$(SRC_DIR)/procedural/procedural_surface_material.c \
	$(SRC_DIR)/procedural/procedural_surface_prism_mesh.c \
	$(SRC_DIR)/procedural/procedural_surface_field_3d.c \
	$(SRC_DIR)/procedural/procedural_surface_recipe.c \
	$(SRC_DIR)/procedural/procedural_surface_topology_contract.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c

$(PROCEDURAL_SURFACE_GRAPH_TEST_BIN): \
	$(PROCEDURAL_SURFACE_GRAPH_TEST_SRCS) \
	$(INC_DIR)/procedural/procedural_surface_graph.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		-DPROCEDURAL_SURFACE_FIXTURE_ROOT=\"$(CURDIR)/tests/fixtures/procedural_surface_rock_prism_psg0\" \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-o $@ $(PROCEDURAL_SURFACE_GRAPH_TEST_SRCS) \
		$(JSON_LIBS) -lm

test-procedural-surface-graph-contract: \
	$(PROCEDURAL_SURFACE_GRAPH_TEST_BIN)
	@$(PROCEDURAL_SURFACE_GRAPH_TEST_BIN)

PROCEDURAL_SURFACE_FIELD_GRAPH_TEST_BIN := \
	$(BUILD_DIR)/tests/procedural_surface_field_graph_test
PROCEDURAL_SURFACE_FIELD_GRAPH_TEST_SRCS := \
	$(TEST_DIR)/test_procedural_surface_field_graph.c \
	$(SRC_DIR)/procedural/procedural_surface_field_graph.c \
	$(SRC_DIR)/procedural/procedural_surface_field_graph_json.c \
	$(SRC_DIR)/procedural/procedural_surface_field_graph_eval.c \
	$(SRC_DIR)/procedural/procedural_surface_field_graph_noise.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c

$(PROCEDURAL_SURFACE_FIELD_GRAPH_TEST_BIN): \
	$(PROCEDURAL_SURFACE_FIELD_GRAPH_TEST_SRCS) \
	$(INC_DIR)/procedural/procedural_surface_field_graph.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		-DPROCEDURAL_SURFACE_FIELD_PRESET_ROOT=\"$(CURDIR)/tests/fixtures/procedural_surface_field_presets\" \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-o $@ $(PROCEDURAL_SURFACE_FIELD_GRAPH_TEST_SRCS) \
		$(JSON_LIBS) -lm

test-procedural-surface-field-graph-contract: \
	$(PROCEDURAL_SURFACE_FIELD_GRAPH_TEST_BIN)
	@$(PROCEDURAL_SURFACE_FIELD_GRAPH_TEST_BIN)

PROCEDURAL_SURFACE_FEATURE_FIELD_TEST_BIN := $(BUILD_DIR)/tests/procedural_surface_feature_field_test
$(PROCEDURAL_SURFACE_FEATURE_FIELD_TEST_BIN): $(TEST_DIR)/test_procedural_surface_feature_field.c $(SRC_DIR)/procedural/procedural_surface_feature_field.c $(SRC_DIR)/app/ray_tracing_sha256.c $(INC_DIR)/procedural/procedural_surface_feature_field.h

	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g $(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) -I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include -o $@ $(TEST_DIR)/test_procedural_surface_feature_field.c $(SRC_DIR)/procedural/procedural_surface_feature_field.c $(SRC_DIR)/app/ray_tracing_sha256.c $(CORE_IO_DIR)/src/core_io.c $(CORE_BASE_DIR)/src/core_base.c $(JSON_LIBS) -lm

test-procedural-surface-feature-field-contract: $(PROCEDURAL_SURFACE_FEATURE_FIELD_TEST_BIN)
	@$<

PROCEDURAL_SURFACE_FEATURE_CURVE_TEST_BIN := $(BUILD_DIR)/tests/procedural_surface_feature_curve_test
$(PROCEDURAL_SURFACE_FEATURE_CURVE_TEST_BIN): $(TEST_DIR)/test_procedural_surface_feature_curve.c $(SRC_DIR)/procedural/procedural_surface_feature_curve.c $(SRC_DIR)/app/ray_tracing_sha256.c $(INC_DIR)/procedural/procedural_surface_feature_curve.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g $(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) -o $@ $(TEST_DIR)/test_procedural_surface_feature_curve.c $(SRC_DIR)/procedural/procedural_surface_feature_curve.c $(SRC_DIR)/app/ray_tracing_sha256.c $(JSON_LIBS) -lm
test-procedural-surface-feature-curve-contract: $(PROCEDURAL_SURFACE_FEATURE_CURVE_TEST_BIN)
	@$<
PROCEDURAL_SURFACE_FEATURE_SELECTION_TEST_BIN := $(BUILD_DIR)/tests/procedural_surface_feature_selection_test
$(PROCEDURAL_SURFACE_FEATURE_SELECTION_TEST_BIN): $(TEST_DIR)/test_procedural_surface_feature_selection.c $(SRC_DIR)/procedural/procedural_surface_feature_selection.c $(SRC_DIR)/procedural/procedural_surface_feature_field.c $(SRC_DIR)/procedural/procedural_imported_surface_region.c $(SRC_DIR)/procedural/procedural_solid_mesh.c $(SRC_DIR)/procedural/procedural_solid_graph.c $(SRC_DIR)/procedural/procedural_solid_graph_json.c $(SRC_DIR)/procedural/procedural_solid_graph_eval.c $(SRC_DIR)/procedural/procedural_solid_source_accel.c $(SRC_DIR)/app/ray_tracing_sha256.c
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g $(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) -I$(CORE_MESH_ASSET_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include -o $@ $(TEST_DIR)/test_procedural_surface_feature_selection.c $(SRC_DIR)/procedural/procedural_surface_feature_selection.c $(SRC_DIR)/procedural/procedural_surface_feature_field.c $(SRC_DIR)/procedural/procedural_imported_surface_region.c $(SRC_DIR)/procedural/procedural_solid_mesh.c $(SRC_DIR)/procedural/procedural_solid_graph.c $(SRC_DIR)/procedural/procedural_solid_graph_json.c $(SRC_DIR)/procedural/procedural_solid_graph_eval.c $(SRC_DIR)/procedural/procedural_solid_source_accel.c $(SRC_DIR)/app/ray_tracing_sha256.c $(CORE_MESH_ASSET_DIR)/src/core_mesh_asset.c $(CORE_MESH_ASSET_DIR)/src/core_mesh_asset_runtime_document.c $(CORE_IO_DIR)/src/core_io.c $(CORE_BASE_DIR)/src/core_base.c $(CORE_OBJECT_DIR)/src/core_object.c $(CORE_UNITS_DIR)/src/core_units.c $(CORE_MESH_ASSET_DIR)/../../shape/external/cjson/cJSON.c $(JSON_LIBS) -lm
test-procedural-surface-feature-selection-contract: $(PROCEDURAL_SURFACE_FEATURE_SELECTION_TEST_BIN)
	@$<

PROCEDURAL_SURFACE_FEATURE_SELECTION_TOOL_BIN := \
	$(BUILD_DIR)/tools/cli/procedural_surface_feature_selection_tool
PROCEDURAL_SURFACE_FEATURE_SELECTION_TOOL_SRCS := \
	tools/cli/procedural_surface_feature_selection_tool.c \
	$(SRC_DIR)/procedural/procedural_surface_feature_selection.c \
	$(SRC_DIR)/procedural/procedural_surface_feature_field.c \
	$(SRC_DIR)/procedural/procedural_surface_wood_grain.c \
	$(SRC_DIR)/procedural/procedural_imported_surface_region.c \
	$(SRC_DIR)/procedural/procedural_solid_mesh.c \
	$(SRC_DIR)/procedural/procedural_solid_graph.c \
	$(SRC_DIR)/procedural/procedural_solid_graph_json.c \
	$(SRC_DIR)/procedural/procedural_solid_graph_eval.c \
	$(SRC_DIR)/procedural/procedural_solid_source_accel.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset_runtime_document.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_MESH_ASSET_DIR)/../../shape/external/cjson/cJSON.c

$(PROCEDURAL_SURFACE_FEATURE_SELECTION_TOOL_BIN): $(PROCEDURAL_SURFACE_FEATURE_SELECTION_TOOL_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_MESH_ASSET_DIR)/include -I$(CORE_IO_DIR)/include \
		-I$(CORE_BASE_DIR)/include -I$(CORE_OBJECT_DIR)/include \
		-I$(CORE_UNITS_DIR)/include -o $@ \
		$(PROCEDURAL_SURFACE_FEATURE_SELECTION_TOOL_SRCS) $(JSON_LIBS) -lm

procedural-surface-feature-selection-tool: $(PROCEDURAL_SURFACE_FEATURE_SELECTION_TOOL_BIN)
	@echo "procedural surface feature selection tool ready: $<"

PROCEDURAL_SOLID_MESH_DIGEST_TOOL_BIN := $(BUILD_DIR)/tools/cli/procedural_solid_mesh_digest_tool
PROCEDURAL_SOLID_MESH_DIGEST_TOOL_SRCS := \
	tools/cli/procedural_solid_mesh_digest_tool.c \
	$(SRC_DIR)/procedural/procedural_solid_mesh.c \
	$(SRC_DIR)/procedural/procedural_solid_graph.c \
	$(SRC_DIR)/procedural/procedural_solid_graph_json.c \
	$(SRC_DIR)/procedural/procedural_solid_graph_eval.c \
	$(SRC_DIR)/procedural/procedural_solid_source_accel.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset_runtime_document.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_MESH_ASSET_DIR)/../../shape/external/cjson/cJSON.c
$(PROCEDURAL_SOLID_MESH_DIGEST_TOOL_BIN): $(PROCEDURAL_SOLID_MESH_DIGEST_TOOL_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g $(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_MESH_ASSET_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_OBJECT_DIR)/include \
		-I$(CORE_UNITS_DIR)/include -I$(CORE_BASE_DIR)/include -o $@ \
		$(PROCEDURAL_SOLID_MESH_DIGEST_TOOL_SRCS) $(JSON_LIBS) -lm
procedural-solid-mesh-digest-tool: $(PROCEDURAL_SOLID_MESH_DIGEST_TOOL_BIN)
	@echo "procedural solid mesh digest tool ready: $<"

PROCEDURAL_SURFACE_AUTHORING_COMMON_SRCS := \
	$(SRC_DIR)/procedural/procedural_surface_field_graph.c \
	$(SRC_DIR)/procedural/procedural_surface_field_graph_json.c \
	$(SRC_DIR)/procedural/procedural_surface_field_graph_eval.c \
	$(SRC_DIR)/procedural/procedural_surface_field_graph_noise.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c

PROCEDURAL_SURFACE_AUTHORING_TEST_BIN := \
	$(BUILD_DIR)/tests/procedural_surface_authoring_test
PROCEDURAL_SURFACE_AUTHORING_TEST_SRCS := \
	$(TEST_DIR)/test_procedural_surface_authoring.c \
	$(SRC_DIR)/procedural/procedural_surface_authoring.c \
	$(PROCEDURAL_SURFACE_AUTHORING_COMMON_SRCS) \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_BASE_DIR)/src/core_base.c

$(PROCEDURAL_SURFACE_AUTHORING_TEST_BIN): \
	$(PROCEDURAL_SURFACE_AUTHORING_TEST_SRCS) \
	$(INC_DIR)/procedural/procedural_surface_authoring.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		-DPROCEDURAL_SURFACE_FIELD_PRESET_ROOT=\"$(CURDIR)/tests/fixtures/procedural_surface_field_presets\" \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include \
		-o $@ $(PROCEDURAL_SURFACE_AUTHORING_TEST_SRCS) \
		$(JSON_LIBS) -lm

test-procedural-surface-authoring-contract: \
	$(PROCEDURAL_SURFACE_AUTHORING_TEST_BIN) \
	test-procedural-surface-authoring-contract-matrix
	@$(PROCEDURAL_SURFACE_AUTHORING_TEST_BIN)

PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_TEST_BIN := \
	$(BUILD_DIR)/tests/procedural_surface_authoring_document_test
PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_TEST_SRCS := \
	$(TEST_DIR)/test_procedural_surface_authoring_document.c \
	$(SRC_DIR)/procedural/procedural_surface_authoring_document.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c

$(PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_TEST_BIN): \
	$(PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_TEST_SRCS) \
	$(INC_DIR)/procedural/procedural_surface_authoring_document.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-o $@ $(PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_TEST_SRCS) \
		$(JSON_LIBS) -lm

test-procedural-surface-authoring-document-contract: \
	$(PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_TEST_BIN) \
	test-procedural-surface-authoring-document-adapter-contract \
	test-procedural-surface-authoring-document-canvas-visual
	@$(PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_TEST_BIN)

PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_TOOL_BIN := \
	$(BUILD_DIR)/tools/cli/procedural_surface_authoring_document_tool
PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_TOOL_SRCS := \
	tools/cli/procedural_surface_authoring_document_tool.c \
	$(SRC_DIR)/procedural/procedural_surface_authoring_document.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c

$(PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_TOOL_BIN): \
	$(PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_TOOL_SRCS) \
	$(INC_DIR)/procedural/procedural_surface_authoring_document.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-o $@ $(PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_TOOL_SRCS) \
		$(JSON_LIBS) -lm

test-procedural-surface-authoring-document-adapter-contract: \
	$(PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_TOOL_BIN)
	@set -e; \
	tmp="$$(mktemp -d /tmp/surface-authoring-document.XXXXXX)"; \
	trap 'rm -rf "$$tmp"' EXIT; \
	fixture="tests/fixtures/procedural_surface_authoring_document_v1/cube_composition.json"; \
	created="$$tmp/created.json"; edited="$$tmp/edited.json"; \
	$(PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_TOOL_BIN) create \
		--output "$$created" --document-id cube_surface_v1 --source-object-id cube \
		--source-mesh-digest aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa \
		--material brown_umber_mix_basecolor bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb material,microdetail_normal \
		--surface-field dirt_noise_microdetail_normal cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc microdetail_normal \
		--face-selector positive_z_face_selector dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd material,attached_asset \
		--attachment grass_attachment eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee attached_asset >/dev/null; \
	python3 -c 'import json,sys; a=json.load(open(sys.argv[1])); b=json.load(open(sys.argv[2])); assert a==b' "$$created" "$$fixture"; \
	readback="$$($(PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_TOOL_BIN) inspect --input "$$created")"; \
	digest="$$(python3 -c 'import json,sys; print(json.loads(sys.argv[1])["compile_plan"]["document_digest_sha256"])' "$$readback")"; \
	edit_readback="$$( $(PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_TOOL_BIN) edit --input "$$created" --output "$$edited" \
		--replace material_graph umber_material ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff material,microdetail_normal \
		--expected-document-digest "$$digest" \
		--expected-source-mesh-digest aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa \
		--expected-reference-digest bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb )"; \
	python3 -c 'import json,sys; x=json.loads(sys.argv[1]); f=json.load(open(sys.argv[2])); assert x["status"]=="ok"; assert x["canonical_document"]["source_mesh_digest_sha256"]==f["source_mesh_digest_sha256"]; assert x["undo_document"]==f' "$$edit_readback" "$$fixture"; \
	canvas_readback="$$( $(PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_TOOL_BIN) canvas --input "$$created" )"; \
	python3 -c 'import json,sys; x=json.loads(sys.argv[1]); assert x["schema"]=="ray_tracing.surface_authoring_document_canvas"; assert x["interaction"]=={"read_only":True,"can_select":True,"can_zoom":True,"can_pan":True,"can_edit":False,"can_save":False,"can_promote":False}; assert len(x["nodes"])==9; assert len(x["edges"])==8; assert x["compile_plan"]["source_mesh_digest_sha256"]=="a"*64' "$$canvas_readback"; \
	if $(PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_TOOL_BIN) edit --input "$$created" --output "$$tmp/stale.json" \
		--replace material_graph stale ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff material \
		--expected-document-digest 0000000000000000000000000000000000000000000000000000000000000000 \
		--expected-source-mesh-digest aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa \
		--expected-reference-digest bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb >/dev/null; then exit 1; fi; \
	if $(PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_TOOL_BIN) edit --input "$$created" --output "$$tmp/source-stale.json" \
		--replace material_graph stale ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff material \
		--expected-document-digest "$$digest" \
		--expected-source-mesh-digest 1111111111111111111111111111111111111111111111111111111111111111 \
		--expected-reference-digest bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb >/dev/null; then exit 1; fi; \
	if test -e "$$tmp/source-stale.json"; then exit 1; fi; \
	if $(PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_TOOL_BIN) edit --input "$$created" --output "$$tmp/ref-stale.json" \
		--replace material_graph stale ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff material \
		--expected-document-digest "$$digest" \
		--expected-source-mesh-digest aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa \
		--expected-reference-digest 1111111111111111111111111111111111111111111111111111111111111111 >/dev/null; then exit 1; fi; \
	if test -e "$$tmp/ref-stale.json"; then exit 1; fi; \
	printf '%s\n' 'surface_authoring_document_adapter fixture_round_trip=ok stale_guards=ok atomic_failure=ok canvas_readback=ok'

test-procedural-surface-authoring-document-canvas-visual: \
	$(PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_TOOL_BIN)
	@set -e; \
	root="$(BUILD_DIR)/agent_runs/surface_authoring_document_canvas"; \
	mkdir -p "$$root"; \
	$(PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_TOOL_BIN) canvas \
		--input tests/fixtures/procedural_surface_authoring_document_v1/cube_composition.json \
		> "$$root/canvas.json"; \
	python3 tools/procedural_surface_authoring_canvas_visual.py \
		--canvas "$$root/canvas.json" \
		--output-svg "$$root/cube_surface_authoring_canvas.svg" \
		--output-summary "$$root/visual_summary.json" >/dev/null; \
	python3 -c 'import json,sys; x=json.load(open(sys.argv[1])); assert x["read_only"] is True; assert x["node_count"]==9; assert x["edge_count"]==8; assert x["width"]>=960; assert x["height"]>=620' "$$root/visual_summary.json"; \
	test -s "$$root/cube_surface_authoring_canvas.svg"; \
	printf '%s\n' 'surface_authoring_document_canvas_visual svg=ok schema=ok read_only=ok'

test-procedural-surface-authoring-contract-matrix: \
	$(PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_TOOL_BIN)
	@python3 tests/integration/test_procedural_surface_authoring_contract_matrix.py \
		--document-tool $(PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_TOOL_BIN)

# V2 curve grooming keeps placement entirely in selector/carrier data and
# forwards the typed controls to the existing PSG-23E groom compiler.
test-procedural-surface-authoring-v2-curve-groom-contract:
	@python3 tests/integration/test_procedural_surface_authoring_document_v2.py
	@python3 tests/integration/test_procedural_surface_authoring_v2_execution_resolver.py
	@python3 tests/integration/test_procedural_surface_authoring_v2_curve_groom_executor.py
	@python3 tests/integration/test_procedural_surface_authoring_v2_curve_groom_runtime_materializer.py

# This is intentionally separate from the fast planner contract: it runs the
# established typed PSG-16B/PSG-17 headless proofs and can take materially
# longer than schema/readback validation.
test-procedural-surface-authoring-contract-matrix-material-microdetail: \
	test-procedural-solid-psg16b-visual-proof
	@python3 tools/procedural_surface_contract_matrix.py \
		--matrix tests/fixtures/procedural_surface_contract_matrix_v1/material_microdetail_mountain_execution.json \
		--execute-profile material --execute-profile microdetail \
		--output-root build/agent_runs/surface_authoring_contract_matrix/material_microdetail

test-procedural-surface-authoring-contract-matrix-material-microdetail-composed: \
	test-procedural-solid-psg16b-visual-proof \
	$(PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_TOOL_BIN)
	@python3 tools/procedural_surface_material_microdetail_visual_proof.py \
		--output-root build/agent_runs/surface_authoring_contract_matrix/material_microdetail/composed
	@python3 tools/procedural_surface_contract_matrix.py \
		--matrix tests/fixtures/procedural_surface_contract_matrix_v1/material_microdetail_mountain_execution.json \
		--document-tool $(PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_TOOL_BIN) \
		--execute-material-microdetail \
		--material-microdetail-proof-receipt build/agent_runs/surface_authoring_contract_matrix/material_microdetail/composed/review/material_microdetail_visual_proof.json \
		--output-root build/agent_runs/surface_authoring_contract_matrix/material_microdetail/composed_matrix

test-procedural-surface-authoring-contract-matrix-selector-material-microdetail: \
	$(PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_TOOL_BIN) \
	$(PROCEDURAL_SURFACE_FEATURE_SELECTION_TOOL_BIN) \
	$(PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_BIN)
	@python3 tools/procedural_surface_selector_material_microdetail_visual_proof.py \
		--output-root build/agent_runs/surface_authoring_contract_matrix/selector_material_microdetail/composed
	@python3 tools/procedural_surface_contract_matrix.py \
		--matrix tests/fixtures/procedural_surface_contract_matrix_v1/selector_material_microdetail_execution.json \
		--document-tool $(PROCEDURAL_SURFACE_AUTHORING_DOCUMENT_TOOL_BIN) \
		--execute-selector-material-microdetail \
		--selector-material-microdetail-proof-receipt build/agent_runs/surface_authoring_contract_matrix/selector_material_microdetail/composed/review/selector_material_microdetail_visual_proof.json \
		--output-root build/agent_runs/surface_authoring_contract_matrix/selector_material_microdetail/composed_matrix

PROCEDURAL_SURFACE_BINDING_TEST_BIN := \
	$(BUILD_DIR)/tests/procedural_surface_binding_test
PROCEDURAL_SURFACE_BINDING_TEST_SRCS := \
	$(TEST_DIR)/test_procedural_surface_binding.c \
	$(SRC_DIR)/procedural/procedural_surface_binding.c \
	$(PROCEDURAL_SURFACE_AUTHORING_COMMON_SRCS)

$(PROCEDURAL_SURFACE_BINDING_TEST_BIN): \
	$(PROCEDURAL_SURFACE_BINDING_TEST_SRCS) \
	$(INC_DIR)/procedural/procedural_surface_binding.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		-DPROCEDURAL_SURFACE_FIELD_PRESET_ROOT=\"$(CURDIR)/tests/fixtures/procedural_surface_field_presets\" \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-o $@ $(PROCEDURAL_SURFACE_BINDING_TEST_SRCS) \
		$(JSON_LIBS) -lm

test-procedural-surface-binding-contract: \
	$(PROCEDURAL_SURFACE_BINDING_TEST_BIN)
	@$(PROCEDURAL_SURFACE_BINDING_TEST_BIN)

PROCEDURAL_SURFACE_TERRAIN_TEST_BIN := \
	$(BUILD_DIR)/tests/procedural_surface_terrain_test
PROCEDURAL_SURFACE_TERRAIN_TEST_SRCS := \
	$(TEST_DIR)/test_procedural_surface_terrain.c \
	$(SRC_DIR)/procedural/procedural_surface_prism_binding.c \
	$(SRC_DIR)/procedural/procedural_surface_prism_mesh.c \
	$(SRC_DIR)/procedural/procedural_surface_binding.c \
	$(SRC_DIR)/procedural/procedural_surface_field_3d.c \
	$(SRC_DIR)/procedural/procedural_surface_recipe.c \
	$(SRC_DIR)/procedural/procedural_surface_topology_contract.c \
	$(PROCEDURAL_SURFACE_AUTHORING_COMMON_SRCS)

$(PROCEDURAL_SURFACE_TERRAIN_TEST_BIN): \
	$(PROCEDURAL_SURFACE_TERRAIN_TEST_SRCS) \
	$(INC_DIR)/procedural/procedural_surface_prism_binding.h \
	$(INC_DIR)/procedural/procedural_surface_prism_mesh.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		-DPROCEDURAL_SURFACE_FIELD_PRESET_ROOT=\"$(CURDIR)/tests/fixtures/procedural_surface_field_presets\" \
		-DPROCEDURAL_SURFACE_FIXTURE_ROOT=\"$(CURDIR)/tests/fixtures/procedural_surface_rock_prism_psg0\" \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-o $@ $(PROCEDURAL_SURFACE_TERRAIN_TEST_SRCS) \
		$(JSON_LIBS) -lm

test-procedural-surface-terrain-contract: \
	$(PROCEDURAL_SURFACE_TERRAIN_TEST_BIN)
	@$(PROCEDURAL_SURFACE_TERRAIN_TEST_BIN)

PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_TEST_BIN := \
	$(BUILD_DIR)/tests/procedural_surface_selected_face_shell_test
PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_TEST_SRCS := \
	$(TEST_DIR)/test_procedural_surface_selected_face_shell.c \
	$(SRC_DIR)/procedural/procedural_surface_selected_face_shell.c \
	$(SRC_DIR)/procedural/procedural_surface_prism_binding.c \
	$(SRC_DIR)/procedural/procedural_surface_prism_mesh.c \
	$(SRC_DIR)/procedural/procedural_surface_binding.c \
	$(SRC_DIR)/procedural/procedural_surface_field_3d.c \
	$(SRC_DIR)/procedural/procedural_surface_recipe.c \
	$(SRC_DIR)/procedural/procedural_surface_topology_contract.c \
	$(PROCEDURAL_SURFACE_AUTHORING_COMMON_SRCS)

$(PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_TEST_BIN): \
	$(PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_TEST_SRCS) \
	$(INC_DIR)/procedural/procedural_surface_selected_face_shell.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		-DPROCEDURAL_SURFACE_FIELD_PRESET_ROOT=\"$(CURDIR)/tests/fixtures/procedural_surface_field_presets\" \
		-DPROCEDURAL_SURFACE_FIXTURE_ROOT=\"$(CURDIR)/tests/fixtures/procedural_surface_rock_prism_psg0\" \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-o $@ $(PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_TEST_SRCS) \
		$(JSON_LIBS) -lm

test-procedural-surface-selected-face-shell-contract: \
	$(PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_TEST_BIN)
	@$(PROCEDURAL_SURFACE_SELECTED_FACE_SHELL_TEST_BIN)

PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_TEST_BIN := \
	$(BUILD_DIR)/tests/procedural_surface_feature_relief_shell_test
PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_TEST_SRCS := \
	$(TEST_DIR)/test_procedural_surface_feature_relief_shell.c \
	$(SRC_DIR)/procedural/procedural_surface_feature_relief_shell.c \
	$(SRC_DIR)/procedural/procedural_surface_feature_field.c \
	$(SRC_DIR)/procedural/procedural_surface_selected_face_shell.c \
	$(SRC_DIR)/procedural/procedural_surface_prism_binding.c \
	$(SRC_DIR)/procedural/procedural_surface_prism_mesh.c \
	$(SRC_DIR)/procedural/procedural_surface_binding.c \
	$(SRC_DIR)/procedural/procedural_surface_field_3d.c \
	$(SRC_DIR)/procedural/procedural_surface_recipe.c \
	$(SRC_DIR)/procedural/procedural_surface_topology_contract.c \
	$(PROCEDURAL_SURFACE_AUTHORING_COMMON_SRCS) \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_BASE_DIR)/src/core_base.c

$(PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_TEST_BIN): \
	$(PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_TEST_SRCS) \
	$(INC_DIR)/procedural/procedural_surface_feature_relief_shell.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		-DPROCEDURAL_SURFACE_FIELD_PRESET_ROOT=\"$(CURDIR)/tests/fixtures/procedural_surface_field_presets\" \
		-DPROCEDURAL_SURFACE_FIXTURE_ROOT=\"$(CURDIR)/tests/fixtures/procedural_surface_rock_prism_psg0\" \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include \
		-o $@ $(PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_TEST_SRCS) \
		$(JSON_LIBS) -lm

test-procedural-surface-feature-relief-shell-contract: \
	$(PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_TEST_BIN)
	@$(PROCEDURAL_SURFACE_FEATURE_RELIEF_SHELL_TEST_BIN)

test-procedural-surface-feature-relief-compiler-contract: \
	$(PROCEDURAL_SURFACE_FIELD_PRESET_ASSET_TOOL_BIN)
	python3 tests/integration/test_procedural_surface_feature_relief_compiler_psg24r.py \
		--asset-tool "$(PROCEDURAL_SURFACE_FIELD_PRESET_ASSET_TOOL_BIN)"

PROCEDURAL_SURFACE_SHELL_TEST_BIN := \
	$(BUILD_DIR)/tests/procedural_surface_shell_test
PROCEDURAL_SURFACE_SHELL_TEST_SRCS := \
	$(TEST_DIR)/test_procedural_surface_shell.c \
	$(SRC_DIR)/procedural/procedural_surface_shell.c \
	$(SRC_DIR)/procedural/procedural_surface_binding.c \
	$(PROCEDURAL_SURFACE_AUTHORING_COMMON_SRCS) \
	third_party/codework_shared/core/core_mesh_asset/src/core_mesh_asset.c \
	third_party/codework_shared/core/core_mesh_asset/src/core_mesh_asset_runtime_document.c \
	third_party/codework_shared/core/core_io/src/core_io.c \
	third_party/codework_shared/core/core_object/src/core_object.c \
	third_party/codework_shared/core/core_units/src/core_units.c \
	third_party/codework_shared/core/core_base/src/core_base.c \
	third_party/codework_shared/core/core_mesh_asset/../../shape/external/cjson/cJSON.c

$(PROCEDURAL_SURFACE_SHELL_TEST_BIN): \
	$(PROCEDURAL_SURFACE_SHELL_TEST_SRCS) \
	$(INC_DIR)/procedural/procedural_surface_shell.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-Ithird_party/codework_shared/core/core_mesh_asset/include \
		-Ithird_party/codework_shared/core/core_object/include \
		-Ithird_party/codework_shared/core/core_io/include \
		-Ithird_party/codework_shared/core/core_units/include \
		-Ithird_party/codework_shared/core/core_base/include \
		-o $@ $(PROCEDURAL_SURFACE_SHELL_TEST_SRCS) \
		$(JSON_LIBS) -lm

test-procedural-surface-shell-contract: \
	$(PROCEDURAL_SURFACE_SHELL_TEST_BIN)
	@$(PROCEDURAL_SURFACE_SHELL_TEST_BIN)

PROCEDURAL_SURFACE_SHELL_TOOL_BIN := \
	$(BUILD_DIR)/tools/cli/procedural_surface_shell_tool
PROCEDURAL_SURFACE_SHELL_TOOL_SRCS := \
	tools/cli/procedural_surface_shell_tool.c \
	$(SRC_DIR)/procedural/procedural_surface_shell.c \
	$(SRC_DIR)/procedural/procedural_surface_binding.c \
	$(PROCEDURAL_SURFACE_AUTHORING_COMMON_SRCS) \
	third_party/codework_shared/core/core_mesh_asset/src/core_mesh_asset.c \
	third_party/codework_shared/core/core_mesh_asset/src/core_mesh_asset_runtime_document.c \
	third_party/codework_shared/core/core_io/src/core_io.c \
	third_party/codework_shared/core/core_object/src/core_object.c \
	third_party/codework_shared/core/core_units/src/core_units.c \
	third_party/codework_shared/core/core_base/src/core_base.c \
	third_party/codework_shared/core/core_mesh_asset/../../shape/external/cjson/cJSON.c

$(PROCEDURAL_SURFACE_SHELL_TOOL_BIN): \
	$(PROCEDURAL_SURFACE_SHELL_TOOL_SRCS) \
	$(INC_DIR)/procedural/procedural_surface_shell.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-Ithird_party/codework_shared/core/core_mesh_asset/include \
		-Ithird_party/codework_shared/core/core_object/include \
		-Ithird_party/codework_shared/core/core_io/include \
		-Ithird_party/codework_shared/core/core_units/include \
		-Ithird_party/codework_shared/core/core_base/include \
		-o $@ $(PROCEDURAL_SURFACE_SHELL_TOOL_SRCS) \
		$(JSON_LIBS) -lm

procedural-surface-shell-tool: $(PROCEDURAL_SURFACE_SHELL_TOOL_BIN)
	@echo "procedural surface shell tool ready: $(PROCEDURAL_SURFACE_SHELL_TOOL_BIN)"

PROCEDURAL_SOLID_ASSET_TOOL_BIN := \
	$(BUILD_DIR)/tools/cli/procedural_solid_asset_tool
PROCEDURAL_SOLID_ASSET_TOOL_SRCS := \
	tools/cli/procedural_solid_asset_tool.c \
	$(SRC_DIR)/procedural/procedural_solid_remesh.c \
	$(SRC_DIR)/procedural/procedural_solid_feature.c \
	$(SRC_DIR)/procedural/procedural_solid_regions.c \
	$(SRC_DIR)/procedural/procedural_solid_local_remesh.c \
	$(SRC_DIR)/procedural/procedural_solid_crease.c \
	$(SRC_DIR)/procedural/procedural_solid_shading.c \
	$(SRC_DIR)/procedural/procedural_solid_quality.c \
	$(PROCEDURAL_SOLID_COMMON_SRCS)

$(PROCEDURAL_SOLID_ASSET_TOOL_BIN): \
	$(PROCEDURAL_SOLID_ASSET_TOOL_SRCS) \
	$(INC_DIR)/procedural/procedural_solid_graph.h \
	$(INC_DIR)/procedural/procedural_solid_mesh.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-Ithird_party/codework_shared/core/core_mesh_asset/include \
		-Ithird_party/codework_shared/core/core_object/include \
		-Ithird_party/codework_shared/core/core_io/include \
		-Ithird_party/codework_shared/core/core_units/include \
		-Ithird_party/codework_shared/core/core_base/include \
		-o $@ $(PROCEDURAL_SOLID_ASSET_TOOL_SRCS) \
		$(JSON_LIBS) -lm

procedural-solid-asset-tool: $(PROCEDURAL_SOLID_ASSET_TOOL_BIN)
	@echo "procedural solid asset tool ready: $(PROCEDURAL_SOLID_ASSET_TOOL_BIN)"

PROCEDURAL_SOLID_AGENT_TOOL_BIN := \
	$(BUILD_DIR)/tools/cli/procedural_solid_agent_tool
PROCEDURAL_SOLID_AGENT_TOOL_SRCS := \
	tools/cli/procedural_solid_agent_tool.c \
	$(SRC_DIR)/procedural/procedural_solid_authoring.c \
	$(SRC_DIR)/procedural/procedural_solid_graph.c \
	$(SRC_DIR)/procedural/procedural_solid_graph_json.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_BASE_DIR)/src/core_base.c

$(PROCEDURAL_SOLID_AGENT_TOOL_BIN): \
	$(PROCEDURAL_SOLID_AGENT_TOOL_SRCS) \
	$(INC_DIR)/procedural/procedural_solid_authoring.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_MESH_ASSET_DIR)/include \
		-I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include \
		-I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-o $@ $(PROCEDURAL_SOLID_AGENT_TOOL_SRCS) \
		$(JSON_LIBS) -lm

procedural-solid-agent-tool: $(PROCEDURAL_SOLID_AGENT_TOOL_BIN)
	@echo "procedural solid agent tool ready: $(PROCEDURAL_SOLID_AGENT_TOOL_BIN)"

test-procedural-solid-agent-flow: \
	$(PROCEDURAL_SOLID_AGENT_TOOL_BIN) \
	$(PROCEDURAL_SOLID_ASSET_TOOL_BIN)
	python3 tests/integration/test_procedural_solid_agent_flow.py \
		--agent-tool "$(PROCEDURAL_SOLID_AGENT_TOOL_BIN)" \
		--asset-tool "$(PROCEDURAL_SOLID_ASSET_TOOL_BIN)"

test-procedural-solid-psg11-flow: $(PROCEDURAL_SOLID_ASSET_TOOL_BIN)
	python3 tests/integration/test_procedural_solid_psg11_flow.py \
		"$(PROCEDURAL_SOLID_ASSET_TOOL_BIN)"

test-procedural-solid-psg12-flow: $(PROCEDURAL_SOLID_ASSET_TOOL_BIN)
	python3 tests/integration/test_procedural_solid_psg12_flow.py \
		"$(PROCEDURAL_SOLID_ASSET_TOOL_BIN)"

PROCEDURAL_SOLID_MATERIAL_AGENT_TOOL_BIN := \
	$(BUILD_DIR)/tools/cli/procedural_solid_material_agent_tool
PROCEDURAL_SOLID_MATERIAL_AGENT_TOOL_SRCS := \
	tools/cli/procedural_solid_material_agent_tool.c \
	$(SRC_DIR)/procedural/procedural_solid_material_binding.c \
	$(SRC_DIR)/procedural/procedural_solid_mesh.c \
	$(SRC_DIR)/procedural/procedural_solid_graph.c \
	$(SRC_DIR)/procedural/procedural_solid_graph_json.c \
	$(SRC_DIR)/procedural/procedural_solid_graph_eval.c \
	$(SRC_DIR)/procedural/procedural_solid_source_accel.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c \
	$(CORE_MESH_ASSET_SRCS) \
	$(CORE_MESH_ASSET_DIR)/../../shape/external/cjson/cJSON.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_BASE_DIR)/src/core_base.c

$(PROCEDURAL_SOLID_MATERIAL_AGENT_TOOL_BIN): \
	$(PROCEDURAL_SOLID_MATERIAL_AGENT_TOOL_SRCS) \
	$(INC_DIR)/procedural/procedural_solid_material_binding.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_MESH_ASSET_DIR)/include \
		-I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include \
		-I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-o $@ $(PROCEDURAL_SOLID_MATERIAL_AGENT_TOOL_SRCS) \
		$(JSON_LIBS) -lm

procedural-solid-material-agent-tool: \
	$(PROCEDURAL_SOLID_MATERIAL_AGENT_TOOL_BIN)
	@echo "procedural solid material agent tool ready: $<"

test-procedural-solid-psg13-flow: \
	$(PROCEDURAL_SOLID_ASSET_TOOL_BIN) \
	$(PROCEDURAL_SOLID_MATERIAL_AGENT_TOOL_BIN)
	python3 tests/integration/test_procedural_solid_psg13_flow.py \
		"$(PROCEDURAL_SOLID_ASSET_TOOL_BIN)" \
		"$(PROCEDURAL_SOLID_MATERIAL_AGENT_TOOL_BIN)"

PROCEDURAL_SOLID_AUTHORED_MATERIAL_AGENT_TOOL_BIN := \
	$(BUILD_DIR)/tools/cli/procedural_solid_authored_material_agent_tool
PROCEDURAL_SOLID_AUTHORED_MATERIAL_AGENT_TOOL_SRCS := \
	tools/cli/procedural_solid_authored_material_agent_tool.c \
	$(SRC_DIR)/procedural/procedural_solid_authored_material.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_BASE_DIR)/src/core_base.c

$(PROCEDURAL_SOLID_AUTHORED_MATERIAL_AGENT_TOOL_BIN): \
	$(PROCEDURAL_SOLID_AUTHORED_MATERIAL_AGENT_TOOL_SRCS) \
	$(INC_DIR)/procedural/procedural_solid_authored_material.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include \
		-o $@ $(PROCEDURAL_SOLID_AUTHORED_MATERIAL_AGENT_TOOL_SRCS) \
		$(JSON_LIBS) -lm

procedural-solid-authored-material-agent-tool: \
	$(PROCEDURAL_SOLID_AUTHORED_MATERIAL_AGENT_TOOL_BIN)
	@echo "procedural solid authored material agent tool ready: $<"

PROCEDURAL_SOLID_MATERIAL_GRAPH_AGENT_TOOL_BIN := \
	$(BUILD_DIR)/tools/cli/procedural_solid_material_graph_agent_tool
PROCEDURAL_SOLID_MATERIAL_GRAPH_AGENT_TOOL_SRCS := \
	tools/cli/procedural_solid_material_graph_agent_tool.c \
	$(SRC_DIR)/procedural/procedural_solid_material_graph.c \
	$(SRC_DIR)/procedural/procedural_solid_material_graph_json.c \
	$(SRC_DIR)/procedural/procedural_solid_material_graph_geometry.c \
	$(SRC_DIR)/procedural/procedural_solid_material_graph_geometry_corner.c \
	$(SRC_DIR)/procedural/procedural_solid_material_runtime_program.c \
	$(SRC_DIR)/procedural/procedural_surface_feature_field.c \
	$(SRC_DIR)/procedural/procedural_surface_feature_curve.c \
	$(SRC_DIR)/procedural/procedural_surface_wood_grain.c \
	$(SRC_DIR)/procedural/procedural_solid_authored_material.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_BASE_DIR)/src/core_base.c

$(PROCEDURAL_SOLID_MATERIAL_GRAPH_AGENT_TOOL_BIN): \
	$(PROCEDURAL_SOLID_MATERIAL_GRAPH_AGENT_TOOL_SRCS) \
	$(INC_DIR)/procedural/procedural_solid_material_graph.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_MESH_ASSET_DIR)/include \
		-I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include \
		-I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-o $@ $(PROCEDURAL_SOLID_MATERIAL_GRAPH_AGENT_TOOL_SRCS) \
		$(JSON_LIBS) -lm

procedural-solid-material-graph-agent-tool: \
	$(PROCEDURAL_SOLID_MATERIAL_GRAPH_AGENT_TOOL_BIN)
	@echo "procedural solid material graph agent tool ready: $<"

PROCEDURAL_SOLID_MATERIAL_DEBUG_TOOL_BIN := \
	$(BUILD_DIR)/tools/cli/procedural_solid_material_debug_tool
PROCEDURAL_SOLID_MATERIAL_DEBUG_TOOL_SRCS := \
	tools/cli/procedural_solid_material_debug_tool.c \
	$(filter-out tools/cli/procedural_solid_material_graph_agent_tool.c, \
		$(PROCEDURAL_SOLID_MATERIAL_GRAPH_AGENT_TOOL_SRCS)) \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset_runtime_document.c \
	$(CORE_MESH_ASSET_DIR)/../../shape/external/cjson/cJSON.c

$(PROCEDURAL_SOLID_MATERIAL_DEBUG_TOOL_BIN): \
	$(PROCEDURAL_SOLID_MATERIAL_DEBUG_TOOL_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_MESH_ASSET_DIR)/include \
		-I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include \
		-I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-o $@ $(PROCEDURAL_SOLID_MATERIAL_DEBUG_TOOL_SRCS) \
		$(JSON_LIBS) -lm

procedural-solid-material-debug-tool: \
	$(PROCEDURAL_SOLID_MATERIAL_DEBUG_TOOL_BIN)
	@echo "procedural solid material debug tool ready: $<"

PROCEDURAL_SOLID_MATERIAL_GRAPH_TEST_BIN := \
	$(BUILD_DIR)/tests/test_procedural_solid_material_graph
$(PROCEDURAL_SOLID_MATERIAL_GRAPH_TEST_BIN): \
	tests/test_procedural_solid_material_graph.c \
	$(PROCEDURAL_SOLID_MATERIAL_GRAPH_AGENT_TOOL_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_MESH_ASSET_DIR)/include \
		-I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include \
		-I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-o $@ tests/test_procedural_solid_material_graph.c \
		$(filter-out tools/cli/procedural_solid_material_graph_agent_tool.c, \
			$(PROCEDURAL_SOLID_MATERIAL_GRAPH_AGENT_TOOL_SRCS)) \
		$(JSON_LIBS) -lm

test-procedural-solid-material-graph: \
	$(PROCEDURAL_SOLID_MATERIAL_GRAPH_TEST_BIN)
	$<

test-procedural-solid-psg15-flow: \
	$(PROCEDURAL_SOLID_MATERIAL_GRAPH_AGENT_TOOL_BIN)
	python3 tests/integration/test_procedural_solid_psg15_flow.py \
		"$(PROCEDURAL_SOLID_MATERIAL_GRAPH_AGENT_TOOL_BIN)"

PROCEDURAL_SOLID_AUTHORED_BINDING_AGENT_TOOL_BIN := \
	$(BUILD_DIR)/tools/cli/procedural_solid_authored_binding_agent_tool
PROCEDURAL_SOLID_AUTHORED_BINDING_AGENT_TOOL_SRCS := \
	tools/cli/procedural_solid_authored_binding_agent_tool.c \
	$(SRC_DIR)/procedural/procedural_solid_authored_material.c \
	$(SRC_DIR)/procedural/procedural_solid_authored_material_binding.c \
	$(PROCEDURAL_SOLID_MATERIAL_AGENT_TOOL_SRCS)

$(PROCEDURAL_SOLID_AUTHORED_BINDING_AGENT_TOOL_BIN): \
	$(PROCEDURAL_SOLID_AUTHORED_BINDING_AGENT_TOOL_SRCS) \
	$(INC_DIR)/procedural/procedural_solid_authored_material.h \
	$(INC_DIR)/procedural/procedural_solid_authored_material_binding.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_MESH_ASSET_DIR)/include \
		-I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include \
		-I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-o $@ $(filter-out tools/cli/procedural_solid_material_agent_tool.c, \
			$(PROCEDURAL_SOLID_AUTHORED_BINDING_AGENT_TOOL_SRCS)) \
		$(JSON_LIBS) -lm

procedural-solid-authored-binding-agent-tool: \
	$(PROCEDURAL_SOLID_AUTHORED_BINDING_AGENT_TOOL_BIN)
	@echo "procedural solid authored binding agent tool ready: $<"

test-procedural-solid-psg14-flow: \
	$(PROCEDURAL_SOLID_ASSET_TOOL_BIN) \
	$(PROCEDURAL_SOLID_MATERIAL_AGENT_TOOL_BIN) \
	$(PROCEDURAL_SOLID_AUTHORED_MATERIAL_AGENT_TOOL_BIN) \
	$(PROCEDURAL_SOLID_AUTHORED_BINDING_AGENT_TOOL_BIN)
	python3 tests/integration/test_procedural_solid_psg14_flow.py \
		"$(PROCEDURAL_SOLID_ASSET_TOOL_BIN)" \
		"$(PROCEDURAL_SOLID_MATERIAL_AGENT_TOOL_BIN)" \
		"$(PROCEDURAL_SOLID_AUTHORED_MATERIAL_AGENT_TOOL_BIN)" \
		"$(PROCEDURAL_SOLID_AUTHORED_BINDING_AGENT_TOOL_BIN)"

PROCEDURAL_SURFACE_AGENT_TOOL_BIN := \
	$(BUILD_DIR)/tools/cli/procedural_surface_agent_tool
PROCEDURAL_SURFACE_AGENT_TOOL_SRCS := \
	tools/cli/procedural_surface_agent_tool.c \
	$(SRC_DIR)/procedural/procedural_surface_authoring.c \
	$(SRC_DIR)/procedural/procedural_surface_binding.c \
	$(PROCEDURAL_SURFACE_AUTHORING_COMMON_SRCS) \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_BASE_DIR)/src/core_base.c

$(PROCEDURAL_SURFACE_AGENT_TOOL_BIN): \
	$(PROCEDURAL_SURFACE_AGENT_TOOL_SRCS) \
	$(INC_DIR)/procedural/procedural_surface_authoring.h \
	$(INC_DIR)/procedural/procedural_surface_binding.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include \
		-o $@ $(PROCEDURAL_SURFACE_AGENT_TOOL_SRCS) \
		$(JSON_LIBS) -lm

procedural-surface-agent-tool: $(PROCEDURAL_SURFACE_AGENT_TOOL_BIN)
	@echo "procedural surface agent tool ready: $(PROCEDURAL_SURFACE_AGENT_TOOL_BIN)"

PROCEDURAL_SURFACE_GRAPH_TOOL_BIN := \
	$(BUILD_DIR)/tools/cli/procedural_surface_graph_tool
PROCEDURAL_SURFACE_GRAPH_TOOL_SRCS := \
	tools/cli/procedural_surface_graph_tool.c \
	$(SRC_DIR)/procedural/procedural_surface_graph.c \
	$(SRC_DIR)/procedural/procedural_surface_graph_json.c \
	$(SRC_DIR)/procedural/procedural_surface_graph_compile.c \
	$(SRC_DIR)/procedural/procedural_surface_recipe.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c

$(PROCEDURAL_SURFACE_GRAPH_TOOL_BIN): \
	$(PROCEDURAL_SURFACE_GRAPH_TOOL_SRCS) \
	$(INC_DIR)/procedural/procedural_surface_graph.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-o $@ $(PROCEDURAL_SURFACE_GRAPH_TOOL_SRCS) \
		$(JSON_LIBS) -lm

procedural-surface-graph-tool: $(PROCEDURAL_SURFACE_GRAPH_TOOL_BIN)
	@echo "procedural surface graph tool ready: $(PROCEDURAL_SURFACE_GRAPH_TOOL_BIN)"

PROCEDURAL_SURFACE_PREVIEW_ASSET_TOOL_BIN := \
	$(BUILD_DIR)/tools/cli/procedural_surface_preview_asset_tool
PROCEDURAL_SURFACE_PREVIEW_ASSET_TOOL_SRCS := \
	tools/cli/procedural_surface_preview_asset_tool.c \
	$(SRC_DIR)/procedural/procedural_surface_derived_asset.c \
	$(SRC_DIR)/procedural/procedural_surface_material.c \
	$(SRC_DIR)/procedural/procedural_surface_prism_mesh.c \
	$(SRC_DIR)/procedural/procedural_surface_mesh_asset_adapter.c \
	$(SRC_DIR)/procedural/procedural_surface_field_3d.c \
	$(SRC_DIR)/procedural/procedural_surface_recipe.c \
	$(SRC_DIR)/procedural/procedural_surface_topology_contract.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset_runtime_document.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_MESH_ASSET_DIR)/../../shape/external/cjson/cJSON.c

$(PROCEDURAL_SURFACE_PREVIEW_ASSET_TOOL_BIN): \
	$(PROCEDURAL_SURFACE_PREVIEW_ASSET_TOOL_SRCS) \
	$(INC_DIR)/procedural/procedural_surface_prism_mesh.h \
	$(INC_DIR)/procedural/procedural_surface_mesh_asset_adapter.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_MESH_ASSET_DIR)/include -I$(CORE_IO_DIR)/include \
		-I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-I$(CORE_BASE_DIR)/include \
		-o $@ $(PROCEDURAL_SURFACE_PREVIEW_ASSET_TOOL_SRCS) \
		$(JSON_LIBS) -lm

procedural-surface-preview-asset-tool: \
	$(PROCEDURAL_SURFACE_PREVIEW_ASSET_TOOL_BIN)
	@echo "procedural surface preview asset tool ready: $(PROCEDURAL_SURFACE_PREVIEW_ASSET_TOOL_BIN)"

PROCEDURAL_SURFACE_FIELD_PRESET_ASSET_TOOL_BIN := \
	$(BUILD_DIR)/tools/cli/procedural_surface_field_preset_asset_tool
PROCEDURAL_SURFACE_FIELD_PRESET_ASSET_TOOL_SRCS := \
	tools/cli/procedural_surface_field_preset_asset_tool.c \
	$(SRC_DIR)/procedural/procedural_surface_derived_asset.c \
	$(SRC_DIR)/procedural/procedural_surface_field_graph.c \
	$(SRC_DIR)/procedural/procedural_surface_field_graph_json.c \
	$(SRC_DIR)/procedural/procedural_surface_field_graph_eval.c \
	$(SRC_DIR)/procedural/procedural_surface_field_graph_noise.c \
	$(SRC_DIR)/procedural/procedural_surface_binding.c \
	$(SRC_DIR)/procedural/procedural_surface_prism_binding.c \
	$(SRC_DIR)/procedural/procedural_surface_selected_face_shell.c \
	$(SRC_DIR)/procedural/procedural_surface_feature_relief_shell.c \
	$(SRC_DIR)/procedural/procedural_surface_feature_field.c \
	$(SRC_DIR)/procedural/procedural_surface_wood_grain.c \
	$(SRC_DIR)/procedural/procedural_surface_material.c \
	$(SRC_DIR)/procedural/procedural_surface_prism_mesh.c \
	$(SRC_DIR)/procedural/procedural_surface_mesh_asset_adapter.c \
	$(SRC_DIR)/procedural/procedural_surface_field_3d.c \
	$(SRC_DIR)/procedural/procedural_surface_recipe.c \
	$(SRC_DIR)/procedural/procedural_surface_topology_contract.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset_runtime_document.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_MESH_ASSET_DIR)/../../shape/external/cjson/cJSON.c

$(PROCEDURAL_SURFACE_FIELD_PRESET_ASSET_TOOL_BIN): \
	$(PROCEDURAL_SURFACE_FIELD_PRESET_ASSET_TOOL_SRCS) \
	$(INC_DIR)/procedural/procedural_surface_field_graph.h \
	$(INC_DIR)/procedural/procedural_surface_binding.h \
	$(INC_DIR)/procedural/procedural_surface_prism_binding.h \
	$(INC_DIR)/procedural/procedural_surface_prism_mesh.h \
	$(INC_DIR)/procedural/procedural_surface_selected_face_shell.h \
	$(INC_DIR)/procedural/procedural_surface_feature_relief_shell.h \
	$(INC_DIR)/procedural/procedural_surface_feature_field.h \
	$(INC_DIR)/procedural/procedural_surface_wood_grain.h \
	$(INC_DIR)/procedural/procedural_surface_mesh_asset_adapter.h
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_MESH_ASSET_DIR)/include -I$(CORE_IO_DIR)/include \
		-I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-I$(CORE_BASE_DIR)/include \
		-o $@ $(PROCEDURAL_SURFACE_FIELD_PRESET_ASSET_TOOL_SRCS) \
		$(JSON_LIBS) -lm

procedural-surface-field-preset-asset-tool: \
	$(PROCEDURAL_SURFACE_FIELD_PRESET_ASSET_TOOL_BIN)
	@echo "procedural surface field preset asset tool ready: $(PROCEDURAL_SURFACE_FIELD_PRESET_ASSET_TOOL_BIN)"

test-procedural-surface-visual-proof: \
	$(PROCEDURAL_SURFACE_PREVIEW_ASSET_TOOL_BIN) \
	$(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tools/procedural_surface_visual_proof.py \
		--asset-tool "$(PROCEDURAL_SURFACE_PREVIEW_ASSET_TOOL_BIN)" \
		--render-cli "$(RAY_TRACING_RENDER_HEADLESS_BIN)"

test-procedural-surface-field-preset-visual-proof: \
	$(PROCEDURAL_SURFACE_FIELD_PRESET_ASSET_TOOL_BIN) \
	$(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tools/procedural_surface_field_preset_visual_proof.py \
		--asset-tool "$(PROCEDURAL_SURFACE_FIELD_PRESET_ASSET_TOOL_BIN)" \
		--render-cli "$(RAY_TRACING_RENDER_HEADLESS_BIN)"

test-procedural-surface-binding-visual-proof: \
	$(PROCEDURAL_SURFACE_FIELD_PRESET_ASSET_TOOL_BIN) \
	$(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tools/procedural_surface_field_preset_visual_proof.py \
		--contract tests/fixtures/procedural_surface_field_presets/preset_binding_visual_contract.json \
		--output-root build/agent_runs/ray_tracing/procedural_surface_field_presets/psg8_binding \
		--asset-tool "$(PROCEDURAL_SURFACE_FIELD_PRESET_ASSET_TOOL_BIN)" \
		--render-cli "$(RAY_TRACING_RENDER_HEADLESS_BIN)"

test-procedural-surface-terrain-visual-proof: \
	$(PROCEDURAL_SURFACE_FIELD_PRESET_ASSET_TOOL_BIN) \
	$(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tools/procedural_surface_field_preset_visual_proof.py \
		--contract tests/fixtures/procedural_surface_field_presets/terrain_body_visual_contract.json \
		--output-root build/agent_runs/ray_tracing/procedural_surface_field_presets/psg8_5_terrain \
		--asset-tool "$(PROCEDURAL_SURFACE_FIELD_PRESET_ASSET_TOOL_BIN)" \
		--render-cli "$(RAY_TRACING_RENDER_HEADLESS_BIN)"

test-procedural-surface-selected-face-visual-proof: \
	$(PROCEDURAL_SURFACE_FIELD_PRESET_ASSET_TOOL_BIN) \
	$(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tools/procedural_surface_selected_face_visual_proof.py \
		--asset-tool "$(PROCEDURAL_SURFACE_FIELD_PRESET_ASSET_TOOL_BIN)" \
		--render-cli "$(RAY_TRACING_RENDER_HEADLESS_BIN)"

test-procedural-surface-feature-relief-visual-proof: \
	$(PROCEDURAL_SURFACE_FIELD_PRESET_ASSET_TOOL_BIN) \
	$(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tools/procedural_surface_feature_relief_visual_proof.py \
		--asset-tool "$(PROCEDURAL_SURFACE_FIELD_PRESET_ASSET_TOOL_BIN)" \
		--render-cli "$(RAY_TRACING_RENDER_HEADLESS_BIN)"

test-procedural-surface-shell-visual-proof: \
	$(PROCEDURAL_SURFACE_SHELL_TOOL_BIN) \
	$(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tools/procedural_surface_shell_visual_proof.py \
		--shell-tool "$(PROCEDURAL_SURFACE_SHELL_TOOL_BIN)" \
		--render-cli "$(RAY_TRACING_RENDER_HEADLESS_BIN)"

test-procedural-solid-visual-proof: \
	$(PROCEDURAL_SOLID_ASSET_TOOL_BIN) \
	$(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tools/procedural_solid_visual_proof.py \
		--solid-tool "$(PROCEDURAL_SOLID_ASSET_TOOL_BIN)" \
		--render-cli "$(RAY_TRACING_RENDER_HEADLESS_BIN)"

test-procedural-solid-psg12-visual-proof: \
	$(PROCEDURAL_SOLID_ASSET_TOOL_BIN) \
	$(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tools/procedural_solid_visual_proof.py \
		--quality-adaptive \
		--solid-tool "$(PROCEDURAL_SOLID_ASSET_TOOL_BIN)" \
		--render-cli "$(RAY_TRACING_RENDER_HEADLESS_BIN)"

test-procedural-solid-psg13-visual-proof: \
	$(PROCEDURAL_SOLID_ASSET_TOOL_BIN) \
	$(PROCEDURAL_SOLID_MATERIAL_AGENT_TOOL_BIN) \
	$(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tools/procedural_solid_material_visual_proof.py \
		--solid-tool "$(PROCEDURAL_SOLID_ASSET_TOOL_BIN)" \
		--material-agent-tool "$(PROCEDURAL_SOLID_MATERIAL_AGENT_TOOL_BIN)" \
		--render-cli "$(RAY_TRACING_RENDER_HEADLESS_BIN)"

test-procedural-solid-psg14-visual-proof: \
	$(PROCEDURAL_SOLID_ASSET_TOOL_BIN) \
	$(PROCEDURAL_SOLID_MATERIAL_AGENT_TOOL_BIN) \
	$(PROCEDURAL_SOLID_AUTHORED_MATERIAL_AGENT_TOOL_BIN) \
	$(PROCEDURAL_SOLID_AUTHORED_BINDING_AGENT_TOOL_BIN) \
	$(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tools/procedural_solid_authored_material_visual_proof.py \
		--solid-tool "$(PROCEDURAL_SOLID_ASSET_TOOL_BIN)" \
		--region-binding-tool "$(PROCEDURAL_SOLID_MATERIAL_AGENT_TOOL_BIN)" \
		--material-tool "$(PROCEDURAL_SOLID_AUTHORED_MATERIAL_AGENT_TOOL_BIN)" \
		--authored-binding-tool \
			"$(PROCEDURAL_SOLID_AUTHORED_BINDING_AGENT_TOOL_BIN)" \
		--render-cli "$(RAY_TRACING_RENDER_HEADLESS_BIN)"

test-procedural-solid-psg16b-visual-contract:
	python3 tools/procedural_solid_material_graph_visual_proof.py \
		--validate-contract-only

test-procedural-solid-psg15-visual-contract: \
	test-procedural-solid-psg16b-visual-contract

test-procedural-solid-psg17-visual-contract:
	python3 tools/procedural_solid_microdetail_normal_visual_proof.py \
		--validate-contract-only

test-procedural-solid-psg16b-visual-proof: \
	$(PROCEDURAL_SOLID_ASSET_TOOL_BIN) \
	$(PROCEDURAL_SURFACE_FIELD_PRESET_ASSET_TOOL_BIN) \
	$(PROCEDURAL_SOLID_MATERIAL_AGENT_TOOL_BIN) \
	$(PROCEDURAL_SOLID_AUTHORED_MATERIAL_AGENT_TOOL_BIN) \
	$(PROCEDURAL_SOLID_AUTHORED_BINDING_AGENT_TOOL_BIN) \
	$(PROCEDURAL_SOLID_MATERIAL_GRAPH_AGENT_TOOL_BIN) \
	$(PROCEDURAL_SOLID_MATERIAL_DEBUG_TOOL_BIN) \
	$(PROCEDURAL_SOLID_AGENT_TOOL_BIN) \
	$(PROCEDURAL_SURFACE_AGENT_TOOL_BIN) \
	$(RAY_TRACING_RENDER_HEADLESS_BIN)

test-procedural-solid-psg15-visual-proof: \
	test-procedural-solid-psg16b-visual-proof
	python3 tools/procedural_solid_material_graph_visual_proof.py \
		--solid-tool "$(PROCEDURAL_SOLID_ASSET_TOOL_BIN)" \
		--solid-agent-tool "$(PROCEDURAL_SOLID_AGENT_TOOL_BIN)" \
		--surface-agent-tool "$(PROCEDURAL_SURFACE_AGENT_TOOL_BIN)" \
		--field-tool "$(PROCEDURAL_SURFACE_FIELD_PRESET_ASSET_TOOL_BIN)" \
		--region-binding-tool "$(PROCEDURAL_SOLID_MATERIAL_AGENT_TOOL_BIN)" \
		--material-tool "$(PROCEDURAL_SOLID_AUTHORED_MATERIAL_AGENT_TOOL_BIN)" \
		--authored-binding-tool \
			"$(PROCEDURAL_SOLID_AUTHORED_BINDING_AGENT_TOOL_BIN)" \
		--graph-tool "$(PROCEDURAL_SOLID_MATERIAL_GRAPH_AGENT_TOOL_BIN)" \
		--debug-tool "$(PROCEDURAL_SOLID_MATERIAL_DEBUG_TOOL_BIN)" \
		--render-cli "$(RAY_TRACING_RENDER_HEADLESS_BIN)"

test-procedural-solid-psg17-visual-proof: \
	$(PROCEDURAL_SOLID_ASSET_TOOL_BIN) \
	$(PROCEDURAL_SURFACE_FIELD_PRESET_ASSET_TOOL_BIN) \
	$(PROCEDURAL_SOLID_MATERIAL_AGENT_TOOL_BIN) \
	$(PROCEDURAL_SOLID_AUTHORED_MATERIAL_AGENT_TOOL_BIN) \
	$(PROCEDURAL_SOLID_AUTHORED_BINDING_AGENT_TOOL_BIN) \
	$(PROCEDURAL_SOLID_MATERIAL_GRAPH_AGENT_TOOL_BIN) \
	$(PROCEDURAL_SOLID_MATERIAL_DEBUG_TOOL_BIN) \
	$(PROCEDURAL_SOLID_AGENT_TOOL_BIN) \
	$(PROCEDURAL_SURFACE_AGENT_TOOL_BIN) \
	$(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tools/procedural_solid_microdetail_normal_visual_proof.py \
		--solid-tool "$(PROCEDURAL_SOLID_ASSET_TOOL_BIN)" \
		--solid-agent-tool "$(PROCEDURAL_SOLID_AGENT_TOOL_BIN)" \
		--surface-agent-tool "$(PROCEDURAL_SURFACE_AGENT_TOOL_BIN)" \
		--field-tool "$(PROCEDURAL_SURFACE_FIELD_PRESET_ASSET_TOOL_BIN)" \
		--region-binding-tool "$(PROCEDURAL_SOLID_MATERIAL_AGENT_TOOL_BIN)" \
		--material-tool "$(PROCEDURAL_SOLID_AUTHORED_MATERIAL_AGENT_TOOL_BIN)" \
		--authored-binding-tool \
			"$(PROCEDURAL_SOLID_AUTHORED_BINDING_AGENT_TOOL_BIN)" \
		--graph-tool "$(PROCEDURAL_SOLID_MATERIAL_GRAPH_AGENT_TOOL_BIN)" \
		--debug-tool "$(PROCEDURAL_SOLID_MATERIAL_DEBUG_TOOL_BIN)" \
		--render-cli "$(RAY_TRACING_RENDER_HEADLESS_BIN)"

test-procedural-surface-agent-iteration: \
	$(PROCEDURAL_SURFACE_AGENT_TOOL_BIN) \
	$(PROCEDURAL_SURFACE_FIELD_PRESET_ASSET_TOOL_BIN) \
	$(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tools/procedural_surface_agent_iteration.py \
		--agent-tool "$(PROCEDURAL_SURFACE_AGENT_TOOL_BIN)" \
		--asset-tool "$(PROCEDURAL_SURFACE_FIELD_PRESET_ASSET_TOOL_BIN)" \
		--render-cli "$(RAY_TRACING_RENDER_HEADLESS_BIN)"

test-ray-tracing-artifact-comparison:
	PYTHONPYCACHEPREFIX="$(CURDIR)/build/pycache" \
		python3 -m unittest tests/test_compare_render_artifacts.py

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

RAY_TRACING_DURABLE_FRAME_RECOVERY_TEST_BIN := \
	$(BUILD_DIR)/tests/ray_tracing_durable_frame_recovery_test
RAY_TRACING_DURABLE_FRAME_RECOVERY_TEST_SRCS := \
	$(TEST_DIR)/test_ray_tracing_durable_frame_recovery.c \
	$(SRC_DIR)/app/ray_tracing_durable_io.c \
	$(SRC_DIR)/app/ray_tracing_recovery_authority.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c \
	$(SRC_DIR)/app/ray_tracing_frame_recovery.c

$(RAY_TRACING_DURABLE_FRAME_RECOVERY_TEST_BIN): \
	$(RAY_TRACING_DURABLE_FRAME_RECOVERY_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-o $@ $(RAY_TRACING_DURABLE_FRAME_RECOVERY_TEST_SRCS) $(JSON_LIBS)

test-ray-tracing-durable-frame-recovery: \
	$(RAY_TRACING_DURABLE_FRAME_RECOVERY_TEST_BIN)
	@$(RAY_TRACING_DURABLE_FRAME_RECOVERY_TEST_BIN)

RAY_TRACING_RECOVERY_AUTHORITY_TEST_BIN := \
	$(BUILD_DIR)/tests/ray_tracing_recovery_authority_test
RAY_TRACING_RECOVERY_AUTHORITY_TEST_SRCS := \
	$(TEST_DIR)/test_ray_tracing_recovery_authority.c \
	$(SRC_DIR)/app/ray_tracing_recovery_authority.c \
	$(SRC_DIR)/app/ray_tracing_durable_io.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c

$(RAY_TRACING_RECOVERY_AUTHORITY_TEST_BIN): \
	$(RAY_TRACING_RECOVERY_AUTHORITY_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-o $@ $(RAY_TRACING_RECOVERY_AUTHORITY_TEST_SRCS) $(JSON_LIBS)

test-ray-tracing-recovery-authority: \
	$(RAY_TRACING_RECOVERY_AUTHORITY_TEST_BIN)
	@$(RAY_TRACING_RECOVERY_AUTHORITY_TEST_BIN)

RAY_TRACING_WORKER_PROTOCOL_TEST_BIN := \
	$(BUILD_DIR)/tests/ray_tracing_worker_protocol_test
RAY_TRACING_WORKER_PROTOCOL_TEST_SRCS := \
	$(TEST_DIR)/test_ray_tracing_worker_protocol.c \
	$(SRC_DIR)/app/ray_tracing_worker_protocol.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c \
	$(SRC_DIR)/app/ray_tracing_recovery_authority.c \
	$(SRC_DIR)/app/ray_tracing_durable_io.c

$(RAY_TRACING_WORKER_PROTOCOL_TEST_BIN): \
	$(RAY_TRACING_WORKER_PROTOCOL_TEST_SRCS) $(WORKER_VERSION_HEADER)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(WORKER_VERSION_GENERATED_DIR) -I$(INC_DIR) -I$(SRC_DIR) \
		-o $@ $(RAY_TRACING_WORKER_PROTOCOL_TEST_SRCS) $(JSON_LIBS)

test-ray-tracing-worker-protocol: $(RAY_TRACING_WORKER_PROTOCOL_TEST_BIN)
	@$(RAY_TRACING_WORKER_PROTOCOL_TEST_BIN)

test-ray-tracing-worker-version-contract: worker-version-contract
	@python3 tests/integration/run_ray_tracing_worker_version_contract.py

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

SCENE_EDITOR_MESH_PREVIEW_SHADING_TEST_BIN := \
	$(BUILD_DIR)/tests/scene_editor_mesh_preview_shading_test
SCENE_EDITOR_MESH_PREVIEW_SHADING_TEST_SRCS := \
	$(TEST_DIR)/scene_editor_mesh_preview_shading_test.c \
	$(SRC_DIR)/editor/scene_editor_mesh_preview_shading.c

$(SCENE_EDITOR_MESH_PREVIEW_SHADING_TEST_BIN): \
	$(SCENE_EDITOR_MESH_PREVIEW_SHADING_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Wno-unknown-attributes -Wno-c23-extensions -g \
		$(SDL_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) -I$(CORE_MESH_PREVIEW_DIR)/include \
		-I$(CORE_MESH_ASSET_DIR)/include -I$(CORE_OBJECT_DIR)/include \
		-I$(CORE_UNITS_DIR)/include -I$(CORE_BASE_DIR)/include \
		-o $@ $(SCENE_EDITOR_MESH_PREVIEW_SHADING_TEST_SRCS) $(SDL_LIBS) -lm

test-scene-editor-mesh-preview-shading: \
	$(SCENE_EDITOR_MESH_PREVIEW_SHADING_TEST_BIN)
	@$(SCENE_EDITOR_MESH_PREVIEW_SHADING_TEST_BIN)

SCENE_EDITOR_PRIMITIVE_PREVIEW_GEOMETRY_TEST_BIN := \
	$(BUILD_DIR)/tests/scene_editor_primitive_preview_geometry_test
SCENE_EDITOR_PRIMITIVE_PREVIEW_GEOMETRY_TEST_SRCS := \
	$(TEST_DIR)/scene_editor_primitive_preview_geometry_test.c \
	$(SRC_DIR)/editor/scene_editor_primitive_preview_geometry.c

$(SCENE_EDITOR_PRIMITIVE_PREVIEW_GEOMETRY_TEST_BIN): \
	$(SCENE_EDITOR_PRIMITIVE_PREVIEW_GEOMETRY_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Wno-unknown-attributes -Wno-c23-extensions -g \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-o $@ $(SCENE_EDITOR_PRIMITIVE_PREVIEW_GEOMETRY_TEST_SRCS) -lm

test-scene-editor-primitive-preview-geometry: \
	$(SCENE_EDITOR_PRIMITIVE_PREVIEW_GEOMETRY_TEST_BIN)
	@$(SCENE_EDITOR_PRIMITIVE_PREVIEW_GEOMETRY_TEST_BIN)

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

PROCEDURAL_SURFACE_DERIVED_ASSET_LOAD_SRCS := \
	$(SRC_DIR)/procedural/procedural_surface_derived_asset.c \
	$(SRC_DIR)/procedural/procedural_surface_recipe.c \
	$(SRC_DIR)/procedural/procedural_surface_binding.c \
	$(SRC_DIR)/procedural/procedural_surface_field_graph.c \
	$(SRC_DIR)/procedural/procedural_surface_field_graph_json.c \
	$(SRC_DIR)/procedural/procedural_surface_field_graph_eval.c \
	$(SRC_DIR)/procedural/procedural_surface_field_graph_noise.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c

PROCEDURAL_SOLID_MATERIAL_BINDING_LOAD_SRCS := \
	$(SRC_DIR)/procedural/procedural_solid_material_binding.c \
	$(SRC_DIR)/procedural/procedural_solid_authored_material.c \
	$(SRC_DIR)/procedural/procedural_solid_authored_material_binding.c \
	$(SRC_DIR)/procedural/procedural_solid_mesh.c \
	$(SRC_DIR)/procedural/procedural_solid_graph.c \
	$(SRC_DIR)/procedural/procedural_solid_graph_json.c \
	$(SRC_DIR)/procedural/procedural_solid_graph_eval.c \
	$(SRC_DIR)/procedural/procedural_solid_source_accel.c

PROCEDURAL_SOLID_MATERIAL_GRAPH_LOAD_SRCS := \
	$(SRC_DIR)/procedural/procedural_imported_surface_region.c \
	$(SRC_DIR)/procedural/procedural_solid_material_graph.c \
	$(SRC_DIR)/procedural/procedural_solid_material_graph_json.c \
	$(SRC_DIR)/procedural/procedural_solid_material_graph_geometry.c \
	$(SRC_DIR)/procedural/procedural_solid_material_graph_geometry_corner.c \
	$(SRC_DIR)/procedural/procedural_solid_material_runtime_program.c \
	$(SRC_DIR)/procedural/procedural_surface_feature_field.c \
	$(SRC_DIR)/procedural/procedural_surface_feature_curve.c \
	$(SRC_DIR)/procedural/procedural_surface_wood_grain.c

PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_BIN := \
	$(BUILD_DIR)/tools/cli/procedural_imported_surface_region_tool
PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_SRCS := \
	tools/cli/procedural_imported_surface_region_tool.c \
	$(SRC_DIR)/procedural/procedural_imported_surface_region.c \
	$(SRC_DIR)/procedural/procedural_solid_mesh.c \
	$(SRC_DIR)/procedural/procedural_solid_graph.c \
	$(SRC_DIR)/procedural/procedural_solid_graph_json.c \
	$(SRC_DIR)/procedural/procedural_solid_graph_eval.c \
	$(SRC_DIR)/procedural/procedural_solid_source_accel.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset_runtime_document.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_MESH_ASSET_DIR)/../../shape/external/cjson/cJSON.c

$(PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_BIN): \
	$(PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_MESH_ASSET_DIR)/include \
		-I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include \
		-I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-o $@ $(PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_SRCS) \
		$(JSON_LIBS) -lm

procedural-imported-surface-region-tool: \
	$(PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_BIN)
	@echo "procedural imported surface region tool ready: $<"

test-procedural-imported-surface-region-psg19: \
	$(PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_BIN)
	@$(MAKE) -C ../line_drawing imported_mesh_harness >/dev/null
	@python3 tests/integration/test_procedural_imported_surface_region_psg19.py \
		$(PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_BIN) \
		../../tools/procedural_object_authoring/procedural_stl_tool.py \
		../line_drawing/build/toolchains/clang/bin/imported_mesh_harness
	@echo "PSG-19 imported surface region lane passed"

test-procedural-imported-surface-region-psg19-visual-proof: \
	$(PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_BIN) \
	$(PROCEDURAL_SOLID_MATERIAL_AGENT_TOOL_BIN) \
	$(PROCEDURAL_SOLID_AUTHORED_MATERIAL_AGENT_TOOL_BIN) \
	$(PROCEDURAL_SOLID_AUTHORED_BINDING_AGENT_TOOL_BIN) \
	$(PROCEDURAL_SOLID_MATERIAL_GRAPH_AGENT_TOOL_BIN) \
	$(RAY_TRACING_RENDER_HEADLESS_BIN)
	@$(MAKE) -C ../line_drawing imported_mesh_harness >/dev/null
	@python3 tools/procedural_imported_surface_region_visual_proof.py
	@echo "PSG-19 imported surface region visual proof passed"

PROCEDURAL_IMPORTED_SURFACE_INSET_TOOL_BIN := \
	$(BUILD_DIR)/tools/cli/procedural_imported_surface_inset_tool
PROCEDURAL_IMPORTED_SURFACE_INSET_TOOL_SRCS := \
	tools/cli/procedural_imported_surface_inset_tool.c \
	$(SRC_DIR)/procedural/procedural_imported_surface_inset.c \
	$(SRC_DIR)/procedural/procedural_imported_surface_inset_refinement.c \
	$(SRC_DIR)/procedural/procedural_imported_surface_inset_topology.c \
	$(SRC_DIR)/procedural/procedural_imported_surface_region.c \
	$(SRC_DIR)/procedural/procedural_solid_mesh.c \
	$(SRC_DIR)/procedural/procedural_solid_graph.c \
	$(SRC_DIR)/procedural/procedural_solid_graph_json.c \
	$(SRC_DIR)/procedural/procedural_solid_graph_eval.c \
	$(SRC_DIR)/procedural/procedural_solid_source_accel.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset_runtime_document.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_MESH_ASSET_DIR)/../../shape/external/cjson/cJSON.c

$(PROCEDURAL_IMPORTED_SURFACE_INSET_TOOL_BIN): \
	$(PROCEDURAL_IMPORTED_SURFACE_INSET_TOOL_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_MESH_ASSET_DIR)/include \
		-I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include \
		-I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-o $@ $(PROCEDURAL_IMPORTED_SURFACE_INSET_TOOL_SRCS) \
		$(JSON_LIBS) -lm

procedural-imported-surface-inset-tool: \
	$(PROCEDURAL_IMPORTED_SURFACE_INSET_TOOL_BIN)
	@echo "procedural imported surface inset tool ready: $<"

test-procedural-imported-surface-inset-psg20: \
	$(PROCEDURAL_IMPORTED_SURFACE_INSET_TOOL_BIN) \
	$(PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_BIN)
	@$(MAKE) -C ../line_drawing imported_mesh_harness >/dev/null
	@python3 tests/integration/test_procedural_imported_surface_inset_psg20.py \
		$(PROCEDURAL_IMPORTED_SURFACE_INSET_TOOL_BIN) \
		$(PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_BIN) \
		../../tools/procedural_object_authoring/procedural_stl_tool.py \
		../line_drawing/build/toolchains/clang/bin/imported_mesh_harness
	@echo "PSG-21 adaptive imported surface inset lane passed"

test-procedural-imported-surface-inset-psg21: \
	test-procedural-imported-surface-inset-psg20

test-procedural-imported-surface-inset-psg20-visual-proof: \
	$(PROCEDURAL_IMPORTED_SURFACE_INSET_TOOL_BIN) \
	$(PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_BIN) \
	$(PROCEDURAL_SOLID_MATERIAL_AGENT_TOOL_BIN) \
	$(PROCEDURAL_SOLID_AUTHORED_MATERIAL_AGENT_TOOL_BIN) \
	$(PROCEDURAL_SOLID_AUTHORED_BINDING_AGENT_TOOL_BIN) \
	$(PROCEDURAL_SOLID_MATERIAL_GRAPH_AGENT_TOOL_BIN) \
	$(RAY_TRACING_RENDER_HEADLESS_BIN)
	@$(MAKE) -C ../line_drawing imported_mesh_harness >/dev/null
	@python3 tools/procedural_imported_surface_inset_visual_proof.py
	@echo "PSG-21 adaptive imported surface inset visual proof passed"

test-procedural-imported-surface-inset-psg21-visual-proof: \
	test-procedural-imported-surface-inset-psg20-visual-proof

PROCEDURAL_IMPORTED_SURFACE_GROWTH_TOOL_BIN := \
	$(BUILD_DIR)/tools/cli/procedural_imported_surface_growth_tool
PROCEDURAL_IMPORTED_SURFACE_GROWTH_TOOL_SRCS := \
	tools/cli/procedural_imported_surface_growth_tool.c \
	$(SRC_DIR)/procedural/procedural_imported_surface_growth.c \
	$(SRC_DIR)/procedural/procedural_imported_surface_growth_explicit.c \
	$(SRC_DIR)/procedural/procedural_imported_surface_growth_sampling.c \
	$(SRC_DIR)/procedural/procedural_imported_surface_growth_geometry.c \
	$(SRC_DIR)/procedural/procedural_imported_surface_growth_validation.c \
	$(SRC_DIR)/procedural/procedural_imported_surface_region.c \
	$(SRC_DIR)/procedural/procedural_solid_mesh.c \
	$(SRC_DIR)/procedural/procedural_solid_graph.c \
	$(SRC_DIR)/procedural/procedural_solid_graph_json.c \
	$(SRC_DIR)/procedural/procedural_solid_graph_eval.c \
	$(SRC_DIR)/procedural/procedural_solid_source_accel.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset_runtime_document.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_MESH_ASSET_DIR)/../../shape/external/cjson/cJSON.c

$(PROCEDURAL_IMPORTED_SURFACE_GROWTH_TOOL_BIN): \
	$(PROCEDURAL_IMPORTED_SURFACE_GROWTH_TOOL_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_MESH_ASSET_DIR)/include \
		-I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include \
		-I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-o $@ $(PROCEDURAL_IMPORTED_SURFACE_GROWTH_TOOL_SRCS) \
		$(JSON_LIBS) -lm

procedural-imported-surface-growth-tool: \
	$(PROCEDURAL_IMPORTED_SURFACE_GROWTH_TOOL_BIN)
	@echo "procedural imported surface growth tool ready: $<"

test-procedural-imported-surface-growth-psg22: \
	$(PROCEDURAL_IMPORTED_SURFACE_GROWTH_TOOL_BIN) \
	$(PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_BIN)
	@$(MAKE) -C ../line_drawing imported_mesh_harness >/dev/null
	@python3 tests/integration/test_procedural_imported_surface_growth_psg22.py \
		$(PROCEDURAL_IMPORTED_SURFACE_GROWTH_TOOL_BIN) \
		$(PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_BIN) \
		../../tools/procedural_object_authoring/procedural_stl_tool.py \
		../line_drawing/build/toolchains/clang/bin/imported_mesh_harness
	@echo "PSG-22 imported surface attached growth lane passed"

test-procedural-surface-feature-selection-psg24: \
	$(PROCEDURAL_SURFACE_FEATURE_SELECTION_TOOL_BIN) \
	$(PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_BIN) \
	$(PROCEDURAL_IMPORTED_SURFACE_INSET_TOOL_BIN) \
	$(PROCEDURAL_IMPORTED_SURFACE_GROWTH_TOOL_BIN)
	@$(MAKE) -C ../line_drawing imported_mesh_harness >/dev/null
	@python3 tests/integration/test_procedural_surface_feature_selection_psg24.py \
		$(PROCEDURAL_SURFACE_FEATURE_SELECTION_TOOL_BIN) \
		$(PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_BIN) \
		$(PROCEDURAL_IMPORTED_SURFACE_INSET_TOOL_BIN) \
		$(PROCEDURAL_IMPORTED_SURFACE_GROWTH_TOOL_BIN) \
		../../tools/procedural_object_authoring/procedural_stl_tool.py \
		../line_drawing/build/toolchains/clang/bin/imported_mesh_harness
	@echo "PSG-24 field-selected carrier bridge passed"

test-procedural-surface-feature-inset-psg24c: \
	$(PROCEDURAL_SURFACE_FEATURE_SELECTION_TOOL_BIN) \
	$(PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_BIN) \
	$(PROCEDURAL_IMPORTED_SURFACE_INSET_TOOL_BIN) \
	$(PROCEDURAL_SOLID_MESH_DIGEST_TOOL_BIN)
	@$(MAKE) -C ../line_drawing imported_mesh_harness >/dev/null
	@python3 tests/integration/test_procedural_surface_feature_inset_psg24c.py \
		$(PROCEDURAL_SURFACE_FEATURE_SELECTION_TOOL_BIN) \
		$(PROCEDURAL_IMPORTED_SURFACE_INSET_TOOL_BIN) \
		$(PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_BIN) \
		../../tools/procedural_object_authoring/procedural_stl_tool.py \
		../line_drawing/build/toolchains/clang/bin/imported_mesh_harness \
		$(PROCEDURAL_SOLID_MESH_DIGEST_TOOL_BIN) \
		tools/procedural_surface_feature_inset_compiler.py
	@echo "PSG-24C physical retained/wall/floor feature inset lane passed"

test-procedural-surface-feature-inset-psg24c-visual-proof: \
	$(PROCEDURAL_SURFACE_FEATURE_SELECTION_TOOL_BIN) \
	$(PROCEDURAL_IMPORTED_SURFACE_INSET_TOOL_BIN) \
	$(RAY_TRACING_RENDER_HEADLESS_BIN)
	@python3 tools/procedural_surface_feature_inset_visual_proof.py
	@echo "PSG-24C physical feature inset visual proof passed"

test-procedural-surface-feature-deposit-psg24d: \
	$(PROCEDURAL_SURFACE_FEATURE_SELECTION_TOOL_BIN) \
	$(PROCEDURAL_IMPORTED_SURFACE_GROWTH_TOOL_BIN) \
	$(PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_BIN) \
	$(PROCEDURAL_SOLID_MESH_DIGEST_TOOL_BIN)
	@$(MAKE) -C ../line_drawing imported_mesh_harness >/dev/null
	@python3 tests/integration/test_procedural_surface_feature_deposit_psg24d.py \
		$(PROCEDURAL_SURFACE_FEATURE_SELECTION_TOOL_BIN) \
		$(PROCEDURAL_IMPORTED_SURFACE_GROWTH_TOOL_BIN) \
		$(PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_BIN) \
		../../tools/procedural_object_authoring/procedural_stl_tool.py \
		../line_drawing/build/toolchains/clang/bin/imported_mesh_harness \
		$(PROCEDURAL_SOLID_MESH_DIGEST_TOOL_BIN) \
		tools/procedural_surface_feature_deposit_compiler.py \
		../../tools/procedural_object_authoring/procedural_object_bundle.py
	@echo "PSG-24D positive feature attached-deposit lane passed"

test-procedural-surface-feature-deposit-psg24d-visual-proof: \
	$(PROCEDURAL_SURFACE_FEATURE_SELECTION_TOOL_BIN) \
	$(PROCEDURAL_IMPORTED_SURFACE_GROWTH_TOOL_BIN) \
	$(RAY_TRACING_RENDER_HEADLESS_BIN)
	@python3 tools/procedural_surface_feature_deposit_visual_proof.py
	@echo "PSG-24D attached surface-feature deposit visual proof passed"

test-procedural-imported-surface-growth-psg22-visual-proof: \
	$(PROCEDURAL_IMPORTED_SURFACE_GROWTH_TOOL_BIN) \
	$(PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_BIN) \
	$(RAY_TRACING_RENDER_HEADLESS_BIN)
	@$(MAKE) -C ../line_drawing imported_mesh_harness >/dev/null
	@python3 tools/procedural_imported_surface_growth_visual_proof.py
	@echo "PSG-22 imported surface attached growth visual proof passed"

PROCEDURAL_IMPORTED_SURFACE_STRANDS_TOOL_BIN := \
	$(BUILD_DIR)/tools/cli/procedural_imported_surface_strands_tool
PROCEDURAL_IMPORTED_SURFACE_STRANDS_TOOL_SRCS := \
	tools/cli/procedural_imported_surface_strands_tool.c \
	$(SRC_DIR)/procedural/procedural_imported_surface_strands.c \
	$(SRC_DIR)/procedural/procedural_imported_surface_strands_sampling.c \
	$(SRC_DIR)/procedural/procedural_imported_surface_strands_geometry.c \
	$(SRC_DIR)/procedural/procedural_imported_surface_strands_validation.c \
	$(SRC_DIR)/procedural/procedural_imported_surface_region.c \
	$(SRC_DIR)/procedural/procedural_solid_mesh.c \
	$(SRC_DIR)/procedural/procedural_solid_graph.c \
	$(SRC_DIR)/procedural/procedural_solid_graph_json.c \
	$(SRC_DIR)/procedural/procedural_solid_graph_eval.c \
	$(SRC_DIR)/procedural/procedural_solid_source_accel.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset_runtime_document.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_MESH_ASSET_DIR)/../../shape/external/cjson/cJSON.c

$(PROCEDURAL_IMPORTED_SURFACE_STRANDS_TOOL_BIN): \
	$(PROCEDURAL_IMPORTED_SURFACE_STRANDS_TOOL_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g \
		-D_DARWIN_C_SOURCE -D_POSIX_C_SOURCE=200809L \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_MESH_ASSET_DIR)/include \
		-I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include \
		-I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-o $@ $(PROCEDURAL_IMPORTED_SURFACE_STRANDS_TOOL_SRCS) \
		$(JSON_LIBS) -lm

procedural-imported-surface-strands-tool: \
	$(PROCEDURAL_IMPORTED_SURFACE_STRANDS_TOOL_BIN)
	@echo "procedural imported surface strands tool ready: $<"

test-procedural-imported-surface-strands-psg23a: \
	$(PROCEDURAL_IMPORTED_SURFACE_STRANDS_TOOL_BIN) \
	$(PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_BIN)
	@$(MAKE) -C ../line_drawing imported_mesh_harness >/dev/null
	@python3 tests/integration/test_procedural_imported_surface_strands_psg23a.py \
		$(PROCEDURAL_IMPORTED_SURFACE_STRANDS_TOOL_BIN) \
		$(PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_BIN) \
		../../tools/procedural_object_authoring/procedural_stl_tool.py \
		../line_drawing/build/toolchains/clang/bin/imported_mesh_harness
	@echo "PSG-23A imported surface rooted strand lane passed"

test-procedural-imported-surface-strands-psg23a-visual-proof: \
	$(PROCEDURAL_IMPORTED_SURFACE_STRANDS_TOOL_BIN) \
	$(PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_BIN) \
	$(RAY_TRACING_RENDER_HEADLESS_BIN)
	@$(MAKE) -C ../line_drawing imported_mesh_harness >/dev/null
	@python3 tools/procedural_imported_surface_strands_visual_proof.py
	@echo "PSG-23A imported surface rooted strand visual proof passed"

RUNTIME_CURVE_BLAS_PSG23B_TEST_BIN := \
	$(BUILD_DIR)/tests/runtime_curve_blas_psg23b_test
RUNTIME_CURVE_BLAS_PSG23B_TEST_SRCS := \
	$(TEST_DIR)/test_runtime_curve_blas_psg23b.c \
	$(TEST_DIR)/test_runtime_material_payload_stub.c \
	$(SRC_DIR)/procedural/procedural_imported_surface_strand_curve.c \
	$(SRC_DIR)/render/runtime_curve_primitive_3d.c \
	$(SRC_DIR)/render/runtime_curve_blas_3d.c \
	$(SRC_DIR)/render/runtime_scene_curve_3d.c \
	$(SRC_DIR)/render/runtime_ray_3d.c \
	$(SRC_DIR)/render/runtime_scene_3d.c \
	$(SRC_DIR)/render/runtime_emissive_light_set_3d.c \
	$(SRC_DIR)/render/runtime_light_set_3d.c \
	$(SRC_DIR)/render/runtime_environment_3d.c \
	$(SRC_DIR)/render/runtime_triangle_bvh_3d.c \
	$(SRC_DIR)/render/runtime_triangle_bvh_trace_3d.c \
	$(SRC_DIR)/render/runtime_volume_3d.c

$(RUNTIME_CURVE_BLAS_PSG23B_TEST_BIN): \
	$(RUNTIME_CURVE_BLAS_PSG23B_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror \
		-Wno-unknown-attributes -Wno-c23-extensions -g \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_MESH_ASSET_DIR)/include \
		-I$(CORE_MESH_PREVIEW_DIR)/include \
		-I$(CORE_MESH_PREVIEW_DIR)/../../shape/external \
		-I$(CORE_BASE_DIR)/include \
		-I$(CORE_OBJECT_DIR)/include \
		-I$(CORE_UNITS_DIR)/include \
		-o $@ $(RUNTIME_CURVE_BLAS_PSG23B_TEST_SRCS) -lm

test-procedural-imported-surface-strands-psg23b: \
	$(RUNTIME_CURVE_BLAS_PSG23B_TEST_BIN)
	@$(RUNTIME_CURVE_BLAS_PSG23B_TEST_BIN)
	@echo "PSG-23B native curve primitive and BLAS lane passed"

test-procedural-imported-surface-strands-psg23b-visual-proof: \
	$(RUNTIME_CURVE_BLAS_PSG23B_TEST_BIN)
	@python3 tools/procedural_imported_surface_curve_parity_proof.py
	@echo "PSG-23B native curve parity visual proof passed"

RUNTIME_CURVE_SCENE_PSG23C_TEST_BIN := \
	$(BUILD_DIR)/tests/runtime_curve_scene_psg23c_test
RUNTIME_CURVE_SCENE_PSG23C_TEST_SRCS := \
	$(TEST_DIR)/test_runtime_curve_scene_psg23c.c \
	$(TEST_DIR)/test_runtime_curve_scene_psg23c_support.c \
	$(SRC_DIR)/render/runtime_curve_primitive_3d.c \
	$(SRC_DIR)/render/runtime_curve_blas_3d.c \
	$(SRC_DIR)/render/runtime_scene_curve_3d.c \
	$(SRC_DIR)/render/runtime_scene_accel_3d.c \
	$(SRC_DIR)/render/runtime_scene_accel_3d_instances.c \
	$(SRC_DIR)/render/runtime_ray_3d.c \
	$(SRC_DIR)/render/runtime_scene_3d.c \
	$(SRC_DIR)/render/runtime_emissive_light_set_3d.c \
	$(SRC_DIR)/render/runtime_light_set_3d.c \
	$(SRC_DIR)/render/runtime_environment_3d.c \
	$(SRC_DIR)/render/runtime_triangle_bvh_3d.c \
	$(SRC_DIR)/render/runtime_triangle_bvh_trace_3d.c \
	$(SRC_DIR)/render/runtime_volume_3d.c

$(RUNTIME_CURVE_SCENE_PSG23C_TEST_BIN): \
	$(RUNTIME_CURVE_SCENE_PSG23C_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror \
		-Wno-unknown-attributes -Wno-c23-extensions -g \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_MESH_ASSET_DIR)/include \
		-I$(CORE_MESH_PREVIEW_DIR)/include \
		-I$(CORE_MESH_PREVIEW_DIR)/../../shape/external \
		-I$(CORE_BASE_DIR)/include \
		-I$(CORE_OBJECT_DIR)/include \
		-I$(CORE_UNITS_DIR)/include \
		-o $@ $(RUNTIME_CURVE_SCENE_PSG23C_TEST_SRCS) -lm

test-procedural-imported-surface-strands-psg23c: \
	$(RUNTIME_CURVE_SCENE_PSG23C_TEST_BIN)
	@$(RUNTIME_CURVE_SCENE_PSG23C_TEST_BIN)
	@echo "PSG-23C curve scene TLAS and material dispatch lane passed"

test-procedural-imported-surface-strands-psg23d: \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless
	@python3 tests/integration/test_procedural_curve_asset_psg23d.py \
		tools/procedural_curve_asset_authoring.py \
		$(BUILD_DIR)/tools/cli/ray_tracing_render_headless \
		tests/fixtures/procedural_curve_assets_psg23d/baseline.curve_authoring.json

test-procedural-imported-surface-strands-psg23d-visual-proof: \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless
	@python3 tools/procedural_curve_asset_visual_proof.py

test-procedural-imported-surface-strands-psg23e: \
	$(PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_BIN) \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless
	@$(MAKE) -C ../line_drawing imported_mesh_harness >/dev/null
	@python3 tests/integration/test_procedural_carrier_curve_groom_psg23e.py \
		tools/procedural_carrier_curve_groom_authoring.py \
		$(PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_BIN) \
		../../tools/procedural_object_authoring/procedural_stl_tool.py \
		../line_drawing/build/toolchains/clang/bin/imported_mesh_harness \
		$(BUILD_DIR)/tools/cli/ray_tracing_render_headless \
		tests/fixtures/procedural_imported_surface_strands_psg23a
	@echo "PSG-23E carrier-aware guide/clump groom lane passed"

test-procedural-imported-surface-strands-psg23e-visual-proof: \
	$(PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_BIN) \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless
	@$(MAKE) -C ../line_drawing imported_mesh_harness >/dev/null
	@python3 tools/procedural_carrier_curve_groom_visual_proof.py
	@echo "PSG-23E carrier-aware guide/clump visual proof passed"

RUNTIME_CURVE_DENSITY_PSG23F_TEST_BIN := \
	$(BUILD_DIR)/tests/runtime_curve_density_psg23f_test
RUNTIME_CURVE_DENSITY_PSG23F_TEST_SRCS := \
	$(TEST_DIR)/test_runtime_curve_density_psg23f.c \
	$(TEST_DIR)/test_runtime_material_payload_stub.c \
	$(SRC_DIR)/render/runtime_curve_primitive_3d.c \
	$(SRC_DIR)/render/runtime_curve_blas_3d.c \
	$(SRC_DIR)/render/runtime_scene_curve_3d.c \
	$(SRC_DIR)/render/runtime_ray_3d.c \
	$(SRC_DIR)/render/runtime_scene_3d.c \
	$(SRC_DIR)/render/runtime_emissive_light_set_3d.c \
	$(SRC_DIR)/render/runtime_light_set_3d.c \
	$(SRC_DIR)/render/runtime_environment_3d.c \
	$(SRC_DIR)/render/runtime_triangle_bvh_3d.c \
	$(SRC_DIR)/render/runtime_triangle_bvh_trace_3d.c \
	$(SRC_DIR)/render/runtime_volume_3d.c

$(RUNTIME_CURVE_DENSITY_PSG23F_TEST_BIN): \
	$(RUNTIME_CURVE_DENSITY_PSG23F_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror \
		-Wno-unknown-attributes -Wno-c23-extensions -g \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_MESH_ASSET_DIR)/include \
		-I$(CORE_MESH_PREVIEW_DIR)/include \
		-I$(CORE_MESH_PREVIEW_DIR)/../../shape/external \
		-I$(CORE_BASE_DIR)/include \
		-I$(CORE_OBJECT_DIR)/include \
		-I$(CORE_UNITS_DIR)/include \
		-o $@ $(RUNTIME_CURVE_DENSITY_PSG23F_TEST_SRCS) -lm

test-procedural-imported-surface-strands-psg23f: \
	$(RUNTIME_CURVE_DENSITY_PSG23F_TEST_BIN) \
	$(PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_BIN) \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless
	@$(MAKE) -C ../line_drawing imported_mesh_harness >/dev/null
	@$(RUNTIME_CURVE_DENSITY_PSG23F_TEST_BIN)
	@python3 tests/integration/test_procedural_curve_render_children_psg23f.py \
		tools/procedural_curve_render_children_authoring.py \
		tools/procedural_carrier_curve_groom_authoring.py \
		$(PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_BIN) \
		../../tools/procedural_object_authoring/procedural_stl_tool.py \
		../line_drawing/build/toolchains/clang/bin/imported_mesh_harness \
		$(BUILD_DIR)/tools/cli/ray_tracing_render_headless \
		tests/fixtures/procedural_imported_surface_strands_psg23a
	@echo "PSG-23F deterministic render-child density and LOD lane passed"

test-procedural-imported-surface-strands-psg23f-visual-proof: \
	$(RUNTIME_CURVE_DENSITY_PSG23F_TEST_BIN) \
	$(PROCEDURAL_IMPORTED_SURFACE_REGION_TOOL_BIN) \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless
	@$(MAKE) -C ../line_drawing imported_mesh_harness >/dev/null
	@$(RUNTIME_CURVE_DENSITY_PSG23F_TEST_BIN)
	@python3 tools/procedural_curve_render_children_visual_proof.py
	@echo "PSG-23F guide-to-render-child density visual proof passed"

RUNTIME_HAIR_SCATTERING_PSG23G_TEST_BIN := \
	$(BUILD_DIR)/tests/runtime_hair_scattering_psg23g_test
RUNTIME_HAIR_SCATTERING_PSG23G_TEST_SRCS := \
	$(TEST_DIR)/test_runtime_hair_scattering_psg23g.c \
	$(SRC_DIR)/render/runtime_hair_scattering_3d.c

$(RUNTIME_HAIR_SCATTERING_PSG23G_TEST_BIN): \
	$(RUNTIME_HAIR_SCATTERING_PSG23G_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror \
		-Wno-unknown-attributes -Wno-c23-extensions -g \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_MESH_ASSET_DIR)/include \
		-I$(CORE_MESH_PREVIEW_DIR)/include \
		-I$(CORE_MESH_PREVIEW_DIR)/../../shape/external \
		-I$(CORE_BASE_DIR)/include \
		-I$(CORE_OBJECT_DIR)/include \
		-I$(CORE_UNITS_DIR)/include \
		-o $@ $(RUNTIME_HAIR_SCATTERING_PSG23G_TEST_SRCS) -lm

test-procedural-imported-surface-strands-psg23g: \
	$(RUNTIME_HAIR_SCATTERING_PSG23G_TEST_BIN) \
	$(BUILD_DIR)/tools/cli/ray_tracing_render_headless
	@$(RUNTIME_HAIR_SCATTERING_PSG23G_TEST_BIN)
	@echo "PSG-23G Disney-v2 single-fiber hair scattering lane passed"

RUNTIME_MESH_ASSET_LOADER_TEST_BIN := $(BUILD_DIR)/tests/runtime_mesh_asset_loader_test
RUNTIME_MESH_ASSET_LOADER_TEST_SRCS := \
	$(TEST_DIR)/test_runtime_mesh_asset_loader.c \
	$(SRC_DIR)/import/runtime_mesh_asset_loader.c \
	$(SRC_DIR)/import/runtime_mesh_asset_loader_authored_material.c \
	$(SRC_DIR)/procedural/procedural_surface_wood_grain.c \
	$(SRC_DIR)/import/runtime_mesh_asset_loader_cache.c \
	$(SRC_DIR)/import/runtime_mesh_asset_loader_utils.c \
	$(SRC_DIR)/import/runtime_mesh_asset_pack.c \
	$(PROCEDURAL_SURFACE_DERIVED_ASSET_LOAD_SRCS) \
	$(PROCEDURAL_SOLID_MATERIAL_BINDING_LOAD_SRCS) \
	$(PROCEDURAL_SOLID_MATERIAL_GRAPH_LOAD_SRCS) \
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
	$(SRC_DIR)/import/runtime_mesh_asset_loader_authored_material.c \
	$(SRC_DIR)/import/runtime_mesh_asset_loader_cache.c \
	$(SRC_DIR)/import/runtime_mesh_asset_loader_utils.c \
	$(SRC_DIR)/import/runtime_mesh_asset_pack.c \
	$(PROCEDURAL_SURFACE_DERIVED_ASSET_LOAD_SRCS) \
	$(PROCEDURAL_SOLID_MATERIAL_BINDING_LOAD_SRCS) \
	$(PROCEDURAL_SOLID_MATERIAL_GRAPH_LOAD_SRCS) \
	$(CORE_MESH_PREVIEW_DIR)/src/core_mesh_preview.c \
	$(SRC_DIR)/render/runtime_curve_primitive_3d.c \
	$(SRC_DIR)/render/runtime_curve_blas_3d.c \
	$(SRC_DIR)/render/runtime_scene_curve_3d.c \
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
	$(TEST_DIR)/test_runtime_curve_asset_loader_noop_stub.c \
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

PROCEDURAL_SOLID_MATERIAL_RUNTIME_TEST_BIN := \
	$(BUILD_DIR)/tests/procedural_solid_material_runtime_test
PROCEDURAL_SOLID_MATERIAL_RUNTIME_TEST_SRCS := \
	$(TEST_DIR)/test_procedural_solid_material_runtime.c \
	$(TEST_DIR)/test_runtime_material_payload_stub.c \
	$(SRC_DIR)/render/runtime_curve_primitive_3d.c \
	$(SRC_DIR)/render/runtime_curve_blas_3d.c \
	$(SRC_DIR)/render/runtime_scene_curve_3d.c \
	$(SRC_DIR)/render/runtime_ray_3d.c \
	$(SRC_DIR)/render/runtime_scene_3d.c \
	$(SRC_DIR)/render/runtime_emissive_light_set_3d.c \
	$(SRC_DIR)/render/runtime_environment_3d.c \
	$(SRC_DIR)/render/runtime_light_set_3d.c \
	$(SRC_DIR)/render/runtime_dynamic_geometry_accel_3d.c \
	$(SRC_DIR)/import/runtime_mesh_asset_loader_authored_material.c \
	$(SRC_DIR)/import/runtime_mesh_asset_pack.c \
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
	$(TEST_DIR)/test_runtime_curve_asset_loader_noop_stub.c \
	$(PROCEDURAL_SOLID_MATERIAL_BINDING_LOAD_SRCS) \
	$(PROCEDURAL_SOLID_MATERIAL_GRAPH_LOAD_SRCS) \
	$(SRC_DIR)/app/ray_tracing_sha256.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset.c \
	$(CORE_MESH_ASSET_DIR)/src/core_mesh_asset_runtime_document.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_SCENE_DIR)/src/core_scene.c \
	$(CORE_OBJECT_DIR)/src/core_object.c \
	$(CORE_UNITS_DIR)/src/core_units.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_MESH_ASSET_DIR)/../../shape/external/cjson/cJSON.c

$(PROCEDURAL_SOLID_MATERIAL_RUNTIME_TEST_BIN): \
	$(PROCEDURAL_SOLID_MATERIAL_RUNTIME_TEST_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror \
		-Wno-unknown-attributes -Wno-c23-extensions -g \
		$(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_MESH_ASSET_DIR)/include \
		-I$(CORE_MESH_PREVIEW_DIR)/include \
		-I$(CORE_IO_DIR)/include -I$(CORE_SCENE_DIR)/include \
		-I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include \
		-I$(CORE_BASE_DIR)/include \
		-o $@ $(PROCEDURAL_SOLID_MATERIAL_RUNTIME_TEST_SRCS) \
		$(JSON_LIBS) -lm

test-procedural-solid-material-runtime: \
	$(PROCEDURAL_SOLID_MATERIAL_RUNTIME_TEST_BIN)
	@$(PROCEDURAL_SOLID_MATERIAL_RUNTIME_TEST_BIN)

test-smooth-mesh-reflection-fixtures: $(SMOOTH_MESH_RUNTIME_COMPILE_TOOL_BIN)
	bash tests/integration/run_smooth_mesh_reflection_fixture_smoke.sh

test-smooth-mesh-reflection-matrix: $(SMOOTH_MESH_RUNTIME_COMPILE_TOOL_BIN) $(RAY_TRACING_RENDER_HEADLESS_BIN)
	bash tests/integration/run_smooth_mesh_reflection_matrix.sh

RUNTIME_MESH_ASSET_HEADLESS_AUDIT_TEST_BIN := $(BUILD_DIR)/tests/runtime_mesh_asset_headless_audit_test
RUNTIME_MESH_ASSET_HEADLESS_AUDIT_TEST_SRCS := \
	$(TEST_DIR)/test_runtime_mesh_asset_headless_audit.c \
	$(TEST_DIR)/test_runtime_material_payload_stub.c \
	$(SRC_DIR)/import/runtime_mesh_asset_loader.c \
	$(SRC_DIR)/import/runtime_mesh_asset_loader_authored_material.c \
	$(SRC_DIR)/import/runtime_mesh_asset_loader_cache.c \
	$(SRC_DIR)/import/runtime_mesh_asset_loader_utils.c \
	$(SRC_DIR)/import/runtime_mesh_asset_pack.c \
	$(PROCEDURAL_SURFACE_DERIVED_ASSET_LOAD_SRCS) \
	$(PROCEDURAL_SOLID_MATERIAL_BINDING_LOAD_SRCS) \
	$(PROCEDURAL_SOLID_MATERIAL_GRAPH_LOAD_SRCS) \
	$(CORE_MESH_PREVIEW_DIR)/src/core_mesh_preview.c \
	$(SRC_DIR)/render/runtime_camera_3d_rays.c \
	$(SRC_DIR)/render/runtime_curve_primitive_3d.c \
	$(SRC_DIR)/render/runtime_curve_blas_3d.c \
	$(SRC_DIR)/render/runtime_scene_curve_3d.c \
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
	$(TEST_DIR)/test_runtime_curve_asset_loader_noop_stub.c \
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
	$(SRC_DIR)/render/runtime_curve_primitive_3d.c \
	$(SRC_DIR)/render/runtime_curve_blas_3d.c \
	$(SRC_DIR)/render/runtime_scene_curve_3d.c \
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
	$(SRC_DIR)/ui/menu/workspace_authoring/ray_tracing_surface_authoring_canvas.c \
	$(SRC_DIR)/ui/menu/workspace_authoring/ray_tracing_surface_authoring_canvas_view.c \
	$(SRC_DIR)/procedural/procedural_surface_authoring_document.c \
	$(SRC_DIR)/app/ray_tracing_sha256.c \
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

RAY_TRACING_SURFACE_AUTHORING_CANVAS_TEST_BIN := $(BUILD_DIR)/tests/ray_tracing_surface_authoring_canvas_test
RAY_TRACING_SURFACE_AUTHORING_CANVAS_TEST_SRCS := \
	$(TEST_DIR)/test_ray_tracing_surface_authoring_canvas.c \
	$(SRC_DIR)/ui/menu/workspace_authoring/ray_tracing_surface_authoring_canvas.c \
	$(SRC_DIR)/ui/menu/workspace_authoring/ray_tracing_surface_authoring_canvas_view.c

$(RAY_TRACING_SURFACE_AUTHORING_CANVAS_TEST_BIN): $(RAY_TRACING_SURFACE_AUTHORING_CANVAS_TEST_SRCS) \
		tests/fixtures/procedural_surface_authoring_document_v1/cube_composition.canvas.json
	@mkdir -p $(dir $@)
	$(CC) $(CSTD) -Wall -Wextra -Wpedantic -Werror -g $(JSON_CFLAGS) -I$(INC_DIR) -I$(SRC_DIR) \
		$(RAY_TRACING_SURFACE_AUTHORING_CANVAS_TEST_SRCS) -o $@ $(JSON_LIBS) -lm

test-ray-tracing-surface-authoring-canvas: $(RAY_TRACING_SURFACE_AUTHORING_CANVAS_TEST_BIN)
	@$(RAY_TRACING_SURFACE_AUTHORING_CANVAS_TEST_BIN) || (echo "ray tracing surface authoring canvas test failed."; exit 1)

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

test-ray-tracing-animated-water-photon-caustics: $(RAY_TRACING_RENDER_HEADLESS_BIN)
	python3 tests/integration/run_ray_tracing_animated_water_photon_caustics.py

test-ray-tracing-job-runner-smoke: \
	$(RAY_TRACING_RENDER_HEADLESS_BIN) \
	$(RAY_TRACING_JOB_RUNNER_BIN) \
	$(RAY_TRACING_WORKER_RUNTIME_BIN)
	tests/integration/run_ray_tracing_job_runner_smoke.sh

test-ray-tracing-job-runner-bundle-smoke: \
	$(RAY_TRACING_RENDER_HEADLESS_BIN) \
	$(RAY_TRACING_JOB_RUNNER_BIN) \
	$(RAY_TRACING_WORKER_RUNTIME_BIN)
	tests/integration/run_ray_tracing_job_runner_bundle_smoke.sh

test-ray-tracing-job-runner-policy: $(RAY_TRACING_RENDER_HEADLESS_BIN) $(RAY_TRACING_WORKER_RUNTIME_BIN) $(RAY_TRACING_JOB_RUNNER_BIN)
	tests/integration/run_ray_tracing_job_runner_policy.sh

test-ray-tracing-worker-protocol-phase-b: \
	$(RAY_TRACING_RENDER_HEADLESS_BIN) \
	$(RAY_TRACING_WORKER_RUNTIME_BIN) \
	$(RAY_TRACING_JOB_RUNNER_BIN)
	bash tests/integration/run_ray_tracing_worker_protocol_phase_b.sh

test-ray-tracing-temporal-checkpoint-phase-c: \
	ray-tracing-render-headless ray-tracing-job-runner ray-tracing-worker-runtime
	bash tests/integration/run_ray_tracing_temporal_checkpoint_phase_c.sh

test-ray-tracing-tile-batch-checkpoint-phase-d: \
	ray-tracing-render-headless ray-tracing-job-runner ray-tracing-worker-runtime
	bash tests/integration/run_ray_tracing_tile_batch_checkpoint_phase_d.sh

test-ray-tracing-fleet-recovery-phase-e: \
	ray-tracing-job-runner ray-tracing-worker-runtime test-ray-tracing-recovery-authority
	bash tests/integration/run_ray_tracing_fleet_recovery_phase_e.sh

test-ray-tracing-wtr66-preview-matrix-planner-dry-run:
	bash tests/integration/run_ray_tracing_wtr66_preview_matrix_planner_dry_run.sh

test-ray-tracing-wtr66-preview-matrix-local-job-runner: $(RAY_TRACING_RENDER_HEADLESS_BIN) $(RAY_TRACING_JOB_RUNNER_BIN)
	bash tests/integration/run_ray_tracing_wtr66_preview_matrix_local_job_runner.sh

test-ray-tracing-evaluated-scene-preview-parity:
	TEST_RUNNER_GROUP=runtime_evaluated_scene_preview $(MAKE) test

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
