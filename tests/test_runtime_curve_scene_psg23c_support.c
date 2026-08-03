#include "render/runtime_dynamic_geometry_accel_3d.h"
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_mesh_blas_cache_3d.h"

#include <string.h>

void RuntimeMaterialPayload3D_Reset(RuntimeMaterialPayload3D* payload) {
    if (!payload) return;
    memset(payload, 0, sizeof(*payload));
    payload->sceneObjectIndex = -1;
    payload->materialId = -1;
}

bool RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(
    int scene_object_index,
    RuntimeMaterialPayload3D* out_payload) {
    if (!out_payload || scene_object_index < 0) return false;
    RuntimeMaterialPayload3D_Reset(out_payload);
    out_payload->valid = true;
    out_payload->sceneObjectIndex = scene_object_index;
    out_payload->materialId = 1000 + scene_object_index;
    out_payload->baseColorR = 0.18;
    out_payload->baseColorG = 0.42;
    out_payload->baseColorB = 0.09;
    return true;
}

bool RuntimeMaterialPayload3D_ResolveFromHit(
    const HitInfo3D* hit,
    RuntimeMaterialPayload3D* out_payload) {
    return hit && RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(
                      hit->sceneObjectIndex, out_payload);
}

bool RuntimeDynamicGeometryAcceleration3D_TraceWaterSurfaceFirstHit(
    const RuntimeScene3D* scene,
    const Ray3D* ray,
    double t_min,
    double t_max,
    HitInfo3D* out_hit) {
    (void)scene;
    (void)ray;
    (void)t_min;
    (void)t_max;
    (void)out_hit;
    return false;
}

bool RuntimeDynamicGeometryAcceleration3D_OwnsScenePrimitive(
    const RuntimeScene3D* scene,
    int primitive_index) {
    (void)scene;
    (void)primitive_index;
    return false;
}

bool RuntimeMeshBLASCache3D_FindAsset(
    const RayTracingRuntimeMeshAsset* asset,
    RuntimeMeshBLASCache3DView* out_view) {
    (void)asset;
    if (out_view) memset(out_view, 0, sizeof(*out_view));
    return false;
}
