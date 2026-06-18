#include "app/agent_render_request.h"

#include <json-c/json.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "import/fluid_volume_import_3d.h"
#include "render/runtime_scene_3d.h"

static void diag_set(char *out, size_t out_size, const char *message) {
    if (!out || out_size == 0u || !message) return;
    snprintf(out, out_size, "%s", message);
}

static bool copy_string(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0u || !src) return false;
    if (snprintf(dst, dst_size, "%s", src) >= (int)dst_size) {
        dst[0] = '\0';
        return false;
    }
    return true;
}

static bool read_text_file(const char *path, char **out_text) {
    FILE *file = NULL;
    long size = 0;
    char *text = NULL;
    size_t read_count = 0u;

    if (!path || !path[0] || !out_text) return false;
    *out_text = NULL;
    file = fopen(path, "rb");
    if (!file) return false;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }
    size = ftell(file);
    if (size <= 0) {
        fclose(file);
        return false;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }
    text = (char *)malloc((size_t)size + 1u);
    if (!text) {
        fclose(file);
        return false;
    }
    read_count = fread(text, 1u, (size_t)size, file);
    fclose(file);
    if (read_count != (size_t)size) {
        free(text);
        return false;
    }
    text[size] = '\0';
    *out_text = text;
    return true;
}

static void dirname_of(const char *path, char *out_dir, size_t out_dir_size) {
    const char *slash = NULL;
    size_t len = 0u;
    if (!out_dir || out_dir_size == 0u) return;
    out_dir[0] = '\0';
    if (!path || !path[0]) return;
    slash = strrchr(path, '/');
    if (!slash) {
        snprintf(out_dir, out_dir_size, ".");
        return;
    }
    len = (size_t)(slash - path);
    if (len == 0u) {
        snprintf(out_dir, out_dir_size, "/");
        return;
    }
    if (len >= out_dir_size) len = out_dir_size - 1u;
    memcpy(out_dir, path, len);
    out_dir[len] = '\0';
}

static bool path_is_absolute(const char *path) {
    return path && path[0] == '/';
}

static bool resolve_request_path(const char *request_dir,
                                 const char *path,
                                 char *out_path,
                                 size_t out_path_size) {
    if (!path || !path[0] || !out_path || out_path_size == 0u) return false;
    if (path_is_absolute(path)) {
        return copy_string(out_path, out_path_size, path);
    }
    if (!request_dir || !request_dir[0] || strcmp(request_dir, ".") == 0) {
        return copy_string(out_path, out_path_size, path);
    }
    if (snprintf(out_path, out_path_size, "%s/%s", request_dir, path) >= (int)out_path_size) {
        out_path[0] = '\0';
        return false;
    }
    return true;
}

static bool json_get_object(json_object *owner, const char *key, json_object **out_obj) {
    json_object *obj = NULL;
    if (out_obj) *out_obj = NULL;
    if (!owner || !key || !json_object_object_get_ex(owner, key, &obj) ||
        !json_object_is_type(obj, json_type_object)) {
        return false;
    }
    if (out_obj) *out_obj = obj;
    return true;
}

static bool json_get_string(json_object *owner, const char *key, const char **out_value) {
    json_object *obj = NULL;
    if (out_value) *out_value = NULL;
    if (!owner || !key || !json_object_object_get_ex(owner, key, &obj) ||
        !json_object_is_type(obj, json_type_string)) {
        return false;
    }
    if (out_value) *out_value = json_object_get_string(obj);
    return true;
}

static bool json_get_int(json_object *owner, const char *key, int *out_value) {
    json_object *obj = NULL;
    if (out_value) *out_value = 0;
    if (!owner || !key || !json_object_object_get_ex(owner, key, &obj) ||
        (!json_object_is_type(obj, json_type_int) &&
         !json_object_is_type(obj, json_type_double))) {
        return false;
    }
    if (out_value) *out_value = json_object_get_int(obj);
    return true;
}

