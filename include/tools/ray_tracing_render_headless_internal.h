#ifndef RAY_TRACING_RENDER_HEADLESS_INTERNAL_H
#define RAY_TRACING_RENDER_HEADLESS_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include "app/agent_render_request.h"
#include "import/runtime_scene_bridge.h"
#include "render/ray_tracing_mode_backend.h"
#include "render/runtime_native_3d_render.h"
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
    RuntimeEnvironment3D environment_summary;
    bool environment_summary_built;
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
    bool water_surface_material_payload_applied;
    double water_surface_payload_ior;
    double water_surface_payload_absorption_distance_m;
    double water_surface_payload_transparency;
    double water_surface_payload_tint_r;
    double water_surface_payload_tint_g;
    double water_surface_payload_tint_b;
    int water_surface_triangle_count;
    RuntimeNative3DRenderStats stats;
    RuntimeNative3DPreparedSceneCacheStats prepared_scene_cache_stats;
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

#endif
