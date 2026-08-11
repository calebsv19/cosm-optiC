#include "render/runtime_scene_3d_builder.h"

#include "render/runtime_scene_curve_3d.h"

bool RuntimeScene3DBuilder_AppendCurveAssetSet(
    RuntimeScene3D *scene,
    const RayTracingRuntimeCurveAssetSet *curve_assets) {
    if (!scene || !curve_assets) return false;
    for (int i = 0; i < curve_assets->instance_count; ++i) {
        const RayTracingRuntimeCurveAssetInstance *instance =
            &curve_assets->instances[i];
        RuntimeCurveSceneInstance3DDescriptor descriptor;
        if (instance->asset_index < 0 ||
            instance->asset_index >= curve_assets->asset_count) {
            return false;
        }
        descriptor.assetId = instance->asset_id;
        descriptor.objectId = instance->object_id;
        descriptor.sceneObjectIndex = instance->scene_object_index;
        descriptor.position = vec3(instance->position_x,
                                   instance->position_y,
                                   instance->position_z);
        descriptor.rotation = vec3(instance->rotation_x,
                                   instance->rotation_y,
                                   instance->rotation_z);
        descriptor.uniformScale = instance->uniform_scale;
        if (!RuntimeScene3D_AddCurveInstance(
                scene,
                &curve_assets->assets[instance->asset_index].asset,
                &descriptor)) {
            return false;
        }
    }
    return true;
}
