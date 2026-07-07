#include "app/agent_render_request_internal.h"

bool agent_render_request_json_get_rgb(json_object *owner,
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
        return RayTracingJsonGetDouble(obj, "r", out_r) &&
               RayTracingJsonGetDouble(obj, "g", out_g) &&
               RayTracingJsonGetDouble(obj, "b", out_b);
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

int agent_render_request_parse_volume_source_kind(const char *kind_label, const char *path) {
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

RayTracing3DIntegratorId agent_render_request_parse_integrator_3d(const char *label) {
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

int agent_render_request_parse_inspection_preset(const char *label) {
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

bool agent_render_request_parse_trace_route(const char* label, RuntimeRay3DTraceRoute* out_route) {
    if (!label || !label[0] || !out_route) return false;
    if (strcmp(label, "flattened_bvh") == 0 ||
        strcmp(label, "flattened") == 0 ||
        strcmp(label, "bvh") == 0) {
        *out_route = RUNTIME_RAY_3D_TRACE_ROUTE_FLATTENED_BVH;
        return true;
    }
    if (strcmp(label, "tlas_blas_parity") == 0 ||
        strcmp(label, "parity") == 0 ||
        strcmp(label, "blas_tlas_parity") == 0) {
        *out_route = RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS_PARITY;
        return true;
    }
    if (strcmp(label, "tlas_blas") == 0 ||
        strcmp(label, "blas_tlas") == 0 ||
        strcmp(label, "accelerated") == 0) {
        *out_route = RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS;
        return true;
    }
    return false;
}

RuntimeDisneyV2CausticMode3D agent_render_request_caustic_mode_to_disney_v2_mode(
    RuntimeCausticMode3D mode) {
    switch (mode) {
        case RUNTIME_CAUSTIC_MODE_ANALYTIC:
            return RUNTIME_DISNEY_V2_CAUSTIC_MODE_ANALYTIC;
        case RUNTIME_CAUSTIC_MODE_TRANSPORT:
            return RUNTIME_DISNEY_V2_CAUSTIC_MODE_TRANSPORT;
        case RUNTIME_CAUSTIC_MODE_OFF:
        case RUNTIME_CAUSTIC_MODE_SPATIAL_CACHE:
        default:
            return RUNTIME_DISNEY_V2_CAUSTIC_MODE_OFF;
    }
}

int agent_render_request_parse_environment_light_mode(const char *label) {
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

int agent_render_request_parse_environment_preset(const char *label) {
    return RuntimeEnvironment3DPresetFromLabel(label);
}

void agent_render_request_set_diagf(char *out, size_t out_size, const char *format, ...) {
    va_list args;
    if (!out || out_size == 0u || !format) return;
    va_start(args, format);
    vsnprintf(out, out_size, format, args);
    va_end(args);
}

int agent_render_request_clamp_secondary_diffuse_samples_3d_override(int value) {
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

int agent_render_request_clamp_transmission_samples_3d_override(int value) {
    if (value < RUNTIME_3D_TRANSMISSION_SAMPLES_MIN) {
        value = RUNTIME_3D_TRANSMISSION_SAMPLES_MIN;
    }
    if (value > RUNTIME_3D_TRANSMISSION_SAMPLES_MAX) {
        value = RUNTIME_3D_TRANSMISSION_SAMPLES_MAX;
    }
    return value;
}
