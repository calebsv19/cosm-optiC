#ifndef RENDER_RUNTIME_SPECULAR_REFLECTION_3D_H
#define RENDER_RUNTIME_SPECULAR_REFLECTION_3D_H

#include <stdbool.h>

#include "render/runtime_light_emitter_3d.h"
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_native_3d_sampling.h"

typedef struct {
    bool traced;
    bool geometryHit;
    bool emitterHit;
    bool emitterWins;
    Ray3D ray;
    HitInfo3D hitInfo;
    RuntimeLightEmitterHit3DResult emitterHitInfo;
    double weight;
    double tintR;
    double tintG;
    double tintB;
} RuntimeSpecularReflection3DResult;

bool RuntimeSpecularReflection3D_Trace(const RuntimeScene3D* scene,
                                       const HitInfo3D* hit,
                                       const RuntimeMaterialPayload3D* payload,
                                       Vec3 view_dir,
                                       const RuntimeNative3DSamplingContext* sampling,
                                       RuntimeSpecularReflection3DResult* out_result);

#endif
