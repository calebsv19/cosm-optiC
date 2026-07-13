#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app/animation.h"
#include "app/animation_output.h"
#include "app/agent_render_request.h"
#include "app/data_paths.h"
#include "app/render_export_batch.h"
#include "config/config_manager.h"
#include "render/pipeline/ray_tracing2_native3d_overlay.h"
#include "render/pipeline/ray_tracing2_preview_present.h"
#include "render/ray_tracing_integrator_catalog.h"
#include "render/runtime_native_3d_resolution.h"
#include "test_config_animation_internal.h"
#include "test_support.h"

static int test_animation_integrator_split_roundtrip_and_default_3d(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    const char* json_missing_3d_integrator =
        "{\n"
        "  \"spaceMode\": 1,\n"
        "  \"integratorMode\": 1\n"
        "}\n";

    assert_true("integrator_split_write_missing_3d",
                write_text_file(kRuntimeAnimationConfigPath, json_missing_3d_integrator));
    LoadAnimationConfig();
    assert_true("integrator_split_missing_3d_defaulted",
                animSettings.integratorMode3D == RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT);
    assert_true("integrator_split_2d_preserved_on_load",
                animSettings.integratorMode == RAY_TRACING_2D_INTEGRATOR_HYBRID);

    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = RAY_TRACING_2D_INTEGRATOR_HYBRID;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DISNEY;
    SaveAnimationConfig();
    animSettings.integratorMode = RAY_TRACING_2D_INTEGRATOR_FORWARD_LIGHT;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT;
    LoadAnimationConfig();

    assert_true("integrator_split_roundtrip_2d_persisted",
                animSettings.integratorMode == RAY_TRACING_2D_INTEGRATOR_HYBRID);
    assert_true("integrator_split_roundtrip_3d_persisted",
                animSettings.integratorMode3D == RAY_TRACING_3D_INTEGRATOR_DISNEY);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_agent_render_request_disney_v2_integrator_label_roundtrip(void) {
    char request_path[PATH_MAX];
    char diagnostics[256];
    RayTracingAgentRenderRequest request;
    const char* json_text =
        "{\n"
        "  \"schema_version\": \"ray_tracing_agent_render_request_v1\",\n"
        "  \"run_id\": \"disney_v2_integrator_label_test\",\n"
        "  \"scene\": {\"runtime_scene_path\": \"scene_runtime.json\"},\n"
        "  \"render\": {\"integrator_3d\": \"disney_v2\"}\n"
        "}\n";

    snprintf(request_path,
             sizeof(request_path),
             "%s",
             "/tmp/ray_tracing_agent_render_disney_v2_request.json");
    assert_true("agent_render_disney_v2_request_write",
                write_text_file(request_path, json_text));
    assert_true("agent_render_disney_v2_request_load",
                ray_tracing_agent_render_request_load_file(request_path,
                                                           &request,
                                                           diagnostics,
                                                           sizeof(diagnostics)));
    assert_true("agent_render_disney_v2_request_override",
                request.has_integrator_3d_override);
    assert_true("agent_render_disney_v2_request_id",
                request.integrator_3d == RAY_TRACING_3D_INTEGRATOR_DISNEY_V2);
    assert_true("agent_render_disney_v2_caustic_default_mode",
                request.caustic_mode == RUNTIME_DISNEY_V2_CAUSTIC_MODE_OFF);
    assert_true("agent_render_disney_v2_caustic_default_disabled",
                !request.caustic_sidecar_enabled);
    assert_true("agent_render_disney_v2_label",
                strcmp(ray_tracing_agent_render_request_integrator_label(
                           RAY_TRACING_3D_INTEGRATOR_DISNEY_V2),
                       "disney_v2") == 0);
    assert_true("agent_render_current_disney_label_stable",
                strcmp(ray_tracing_agent_render_request_integrator_label(
                           RAY_TRACING_3D_INTEGRATOR_DISNEY),
                       "disney") == 0);
    unlink(request_path);
    return 0;
}

static int test_agent_render_request_denoise_override_roundtrip(void) {
    char request_path[PATH_MAX];
    char diagnostics[256];
    RayTracingAgentRenderRequest request;
    const char* json_text =
        "{\n"
        "  \"schema_version\": \"ray_tracing_agent_render_request_v1\",\n"
        "  \"run_id\": \"disney_v2_denoise_override_test\",\n"
        "  \"scene\": {\"runtime_scene_path\": \"scene_runtime.json\"},\n"
        "  \"render\": {\n"
        "    \"integrator_3d\": \"disney_v2\",\n"
        "    \"temporal_frames\": 12,\n"
        "    \"denoise_enabled\": false\n"
        "  }\n"
        "}\n";

    snprintf(request_path,
             sizeof(request_path),
             "%s",
             "/tmp/ray_tracing_agent_render_denoise_override_request.json");
    assert_true("agent_render_denoise_override_write",
                write_text_file(request_path, json_text));
    assert_true("agent_render_denoise_override_load",
                ray_tracing_agent_render_request_load_file(request_path,
                                                           &request,
                                                           diagnostics,
                                                           sizeof(diagnostics)));
    assert_true("agent_render_denoise_override_present",
                request.has_denoise_enabled_override);
    assert_true("agent_render_denoise_override_false",
                !request.denoise_enabled_override);
    assert_true("agent_render_denoise_override_temporal",
                request.temporal_frames == 12);
    assert_true("agent_render_denoise_override_integrator",
                request.integrator_3d == RAY_TRACING_3D_INTEGRATOR_DISNEY_V2);
    unlink(request_path);
    return 0;
}

static int test_agent_render_request_disney_v2_caustic_sidecar_roundtrip(void) {
    char request_path[PATH_MAX];
    char diagnostics[256];
    RayTracingAgentRenderRequest request;
    const char* json_text =
        "{\n"
        "  \"schema_version\": \"ray_tracing_agent_render_request_v1\",\n"
        "  \"run_id\": \"disney_v2_caustic_sidecar_test\",\n"
        "  \"scene\": {\"runtime_scene_path\": \"scene_runtime.json\"},\n"
        "  \"render\": {\"integrator_3d\": \"disney_v2\"},\n"
        "  \"inspection\": {\n"
        "    \"caustic_sidecar_enabled\": true,\n"
        "    \"caustic_sidecar_strength\": 2.5\n"
        "  }\n"
        "}\n";

    snprintf(request_path,
             sizeof(request_path),
             "%s",
             "/tmp/ray_tracing_agent_render_disney_v2_caustic_sidecar_request.json");
    assert_true("agent_render_caustic_sidecar_request_write",
                write_text_file(request_path, json_text));
    assert_true("agent_render_caustic_sidecar_request_load",
                ray_tracing_agent_render_request_load_file(request_path,
                                                           &request,
                                                           diagnostics,
                                                           sizeof(diagnostics)));
    assert_true("agent_render_caustic_sidecar_enabled",
                request.caustic_sidecar_enabled);
    assert_true("agent_render_caustic_sidecar_mode",
                request.caustic_mode == RUNTIME_DISNEY_V2_CAUSTIC_MODE_ANALYTIC);
    assert_true("agent_render_caustic_sidecar_strength_override",
                request.has_caustic_sidecar_strength_override);
    assert_close("agent_render_caustic_sidecar_strength",
                 request.caustic_sidecar_strength,
                 2.5,
                 1e-12);
    unlink(request_path);
    return 0;
}

static int test_agent_render_request_disney_v2_caustic_mode_off_roundtrip(void) {
    char request_path[PATH_MAX];
    char diagnostics[256];
    RayTracingAgentRenderRequest request;
    const char* json_text =
        "{\n"
        "  \"schema_version\": \"ray_tracing_agent_render_request_v1\",\n"
        "  \"run_id\": \"disney_v2_caustic_mode_off_test\",\n"
        "  \"scene\": {\"runtime_scene_path\": \"scene_runtime.json\"},\n"
        "  \"render\": {\"integrator_3d\": \"disney_v2\"},\n"
        "  \"inspection\": {\"caustic_mode\": \"off\"}\n"
        "}\n";

    snprintf(request_path,
             sizeof(request_path),
             "%s",
             "/tmp/ray_tracing_agent_render_disney_v2_caustic_off_request.json");
    assert_true("agent_render_caustic_off_request_write",
                write_text_file(request_path, json_text));
    assert_true("agent_render_caustic_off_request_load",
                ray_tracing_agent_render_request_load_file(request_path,
                                                           &request,
                                                           diagnostics,
                                                           sizeof(diagnostics)));
    assert_true("agent_render_caustic_off_mode",
                request.caustic_mode == RUNTIME_DISNEY_V2_CAUSTIC_MODE_OFF);
    assert_true("agent_render_caustic_off_disabled",
                !request.caustic_sidecar_enabled);
    unlink(request_path);
    return 0;
}

static int test_agent_render_request_spatial_caustic_cache_phase0_contract(void) {
    char request_path[PATH_MAX];
    char diagnostics[256];
    RayTracingAgentRenderRequest request;
    RuntimeCausticReadback3D readback;
    const char* json_text =
        "{\n"
        "  \"schema_version\": \"ray_tracing_agent_render_request_v1\",\n"
        "  \"run_id\": \"spatial_caustic_cache_phase0_test\",\n"
        "  \"scene\": {\"runtime_scene_path\": \"scene_runtime.json\"},\n"
        "  \"render\": {\"integrator_3d\": \"direct_light\"},\n"
        "  \"inspection\": {\n"
        "    \"caustic_mode\": \"spatial_cache\",\n"
        "    \"caustic_volume_enabled\": true,\n"
        "    \"caustic_surface_enabled\": true,\n"
        "    \"caustic_sample_budget\": 4096,\n"
        "    \"caustic_max_path_depth\": 5,\n"
        "    \"caustic_debug_summary\": true\n"
        "  }\n"
        "}\n";

    snprintf(request_path,
             sizeof(request_path),
             "%s",
             "/tmp/ray_tracing_agent_render_spatial_caustic_cache_phase0_request.json");
    assert_true("agent_render_spatial_caustic_phase0_request_write",
                write_text_file(request_path, json_text));
    assert_true("agent_render_spatial_caustic_phase0_request_load",
                ray_tracing_agent_render_request_load_file(request_path,
                                                           &request,
                                                           diagnostics,
                                                           sizeof(diagnostics)));
    assert_true("agent_render_spatial_caustic_phase0_mode",
                request.caustic_settings.mode == RUNTIME_CAUSTIC_MODE_SPATIAL_CACHE);
    assert_true("agent_render_spatial_caustic_phase0_disney_sidecar_off",
                request.caustic_mode == RUNTIME_DISNEY_V2_CAUSTIC_MODE_OFF);
    assert_true("agent_render_spatial_caustic_phase0_sidecar_disabled",
                !request.caustic_sidecar_enabled);
    assert_true("agent_render_spatial_caustic_phase0_volume_requested",
                request.caustic_settings.volumeCacheEnabled);
    assert_true("agent_render_spatial_caustic_phase0_surface_requested",
                request.caustic_settings.surfaceCacheEnabled);
    assert_true("agent_render_spatial_caustic_phase0_sample_budget",
                request.caustic_settings.sampleBudget == 4096);
    assert_true("agent_render_spatial_caustic_phase0_path_depth",
                request.caustic_settings.maxPathDepth == 5);
    assert_true("agent_render_spatial_caustic_phase0_debug",
                request.caustic_settings.debugSummaryEnabled);
    readback = RuntimeCausticSettings3D_Phase0Readback(&request.caustic_settings,
                                                       request.caustic_sidecar_enabled);
    assert_true("agent_render_spatial_caustic_phase0_readback_volume",
                readback.volumeCacheState ==
                    RUNTIME_CAUSTIC_CACHE_STATE_REQUESTED_NOT_ALLOCATED);
    assert_true("agent_render_spatial_caustic_phase0_readback_surface",
                readback.surfaceCacheState ==
                    RUNTIME_CAUSTIC_CACHE_STATE_REQUESTED_NOT_ALLOCATED);
    assert_true("agent_render_spatial_caustic_phase0_readback_no_path",
                !readback.pathEmissionActive);
    unlink(request_path);
    return 0;
}

static int test_agent_render_request_spatial_caustic_cache_phase3_combined_sidecar(void) {
    char request_path[PATH_MAX];
    char diagnostics[256];
    RayTracingAgentRenderRequest request;
    RuntimeCausticReadback3D readback;
    const char* json_text =
        "{\n"
        "  \"schema_version\": \"ray_tracing_agent_render_request_v1\",\n"
        "  \"run_id\": \"spatial_caustic_cache_phase3_combined_test\",\n"
        "  \"scene\": {\"runtime_scene_path\": \"scene_runtime.json\"},\n"
        "  \"render\": {\"integrator_3d\": \"disney_v2\"},\n"
        "  \"inspection\": {\n"
        "    \"caustic_mode\": \"spatial_cache\",\n"
        "    \"caustic_volume_enabled\": true,\n"
        "    \"caustic_sidecar_enabled\": true,\n"
        "    \"caustic_sidecar_strength\": 2.0\n"
        "  }\n"
        "}\n";

    snprintf(request_path,
             sizeof(request_path),
             "%s",
             "/tmp/ray_tracing_agent_render_spatial_caustic_cache_phase3_combined_request.json");
    assert_true("agent_render_spatial_caustic_phase3_combined_request_write",
                write_text_file(request_path, json_text));
    assert_true("agent_render_spatial_caustic_phase3_combined_request_load",
                ray_tracing_agent_render_request_load_file(request_path,
                                                           &request,
                                                           diagnostics,
                                                           sizeof(diagnostics)));
    assert_true("agent_render_spatial_caustic_phase3_combined_mode",
                request.caustic_settings.mode == RUNTIME_CAUSTIC_MODE_SPATIAL_CACHE);
    assert_true("agent_render_spatial_caustic_phase3_combined_volume",
                request.caustic_settings.volumeCacheEnabled);
    assert_true("agent_render_spatial_caustic_phase3_combined_sidecar",
                request.caustic_sidecar_enabled);
    assert_true("agent_render_spatial_caustic_phase3_combined_disney_mode",
                request.caustic_mode == RUNTIME_DISNEY_V2_CAUSTIC_MODE_ANALYTIC);
    readback = RuntimeCausticSettings3D_Phase0Readback(&request.caustic_settings,
                                                       request.caustic_sidecar_enabled);
    assert_true("agent_render_spatial_caustic_phase3_combined_readback_sidecar",
                readback.analyticSidecarRequested);
    assert_true("agent_render_spatial_caustic_phase3_combined_readback_volume",
                readback.volumeCacheState ==
                    RUNTIME_CAUSTIC_CACHE_STATE_REQUESTED_NOT_ALLOCATED);
    unlink(request_path);
    return 0;
}

static int test_agent_render_request_disney_v2_caustic_transport_reserved(void) {
    char request_path[PATH_MAX];
    char diagnostics[256];
    RayTracingAgentRenderRequest request;
    const char* json_text =
        "{\n"
        "  \"schema_version\": \"ray_tracing_agent_render_request_v1\",\n"
        "  \"run_id\": \"disney_v2_caustic_transport_reserved_test\",\n"
        "  \"scene\": {\"runtime_scene_path\": \"scene_runtime.json\"},\n"
        "  \"render\": {\"integrator_3d\": \"disney_v2\"},\n"
        "  \"inspection\": {\"caustic_mode\": \"transport\"}\n"
        "}\n";

    snprintf(request_path,
             sizeof(request_path),
             "%s",
             "/tmp/ray_tracing_agent_render_disney_v2_caustic_transport_request.json");
    assert_true("agent_render_caustic_transport_request_write",
                write_text_file(request_path, json_text));
    assert_true("agent_render_caustic_transport_request_rejected",
                !ray_tracing_agent_render_request_load_file(request_path,
                                                            &request,
                                                            diagnostics,
                                                            sizeof(diagnostics)));
    assert_true("agent_render_caustic_transport_diag",
                strstr(diagnostics, "caustic_volume_enabled=true") != NULL);
    unlink(request_path);
    return 0;
}

static int test_agent_render_request_trace_route_roundtrip_and_validation(void) {
    char request_path[PATH_MAX];
    char default_request_path[PATH_MAX];
    char invalid_request_path[PATH_MAX];
    char diagnostics[256];
    RayTracingAgentRenderRequest request;
    const char* json_text =
        "{\n"
        "  \"schema_version\": \"ray_tracing_agent_render_request_v1\",\n"
        "  \"run_id\": \"trace_route_parity_test\",\n"
        "  \"scene\": {\"runtime_scene_path\": \"scene_runtime.json\"},\n"
        "  \"inspection\": {\"trace_route\": \"tlas_blas_parity\"}\n"
        "}\n";
    const char* default_json_text =
        "{\n"
        "  \"schema_version\": \"ray_tracing_agent_render_request_v1\",\n"
        "  \"run_id\": \"trace_route_default_test\",\n"
        "  \"scene\": {\"runtime_scene_path\": \"scene_runtime.json\"}\n"
        "}\n";
    const char* invalid_json_text =
        "{\n"
        "  \"schema_version\": \"ray_tracing_agent_render_request_v1\",\n"
        "  \"run_id\": \"trace_route_invalid_test\",\n"
        "  \"scene\": {\"runtime_scene_path\": \"scene_runtime.json\"},\n"
        "  \"inspection\": {\"trace_route\": \"unknown_route\"}\n"
        "}\n";

    snprintf(request_path,
             sizeof(request_path),
             "%s",
             "/tmp/ray_tracing_agent_render_trace_route_request.json");
    snprintf(default_request_path,
             sizeof(default_request_path),
             "%s",
             "/tmp/ray_tracing_agent_render_trace_route_default_request.json");
    snprintf(invalid_request_path,
             sizeof(invalid_request_path),
             "%s",
             "/tmp/ray_tracing_agent_render_trace_route_invalid_request.json");
    assert_true("agent_render_trace_route_request_write",
                write_text_file(request_path, json_text));
    assert_true("agent_render_trace_route_request_load",
                ray_tracing_agent_render_request_load_file(request_path,
                                                           &request,
                                                           diagnostics,
                                                           sizeof(diagnostics)));
    assert_true("agent_render_trace_route_has_override",
                request.has_trace_route_override);
    assert_true("agent_render_trace_route_parity",
                request.trace_route == RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS_PARITY);

    assert_true("agent_render_trace_route_default_request_write",
                write_text_file(default_request_path, default_json_text));
    assert_true("agent_render_trace_route_default_request_load",
                ray_tracing_agent_render_request_load_file(default_request_path,
                                                           &request,
                                                           diagnostics,
                                                           sizeof(diagnostics)));
    assert_true("agent_render_trace_route_default_no_override",
                !request.has_trace_route_override);
    assert_true("agent_render_trace_route_default_tlas",
                request.trace_route == RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS);

    assert_true("agent_render_trace_route_invalid_request_write",
                write_text_file(invalid_request_path, invalid_json_text));
    assert_true("agent_render_trace_route_invalid_rejected",
                !ray_tracing_agent_render_request_load_file(invalid_request_path,
                                                            &request,
                                                            diagnostics,
                                                            sizeof(diagnostics)));
    assert_true("agent_render_trace_route_invalid_diag",
                strstr(diagnostics, "inspection.trace_route") != NULL);

    unlink(request_path);
    unlink(default_request_path);
    unlink(invalid_request_path);
    return 0;
}

static int test_agent_render_request_caustic_transport_volume_phase4_contract(void) {
    char request_path[PATH_MAX];
    char diagnostics[256];
    RayTracingAgentRenderRequest request;
    RuntimeCausticReadback3D readback;
    const char* json_text =
        "{\n"
        "  \"schema_version\": \"ray_tracing_agent_render_request_v1\",\n"
        "  \"run_id\": \"caustic_transport_volume_phase4_test\",\n"
        "  \"scene\": {\"runtime_scene_path\": \"scene_runtime.json\"},\n"
        "  \"render\": {\"integrator_3d\": \"direct_light\"},\n"
        "  \"inspection\": {\n"
        "    \"caustic_mode\": \"transport\",\n"
        "    \"caustic_volume_enabled\": true,\n"
        "    \"caustic_sample_budget\": 128,\n"
        "    \"caustic_max_path_depth\": 2,\n"
        "    \"caustic_transport_emission_policy\": \"analytic_sphere_lens\",\n"
        "    \"caustic_transport_debug_export_enabled\": true\n"
        "  }\n"
        "}\n";

    snprintf(request_path,
             sizeof(request_path),
             "%s",
             "/tmp/ray_tracing_agent_render_caustic_transport_volume_phase4_request.json");
    assert_true("agent_render_caustic_transport_phase4_request_write",
                write_text_file(request_path, json_text));
    assert_true("agent_render_caustic_transport_phase4_request_load",
                ray_tracing_agent_render_request_load_file(request_path,
                                                           &request,
                                                           diagnostics,
                                                           sizeof(diagnostics)));
    assert_true("agent_render_caustic_transport_phase4_mode",
                request.caustic_settings.mode == RUNTIME_CAUSTIC_MODE_TRANSPORT);
    assert_true("agent_render_caustic_transport_phase4_volume",
                request.caustic_settings.volumeCacheEnabled);
    assert_true("agent_render_caustic_transport_phase4_sidecar_off",
                !request.caustic_sidecar_enabled);
    assert_true("agent_render_caustic_transport_phase4_disney_mode",
                request.caustic_mode == RUNTIME_DISNEY_V2_CAUSTIC_MODE_OFF);
    assert_true("agent_render_caustic_transport_phase4_budget",
                request.caustic_settings.sampleBudget == 128);
    assert_true("agent_render_caustic_transport_phase4_depth",
                request.caustic_settings.maxPathDepth == 2);
    assert_true("agent_render_caustic_transport_phase13_emission_policy",
                request.caustic_settings.emissionPolicy ==
                    RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_SPHERE_LENS);
    assert_true("agent_render_caustic_transport_phase4_debug_export",
                request.caustic_settings.debugExportEnabled);
    readback = RuntimeCausticSettings3D_Phase0Readback(&request.caustic_settings,
                                                       request.caustic_sidecar_enabled);
    assert_true("agent_render_caustic_transport_phase4_path_active",
                readback.pathEmissionActive);
    assert_true("agent_render_caustic_transport_phase4_not_reserved",
                !readback.transportReserved);
    assert_true("agent_render_caustic_transport_phase13_readback_emission_policy",
                readback.emissionPolicy ==
                    RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_SPHERE_LENS);
    assert_true("agent_render_caustic_transport_phase4_volume_state",
                readback.volumeCacheState ==
                    RUNTIME_CAUSTIC_CACHE_STATE_REQUESTED_NOT_ALLOCATED);
    unlink(request_path);
    return 0;
}

static int test_agent_render_request_caustic_photon_map_contract_only(void) {
    char request_path[PATH_MAX];
    char diagnostics[256];
    RayTracingAgentRenderRequest request;
    RuntimeCausticReadback3D readback;
    RuntimeCausticPhotonSample3D sample = {0};
    RuntimeCausticPhotonDielectricEvent3D dielectric = {0};
    const char* json_text =
        "{\n"
        "  \"schema_version\": \"ray_tracing_agent_render_request_v1\",\n"
        "  \"run_id\": \"caustic_photon_map_contract_only_test\",\n"
        "  \"scene\": {\"runtime_scene_path\": \"scene_runtime.json\"},\n"
        "  \"render\": {\"integrator_3d\": \"direct_light\"},\n"
        "  \"inspection\": {\n"
        "    \"caustic_mode\": \"transport\",\n"
        "    \"caustic_surface_enabled\": true,\n"
        "    \"caustic_sample_budget\": 128,\n"
        "    \"caustic_max_path_depth\": 4,\n"
        "    \"caustic_transport_engine\": \"photon_map\"\n"
        "  }\n"
        "}\n";

    sample.photonId = 7u;
    sample.emissionPdf = 0.25;
    dielectric.photonId = sample.photonId;
    dielectric.selectedBranch = RUNTIME_CAUSTIC_PHOTON_BRANCH_REFRACTED;
    dielectric.branchPdf = 0.75;

    snprintf(request_path,
             sizeof(request_path),
             "%s",
             "/tmp/ray_tracing_agent_render_caustic_photon_map_contract_request.json");
    assert_true("agent_render_caustic_photon_map_request_write",
                write_text_file(request_path, json_text));
    assert_true("agent_render_caustic_photon_map_request_load",
                ray_tracing_agent_render_request_load_file(request_path,
                                                           &request,
                                                           diagnostics,
                                                           sizeof(diagnostics)));
    assert_true("agent_render_caustic_photon_map_mode",
                request.caustic_settings.mode == RUNTIME_CAUSTIC_MODE_TRANSPORT);
    assert_true("agent_render_caustic_photon_map_engine",
                request.caustic_settings.transportEngine ==
                    RUNTIME_CAUSTIC_TRANSPORT_ENGINE_PHOTON_MAP);
    assert_true("agent_render_caustic_photon_map_sidecar_off",
                !request.caustic_sidecar_enabled);
    assert_true("agent_render_caustic_photon_map_record_contract",
                dielectric.photonId == sample.photonId &&
                    dielectric.selectedBranch == RUNTIME_CAUSTIC_PHOTON_BRANCH_REFRACTED &&
                    dielectric.branchPdf > sample.emissionPdf);
    readback = RuntimeCausticSettings3D_Phase0Readback(&request.caustic_settings,
                                                       request.caustic_sidecar_enabled);
    assert_true("agent_render_caustic_photon_map_requested",
                readback.photonMapRequested);
    assert_true("agent_render_caustic_photon_map_not_implemented",
                !readback.photonMapImplemented);
    assert_true("agent_render_caustic_photon_map_no_exploratory_path",
                !readback.pathEmissionActive);
    assert_true("agent_render_caustic_photon_map_label",
                strcmp(RuntimeCausticTransportEngine3D_Label(readback.transportEngine),
                       "photon_map") == 0);
    unlink(request_path);
    return 0;
}

static int test_agent_render_request_caustic_photon_product_mode_ppm6(void) {
    char request_path[PATH_MAX];
    char diagnostics[256];
    RayTracingAgentRenderRequest request;
    RuntimeCausticReadback3D readback;
    const char* json_text =
        "{\n"
        "  \"schema_version\": \"ray_tracing_agent_render_request_v1\",\n"
        "  \"run_id\": \"caustic_photon_product_mode_ppm6_test\",\n"
        "  \"scene\": {\"runtime_scene_path\": \"scene_runtime.json\"},\n"
        "  \"render\": {\"integrator_3d\": \"direct_light\"},\n"
        "  \"inspection\": {\n"
        "    \"caustic_product_mode\": \"production\",\n"
        "    \"caustic_surface_query_enabled\": true,\n"
        "    \"caustic_volume_query_enabled\": true,\n"
        "    \"caustic_render_contribution_enabled\": true,\n"
        "    \"caustic_photon_sample_budget\": 192,\n"
        "    \"caustic_photon_max_path_depth\": 6,\n"
        "    \"caustic_photon_surface_radiance_scale\": 2.5,\n"
        "    \"caustic_photon_trace_populated_callsite_readback_enabled\": true\n"
        "  }\n"
        "}\n";

    snprintf(request_path,
             sizeof(request_path),
             "%s",
             "/tmp/ray_tracing_agent_render_caustic_photon_product_mode_ppm6_request.json");
    assert_true("agent_render_caustic_photon_product_mode_request_write",
                write_text_file(request_path, json_text));
    assert_true("agent_render_caustic_photon_product_mode_request_load",
                ray_tracing_agent_render_request_load_file(request_path,
                                                           &request,
                                                           diagnostics,
                                                           sizeof(diagnostics)));
    assert_true("agent_render_caustic_photon_product_mode_override",
                request.has_caustic_product_mode_override &&
                    request.has_caustic_mode_override);
    assert_true("agent_render_caustic_photon_product_mode_settings",
                request.caustic_photon_integration_settings.productMode ==
                        RUNTIME_CAUSTIC_PRODUCT_MODE_PRODUCTION &&
                    request.caustic_photon_integration_settings.renderContributionEnabled &&
                    request.caustic_photon_trace_populated_callsite_readback_enabled);
    assert_true("agent_render_caustic_photon_product_mode_caustic_settings",
                request.caustic_settings.mode == RUNTIME_CAUSTIC_MODE_TRANSPORT &&
                    request.caustic_settings.transportEngine ==
                        RUNTIME_CAUSTIC_TRANSPORT_ENGINE_PHOTON_MAP &&
                    request.caustic_settings.surfaceCacheEnabled &&
                    request.caustic_settings.volumeCacheEnabled &&
                    request.caustic_settings.sampleBudget == 192 &&
                    request.caustic_settings.maxPathDepth == 6);
    assert_true("agent_render_caustic_photon_product_mode_sidecar_off",
                !request.caustic_sidecar_enabled &&
                    request.caustic_mode == RUNTIME_DISNEY_V2_CAUSTIC_MODE_OFF);
    readback = RuntimeCausticSettings3D_Phase0Readback(&request.caustic_settings,
                                                       request.caustic_sidecar_enabled);
    assert_true("agent_render_caustic_photon_product_mode_readback",
                readback.photonMapRequested &&
                    !readback.pathEmissionActive &&
                    readback.transportEngine ==
                        RUNTIME_CAUSTIC_TRANSPORT_ENGINE_PHOTON_MAP);
    unlink(request_path);
    return 0;
}

static int test_agent_render_request_caustic_lens_traversal_profile_override(void) {
    char request_path[PATH_MAX];
    char diagnostics[256];
    RayTracingAgentRenderRequest request;
    const char* json_text =
        "{\n"
        "  \"schema_version\": \"ray_tracing_agent_render_request_v1\",\n"
        "  \"run_id\": \"caustic_lens_profile_override_test\",\n"
        "  \"scene\": {\"runtime_scene_path\": \"scene_runtime.json\"},\n"
        "  \"render\": {\"integrator_3d\": \"direct_light\"},\n"
        "  \"inspection\": {\n"
        "    \"caustic_mode\": \"transport\",\n"
        "    \"caustic_volume_enabled\": true,\n"
        "    \"caustic_transport_emission_policy\": \"analytic_prism_lens\",\n"
        "    \"caustic_lens_traversal_profile\": {\n"
        "      \"preset\": \"dense_glass\",\n"
        "      \"material_ior\": 1.72,\n"
        "      \"fresnel_scale\": 0.75,\n"
        "      \"transmission_scale\": 1.15,\n"
        "      \"tint\": [0.80, 0.90, 1.00],\n"
        "      \"absorption_distance\": 6.5,\n"
        "      \"aperture_radius_scale\": 0.40\n"
        "    }\n"
        "  }\n"
        "}\n";

    snprintf(request_path,
             sizeof(request_path),
             "%s",
             "/tmp/ray_tracing_agent_render_caustic_lens_profile_request.json");
    assert_true("agent_render_caustic_lens_profile_request_write",
                write_text_file(request_path, json_text));
    assert_true("agent_render_caustic_lens_profile_request_load",
                ray_tracing_agent_render_request_load_file(request_path,
                                                           &request,
                                                           diagnostics,
                                                           sizeof(diagnostics)));
    assert_true("agent_render_caustic_lens_profile_override_enabled",
                request.caustic_settings.hasTraversalProfileOverride);
    assert_true("agent_render_caustic_lens_profile_policy",
                request.caustic_settings.emissionPolicy ==
                    RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_PRISM_LENS);
    assert_true("agent_render_caustic_lens_profile_kind",
                request.caustic_settings.traversalProfileOverride.kind ==
                    RUNTIME_CAUSTIC_LENS_TRAVERSAL_PROFILE_CUSTOM);
    assert_true("agent_render_caustic_lens_profile_ior",
                request.caustic_settings.traversalProfileOverride.materialIor > 1.719 &&
                    request.caustic_settings.traversalProfileOverride.materialIor < 1.721);
    assert_true("agent_render_caustic_lens_profile_fresnel_scale",
                request.caustic_settings.traversalProfileOverride.fresnelScale > 0.749 &&
                    request.caustic_settings.traversalProfileOverride.fresnelScale < 0.751);
    assert_true("agent_render_caustic_lens_profile_transmission_scale",
                request.caustic_settings.traversalProfileOverride.transmissionScale > 1.149 &&
                    request.caustic_settings.traversalProfileOverride.transmissionScale < 1.151);
    assert_true("agent_render_caustic_lens_profile_tint",
                request.caustic_settings.traversalProfileOverride.tint.x > 0.799 &&
                    request.caustic_settings.traversalProfileOverride.tint.y > 0.899 &&
                    request.caustic_settings.traversalProfileOverride.tint.z > 0.999);
    assert_true("agent_render_caustic_lens_profile_absorption",
                request.caustic_settings.traversalProfileOverride.absorptionDistance > 6.49 &&
                    request.caustic_settings.traversalProfileOverride.absorptionDistance < 6.51);
    assert_true("agent_render_caustic_lens_profile_aperture",
                request.caustic_settings.traversalProfileOverride.apertureRadiusScale > 0.399 &&
                    request.caustic_settings.traversalProfileOverride.apertureRadiusScale < 0.401);
    unlink(request_path);
    return 0;
}

static int test_agent_render_request_caustic_transport_surface_sidecar_combined_contract(void) {
    char request_path[PATH_MAX];
    char diagnostics[256];
    RayTracingAgentRenderRequest request;
    RuntimeCausticReadback3D readback;
    const char* json_text =
        "{\n"
        "  \"schema_version\": \"ray_tracing_agent_render_request_v1\",\n"
        "  \"run_id\": \"caustic_transport_surface_sidecar_phase6_test\",\n"
        "  \"scene\": {\"runtime_scene_path\": \"scene_runtime.json\"},\n"
        "  \"render\": {\"integrator_3d\": \"disney_v2\"},\n"
        "  \"inspection\": {\n"
        "    \"caustic_mode\": \"transport\",\n"
        "    \"caustic_surface_enabled\": true,\n"
        "    \"caustic_sidecar_enabled\": true,\n"
        "    \"caustic_sample_budget\": 256,\n"
        "    \"caustic_max_path_depth\": 2,\n"
        "    \"caustic_surface_energy_scale\": 12.5,\n"
        "    \"caustic_surface_footprint_scale\": 3.25,\n"
        "    \"caustic_surface_receiver_fallback_enabled\": false\n"
        "  }\n"
        "}\n";

    snprintf(request_path,
             sizeof(request_path),
             "%s",
             "/tmp/ray_tracing_agent_render_caustic_transport_surface_sidecar_phase6_request.json");
    assert_true("agent_render_caustic_transport_sidecar_request_write",
                write_text_file(request_path, json_text));
    assert_true("agent_render_caustic_transport_sidecar_request_load",
                ray_tracing_agent_render_request_load_file(request_path,
                                                           &request,
                                                           diagnostics,
                                                           sizeof(diagnostics)));
    assert_true("agent_render_caustic_transport_sidecar_transport_mode",
                request.caustic_settings.mode == RUNTIME_CAUSTIC_MODE_TRANSPORT);
    assert_true("agent_render_caustic_transport_sidecar_surface",
                request.caustic_settings.surfaceCacheEnabled);
    assert_true("agent_render_caustic_transport_sidecar_enabled",
                request.caustic_sidecar_enabled);
    assert_true("agent_render_caustic_transport_sidecar_disney_mode",
                request.caustic_mode == RUNTIME_DISNEY_V2_CAUSTIC_MODE_ANALYTIC);
    assert_true("agent_render_caustic_transport_sidecar_energy_scale",
                request.caustic_settings.surfaceRadianceScale > 12.49 &&
                    request.caustic_settings.surfaceRadianceScale < 12.51);
    assert_true("agent_render_caustic_transport_sidecar_footprint_scale",
                request.caustic_settings.surfaceFootprintScale > 3.24 &&
                    request.caustic_settings.surfaceFootprintScale < 3.26);
    assert_true("agent_render_caustic_transport_sidecar_receiver_fallback_disabled",
                !request.caustic_settings.surfaceReceiverFallbackEnabled);
    readback = RuntimeCausticSettings3D_Phase0Readback(&request.caustic_settings,
                                                       request.caustic_sidecar_enabled);
    assert_true("agent_render_caustic_transport_sidecar_path_active",
                readback.pathEmissionActive);
    assert_true("agent_render_caustic_transport_sidecar_surface_state",
                readback.surfaceCacheState ==
                    RUNTIME_CAUSTIC_CACHE_STATE_REQUESTED_NOT_ALLOCATED);
    assert_true("agent_render_caustic_transport_sidecar_readback_sidecar",
                readback.analyticSidecarRequested);
    unlink(request_path);
    return 0;
}

static int test_agent_render_request_caustic_sidecar_rejects_non_disney_v2(void) {
    char request_path[PATH_MAX];
    char diagnostics[256];
    RayTracingAgentRenderRequest request;
    const char* json_text =
        "{\n"
        "  \"schema_version\": \"ray_tracing_agent_render_request_v1\",\n"
        "  \"run_id\": \"direct_light_caustic_sidecar_reject_test\",\n"
        "  \"scene\": {\"runtime_scene_path\": \"scene_runtime.json\"},\n"
        "  \"render\": {\"integrator_3d\": \"direct_light\"},\n"
        "  \"inspection\": {\"caustic_sidecar_enabled\": true}\n"
        "}\n";

    snprintf(request_path,
             sizeof(request_path),
             "%s",
             "/tmp/ray_tracing_agent_render_direct_light_caustic_sidecar_request.json");
    assert_true("agent_render_caustic_sidecar_reject_request_write",
                write_text_file(request_path, json_text));
    assert_true("agent_render_caustic_sidecar_reject_load",
                !ray_tracing_agent_render_request_load_file(request_path,
                                                            &request,
                                                            diagnostics,
                                                            sizeof(diagnostics)));
    assert_true("agent_render_caustic_sidecar_reject_diag",
                strstr(diagnostics, "requires render.integrator_3d=disney_v2") != NULL);
    unlink(request_path);
    return 0;
}

static int test_agent_render_request_volume_visible_roundtrip(void) {
    char request_path[PATH_MAX];
    char diagnostics[256];
    RayTracingAgentRenderRequest request;
    const char* json_default_text =
        "{\n"
        "  \"schema_version\": \"ray_tracing_agent_render_request_v1\",\n"
        "  \"run_id\": \"volume_visible_default_test\",\n"
        "  \"scene\": {\"runtime_scene_path\": \"scene_runtime.json\"},\n"
        "  \"volume\": {\n"
        "    \"enabled\": true,\n"
        "    \"source_kind\": \"manifest\",\n"
        "    \"source_path\": \"volume_manifest.json\"\n"
        "  }\n"
        "}\n";
    const char* json_text =
        "{\n"
        "  \"schema_version\": \"ray_tracing_agent_render_request_v1\",\n"
        "  \"run_id\": \"volume_visible_roundtrip_test\",\n"
        "  \"scene\": {\"runtime_scene_path\": \"scene_runtime.json\"},\n"
        "  \"volume\": {\n"
        "    \"enabled\": true,\n"
        "    \"source_kind\": \"manifest\",\n"
        "    \"source_path\": \"volume_manifest.json\",\n"
        "    \"visible\": false,\n"
        "    \"affects_lighting\": false,\n"
        "    \"debug_overlay\": true\n"
        "  }\n"
        "}\n";

    snprintf(request_path,
             sizeof(request_path),
             "%s",
             "/tmp/ray_tracing_agent_render_volume_visible_request.json");
    assert_true("agent_render_volume_visible_default_request_write",
                write_text_file(request_path, json_default_text));
    assert_true("agent_render_volume_visible_default_request_load",
                ray_tracing_agent_render_request_load_file(request_path,
                                                           &request,
                                                           diagnostics,
                                                           sizeof(diagnostics)));
    assert_true("agent_render_volume_visible_default_true", request.volume_visible);
    assert_true("agent_render_volume_affects_default_true", request.volume_affects_lighting);
    assert_true("agent_render_volume_debug_default_false", !request.volume_debug_overlay);
    assert_true("agent_render_volume_visible_request_write",
                write_text_file(request_path, json_text));
    assert_true("agent_render_volume_visible_request_load",
                ray_tracing_agent_render_request_load_file(request_path,
                                                           &request,
                                                           diagnostics,
                                                           sizeof(diagnostics)));
    assert_true("agent_render_volume_visible_source_enabled", request.volume_enabled);
    assert_true("agent_render_volume_visible_false", !request.volume_visible);
    assert_true("agent_render_volume_visible_affects_false", !request.volume_affects_lighting);
    assert_true("agent_render_volume_visible_debug_true", request.volume_debug_overlay);
    unlink(request_path);
    return 0;
}

static int test_agent_render_request_volume_material_remap_roundtrip(void) {
    char request_path[PATH_MAX];
    char diagnostics[256];
    RayTracingAgentRenderRequest request;
    const char* json_text =
        "{\n"
        "  \"schema_version\": \"ray_tracing_agent_render_request_v1\",\n"
        "  \"run_id\": \"volume_material_remap_roundtrip_test\",\n"
        "  \"scene\": {\"runtime_scene_path\": \"scene_runtime.json\"},\n"
        "  \"inspection\": {\n"
        "    \"volume_density_scale\": 0.35,\n"
        "    \"volume_density_gamma\": 0.65,\n"
        "    \"volume_scatter_gain\": 2.5,\n"
        "    \"volume_absorption_gain\": 0.15,\n"
        "    \"volume_opacity_clamp\": 0.70,\n"
        "    \"volume_albedo\": {\"r\": 0.92, \"g\": 0.91, \"b\": 0.88}\n"
        "  }\n"
        "}\n";

    snprintf(request_path,
             sizeof(request_path),
             "%s",
             "/tmp/ray_tracing_agent_render_volume_material_request.json");
    assert_true("agent_render_volume_material_request_write",
                write_text_file(request_path, json_text));
    assert_true("agent_render_volume_material_request_load",
                ray_tracing_agent_render_request_load_file(request_path,
                                                           &request,
                                                           diagnostics,
                                                           sizeof(diagnostics)));
    assert_true("agent_render_volume_density_scale_present",
                request.has_volume_density_scale_override);
    assert_close("agent_render_volume_density_scale_value",
                 request.volume_density_scale_override,
                 0.35,
                 0.000001);
    assert_true("agent_render_volume_density_gamma_present",
                request.has_volume_density_gamma_override);
    assert_close("agent_render_volume_density_gamma_value",
                 request.volume_density_gamma_override,
                 0.65,
                 0.000001);
    assert_true("agent_render_volume_scatter_gain_present",
                request.has_volume_scatter_gain_override);
    assert_close("agent_render_volume_scatter_gain_value",
                 request.volume_scatter_gain_override,
                 2.5,
                 0.000001);
    assert_true("agent_render_volume_absorption_gain_present",
                request.has_volume_absorption_gain_override);
    assert_close("agent_render_volume_absorption_gain_value",
                 request.volume_absorption_gain_override,
                 0.15,
                 0.000001);
    assert_true("agent_render_volume_opacity_clamp_present",
                request.has_volume_opacity_clamp_override);
    assert_close("agent_render_volume_opacity_clamp_value",
                 request.volume_opacity_clamp_override,
                 0.70,
                 0.000001);
    assert_true("agent_render_volume_albedo_present",
                request.has_volume_albedo_override);
    assert_close("agent_render_volume_albedo_r",
                 request.volume_albedo_r,
                 0.92,
                 0.000001);
    assert_close("agent_render_volume_albedo_g",
                 request.volume_albedo_g,
                 0.91,
                 0.000001);
    assert_close("agent_render_volume_albedo_b",
                 request.volume_albedo_b,
                 0.88,
                 0.000001);
    unlink(request_path);
    return 0;
}

static int test_agent_render_request_resource_budget_roundtrip(void) {
    char request_path[PATH_MAX];
    char diagnostics[256];
    RayTracingAgentRenderRequest request;
    const char* json_text =
        "{\n"
        "  \"schema_version\": \"ray_tracing_agent_render_request_v1\",\n"
        "  \"run_id\": \"resource_budget_test\",\n"
        "  \"scene\": {\"runtime_scene_path\": \"scene_runtime.json\"},\n"
        "  \"resources\": {\n"
        "    \"cpu_percent\": 50,\n"
        "    \"max_workers\": 2,\n"
        "    \"reserve_cpu_count\": 1\n"
        "  }\n"
        "}\n";

    snprintf(request_path,
             sizeof(request_path),
             "%s",
             "/tmp/ray_tracing_agent_render_resource_budget_request.json");
    assert_true("agent_render_resource_budget_request_write",
                write_text_file(request_path, json_text));
    assert_true("agent_render_resource_budget_request_load",
                ray_tracing_agent_render_request_load_file(request_path,
                                                           &request,
                                                           diagnostics,
                                                           sizeof(diagnostics)));
    assert_true("agent_render_resource_budget_present", request.has_resource_budget);
    assert_true("agent_render_resource_budget_cpu_percent",
                request.resource_cpu_percent == 50);
    assert_true("agent_render_resource_budget_max_workers",
                request.resource_max_workers == 2);
    assert_true("agent_render_resource_budget_reserve",
                request.resource_reserve_cpu_count == 1);
    unlink(request_path);
    return 0;
}

static int test_agent_render_request_resource_budget_env_default(void) {
    char request_path[PATH_MAX];
    char diagnostics[256];
    RayTracingAgentRenderRequest request;
    const char* json_text =
        "{\n"
        "  \"schema_version\": \"ray_tracing_agent_render_request_v1\",\n"
        "  \"run_id\": \"resource_budget_env_test\",\n"
        "  \"scene\": {\"runtime_scene_path\": \"scene_runtime.json\"}\n"
        "}\n";

    snprintf(request_path,
             sizeof(request_path),
             "%s",
             "/tmp/ray_tracing_agent_render_resource_budget_env_request.json");
    assert_true("agent_render_resource_budget_env_request_write",
                write_text_file(request_path, json_text));
    setenv("CODEWORK_RAY_TRACING_DEFAULT_CPU_PERCENT", "40", 1);
    assert_true("agent_render_resource_budget_env_request_load",
                ray_tracing_agent_render_request_load_file(request_path,
                                                           &request,
                                                           diagnostics,
                                                           sizeof(diagnostics)));
    unsetenv("CODEWORK_RAY_TRACING_DEFAULT_CPU_PERCENT");
    assert_true("agent_render_resource_budget_env_present", request.has_resource_budget);
    assert_true("agent_render_resource_budget_env_cpu_percent",
                request.resource_cpu_percent == 40);
    assert_true("agent_render_resource_budget_env_max_workers",
                request.resource_max_workers == 0);
    unlink(request_path);
    return 0;
}

static int test_agent_render_request_environment_lighting_overrides(void) {
    char request_path[PATH_MAX];
    char diagnostics[256];
    RayTracingAgentRenderRequest request;
    const char* json_text =
        "{\n"
        "  \"schema_version\": \"ray_tracing_agent_render_request_v1\",\n"
        "  \"run_id\": \"environment_lighting_override_test\",\n"
        "  \"scene\": {\"runtime_scene_path\": \"scene_runtime.json\"},\n"
        "  \"inspection\": {\n"
        "    \"environment_light_mode\": \"ambient\",\n"
        "    \"ambient_strength\": 0.32,\n"
        "    \"environment_preset\": \"warm_sky\",\n"
        "    \"background_brightness\": 0.74,\n"
        "    \"background_color\": [0.70, 0.80, 1.00],\n"
        "    \"top_fill_strength\": 1.25\n"
        "  }\n"
        "}\n";

    snprintf(request_path,
             sizeof(request_path),
             "%s",
             "/tmp/ray_tracing_agent_render_environment_lighting_request.json");
    assert_true("agent_render_environment_lighting_request_write",
                write_text_file(request_path, json_text));
    assert_true("agent_render_environment_lighting_request_load",
                ray_tracing_agent_render_request_load_file(request_path,
                                                           &request,
                                                           diagnostics,
                                                           sizeof(diagnostics)));
    assert_true("agent_render_environment_lighting_mode_override",
                request.has_environment_light_mode_override &&
                    request.environment_light_mode_override == ENVIRONMENT_LIGHT_MODE_AMBIENT);
    assert_true("agent_render_environment_lighting_ambient_override",
                request.has_ambient_strength_override);
    assert_close("agent_render_environment_lighting_ambient_strength",
                 request.ambient_strength_override,
                 0.32,
                 1e-9);
    assert_true("agent_render_environment_lighting_preset_override",
                request.has_environment_preset_override &&
                    request.environment_preset_override == ENVIRONMENT_PRESET_WARM_SKY);
    assert_true("agent_render_environment_lighting_background_brightness_override",
                request.has_background_brightness_override);
    assert_close("agent_render_environment_lighting_background_brightness",
                 request.background_brightness_override,
                 0.74,
                 1e-9);
    assert_true("agent_render_environment_lighting_background_color_override",
                request.has_background_color_override);
    assert_close("agent_render_environment_lighting_background_color_r",
                 request.background_color_r,
                 0.70,
                 1e-9);
    assert_close("agent_render_environment_lighting_background_color_g",
                 request.background_color_g,
                 0.80,
                 1e-9);
    assert_close("agent_render_environment_lighting_background_color_b",
                 request.background_color_b,
                 1.00,
                 1e-9);
    unlink(request_path);
    return 0;
}

static int test_agent_render_request_render_trace_cost_ledger_flag(void) {
    char request_path[PATH_MAX];
    char diagnostics[256];
    RayTracingAgentRenderRequest request;
    const char* json_text =
        "{\n"
        "  \"schema_version\": \"ray_tracing_agent_render_request_v1\",\n"
        "  \"run_id\": \"render_trace_cost_ledger_test\",\n"
        "  \"scene\": {\"runtime_scene_path\": \"scene_runtime.json\"},\n"
        "  \"inspection\": {\n"
        "    \"render_trace_cost_ledger_enabled\": true\n"
        "  }\n"
        "}\n";

    snprintf(request_path,
             sizeof(request_path),
             "%s",
             "/tmp/ray_tracing_agent_render_trace_cost_ledger_request.json");
    assert_true("agent_render_trace_cost_ledger_request_write",
                write_text_file(request_path, json_text));
    assert_true("agent_render_trace_cost_ledger_request_load",
                ray_tracing_agent_render_request_load_file(request_path,
                                                           &request,
                                                           diagnostics,
                                                           sizeof(diagnostics)));
    assert_true("agent_render_trace_cost_ledger_enabled",
                request.render_trace_cost_ledger_enabled);
    unlink(request_path);
    return 0;
}

static int test_animation_native_3d_temporal_frames_roundtrip_and_clamp(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    const char* json_missing_temporal =
        "{\n"
        "  \"spaceMode\": 1,\n"
        "  \"integratorMode3D\": 1\n"
        "}\n";
    const char* json_invalid_temporal =
        "{\n"
        "  \"spaceMode\": 1,\n"
        "  \"integratorMode3D\": 1,\n"
        "  \"temporalFrames3D\": 0\n"
        "}\n";

    assert_true("native_3d_temporal_write_missing",
                write_text_file(kRuntimeAnimationConfigPath, json_missing_temporal));
    LoadAnimationConfig();
    assert_true("native_3d_temporal_missing_defaults",
                animSettings.temporalFrames3D == RUNTIME_3D_TEMPORAL_FRAMES_DEFAULT);

    assert_true("native_3d_temporal_write_invalid",
                write_text_file(kRuntimeAnimationConfigPath, json_invalid_temporal));
    LoadAnimationConfig();
    assert_true("native_3d_temporal_invalid_clamped_min",
                animSettings.temporalFrames3D == RUNTIME_3D_TEMPORAL_FRAMES_MIN);

    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.temporalFrames3D = 18;
    SaveAnimationConfig();
    animSettings.temporalFrames3D = RUNTIME_3D_TEMPORAL_FRAMES_DEFAULT;
    LoadAnimationConfig();
    assert_true("native_3d_temporal_roundtrip_persisted",
                animSettings.temporalFrames3D == 18);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_native_3d_bounce_depth_and_roulette_roundtrip_and_clamp(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    const char* json_missing_native_3d_bounce =
        "{\n"
        "  \"spaceMode\": 1,\n"
        "  \"integratorMode3D\": 1\n"
        "}\n";
    const char* json_invalid_native_3d_bounce =
        "{\n"
        "  \"spaceMode\": 1,\n"
        "  \"integratorMode3D\": 1,\n"
        "  \"bounceDepth3D\": 0,\n"
        "  \"rouletteThreshold3D\": 0.5\n"
        "}\n";

    assert_true("native_3d_bounce_write_missing",
                write_text_file(kRuntimeAnimationConfigPath, json_missing_native_3d_bounce));
    LoadAnimationConfig();
    assert_true("native_3d_bounce_missing_depth_defaults",
                animSettings.bounceDepth3D == RUNTIME_3D_BOUNCE_DEPTH_DEFAULT);
    assert_true("native_3d_bounce_missing_roulette_defaults",
                fabs(animSettings.rouletteThreshold3D -
                     RUNTIME_3D_ROULETTE_THRESHOLD_DEFAULT) <= 1e-9);

    assert_true("native_3d_bounce_write_invalid",
                write_text_file(kRuntimeAnimationConfigPath, json_invalid_native_3d_bounce));
    LoadAnimationConfig();
    assert_true("native_3d_bounce_invalid_depth_clamped_min",
                animSettings.bounceDepth3D == RUNTIME_3D_BOUNCE_DEPTH_MIN);
    assert_true("native_3d_bounce_invalid_roulette_clamped_max",
                fabs(animSettings.rouletteThreshold3D -
                     RUNTIME_3D_ROULETTE_THRESHOLD_MAX) <= 1e-9);

    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.bounceDepth3D = 5;
    animSettings.rouletteThreshold3D = 0.025;
    SaveAnimationConfig();
    animSettings.bounceDepth3D = RUNTIME_3D_BOUNCE_DEPTH_DEFAULT;
    animSettings.rouletteThreshold3D = RUNTIME_3D_ROULETTE_THRESHOLD_DEFAULT;
    LoadAnimationConfig();
    assert_true("native_3d_bounce_roundtrip_depth_persisted",
                animSettings.bounceDepth3D == 5);
    assert_true("native_3d_bounce_roundtrip_roulette_persisted",
                fabs(animSettings.rouletteThreshold3D - 0.025) <= 1e-9);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_native_3d_top_fill_roundtrip_and_default(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    const char* json_missing_top_fill =
        "{\n"
        "  \"spaceMode\": 1,\n"
        "  \"integratorMode3D\": 1\n"
        "}\n";

    assert_true("native_3d_top_fill_write_missing",
                write_text_file(kRuntimeAnimationConfigPath, json_missing_top_fill));
    LoadAnimationConfig();
    assert_true("native_3d_top_fill_missing_defaults_off",
                animSettings.environmentLightMode == ENVIRONMENT_LIGHT_MODE_OFF);
    assert_close("native_3d_top_fill_missing_strength_default",
                 animSettings.topFillStrength,
                 1.0,
                 1e-9);

    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.environmentLightMode = ENVIRONMENT_LIGHT_MODE_TOP_FILL;
    animSettings.topFillStrength = 2.5;
    SaveAnimationConfig();
    animSettings.environmentLightMode = ENVIRONMENT_LIGHT_MODE_OFF;
    animSettings.topFillStrength = 1.0;
    LoadAnimationConfig();
    assert_true("native_3d_top_fill_roundtrip_persisted",
                animSettings.environmentLightMode == ENVIRONMENT_LIGHT_MODE_TOP_FILL);
    assert_close("native_3d_top_fill_strength_roundtrip_persisted",
                 animSettings.topFillStrength,
                 2.5,
                 1e-9);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_native_3d_environment_model_roundtrip_and_default(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    const char* json_missing_environment_model =
        "{\n"
        "  \"spaceMode\": 1,\n"
        "  \"environmentLightMode\": 2,\n"
        "  \"environmentBrightness\": 96.0,\n"
        "  \"environmentBrightnessUsesByteFloor\": true\n"
        "}\n";
    const char* json_invalid_environment_model =
        "{\n"
        "  \"spaceMode\": 1,\n"
        "  \"environmentPreset\": 999,\n"
        "  \"environmentBackgroundBrightnessAuto\": false,\n"
        "  \"environmentBackgroundBrightness\": 9.0,\n"
        "  \"environmentBackgroundColorR\": -1.0,\n"
        "  \"environmentBackgroundColorG\": 0.5,\n"
        "  \"environmentBackgroundColorB\": 2.0\n"
        "}\n";

    assert_true("native_3d_environment_model_write_missing",
                write_text_file(kRuntimeAnimationConfigPath, json_missing_environment_model));
    LoadAnimationConfig();
    assert_true("native_3d_environment_model_missing_preset_sky",
                animSettings.environmentPreset == ENVIRONMENT_PRESET_SKY);
    assert_true("native_3d_environment_model_missing_background_auto",
                animSettings.environmentBackgroundBrightnessAuto);
    assert_true("native_3d_environment_model_missing_background_legacy",
                !animSettings.environmentBackgroundLightingAuthored);
    assert_close("native_3d_environment_model_missing_background_color_r",
                 animSettings.environmentBackgroundColorR,
                 1.0,
                 1e-9);

    assert_true("native_3d_environment_model_write_invalid",
                write_text_file(kRuntimeAnimationConfigPath, json_invalid_environment_model));
    LoadAnimationConfig();
    assert_true("native_3d_environment_model_invalid_preset_clamped",
                animSettings.environmentPreset == ENVIRONMENT_PRESET_SKY);
    assert_true("native_3d_environment_model_invalid_auto_false",
                !animSettings.environmentBackgroundBrightnessAuto);
    assert_true("native_3d_environment_model_invalid_background_authored",
                animSettings.environmentBackgroundLightingAuthored);
    assert_close("native_3d_environment_model_invalid_brightness_clamped",
                 animSettings.environmentBackgroundBrightness,
                 4.0,
                 1e-9);
    assert_close("native_3d_environment_model_invalid_color_r_clamped",
                 animSettings.environmentBackgroundColorR,
                 0.0,
                 1e-9);
    assert_close("native_3d_environment_model_invalid_color_b_clamped",
                 animSettings.environmentBackgroundColorB,
                 1.0,
                 1e-9);

    animSettings.environmentPreset = ENVIRONMENT_PRESET_WARM_SKY;
    animSettings.environmentBackgroundLightingAuthored = true;
    animSettings.environmentBackgroundBrightnessAuto = false;
    animSettings.environmentBackgroundBrightness = 0.65;
    animSettings.environmentBackgroundColorR = 0.70;
    animSettings.environmentBackgroundColorG = 0.80;
    animSettings.environmentBackgroundColorB = 0.95;
    SaveAnimationConfig();
    animSettings.environmentPreset = ENVIRONMENT_PRESET_SKY;
    animSettings.environmentBackgroundBrightnessAuto = true;
    animSettings.environmentBackgroundBrightness = 0.0;
    animSettings.environmentBackgroundColorR = 1.0;
    animSettings.environmentBackgroundColorG = 1.0;
    animSettings.environmentBackgroundColorB = 1.0;
    LoadAnimationConfig();
    assert_true("native_3d_environment_model_roundtrip_preset",
                animSettings.environmentPreset == ENVIRONMENT_PRESET_WARM_SKY);
    assert_true("native_3d_environment_model_roundtrip_auto_false",
                !animSettings.environmentBackgroundBrightnessAuto);
    assert_true("native_3d_environment_model_roundtrip_background_authored",
                animSettings.environmentBackgroundLightingAuthored);
    assert_close("native_3d_environment_model_roundtrip_brightness",
                 animSettings.environmentBackgroundBrightness,
                 0.65,
                 1e-9);
    assert_close("native_3d_environment_model_roundtrip_color_g",
                 animSettings.environmentBackgroundColorG,
                 0.80,
                 1e-9);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_native_3d_disney_denoise_roundtrip_and_default(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    const char* json_missing_denoise =
        "{\n"
        "  \"spaceMode\": 1,\n"
        "  \"integratorMode3D\": 4\n"
        "}\n";

    assert_true("native_3d_denoise_write_missing",
                write_text_file(kRuntimeAnimationConfigPath, json_missing_denoise));
    LoadAnimationConfig();
    assert_true("native_3d_denoise_missing_defaults_on",
                animSettings.disneyDenoiseEnabled);

    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.disneyDenoiseEnabled = false;
    SaveAnimationConfig();
    animSettings.disneyDenoiseEnabled = true;
    LoadAnimationConfig();
    assert_true("native_3d_denoise_roundtrip_persisted",
                !animSettings.disneyDenoiseEnabled);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_environment_brightness_byte_floor_roundtrip_and_legacy_migration(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    const char* json_legacy_environment =
        "{\n"
        "  \"environmentBrightness\": 0.35\n"
        "}\n";

    assert_true("environment_floor_legacy_write",
                write_text_file(kRuntimeAnimationConfigPath, json_legacy_environment));
    LoadAnimationConfig();
    assert_true("environment_floor_legacy_migrated_to_byte_domain",
                animSettings.environmentBrightness >= 0.0 &&
                animSettings.environmentBrightness <= 255.0 &&
                fabs(animSettings.environmentBrightness - 121.0) <= 1.0);

    animSettings.environmentBrightness = 128.0;
    SaveAnimationConfig();
    animSettings.environmentBrightness = 0.0;
    LoadAnimationConfig();
    assert_true("environment_floor_roundtrip_persisted",
                fabs(animSettings.environmentBrightness - 128.0) <= 1e-6);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_light_intensity_missing_uses_authored_default(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    const char* json_missing_light =
        "{\n"
        "  \"spaceMode\": 1,\n"
        "  \"integratorMode3D\": 4\n"
        "}\n";

    animSettings.lightIntensity = 5.0;
    assert_true("light_intensity_missing_write_partial",
                write_text_file(kRuntimeAnimationConfigPath, json_missing_light));
    LoadAnimationConfig();
    assert_close("light_intensity_missing_uses_authored_default",
                 animSettings.lightIntensity,
                 RAY_TRACING_DEFAULT_LIGHT_INTENSITY,
                 1e-9);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_runtime_window_override_roundtrip_and_apply(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;

    sceneSettings.windowWidth = 640;
    sceneSettings.windowHeight = 904;
    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    SaveAnimationConfig();

    animSettings.runtimeWindowWidth = 0;
    animSettings.runtimeWindowHeight = 0;
    sceneSettings.windowWidth = 1200;
    sceneSettings.windowHeight = 800;
    LoadAnimationConfig();

    assert_true("runtime_window_override_roundtrip_width",
                animSettings.runtimeWindowWidth == 640);
    assert_true("runtime_window_override_roundtrip_height",
                animSettings.runtimeWindowHeight == 904);

    ApplyAnimationWindowSizeOverride();
    assert_true("runtime_window_override_apply_width",
                sceneSettings.windowWidth == 640);
    assert_true("runtime_window_override_apply_height",
                sceneSettings.windowHeight == 904);

    sceneSettings = saved_scene;
    animSettings = saved_anim;
    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_native_3d_render_scale_roundtrip_and_clamp(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    const char* json_missing_scale =
        "{\n"
        "  \"spaceMode\": 1,\n"
        "  \"integratorMode3D\": 1\n"
        "}\n";
    const char* json_invalid_scale =
        "{\n"
        "  \"spaceMode\": 1,\n"
        "  \"integratorMode3D\": 1,\n"
        "  \"renderScale3D\": -4\n"
        "}\n";

    assert_true("native_3d_render_scale_write_missing",
                write_text_file(kRuntimeAnimationConfigPath, json_missing_scale));
    LoadAnimationConfig();
    assert_true("native_3d_render_scale_missing_defaults",
                animSettings.renderScale3D == RUNTIME_3D_RENDER_SCALE_DEFAULT);

    assert_true("native_3d_render_scale_write_invalid",
                write_text_file(kRuntimeAnimationConfigPath, json_invalid_scale));
    LoadAnimationConfig();
    assert_true("native_3d_render_scale_invalid_clamped_default",
                animSettings.renderScale3D == RUNTIME_3D_RENDER_SCALE_DEFAULT);

    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.renderScale3D = 4;
    SaveAnimationConfig();
    animSettings.renderScale3D = RUNTIME_3D_RENDER_SCALE_DEFAULT;
    LoadAnimationConfig();
    assert_true("native_3d_render_scale_roundtrip_persisted",
                animSettings.renderScale3D == 4);

    animSettings.renderScale3D = RUNTIME_3D_RENDER_SCALE_HIDPI;
    SaveAnimationConfig();
    animSettings.renderScale3D = RUNTIME_3D_RENDER_SCALE_DEFAULT;
    LoadAnimationConfig();
    assert_true("native_3d_render_scale_hidpi_roundtrip_persisted",
                animSettings.renderScale3D == RUNTIME_3D_RENDER_SCALE_HIDPI);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_runtime_native_3d_resolution_scale_contract(void) {
    uint8_t src[4] = {10, 20, 30, 40};
    uint8_t dst[16] = {0};
    uint8_t src_abgr[16] = {
        0, 0, 0, 255,
        100, 100, 100, 255,
        200, 200, 200, 255,
        255, 255, 255, 255
    };
    uint8_t dst_abgr[64] = {0};
    uint8_t dimmed_abgr[16] = {0};
    int width = 0;
    int height = 0;
    int rect_x = 0;
    int rect_y = 0;
    int rect_w = 0;
    int rect_h = 0;

    assert_true("runtime_native_3d_scale_resolve_ok",
                RuntimeNative3DResolveScaledDimensions(1200, 900, 2, &width, &height));
    assert_true("runtime_native_3d_scale_resolve_width", width == 600);
    assert_true("runtime_native_3d_scale_resolve_height", height == 450);
    assert_true("runtime_native_3d_hidpi_host_resolve_ok",
                RuntimeNative3DResolveHostDimensions(1200, 900, 2400, 1800,
                                                     RUNTIME_3D_RENDER_SCALE_HIDPI,
                                                     &width, &height));
    assert_true("runtime_native_3d_hidpi_host_width", width == 2400);
    assert_true("runtime_native_3d_hidpi_host_height", height == 1800);
    assert_true("runtime_native_3d_hidpi_scaled_resolve_ok",
                RuntimeNative3DResolveScaledDimensions(width,
                                                       height,
                                                       RUNTIME_3D_RENDER_SCALE_HIDPI,
                                                       &width,
                                                       &height));
    assert_true("runtime_native_3d_hidpi_scaled_width", width == 2400);
    assert_true("runtime_native_3d_hidpi_scaled_height", height == 1800);
    assert_true("runtime_native_3d_scale_clamp_max",
                RuntimeNative3DClampRenderScale(99) == RUNTIME_3D_RENDER_SCALE_MAX);
    assert_true("runtime_native_3d_scale_clamp_hidpi",
                RuntimeNative3DClampRenderScale(RUNTIME_3D_RENDER_SCALE_HIDPI) ==
                    RUNTIME_3D_RENDER_SCALE_HIDPI);

    RuntimeNative3DUpscaleNearest(src, 2, 2, dst, 4, 4);
    assert_true("runtime_native_3d_scale_upscale_top_left", dst[0] == 10);
    assert_true("runtime_native_3d_scale_upscale_top_right", dst[3] == 20);
    assert_true("runtime_native_3d_scale_upscale_bottom_left", dst[12] == 30);
    assert_true("runtime_native_3d_scale_upscale_bottom_right", dst[15] == 40);
    RuntimeNative3DUpscaleBilinearABGR(src_abgr, 2, 2, dst_abgr, 4, 4);
    assert_true("runtime_native_3d_scale_bilinear_preserves_top_left",
                dst_abgr[0] == 0);
    assert_true("runtime_native_3d_scale_bilinear_preserves_bottom_right",
                dst_abgr[((size_t)3 * 4u + 3u) * 4u] == 255);
    assert_true("runtime_native_3d_scale_bilinear_center_softens_block",
                dst_abgr[((size_t)1 * 4u + 1u) * 4u] > 0 &&
                dst_abgr[((size_t)1 * 4u + 1u) * 4u] < 100);
    assert_true("runtime_native_3d_scale_bilinear_alpha_preserved",
                dst_abgr[((size_t)1 * 4u + 1u) * 4u + 3u] == 255);
    RayTracing2PreviewPresent_DimCopyABGR(src_abgr, dimmed_abgr, 4u, 1u, 4u);
    assert_true("runtime_native_3d_preview_dim_copy_quarter_blue",
                dimmed_abgr[4] == 25);
    assert_true("runtime_native_3d_preview_dim_copy_quarter_green",
                dimmed_abgr[5] == 25);
    assert_true("runtime_native_3d_preview_dim_copy_quarter_red",
                dimmed_abgr[6] == 25);
    assert_true("runtime_native_3d_preview_dim_copy_alpha_forced_opaque",
                dimmed_abgr[7] == 255);
    assert_true("runtime_native_3d_scale_rect_map_ok",
                RuntimeNative3DResolveUpscaledRect(32,
                                                   16,
                                                   32,
                                                   32,
                                                   300,
                                                   225,
                                                   1200,
                                                   900,
                                                   &rect_x,
                                                   &rect_y,
                                                   &rect_w,
                                                   &rect_h));
    assert_true("runtime_native_3d_scale_rect_map_x", rect_x == 128);
    assert_true("runtime_native_3d_scale_rect_map_y", rect_y == 64);
    assert_true("runtime_native_3d_scale_rect_map_w", rect_w == 128);
    assert_true("runtime_native_3d_scale_rect_map_h", rect_h == 128);
    return 0;
}

static int test_animation_video_output_root_migrates_from_output_root(void) {
    char tmp_template[] = "/tmp/ray_tracing_video_root_XXXXXX";
    char *tmp_root = mkdtemp(tmp_template);
    size_t backup_size = 0;
    char *backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    char json[1024];
    char expected_video_root[PATH_MAX];

    assert_true("video_output_root_tmpdir_created", tmp_root != NULL);
    if (!tmp_root) {
        restore_runtime_animation_config(backup, backup_size);
        return 0;
    }

    snprintf(json,
             sizeof(json),
             "{\n"
             "  \"inputRoot\": \"config\",\n"
             "  \"outputRoot\": \"%s\",\n"
             "  \"frameDir\": \"%s/frames/default\",\n"
             "  \"fps\": 24,\n"
             "  \"spaceMode\": 0\n"
             "}\n",
             tmp_root,
             tmp_root);
    assert_true("video_output_root_write_runtime",
                write_text_file(kRuntimeAnimationConfigPath, json));
    LoadAnimationConfig();

    assert_true("video_output_root_migrated_compose",
                ray_tracing_compose_path(tmp_root,
                                         "videos",
                                         expected_video_root,
                                         sizeof(expected_video_root)));
    assert_true("video_output_root_migrated_matches",
                strcmp(animSettings.videoOutputRoot, expected_video_root) == 0);
    assert_true("video_output_root_migrated_exists",
                path_exists(animSettings.videoOutputRoot));

    rmdir(expected_video_root);
    rmdir(tmp_root);
    setenv("RAY_TRACING_VIDEO_OUTPUT_ROOT", ray_tracing_default_video_output_root(), 1);
    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_data_paths_resolve_video_output_path_uses_configured_root(void) {
    char output_path[PATH_MAX];
    const char *video_env = getenv("RAY_TRACING_VIDEO_OUTPUT_ROOT");
    char video_env_backup[PATH_MAX] = {0};
    bool had_video_env = false;
    if (video_env && video_env[0]) {
        strncpy(video_env_backup, video_env, sizeof(video_env_backup) - 1);
        video_env_backup[sizeof(video_env_backup) - 1] = '\0';
        had_video_env = true;
    }

    assert_true("video_output_path_resolve_configured",
                ray_tracing_resolve_video_output_path("/tmp/ray_tracing_video_root",
                                                      output_path,
                                                      sizeof(output_path)));
    assert_true("video_output_path_resolve_configured_value",
                strcmp(output_path,
                       "/tmp/ray_tracing_video_root/output.mp4") == 0);

    setenv("RAY_TRACING_VIDEO_OUTPUT_ROOT", ray_tracing_default_video_output_root(), 1);
    assert_true("video_output_path_resolve_default",
                ray_tracing_resolve_video_output_path("",
                                                      output_path,
                                                      sizeof(output_path)));
    assert_true("video_output_path_resolve_default_value",
                strcmp(output_path,
                       ray_tracing_default_video_output_path()) == 0);
    if (had_video_env) {
        setenv("RAY_TRACING_VIDEO_OUTPUT_ROOT", video_env_backup, 1);
    } else {
        unsetenv("RAY_TRACING_VIDEO_OUTPUT_ROOT");
    }
    return 0;
}

static int test_render_export_batch_counts_and_clears_frames(void) {
    char tmp_template[] = "/tmp/ray_tracing_export_frames_XXXXXX";
    char *tmp_root = mkdtemp(tmp_template);
    char frame0[PATH_MAX];
    char frame1[PATH_MAX];
    char note[PATH_MAX];
    char expected_video_output[PATH_MAX];
    char original_frame_dir[sizeof(animSettings.frameDir)];
    char original_video_root[sizeof(animSettings.videoOutputRoot)];
    RayTracingRenderExportStatus status = {0};

    assert_true("export_batch_tmpdir_created", tmp_root != NULL);
    if (!tmp_root) return 0;

    snprintf(frame0, sizeof(frame0), "%s/frame_0000.bmp", tmp_root);
    snprintf(frame1, sizeof(frame1), "%s/frame_0001.bmp", tmp_root);
    snprintf(note, sizeof(note), "%s/keep.txt", tmp_root);
    assert_true("export_batch_write_frame0", write_text_file(frame0, "a"));
    assert_true("export_batch_write_frame1", write_text_file(frame1, "b"));
    assert_true("export_batch_write_note", write_text_file(note, "keep"));

    strncpy(original_frame_dir, animSettings.frameDir, sizeof(original_frame_dir) - 1);
    original_frame_dir[sizeof(original_frame_dir) - 1] = '\0';
    strncpy(original_video_root, animSettings.videoOutputRoot, sizeof(original_video_root) - 1);
    original_video_root[sizeof(original_video_root) - 1] = '\0';

    strncpy(animSettings.frameDir, tmp_root, sizeof(animSettings.frameDir) - 1);
    animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
    strncpy(animSettings.videoOutputRoot, tmp_root, sizeof(animSettings.videoOutputRoot) - 1);
    animSettings.videoOutputRoot[sizeof(animSettings.videoOutputRoot) - 1] = '\0';
    assert_true("export_batch_expected_video_path",
                ray_tracing_resolve_video_output_path(animSettings.videoOutputRoot,
                                                      expected_video_output,
                                                      sizeof(expected_video_output)));

    assert_true("export_batch_count_ok", ray_tracing_render_export_count_active_frames(&status));
    assert_true("export_batch_count_two", status.frame_count == 2u);
    assert_true("export_batch_video_path",
                strcmp(status.video_output_path, expected_video_output) == 0);

    assert_true("export_batch_clear_ok", ray_tracing_render_export_clear_active_frames(&status));
    assert_true("export_batch_clear_removed_two", status.files_cleared == 2u);
    assert_true("export_batch_frame0_removed", !path_exists(frame0));
    assert_true("export_batch_frame1_removed", !path_exists(frame1));
    assert_true("export_batch_note_retained", path_exists(note));

    strncpy(animSettings.frameDir, original_frame_dir, sizeof(animSettings.frameDir) - 1);
    animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
    strncpy(animSettings.videoOutputRoot, original_video_root, sizeof(animSettings.videoOutputRoot) - 1);
    animSettings.videoOutputRoot[sizeof(animSettings.videoOutputRoot) - 1] = '\0';

    unlink(note);
    rmdir(tmp_root);
    return 0;
}

static int test_render_export_batch_reports_highest_and_next_frame(void) {
    char tmp_template[] = "/tmp/ray_tracing_export_frames_next_XXXXXX";
    char *tmp_root = mkdtemp(tmp_template);
    char frame0[PATH_MAX];
    char frame5[PATH_MAX];
    char frame12[PATH_MAX];
    char original_frame_dir[sizeof(animSettings.frameDir)];
    char original_video_root[sizeof(animSettings.videoOutputRoot)];
    RayTracingRenderExportStatus status = {0};

    assert_true("export_batch_next_tmpdir_created", tmp_root != NULL);
    if (!tmp_root) return 0;

    snprintf(frame0, sizeof(frame0), "%s/frame_0000.bmp", tmp_root);
    snprintf(frame5, sizeof(frame5), "%s/frame_0005.bmp", tmp_root);
    snprintf(frame12, sizeof(frame12), "%s/frame_0012.bmp", tmp_root);
    assert_true("export_batch_next_write_frame0", write_text_file(frame0, "a"));
    assert_true("export_batch_next_write_frame5", write_text_file(frame5, "b"));
    assert_true("export_batch_next_write_frame12", write_text_file(frame12, "c"));

    strncpy(original_frame_dir, animSettings.frameDir, sizeof(original_frame_dir) - 1);
    original_frame_dir[sizeof(original_frame_dir) - 1] = '\0';
    strncpy(original_video_root, animSettings.videoOutputRoot, sizeof(original_video_root) - 1);
    original_video_root[sizeof(original_video_root) - 1] = '\0';

    strncpy(animSettings.frameDir, tmp_root, sizeof(animSettings.frameDir) - 1);
    animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
    strncpy(animSettings.videoOutputRoot, tmp_root, sizeof(animSettings.videoOutputRoot) - 1);
    animSettings.videoOutputRoot[sizeof(animSettings.videoOutputRoot) - 1] = '\0';

    assert_true("export_batch_next_describe_ok",
                ray_tracing_render_export_describe_active(&status));
    assert_true("export_batch_next_count_three", status.frame_count == 3u);
    assert_true("export_batch_next_highest_twelve", status.highest_frame_index == 12);
    assert_true("export_batch_next_next_thirteen", status.next_frame_index == 13);

    strncpy(animSettings.frameDir, original_frame_dir, sizeof(animSettings.frameDir) - 1);
    animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
    strncpy(animSettings.videoOutputRoot, original_video_root, sizeof(animSettings.videoOutputRoot) - 1);
    animSettings.videoOutputRoot[sizeof(animSettings.videoOutputRoot) - 1] = '\0';

    unlink(frame0);
    unlink(frame5);
    unlink(frame12);
    rmdir(tmp_root);
    return 0;
}

static int test_render_export_batch_make_video_rejects_empty_frame_dir(void) {
    char tmp_template[] = "/tmp/ray_tracing_export_video_empty_XXXXXX";
    char *tmp_root = mkdtemp(tmp_template);
    char original_frame_dir[sizeof(animSettings.frameDir)];
    char original_video_root[sizeof(animSettings.videoOutputRoot)];
    RayTracingRenderExportStatus status = {0};

    assert_true("export_video_empty_tmpdir_created", tmp_root != NULL);
    if (!tmp_root) return 0;

    strncpy(original_frame_dir, animSettings.frameDir, sizeof(original_frame_dir) - 1);
    original_frame_dir[sizeof(original_frame_dir) - 1] = '\0';
    strncpy(original_video_root, animSettings.videoOutputRoot, sizeof(original_video_root) - 1);
    original_video_root[sizeof(original_video_root) - 1] = '\0';

    strncpy(animSettings.frameDir, tmp_root, sizeof(animSettings.frameDir) - 1);
    animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
    strncpy(animSettings.videoOutputRoot, tmp_root, sizeof(animSettings.videoOutputRoot) - 1);
    animSettings.videoOutputRoot[sizeof(animSettings.videoOutputRoot) - 1] = '\0';

    assert_true("export_video_empty_rejected",
                !ray_tracing_render_export_make_video(&status));
    assert_true("export_video_empty_code",
                status.code == RAY_TRACING_RENDER_EXPORT_NO_FRAMES);

    strncpy(animSettings.frameDir, original_frame_dir, sizeof(animSettings.frameDir) - 1);
    animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
    strncpy(animSettings.videoOutputRoot, original_video_root, sizeof(animSettings.videoOutputRoot) - 1);
    animSettings.videoOutputRoot[sizeof(animSettings.videoOutputRoot) - 1] = '\0';

    rmdir(tmp_root);
    return 0;
}

static int test_animation_deep_render_frame_resume_roundtrip_and_clamp(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    const char* json_missing_start_resume =
        "{\n"
        "  \"spaceMode\": 1,\n"
        "  \"deepRenderMode\": true\n"
        "}\n";
    const char* json_invalid_start_resume =
        "{\n"
        "  \"spaceMode\": 1,\n"
        "  \"deepRenderMode\": true,\n"
        "  \"startFrameIndex\": -7,\n"
        "  \"resumeFromExistingFrames\": true\n"
        "}\n";

    assert_true("deep_render_start_resume_write_missing",
                write_text_file(kRuntimeAnimationConfigPath, json_missing_start_resume));
    LoadAnimationConfig();
    assert_true("deep_render_start_missing_defaults_zero",
                animSettings.startFrameIndex == 0);
    assert_true("deep_render_resume_missing_defaults_off",
                !animSettings.resumeFromExistingFrames);

    assert_true("deep_render_start_resume_write_invalid",
                write_text_file(kRuntimeAnimationConfigPath, json_invalid_start_resume));
    LoadAnimationConfig();
    assert_true("deep_render_start_invalid_clamped_zero",
                animSettings.startFrameIndex == 0);
    assert_true("deep_render_resume_invalid_preserved_true",
                animSettings.resumeFromExistingFrames);

    animSettings.startFrameIndex = 117;
    animSettings.resumeFromExistingFrames = true;
    SaveAnimationConfig();
    animSettings.startFrameIndex = 0;
    animSettings.resumeFromExistingFrames = false;
    LoadAnimationConfig();
    assert_true("deep_render_start_roundtrip_persisted",
                animSettings.startFrameIndex == 117);
    assert_true("deep_render_resume_roundtrip_persisted",
                animSettings.resumeFromExistingFrames);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_native_3d_caustics_roundtrip_and_default_off(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    const char* json_missing_caustics =
        "{\n"
        "  \"spaceMode\": 1,\n"
        "  \"integratorMode3D\": 3\n"
        "}\n";

    animSettings.causticMode3D = 3;
    animSettings.causticTransportEngine3D = 1;
    animSettings.causticSurfaceCacheEnabled3D = true;
    animSettings.causticVolumeCacheEnabled3D = false;
    animSettings.causticSampleBudget3D = 4096;
    animSettings.causticMaxPathDepth3D = 8;
    animSettings.causticDebugSummaryEnabled3D = true;
    animSettings.causticDebugExportEnabled3D = false;
    animSettings.menuWorkspaceModule = 2;
    animSettings.menuPaneSceneWidth = 430;
    animSettings.menuPaneHealthWidth = 380;
    SaveAnimationConfig();

    animSettings.causticMode3D = 0;
    animSettings.causticTransportEngine3D = 0;
    animSettings.causticSurfaceCacheEnabled3D = false;
    animSettings.causticVolumeCacheEnabled3D = true;
    animSettings.causticSampleBudget3D = 0;
    animSettings.causticMaxPathDepth3D = 0;
    animSettings.causticDebugSummaryEnabled3D = false;
    animSettings.causticDebugExportEnabled3D = true;
    animSettings.menuWorkspaceModule = 0;
    animSettings.menuPaneSceneWidth = 390;
    animSettings.menuPaneHealthWidth = 340;
    LoadAnimationConfig();

    assert_true("caustics_roundtrip_mode", animSettings.causticMode3D == 3);
    assert_true("caustics_roundtrip_engine",
                animSettings.causticTransportEngine3D == 1);
    assert_true("caustics_roundtrip_surface_cache",
                animSettings.causticSurfaceCacheEnabled3D);
    assert_true("caustics_roundtrip_volume_cache",
                !animSettings.causticVolumeCacheEnabled3D);
    assert_true("caustics_roundtrip_sample_budget",
                animSettings.causticSampleBudget3D == 4096);
    assert_true("caustics_roundtrip_path_depth",
                animSettings.causticMaxPathDepth3D == 8);
    assert_true("caustics_roundtrip_debug_summary",
                animSettings.causticDebugSummaryEnabled3D);
    assert_true("caustics_roundtrip_debug_export",
                !animSettings.causticDebugExportEnabled3D);
    assert_true("menu_workspace_roundtrip_module",
                animSettings.menuWorkspaceModule == 2);
    assert_true("menu_workspace_roundtrip_scene_width",
                animSettings.menuPaneSceneWidth == 430);
    assert_true("menu_workspace_roundtrip_health_width",
                animSettings.menuPaneHealthWidth == 380);

    assert_true("caustics_write_legacy_config",
                write_text_file(kRuntimeAnimationConfigPath, json_missing_caustics));
    LoadAnimationConfig();
    assert_true("caustics_legacy_mode_defaults_off",
                animSettings.causticMode3D == RUNTIME_3D_CAUSTIC_MODE_DEFAULT);
    assert_true("caustics_legacy_surface_cache_defaults_off",
                !animSettings.causticSurfaceCacheEnabled3D);
    assert_true("caustics_legacy_volume_cache_defaults_off",
                !animSettings.causticVolumeCacheEnabled3D);
    assert_true("caustics_legacy_budget_defaults_zero",
                animSettings.causticSampleBudget3D == 0);
    assert_true("caustics_legacy_depth_defaults_zero",
                animSettings.causticMaxPathDepth3D == 0);
    assert_true("menu_workspace_legacy_module_defaults_render",
                animSettings.menuWorkspaceModule == MENU_WORKSPACE_MODULE_DEFAULT);
    assert_true("menu_workspace_legacy_scene_width_defaults",
                animSettings.menuPaneSceneWidth == MENU_PANE_SCENE_WIDTH_DEFAULT);
    assert_true("menu_workspace_legacy_health_width_defaults",
                animSettings.menuPaneHealthWidth == MENU_PANE_HEALTH_WIDTH_DEFAULT);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_native_3d_export_frame_bmp_uses_preview_buffer_directly(void) {
    char tmp_template[] = "/tmp/ray_tracing_native3d_export_XXXXXX";
    char* tmp_root = mkdtemp(tmp_template);
    char frame_path[PATH_MAX];
    const Uint8 preview_rgba[] = {
        255u, 0u,   0u,   255u,
        0u,   255u, 0u,   255u
    };
    SDL_Surface* loaded = NULL;
    SDL_Surface* converted = NULL;
    Uint32* pixels = NULL;
    Uint8 r = 0u;
    Uint8 g = 0u;
    Uint8 b = 0u;
    Uint8 a = 0u;

    assert_true("native_3d_export_tmpdir_created", tmp_root != NULL);
    if (!tmp_root) return 0;

    snprintf(frame_path, sizeof(frame_path), "%s/frame_0000.bmp", tmp_root);
    assert_true("native_3d_export_save_ok",
                RayTracing2Native3DOverlay_ExportFrameBMP(frame_path,
                                                          2,
                                                          1,
                                                          preview_rgba,
                                                          NULL));
    assert_true("native_3d_export_file_exists", path_exists(frame_path));

    loaded = SDL_LoadBMP(frame_path);
    assert_true("native_3d_export_load_saved_bmp", loaded != NULL);
    if (loaded) {
        converted = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_RGBA32, 0);
        assert_true("native_3d_export_convert_loaded_surface", converted != NULL);
    }
    if (converted) {
        pixels = (Uint32*)converted->pixels;
        SDL_GetRGBA(pixels[0], converted->format, &r, &g, &b, &a);
        assert_true("native_3d_export_first_pixel_red",
                    r == 255u && g == 0u && b == 0u);
        SDL_GetRGBA(pixels[1], converted->format, &r, &g, &b, &a);
        assert_true("native_3d_export_second_pixel_green",
                    r == 0u && g == 255u && b == 0u);
    }

    if (converted) SDL_FreeSurface(converted);
    if (loaded) SDL_FreeSurface(loaded);
    unlink(frame_path);
    rmdir(tmp_root);
    return 0;
}

int run_test_config_animation_settings_export_suite(void) {
    int before = test_support_failures();

    test_animation_integrator_split_roundtrip_and_default_3d();
    test_agent_render_request_disney_v2_integrator_label_roundtrip();
    test_agent_render_request_denoise_override_roundtrip();
    test_agent_render_request_disney_v2_caustic_sidecar_roundtrip();
    test_agent_render_request_disney_v2_caustic_mode_off_roundtrip();
    test_agent_render_request_spatial_caustic_cache_phase0_contract();
    test_agent_render_request_spatial_caustic_cache_phase3_combined_sidecar();
    test_agent_render_request_disney_v2_caustic_transport_reserved();
    test_agent_render_request_trace_route_roundtrip_and_validation();
    test_agent_render_request_caustic_transport_volume_phase4_contract();
    test_agent_render_request_caustic_photon_map_contract_only();
    test_agent_render_request_caustic_photon_product_mode_ppm6();
    test_agent_render_request_caustic_lens_traversal_profile_override();
    test_agent_render_request_caustic_transport_surface_sidecar_combined_contract();
    test_agent_render_request_caustic_sidecar_rejects_non_disney_v2();
    test_agent_render_request_volume_visible_roundtrip();
    test_agent_render_request_volume_material_remap_roundtrip();
    test_agent_render_request_resource_budget_roundtrip();
    test_agent_render_request_resource_budget_env_default();
    test_agent_render_request_environment_lighting_overrides();
    test_agent_render_request_render_trace_cost_ledger_flag();
    test_animation_native_3d_temporal_frames_roundtrip_and_clamp();
    test_animation_native_3d_bounce_depth_and_roulette_roundtrip_and_clamp();
    test_animation_native_3d_top_fill_roundtrip_and_default();
    test_animation_native_3d_environment_model_roundtrip_and_default();
    test_animation_native_3d_disney_denoise_roundtrip_and_default();
    test_animation_environment_brightness_byte_floor_roundtrip_and_legacy_migration();
    test_animation_light_intensity_missing_uses_authored_default();
    test_animation_runtime_window_override_roundtrip_and_apply();
    test_animation_native_3d_render_scale_roundtrip_and_clamp();
    test_runtime_native_3d_resolution_scale_contract();
    test_animation_video_output_root_migrates_from_output_root();
    test_data_paths_resolve_video_output_path_uses_configured_root();
    test_animation_deep_render_frame_resume_roundtrip_and_clamp();
    test_animation_native_3d_caustics_roundtrip_and_default_off();
    test_render_export_batch_counts_and_clears_frames();
    test_render_export_batch_reports_highest_and_next_frame();
    test_render_export_batch_make_video_rejects_empty_frame_dir();
    test_native_3d_export_frame_bmp_uses_preview_buffer_directly();
    return test_support_failures() - before;
}
