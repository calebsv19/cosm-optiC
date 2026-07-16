#include "config/mesh_import_policy.h"

#include <stdio.h>

int ray_tracing_mesh_import_normal_mode_clamp(int mode) {
    if (mode < RAY_TRACING_MESH_IMPORT_NORMAL_MODE_NONE ||
        mode > RAY_TRACING_MESH_IMPORT_NORMAL_MODE_CREASE_AWARE) {
        return RAY_TRACING_MESH_IMPORT_NORMAL_MODE_DEFAULT;
    }
    return mode;
}

double ray_tracing_mesh_import_crease_angle_clamp(double degrees) {
    if (degrees < RAY_TRACING_MESH_IMPORT_CREASE_ANGLE_MIN ||
        degrees > RAY_TRACING_MESH_IMPORT_CREASE_ANGLE_MAX) {
        return RAY_TRACING_MESH_IMPORT_CREASE_ANGLE_DEFAULT;
    }
    return degrees;
}

void ray_tracing_mesh_import_policy_normalize(AnimationConfig *config) {
    if (!config) return;
    config->meshImportNormalMode =
        ray_tracing_mesh_import_normal_mode_clamp(config->meshImportNormalMode);
    config->meshImportCreaseAngleDegrees =
        ray_tracing_mesh_import_crease_angle_clamp(config->meshImportCreaseAngleDegrees);
}

const char *ray_tracing_mesh_import_normal_mode_name(int mode) {
    switch (ray_tracing_mesh_import_normal_mode_clamp(mode)) {
        case RAY_TRACING_MESH_IMPORT_NORMAL_MODE_NONE: return "Flat";
        case RAY_TRACING_MESH_IMPORT_NORMAL_MODE_SMOOTH: return "Smooth";
        case RAY_TRACING_MESH_IMPORT_NORMAL_MODE_CREASE_AWARE: return "Crease-aware";
        default: return "Crease-aware";
    }
}

void ray_tracing_mesh_import_policy_format_label(const AnimationConfig *config,
                                                 char *out,
                                                 size_t out_size) {
    int mode = RAY_TRACING_MESH_IMPORT_NORMAL_MODE_DEFAULT;
    double crease = RAY_TRACING_MESH_IMPORT_CREASE_ANGLE_DEFAULT;
    if (config) {
        mode = ray_tracing_mesh_import_normal_mode_clamp(config->meshImportNormalMode);
        crease = ray_tracing_mesh_import_crease_angle_clamp(
            config->meshImportCreaseAngleDegrees);
    }
    if (!out || out_size == 0u) return;
    if (mode == RAY_TRACING_MESH_IMPORT_NORMAL_MODE_CREASE_AWARE) {
        snprintf(out, out_size, "Mesh Normals: Crease %.0f deg", crease);
    } else {
        snprintf(out, out_size, "Mesh Normals: %s", ray_tracing_mesh_import_normal_mode_name(mode));
    }
    out[out_size - 1u] = '\0';
}
