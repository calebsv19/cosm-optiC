#include "app/agent_render_request_internal.h"

static bool agent_render_request_json_get_double_any(json_object* owner,
                                                     const char* key_a,
                                                     const char* key_b,
                                                     double* out_value) {
    if (RayTracingJsonGetDouble(owner, key_a, out_value)) return true;
    if (key_b && RayTracingJsonGetDouble(owner, key_b, out_value)) return true;
    return false;
}

static bool agent_render_request_parse_caustic_lens_traversal_profile(
    json_object* owner,
    const char* key,
    RuntimeCausticLensTraversalProfile3D* out_profile,
    char* out_diagnostics,
    size_t out_diagnostics_size,
    const char* request_path) {
    json_object* profile_obj = NULL;
    const char* preset = NULL;
    double value = 0.0;

    if (!owner || !key || !out_profile ||
        !json_object_object_get_ex(owner, key, &profile_obj)) {
        return false;
    }

    if (json_object_is_type(profile_obj, json_type_string)) {
        preset = json_object_get_string(profile_obj);
        if (!RuntimeCausticLensTransport3D_PresetTraversalProfileFromLabel(preset,
                                                                           out_profile)) {
            agent_render_request_set_diagf(out_diagnostics,
                              out_diagnostics_size,
                              "request=%s field=inspection.%s invalid preset=%s",
                              request_path,
                              key,
                              preset ? preset : "<null>");
            return false;
        }
        return true;
    }

    if (!json_object_is_type(profile_obj, json_type_object)) {
        agent_render_request_set_diagf(out_diagnostics,
                          out_diagnostics_size,
                          "request=%s field=inspection.%s expected string or object",
                          request_path,
                          key);
        return false;
    }

    RuntimeCausticLensTransport3D_DefaultTraversalProfile(out_profile);
    out_profile->kind = RUNTIME_CAUSTIC_LENS_TRAVERSAL_PROFILE_CUSTOM;
    if (RayTracingJsonGetString(profile_obj, "preset", &preset)) {
        if (!RuntimeCausticLensTransport3D_PresetTraversalProfileFromLabel(preset,
                                                                           out_profile)) {
            agent_render_request_set_diagf(out_diagnostics,
                              out_diagnostics_size,
                              "request=%s field=inspection.%s.preset invalid preset=%s",
                              request_path,
                              key,
                              preset ? preset : "<null>");
            return false;
        }
    }

    if (agent_render_request_json_get_double_any(profile_obj,
                                                 "outside_ior",
                                                 "outsideIor",
                                                 &value)) {
        out_profile->outsideIor = value;
    }
    if (agent_render_request_json_get_double_any(profile_obj,
                                                 "material_ior",
                                                 "materialIor",
                                                 &value) ||
        RayTracingJsonGetDouble(profile_obj, "ior", &value)) {
        out_profile->materialIor = value;
    }
    if (agent_render_request_json_get_double_any(profile_obj,
                                                 "fresnel_scale",
                                                 "fresnelScale",
                                                 &value)) {
        out_profile->fresnelScale = value;
    }
    if (agent_render_request_json_get_double_any(profile_obj,
                                                 "transmission_scale",
                                                 "transmissionScale",
                                                 &value)) {
        out_profile->transmissionScale = value;
    }
    if (agent_render_request_json_get_rgb(profile_obj, "tint",
                                          &out_profile->tint.x,
                                          &out_profile->tint.y,
                                          &out_profile->tint.z) ||
        agent_render_request_json_get_rgb(profile_obj, "absorption_tint",
                                          &out_profile->tint.x,
                                          &out_profile->tint.y,
                                          &out_profile->tint.z)) {
        out_profile->kind = RUNTIME_CAUSTIC_LENS_TRAVERSAL_PROFILE_CUSTOM;
    }
    if (agent_render_request_json_get_double_any(profile_obj,
                                                 "absorption_distance",
                                                 "absorptionDistance",
                                                 &value)) {
        out_profile->absorptionDistance = value;
    }
    if (agent_render_request_json_get_double_any(profile_obj,
                                                 "aperture_radius_scale",
                                                 "apertureRadiusScale",
                                                 &value)) {
        out_profile->apertureRadiusScale = value;
    }

    RuntimeCausticLensTransport3D_NormalizeTraversalProfile(out_profile);
    out_profile->kind = RUNTIME_CAUSTIC_LENS_TRAVERSAL_PROFILE_CUSTOM;
    return true;
}

