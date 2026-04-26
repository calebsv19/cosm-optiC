#ifndef RENDER_RUNTIME_MATERIAL_PAYLOAD_3D_H
#define RENDER_RUNTIME_MATERIAL_PAYLOAD_3D_H

#include <stdbool.h>

#include "render/material_bsdf.h"
#include "render/runtime_ray_3d.h"

typedef struct {
    bool valid;
    int sceneObjectIndex;
    int materialId;
    MaterialBSDF bsdf;
} RuntimeMaterialPayload3D;

void RuntimeMaterialPayload3D_Reset(RuntimeMaterialPayload3D* payload);

bool RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(int scene_object_index,
                                                          RuntimeMaterialPayload3D* out_payload);

bool RuntimeMaterialPayload3D_ResolveFromHit(const HitInfo3D* hit,
                                             RuntimeMaterialPayload3D* out_payload);

#endif
