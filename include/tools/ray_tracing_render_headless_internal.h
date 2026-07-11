#ifndef RAY_TRACING_RENDER_HEADLESS_INTERNAL_H
#define RAY_TRACING_RENDER_HEADLESS_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "app/agent_render_request.h"
#include "import/runtime_scene_bridge.h"
#include "import/runtime_scene_motion_bridge.h"
#include "import/runtime_mesh_asset_loader.h"
#include "render/ray_tracing_mode_backend.h"
#include "render/runtime_dynamic_geometry_accel_3d.h"
#include "render/runtime_caustic_photon_integration_3d.h"
#include "render/runtime_mesh_blas_cache_3d.h"
#include "render/runtime_native_3d_render.h"
#include "render/runtime_render_trace_cost_ledger_3d.h"
#include "render/runtime_scene_accel_3d.h"
#include "render/runtime_scene_3d_builder.h"
#include "render/runtime_triangle_bvh_3d.h"
#include "render/runtime_volume_3d_debug.h"

#define RAY_TRACING_HEADLESS_OBJECT_AUDIT_MAX 64

typedef struct RayTracingHeadlessObjectAuditEntry {
    bool used;
    int scene_object_index;
    char object_id[64];
    char object_type[20];
    int material_id;
    double alpha;
    double reflectivity;
    double roughness;
    double emissive_strength;
    int texture_id;
    double texture_strength;
    double texture_scale;
    double texture_offset_u;
    double texture_offset_v;
    int texture_seed;
    int texture_pattern_mode;
    double texture_coverage;
    double texture_grain;
    double texture_edge_softness;
    double texture_contrast;
    double texture_flow;
    double texture_color_depth;
    double texture_surface_damage;
    int packed_color;
    int primitive_count;
    int triangle_count;
    int primary_hit_pixels;
    double center_x;
    double center_y;
    double center_z;
    bool center_projectable;
    bool center_inside_viewport;
    double center_screen_x;
    double center_screen_y;
    double center_camera_depth;
} RayTracingHeadlessObjectAuditEntry;

