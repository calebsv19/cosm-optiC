#ifndef RAY_TRACING_RENDER_HEADLESS_INTERNAL_H
#define RAY_TRACING_RENDER_HEADLESS_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include "app/agent_render_request.h"
#include "import/runtime_scene_bridge.h"
#include "render/ray_tracing_mode_backend.h"
#include "render/runtime_native_3d_render.h"
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
    int frames_rendered;
    RayTracingRuntimeRoute route;
    RuntimeSceneBridgePreflight scene_summary;
    RuntimeVolumeDebugSummary3D volume_summary;
    RuntimeNative3DRenderStats stats;
    size_t nonzero_pixels;
    uint8_t max_r;
    uint8_t max_g;
    uint8_t max_b;
    char frame_dir[PATH_MAX];
    char first_frame_path[PATH_MAX];
    char last_frame_path[PATH_MAX];
    int object_audit_count;
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
    const char *state,
    const char *diagnostics,
    const char *job_status_path,
    const char *job_id,
    const char *request_path,
    int exit_code);

#endif
