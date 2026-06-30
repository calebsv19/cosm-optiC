#include "tools/ray_tracing_render_headless_internal.h"

#include "app/ray_tracing_request_utils.h"
#include "render/runtime_disney_v2_caustic_sidecar_3d.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <json-c/json.h>

static const char *route_family_label(RayTracingRouteFamily family) {
    switch (family) {
        case RAY_TRACING_ROUTE_NATIVE_3D:
            return "native_3d";
        case RAY_TRACING_ROUTE_COMPAT_3D_FALLBACK:
            return "compat_3d_fallback";
        case RAY_TRACING_ROUTE_CANONICAL_2D:
        default:
            return "canonical_2d";
    }
}

static const char *environment_light_mode_label(EnvironmentLightMode mode) {
    switch (animation_config_environment_light_mode_clamp(mode)) {
        case ENVIRONMENT_LIGHT_MODE_TOP_FILL:
            return "top_fill";
        case ENVIRONMENT_LIGHT_MODE_AMBIENT:
            return "ambient";
        case ENVIRONMENT_LIGHT_MODE_OFF:
        default:
            return "off";
    }
}

void ray_tracing_render_headless_write_summary(
    FILE *file,
    const RayTracingAgentRenderRequest *request,
    const RayTracingHeadlessPreflight *preflight) {
    if (!file || !request || !preflight) return;
    fprintf(file, "{\n");
    fprintf(file, "  \"schema_version\": \"ray_tracing_headless_summary_v1\",\n");
    fprintf(file, "  \"run_id\": ");
    RayTracingJsonWriteString(file, request->run_id);
    fprintf(file, ",\n");
    fprintf(file, "  \"request_loaded\": %s,\n", preflight->request_loaded ? "true" : "false");
    fprintf(file, "  \"scene_applied\": %s,\n", preflight->scene_applied ? "true" : "false");
    fprintf(file, "  \"volume_attached\": %s,\n", preflight->volume_attached ? "true" : "false");
    fprintf(file, "  \"volume_summary_built\": %s,\n",
            preflight->volume_summary_built ? "true" : "false");
    fprintf(file, "  \"water_surface_source_found\": %s,\n",
            preflight->water_surface_source_found ? "true" : "false");
    fprintf(file, "  \"water_surface_loaded\": %s,\n",
            preflight->water_surface_loaded ? "true" : "false");
    fprintf(file, "  \"water_surface_mesh_attached\": %s,\n",
            preflight->water_surface_mesh_attached ? "true" : "false");
    fprintf(file, "  \"route_family\": ");
    RayTracingJsonWriteString(file, route_family_label(preflight->route.routeFamily));
    fprintf(file, ",\n");
    fprintf(file, "  \"route_native_3d\": %s,\n", preflight->route_native_3d ? "true" : "false");
    fprintf(file, "  \"prepared_frame\": %s,\n", preflight->prepared_frame ? "true" : "false");
    fprintf(file, "  \"rendered_frames\": %s,\n", preflight->rendered_frames ? "true" : "false");
    fprintf(file, "  \"frames_rendered\": %d,\n", preflight->frames_rendered);
    fprintf(file, "  \"runtime_scene_path\": ");
    RayTracingJsonWriteString(file, request->runtime_scene_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"volume_source_path\": ");
    RayTracingJsonWriteString(file, request->volume_source_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"volume_source_kind\": ");
    RayTracingJsonWriteString(file,
                      ray_tracing_agent_render_request_volume_kind_label(
                          request->volume_source_kind));
    fprintf(file, ",\n");
    fprintf(file, "  \"volume_visible\": %s,\n",
            request->volume_visible ? "true" : "false");
    fprintf(file, "  \"integrator_3d\": ");
    RayTracingJsonWriteString(file,
                      ray_tracing_agent_render_request_integrator_label(
                          request->integrator_3d));
    fprintf(file, ",\n");
    fprintf(file, "  \"render\": {\n");
    fprintf(file, "    \"start_frame\": %d,\n", request->start_frame);
    fprintf(file, "    \"frame_count\": %d,\n", request->frame_count);
    fprintf(file, "    \"width\": %d,\n", request->width);
    fprintf(file, "    \"height\": %d,\n", request->height);
    fprintf(file, "    \"normalized_t\": %.9f,\n", request->normalized_t);
    fprintf(file, "    \"temporal_frames\": %d,\n", request->temporal_frames);
    fprintf(file, "    \"has_denoise_enabled_override\": %s,\n",
            request->has_denoise_enabled_override ? "true" : "false");
    fprintf(file, "    \"denoise_enabled\": %s\n",
            preflight->denoise_enabled ? "true" : "false");
    fprintf(file, "  },\n");
    fprintf(file, "  \"resources\": {\n");
    fprintf(file, "    \"has_budget\": %s,\n",
            request->has_resource_budget ? "true" : "false");
    fprintf(file, "    \"cpu_percent\": %d,\n", request->resource_cpu_percent);
    fprintf(file, "    \"max_workers\": %d,\n", request->resource_max_workers);
    fprintf(file, "    \"reserve_cpu_count\": %d\n", request->resource_reserve_cpu_count);
    fprintf(file, "  },\n");
    fprintf(file, "  \"denoise\": {\n");
    fprintf(file, "    \"has_request_override\": %s,\n",
            request->has_denoise_enabled_override ? "true" : "false");
    fprintf(file, "    \"enabled\": %s,\n",
            preflight->denoise_enabled ? "true" : "false");
    fprintf(file, "    \"applied\": %s\n",
            preflight->stats.denoiseRawPixelCount > 0 ? "true" : "false");
    fprintf(file, "  },\n");
    fprintf(file, "  \"inspection\": {\n");
    fprintf(file, "    \"preset\": ");
    RayTracingJsonWriteString(file,
                      ray_tracing_agent_render_request_inspection_preset_label(
                          request->inspection_preset));
    fprintf(file, ",\n");
    fprintf(file, "    \"has_camera_zoom_override\": %s,\n",
            request->has_camera_zoom_override ? "true" : "false");
    fprintf(file, "    \"camera_zoom\": %.9f,\n", request->camera_zoom_override);
    fprintf(file, "    \"has_camera_position_override\": %s,\n",
            request->has_camera_position_override ? "true" : "false");
    fprintf(file, "    \"camera_position\": { \"x\": %.9f, \"y\": %.9f, \"z\": %.9f },\n",
            request->camera_position_x,
            request->camera_position_y,
            request->camera_position_z);
    fprintf(file, "    \"has_camera_look_at_override\": %s,\n",
            request->has_camera_look_at_override ? "true" : "false");
    fprintf(file, "    \"camera_look_at\": { \"x\": %.9f, \"y\": %.9f, \"z\": %.9f },\n",
            request->camera_look_at_x,
            request->camera_look_at_y,
            request->camera_look_at_z);
    fprintf(file, "    \"has_environment_brightness_override\": %s,\n",
            request->has_environment_brightness_override ? "true" : "false");
    fprintf(file, "    \"environment_brightness\": %.9f,\n",
            request->environment_brightness_override);
    fprintf(file, "    \"has_ambient_strength_override\": %s,\n",
            request->has_ambient_strength_override ? "true" : "false");
    fprintf(file, "    \"ambient_strength\": %.9f,\n", request->ambient_strength_override);
    fprintf(file, "    \"has_environment_light_mode_override\": %s,\n",
            request->has_environment_light_mode_override ? "true" : "false");
    fprintf(file, "    \"environment_light_mode\": ");
    RayTracingJsonWriteString(file, environment_light_mode_label(request->environment_light_mode_override));
    fprintf(file, ",\n");
    fprintf(file, "    \"has_environment_preset_override\": %s,\n",
            request->has_environment_preset_override ? "true" : "false");
    fprintf(file, "    \"environment_preset\": ");
    RayTracingJsonWriteString(file,
                      RuntimeEnvironment3DPresetLabel(
                          (EnvironmentPreset)request->environment_preset_override));
    fprintf(file, ",\n");
    fprintf(file, "    \"has_background_brightness_override\": %s,\n",
            request->has_background_brightness_override ? "true" : "false");
    fprintf(file, "    \"background_brightness\": %.9f,\n",
            request->background_brightness_override);
    fprintf(file, "    \"has_background_color_override\": %s,\n",
            request->has_background_color_override ? "true" : "false");
    fprintf(file, "    \"background_color\": { \"r\": %.9f, \"g\": %.9f, \"b\": %.9f },\n",
            request->background_color_r,
            request->background_color_g,
            request->background_color_b);
    fprintf(file, "    \"has_top_fill_strength_override\": %s,\n",
            request->has_top_fill_strength_override ? "true" : "false");
    fprintf(file, "    \"top_fill_strength\": %.9f,\n", request->top_fill_strength_override);
    fprintf(file, "    \"has_light_intensity_override\": %s,\n",
            request->has_light_intensity_override ? "true" : "false");
    fprintf(file, "    \"light_intensity\": %.9f,\n", request->light_intensity_override);
    fprintf(file, "    \"has_light_radius_override\": %s,\n",
            request->has_light_radius_override ? "true" : "false");
    fprintf(file, "    \"light_radius\": %.9f,\n", request->light_radius_override);
    fprintf(file, "    \"has_forward_decay_override\": %s,\n",
            request->has_forward_decay_override ? "true" : "false");
    fprintf(file, "    \"forward_decay\": %.9f,\n", request->forward_decay_override);
    fprintf(file, "    \"has_volume_scatter_gain_override\": %s,\n",
            request->has_volume_scatter_gain_override ? "true" : "false");
    fprintf(file, "    \"volume_scatter_gain\": %.9f,\n",
            request->volume_scatter_gain_override);
    fprintf(file, "    \"has_volume_density_scale_override\": %s,\n",
            request->has_volume_density_scale_override ? "true" : "false");
    fprintf(file, "    \"volume_density_scale\": %.9f,\n",
            request->volume_density_scale_override);
    fprintf(file, "    \"has_volume_density_gamma_override\": %s,\n",
            request->has_volume_density_gamma_override ? "true" : "false");
    fprintf(file, "    \"volume_density_gamma\": %.9f,\n",
            request->volume_density_gamma_override);
    fprintf(file, "    \"has_volume_absorption_gain_override\": %s,\n",
            request->has_volume_absorption_gain_override ? "true" : "false");
    fprintf(file, "    \"volume_absorption_gain\": %.9f,\n",
            request->volume_absorption_gain_override);
    fprintf(file, "    \"has_volume_opacity_clamp_override\": %s,\n",
            request->has_volume_opacity_clamp_override ? "true" : "false");
    fprintf(file, "    \"volume_opacity_clamp\": %.9f,\n",
            request->volume_opacity_clamp_override);
    fprintf(file, "    \"has_volume_step_scale_override\": %s,\n",
            request->has_volume_step_scale_override ? "true" : "false");
    fprintf(file, "    \"volume_step_scale\": %.9f,\n",
            request->volume_step_scale_override);
    fprintf(file, "    \"has_secondary_diffuse_samples_3d_override\": %s,\n",
            request->has_secondary_diffuse_samples_3d_override ? "true" : "false");
    fprintf(file, "    \"secondary_diffuse_samples_3d\": %d,\n",
            request->secondary_diffuse_samples_3d_override);
    fprintf(file, "    \"has_transmission_samples_3d_override\": %s,\n",
            request->has_transmission_samples_3d_override ? "true" : "false");
    fprintf(file, "    \"transmission_samples_3d\": %d,\n",
            request->transmission_samples_3d_override);
    fprintf(file, "    \"has_caustic_mode_override\": %s,\n",
            request->has_caustic_mode_override ? "true" : "false");
    fprintf(file, "    \"caustic_mode\": \"%s\",\n",
            RuntimeDisneyV2_3D_CausticModeLabel(request->caustic_mode));
    fprintf(file, "    \"has_caustic_sidecar_enabled_override\": %s,\n",
            request->has_caustic_sidecar_enabled_override ? "true" : "false");
    fprintf(file, "    \"caustic_sidecar_enabled\": %s,\n",
            request->caustic_sidecar_enabled ? "true" : "false");
    fprintf(file, "    \"has_caustic_sidecar_strength_override\": %s,\n",
            request->has_caustic_sidecar_strength_override ? "true" : "false");
    fprintf(file, "    \"caustic_sidecar_strength\": %.9f,\n",
            request->caustic_sidecar_strength);
    fprintf(file, "    \"has_volume_tint_override\": %s,\n",
            request->has_volume_tint_override ? "true" : "false");
    fprintf(file, "    \"volume_tint\": { \"r\": %.9f, \"g\": %.9f, \"b\": %.9f },\n",
            request->volume_tint_r,
            request->volume_tint_g,
            request->volume_tint_b);
    fprintf(file, "    \"has_volume_albedo_override\": %s,\n",
            request->has_volume_albedo_override ? "true" : "false");
    fprintf(file, "    \"volume_albedo\": { \"r\": %.9f, \"g\": %.9f, \"b\": %.9f }\n",
            request->volume_albedo_r,
            request->volume_albedo_g,
            request->volume_albedo_b);
    fprintf(file, "  },\n");
    {
        const RuntimeEnvironment3D *environment = &preflight->environment_summary;
        const double ambient_strength =
            RuntimeEnvironment3D_AmbientStrength(environment);
        const double background_brightness =
            RuntimeEnvironment3D_BackgroundBrightness(environment);
        const bool ambient_contributes =
            preflight->environment_summary_built &&
            environment->lightMode == ENVIRONMENT_LIGHT_MODE_AMBIENT &&
            ambient_strength > 0.0;
        const bool background_contributes =
            preflight->environment_summary_built &&
            environment->lightMode == ENVIRONMENT_LIGHT_MODE_AMBIENT &&
            background_brightness > 0.0;
        const bool top_fill_contributes =
            preflight->environment_summary_built &&
            environment->lightMode == ENVIRONMENT_LIGHT_MODE_TOP_FILL &&
            environment->topFillIntensity > 0.0;
        fprintf(file, "  \"environment_lighting\": {\n");
        fprintf(file, "    \"built\": %s,\n",
                preflight->environment_summary_built ? "true" : "false");
        fprintf(file, "    \"mode\": ");
        RayTracingJsonWriteString(file, environment_light_mode_label(environment->lightMode));
        fprintf(file, ",\n");
        fprintf(file, "    \"preset\": ");
        RayTracingJsonWriteString(file, RuntimeEnvironment3DPresetLabel(environment->preset));
        fprintf(file, ",\n");
        fprintf(file, "    \"ambient_strength\": %.9f,\n", ambient_strength);
        fprintf(file, "    \"ambient_color\": [%.9f, %.9f, %.9f],\n",
                environment->ambientColor.x,
                environment->ambientColor.y,
                environment->ambientColor.z);
        fprintf(file, "    \"background_brightness\": %.9f,\n", background_brightness);
        fprintf(file, "    \"background_brightness_source\": ");
        RayTracingJsonWriteString(file,
                          environment->backgroundIntensityDerivedFromAmbient
                              ? "ambient_strength_compat"
                              : "background_brightness");
        fprintf(file, ",\n");
        fprintf(file, "    \"background_color\": [%.9f, %.9f, %.9f],\n",
                environment->backgroundColor.x,
                environment->backgroundColor.y,
                environment->backgroundColor.z);
        fprintf(file, "    \"background_top_color\": [%.9f, %.9f, %.9f],\n",
                environment->backgroundTopColor.x,
                environment->backgroundTopColor.y,
                environment->backgroundTopColor.z);
        fprintf(file, "    \"background_bottom_color\": [%.9f, %.9f, %.9f],\n",
                environment->backgroundBottomColor.x,
                environment->backgroundBottomColor.y,
                environment->backgroundBottomColor.z);
        fprintf(file, "    \"top_fill_strength\": %.9f,\n",
                environment->topFillIntensity);
        fprintf(file, "    \"ambient_surface_fill_contributes\": %s,\n",
                ambient_contributes ? "true" : "false");
        fprintf(file, "    \"background_miss_contributes\": %s,\n",
                background_contributes ? "true" : "false");
        fprintf(file, "    \"top_fill_contributes\": %s,\n",
                top_fill_contributes ? "true" : "false");
        fprintf(file, "    \"authored_direct_light_count\": %d\n",
                preflight->scene_summary.light_count);
        fprintf(file, "  },\n");
    }
    fprintf(file, "  \"registered_lights\": {\n");
    fprintf(file, "    \"light_count\": %d,\n", preflight->registered_light_count);
    fprintf(file, "    \"enabled_count\": %d,\n", preflight->registered_enabled_light_count);
    fprintf(file, "    \"shape_counts\": { \"point\": %d, \"sphere\": %d, \"disk\": %d, \"rect\": %d, \"mesh_emissive\": %d },\n",
            preflight->registered_light_point_count,
            preflight->registered_light_sphere_count,
            preflight->registered_light_disk_count,
            preflight->registered_light_rect_count,
            preflight->registered_light_mesh_emissive_count);
    fprintf(file, "    \"source_counts\": { \"authored\": %d, \"compatibility\": %d, \"material_emitter\": %d },\n",
            preflight->registered_light_authored_count,
            preflight->registered_light_compatibility_count,
            preflight->registered_light_material_emitter_count);
    fprintf(file, "    \"material_emitter_enabled_count\": %d,\n",
            preflight->registered_light_material_emitter_enabled_count);
    fprintf(file, "    \"mesh_area_sampler_only_count\": %d,\n",
            preflight->registered_light_mesh_area_sampler_only_count);
    fprintf(file, "    \"emissive_candidate_count\": %d,\n",
            preflight->registered_light_emissive_candidate_count);
    fprintf(file, "    \"emissive_area\": %.9f,\n",
            preflight->registered_light_emissive_area);
    fprintf(file, "    \"emissive_weight\": %.9f,\n",
            preflight->registered_light_emissive_weight);
    fprintf(file, "    \"emissive_proxy_radius_max\": %.9f,\n",
            preflight->registered_light_emissive_proxy_radius_max);
    fprintf(file, "    \"first_color\": [%.9f, %.9f, %.9f]\n",
            preflight->registered_light_first_color_r,
            preflight->registered_light_first_color_g,
            preflight->registered_light_first_color_b);
    fprintf(file, "  },\n");
    fprintf(file, "  \"object_audit_summary\": {\n");
    fprintf(file, "    \"enabled\": %s,\n", preflight->object_audit_enabled ? "true" : "false");
    fprintf(file, "    \"requested_max_dimension\": %d,\n", request->object_audit_max_dimension);
    fprintf(file, "    \"width\": %d,\n", preflight->object_audit_width);
    fprintf(file, "    \"height\": %d,\n", preflight->object_audit_height);
    fprintf(file, "    \"stride_x\": %d,\n", preflight->object_audit_stride_x);
    fprintf(file, "    \"stride_y\": %d,\n", preflight->object_audit_stride_y);
    fprintf(file, "    \"scale_factor\": %d,\n", preflight->object_audit_scale_factor);
    fprintf(file, "    \"sample_count\": %d,\n", preflight->object_audit_sample_count);
    fprintf(file, "    \"full_resolution_pixel_count\": %llu\n",
            (unsigned long long)((request->width > 0 && request->height > 0)
                                     ? ((unsigned long long)request->width *
                                        (unsigned long long)request->height)
                                     : 0ull));
    fprintf(file, "  },\n");
    fprintf(file, "  \"outputs\": {\n");
    fprintf(file, "    \"root\": ");
    RayTracingJsonWriteString(file, request->output_root);
    fprintf(file, ",\n");
    fprintf(file, "    \"frame_dir\": ");
    RayTracingJsonWriteString(file, preflight->frame_dir);
    fprintf(file, ",\n");
    fprintf(file, "    \"first_frame_path\": ");
    RayTracingJsonWriteString(file, preflight->first_frame_path);
    fprintf(file, ",\n");
    fprintf(file, "    \"last_frame_path\": ");
    RayTracingJsonWriteString(file, preflight->last_frame_path);
    fprintf(file, ",\n");
    fprintf(file, "    \"video_enabled\": %s,\n", request->video_enabled ? "true" : "false");
    fprintf(file, "    \"video_path\": ");
    RayTracingJsonWriteString(file, request->video_path);
    fprintf(file, ",\n");
    fprintf(file, "    \"video_fps\": %d", request->video_fps);
    fprintf(file, "\n");
    fprintf(file, "  },\n");
    fprintf(file, "  \"scene_summary\": {\n");
    fprintf(file, "    \"valid_contract\": %s,\n",
            preflight->scene_summary.valid_contract ? "true" : "false");
    fprintf(file, "    \"scene_id\": ");
    RayTracingJsonWriteString(file, preflight->scene_summary.scene_id);
    fprintf(file, ",\n");
    fprintf(file, "    \"object_count\": %d,\n", preflight->scene_summary.object_count);
    fprintf(file, "    \"material_count\": %d,\n", preflight->scene_summary.material_count);
    fprintf(file, "    \"light_count\": %d,\n", preflight->scene_summary.light_count);
    fprintf(file, "    \"camera_count\": %d\n", preflight->scene_summary.camera_count);
    fprintf(file, "  },\n");
    fprintf(file, "  \"volume_summary\": {\n");
    fprintf(file, "    \"enabled\": %s,\n", preflight->volume_summary.enabled ? "true" : "false");
    fprintf(file, "    \"debug_overlay_enabled\": %s,\n",
            preflight->volume_summary.debugOverlayEnabled ? "true" : "false");
    fprintf(file, "    \"has_data\": %s,\n", preflight->volume_summary.hasData ? "true" : "false");
    fprintf(file, "    \"layout_valid\": %s,\n",
            preflight->volume_summary.layoutValid ? "true" : "false");
    fprintf(file, "    \"has_density\": %s,\n",
            preflight->volume_summary.hasDensity ? "true" : "false");
    fprintf(file, "    \"has_velocity\": %s,\n",
            preflight->volume_summary.hasVelocity ? "true" : "false");
    fprintf(file, "    \"has_pressure\": %s,\n",
            preflight->volume_summary.hasPressure ? "true" : "false");
    fprintf(file, "    \"has_solid_mask\": %s,\n",
            preflight->volume_summary.hasSolidMask ? "true" : "false");
    fprintf(file, "    \"grid_w\": %u,\n", preflight->volume_summary.gridW);
    fprintf(file, "    \"grid_h\": %u,\n", preflight->volume_summary.gridH);
    fprintf(file, "    \"grid_d\": %u,\n", preflight->volume_summary.gridD);
    fprintf(file, "    \"cell_count\": %llu,\n",
            (unsigned long long)preflight->volume_summary.cellCount);
    fprintf(file, "    \"density_non_zero_cell_count\": %llu,\n",
            (unsigned long long)preflight->volume_summary.densityNonZeroCellCount);
    fprintf(file, "    \"density_min\": %.9f,\n", preflight->volume_summary.densityMin);
    fprintf(file, "    \"density_max\": %.9f\n", preflight->volume_summary.densityMax);
    fprintf(file, "  },\n");
    fprintf(file, "  \"volume_frame_selection\": {\n");
    fprintf(file, "    \"built\": %s,\n",
            preflight->volume_frame_selection_built ? "true" : "false");
    fprintf(file, "    \"dynamic\": %s,\n",
            preflight->volume_frame_selection_dynamic ? "true" : "false");
    fprintf(file, "    \"requested_first_frame_index\": %d,\n",
            preflight->volume_requested_first_frame_index);
    fprintf(file, "    \"requested_last_frame_index\": %d,\n",
            preflight->volume_requested_last_frame_index);
    fprintf(file, "    \"loaded_first_frame_index\": %llu,\n",
            (unsigned long long)preflight->volume_loaded_first_frame_index);
    fprintf(file, "    \"loaded_last_frame_index\": %llu,\n",
            (unsigned long long)preflight->volume_loaded_last_frame_index);
    fprintf(file, "    \"selected_first_frame_path\": ");
    RayTracingJsonWriteString(file, preflight->volume_selected_first_frame_path);
    fprintf(file, ",\n");
    fprintf(file, "    \"selected_last_frame_path\": ");
    RayTracingJsonWriteString(file, preflight->volume_selected_last_frame_path);
    fprintf(file, "\n");
    fprintf(file, "  },\n");
    fprintf(file, "  \"water_surface\": {\n");
    fprintf(file, "    \"source_found\": %s,\n",
            preflight->water_surface_source_found ? "true" : "false");
    fprintf(file, "    \"loaded\": %s,\n",
            preflight->water_surface_loaded ? "true" : "false");
    fprintf(file, "    \"frame_selection_built\": %s,\n",
            preflight->water_surface_frame_selection_built ? "true" : "false");
    fprintf(file, "    \"dynamic\": %s,\n",
            preflight->water_surface_frame_selection_dynamic ? "true" : "false");
    fprintf(file, "    \"mesh_attached\": %s,\n",
            preflight->water_surface_mesh_attached ? "true" : "false");
    fprintf(file, "    \"triangle_count\": %d,\n",
            preflight->water_surface_triangle_count);
    fprintf(file, "    \"requested_first_frame_index\": %d,\n",
            preflight->water_surface_requested_first_frame_index);
    fprintf(file, "    \"requested_last_frame_index\": %d,\n",
            preflight->water_surface_requested_last_frame_index);
    fprintf(file, "    \"loaded_first_frame_index\": %llu,\n",
            (unsigned long long)preflight->water_surface_loaded_first_frame_index);
    fprintf(file, "    \"loaded_last_frame_index\": %llu,\n",
            (unsigned long long)preflight->water_surface_loaded_last_frame_index);
    fprintf(file, "    \"manifest_path\": ");
    RayTracingJsonWriteString(file, preflight->water_surface_manifest_path);
    fprintf(file, ",\n");
    fprintf(file, "    \"selected_first_frame_path\": ");
    RayTracingJsonWriteString(file, preflight->water_surface_selected_first_frame_path);
    fprintf(file, ",\n");
    fprintf(file, "    \"selected_last_frame_path\": ");
    RayTracingJsonWriteString(file, preflight->water_surface_selected_last_frame_path);
    fprintf(file, ",\n");
    fprintf(file, "    \"surface_axis\": ");
    RayTracingJsonWriteString(file, preflight->water_surface_axis);
    fprintf(file, ",\n");
    fprintf(file, "    \"grid_w\": %u,\n", preflight->water_surface_grid_w);
    fprintf(file, "    \"grid_d\": %u,\n", preflight->water_surface_grid_d);
    fprintf(file, "    \"sample_count\": %llu,\n",
            (unsigned long long)preflight->water_surface_sample_count);
    fprintf(file, "    \"wet_columns\": %u,\n", preflight->water_surface_wet_columns);
    fprintf(file, "    \"dry_columns\": %u,\n", preflight->water_surface_dry_columns);
    fprintf(file, "    \"solid_columns\": %u,\n", preflight->water_surface_solid_columns);
    fprintf(file, "    \"water_cells\": %u,\n", preflight->water_surface_water_cells);
    fprintf(file, "    \"surface_min_y\": %.9f,\n", preflight->water_surface_min_y);
    fprintf(file, "    \"surface_max_y\": %.9f,\n", preflight->water_surface_max_y);
    fprintf(file, "    \"surface_avg_y\": %.9f,\n", preflight->water_surface_avg_y);
    fprintf(file, "    \"max_slope\": %.9f,\n", preflight->water_surface_max_slope);
    fprintf(file, "    \"finite_normals\": %s,\n",
            preflight->water_surface_finite_normals ? "true" : "false");
    fprintf(file, "    \"material\": {\n");
    fprintf(file, "      \"ior\": %.9f,\n", preflight->water_surface_material_ior);
    fprintf(file, "      \"absorption_distance_m\": %.9f,\n",
            preflight->water_surface_absorption_distance_m);
    fprintf(file, "      \"absorption_rgb\": [%.9f, %.9f, %.9f],\n",
            preflight->water_surface_absorption_r,
            preflight->water_surface_absorption_g,
            preflight->water_surface_absorption_b);
    fprintf(file, "      \"reflectivity\": %.9f,\n",
            preflight->water_surface_material_reflectivity);
    fprintf(file, "      \"roughness\": %.9f\n",
            preflight->water_surface_material_roughness);
    fprintf(file, "    },\n");
    fprintf(file, "    \"payload\": {\n");
    fprintf(file, "      \"applied\": %s,\n",
            preflight->water_surface_material_payload_applied ? "true" : "false");
    fprintf(file, "      \"ior\": %.9f,\n", preflight->water_surface_payload_ior);
    fprintf(file, "      \"absorption_distance_m\": %.9f,\n",
            preflight->water_surface_payload_absorption_distance_m);
    fprintf(file, "      \"transparency\": %.9f,\n",
            preflight->water_surface_payload_transparency);
    fprintf(file, "      \"reflectivity\": %.9f,\n",
            preflight->water_surface_payload_reflectivity);
    fprintf(file, "      \"roughness\": %.9f,\n",
            preflight->water_surface_payload_roughness);
    fprintf(file, "      \"tint_rgb\": [%.9f, %.9f, %.9f]\n",
            preflight->water_surface_payload_tint_r,
            preflight->water_surface_payload_tint_g,
            preflight->water_surface_payload_tint_b);
    fprintf(file, "    }\n");
    fprintf(file, "  },\n");
    fprintf(file, "  \"render_stats\": {\n");
    fprintf(file, "    \"hit_pixels\": %d,\n", preflight->stats.hitPixelCount);
    fprintf(file, "    \"visible_pixels\": %d,\n", preflight->stats.visiblePixelCount);
    fprintf(file, "    \"secondary_rays\": %d,\n", preflight->stats.secondaryRayCount);
    fprintf(file, "    \"secondary_hits\": %d,\n", preflight->stats.secondaryHitCount);
    fprintf(file, "    \"emissive_area_candidate_count\": %d,\n",
            preflight->stats.emissiveAreaCandidateCount);
    fprintf(file, "    \"emissive_area_selected_candidates\": %d,\n",
            preflight->stats.emissiveAreaSelectedCandidateCount);
    fprintf(file, "    \"emissive_area_visibility_rays\": %d,\n",
            preflight->stats.emissiveAreaVisibilityRayCount);
    fprintf(file, "    \"emissive_area_primary_samples\": %d,\n",
            preflight->stats.emissiveAreaPrimarySampleCount);
    fprintf(file, "    \"emissive_area_recursive_samples\": %d,\n",
            preflight->stats.emissiveAreaRecursiveSampleCount);
    fprintf(file, "    \"emissive_area_recursive_policy_skips\": %d,\n",
            preflight->stats.emissiveAreaRecursivePolicySkipCount);
    fprintf(file, "    \"emissive_area_recursive_candidate_cap_skips\": %d,\n",
            preflight->stats.emissiveAreaRecursiveCandidateCapSkipCount);
    fprintf(file, "    \"emissive_area_recursive_triangle_cap_skips\": %d,\n",
            preflight->stats.emissiveAreaRecursiveTriangleCapSkipCount);
    fprintf(file, "    \"emissive_area_recursive_candidate_cap\": %d,\n",
            preflight->stats.emissiveAreaRecursiveCandidateCap);
    fprintf(file, "    \"emissive_area_recursive_triangle_cap\": %d,\n",
            preflight->stats.emissiveAreaRecursiveTriangleCap);
    fprintf(file, "    \"emissive_area_full_scan_fallbacks\": %d,\n",
            preflight->stats.emissiveAreaFullScanFallbackCount);
    fprintf(file, "    \"caustic_sidecar_enabled\": %s,\n",
            preflight->stats.causticSidecarEnabled > 0 ? "true" : "false");
    fprintf(file, "    \"caustic_sidecar_samples\": %d,\n",
            preflight->stats.causticSidecarSampleCount);
    fprintf(file, "    \"caustic_sidecar_contributing_samples\": %d,\n",
            preflight->stats.causticSidecarContributingSampleCount);
    fprintf(file, "    \"max_caustic_sidecar_radiance\": %.9f,\n",
            preflight->stats.maxCausticSidecarRadiance);
    fprintf(file, "    \"total_caustic_sidecar_radiance\": %.9f,\n",
            preflight->stats.totalCausticSidecarRadiance);
    fprintf(file, "    \"mirror_dominant_pixels\": %d,\n",
            preflight->stats.mirrorDominantPixelCount);
    fprintf(file, "    \"mirror_base_attenuated_pixels\": %d,\n",
            preflight->stats.mirrorBaseAttenuatedPixelCount);
    fprintf(file, "    \"mirror_reflection_hit_pixels\": %d,\n",
            preflight->stats.mirrorReflectionHitPixelCount);
    fprintf(file, "    \"mirror_emitter_reflection_pixels\": %d,\n",
            preflight->stats.mirrorEmitterReflectionPixelCount);
    fprintf(file, "    \"mirror_geometry_reflection_pixels\": %d,\n",
            preflight->stats.mirrorGeometryReflectionPixelCount);
    fprintf(file, "    \"max_mirror_dominance\": %.9f,\n",
            preflight->stats.maxMirrorDominance);
    fprintf(file, "    \"max_mirror_specular_reflection_radiance\": %.9f,\n",
            preflight->stats.maxMirrorSpecularReflectionRadiance);
    fprintf(file, "    \"max_mirror_base_radiance_before_attenuation\": %.9f,\n",
            preflight->stats.maxMirrorBaseRadianceBeforeAttenuation);
    fprintf(file, "    \"max_mirror_base_radiance_after_attenuation\": %.9f,\n",
            preflight->stats.maxMirrorBaseRadianceAfterAttenuation);
    fprintf(file, "    \"total_mirror_specular_reflection_radiance\": %.9f,\n",
            preflight->stats.totalMirrorSpecularReflectionRadiance);
    fprintf(file, "    \"total_mirror_base_radiance_before_attenuation\": %.9f,\n",
            preflight->stats.totalMirrorBaseRadianceBeforeAttenuation);
    fprintf(file, "    \"total_mirror_base_radiance_after_attenuation\": %.9f,\n",
            preflight->stats.totalMirrorBaseRadianceAfterAttenuation);
    fprintf(file, "    \"temporal_committed_subpasses\": %d,\n",
            preflight->stats.temporalCommittedSubpasses);
    fprintf(file, "    \"temporal_pixels_rendered\": %d,\n",
            preflight->stats.temporalPixelsRendered);
    fprintf(file, "    \"temporal_pixels_skipped\": %d,\n",
            preflight->stats.temporalPixelsSkipped);
    fprintf(file, "    \"temporal_active_pixels\": %d,\n",
            preflight->stats.temporalActivePixelCount);
    fprintf(file, "    \"temporal_active_tiles\": %d,\n",
            preflight->stats.temporalActiveTileCount);
    fprintf(file, "    \"temporal_inactive_tiles\": %d,\n",
            preflight->stats.temporalInactiveTileCount);
    fprintf(file, "    \"denoise_temporal_frame_count\": %d,\n",
            preflight->stats.denoiseTemporalFrameCount);
    fprintf(file, "    \"denoise_raw_pixel_count\": %d,\n",
            preflight->stats.denoiseRawPixelCount);
    fprintf(file, "    \"denoise_reconstructed_pixel_count\": %d,\n",
            preflight->stats.denoiseReconstructedPixelCount);
    fprintf(file, "    \"denoise_stable_interior_sample_count\": %d,\n",
            preflight->stats.denoiseStableInteriorSampleCount);
    fprintf(file, "    \"denoise_rejected_edge_sample_count\": %d,\n",
            preflight->stats.denoiseRejectedEdgeSampleCount);
    fprintf(file, "    \"denoise_preserved_transparent_pixel_count\": %d,\n",
            preflight->stats.denoisePreservedTransparentPixelCount);
    fprintf(file, "    \"denoise_preserved_mirror_glossy_pixel_count\": %d,\n",
            preflight->stats.denoisePreservedMirrorGlossyPixelCount);
    fprintf(file, "    \"denoise_skipped_unstable_temporal_pixel_count\": %d,\n",
            preflight->stats.denoiseSkippedUnstableTemporalPixelCount);
    fprintf(file, "    \"denoise_skipped_invalid_surface_pixel_count\": %d,\n",
            preflight->stats.denoiseSkippedInvalidSurfacePixelCount);
    fprintf(file, "    \"denoise_raw_radiance_luma_total\": %.9f,\n",
            preflight->stats.denoiseRawRadianceLumaTotal);
    fprintf(file, "    \"denoise_reconstructed_radiance_luma_total\": %.9f,\n",
            preflight->stats.denoiseReconstructedRadianceLumaTotal);
    fprintf(file, "    \"denoise_radiance_luma_delta\": %.9f,\n",
            preflight->stats.denoiseReconstructedRadianceLumaTotal -
                preflight->stats.denoiseRawRadianceLumaTotal);
    fprintf(file, "    \"max_radiance\": %.9f,\n", preflight->stats.maxRadiance);
    fprintf(file, "    \"max_bounce_radiance\": %.9f,\n",
            preflight->stats.maxBounceRadiance);
    fprintf(file, "    \"total_bounce_radiance\": %.9f,\n",
            preflight->stats.totalBounceRadiance);
    fprintf(file, "    \"nonzero_pixels\": %llu,\n",
            (unsigned long long)preflight->nonzero_pixels);
    fprintf(file, "    \"max_rgb\": [%u, %u, %u]\n",
            (unsigned)preflight->max_r,
            (unsigned)preflight->max_g,
            (unsigned)preflight->max_b);
    fprintf(file, "  },\n");
    fprintf(file, "  \"timing_breakdown\": {\n");
    fprintf(file, "    \"runtime_scene_apply_ms\": %.6f,\n",
            preflight->runtime_scene_apply_ms);
    fprintf(file, "    \"runtime_scene_preflight_ms\": %.6f,\n",
            preflight->runtime_scene_preflight_ms);
    fprintf(file, "    \"native_prepare_frame_ms\": %.6f,\n",
            preflight->native_prepare_frame_ms);
    fprintf(file, "    \"object_audit_ms\": %.6f,\n", preflight->object_audit_ms);
    fprintf(file, "    \"render_frames_ms\": %.6f,\n", preflight->render_frames_ms);
    fprintf(file, "    \"render_trace_ms\": %.6f,\n", preflight->render_trace_ms);
    fprintf(file, "    \"frame_analysis_ms\": %.6f,\n", preflight->frame_analysis_ms);
    fprintf(file, "    \"frame_write_ms\": %.6f,\n", preflight->frame_write_ms);
    fprintf(file, "    \"video_encode_ms\": %.6f,\n", preflight->video_encode_ms);
    fprintf(file, "    \"total_run_ms\": %.6f,\n", preflight->total_run_ms);
    fprintf(file, "    \"mesh_asset_loader\": {\n");
    fprintf(file, "      \"total_ms\": %.6f,\n",
            preflight->mesh_asset_timing_stats.total_ms);
    fprintf(file, "      \"scene_read_ms\": %.6f,\n",
            preflight->mesh_asset_timing_stats.scene_read_ms);
    fprintf(file, "      \"scene_parse_ms\": %.6f,\n",
            preflight->mesh_asset_timing_stats.scene_parse_ms);
    fprintf(file, "      \"asset_load_total_ms\": %.6f,\n",
            preflight->mesh_asset_timing_stats.asset_load_total_ms);
    fprintf(file, "      \"asset_runtime_document_load_ms\": %.6f,\n",
            preflight->mesh_asset_timing_stats.asset_runtime_document_load_ms);
    fprintf(file, "      \"asset_document_copy_ms\": %.6f,\n",
            preflight->mesh_asset_timing_stats.asset_document_copy_ms);
    fprintf(file, "      \"asset_load_calls\": %d,\n",
            preflight->mesh_asset_timing_stats.asset_load_calls);
    fprintf(file, "      \"asset_cache_hits\": %d,\n",
            preflight->mesh_asset_timing_stats.asset_cache_hits);
    fprintf(file, "      \"asset_cache_misses\": %d,\n",
            preflight->mesh_asset_timing_stats.asset_cache_misses);
    fprintf(file, "      \"loaded_assets\": %d,\n",
            preflight->mesh_asset_timing_stats.loaded_assets);
    fprintf(file, "      \"loaded_instances\": %d,\n",
            preflight->mesh_asset_timing_stats.loaded_instances);
    fprintf(file, "      \"loaded_asset_bytes\": %llu,\n",
            preflight->mesh_asset_timing_stats.loaded_asset_bytes);
    fprintf(file, "      \"loaded_vertices\": %llu,\n",
            preflight->mesh_asset_timing_stats.loaded_vertices);
    fprintf(file, "      \"loaded_triangles\": %llu\n",
            preflight->mesh_asset_timing_stats.loaded_triangles);
    fprintf(file, "    },\n");
    fprintf(file, "    \"scene_builder\": {\n");
    fprintf(file, "      \"total_ms\": %.6f,\n",
            preflight->scene_builder_timing_stats.total_ms);
    fprintf(file, "      \"primitive_seed_ms\": %.6f,\n",
            preflight->scene_builder_timing_stats.primitive_seed_ms);
    fprintf(file, "      \"mesh_append_total_ms\": %.6f,\n",
            preflight->scene_builder_timing_stats.mesh_append_total_ms);
    fprintf(file, "      \"mesh_append_reserve_ms\": %.6f,\n",
            preflight->scene_builder_timing_stats.mesh_append_reserve_ms);
    fprintf(file, "      \"mesh_append_expand_ms\": %.6f,\n",
            preflight->scene_builder_timing_stats.mesh_append_expand_ms);
    fprintf(file, "      \"bvh_rebuild_wall_ms\": %.6f,\n",
            preflight->scene_builder_timing_stats.bvh_rebuild_wall_ms);
    fprintf(file, "      \"mesh_append_calls\": %d,\n",
            preflight->scene_builder_timing_stats.mesh_append_calls);
    fprintf(file, "      \"mesh_append_assets\": %d,\n",
            preflight->scene_builder_timing_stats.mesh_append_assets);
    fprintf(file, "      \"mesh_append_instances\": %d,\n",
            preflight->scene_builder_timing_stats.mesh_append_instances);
    fprintf(file, "      \"mesh_append_triangles_expected\": %d,\n",
            preflight->scene_builder_timing_stats.mesh_append_triangles_expected);
    fprintf(file, "      \"mesh_append_triangles_appended\": %d\n",
            preflight->scene_builder_timing_stats.mesh_append_triangles_appended);
    fprintf(file, "    }\n");
    fprintf(file, "  },\n");
    fprintf(file, "  \"prepared_scene_cache\": {\n");
    fprintf(file, "    \"valid\": %s,\n",
            preflight->prepared_scene_cache_stats.valid ? "true" : "false");
    fprintf(file, "    \"generation\": %llu,\n",
            (unsigned long long)preflight->prepared_scene_cache_stats.generation);
    fprintf(file, "    \"cached_generation\": %llu,\n",
            (unsigned long long)preflight->prepared_scene_cache_stats.cachedGeneration);
    fprintf(file, "    \"hits\": %llu,\n",
            (unsigned long long)preflight->prepared_scene_cache_stats.hits);
    fprintf(file, "    \"misses\": %llu,\n",
            (unsigned long long)preflight->prepared_scene_cache_stats.misses);
    fprintf(file, "    \"stores\": %llu,\n",
            (unsigned long long)preflight->prepared_scene_cache_stats.stores);
    fprintf(file, "    \"invalidations\": %llu,\n",
            (unsigned long long)preflight->prepared_scene_cache_stats.invalidations);
    fprintf(file, "    \"static_geometry_reuse_enabled\": %s,\n",
            preflight->prepared_scene_cache_stats.staticGeometryReuseEnabled ? "true"
                                                                             : "false");
    fprintf(file, "    \"time_independent_hits\": %llu,\n",
            (unsigned long long)preflight->prepared_scene_cache_stats.timeIndependentHits);
    fprintf(file, "    \"cached_normalized_t\": %.9f,\n",
            preflight->prepared_scene_cache_stats.cachedNormalizedT);
    fprintf(file, "    \"last_requested_normalized_t\": %.9f,\n",
            preflight->prepared_scene_cache_stats.lastRequestedNormalizedT);
    fprintf(file, "    \"cached_primitive_count\": %d,\n",
            preflight->prepared_scene_cache_stats.cachedPrimitiveCount);
    fprintf(file, "    \"cached_triangle_count\": %d,\n",
            preflight->prepared_scene_cache_stats.cachedTriangleCount);
    fprintf(file, "    \"cached_bvh_node_count\": %d,\n",
            preflight->prepared_scene_cache_stats.cachedBVHNodeCount);
    fprintf(file, "    \"cached_bvh_leaf_count\": %d\n",
            preflight->prepared_scene_cache_stats.cachedBVHLeafCount);
    fprintf(file, "  },\n");
    fprintf(file, "  \"prepared_acceleration\": {\n");
    fprintf(file, "    \"enabled\": %s,\n",
            preflight->scene_acceleration_stats.enabled ? "true" : "false");
    fprintf(file, "    \"prepared_accel_reuse_status\": ");
    RayTracingJsonWriteString(
        file,
        RuntimeSceneAcceleration3DReuseStatusLabel(
            preflight->scene_acceleration_stats.reuseStatus));
    fprintf(file, ",\n");
    fprintf(file, "    \"blas_prepare_calls\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.blasPrepareCalls);
    fprintf(file, "    \"blas_cache_hits\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.blasCacheHits);
    fprintf(file, "    \"blas_cache_misses\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.blasCacheMisses);
    fprintf(file, "    \"blas_cache_invalidations\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.blasCacheInvalidations);
    fprintf(file, "    \"blas_full_rebuilds\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.blasFullRebuilds);
    fprintf(file, "    \"blas_cached_asset_count\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.blasCachedAssetCount);
    fprintf(file, "    \"tlas_node_count\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.tlasNodeCount);
    fprintf(file, "    \"tlas_instance_count\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.tlasInstanceCount);
    fprintf(file, "    \"tlas_rebuilds\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.tlasRebuilds);
    fprintf(file, "    \"tlas_refits\": %llu\n",
            (unsigned long long)preflight->scene_acceleration_stats.tlasRefits);
    fprintf(file, "  },\n");
    fprintf(file, "  \"bvh_summary\": {\n");
    fprintf(file, "    \"ready\": %s,\n",
            preflight->bvh_build_stats.ready ? "true" : "false");
    fprintf(file, "    \"triangle_count\": %d,\n",
            preflight->bvh_build_stats.triangleCount);
    fprintf(file, "    \"node_count\": %d,\n", preflight->bvh_build_stats.nodeCount);
    fprintf(file, "    \"leaf_count\": %d,\n", preflight->bvh_build_stats.leafCount);
    fprintf(file, "    \"max_depth\": %d,\n", preflight->bvh_build_stats.maxDepth);
    fprintf(file, "    \"leaf_size\": %d,\n", preflight->bvh_build_stats.leafSize);
    fprintf(file, "    \"max_leaf_triangle_count\": %d,\n",
            preflight->bvh_build_stats.maxLeafTriangleCount);
    fprintf(file, "    \"build_cpu_ms\": %.6f,\n",
            preflight->bvh_build_stats.buildCpuMs);
    fprintf(file, "    \"allocation_cpu_ms\": %.6f,\n",
            preflight->bvh_build_stats.allocationCpuMs);
    fprintf(file, "    \"centroid_build_cpu_ms\": %.6f,\n",
            preflight->bvh_build_stats.centroidBuildCpuMs);
    fprintf(file, "    \"tree_build_cpu_ms\": %.6f,\n",
            preflight->bvh_build_stats.treeBuildCpuMs);
    fprintf(file, "    \"range_bounds_cpu_ms\": %.6f,\n",
            preflight->bvh_build_stats.rangeBoundsCpuMs);
    fprintf(file, "    \"sort_cpu_ms\": %.6f,\n",
            preflight->bvh_build_stats.sortCpuMs);
    fprintf(file, "    \"node_append_cpu_ms\": %.6f,\n",
            preflight->bvh_build_stats.nodeAppendCpuMs);
    fprintf(file, "    \"final_stats_cpu_ms\": %.6f,\n",
            preflight->bvh_build_stats.finalStatsCpuMs);
    fprintf(file, "    \"build_unaccounted_cpu_ms\": %.6f,\n",
            preflight->bvh_build_stats.buildUnaccountedCpuMs);
    fprintf(file, "    \"range_bounds_calls\": %llu,\n",
            (unsigned long long)preflight->bvh_build_stats.rangeBoundsCalls);
    fprintf(file, "    \"sort_calls\": %llu,\n",
            (unsigned long long)preflight->bvh_build_stats.sortCalls);
    fprintf(file, "    \"node_append_calls\": %llu,\n",
            (unsigned long long)preflight->bvh_build_stats.nodeAppendCalls);
    fprintf(file, "    \"max_range_bounds_count\": %d,\n",
            preflight->bvh_build_stats.maxRangeBoundsCount);
    fprintf(file, "    \"max_sort_count\": %d,\n",
            preflight->bvh_build_stats.maxSortCount);
    fprintf(file, "    \"node_bytes\": %llu,\n",
            (unsigned long long)preflight->bvh_build_stats.nodeBytes);
    fprintf(file, "    \"index_bytes\": %llu,\n",
            (unsigned long long)preflight->bvh_build_stats.indexBytes);
    fprintf(file, "    \"centroid_bytes\": %llu,\n",
            (unsigned long long)preflight->bvh_build_stats.centroidBytes);
    fprintf(file, "    \"triangle_bounds_min_bytes\": %llu,\n",
            (unsigned long long)preflight->bvh_build_stats.triangleBoundsMinBytes);
    fprintf(file, "    \"triangle_bounds_max_bytes\": %llu,\n",
            (unsigned long long)preflight->bvh_build_stats.triangleBoundsMaxBytes);
    fprintf(file, "    \"sort_scratch_bytes\": %llu,\n",
            (unsigned long long)preflight->bvh_build_stats.sortScratchBytes);
    fprintf(file, "    \"build_scratch_bytes\": %llu,\n",
            (unsigned long long)preflight->bvh_build_stats.buildScratchBytes);
    fprintf(file, "    \"total_bytes\": %llu,\n",
            (unsigned long long)preflight->bvh_build_stats.totalBytes);
    fprintf(file, "    \"trace_calls\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.traceCalls);
    fprintf(file, "    \"trace_hits\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.traceHits);
    fprintf(file, "    \"trace_misses\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.traceMisses);
    fprintf(file, "    \"trace_overflows\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.traceOverflows);
    fprintf(file, "    \"flat_fallback_calls\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.flatFallbackCalls);
    fprintf(file, "    \"overflow_fallback_calls\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.overflowFallbackCalls);
    fprintf(file, "    \"node_visits\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.nodeVisits);
    fprintf(file, "    \"leaf_visits\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.leafVisits);
    fprintf(file, "    \"aabb_tests\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.aabbTests);
    fprintf(file, "    \"aabb_hits\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.aabbHits);
    fprintf(file, "    \"triangle_tests\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.triangleTests);
    fprintf(file, "    \"triangle_hits\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.triangleHits);
    fprintf(file, "    \"max_stack_depth\": %llu\n",
            (unsigned long long)preflight->bvh_trace_stats.maxStackDepth);
    fprintf(file, "  },\n");
    fprintf(file, "  \"object_audit\": [\n");
    {
        int emitted = 0;
        for (int i = 0; i < RAY_TRACING_HEADLESS_OBJECT_AUDIT_MAX; ++i) {
            const RayTracingHeadlessObjectAuditEntry *entry = &preflight->object_audit[i];
            if (!entry->used) continue;
            if (emitted > 0) {
                fprintf(file, ",\n");
            }
            fprintf(file, "    {\n");
            fprintf(file, "      \"scene_object_index\": %d,\n", entry->scene_object_index);
            fprintf(file, "      \"object_id\": ");
            RayTracingJsonWriteString(file, entry->object_id);
            fprintf(file, ",\n");
            fprintf(file, "      \"object_type\": ");
            RayTracingJsonWriteString(file, entry->object_type);
            fprintf(file, ",\n");
            fprintf(file, "      \"material_id\": %d,\n", entry->material_id);
            fprintf(file, "      \"alpha\": %.6f,\n", entry->alpha);
            fprintf(file, "      \"reflectivity\": %.6f,\n", entry->reflectivity);
            fprintf(file, "      \"roughness\": %.6f,\n", entry->roughness);
            fprintf(file, "      \"emissive_strength\": %.6f,\n", entry->emissive_strength);
            fprintf(file, "      \"texture_id\": %d,\n", entry->texture_id);
            fprintf(file, "      \"texture_strength\": %.6f,\n", entry->texture_strength);
            fprintf(file, "      \"texture_scale\": %.6f,\n", entry->texture_scale);
            fprintf(file, "      \"texture_offset_u\": %.6f,\n", entry->texture_offset_u);
            fprintf(file, "      \"texture_offset_v\": %.6f,\n", entry->texture_offset_v);
            fprintf(file, "      \"texture_seed\": %d,\n", entry->texture_seed);
            fprintf(file, "      \"texture_pattern_mode\": %d,\n", entry->texture_pattern_mode);
            fprintf(file, "      \"texture_coverage\": %.6f,\n", entry->texture_coverage);
            fprintf(file, "      \"texture_grain\": %.6f,\n", entry->texture_grain);
            fprintf(file, "      \"texture_edge_softness\": %.6f,\n", entry->texture_edge_softness);
            fprintf(file, "      \"texture_contrast\": %.6f,\n", entry->texture_contrast);
            fprintf(file, "      \"texture_flow\": %.6f,\n", entry->texture_flow);
            fprintf(file, "      \"texture_color_depth\": %.6f,\n", entry->texture_color_depth);
            fprintf(file, "      \"texture_surface_damage\": %.6f,\n", entry->texture_surface_damage);
            fprintf(file, "      \"packed_color\": %d,\n", entry->packed_color);
            fprintf(file, "      \"primitive_count\": %d,\n", entry->primitive_count);
            fprintf(file, "      \"triangle_count\": %d,\n", entry->triangle_count);
            fprintf(file, "      \"primary_hit_pixels\": %d,\n", entry->primary_hit_pixels);
            fprintf(file, "      \"center\": { \"x\": %.6f, \"y\": %.6f, \"z\": %.6f },\n",
                    entry->center_x,
                    entry->center_y,
                    entry->center_z);
            fprintf(file, "      \"center_projectable\": %s,\n",
                    entry->center_projectable ? "true" : "false");
            fprintf(file, "      \"center_inside_viewport\": %s,\n",
                    entry->center_inside_viewport ? "true" : "false");
            fprintf(file,
                    "      \"center_screen\": { \"x\": %.6f, \"y\": %.6f, \"camera_depth\": %.6f }\n",
                    entry->center_screen_x,
                    entry->center_screen_y,
                    entry->center_camera_depth);
            fprintf(file, "    }");
            emitted += 1;
        }
        if (emitted > 0) {
            fprintf(file, "\n");
        }
    }
    fprintf(file, "  ],\n");
    fprintf(file, "  \"diagnostics\": ");
    RayTracingJsonWriteString(file, preflight->diagnostics);
    fprintf(file, "\n");
    fprintf(file, "}\n");
}

bool ray_tracing_render_headless_write_summary_file(
    const char *path,
    const RayTracingAgentRenderRequest *request,
    const RayTracingHeadlessPreflight *preflight) {
    FILE *file = NULL;
    if (!path || !path[0]) return true;
    file = fopen(path, "wb");
    if (!file) return false;
    ray_tracing_render_headless_write_summary(file, request, preflight);
    fclose(file);
    return true;
}

static bool utc_now_string(char *out, size_t out_size) {
    time_t now = 0;
    struct tm tm_utc;
    if (!out || out_size == 0u) return false;
    out[0] = '\0';
    now = time(NULL);
    if (now == (time_t)-1) return false;
    if (gmtime_r(&now, &tm_utc) == NULL) return false;
    return strftime(out, out_size, "%Y-%m-%dT%H:%M:%SZ", &tm_utc) > 0u;
}

static bool ensure_directory_exists(const char *path) {
    char tmp[PATH_MAX];
    size_t len = 0u;

    if (!path || !path[0]) return false;
    len = strlen(path);
    if (len >= sizeof(tmp)) return false;
    memcpy(tmp, path, len + 1u);

    for (size_t i = 1u; i < len; ++i) {
        if (tmp[i] != '/') continue;
        tmp[i] = '\0';
        if (tmp[0] != '\0' && mkdir(tmp, 0700) != 0 && errno != EEXIST) {
            return false;
        }
        tmp[i] = '/';
    }

    if (mkdir(tmp, 0700) != 0 && errno != EEXIST) {
        return false;
    }
    return true;
}

static bool ensure_parent_directory_exists(const char *path) {
    const char *slash = NULL;
    char dir[PATH_MAX];
    size_t len = 0u;
    if (!path || !path[0]) return false;
    slash = strrchr(path, '/');
    if (!slash) return true;
    len = (size_t)(slash - path);
    if (len == 0u) {
        return true;
    }
    if (len >= sizeof(dir)) return false;
    memcpy(dir, path, len);
    dir[len] = '\0';
    return ensure_directory_exists(dir);
}

static bool write_progress_file(const char *path,
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
                                const char *diagnostics) {
    FILE *file = NULL;
    char updated_at_utc[32] = {0};
    if (!path || !path[0] || !request) return true;
    if (!ensure_parent_directory_exists(path)) return false;
    utc_now_string(updated_at_utc, sizeof(updated_at_utc));
    file = fopen(path, "wb");
    if (!file) return false;
    fprintf(file, "{\n");
    fprintf(file, "  \"schema_version\": \"ray_tracing_render_progress_v1\",\n");
    fprintf(file, "  \"run_id\": ");
    RayTracingJsonWriteString(file, request->run_id);
    fprintf(file, ",\n");
    fprintf(file, "  \"stage\": ");
    RayTracingJsonWriteString(file, stage ? stage : "unknown");
    fprintf(file, ",\n");
    fprintf(file, "  \"state\": ");
    RayTracingJsonWriteString(file, state ? state : "unknown");
    fprintf(file, ",\n");
    fprintf(file, "  \"frame_index\": %d,\n", frame_index);
    fprintf(file, "  \"frames_completed\": %d,\n", frames_completed);
    fprintf(file, "  \"frame_count\": %d,\n", request->frame_count);
    fprintf(file, "  \"temporal_subpasses_started\": %d,\n", temporal_subpasses_started);
    fprintf(file, "  \"temporal_subpasses_completed\": %d,\n", temporal_subpasses_completed);
    fprintf(file, "  \"temporal_subpasses_total\": %d,\n", temporal_subpasses_total);
    fprintf(file, "  \"completed_tiles_in_subpass\": %zu,\n", completed_tiles_in_subpass);
    fprintf(file, "  \"total_tiles_in_subpass\": %zu,\n", total_tiles_in_subpass);
    fprintf(file, "  \"elapsed_seconds\": %.6f,\n", elapsed_seconds > 0.0 ? elapsed_seconds : 0.0);
    fprintf(file, "  \"estimated_remaining_seconds\": %.6f,\n",
            estimated_remaining_seconds >= 0.0 ? estimated_remaining_seconds : -1.0);
    fprintf(file,
            "  \"progress_ratio\": %.6f,\n",
            RayTracingProgressRatioActive(frames_completed,
                                          request->frame_count,
                                          temporal_subpasses_started,
                                          temporal_subpasses_completed,
                                          temporal_subpasses_total,
                                          completed_tiles_in_subpass,
                                          total_tiles_in_subpass));
    fprintf(file, "  \"updated_at_utc\": ");
    RayTracingJsonWriteString(file, updated_at_utc);
    fprintf(file, ",\n");
    fprintf(file, "  \"diagnostics\": ");
    RayTracingJsonWriteString(file, diagnostics ? diagnostics : "");
    fprintf(file, "\n}\n");
    fclose(file);
    return true;
}

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
    int exit_code) {
    if (!write_progress_file(progress_path,
                             request,
                             stage,
                             frame_index,
                             frames_completed,
                             temporal_subpasses_started,
                             temporal_subpasses_completed,
                             temporal_subpasses_total,
                             completed_tiles_in_subpass,
                             total_tiles_in_subpass,
                             elapsed_seconds,
                             estimated_remaining_seconds,
                             state,
                             diagnostics)) {
        return false;
    }
    if (job_status_path && job_status_path[0] && job_id && job_id[0]) {
        if (!ray_tracing_render_headless_write_job_status_file(job_status_path,
                                                               job_id,
                                                               request_path,
                                                               request,
                                                               state,
                                                               stage,
                                                               exit_code,
                                                               frame_index,
                                                               frames_completed,
                                                               temporal_subpasses_started,
                                                               temporal_subpasses_completed,
                                                               temporal_subpasses_total,
                                                               completed_tiles_in_subpass,
                                                               total_tiles_in_subpass,
                                                               elapsed_seconds,
                                                               estimated_remaining_seconds,
                                                               diagnostics)) {
            return false;
        }
    }
    return true;
}

static void load_existing_job_status_times(const char *path,
                                           char *out_submitted_at_utc,
                                           size_t submitted_size,
                                           char *out_started_at_utc,
                                           size_t started_size) {
    json_object *root = NULL;
    json_object *value = NULL;
    const char *text_value = NULL;
    if (out_submitted_at_utc && submitted_size > 0u) out_submitted_at_utc[0] = '\0';
    if (out_started_at_utc && started_size > 0u) out_started_at_utc[0] = '\0';
    if (!path || !path[0]) return;
    root = json_object_from_file(path);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return;
    }
    if (out_submitted_at_utc && submitted_size > 0u &&
        json_object_object_get_ex(root, "submitted_at_utc", &value) &&
        json_object_is_type(value, json_type_string)) {
        text_value = json_object_get_string(value);
        if (text_value) snprintf(out_submitted_at_utc, submitted_size, "%s", text_value);
    }
    if (out_started_at_utc && started_size > 0u &&
        json_object_object_get_ex(root, "started_at_utc", &value) &&
        json_object_is_type(value, json_type_string)) {
        text_value = json_object_get_string(value);
        if (text_value) snprintf(out_started_at_utc, started_size, "%s", text_value);
    }
    json_object_put(root);
}

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
    const char *diagnostics) {
    FILE *file = NULL;
    char updated_at_utc[32] = {0};
    char started_at_utc[32] = {0};
    char finished_at_utc[32] = {0};
    char submitted_at_utc[32] = {0};
    char stdout_path[PATH_MAX] = {0};
    char stderr_path[PATH_MAX] = {0};
    char pid_path[PATH_MAX] = {0};
    char job_root[PATH_MAX] = {0};
    char overwrite_policy[32] = {0};
    const char *slash = NULL;
    size_t job_root_len = 0u;
    if (!path || !path[0] || !job_id || !job_id[0] || !request) return true;
    if (!ensure_parent_directory_exists(path)) return false;
    load_existing_job_status_times(path,
                                   submitted_at_utc,
                                   sizeof(submitted_at_utc),
                                   started_at_utc,
                                   sizeof(started_at_utc));
    slash = strrchr(path, '/');
    if (slash) {
        job_root_len = (size_t)(slash - path);
        if (job_root_len > 0u && job_root_len < sizeof(job_root)) {
            memcpy(job_root, path, job_root_len);
            job_root[job_root_len] = '\0';
            snprintf(stdout_path, sizeof(stdout_path), "%s/stdout.log", job_root);
            snprintf(stderr_path, sizeof(stderr_path), "%s/stderr.log", job_root);
            snprintf(pid_path, sizeof(pid_path), "%s/pid.txt", job_root);
        }
    }
    utc_now_string(updated_at_utc, sizeof(updated_at_utc));
    if ((strcmp(state ? state : "", "running") == 0 ||
         strcmp(state ? state : "", "completed") == 0 ||
         strcmp(state ? state : "", "failed") == 0) &&
        started_at_utc[0] == '\0') {
        snprintf(started_at_utc, sizeof(started_at_utc), "%s", updated_at_utc);
    }
    if (strcmp(state ? state : "", "completed") == 0 ||
        strcmp(state ? state : "", "failed") == 0 ||
        strcmp(state ? state : "", "cancelled") == 0) {
        snprintf(finished_at_utc, sizeof(finished_at_utc), "%s", updated_at_utc);
    }
    if (submitted_at_utc[0] == '\0') {
        snprintf(submitted_at_utc, sizeof(submitted_at_utc), "%s", updated_at_utc);
    }
    if (request->overwrite) {
        snprintf(overwrite_policy, sizeof(overwrite_policy), "overwrite");
    } else if (request->has_sampling_window && request->sampling_frame_offset > 0) {
        snprintf(overwrite_policy, sizeof(overwrite_policy), "resume");
    } else {
        snprintf(overwrite_policy, sizeof(overwrite_policy), "fail_if_exists");
    }
    file = fopen(path, "wb");
    if (!file) return false;
    fprintf(file, "{\n");
    fprintf(file, "  \"schema_version\": \"ray_tracing_detached_job_status_v1\",\n");
    fprintf(file, "  \"program\": \"ray_tracing\",\n");
    fprintf(file, "  \"tool\": \"ray_tracing_render_headless\",\n");
    fprintf(file, "  \"job_id\": ");
    RayTracingJsonWriteString(file, job_id);
    fprintf(file, ",\n");
    fprintf(file, "  \"state\": ");
    RayTracingJsonWriteString(file, state ? state : "unknown");
    fprintf(file, ",\n");
    fprintf(file, "  \"stage\": ");
    RayTracingJsonWriteString(file, stage ? stage : "");
    fprintf(file, ",\n");
    fprintf(file, "  \"request_path\": ");
    RayTracingJsonWriteString(file, request_path ? request_path : "");
    fprintf(file, ",\n");
    fprintf(file, "  \"output_root\": ");
    RayTracingJsonWriteString(file, request->output_root);
    fprintf(file, ",\n");
    fprintf(file, "  \"progress_path\": ");
    RayTracingJsonWriteString(file, request->progress_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"summary_path\": ");
    RayTracingJsonWriteString(file, request->summary_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"stdout_path\": ");
    RayTracingJsonWriteString(file, stdout_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"stderr_path\": ");
    RayTracingJsonWriteString(file, stderr_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"pid_path\": ");
    RayTracingJsonWriteString(file, pid_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"pid\": %ld,\n", (long)getpid());
    fprintf(file, "  \"exit_code\": %d,\n", exit_code);
    fprintf(file, "  \"overwrite_policy\": ");
    RayTracingJsonWriteString(file, overwrite_policy);
    fprintf(file, ",\n");
    fprintf(file, "  \"requested_start_frame\": %d,\n",
            request->start_frame - (request->has_sampling_window ? request->sampling_frame_offset : 0));
    fprintf(file, "  \"requested_frame_count\": %d,\n",
            request->has_sampling_window ? request->sampling_frame_count : request->frame_count);
    fprintf(file, "  \"effective_start_frame\": %d,\n", request->start_frame);
    fprintf(file, "  \"effective_frame_count\": %d,\n", request->frame_count);
    fprintf(file, "  \"frame_index\": %d,\n", frame_index);
    fprintf(file, "  \"frames_completed\": %d,\n", frames_completed);
    fprintf(file, "  \"frame_count\": %d,\n", request->frame_count);
    fprintf(file, "  \"temporal_subpasses_started\": %d,\n", temporal_subpasses_started);
    fprintf(file, "  \"temporal_subpasses_completed\": %d,\n", temporal_subpasses_completed);
    fprintf(file, "  \"temporal_subpasses_total\": %d,\n", temporal_subpasses_total);
    fprintf(file, "  \"completed_tiles_in_subpass\": %zu,\n", completed_tiles_in_subpass);
    fprintf(file, "  \"total_tiles_in_subpass\": %zu,\n", total_tiles_in_subpass);
    fprintf(file, "  \"elapsed_seconds\": %.6f,\n", elapsed_seconds > 0.0 ? elapsed_seconds : 0.0);
    fprintf(file, "  \"estimated_remaining_seconds\": %.6f,\n",
            estimated_remaining_seconds >= 0.0 ? estimated_remaining_seconds : -1.0);
    fprintf(file,
            "  \"progress_ratio\": %.6f,\n",
            RayTracingProgressRatioActive(frames_completed,
                                          request->frame_count,
                                          temporal_subpasses_started,
                                          temporal_subpasses_completed,
                                          temporal_subpasses_total,
                                          completed_tiles_in_subpass,
                                          total_tiles_in_subpass));
    fprintf(file, "  \"submitted_at_utc\": ");
    RayTracingJsonWriteString(file, submitted_at_utc);
    fprintf(file, ",\n");
    fprintf(file, "  \"started_at_utc\": ");
    RayTracingJsonWriteString(file, started_at_utc);
    fprintf(file, ",\n");
    fprintf(file, "  \"finished_at_utc\": ");
    RayTracingJsonWriteString(file, finished_at_utc);
    fprintf(file, ",\n");
    fprintf(file, "  \"updated_at_utc\": ");
    RayTracingJsonWriteString(file, updated_at_utc);
    fprintf(file, ",\n");
    fprintf(file, "  \"diagnostics\": ");
    RayTracingJsonWriteString(file, diagnostics ? diagnostics : "");
    fprintf(file, "\n}\n");
    fclose(file);
    return true;
}
