#ifndef RENDER_RUNTIME_MIRROR_COMPOSITION_3D_H
#define RENDER_RUNTIME_MIRROR_COMPOSITION_3D_H

#include <stdbool.h>

#include "render/runtime_material_payload_3d.h"

typedef struct {
    bool active;
    double dominance;
    double baseAttenuation;
} RuntimeMirrorComposition3DPolicy;

static inline double RuntimeMirrorComposition3D_Clamp(double value,
                                                      double min_value,
                                                      double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static inline RuntimeMirrorComposition3DPolicy RuntimeMirrorComposition3D_Evaluate(
    const RuntimeMaterialPayload3D* payload) {
    RuntimeMirrorComposition3DPolicy policy = {0};
    double reflectivity = 0.0;
    double specular_weight = 0.0;
    double roughness = 1.0;
    double polish = 0.0;

    policy.baseAttenuation = 1.0;
    if (!payload || !payload->valid || payload->transparency > 1e-6) {
        return policy;
    }

    reflectivity = RuntimeMirrorComposition3D_Clamp(payload->bsdf.reflectivity, 0.0, 1.0);
    specular_weight = RuntimeMirrorComposition3D_Clamp(payload->bsdf.specWeight, 0.0, 1.0);
    roughness = RuntimeMirrorComposition3D_Clamp(payload->bsdf.roughness, 0.0, 1.0);
    polish = RuntimeMirrorComposition3D_Clamp((0.85 - roughness) / 0.83, 0.0, 1.0);

    policy.dominance = RuntimeMirrorComposition3D_Clamp(
        reflectivity * (0.25 + (0.75 * specular_weight)) * polish,
        0.0,
        1.0);
    policy.baseAttenuation = RuntimeMirrorComposition3D_Clamp(1.0 - policy.dominance,
                                                              0.0,
                                                              1.0);
    policy.active = policy.dominance > 0.05;
    return policy;
}

#endif
