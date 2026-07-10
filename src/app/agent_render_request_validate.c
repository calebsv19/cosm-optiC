#include "app/agent_render_request_internal.h"

bool agent_render_request_finalize_loaded(RayTracingAgentRenderRequest* request,
                                          json_object* root,
                                          const char* request_path,
                                          char* out_diagnostics,
                                          size_t out_diagnostics_size) {
    bool bool_value = false;

    if (!request || !root) {
        return false;
    }

    if (request->inspection_preset == RAY_TRACING_AGENT_RENDER_PRESET_GLASS_PREVIEW ||
        request->inspection_preset == RAY_TRACING_AGENT_RENDER_PRESET_GLASS_REVIEW) {
        if (!request->has_integrator_3d_override) {
            request->integrator_3d = RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY;
        }
        if (!request->has_secondary_diffuse_samples_3d_override) {
            request->has_secondary_diffuse_samples_3d_override = true;
            request->secondary_diffuse_samples_3d_override =
                (request->inspection_preset == RAY_TRACING_AGENT_RENDER_PRESET_GLASS_REVIEW) ? 24 : 8;
        }
        if (!request->has_transmission_samples_3d_override) {
            request->has_transmission_samples_3d_override = true;
            request->transmission_samples_3d_override =
                (request->inspection_preset == RAY_TRACING_AGENT_RENDER_PRESET_GLASS_REVIEW) ? 12 : 4;
        }
    }

    if (request->start_frame < 0 || request->frame_count <= 0 ||
        request->width <= 0 || request->height <= 0 ||
        request->temporal_frames <= 0) {
        json_object_put(root);
        agent_render_request_set_diagf(out_diagnostics,
                          out_diagnostics_size,
                          "request=%s field=render numeric fields out of range start_frame=%d frame_count=%d width=%d height=%d temporal_frames=%d",
                          request_path,
                          request->start_frame,
                          request->frame_count,
                          request->width,
                          request->height,
                          request->temporal_frames);
        return false;
    }
    if (request->has_tile_size_override && request->tile_size_override <= 0) {
        json_object_put(root);
        agent_render_request_set_diagf(out_diagnostics,
                          out_diagnostics_size,
                          "request=%s field=render.tile_size out of range tile_size=%d",
                          request_path,
                          request->tile_size_override);
        return false;
    }
    if (request->has_resource_budget) {
        if ((request->resource_cpu_percent != 0 &&
             (request->resource_cpu_percent < 1 || request->resource_cpu_percent > 100)) ||
            request->resource_max_workers < 0 ||
            request->resource_max_workers > 64 ||
            request->resource_reserve_cpu_count < 0 ||
            request->resource_reserve_cpu_count > 64) {
            json_object_put(root);
            agent_render_request_set_diagf(out_diagnostics,
                              out_diagnostics_size,
                              "request=%s field=resources budget out of range cpu_percent=%d max_workers=%d reserve_cpu_count=%d",
                              request_path,
                              request->resource_cpu_percent,
                              request->resource_max_workers,
                              request->resource_reserve_cpu_count);
            return false;
        }
        if (request->resource_cpu_percent == 0 && request->resource_max_workers == 0 &&
            request->resource_reserve_cpu_count == 0) {
            request->has_resource_budget = false;
        }
    }
    if (request->normalized_t < 0.0) request->normalized_t = 0.0;
    if (request->normalized_t > 1.0) request->normalized_t = 1.0;
    if (request->has_sampling_window) {
        if (request->sampling_frame_offset < 0 || request->sampling_frame_count <= 0) {
            json_object_put(root);
            agent_render_request_set_diagf(out_diagnostics,
                              out_diagnostics_size,
                              "request=%s field=sampling window out of range frame_offset=%d frame_count=%d",
                              request_path,
                              request->sampling_frame_offset,
                              request->sampling_frame_count);
            return false;
        }
    }
    if (request->has_camera_zoom_override && request->camera_zoom_override <= 0.0) {
        json_object_put(root);
        agent_render_request_set_diagf(out_diagnostics,
                          out_diagnostics_size,
                          "request=%s field=inspection.camera_zoom out of range value=%.6f",
                          request_path,
                          request->camera_zoom_override);
        return false;
    }
    if (request->has_environment_brightness_override) {
        if (request->environment_brightness_override < 0.0) {
            request->environment_brightness_override = 0.0;
        }
        if (request->environment_brightness_override > 255.0) {
            request->environment_brightness_override = 255.0;
        }
    }
    if (request->has_ambient_strength_override) {
        if (request->ambient_strength_override < 0.0) {
            request->ambient_strength_override = 0.0;
        }
        if (request->ambient_strength_override > 1.0) {
            request->ambient_strength_override = 1.0;
        }
    }
    if (request->has_environment_light_mode_override) {
        request->environment_light_mode_override =
            animation_config_environment_light_mode_clamp(request->environment_light_mode_override);
    }
    if (request->has_environment_preset_override) {
        request->environment_preset_override =
            animation_config_environment_preset_clamp(request->environment_preset_override);
    }
    if (request->has_background_brightness_override) {
        if (!isfinite(request->background_brightness_override) ||
            request->background_brightness_override < 0.0) {
            request->background_brightness_override = 0.0;
        }
        if (request->background_brightness_override > 4.0) {
            request->background_brightness_override = 4.0;
        }
    }
    if (request->has_background_color_override) {
        if (!isfinite(request->background_color_r)) request->background_color_r = 1.0;
        if (!isfinite(request->background_color_g)) request->background_color_g = 1.0;
        if (!isfinite(request->background_color_b)) request->background_color_b = 1.0;
        if (request->background_color_r < 0.0) request->background_color_r = 0.0;
        if (request->background_color_g < 0.0) request->background_color_g = 0.0;
        if (request->background_color_b < 0.0) request->background_color_b = 0.0;
        if (request->background_color_r > 1.0) request->background_color_r = 1.0;
        if (request->background_color_g > 1.0) request->background_color_g = 1.0;
        if (request->background_color_b > 1.0) request->background_color_b = 1.0;
    }
    if (request->has_top_fill_strength_override) {
        if (request->top_fill_strength_override < 0.0) {
            request->top_fill_strength_override = 0.0;
        }
        if (request->top_fill_strength_override > 20.0) {
            request->top_fill_strength_override = 20.0;
        }
    }
    if (request->has_light_intensity_override) {
        if (request->light_intensity_override < 0.0) {
            request->light_intensity_override = 0.0;
        }
        if (request->light_intensity_override > 20.0) {
            request->light_intensity_override = 20.0;
        }
    }
    if (request->has_light_radius_override) {
        if (request->light_radius_override < 0.0) {
            request->light_radius_override = 0.0;
        }
        if (request->light_radius_override > 25.0) {
            request->light_radius_override = 25.0;
        }
    }
    if (request->has_forward_decay_override) {
        if (request->forward_decay_override < 50.0) {
            request->forward_decay_override = 50.0;
        }
        if (request->forward_decay_override > 100000.0) {
            request->forward_decay_override = 100000.0;
        }
    }
    if (request->has_volume_scatter_gain_override) {
        if (request->volume_scatter_gain_override <= 0.0) {
            json_object_put(root);
            agent_render_request_set_diagf(out_diagnostics,
                              out_diagnostics_size,
                              "request=%s field=inspection.volume_scatter_gain out of range value=%.6f",
                              request_path,
                              request->volume_scatter_gain_override);
            return false;
        }
        if (request->volume_scatter_gain_override > 64.0) {
            request->volume_scatter_gain_override = 64.0;
        }
    }
    if (request->has_caustic_volume_scatter_gain_override) {
        if (request->caustic_volume_scatter_gain_override <= 0.0) {
            json_object_put(root);
            agent_render_request_set_diagf(out_diagnostics,
                              out_diagnostics_size,
                              "request=%s field=inspection.caustic_volume_scatter_gain out of range value=%.6f",
                              request_path,
                              request->caustic_volume_scatter_gain_override);
            return false;
        }
        if (request->caustic_volume_scatter_gain_override > 64.0) {
            request->caustic_volume_scatter_gain_override = 64.0;
        }
    }
    if (request->has_volume_density_scale_override) {
        if (request->volume_density_scale_override < 0.0) {
            json_object_put(root);
            agent_render_request_set_diagf(out_diagnostics,
                              out_diagnostics_size,
                              "request=%s field=inspection.volume_density_scale out of range value=%.6f",
                              request_path,
                              request->volume_density_scale_override);
            return false;
        }
        if (request->volume_density_scale_override > 128.0) {
            request->volume_density_scale_override = 128.0;
        }
    }
    if (request->has_volume_density_gamma_override) {
        if (request->volume_density_gamma_override <= 0.0) {
            json_object_put(root);
            agent_render_request_set_diagf(out_diagnostics,
                              out_diagnostics_size,
                              "request=%s field=inspection.volume_density_gamma out of range value=%.6f",
                              request_path,
                              request->volume_density_gamma_override);
            return false;
        }
        if (request->volume_density_gamma_override < 0.05) {
            request->volume_density_gamma_override = 0.05;
        }
        if (request->volume_density_gamma_override > 8.0) {
            request->volume_density_gamma_override = 8.0;
        }
    }
    if (request->has_volume_absorption_gain_override) {
        if (request->volume_absorption_gain_override < 0.0) {
            json_object_put(root);
            agent_render_request_set_diagf(out_diagnostics,
                              out_diagnostics_size,
                              "request=%s field=inspection.volume_absorption_gain out of range value=%.6f",
                              request_path,
                              request->volume_absorption_gain_override);
            return false;
        }
        if (request->volume_absorption_gain_override > 64.0) {
            request->volume_absorption_gain_override = 64.0;
        }
    }
    if (request->has_volume_opacity_clamp_override) {
        if (request->volume_opacity_clamp_override < 0.0) {
            json_object_put(root);
            agent_render_request_set_diagf(out_diagnostics,
                              out_diagnostics_size,
                              "request=%s field=inspection.volume_opacity_clamp out of range value=%.6f",
                              request_path,
                              request->volume_opacity_clamp_override);
            return false;
        }
        if (request->volume_opacity_clamp_override > 128.0) {
            request->volume_opacity_clamp_override = 128.0;
        }
    }
    if (request->has_volume_step_scale_override) {
        if (request->volume_step_scale_override <= 0.0) {
            json_object_put(root);
            agent_render_request_set_diagf(out_diagnostics,
                              out_diagnostics_size,
                              "request=%s field=inspection.volume_step_scale out of range value=%.6f",
                              request_path,
                              request->volume_step_scale_override);
            return false;
        }
        if (request->volume_step_scale_override < 0.05) {
            request->volume_step_scale_override = 0.05;
        }
        if (request->volume_step_scale_override > 4.0) {
            request->volume_step_scale_override = 4.0;
        }
    }
    if (request->has_secondary_diffuse_samples_3d_override) {
        request->secondary_diffuse_samples_3d_override =
            agent_render_request_clamp_secondary_diffuse_samples_3d_override(
                request->secondary_diffuse_samples_3d_override);
    }
    if (request->has_transmission_samples_3d_override) {
        request->transmission_samples_3d_override =
            agent_render_request_clamp_transmission_samples_3d_override(
                request->transmission_samples_3d_override);
    }
    if (request->integrator_3d != RAY_TRACING_3D_INTEGRATOR_DISNEY_V2 &&
        !request->has_caustic_mode_override &&
        !request->has_caustic_sidecar_enabled_override) {
        request->caustic_mode = RUNTIME_DISNEY_V2_CAUSTIC_MODE_OFF;
        request->caustic_settings.mode = RUNTIME_CAUSTIC_MODE_OFF;
        request->caustic_sidecar_enabled = false;
    }
    if (request->caustic_settings.mode == RUNTIME_CAUSTIC_MODE_TRANSPORT &&
        !request->caustic_settings.volumeCacheEnabled &&
        !request->caustic_settings.surfaceCacheEnabled) {
        json_object_put(root);
        agent_render_request_set_diagf(out_diagnostics,
                          out_diagnostics_size,
                          "request=%s field=inspection.caustic_mode transport requires inspection.caustic_volume_enabled=true or inspection.caustic_surface_enabled=true for bounded path emission",
                          request_path);
        return false;
    }
    if (request->caustic_settings.mode == RUNTIME_CAUSTIC_MODE_ANALYTIC &&
        request->integrator_3d != RAY_TRACING_3D_INTEGRATOR_DISNEY_V2) {
        json_object_put(root);
        agent_render_request_set_diagf(out_diagnostics,
                          out_diagnostics_size,
                          "request=%s field=inspection.caustic_mode requires render.integrator_3d=disney_v2",
                          request_path);
        return false;
    }
    if (request->caustic_settings.mode == RUNTIME_CAUSTIC_MODE_SPATIAL_CACHE) {
        if (request->has_caustic_sidecar_enabled_override &&
            request->caustic_sidecar_enabled &&
            request->integrator_3d == RAY_TRACING_3D_INTEGRATOR_DISNEY_V2) {
            request->caustic_mode = RUNTIME_DISNEY_V2_CAUSTIC_MODE_ANALYTIC;
        } else {
            request->caustic_mode = RUNTIME_DISNEY_V2_CAUSTIC_MODE_OFF;
            request->caustic_sidecar_enabled = false;
        }
    }
    if (request->caustic_settings.mode == RUNTIME_CAUSTIC_MODE_TRANSPORT) {
        if (request->has_caustic_sidecar_enabled_override &&
            request->caustic_sidecar_enabled &&
            request->integrator_3d == RAY_TRACING_3D_INTEGRATOR_DISNEY_V2) {
            request->caustic_mode = RUNTIME_DISNEY_V2_CAUSTIC_MODE_ANALYTIC;
        } else {
            request->caustic_mode = RUNTIME_DISNEY_V2_CAUSTIC_MODE_OFF;
            request->caustic_sidecar_enabled = false;
        }
    }
    if (request->caustic_settings.sampleBudget < 0) {
        request->caustic_settings.sampleBudget = 0;
    }
    if (request->caustic_settings.sampleBudget > 1000000) {
        request->caustic_settings.sampleBudget = 1000000;
    }
    if (request->caustic_settings.maxPathDepth < 0) {
        request->caustic_settings.maxPathDepth = 0;
    }
    if (request->caustic_settings.maxPathDepth > 16) {
        request->caustic_settings.maxPathDepth = 16;
    }
    if (request->caustic_settings.surfaceRadianceScale < 0.0) {
        request->caustic_settings.surfaceRadianceScale = 0.0;
    }
    if (request->caustic_settings.surfaceRadianceScale > 128.0) {
        request->caustic_settings.surfaceRadianceScale = 128.0;
    }
    if (request->caustic_settings.surfaceFootprintScale < 0.1) {
        request->caustic_settings.surfaceFootprintScale = 0.1;
    }
    if (request->caustic_settings.surfaceFootprintScale > 16.0) {
        request->caustic_settings.surfaceFootprintScale = 16.0;
    }
    if (request->has_caustic_sidecar_strength_override) {
        if (request->caustic_sidecar_strength < 0.0) request->caustic_sidecar_strength = 0.0;
        if (request->caustic_sidecar_strength > 16.0) request->caustic_sidecar_strength = 16.0;
    }
    if (request->has_volume_tint_override) {
        if (request->volume_tint_r < 0.0) request->volume_tint_r = 0.0;
        if (request->volume_tint_g < 0.0) request->volume_tint_g = 0.0;
        if (request->volume_tint_b < 0.0) request->volume_tint_b = 0.0;
        if (request->volume_tint_r > 8.0) request->volume_tint_r = 8.0;
        if (request->volume_tint_g > 8.0) request->volume_tint_g = 8.0;
        if (request->volume_tint_b > 8.0) request->volume_tint_b = 8.0;
        if (request->volume_tint_r <= 0.0 &&
            request->volume_tint_g <= 0.0 &&
            request->volume_tint_b <= 0.0) {
            json_object_put(root);
            agent_render_request_set_diagf(out_diagnostics,
                              out_diagnostics_size,
                              "request=%s field=inspection.volume_tint out of range r=%.6f g=%.6f b=%.6f",
                              request_path,
                              request->volume_tint_r,
                              request->volume_tint_g,
                              request->volume_tint_b);
            return false;
        }
    }
    if (request->has_volume_albedo_override) {
        if (request->volume_albedo_r < 0.0) request->volume_albedo_r = 0.0;
        if (request->volume_albedo_g < 0.0) request->volume_albedo_g = 0.0;
        if (request->volume_albedo_b < 0.0) request->volume_albedo_b = 0.0;
        if (request->volume_albedo_r > 8.0) request->volume_albedo_r = 8.0;
        if (request->volume_albedo_g > 8.0) request->volume_albedo_g = 8.0;
        if (request->volume_albedo_b > 8.0) request->volume_albedo_b = 8.0;
        if (request->volume_albedo_r <= 0.0 &&
            request->volume_albedo_g <= 0.0 &&
            request->volume_albedo_b <= 0.0) {
            json_object_put(root);
            agent_render_request_set_diagf(out_diagnostics,
                              out_diagnostics_size,
                              "request=%s field=inspection.volume_albedo out of range r=%.6f g=%.6f b=%.6f",
                              request_path,
                              request->volume_albedo_r,
                              request->volume_albedo_g,
                              request->volume_albedo_b);
            return false;
        }
    }
    if (request->object_audit_max_dimension < 16) {
        request->object_audit_max_dimension = 16;
    }
    if (request->object_audit_max_dimension > 2048) {
        request->object_audit_max_dimension = 2048;
    }

    if (RayTracingJsonGetBool(root, "overwrite", &bool_value)) {
        request->overwrite = bool_value;
    }

    return true;
}