typedef struct RayTracingHeadlessPreflight {
    bool request_loaded;
    bool scene_applied;
    bool volume_attached;
    bool volume_summary_built;
    bool route_native_3d;
    bool prepared_frame;
    bool rendered_frames;
    bool denoise_enabled;
    int frames_rendered;
    RayTracingRuntimeRoute route;
    RuntimeSceneBridgePreflight scene_summary;
    RuntimeMotionTrack3DSummary object_motion_summary;
    RuntimeEnvironment3D environment_summary;
    bool environment_summary_built;
    int registered_light_count;
    int registered_enabled_light_count;
    int registered_light_point_count;
    int registered_light_sphere_count;
    int registered_light_disk_count;
    int registered_light_rect_count;
    int registered_light_mesh_emissive_count;
    int registered_light_authored_count;
    int registered_light_material_emitter_count;
    int registered_light_compatibility_count;
    int registered_light_material_emitter_enabled_count;
    int registered_light_mesh_area_sampler_only_count;
    int registered_light_emission_omni_count;
    int registered_light_emission_one_sided_count;
    int registered_light_emission_two_sided_count;
    int registered_light_emissive_candidate_count;
    double registered_light_emissive_area;
    double registered_light_emissive_weight;
    double registered_light_emissive_proxy_radius_max;
    double registered_light_first_position_x;
    double registered_light_first_position_y;
    double registered_light_first_position_z;
    double registered_light_first_radius;
    double registered_light_first_intensity;
    double registered_light_first_color_r;
    double registered_light_first_color_g;
    double registered_light_first_color_b;
    RuntimeVolumeDebugSummary3D volume_summary;
    bool volume_frame_selection_built;
    bool volume_frame_selection_dynamic;
    int volume_requested_first_frame_index;
    int volume_requested_last_frame_index;
    uint64_t volume_loaded_first_frame_index;
    uint64_t volume_loaded_last_frame_index;
    char volume_selected_first_frame_path[PATH_MAX];
    char volume_selected_last_frame_path[PATH_MAX];
    bool water_surface_source_found;
    bool water_surface_loaded;
    bool water_surface_frame_selection_built;
    bool water_surface_frame_selection_dynamic;
    bool water_surface_mesh_attached;
    int water_surface_requested_first_frame_index;
    int water_surface_requested_last_frame_index;
    uint64_t water_surface_loaded_first_frame_index;
    uint64_t water_surface_loaded_last_frame_index;
    char water_surface_manifest_path[PATH_MAX];
    char water_surface_selected_first_frame_path[PATH_MAX];
    char water_surface_selected_last_frame_path[PATH_MAX];
    char water_surface_axis[8];
    uint32_t water_surface_grid_w;
    uint32_t water_surface_grid_d;
    uint64_t water_surface_sample_count;
    uint32_t water_surface_last_grid_w;
    uint32_t water_surface_last_grid_d;
    uint64_t water_surface_last_sample_count;
    uint32_t water_surface_wet_columns;
    uint32_t water_surface_dry_columns;
    uint32_t water_surface_solid_columns;
    uint32_t water_surface_water_cells;
    double water_surface_min_y;
    double water_surface_max_y;
    double water_surface_avg_y;
    double water_surface_max_slope;
    bool water_surface_finite_normals;
    double water_surface_material_ior;
    double water_surface_absorption_distance_m;
    double water_surface_absorption_r;
    double water_surface_absorption_g;
    double water_surface_absorption_b;
    double water_surface_material_reflectivity;
    double water_surface_material_roughness;
    bool water_surface_material_payload_applied;
    double water_surface_payload_ior;
    double water_surface_payload_absorption_distance_m;
    double water_surface_payload_transparency;
    double water_surface_payload_reflectivity;
    double water_surface_payload_roughness;
    double water_surface_payload_tint_r;
    double water_surface_payload_tint_g;
    double water_surface_payload_tint_b;
    int water_surface_triangle_count;
    RuntimeNative3DRenderStats stats;
    RuntimeCausticPhotonRenderCallsiteReadback3D causticPhotonCallsiteReadback;
    bool causticPhotonCallsiteReadbackBuilt;
    RuntimeNative3DPreparedSceneCacheStats prepared_scene_cache_stats;
    RuntimeDynamicGeometryWaterCacheDiagnostics3D dynamic_water_cache_stats;
    RuntimeSceneAcceleration3DDiagnostics scene_acceleration_stats;
    RuntimeRay3DRouteStats ray_trace_route_stats;
    RuntimeRenderTraceCostLedger3D render_trace_cost_ledger;
    RayTracingRuntimeMeshAssetTimingStats mesh_asset_timing_stats;
    RuntimeScene3DBuilderTimingStats scene_builder_timing_stats;
    double runtime_scene_apply_ms;
    double runtime_scene_preflight_ms;
    double native_prepare_frame_ms;
    double caustic_cache_prep_ms;
    double object_audit_ms;
    double render_frames_ms;
    double render_trace_ms;
    double frame_analysis_ms;
    double frame_write_ms;
    double video_encode_ms;
    double total_run_ms;
    RuntimeTriangleBVH3DBuildStats bvh_build_stats;
    RuntimeTriangleBVH3DTraceStats bvh_trace_stats;
    size_t nonzero_pixels;
    uint8_t max_r;
    uint8_t max_g;
    uint8_t max_b;
    char frame_dir[PATH_MAX];
    char first_frame_path[PATH_MAX];
    char last_frame_path[PATH_MAX];
    int object_audit_count;
    bool object_audit_enabled;
    int object_audit_width;
    int object_audit_height;
    int object_audit_stride_x;
    int object_audit_stride_y;
    int object_audit_sample_count;
    int object_audit_scale_factor;
    RayTracingHeadlessObjectAuditEntry
        object_audit[RAY_TRACING_HEADLESS_OBJECT_AUDIT_MAX];
    char diagnostics[1024];
} RayTracingHeadlessPreflight;

typedef struct RayTracingTemporalProgressContext {
    const RayTracingAgentRenderRequest *request;
    const char *job_status_path;
    const char *job_id;
    const char *request_path;
    int frame_index;
    int frames_completed;
    int started_subpasses;
    int completed_subpasses;
    int total_subpasses;
    size_t completed_tiles_in_subpass;
    size_t total_tiles_in_subpass;
    struct timespec frame_started_at;
} RayTracingTemporalProgressContext;

void ray_tracing_render_headless_usage(const char *argv0);
void ray_tracing_headless_write_startup_load_timing_matrix(
    FILE* file,
    const RayTracingHeadlessPreflight* preflight);
