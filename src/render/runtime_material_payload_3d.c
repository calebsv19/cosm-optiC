#include "render/runtime_material_payload_3d.h"

#include <string.h>

#include "config/config_manager.h"
#include "material/material_manager.h"

static bool runtime_material_payload_3d_valid_scene_object_index(int scene_object_index) {
    return scene_object_index >= 0 &&
           scene_object_index < MAX_OBJECTS &&
           scene_object_index < sceneSettings.objectCount;
}

static int runtime_material_payload_3d_clamp_material_id(int material_id) {
    int material_count = MaterialManagerCount();
    int default_id = MaterialManagerDefaultId();

    if (material_count <= 0) {
        return default_id;
    }
    if (material_id < 0 || material_id >= material_count) {
        return default_id;
    }
    return material_id;
}

void RuntimeMaterialPayload3D_Reset(RuntimeMaterialPayload3D* payload) {
    if (!payload) return;
    memset(payload, 0, sizeof(*payload));
    payload->sceneObjectIndex = -1;
    payload->materialId = -1;
}

bool RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(int scene_object_index,
                                                          RuntimeMaterialPayload3D* out_payload) {
    RuntimeMaterialPayload3D payload = {0};
    SceneObject object_copy;
    const Material* material = NULL;

    if (!out_payload) return false;
    RuntimeMaterialPayload3D_Reset(out_payload);
    if (!runtime_material_payload_3d_valid_scene_object_index(scene_object_index)) {
        return false;
    }

    object_copy = sceneSettings.sceneObjects[scene_object_index];
    object_copy.material_id = runtime_material_payload_3d_clamp_material_id(object_copy.material_id);

    RuntimeMaterialPayload3D_Reset(&payload);
    payload.sceneObjectIndex = scene_object_index;
    payload.materialId = object_copy.material_id;
    MaterialBSDFInitFromSceneObject(&object_copy, &payload.bsdf);
    material = MaterialManagerGet(payload.materialId);
    payload.emissive = payload.bsdf.emissive;
    payload.transparency = material ? material->transparency : 0.0;
    payload.valid = true;

    *out_payload = payload;
    return true;
}

bool RuntimeMaterialPayload3D_ResolveFromHit(const HitInfo3D* hit,
                                             RuntimeMaterialPayload3D* out_payload) {
    if (!hit || !out_payload) return false;
    return RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(hit->sceneObjectIndex,
                                                                out_payload);
}