static bool json_get_double(json_object *owner, const char *key, double *out_value) {
    json_object *obj = NULL;
    if (out_value) *out_value = 0.0;
    if (!owner || !key || !json_object_object_get_ex(owner, key, &obj) ||
        (!json_object_is_type(obj, json_type_int) &&
         !json_object_is_type(obj, json_type_double))) {
        return false;
    }
    if (out_value) *out_value = json_object_get_double(obj);
    return true;
}

static bool json_get_bool(json_object *owner, const char *key, bool *out_value) {
    json_object *obj = NULL;
    if (out_value) *out_value = false;
    if (!owner || !key || !json_object_object_get_ex(owner, key, &obj) ||
        !json_object_is_type(obj, json_type_boolean)) {
        return false;
    }
    if (out_value) *out_value = json_object_get_boolean(obj) != 0;
    return true;
}

static bool json_get_rgb(json_object *owner,
                         const char *key,
                         double *out_r,
                         double *out_g,
                         double *out_b) {
    json_object *obj = NULL;
    if (out_r) *out_r = 0.0;
    if (out_g) *out_g = 0.0;
    if (out_b) *out_b = 0.0;
    if (!owner || !key || !out_r || !out_g || !out_b ||
        !json_object_object_get_ex(owner, key, &obj)) {
        return false;
    }
    if (json_object_is_type(obj, json_type_object)) {
        return json_get_double(obj, "r", out_r) &&
               json_get_double(obj, "g", out_g) &&
               json_get_double(obj, "b", out_b);
    }
    if (json_object_is_type(obj, json_type_array) &&
        json_object_array_length(obj) >= 3u) {
        *out_r = json_object_get_double(json_object_array_get_idx(obj, 0));
        *out_g = json_object_get_double(json_object_array_get_idx(obj, 1));
        *out_b = json_object_get_double(json_object_array_get_idx(obj, 2));
        return true;
    }
    return false;
}

static int volume_kind_from_runtime(RuntimeVolume3DSourceKind kind) {
    switch (kind) {
        case RUNTIME_VOLUME_3D_SOURCE_MANIFEST:
            return VOLUME_SOURCE_MANIFEST;
        case RUNTIME_VOLUME_3D_SOURCE_RAW_VF3D:
            return VOLUME_SOURCE_RAW_VF3D;
        case RUNTIME_VOLUME_3D_SOURCE_PACK:
            return VOLUME_SOURCE_PACK;
        case RUNTIME_VOLUME_3D_SOURCE_NONE:
        default:
            return VOLUME_SOURCE_NONE;
    }
}

static int parse_volume_source_kind(const char *kind_label, const char *path) {
    RuntimeVolume3DSourceKind runtime_kind = RUNTIME_VOLUME_3D_SOURCE_NONE;
    if (!kind_label || !kind_label[0] || strcmp(kind_label, "auto") == 0) {
        if (fluid_volume_import_3d_classify_path(path, &runtime_kind)) {
            return volume_kind_from_runtime(runtime_kind);
        }
        return VOLUME_SOURCE_NONE;
    }
    if (strcmp(kind_label, "manifest") == 0 || strcmp(kind_label, "scene_bundle") == 0) {
        return VOLUME_SOURCE_MANIFEST;
    }
    if (strcmp(kind_label, "raw_vf3d") == 0 || strcmp(kind_label, "vf3d") == 0) {
        return VOLUME_SOURCE_RAW_VF3D;
    }
    if (strcmp(kind_label, "pack") == 0) {
        return VOLUME_SOURCE_PACK;
    }
    if (strcmp(kind_label, "none") == 0) {
        return VOLUME_SOURCE_NONE;
    }
    return VOLUME_SOURCE_NONE;
}