bool ray_tracing_agent_render_request_load_file(const char *request_path,
                                                RayTracingAgentRenderRequest *out_request,
                                                char *out_diagnostics,
                                                size_t out_diagnostics_size) {
    RayTracingAgentRenderRequest request;
    char request_dir[PATH_MAX] = {0};
    char *text = NULL;
    json_object *root = NULL;
    json_object *scene = NULL;
    json_object *volume = NULL;
    json_object *render = NULL;
    json_object *output = NULL;
    json_object *progress = NULL;
    json_object *inspection = NULL;
    json_object *caustic_profile = NULL;
    json_object *sampling = NULL;
    json_object *resources = NULL;
    const char *value = NULL;
    int int_value = 0;
    double double_value = 0.0;
    bool bool_value = false;

    RayTracingRequestSetDiag(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!request_path || !request_path[0] || !out_request) return false;

    ray_tracing_agent_render_request_defaults(&request);
    RayTracingDirnameOf(request_path, request_dir, sizeof(request_dir));
    if (!RayTracingReadTextFile(request_path, &text)) {
        agent_render_request_set_diagf(out_diagnostics,
                          out_diagnostics_size,
                          "request=%s field=<file> failed to read request file",
                          request_path);
        return false;
    }

    root = json_tokener_parse(text);
    free(text);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        agent_render_request_set_diagf(out_diagnostics,
                          out_diagnostics_size,
                          "request=%s field=<root> failed to parse request json object",
                          request_path);
        return false;
    }

    if (!RayTracingJsonGetString(root, "schema_version", &value)) {
        json_object_put(root);
        agent_render_request_set_diagf(out_diagnostics,
                          out_diagnostics_size,
                          "request=%s field=schema_version expected=%s actual=<missing-or-non-string>",
                          request_path,
                          RAY_TRACING_AGENT_RENDER_REQUEST_SCHEMA);
        return false;
    }
    if (strcmp(value, RAY_TRACING_AGENT_RENDER_REQUEST_SCHEMA) != 0) {
        const char *actual_schema = value;
        agent_render_request_set_diagf(out_diagnostics,
                          out_diagnostics_size,
                          "request=%s field=schema_version expected=%s actual=%s",
                          request_path,
                          RAY_TRACING_AGENT_RENDER_REQUEST_SCHEMA,
                          actual_schema ? actual_schema : "<null>");
        json_object_put(root);
        return false;
    }

    if (RayTracingJsonGetString(root, "run_id", &value) &&
        !RayTracingCopyString(request.run_id, sizeof(request.run_id), value)) {
        json_object_put(root);
        agent_render_request_set_diagf(out_diagnostics,
                          out_diagnostics_size,
                          "request=%s field=run_id value too long",
                          request_path);
        return false;
    }

    if (!RayTracingJsonGetObject(root, "scene", &scene) ||
        !RayTracingJsonGetString(scene, "runtime_scene_path", &value) ||
        !RayTracingResolveRequestInputPath(request_dir,
                                   value,
                                   request.runtime_scene_path,
                                   sizeof(request.runtime_scene_path))) {
        json_object_put(root);
        agent_render_request_set_diagf(out_diagnostics,
                          out_diagnostics_size,
                          "request=%s field=scene.runtime_scene_path missing, non-string, invalid, or path too long",
                          request_path);
        return false;
    }

    if (RayTracingJsonGetObject(root, "volume", &volume)) {
        const char *kind_label = "auto";
        RayTracingJsonGetBool(volume, "enabled", &request.volume_enabled);
        if (RayTracingJsonGetString(volume, "source_path", &value)) {
            if (!RayTracingResolveRequestInputPath(request_dir,
                                           value,
                                           request.volume_source_path,
                                           sizeof(request.volume_source_path))) {
                json_object_put(root);
                agent_render_request_set_diagf(out_diagnostics,
                                  out_diagnostics_size,
                                  "request=%s field=volume.source_path invalid or path too long",
                                  request_path);
                return false;
            }
            request.volume_enabled = true;
        }
        if (RayTracingJsonGetString(volume, "source_kind", &value)) {
            kind_label = value;
        }
        request.volume_source_kind =
            agent_render_request_parse_volume_source_kind(kind_label, request.volume_source_path);
        RayTracingJsonGetBool(volume, "visible", &request.volume_visible);
        RayTracingJsonGetBool(volume, "affects_lighting", &request.volume_affects_lighting);
        RayTracingJsonGetBool(volume, "debug_overlay", &request.volume_debug_overlay);
        if (request.volume_enabled &&
            (request.volume_source_kind == VOLUME_SOURCE_NONE ||
            request.volume_source_path[0] == '\0')) {
            json_object_put(root);
            agent_render_request_set_diagf(out_diagnostics,
                              out_diagnostics_size,
                              "request=%s field=volume source_kind=%s source_path=%s unsupported or missing",
                              request_path,
                              kind_label ? kind_label : "auto",
                              request.volume_source_path[0] ? request.volume_source_path : "<empty>");
            return false;
        }
    }

    if (RayTracingJsonGetObject(root, "render", &render)) {
        if (RayTracingJsonGetInt(render, "start_frame", &int_value)) request.start_frame = int_value;
        if (RayTracingJsonGetInt(render, "frame_count", &int_value)) request.frame_count = int_value;
        if (RayTracingJsonGetInt(render, "width", &int_value)) request.width = int_value;
        if (RayTracingJsonGetInt(render, "height", &int_value)) request.height = int_value;
        if (RayTracingJsonGetDouble(render, "normalized_t", &double_value)) {
            request.normalized_t = double_value;
        }
        if (RayTracingJsonGetInt(render, "temporal_frames", &int_value)) {
            request.temporal_frames = int_value;
        }
        if (RayTracingJsonGetBool(render, "use_tiled_renderer", &bool_value) ||
            RayTracingJsonGetBool(render, "tiled_renderer", &bool_value)) {
            request.has_tiled_renderer_override = true;
            request.tiled_renderer_override = bool_value;
        }
        if (RayTracingJsonGetInt(render, "tile_size", &int_value)) {
            request.has_tile_size_override = true;
            request.tile_size_override = int_value;
        }
        if (RayTracingJsonGetBool(render, "adaptive_sampling_enabled", &bool_value) ||
            RayTracingJsonGetBool(render, "adaptive_runtime_enabled", &bool_value)) {
            request.has_adaptive_sampling_override = true;
            request.adaptive_sampling_enabled_override = bool_value;
        }
        if (RayTracingJsonGetBool(render, "denoise_enabled", &bool_value)) {
            request.has_denoise_enabled_override = true;
            request.denoise_enabled_override = bool_value;
        }
        if (RayTracingJsonGetString(render, "integrator_3d", &value)) {
            request.integrator_3d = agent_render_request_parse_integrator_3d(value);
            request.has_integrator_3d_override = true;
        }
    }

    if (RayTracingJsonGetObject(root, "resources", &resources)) {
        if (RayTracingJsonGetInt(resources, "cpu_percent", &int_value)) {
            request.has_resource_budget = true;
            request.resource_cpu_percent = int_value;
        }
        if (RayTracingJsonGetInt(resources, "max_workers", &int_value)) {
            request.has_resource_budget = true;
            request.resource_max_workers = int_value;
        }
        if (RayTracingJsonGetInt(resources, "reserve_cpu_count", &int_value)) {
            request.has_resource_budget = true;
            request.resource_reserve_cpu_count = int_value;
        }
    } else {
        if (RayTracingEnvGetInt("CODEWORK_RAY_TRACING_DEFAULT_CPU_PERCENT", &int_value)) {
            request.has_resource_budget = true;
            request.resource_cpu_percent = int_value;
        }
        if (RayTracingEnvGetInt("CODEWORK_RAY_TRACING_DEFAULT_MAX_WORKERS", &int_value)) {
            request.has_resource_budget = true;
            request.resource_max_workers = int_value;
        }
        if (RayTracingEnvGetInt("CODEWORK_RAY_TRACING_DEFAULT_RESERVE_CPU_COUNT", &int_value)) {
            request.has_resource_budget = true;
            request.resource_reserve_cpu_count = int_value;
        }
    }

    if (RayTracingJsonGetObject(root, "sampling", &sampling)) {
        bool has_offset = false;
        bool has_count = false;
        if (RayTracingJsonGetInt(sampling, "frame_offset", &int_value)) {
            request.has_sampling_window = true;
            request.sampling_frame_offset = int_value;
            has_offset = true;
        }
        if (RayTracingJsonGetInt(sampling, "frame_count", &int_value)) {
            request.has_sampling_window = true;
            request.sampling_frame_count = int_value;
            has_count = true;
        }
        if (request.has_sampling_window && (!has_offset || !has_count)) {
            json_object_put(root);
            agent_render_request_set_diagf(out_diagnostics,
                              out_diagnostics_size,
                              "request=%s field=sampling frame_offset and frame_count required together",
                              request_path);
            return false;
        }
    }

    if (RayTracingJsonGetObject(root, "output", &output)) {
        json_object *video = NULL;
        if (RayTracingJsonGetString(output, "root", &value) &&
            !RayTracingResolveRequestOutputPath(request_dir,
                                        value,
                                        request.output_root,
                                        sizeof(request.output_root))) {
            json_object_put(root);
            agent_render_request_set_diagf(out_diagnostics,
                              out_diagnostics_size,
                              "request=%s field=output.root invalid or path too long",
                              request_path);
            return false;
        }
        RayTracingJsonGetBool(output, "overwrite", &request.overwrite);
        if (RayTracingJsonGetObject(output, "video", &video)) {
            RayTracingJsonGetBool(video, "enabled", &request.video_enabled);
            if (RayTracingJsonGetString(video, "path", &value)) {
                if (!RayTracingResolveRequestOutputPath(request_dir,
                                                value,
                                                request.video_path,
                                                sizeof(request.video_path))) {
                    json_object_put(root);
                    agent_render_request_set_diagf(out_diagnostics,
                                      out_diagnostics_size,
                                      "request=%s field=output.video.path invalid or path too long",
                                      request_path);
                    return false;
                }
                request.video_enabled = true;
            }
            if (RayTracingJsonGetInt(video, "fps", &int_value)) {
                request.video_fps = int_value;
            }
            if (request.video_enabled && !request.video_path[0]) {
                json_object_put(root);
                agent_render_request_set_diagf(out_diagnostics,
                                  out_diagnostics_size,
                                  "request=%s field=output.video.path required when video is enabled",
                                  request_path);
                return false;
            }
            if (request.video_fps <= 0) {
                request.video_fps = 30;
            }
        }
    }

    if (RayTracingJsonGetObject(root, "progress", &progress)) {
        if (RayTracingJsonGetString(progress, "summary_path", &value) &&
            !RayTracingResolveRequestOutputPath(request_dir,
                                        value,
                                        request.summary_path,
                                        sizeof(request.summary_path))) {
            json_object_put(root);
            agent_render_request_set_diagf(out_diagnostics,
                              out_diagnostics_size,
                              "request=%s field=progress.summary_path invalid or path too long",
                              request_path);
            return false;
        }
        if (RayTracingJsonGetString(progress, "progress_path", &value) &&
            !RayTracingResolveRequestOutputPath(request_dir,
                                        value,
                                        request.progress_path,
                                        sizeof(request.progress_path))) {
            json_object_put(root);
            agent_render_request_set_diagf(out_diagnostics,
                              out_diagnostics_size,
                              "request=%s field=progress.progress_path invalid or path too long",
                              request_path);
            return false;
        }
    }

    if (RayTracingJsonGetObject(root, "inspection", &inspection)) {
        if (RayTracingJsonGetString(inspection, "preset", &value)) {
            request.inspection_preset = agent_render_request_parse_inspection_preset(value);
        }
        if (RayTracingJsonGetDouble(inspection, "camera_zoom", &double_value)) {
            request.has_camera_zoom_override = true;
            request.camera_zoom_override = double_value;
        }
        if (RayTracingJsonGetObject(inspection, "camera_position", &output)) {
            if (RayTracingJsonGetDouble(output, "x", &double_value)) {
                request.has_camera_position_override = true;
                request.camera_position_x = double_value;
            }
            if (RayTracingJsonGetDouble(output, "y", &double_value)) {
                request.has_camera_position_override = true;
                request.camera_position_y = double_value;
            }
            if (RayTracingJsonGetDouble(output, "z", &double_value)) {
                request.has_camera_position_override = true;
                request.camera_position_z = double_value;
            }
        }
        if (RayTracingJsonGetObject(inspection, "camera_look_at", &output)) {
            if (RayTracingJsonGetDouble(output, "x", &double_value)) {
                request.has_camera_look_at_override = true;
                request.camera_look_at_x = double_value;
            }
            if (RayTracingJsonGetDouble(output, "y", &double_value)) {
                request.has_camera_look_at_override = true;
                request.camera_look_at_y = double_value;
            }
            if (RayTracingJsonGetDouble(output, "z", &double_value)) {
                request.has_camera_look_at_override = true;
                request.camera_look_at_z = double_value;
            }
        }
        if (RayTracingJsonGetDouble(inspection, "environment_brightness", &double_value)) {
            request.has_environment_brightness_override = true;
            request.environment_brightness_override = double_value;
        }
        if (RayTracingJsonGetDouble(inspection, "ambient_strength", &double_value)) {
            request.has_ambient_strength_override = true;
            request.ambient_strength_override = double_value;
        }
        if (RayTracingJsonGetString(inspection, "environment_light_mode", &value)) {
            request.has_environment_light_mode_override = true;
            request.environment_light_mode_override = agent_render_request_parse_environment_light_mode(value);
        }
        if (RayTracingJsonGetString(inspection, "environment_preset", &value)) {
            request.has_environment_preset_override = true;
            request.environment_preset_override = agent_render_request_parse_environment_preset(value);
        }
        if (RayTracingJsonGetDouble(inspection, "background_brightness", &double_value)) {
            request.has_background_brightness_override = true;
            request.background_brightness_override = double_value;
        }
        if (agent_render_request_json_get_rgb(inspection,
                         "background_color",
                         &request.background_color_r,
                         &request.background_color_g,
                         &request.background_color_b)) {
            request.has_background_color_override = true;
        }
        if (RayTracingJsonGetDouble(inspection, "top_fill_strength", &double_value)) {
            request.has_top_fill_strength_override = true;
            request.top_fill_strength_override = double_value;
        }
        if (RayTracingJsonGetDouble(inspection, "light_intensity", &double_value)) {
            request.has_light_intensity_override = true;
            request.light_intensity_override = double_value;
        }
        if (RayTracingJsonGetDouble(inspection, "light_radius", &double_value)) {
            request.has_light_radius_override = true;
            request.light_radius_override = double_value;
        }
        if (RayTracingJsonGetDouble(inspection, "forward_decay", &double_value)) {
            request.has_forward_decay_override = true;
            request.forward_decay_override = double_value;
        }
        if (RayTracingJsonGetDouble(inspection, "volume_scatter_gain", &double_value)) {
            request.has_volume_scatter_gain_override = true;
            request.volume_scatter_gain_override = double_value;
        }
        if (RayTracingJsonGetDouble(inspection,
                                    "caustic_volume_scatter_gain",
                                    &double_value) ||
            RayTracingJsonGetDouble(inspection,
                                    "volume_caustic_scatter_gain",
                                    &double_value)) {
            request.has_caustic_volume_scatter_gain_override = true;
            request.caustic_volume_scatter_gain_override = double_value;
        }
        if (RayTracingJsonGetDouble(inspection, "volume_density_scale", &double_value) ||
            RayTracingJsonGetDouble(inspection, "density_scale", &double_value)) {
            request.has_volume_density_scale_override = true;
            request.volume_density_scale_override = double_value;
        }
        if (RayTracingJsonGetDouble(inspection, "volume_density_gamma", &double_value) ||
            RayTracingJsonGetDouble(inspection, "density_gamma", &double_value)) {
            request.has_volume_density_gamma_override = true;
            request.volume_density_gamma_override = double_value;
        }
        if (RayTracingJsonGetDouble(inspection, "volume_absorption_gain", &double_value) ||
            RayTracingJsonGetDouble(inspection, "absorption_gain", &double_value)) {
            request.has_volume_absorption_gain_override = true;
            request.volume_absorption_gain_override = double_value;
        }
        if (RayTracingJsonGetDouble(inspection, "volume_opacity_clamp", &double_value) ||
            RayTracingJsonGetDouble(inspection, "opacity_clamp", &double_value)) {
            request.has_volume_opacity_clamp_override = true;
            request.volume_opacity_clamp_override = double_value;
        }
        if (RayTracingJsonGetDouble(inspection, "volume_step_scale", &double_value)) {
            request.has_volume_step_scale_override = true;
            request.volume_step_scale_override = double_value;
        }
        if (RayTracingJsonGetInt(inspection, "secondary_diffuse_samples_3d", &int_value)) {
            request.has_secondary_diffuse_samples_3d_override = true;
            request.secondary_diffuse_samples_3d_override = int_value;
        }
        if (RayTracingJsonGetInt(inspection, "transmission_samples_3d", &int_value)) {
            request.has_transmission_samples_3d_override = true;
            request.transmission_samples_3d_override = int_value;
        }
        if (RayTracingJsonGetString(inspection, "trace_route", &value) ||
            RayTracingJsonGetString(inspection, "acceleration_route", &value) ||
            RayTracingJsonGetString(inspection, "prepared_acceleration_route", &value)) {
            request.has_trace_route_override = true;
            if (!agent_render_request_parse_trace_route(value, &request.trace_route)) {
                json_object_put(root);
                agent_render_request_set_diagf(out_diagnostics,
                                  out_diagnostics_size,
                                  "request=%s field=inspection.trace_route invalid value=%s",
                                  request_path,
                                  value);
                return false;
            }
        }
        if (RayTracingJsonGetString(inspection, "caustic_mode", &value) ||
            RayTracingJsonGetString(inspection, "disney_v2_caustic_mode", &value)) {
            request.has_caustic_mode_override = true;
            request.caustic_settings.mode = RuntimeCausticMode3D_FromLabel(value);
            request.caustic_mode =
                agent_render_request_caustic_mode_to_disney_v2_mode(request.caustic_settings.mode);
            request.caustic_sidecar_enabled =
                request.caustic_settings.mode == RUNTIME_CAUSTIC_MODE_ANALYTIC;
        }
        if (RayTracingJsonGetBool(inspection, "caustic_volume_enabled", &bool_value) ||
            RayTracingJsonGetBool(inspection, "caustic_volume_cache_enabled", &bool_value)) {
            request.caustic_settings.volumeCacheEnabled = bool_value;
        }
        if (RayTracingJsonGetBool(inspection, "caustic_surface_enabled", &bool_value) ||
            RayTracingJsonGetBool(inspection, "caustic_surface_cache_enabled", &bool_value)) {
            request.caustic_settings.surfaceCacheEnabled = bool_value;
        }
        if (RayTracingJsonGetInt(inspection, "caustic_sample_budget", &int_value) ||
            RayTracingJsonGetInt(inspection, "caustic_path_count", &int_value)) {
            request.caustic_settings.sampleBudget = int_value;
        }
        if (RayTracingJsonGetInt(inspection, "caustic_max_path_depth", &int_value) ||
            RayTracingJsonGetInt(inspection, "caustic_path_depth", &int_value)) {
            request.caustic_settings.maxPathDepth = int_value;
        }
        if (RayTracingJsonGetString(inspection, "caustic_transport_engine", &value) ||
            RayTracingJsonGetString(inspection, "caustic_engine", &value)) {
            request.caustic_settings.transportEngine =
                RuntimeCausticTransportEngine3D_FromLabel(value);
        }
        if (RayTracingJsonGetString(inspection, "caustic_transport_emission_policy", &value) ||
            RayTracingJsonGetString(inspection, "caustic_emission_policy", &value)) {
            request.caustic_settings.emissionPolicy =
                RuntimeCausticTransportEmissionPolicy3D_FromLabel(value);
        }
        if (RayTracingJsonGetString(inspection, "caustic_product_mode", &value) ||
            RayTracingJsonGetString(inspection, "caustic_photon_product_mode", &value)) {
            request.has_caustic_product_mode_override = true;
            request.caustic_photon_integration_settings.productMode =
                RuntimeCausticProductMode3D_FromLabel(value);
        }
        if (RayTracingJsonGetBool(inspection,
                                  "caustic_surface_query_enabled",
                                  &bool_value) ||
            RayTracingJsonGetBool(inspection,
                                  "caustic_photon_surface_query_enabled",
                                  &bool_value)) {
            request.has_caustic_product_mode_override = true;
            request.caustic_photon_integration_settings.surfaceQueryEnabled = bool_value;
        }
        if (RayTracingJsonGetBool(inspection,
                                  "caustic_volume_query_enabled",
                                  &bool_value) ||
            RayTracingJsonGetBool(inspection,
                                  "caustic_photon_volume_query_enabled",
                                  &bool_value)) {
            request.has_caustic_product_mode_override = true;
            request.caustic_photon_integration_settings.volumeQueryEnabled = bool_value;
        }
        if (RayTracingJsonGetBool(inspection,
                                  "caustic_render_contribution_enabled",
                                  &bool_value) ||
            RayTracingJsonGetBool(inspection,
                                  "caustic_photon_render_contribution_enabled",
                                  &bool_value)) {
            request.has_caustic_product_mode_override = true;
            request.caustic_photon_integration_settings.renderContributionEnabled =
                bool_value;
        }
        if (RayTracingJsonGetBool(inspection,
                                  "caustic_photon_render_prep_population_enabled",
                                  &bool_value) ||
            RayTracingJsonGetBool(inspection,
                                  "caustic_production_render_prep_population_enabled",
                                  &bool_value)) {
            request.has_caustic_product_mode_override = true;
            request.caustic_photon_render_prep_population_enabled = bool_value;
        }
        if (RayTracingJsonGetInt(inspection, "caustic_photon_sample_budget", &int_value)) {
            request.has_caustic_product_mode_override = true;
            request.caustic_photon_integration_settings.sampleBudget = int_value;
        }
        if (RayTracingJsonGetInt(inspection, "caustic_photon_max_path_depth", &int_value)) {
            request.has_caustic_product_mode_override = true;
            request.caustic_photon_integration_settings.maxPathDepth = int_value;
        }
        if (RayTracingJsonGetDouble(inspection,
                                    "caustic_photon_surface_radiance_scale",
                                    &double_value)) {
            request.has_caustic_product_mode_override = true;
            request.caustic_photon_integration_settings.surfaceRadianceScale =
                double_value;
        }
        if (RayTracingJsonGetDouble(inspection,
                                    "caustic_photon_surface_query_radius",
                                    &double_value)) {
            request.has_caustic_product_mode_override = true;
            request.caustic_photon_integration_settings.surfaceQueryRadius =
                double_value;
        }
        if (RayTracingJsonGetDouble(inspection,
                                    "caustic_photon_volume_query_radius",
                                    &double_value)) {
            request.has_caustic_product_mode_override = true;
            request.caustic_photon_integration_settings.volumeQueryRadius =
                double_value;
        }
        if (RayTracingJsonGetBool(inspection,
                                  "caustic_photon_populated_callsite_readback_enabled",
                                  &bool_value) ||
            RayTracingJsonGetBool(inspection,
                                  "caustic_production_populated_callsite_readback_enabled",
                                  &bool_value)) {
            request.has_caustic_product_mode_override = true;
            request.caustic_photon_populated_callsite_readback_enabled = bool_value;
        }
        if (RayTracingJsonGetBool(
                inspection,
                "caustic_photon_trace_populated_callsite_readback_enabled",
                &bool_value) ||
            RayTracingJsonGetBool(
                inspection,
                "caustic_production_trace_populated_callsite_readback_enabled",
                &bool_value)) {
            request.has_caustic_product_mode_override = true;
            request.caustic_photon_trace_populated_callsite_readback_enabled =
                bool_value;
        }
        if (request.has_caustic_product_mode_override) {
            RuntimeCausticPhotonIntegration3D_ApplyToCausticSettings(
                &request.caustic_photon_integration_settings,
                &request.caustic_settings);
            request.has_caustic_mode_override = true;
            request.caustic_mode =
                agent_render_request_caustic_mode_to_disney_v2_mode(
                    request.caustic_settings.mode);
            request.caustic_sidecar_enabled =
                request.caustic_settings.mode == RUNTIME_CAUSTIC_MODE_ANALYTIC;
        }
        if (json_object_object_get_ex(inspection,
                                      "caustic_lens_traversal_profile",
                                      &caustic_profile)) {
            if (!agent_render_request_parse_caustic_lens_traversal_profile(
                    inspection,
                    "caustic_lens_traversal_profile",
                    &request.caustic_settings.traversalProfileOverride,
                    out_diagnostics,
                    out_diagnostics_size,
                    request_path)) {
                json_object_put(root);
                return false;
            }
            request.caustic_settings.hasTraversalProfileOverride = true;
        } else if (json_object_object_get_ex(inspection,
                                             "caustic_lens_profile",
                                             &caustic_profile)) {
            if (!agent_render_request_parse_caustic_lens_traversal_profile(
                    inspection,
                    "caustic_lens_profile",
                    &request.caustic_settings.traversalProfileOverride,
                    out_diagnostics,
                    out_diagnostics_size,
                    request_path)) {
                json_object_put(root);
                return false;
            }
            request.caustic_settings.hasTraversalProfileOverride = true;
        } else if (RayTracingJsonGetString(inspection,
                                           "caustic_lens_traversal_preset",
                                           &value) ||
                   RayTracingJsonGetString(inspection,
                                           "caustic_lens_profile_preset",
                                           &value)) {
            if (!RuntimeCausticLensTransport3D_PresetTraversalProfileFromLabel(
                    value,
                    &request.caustic_settings.traversalProfileOverride)) {
                json_object_put(root);
                agent_render_request_set_diagf(out_diagnostics,
                                  out_diagnostics_size,
                                  "request=%s field=inspection.caustic_lens_traversal_preset invalid preset=%s",
                                  request_path,
                                  value ? value : "<null>");
                return false;
            }
            request.caustic_settings.hasTraversalProfileOverride = true;
        }
        if (RayTracingJsonGetDouble(inspection, "caustic_surface_radiance_scale", &double_value) ||
            RayTracingJsonGetDouble(inspection, "caustic_surface_energy_scale", &double_value) ||
            RayTracingJsonGetDouble(inspection, "caustic_receiver_energy_scale", &double_value)) {
            request.caustic_settings.surfaceRadianceScale = double_value;
        }
        if (RayTracingJsonGetDouble(inspection, "caustic_surface_footprint_scale", &double_value) ||
            RayTracingJsonGetDouble(inspection, "caustic_receiver_footprint_scale", &double_value)) {
            request.caustic_settings.surfaceFootprintScale = double_value;
        }
        if (RayTracingJsonGetBool(inspection,
                                  "caustic_surface_receiver_fallback_enabled",
                                  &bool_value) ||
            RayTracingJsonGetBool(inspection,
                                  "caustic_receiver_fallback_enabled",
                                  &bool_value)) {
            request.caustic_settings.surfaceReceiverFallbackEnabled = bool_value;
        }
        if (RayTracingJsonGetBool(inspection, "caustic_debug_summary", &bool_value)) {
            request.caustic_settings.debugSummaryEnabled = bool_value;
        }
        if (RayTracingJsonGetBool(inspection,
                                  "caustic_transport_debug_export_enabled",
                                  &bool_value) ||
            RayTracingJsonGetBool(inspection,
                                  "caustic_debug_export_enabled",
                                  &bool_value)) {
            request.caustic_settings.debugExportEnabled = bool_value;
        }
        if (RayTracingJsonGetBool(inspection, "caustic_sidecar_enabled", &bool_value) ||
            RayTracingJsonGetBool(inspection,
                                  "disney_v2_caustic_sidecar_enabled",
                                  &bool_value)) {
            request.has_caustic_sidecar_enabled_override = true;
            request.caustic_sidecar_enabled = bool_value;
            if (!request.has_caustic_mode_override) {
                request.caustic_mode = bool_value ? RUNTIME_DISNEY_V2_CAUSTIC_MODE_ANALYTIC
                                                  : RUNTIME_DISNEY_V2_CAUSTIC_MODE_OFF;
                request.caustic_settings.mode =
                    bool_value ? RUNTIME_CAUSTIC_MODE_ANALYTIC
                               : RUNTIME_CAUSTIC_MODE_OFF;
            }
        }
        if (RayTracingJsonGetDouble(inspection, "caustic_sidecar_strength", &double_value) ||
            RayTracingJsonGetDouble(inspection,
                                    "disney_v2_caustic_sidecar_strength",
                                    &double_value)) {
            request.has_caustic_sidecar_strength_override = true;
            request.caustic_sidecar_strength = double_value;
        }
        if (RayTracingJsonGetObject(inspection, "volume_tint", &output)) {
            if (RayTracingJsonGetDouble(output, "r", &double_value)) {
                request.has_volume_tint_override = true;
                request.volume_tint_r = double_value;
            }
            if (RayTracingJsonGetDouble(output, "g", &double_value)) {
                request.has_volume_tint_override = true;
                request.volume_tint_g = double_value;
            }
            if (RayTracingJsonGetDouble(output, "b", &double_value)) {
                request.has_volume_tint_override = true;
                request.volume_tint_b = double_value;
            }
        }
        if (agent_render_request_json_get_rgb(inspection,
                         "volume_albedo",
                         &request.volume_albedo_r,
                         &request.volume_albedo_g,
                         &request.volume_albedo_b) ||
            agent_render_request_json_get_rgb(inspection,
                         "volume_albedo_tint",
                         &request.volume_albedo_r,
                         &request.volume_albedo_g,
                         &request.volume_albedo_b)) {
            request.has_volume_albedo_override = true;
        }
        if (RayTracingJsonGetBool(inspection, "object_audit_enabled", &bool_value)) {
            request.object_audit_enabled = bool_value;
        }
        if (RayTracingJsonGetInt(inspection, "object_audit_max_dimension", &int_value)) {
            request.object_audit_max_dimension = int_value;
        }
    }

    if (!agent_render_request_finalize_loaded(&request,
                                              root,
                                              request_path,
                                              out_diagnostics,
                                              out_diagnostics_size)) {
        return false;
    }

    json_object_put(root);
    *out_request = request;
    RayTracingRequestSetDiag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}
