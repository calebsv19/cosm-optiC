#ifndef RAY_TRACING_AGENT_RENDER_REQUEST_H
#define RAY_TRACING_AGENT_RENDER_REQUEST_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>

#include "config/config_manager.h"
#include "render/ray_tracing_integrator_catalog.h"
#include "render/runtime_caustic_photon_integration_3d.h"
#include "render/runtime_caustic_settings_3d.h"
#include "render/runtime_disney_v2_caustic_sidecar_3d.h"
#include "render/runtime_ray_3d.h"

#define RAY_TRACING_AGENT_RENDER_REQUEST_SCHEMA "ray_tracing_agent_render_request_v1"

typedef struct RayTracingAgentRenderRequest {
    char schema_version[64];
    char run_id[128];
    char runtime_scene_path[PATH_MAX];
    bool volume_enabled;
    int volume_source_kind;
    char volume_source_path[PATH_MAX];
    bool volume_visible;
    bool volume_affects_lighting;
    bool volume_debug_overlay;
    char output_root[PATH_MAX];
    bool video_enabled;
    char video_path[PATH_MAX];
    int video_fps;
    char summary_path[PATH_MAX];
    char progress_path[PATH_MAX];
    int start_frame;
    int frame_count;
    int width;
    int height;
    double normalized_t;
    int temporal_frames;
    bool has_tiled_renderer_override;
    bool tiled_renderer_override;
    bool has_tile_size_override;
    int tile_size_override;
    bool has_adaptive_sampling_override;
    bool adaptive_sampling_enabled_override;
    bool has_denoise_enabled_override;
    bool denoise_enabled_override;
    bool has_sampling_window;
    int sampling_frame_offset;
    int sampling_frame_count;
    bool has_resource_budget;
    int resource_cpu_percent;
    int resource_max_workers;
    int resource_reserve_cpu_count;
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
    bool has_environment_preset_override;
    int environment_preset_override;
    bool has_background_brightness_override;
    double background_brightness_override;
    bool has_background_color_override;
    double background_color_r;
    double background_color_g;
    double background_color_b;
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
    bool has_caustic_volume_scatter_gain_override;
    double caustic_volume_scatter_gain_override;
    bool has_volume_density_scale_override;
    double volume_density_scale_override;
    bool has_volume_density_gamma_override;
    double volume_density_gamma_override;
    bool has_volume_absorption_gain_override;
    double volume_absorption_gain_override;
    bool has_volume_opacity_clamp_override;
    double volume_opacity_clamp_override;
    bool has_volume_step_scale_override;
    double volume_step_scale_override;
    bool has_secondary_diffuse_samples_3d_override;
    int secondary_diffuse_samples_3d_override;
    bool has_transmission_samples_3d_override;
    int transmission_samples_3d_override;
    bool has_trace_route_override;
    RuntimeRay3DTraceRoute trace_route;
    bool has_caustic_mode_override;
    RuntimeDisneyV2CausticMode3D caustic_mode;
    RuntimeCausticSettings3D caustic_settings;
    bool has_caustic_product_mode_override;
    RuntimeCausticPhotonIntegrationSettings3D caustic_photon_integration_settings;
    bool caustic_photon_block_solid_dielectric_direct_paths;
    bool caustic_photon_render_prep_population_enabled;
    bool caustic_photon_populated_callsite_readback_enabled;
    bool caustic_photon_trace_populated_callsite_readback_enabled;
    bool has_caustic_sidecar_enabled_override;
    bool caustic_sidecar_enabled;
    bool has_caustic_sidecar_strength_override;
    double caustic_sidecar_strength;
    bool has_volume_tint_override;
    double volume_tint_r;
    double volume_tint_g;
    double volume_tint_b;
    bool has_volume_albedo_override;
    double volume_albedo_r;
    double volume_albedo_g;
    double volume_albedo_b;
    bool object_audit_enabled;
    int object_audit_max_dimension;
    bool render_trace_cost_ledger_enabled;
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
