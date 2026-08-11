#ifndef RENDER_RUNTIME_SCENE_CURVE_3D_H
#define RENDER_RUNTIME_SCENE_CURVE_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_curve_blas_3d.h"
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_scene_3d.h"

#define RUNTIME_CURVE_SCENE_3D_MAX_ASSET_ID 64

typedef struct RuntimeCurveSceneInstance3DDescriptor {
    const char* assetId;
    const char* objectId;
    int sceneObjectIndex;
    Vec3 position;
    Vec3 rotation;
    double uniformScale;
} RuntimeCurveSceneInstance3DDescriptor;

struct RuntimeCurveSceneInstance3D {
    char assetId[RUNTIME_CURVE_SCENE_3D_MAX_ASSET_ID];
    RuntimePrimitive3DSourceRef source;
    RuntimeCurveAsset3D asset;
    Vec3 position;
    Vec3 rotation;
    double uniformScale;
};

bool RuntimeScene3D_AddCurveInstance(
    RuntimeScene3D* scene,
    const RuntimeCurveAsset3D* asset,
    const RuntimeCurveSceneInstance3DDescriptor* descriptor);
void RuntimeScene3D_ClearCurveInstances(RuntimeScene3D* scene);
bool RuntimeScene3D_CopyCurveInstances(RuntimeScene3D* dst,
                                       const RuntimeScene3D* src);

bool RuntimeSceneCurve3D_InstanceWorldBounds(
    const RuntimeCurveSceneInstance3D* instance,
    Vec3* out_min,
    Vec3* out_max);
bool RuntimeSceneCurve3D_TraceInstance(
    const RuntimeCurveSceneInstance3D* instance,
    int curve_scene_instance_index,
    const Ray3D* world_ray,
    double t_min,
    double t_max,
    HitInfo3D* out_hit);
bool RuntimeSceneCurve3D_TraceAllInstances(
    const RuntimeScene3D* scene,
    const Ray3D* world_ray,
    double t_min,
    double t_max,
    HitInfo3D* out_hit);
bool RuntimeSceneCurve3D_ResolveMaterial(
    const HitInfo3D* curve_hit,
    RuntimeMaterialPayload3D* out_payload);
uint64_t RuntimeSceneCurve3D_GeometrySignature(const RuntimeScene3D* scene);

#endif
