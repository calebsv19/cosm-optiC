#include "render/runtime_material_payload_3d.h"

#include <string.h>

void RuntimeMaterialPayload3D_Reset(RuntimeMaterialPayload3D* payload) {
    if (!payload) return;
    memset(payload, 0, sizeof(*payload));
}

bool RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(
    int scene_object_index,
    RuntimeMaterialPayload3D* out_payload) {
    (void)scene_object_index;
    RuntimeMaterialPayload3D_Reset(out_payload);
    return false;
}

bool RuntimeMaterialPayload3D_ResolveFromHit(const HitInfo3D* hit,
                                             RuntimeMaterialPayload3D* out_payload) {
    (void)hit;
    RuntimeMaterialPayload3D_Reset(out_payload);
    return false;
}
