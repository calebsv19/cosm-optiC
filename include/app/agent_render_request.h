#ifndef RAY_TRACING_AGENT_RENDER_REQUEST_H
#define RAY_TRACING_AGENT_RENDER_REQUEST_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>

#include "config/config_manager.h"
#include "render/ray_tracing_integrator_catalog.h"

#define RAY_TRACING_AGENT_RENDER_REQUEST_SCHEMA "ray_tracing_agent_render_request_v1"

typedef struct RayTracingAgentRenderRequest {
    char schema_version[64];
    char run_id[128];
    char runtime_scene_path[PATH_MAX];
    bool volume_enabled;
    int volume_source_kind;
    char volume_source_path[PATH_MAX];
    bool volume_affects_lighting;
    bool volume_debug_overlay;
    char output_root[PATH_MAX];
    char summary_path[PATH_MAX];
    char progress_path[PATH_MAX];
    int start_frame;
    int frame_count;
    int width;
    int height;
    double normalized_t;
    int temporal_frames;
    bool has_sampling_window;
    int sampling_frame_offset;
    int sampling_frame_count;
    RayTracing3DIntegratorId integrator_3d;
    bool has_integrator_3d_override;
    int inspection_preset;
    bool has_camera_zoom_override;
    double camera_zoom_override;
    bool has_camera_position_override;
    double camera_position_x;
    double camera_position_y;
    double camera_position_z;
    bool has_camera_look_at_override;
    double camera_look_at_x;
    double camera_look_at_y;
    double camera_look_at_z;
    bool has_environment_brightness_override;
    double environment_brightness_override;
    bool has_ambient_strength_override;
    double ambient_strength_override;
    bool has_environment_light_mode_override;
    int environment_light_mode_override;
    bool has_top_fill_strength_override;
    double top_fill_strength_override;
    bool has_light_intensity_override;
    double light_intensity_override;
    bool has_light_radius_override;
    double light_radius_override;
    bool has_forward_decay_override;
    double forward_decay_override;
    bool has_volume_scatter_gain_override;
    double volume_scatter_gain_override;
    bool has_volume_step_scale_override;
    double volume_step_scale_override;
    bool has_secondary_diffuse_samples_3d_override;
    int secondary_diffuse_samples_3d_override;
    bool has_transmission_samples_3d_override;
    int transmission_samples_3d_override;
    bool has_volume_tint_override;
    double volume_tint_r;
    double volume_tint_g;
    double volume_tint_b;
    bool overwrite;
} RayTracingAgentRenderRequest;

void ray_tracing_agent_render_request_defaults(RayTracingAgentRenderRequest *request);
bool ray_tracing_agent_render_request_load_file(const char *request_path,
                                                RayTracingAgentRenderRequest *out_request,
                                                char *out_diagnostics,
                                                size_t out_diagnostics_size);
const char *ray_tracing_agent_render_request_volume_kind_label(int kind);
const char *ray_tracing_agent_render_request_integrator_label(RayTracing3DIntegratorId id);
const char *ray_tracing_agent_render_request_inspection_preset_label(int preset);

#endif
