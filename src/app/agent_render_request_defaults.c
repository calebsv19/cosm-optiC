#include "app/agent_render_request_internal.h"

void ray_tracing_agent_render_request_defaults(RayTracingAgentRenderRequest *request) {
    if (!request) return;
    memset(request, 0, sizeof(*request));
    snprintf(request->schema_version,
             sizeof(request->schema_version),
             "%s",
             RAY_TRACING_AGENT_RENDER_REQUEST_SCHEMA);
    snprintf(request->run_id, sizeof(request->run_id), "ray_tracing_agent_run");
    request->volume_enabled = false;
    request->volume_source_kind = VOLUME_SOURCE_NONE;
    request->volume_visible = true;
    request->volume_affects_lighting = true;
    request->volume_debug_overlay = false;
    request->video_enabled = false;
    request->video_fps = 30;
    request->start_frame = 0;
    request->frame_count = 1;
    request->width = 640;
    request->height = 360;
    request->normalized_t = 0.0;
    request->temporal_frames = 1;
    request->has_denoise_enabled_override = false;
    request->denoise_enabled_override = true;
    request->has_sampling_window = false;
    request->sampling_frame_offset = 0;
    request->sampling_frame_count = 1;
    request->has_resource_budget = false;
    request->resource_cpu_percent = 0;
    request->resource_max_workers = 0;
    request->resource_reserve_cpu_count = 0;
    request->integrator_3d = RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT;
    request->has_integrator_3d_override = false;
    request->inspection_preset = RAY_TRACING_AGENT_RENDER_PRESET_NONE;
    request->camera_zoom_override = 1.0;
    request->camera_position_x = 0.0;
    request->camera_position_y = 0.0;
    request->camera_position_z = 0.0;
    request->camera_look_at_x = 0.0;
    request->camera_look_at_y = 0.0;
    request->camera_look_at_z = 0.0;
    request->environment_brightness_override = 0.0;
    request->ambient_strength_override = 0.0;
    request->environment_light_mode_override = ENVIRONMENT_LIGHT_MODE_OFF;
    request->environment_preset_override = ENVIRONMENT_PRESET_SKY;
    request->background_brightness_override = 0.0;
    request->background_color_r = 1.0;
    request->background_color_g = 1.0;
    request->background_color_b = 1.0;
    request->top_fill_strength_override = 1.0;
    request->light_intensity_override = 0.0;
    request->light_radius_override = 0.0;
    request->forward_decay_override = 0.0;
    request->volume_scatter_gain_override = 1.0;
    request->caustic_volume_scatter_gain_override = 1.0;
    request->volume_density_scale_override = 1.0;
    request->volume_density_gamma_override = 1.0;
    request->volume_absorption_gain_override = 1.0;
    request->volume_opacity_clamp_override = 1.0e30;
    request->volume_step_scale_override = 1.0;
    request->secondary_diffuse_samples_3d_override = RUNTIME_3D_SECONDARY_SAMPLES_DEFAULT;
    request->transmission_samples_3d_override = RUNTIME_3D_TRANSMISSION_SAMPLES_DEFAULT;
    request->has_trace_route_override = false;
    request->trace_route = RuntimeRay3D_DefaultTraceRoute();
    request->has_caustic_mode_override = false;
    request->caustic_mode = RUNTIME_DISNEY_V2_CAUSTIC_MODE_ANALYTIC;
    RuntimeCausticSettings3D_Default(&request->caustic_settings);
    request->has_caustic_product_mode_override = false;
    RuntimeCausticPhotonIntegration3D_DefaultSettings(
        &request->caustic_photon_integration_settings);
    request->caustic_photon_render_prep_population_enabled = false;
    request->caustic_photon_populated_callsite_readback_enabled = false;
    request->caustic_photon_trace_populated_callsite_readback_enabled = false;
    request->has_caustic_sidecar_enabled_override = false;
    request->caustic_sidecar_enabled = true;
    request->has_caustic_sidecar_strength_override = false;
    request->caustic_sidecar_strength = 1.0;
    request->volume_tint_r = 1.0;
    request->volume_tint_g = 1.0;
    request->volume_tint_b = 1.0;
    request->volume_albedo_r = 1.0;
    request->volume_albedo_g = 1.0;
    request->volume_albedo_b = 1.0;
    request->object_audit_enabled = true;
    request->object_audit_max_dimension = 160;
    request->overwrite = false;
}
