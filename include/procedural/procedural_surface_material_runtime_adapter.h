#ifndef PROCEDURAL_SURFACE_MATERIAL_RUNTIME_ADAPTER_H
#define PROCEDURAL_SURFACE_MATERIAL_RUNTIME_ADAPTER_H

#include "procedural/procedural_surface_material.h"
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_material_texture_stack_3d.h"

#include <stdbool.h>

bool ProceduralSurfaceMaterial_ToSurfaceEval(
    const ProceduralSurfaceMaterialSample *sample,
    const RuntimeMaterialSurfaceEval *base_eval,
    RuntimeMaterialSurfaceEval *out_eval);

bool ProceduralSurfaceMaterial_ApplyToPayload(
    const ProceduralSurfaceMaterialSample *sample,
    const RuntimeMaterialSurfaceEval *base_eval,
    RuntimeMaterialPayload3D *payload);

bool ProceduralSurfaceMaterial_ApplyHitToPayload(
    const HitInfo3D *hit,
    RuntimeMaterialPayload3D *payload);

#endif