void ray_tracing_headless_write_frame_dataflow_state_ledger(
    FILE* file,
    const RayTracingAgentRenderRequest* request,
    const RayTracingHeadlessPreflight* preflight);
void ray_tracing_headless_write_direct_light_visibility_policy(
    FILE* file,
    const RuntimeRenderTraceCostLedger3D* ledger);
void ray_tracing_headless_write_caustic_state_summary(
    FILE *file,
    const RayTracingAgentRenderRequest *request,
    const RayTracingHeadlessPreflight *preflight);
void ray_tracing_headless_write_object_audit(
    FILE *file,
    const RayTracingHeadlessPreflight *preflight);
void ray_tracing_headless_audit_prepared_frame(
    RayTracingHeadlessPreflight *preflight,
    const RuntimeNative3DPreparedFrame *frame,
    const RayTracingAgentRenderRequest *request);
void ray_tracing_headless_probe_caustic_photon_callsite(
    RayTracingHeadlessPreflight *preflight,
    const RuntimeNative3DPreparedFrame *frame,
    const RayTracingAgentRenderRequest *request);
void ray_tracing_headless_probe_caustic_photon_trace_callsite(
    RayTracingHeadlessPreflight *preflight,
    const RuntimeNative3DPreparedFrame *frame,
    const RayTracingAgentRenderRequest *request);
bool ray_tracing_headless_request_has_volume_source(
    const RayTracingAgentRenderRequest *request);
bool ray_tracing_headless_populate_volume_frame_selection(
    RayTracingHeadlessPreflight *preflight,
    const RayTracingAgentRenderRequest *request);
bool ray_tracing_headless_populate_water_surface_frame_selection(
    RayTracingHeadlessPreflight *preflight,
    const RayTracingAgentRenderRequest *request);
void ray_tracing_headless_note_water_surface_mesh(
    RayTracingHeadlessPreflight *preflight,
    const RuntimeNative3DPreparedFrame *frame);
void ray_tracing_headless_apply_inspection_overrides(
    const RayTracingAgentRenderRequest *request);
size_t ray_tracing_headless_count_nonzero_pixels(const uint8_t *pixels,
                                                 int width,
                                                 int height,
                                                 uint8_t *out_max_r,
                                                 uint8_t *out_max_g,
                                                 uint8_t *out_max_b);
double ray_tracing_headless_frame_normalized_t(const RayTracingAgentRenderRequest *request,
                                               int local_frame);
const RuntimeNative3DResourceBudget *ray_tracing_headless_request_resource_budget(
    const RayTracingAgentRenderRequest *request,
    RuntimeNative3DResourceBudget *out_budget);
double ray_tracing_elapsed_seconds_since(const struct timespec *start_time);
double ray_tracing_elapsed_ms_since(const struct timespec *start_time);
void ray_tracing_temporal_progress_callback(int started_subpasses,
                                            int completed_subpasses,
                                            int total_subpasses,
                                            void *user_data);
void ray_tracing_tile_progress_callback(int started_subpasses,
                                        int completed_subpasses,
                                        int total_subpasses,
                                        size_t completed_tiles_in_subpass,
                                        size_t total_tiles_in_subpass,
                                        void *user_data);
void ray_tracing_headless_finalize_render_diagnostics(
    RayTracingHeadlessPreflight *preflight,
    const RayTracingAgentRenderRequest *request);
int ray_tracing_headless_encode_video_if_requested(
    const RayTracingAgentRenderRequest *request,
    RayTracingHeadlessPreflight *preflight,
    const char *job_status_path,
    const char *job_id,
    const char *request_path);
void ray_tracing_headless_write_completed_progress(
    const RayTracingAgentRenderRequest *request,
    const RayTracingHeadlessPreflight *preflight,
    const char *job_status_path,
    const char *job_id,
    const char *request_path);
int ray_tracing_headless_validate_render_output_root(
    const RayTracingAgentRenderRequest *request,
    RayTracingHeadlessPreflight *preflight);
int ray_tracing_headless_prepare_frame_directory_and_buffer(
    const RayTracingAgentRenderRequest *request,
    RayTracingHeadlessPreflight *preflight,
    uint8_t **out_pixels);
