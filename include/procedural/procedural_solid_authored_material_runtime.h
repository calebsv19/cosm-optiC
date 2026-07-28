#ifndef PROCEDURAL_SOLID_AUTHORED_MATERIAL_RUNTIME_H
#define PROCEDURAL_SOLID_AUTHORED_MATERIAL_RUNTIME_H

#include "render/runtime_material_payload_3d.h"
#include "scene/object_manager.h"

bool ProceduralSolidAuthoredMaterial_ApplyHitToPayload(
    const SceneObject *object,
    const HitInfo3D *hit,
    RuntimeMaterialPayload3D *payload);

#endif
