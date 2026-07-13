#include "test_runner_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fluid_pack_import_test.h"
#include "test_config_animation_internal.h"
#include "test_fluid_volume_pack_import_3d.h"
#include "test_fluid_volume_import_3d.h"
#include "import/runtime_scene_bridge.h"
#include "kit_viz_fluid_overlay_adapter_test.h"
#include "render_metrics_dataset_test.h"
#include "test_runtime_diffuse_temporal.h"
#include "test_runtime_emission_transparency.h"
#include "test_runtime_caustic_bootstrap_3d.h"
#include "test_runtime_caustic_beam_map_3d.h"
#include "test_runtime_caustic_lens_transport_3d.h"
#include "test_runtime_caustic_photon_emit_3d.h"
#include "test_runtime_caustic_photon_integration_3d.h"
#include "test_runtime_caustic_photon_map_3d.h"
#include "test_runtime_caustic_photon_medium_stack_3d.h"
#include "test_runtime_caustic_photon_path_transport_3d.h"
#include "test_runtime_caustic_photon_bsdf_policy_3d.h"
#include "test_runtime_caustic_photon_bsdf_sampling_3d.h"
#include "test_runtime_caustic_photon_scene_population_3d.h"
#include "test_runtime_caustic_photon_scene_trace_3d.h"
#include "test_runtime_caustic_photon_trace_3d.h"
#include "test_runtime_caustic_sphere_lens_3d.h"
#include "test_runtime_caustic_surface_cache_3d.h"
#include "test_runtime_caustic_transport_3d.h"
#include "test_runtime_caustic_volume_cache_3d.h"
#include "test_runtime_native_3d_denoise.h"
#include "test_runtime_native_3d_render.h"
#include "test_runtime_native_3d_render_internal.h"
#include "test_runtime_native_3d_render_prepared_suite_internal.h"
#include "test_runtime_render_metrics_export.h"
#include "test_runtime_preview_editor.h"
#include "test_runtime_scene_editor.h"
#include "test_runtime_path_policy.h"
#include "test_runtime_mode_backend_policy.h"
#include "test_runtime_object_motion_tracks.h"
#include "test_runtime_lighting_materials.h"
#include "test_runtime_lighting_materials_internal.h"
#include "test_runtime_mesh_asset_loader.h"
#include "test_runtime_scene_3d_geometry.h"
#include "test_runtime_volume_3d.h"
#include "test_water_surface_runtime.h"
#include "test_runtime_scene_bridge_core.h"
#include "test_runtime_scene_bridge_writeback.h"
#include "test_support.h"

int run_test_runner_runtime_tests(void);
int run_test_config_animation_tests(void);
int run_test_ui_menu_contract_tests(void);
int run_test_runtime_native_3d_async_render_bridge_tests(void);
int run_test_runtime_native_3d_async_render_job_tests(void);

typedef int (*TestGroupFn)(void);

typedef struct TestGroup {
    const char* name;
    TestGroupFn run;
} TestGroup;

static int run_bridge_apply_file_mode(const char* runtime_scene_path) {
    RuntimeSceneBridgePreflight preflight;
    RuntimeSceneBridgePreflight summary;
    bool ok = false;
    if (!runtime_scene_path || !runtime_scene_path[0]) {
        fprintf(stderr, "runtime_scene_bridge_apply_file: missing path\n");
        return EXIT_FAILURE;
    }
    ok = runtime_scene_bridge_preflight_file(runtime_scene_path, &preflight);
    if (!ok) {
        fprintf(stderr, "runtime_scene_bridge_preflight_file failed: %s\n", preflight.diagnostics);
        return EXIT_FAILURE;
    }
    ok = runtime_scene_bridge_apply_file(runtime_scene_path, &summary);
    if (!ok) {
        fprintf(stderr, "runtime_scene_bridge_apply_file failed: %s\n", summary.diagnostics);
        return EXIT_FAILURE;
    }
    printf("runtime_scene_bridge_apply_file: PASS scene_id=%s objects=%d materials=%d lights=%d cameras=%d\n",
           summary.scene_id,
           summary.object_count,
           summary.material_count,
           summary.light_count,
           summary.camera_count);
    return EXIT_SUCCESS;
}