void ray_tracing_headless_initial_light_point(double *out_x, double *out_y);
void ray_tracing_headless_reset_render_trace_state(void);
int ray_tracing_headless_note_render_frame_failed(
    const RayTracingAgentRenderRequest *request,
    RayTracingHeadlessPreflight *preflight,
    RayTracingTemporalProgressContext *temporal_progress,
    const struct timespec *stage_started_at,
    int frame_index,
    const char *job_status_path,
    const char *job_id,
    const char *request_path);
int ray_tracing_headless_note_bvh_flat_fallback_failed(
    const RayTracingAgentRenderRequest *request,
    RayTracingHeadlessPreflight *preflight,
    RayTracingTemporalProgressContext *temporal_progress,
    int frame_index,
    const char *job_status_path,
    const char *job_id,
    const char *request_path);
int ray_tracing_headless_prepare_frame_output(
    char *frame_path,
    size_t frame_path_size,
    const RayTracingAgentRenderRequest *request,
    RayTracingHeadlessPreflight *preflight,
    int frame_index,
    const char *job_status_path,
    const char *job_id,
    const char *request_path);
void ray_tracing_headless_note_rendering_frame_started(
    const RayTracingAgentRenderRequest *request,
    const RayTracingHeadlessPreflight *preflight,
    int frame_index,
    const char *job_status_path,
    const char *job_id,
    const char *request_path);
int ray_tracing_headless_write_rendered_frame_output(
    const char *frame_path,
    const uint8_t *pixels,
    int local_frame,
    int frame_index,
    const RayTracingAgentRenderRequest *request,
    RayTracingHeadlessPreflight *preflight,
    RayTracingTemporalProgressContext *temporal_progress,
    const char *job_status_path,
    const char *job_id,
    const char *request_path);
void ray_tracing_headless_write_render_trace_cost_ledger(
    FILE* file,
    const RayTracingHeadlessPreflight* preflight);
void ray_tracing_headless_write_render_stats_summary(
    FILE *file,
    const RayTracingHeadlessPreflight *preflight);
void ray_tracing_headless_write_dynamic_geometry_acceleration_summary(
    FILE *file,
    const RayTracingHeadlessPreflight *preflight);
void ray_tracing_headless_write_dynamic_water_acceleration_cache_summary(
    FILE *file,
    const RayTracingHeadlessPreflight *preflight);
void ray_tracing_headless_write_object_motion_acceleration_summary(
    FILE *file,
    const RayTracingAgentRenderRequest *request,
    const RayTracingHeadlessPreflight *preflight);
void ray_tracing_render_headless_write_summary(FILE *file,
                                               const RayTracingAgentRenderRequest *request,
                                               const RayTracingHeadlessPreflight *preflight);
bool ray_tracing_render_headless_write_summary_file(
    const char *path,
    const RayTracingAgentRenderRequest *request,
    const RayTracingHeadlessPreflight *preflight);
bool ray_tracing_render_headless_write_job_status_file(
    const char *path,
    const char *job_id,
    const char *request_path,
    const RayTracingAgentRenderRequest *request,
    const char *state,
    const char *stage,
    int exit_code,
    int frame_index,
    int frames_completed,
    int temporal_subpasses_started,
    int temporal_subpasses_completed,
    int temporal_subpasses_total,
    size_t completed_tiles_in_subpass,
    size_t total_tiles_in_subpass,
    double elapsed_seconds,
    double estimated_remaining_seconds,
    const char *diagnostics);
bool ray_tracing_render_headless_write_progress_and_job_status(
    const char *progress_path,
    const RayTracingAgentRenderRequest *request,
    const char *stage,
    int frame_index,
    int frames_completed,
    int temporal_subpasses_started,
    int temporal_subpasses_completed,
    int temporal_subpasses_total,
    size_t completed_tiles_in_subpass,
    size_t total_tiles_in_subpass,
    double elapsed_seconds,
    double estimated_remaining_seconds,
    const char *state,
    const char *diagnostics,
    const char *job_status_path,
    const char *job_id,
    const char *request_path,
    int exit_code);
void ray_tracing_render_headless_write_process_started_status(
    const char *job_status_path,
    const char *job_id,
    const char *request_path,
    const RayTracingAgentRenderRequest *request,
    bool render_mode);
void ray_tracing_render_headless_write_process_finished_status(
    const char *job_status_path,
    const char *job_id,
    const char *request_path,
    const RayTracingAgentRenderRequest *request,
    const RayTracingHeadlessPreflight *preflight,
    int run_code);

#endif