static RayTracing3DIntegratorId parse_integrator_3d(const char *label) {
    if (!label || !label[0] || strcmp(label, "direct_light") == 0) {
        return RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT;
    }
    if (strcmp(label, "diffuse_bounce") == 0) {
        return RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE;
    }
    if (strcmp(label, "material") == 0) {
        return RAY_TRACING_3D_INTEGRATOR_MATERIAL;
    }
    if (strcmp(label, "emission_transparency") == 0) {
        return RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY;
    }
    if (strcmp(label, "disney") == 0) {
        return RAY_TRACING_3D_INTEGRATOR_DISNEY;
    }
    if (strcmp(label, "disney_v2") == 0) {
        return RAY_TRACING_3D_INTEGRATOR_DISNEY_V2;
    }
    return RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT;
}

enum {
    RAY_TRACING_AGENT_RENDER_PRESET_NONE = 0,
    RAY_TRACING_AGENT_RENDER_PRESET_GLASS_PREVIEW = 1,
    RAY_TRACING_AGENT_RENDER_PRESET_GLASS_REVIEW = 2
};

static int parse_inspection_preset(const char *label) {
    if (!label || !label[0] || strcmp(label, "none") == 0) {
        return RAY_TRACING_AGENT_RENDER_PRESET_NONE;
    }
    if (strcmp(label, "glass_preview") == 0) {
        return RAY_TRACING_AGENT_RENDER_PRESET_GLASS_PREVIEW;
    }
    if (strcmp(label, "glass_review") == 0) {
        return RAY_TRACING_AGENT_RENDER_PRESET_GLASS_REVIEW;
    }
    return RAY_TRACING_AGENT_RENDER_PRESET_NONE;
}

static int parse_environment_light_mode(const char *label) {
    if (!label || !label[0] || strcmp(label, "off") == 0) {
        return ENVIRONMENT_LIGHT_MODE_OFF;
    }
    if (strcmp(label, "top_fill") == 0 || strcmp(label, "top-fill") == 0) {
        return ENVIRONMENT_LIGHT_MODE_TOP_FILL;
    }
    if (strcmp(label, "ambient") == 0) {
        return ENVIRONMENT_LIGHT_MODE_AMBIENT;
    }
    return ENVIRONMENT_LIGHT_MODE_OFF;
}

static int parse_environment_preset(const char *label) {
    return RuntimeEnvironment3DPresetFromLabel(label);
}

static int clamp_secondary_diffuse_samples_3d_override(int value) {
    if (value < RUNTIME_3D_SECONDARY_SAMPLES_MIN) {
        value = RUNTIME_3D_SECONDARY_SAMPLES_MIN;
    }
    if (value > RUNTIME_3D_SECONDARY_SAMPLES_MAX) {
        value = RUNTIME_3D_SECONDARY_SAMPLES_MAX;
    }
    value = ((value + (RUNTIME_3D_SECONDARY_SAMPLES_STEP / 2)) /
             RUNTIME_3D_SECONDARY_SAMPLES_STEP) *
            RUNTIME_3D_SECONDARY_SAMPLES_STEP;
    if (value < RUNTIME_3D_SECONDARY_SAMPLES_MIN) {
        value = RUNTIME_3D_SECONDARY_SAMPLES_MIN;
    }
    if (value > RUNTIME_3D_SECONDARY_SAMPLES_MAX) {
        value = RUNTIME_3D_SECONDARY_SAMPLES_MAX;
    }
    return value;
}