static int run_bridge_apply_file_defer_mesh_assets_mode(const char* runtime_scene_path) {
    RuntimeSceneBridgePreflight summary;
    bool ok = false;
    if (!runtime_scene_path || !runtime_scene_path[0]) {
        fprintf(stderr, "runtime_scene_bridge_apply_file_defer_mesh_assets: missing path\n");
        return EXIT_FAILURE;
    }
    ok = runtime_scene_bridge_apply_file_defer_mesh_assets(runtime_scene_path, &summary);
    if (!ok) {
        fprintf(stderr,
                "runtime_scene_bridge_apply_file_defer_mesh_assets failed: %s\n",
                summary.diagnostics);
        return EXIT_FAILURE;
    }
    printf("runtime_scene_bridge_apply_file_defer_mesh_assets: PASS scene_id=%s objects=%d materials=%d lights=%d cameras=%d\n",
           summary.scene_id,
           summary.object_count,
           summary.material_count,
           summary.light_count,
           summary.camera_count);
    return EXIT_SUCCESS;
}

int test_runner_main(int argc, char** argv) {
    static const TestGroup groups[] = {
        {"config_animation", run_test_config_animation_tests},
        {"config_animation_settings_export", run_test_config_animation_settings_export_suite},
        {"ui_menu_contracts", run_test_ui_menu_contract_tests},
        {"runtime_scene_bridge_core", run_test_runtime_scene_bridge_core_tests},
        {"runtime_scene_bridge_writeback", run_test_runtime_scene_bridge_writeback_tests},
        {"runtime_scene_3d_geometry", run_test_runtime_scene_3d_geometry_tests},
        {"runtime_volume_3d", run_test_runtime_volume_3d_tests},
        {"runtime_caustic_bootstrap_3d", run_test_runtime_caustic_bootstrap_3d_tests},
        {"runtime_caustic_beam_map_3d", run_test_runtime_caustic_beam_map_3d_tests},
        {"runtime_caustic_lens_transport_3d", run_test_runtime_caustic_lens_transport_3d_tests},
        {"runtime_caustic_photon_emit_3d", run_test_runtime_caustic_photon_emit_3d_tests},
        {"runtime_caustic_photon_integration_3d",
         run_test_runtime_caustic_photon_integration_3d_tests},
        {"runtime_caustic_photon_map_3d", run_test_runtime_caustic_photon_map_3d_tests},
        {"runtime_caustic_photon_medium_stack_3d",
         run_test_runtime_caustic_photon_medium_stack_3d_tests},
        {"runtime_caustic_photon_path_transport_3d",
         run_test_runtime_caustic_photon_path_transport_3d_tests},
        {"runtime_caustic_photon_bsdf_policy_3d",
         run_test_runtime_caustic_photon_bsdf_policy_3d_tests},
        {"runtime_caustic_photon_bsdf_sampling_3d",
         run_test_runtime_caustic_photon_bsdf_sampling_3d_tests},
        {"runtime_caustic_photon_scene_population_3d",
         run_test_runtime_caustic_photon_scene_population_3d_tests},
        {"runtime_caustic_photon_scene_trace_3d",
         run_test_runtime_caustic_photon_scene_trace_3d_tests},
        {"runtime_caustic_photon_trace_3d", run_test_runtime_caustic_photon_trace_3d_tests},
        {"runtime_caustic_sphere_lens_3d", run_test_runtime_caustic_sphere_lens_3d_tests},
        {"runtime_caustic_surface_cache_3d", run_test_runtime_caustic_surface_cache_3d_tests},
        {"runtime_caustic_transport_3d", run_test_runtime_caustic_transport_3d_tests},
        {"runtime_caustic_volume_cache_3d", run_test_runtime_caustic_volume_cache_3d_tests},
        {"water_surface_runtime", run_test_water_surface_runtime_tests},
        {"runtime_lighting_materials_payload", run_test_runtime_lighting_materials_payload_tests},
        {"runtime_lighting_materials_direct_light", run_test_runtime_lighting_materials_direct_light_suite},
        {"runtime_lighting_materials", run_test_runtime_lighting_materials_tests},
        {"runtime_mesh_asset_loader", run_test_runtime_mesh_asset_loader_tests},
        {"runtime_diffuse_temporal", run_test_runtime_diffuse_temporal_tests},
        {"runtime_emission_transparency", run_test_runtime_emission_transparency_tests},
        {"runtime_native_3d_denoise", run_test_runtime_native_3d_denoise_tests},
        {"runtime_native_3d_async_render_bridge", run_test_runtime_native_3d_async_render_bridge_tests},
        {"runtime_native_3d_async_render_job", run_test_runtime_native_3d_async_render_job_tests},
        {"runtime_native_3d_render", run_test_runtime_native_3d_render_tests},
        {"runtime_native_3d_render_live", run_test_runtime_native_3d_render_live_suite},
        {"runtime_native_3d_render_prepared", run_test_runtime_native_3d_render_prepared_suite},
        {"runtime_native_3d_render_prepared_parity_volume",
         run_test_runtime_native_3d_render_prepared_parity_volume_suite},
        {"runtime_native_3d_render_prepared_scatter_preview",
         run_test_runtime_native_3d_render_prepared_scatter_preview_suite},
        {"runtime_render_metrics_export", run_test_runtime_render_metrics_export_tests},
        {"runtime_preview_editor", run_test_runtime_preview_editor_tests},
        {"runtime_scene_editor", run_test_runtime_scene_editor_tests},
        {"runtime_path_policy", run_test_runtime_path_policy_tests},
        {"runtime_mode_backend_policy", run_test_runtime_mode_backend_policy_tests},
        {"runtime_object_motion_tracks", run_test_runtime_object_motion_tracks_tests},
        {"runtime_and_editor", run_test_runner_runtime_tests},
        {"fluid_volume_import_3d", run_test_fluid_volume_import_3d_tests},
        {"fluid_volume_pack_import_3d", run_test_fluid_volume_pack_import_3d_tests},
        {"fluid_pack_import", run_fluid_pack_import_tests},
        {"kit_viz_fluid_overlay_adapter", run_kit_viz_fluid_overlay_adapter_tests},
        {"render_metrics_dataset", run_render_metrics_dataset_tests},
    };

    int failures = 0;
    const char* only_group = getenv("TEST_RUNNER_GROUP");
    bool trace_groups = false;
    const char* trace_env = getenv("TEST_RUNNER_TRACE_GROUPS");

    if (trace_env && trace_env[0] && strcmp(trace_env, "0") != 0) {
        trace_groups = true;
    }

    if (argc == 3 && strcmp(argv[1], "--bridge-apply-file") == 0) {
        return run_bridge_apply_file_mode(argv[2]);
    }
    if (argc == 3 && strcmp(argv[1], "--bridge-apply-file-defer-mesh-assets") == 0) {
        return run_bridge_apply_file_defer_mesh_assets_mode(argv[2]);
    }
    if (argc != 1) {
        fprintf(stderr,
                "usage: %s [--bridge-apply-file <scene_runtime.json>] [--bridge-apply-file-defer-mesh-assets <scene_runtime.json>]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    test_support_reset_failures();
    for (size_t i = 0; i < sizeof(groups) / sizeof(groups[0]); ++i) {
        if (only_group && only_group[0] && strcmp(only_group, groups[i].name) != 0) {
            continue;
        }
        if (trace_groups) {
            fprintf(stderr, "TEST GROUP: %s\n", groups[i].name);
            fflush(stderr);
        }
        failures += groups[i].run();
    }

    if (failures > 0) {
        printf("TEST RESULT: %d failure(s)\n", failures);
        return EXIT_FAILURE;
    }
    printf("TEST RESULT: PASS\n");
    return EXIT_SUCCESS;
}
