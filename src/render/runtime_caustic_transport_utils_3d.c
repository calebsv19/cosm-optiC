#include "render/runtime_caustic_transport_internal_3d.h"

#include <math.h>

double runtime_caustic_transport_clamp(double value,
                                       double min_value,
                                       double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

double runtime_caustic_transport_luma(Vec3 rgb) {
    return 0.2126 * rgb.x + 0.7152 * rgb.y + 0.0722 * rgb.z;
}

double runtime_caustic_transport_analytic_sphere_lens_sample_weight(int path_budget,
                                                                    int sample_count) {
    if (path_budget <= 0 || sample_count <= 0) return 0.0;
    return (double)path_budget /
           ((double)sample_count * (double)RUNTIME_CAUSTIC_TRANSPORT_TRIANGLE_SAMPLE_COUNT);
}

Vec3 runtime_caustic_transport_triangle_sample_point(const RuntimeTriangle3D* triangle,
                                                     int sample_index) {
    static const double barycentric[RUNTIME_CAUSTIC_TRANSPORT_TRIANGLE_SAMPLE_COUNT][3] = {
        {1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0},
        {0.60, 0.20, 0.20},
        {0.20, 0.60, 0.20},
        {0.20, 0.20, 0.60},
        {0.45, 0.45, 0.10}
    };
    const double* b = NULL;
    if (!triangle) return vec3(0.0, 0.0, 0.0);
    if (sample_index < 0 || sample_index >= RUNTIME_CAUSTIC_TRANSPORT_TRIANGLE_SAMPLE_COUNT) {
        sample_index = 0;
    }
    b = barycentric[sample_index];
    return vec3_add(vec3_add(vec3_scale(triangle->p0, b[0]),
                             vec3_scale(triangle->p1, b[1])),
                    vec3_scale(triangle->p2, b[2]));
}

double runtime_caustic_transport_light_attenuation(
    const RuntimeLightSource3D* light,
    double distance_to_target) {
    double falloff = light ? light->falloffDistance : 1.0;
    double d = fmax(distance_to_target, 1.0e-4);
    if (!light) return 0.0;
    if (!(falloff > 1.0e-6)) falloff = 1.0;
    switch (light->falloffMode) {
        case FORWARD_FALLOFF_MODE_NONE:
            return light->intensity;
        case FORWARD_FALLOFF_MODE_LINEAR:
            return light->intensity / (1.0 + (d / falloff));
        case FORWARD_FALLOFF_MODE_QUADRATIC:
        default: {
            double nd = d / falloff;
            return light->intensity / (1.0 + nd * nd);
        }
    }
}

const char* runtime_caustic_transport_light_kind_label(RuntimeLightSource3DKind kind) {
    switch (kind) {
        case RUNTIME_LIGHT_SOURCE_3D_KIND_POINT:
            return "point";
        case RUNTIME_LIGHT_SOURCE_3D_KIND_SPHERE:
            return "sphere";
        case RUNTIME_LIGHT_SOURCE_3D_KIND_DISK:
            return "disk";
        case RUNTIME_LIGHT_SOURCE_3D_KIND_RECT:
            return "rect";
        case RUNTIME_LIGHT_SOURCE_3D_KIND_MESH_EMISSIVE:
            return "mesh_emissive";
        default:
            return "unknown";
    }
}