static int clamp_transmission_samples_3d_override(int value) {
    if (value < RUNTIME_3D_TRANSMISSION_SAMPLES_MIN) {
        value = RUNTIME_3D_TRANSMISSION_SAMPLES_MIN;
    }
    if (value > RUNTIME_3D_TRANSMISSION_SAMPLES_MAX) {
        value = RUNTIME_3D_TRANSMISSION_SAMPLES_MAX;
    }
    return value;
}

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
    request->volume_step_scale_override = 1.0;
    request->secondary_diffuse_samples_3d_override = RUNTIME_3D_SECONDARY_SAMPLES_DEFAULT;
    request->transmission_samples_3d_override = RUNTIME_3D_TRANSMISSION_SAMPLES_DEFAULT;
    request->volume_tint_r = 1.0;
    request->volume_tint_g = 1.0;
    request->volume_tint_b = 1.0;
    request->object_audit_enabled = true;
    request->object_audit_max_dimension = 160;
    request->overwrite = false;
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
    json_object *sampling = NULL;
    const char *value = NULL;
    int int_value = 0;
    double double_value = 0.0;
    bool bool_value = false;

    diag_set(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!request_path || !request_path[0] || !out_request) return false;

    ray_tracing_agent_render_request_defaults(&request);
    dirname_of(request_path, request_dir, sizeof(request_dir));
    if (!read_text_file(request_path, &text)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to read request file");
        return false;
    }

    root = json_tokener_parse(text);
    free(text);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        diag_set(out_diagnostics, out_diagnostics_size, "failed to parse request json");
        return false;
    }

    if (!json_get_string(root, "schema_version", &value) ||
        strcmp(value, RAY_TRACING_AGENT_RENDER_REQUEST_SCHEMA) != 0) {
        json_object_put(root);
        diag_set(out_diagnostics, out_diagnostics_size, "schema_version mismatch");
        return false;
    }

    if (json_get_string(root, "run_id", &value) &&
        !copy_string(request.run_id, sizeof(request.run_id), value)) {
        json_object_put(root);
        diag_set(out_diagnostics, out_diagnostics_size, "run_id too long");
        return false;
    }

    if (!json_get_object(root, "scene", &scene) ||
        !json_get_string(scene, "runtime_scene_path", &value) ||
        !resolve_request_path(request_dir,
                              value,
                              request.runtime_scene_path,
                              sizeof(request.runtime_scene_path))) {
        json_object_put(root);
        diag_set(out_diagnostics, out_diagnostics_size, "scene.runtime_scene_path missing or invalid");
        return false;
    }

    if (json_get_object(root, "volume", &volume)) {
        const char *kind_label = "auto";
        json_get_bool(volume, "enabled", &request.volume_enabled);
        if (json_get_string(volume, "source_path", &value)) {
            if (!resolve_request_path(request_dir,
                                      value,
                                      request.volume_source_path,
                                      sizeof(request.volume_source_path))) {
                json_object_put(root);
                diag_set(out_diagnostics, out_diagnostics_size, "volume.source_path invalid");
                return false;
            }
            request.volume_enabled = true;
        }
        if (json_get_string(volume, "source_kind", &value)) {
            kind_label = value;
        }
        request.volume_source_kind =
            parse_volume_source_kind(kind_label, request.volume_source_path);
        json_get_bool(volume, "affects_lighting", &request.volume_affects_lighting);
        json_get_bool(volume, "debug_overlay", &request.volume_debug_overlay);
        if (request.volume_enabled &&
            (request.volume_source_kind == VOLUME_SOURCE_NONE ||
             request.volume_source_path[0] == '\0')) {
            json_object_put(root);
            diag_set(out_diagnostics, out_diagnostics_size, "volume enabled without supported source");
            return false;
        }
    }

    if (json_get_object(root, "render", &render)) {
        if (json_get_int(render, "start_frame", &int_value)) request.start_frame = int_value;
        if (json_get_int(render, "frame_count", &int_value)) request.frame_count = int_value;
        if (json_get_int(render, "width", &int_value)) request.width = int_value;
        if (json_get_int(render, "height", &int_value)) request.height = int_value;
        if (json_get_double(render, "normalized_t", &double_value)) {
            request.normalized_t = double_value;
        }
        if (json_get_int(render, "temporal_frames", &int_value)) {
            request.temporal_frames = int_value;
        }
        if (json_get_bool(render, "denoise_enabled", &bool_value)) {
            request.has_denoise_enabled_override = true;
            request.denoise_enabled_override = bool_value;
        }
        if (json_get_string(render, "integrator_3d", &value)) {
            request.integrator_3d = parse_integrator_3d(value);
            request.has_integrator_3d_override = true;
        }
    }

    if (json_get_object(root, "sampling", &sampling)) {
        bool has_offset = false;
        bool has_count = false;
        if (json_get_int(sampling, "frame_offset", &int_value)) {
            request.has_sampling_window = true;
            request.sampling_frame_offset = int_value;
            has_offset = true;
        }
        if (json_get_int(sampling, "frame_count", &int_value)) {
            request.has_sampling_window = true;
            request.sampling_frame_count = int_value;
            has_count = true;
        }
        if (request.has_sampling_window && (!has_offset || !has_count)) {
            json_object_put(root);
            diag_set(out_diagnostics, out_diagnostics_size, "sampling.frame_offset and sampling.frame_count required together");
            return false;
        }
    }

    if (json_get_object(root, "output", &output)) {
        json_object *video = NULL;
        if (json_get_string(output, "root", &value) &&
            !resolve_request_path(request_dir, value, request.output_root, sizeof(request.output_root))) {
            json_object_put(root);
            diag_set(out_diagnostics, out_diagnostics_size, "output.root invalid");
            return false;
        }
        json_get_bool(output, "overwrite", &request.overwrite);
        if (json_get_object(output, "video", &video)) {
            json_get_bool(video, "enabled", &request.video_enabled);
            if (json_get_string(video, "path", &value)) {
                if (!resolve_request_path(request_dir,
                                          value,
                                          request.video_path,
                                          sizeof(request.video_path))) {
                    json_object_put(root);
                    diag_set(out_diagnostics, out_diagnostics_size, "output.video.path invalid");
                    return false;
                }
                request.video_enabled = true;
            }
            if (json_get_int(video, "fps", &int_value)) {
                request.video_fps = int_value;
            }
            if (request.video_enabled && !request.video_path[0]) {
                json_object_put(root);
                diag_set(out_diagnostics, out_diagnostics_size, "output.video.path required when video is enabled");
                return false;
            }
            if (request.video_fps <= 0) {
                request.video_fps = 30;
            }
        }
    }

    if (json_get_object(root, "progress", &progress)) {
        if (json_get_string(progress, "summary_path", &value) &&
            !resolve_request_path(request_dir,
                                  value,
                                  request.summary_path,
                                  sizeof(request.summary_path))) {
            json_object_put(root);
            diag_set(out_diagnostics, out_diagnostics_size, "progress.summary_path invalid");
            return false;
        }
        if (json_get_string(progress, "progress_path", &value) &&
            !resolve_request_path(request_dir,
                                  value,
                                  request.progress_path,
                                  sizeof(request.progress_path))) {
            json_object_put(root);
            diag_set(out_diagnostics, out_diagnostics_size, "progress.progress_path invalid");
            return false;
        }
    }

    if (json_get_object(root, "inspection", &inspection)) {
        if (json_get_string(inspection, "preset", &value)) {
            request.inspection_preset = parse_inspection_preset(value);
        }
        if (json_get_double(inspection, "camera_zoom", &double_value)) {
            request.has_camera_zoom_override = true;
            request.camera_zoom_override = double_value;
        }
        if (json_get_object(inspection, "camera_position", &output)) {
            if (json_get_double(output, "x", &double_value)) {
                request.has_camera_position_override = true;
                request.camera_position_x = double_value;
            }
            if (json_get_double(output, "y", &double_value)) {
                request.has_camera_position_override = true;
                request.camera_position_y = double_value;
            }
            if (json_get_double(output, "z", &double_value)) {
                request.has_camera_position_override = true;
                request.camera_position_z = double_value;
            }
        }
        if (json_get_object(inspection, "camera_look_at", &output)) {
            if (json_get_double(output, "x", &double_value)) {
                request.has_camera_look_at_override = true;
                request.camera_look_at_x = double_value;
            }
            if (json_get_double(output, "y", &double_value)) {
                request.has_camera_look_at_override = true;
                request.camera_look_at_y = double_value;
            }
            if (json_get_double(output, "z", &double_value)) {
                request.has_camera_look_at_override = true;
                request.camera_look_at_z = double_value;
            }
        }
        if (json_get_double(inspection, "environment_brightness", &double_value)) {
            request.has_environment_brightness_override = true;
            request.environment_brightness_override = double_value;
        }
        if (json_get_double(inspection, "ambient_strength", &double_value)) {
            request.has_ambient_strength_override = true;
            request.ambient_strength_override = double_value;
        }
        if (json_get_string(inspection, "environment_light_mode", &value)) {
            request.has_environment_light_mode_override = true;
            request.environment_light_mode_override = parse_environment_light_mode(value);
        }
        if (json_get_string(inspection, "environment_preset", &value)) {
            request.has_environment_preset_override = true;
            request.environment_preset_override = parse_environment_preset(value);
        }
        if (json_get_double(inspection, "background_brightness", &double_value)) {
            request.has_background_brightness_override = true;
            request.background_brightness_override = double_value;
        }
        if (json_get_rgb(inspection,
                         "background_color",
                         &request.background_color_r,
                         &request.background_color_g,
                         &request.background_color_b)) {
            request.has_background_color_override = true;
        }
        if (json_get_double(inspection, "top_fill_strength", &double_value)) {
            request.has_top_fill_strength_override = true;
            request.top_fill_strength_override = double_value;
        }
        if (json_get_double(inspection, "light_intensity", &double_value)) {
            request.has_light_intensity_override = true;
            request.light_intensity_override = double_value;
        }
        if (json_get_double(inspection, "light_radius", &double_value)) {
            request.has_light_radius_override = true;
            request.light_radius_override = double_value;
        }
        if (json_get_double(inspection, "forward_decay", &double_value)) {
            request.has_forward_decay_override = true;
            request.forward_decay_override = double_value;
        }
        if (json_get_double(inspection, "volume_scatter_gain", &double_value)) {
            request.has_volume_scatter_gain_override = true;
            request.volume_scatter_gain_override = double_value;
        }
        if (json_get_double(inspection, "volume_step_scale", &double_value)) {
            request.has_volume_step_scale_override = true;
            request.volume_step_scale_override = double_value;
        }
        if (json_get_int(inspection, "secondary_diffuse_samples_3d", &int_value)) {
            request.has_secondary_diffuse_samples_3d_override = true;
            request.secondary_diffuse_samples_3d_override = int_value;
        }
        if (json_get_int(inspection, "transmission_samples_3d", &int_value)) {
            request.has_transmission_samples_3d_override = true;
            request.transmission_samples_3d_override = int_value;
        }
        if (json_get_object(inspection, "volume_tint", &output)) {
            if (json_get_double(output, "r", &double_value)) {
                request.has_volume_tint_override = true;
                request.volume_tint_r = double_value;
            }
            if (json_get_double(output, "g", &double_value)) {
                request.has_volume_tint_override = true;
                request.volume_tint_g = double_value;
            }
            if (json_get_double(output, "b", &double_value)) {
                request.has_volume_tint_override = true;
                request.volume_tint_b = double_value;
            }
        }
        if (json_get_bool(inspection, "object_audit_enabled", &bool_value)) {
            request.object_audit_enabled = bool_value;
        }
        if (json_get_int(inspection, "object_audit_max_dimension", &int_value)) {
            request.object_audit_max_dimension = int_value;
        }
    }

    if (request.inspection_preset == RAY_TRACING_AGENT_RENDER_PRESET_GLASS_PREVIEW ||
        request.inspection_preset == RAY_TRACING_AGENT_RENDER_PRESET_GLASS_REVIEW) {
        if (!request.has_integrator_3d_override) {
            request.integrator_3d = RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY;
        }
        if (!request.has_secondary_diffuse_samples_3d_override) {
            request.has_secondary_diffuse_samples_3d_override = true;
            request.secondary_diffuse_samples_3d_override =
                (request.inspection_preset == RAY_TRACING_AGENT_RENDER_PRESET_GLASS_REVIEW) ? 24 : 8;
        }
        if (!request.has_transmission_samples_3d_override) {
            request.has_transmission_samples_3d_override = true;
            request.transmission_samples_3d_override =
                (request.inspection_preset == RAY_TRACING_AGENT_RENDER_PRESET_GLASS_REVIEW) ? 12 : 4;
        }
    }

    if (request.start_frame < 0 || request.frame_count <= 0 ||
        request.width <= 0 || request.height <= 0 ||
        request.temporal_frames <= 0) {
        json_object_put(root);
        diag_set(out_diagnostics, out_diagnostics_size, "render numeric fields out of range");
        return false;
    }
    if (request.normalized_t < 0.0) request.normalized_t = 0.0;
    if (request.normalized_t > 1.0) request.normalized_t = 1.0;
    if (request.has_sampling_window) {
        if (request.sampling_frame_offset < 0 || request.sampling_frame_count <= 0) {
            json_object_put(root);
            diag_set(out_diagnostics, out_diagnostics_size, "sampling window out of range");
            return false;
        }
    }
    if (request.has_camera_zoom_override && request.camera_zoom_override <= 0.0) {
        json_object_put(root);
        diag_set(out_diagnostics, out_diagnostics_size, "inspection.camera_zoom out of range");
        return false;
    }
    if (request.has_environment_brightness_override) {
        if (request.environment_brightness_override < 0.0) {
            request.environment_brightness_override = 0.0;
        }
        if (request.environment_brightness_override > 255.0) {
            request.environment_brightness_override = 255.0;
        }
    }
    if (request.has_ambient_strength_override) {
        if (request.ambient_strength_override < 0.0) {
            request.ambient_strength_override = 0.0;
        }
        if (request.ambient_strength_override > 1.0) {
            request.ambient_strength_override = 1.0;
        }
    }
    if (request.has_environment_light_mode_override) {
        request.environment_light_mode_override =
            animation_config_environment_light_mode_clamp(request.environment_light_mode_override);
    }
    if (request.has_environment_preset_override) {
        request.environment_preset_override =
            animation_config_environment_preset_clamp(request.environment_preset_override);
    }
    if (request.has_background_brightness_override) {
        if (!isfinite(request.background_brightness_override) ||
            request.background_brightness_override < 0.0) {
            request.background_brightness_override = 0.0;
        }
        if (request.background_brightness_override > 4.0) {
            request.background_brightness_override = 4.0;
        }
    }
    if (request.has_background_color_override) {
        if (!isfinite(request.background_color_r)) request.background_color_r = 1.0;
        if (!isfinite(request.background_color_g)) request.background_color_g = 1.0;
        if (!isfinite(request.background_color_b)) request.background_color_b = 1.0;
        if (request.background_color_r < 0.0) request.background_color_r = 0.0;
        if (request.background_color_g < 0.0) request.background_color_g = 0.0;
        if (request.background_color_b < 0.0) request.background_color_b = 0.0;
        if (request.background_color_r > 1.0) request.background_color_r = 1.0;
        if (request.background_color_g > 1.0) request.background_color_g = 1.0;
        if (request.background_color_b > 1.0) request.background_color_b = 1.0;
    }
    if (request.has_top_fill_strength_override) {
        if (request.top_fill_strength_override < 0.0) {
            request.top_fill_strength_override = 0.0;
        }
        if (request.top_fill_strength_override > 20.0) {
            request.top_fill_strength_override = 20.0;
        }
    }
    if (request.has_light_intensity_override) {
        if (request.light_intensity_override < 0.0) {
            request.light_intensity_override = 0.0;
        }
        if (request.light_intensity_override > 20.0) {
            request.light_intensity_override = 20.0;
        }
    }
    if (request.has_light_radius_override) {
        if (request.light_radius_override < 0.0) {
            request.light_radius_override = 0.0;
        }
        if (request.light_radius_override > 25.0) {
            request.light_radius_override = 25.0;
        }
    }
    if (request.has_forward_decay_override) {
        if (request.forward_decay_override < 50.0) {
            request.forward_decay_override = 50.0;
        }
        if (request.forward_decay_override > 100000.0) {
            request.forward_decay_override = 100000.0;
        }
    }
    if (request.has_volume_scatter_gain_override) {
        if (request.volume_scatter_gain_override <= 0.0) {
            json_object_put(root);
            diag_set(out_diagnostics, out_diagnostics_size, "inspection.volume_scatter_gain out of range");
            return false;
        }
        if (request.volume_scatter_gain_override > 64.0) {
            request.volume_scatter_gain_override = 64.0;
        }
    }
    if (request.has_volume_step_scale_override) {
        if (request.volume_step_scale_override <= 0.0) {
            json_object_put(root);
            diag_set(out_diagnostics, out_diagnostics_size, "inspection.volume_step_scale out of range");
            return false;
        }
        if (request.volume_step_scale_override < 0.05) {
            request.volume_step_scale_override = 0.05;
        }
        if (request.volume_step_scale_override > 4.0) {
            request.volume_step_scale_override = 4.0;
        }
    }
    if (request.has_secondary_diffuse_samples_3d_override) {
        request.secondary_diffuse_samples_3d_override =
            clamp_secondary_diffuse_samples_3d_override(
                request.secondary_diffuse_samples_3d_override);
    }
    if (request.has_transmission_samples_3d_override) {
        request.transmission_samples_3d_override =
            clamp_transmission_samples_3d_override(
                request.transmission_samples_3d_override);
    }
    if (request.has_volume_tint_override) {
        if (request.volume_tint_r < 0.0) request.volume_tint_r = 0.0;
        if (request.volume_tint_g < 0.0) request.volume_tint_g = 0.0;
        if (request.volume_tint_b < 0.0) request.volume_tint_b = 0.0;
        if (request.volume_tint_r > 8.0) request.volume_tint_r = 8.0;
        if (request.volume_tint_g > 8.0) request.volume_tint_g = 8.0;
        if (request.volume_tint_b > 8.0) request.volume_tint_b = 8.0;
        if (request.volume_tint_r <= 0.0 &&
            request.volume_tint_g <= 0.0 &&
            request.volume_tint_b <= 0.0) {
            json_object_put(root);
            diag_set(out_diagnostics, out_diagnostics_size, "inspection.volume_tint out of range");
            return false;
        }
    }
    if (request.object_audit_max_dimension < 16) {
        request.object_audit_max_dimension = 16;
    }
    if (request.object_audit_max_dimension > 2048) {
        request.object_audit_max_dimension = 2048;
    }

    if (json_get_bool(root, "overwrite", &bool_value)) {
        request.overwrite = bool_value;
    }

    json_object_put(root);
    *out_request = request;
    diag_set(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

const char *ray_tracing_agent_render_request_volume_kind_label(int kind) {
    switch (animation_config_volume_source_kind_clamp(kind)) {
        case VOLUME_SOURCE_MANIFEST:
            return "manifest";
        case VOLUME_SOURCE_RAW_VF3D:
            return "raw_vf3d";
        case VOLUME_SOURCE_PACK:
            return "pack";
        case VOLUME_SOURCE_NONE:
        default:
            return "none";
    }
}

const char *ray_tracing_agent_render_request_integrator_label(RayTracing3DIntegratorId id) {
    switch (RayTracingIntegratorCatalog_Clamp3DToShipped((int)id)) {
        case RAY_TRACING_3D_INTEGRATOR_DISNEY_V2:
            return "disney_v2";
        case RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE:
            return "diffuse_bounce";
        case RAY_TRACING_3D_INTEGRATOR_MATERIAL:
            return "material";
        case RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY:
            return "emission_transparency";
        case RAY_TRACING_3D_INTEGRATOR_DISNEY:
            return "disney";
        case RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT:
        default:
            return "direct_light";
    }
}

const char *ray_tracing_agent_render_request_inspection_preset_label(int preset) {
    switch (preset) {
        case RAY_TRACING_AGENT_RENDER_PRESET_GLASS_REVIEW:
            return "glass_review";
        case RAY_TRACING_AGENT_RENDER_PRESET_GLASS_PREVIEW:
            return "glass_preview";
        case RAY_TRACING_AGENT_RENDER_PRESET_NONE:
        default:
            return "none";
    }
}
