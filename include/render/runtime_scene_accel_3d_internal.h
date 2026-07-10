#ifndef RENDER_RUNTIME_SCENE_ACCEL_3D_INTERNAL_H
#define RENDER_RUNTIME_SCENE_ACCEL_3D_INTERNAL_H

#include "render/runtime_scene_accel_3d.h"

typedef struct RuntimeSceneAcceleration3DInstanceBounds {
    Vec3 min;
    Vec3 max;
    int primitiveIndex;
    int sceneObjectIndex;
    char objectId[RUNTIME_SCENE_3D_MAX_OBJECT_ID];
    bool meshAccelerated;
    int assetIndex;
    char assetId[64];
    char assetPath[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX];
    Vec3 position;
    Vec3 rotation;
    Vec3 scale;
    Vec3 pivotScaled;
    const RuntimeTriangleMesh3D* localMesh;
    int* sceneTriangleIndexByLocalTriangle;
    int sceneTriangleIndexByLocalTriangleCount;
} RuntimeSceneAcceleration3DInstanceBounds;

typedef void (*RuntimeSceneAcceleration3DDiagnosticsFn)(const char* message);

void RuntimeSceneAcceleration3D_FreeInstanceIdentityMaps(
    RuntimeSceneAcceleration3DInstanceBounds* instances,
    int instance_count);
bool RuntimeSceneAcceleration3D_CaptureInstanceBounds(
    const RuntimeScene3D* scene,
    RuntimeSceneAcceleration3DInstanceBounds** out_instances,
    int* out_instance_count,
    RuntimeSceneAcceleration3DDiagnosticsFn set_diag);
bool RuntimeSceneAcceleration3D_ApplyMeshAssetRecords(
    const RuntimeScene3D* scene,
    const RayTracingRuntimeMeshAssetSet* mesh_assets,
    RuntimeSceneAcceleration3DInstanceBounds* instances,
    int instance_count);
Vec3 RuntimeSceneAcceleration3D_TransformPoint(
    const RuntimeSceneAcceleration3DInstanceBounds* instance,
    Vec3 local_point);
Vec3 RuntimeSceneAcceleration3D_InverseTransformPoint(
    const RuntimeSceneAcceleration3DInstanceBounds* instance,
    Vec3 world_point);
Vec3 RuntimeSceneAcceleration3D_InverseTransformDirection(
    const RuntimeSceneAcceleration3DInstanceBounds* instance,
    Vec3 world_direction);

#endif
