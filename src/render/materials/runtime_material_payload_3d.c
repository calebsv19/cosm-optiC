#include "render/runtime_material_payload_3d.h"

#include <math.h>
#include <string.h>

#include "config/config_manager.h"
#include "editor/scene_editor_material_face_placement.h"
#include "material/material_manager.h"
#include "render/runtime_material_texture_3d.h"

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

static double runtime_material_payload_3d_clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static double runtime_material_payload_3d_lerp(double a, double b, double t) {
    return a + ((b - a) * runtime_material_payload_3d_clamp01(t));
}

static void runtime_material_payload_3d_refresh_derived(RuntimeMaterialPayload3D* payload) {
    double weight_sum = 0.0;
    if (!payload) return;

    payload->baseColorR = runtime_material_payload_3d_clamp01(payload->baseColorR);
    payload->baseColorG = runtime_material_payload_3d_clamp01(payload->baseColorG);
    payload->baseColorB = runtime_material_payload_3d_clamp01(payload->baseColorB);
    payload->bsdf.baseColorR = payload->baseColorR;
    payload->bsdf.baseColorG = payload->baseColorG;
    payload->bsdf.baseColorB = payload->baseColorB;
    payload->bsdf.albedo = runtime_material_payload_3d_clamp01(
        (0.2126 * payload->baseColorR) +
        (0.7152 * payload->baseColorG) +
        (0.0722 * payload->baseColorB));
    payload->bsdf.reflectivity = runtime_material_payload_3d_clamp01(payload->bsdf.reflectivity);
    payload->bsdf.roughness =
        runtime_material_payload_3d_clamp01(payload->bsdf.roughness);
    if (payload->bsdf.roughness < 0.02) {
        payload->bsdf.roughness = 0.02;
    }
    payload->bsdf.diffuseWeight = runtime_material_payload_3d_clamp01(payload->bsdf.diffuseWeight);
    payload->bsdf.specWeight = runtime_material_payload_3d_clamp01(payload->bsdf.specWeight);

    weight_sum = payload->bsdf.diffuseWeight + payload->bsdf.specWeight;
    if (weight_sum > 1.0) {
        payload->bsdf.diffuseWeight /= weight_sum;
        payload->bsdf.specWeight /= weight_sum;
    }
    payload->bsdf.weightSum = payload->bsdf.diffuseWeight + payload->bsdf.specWeight;
    if (payload->bsdf.weightSum <= 1e-4) {
        payload->bsdf.diffuseWeight = 1.0;
        payload->bsdf.weightSum = 1.0;
    }
}

static void runtime_material_payload_3d_apply_texture(
    const SceneObject* object,
    const HitInfo3D* hit,
    RuntimeMaterialPayload3D* payload) {
    RuntimeMaterialTexture3DSample sample = {0};
    RuntimeMaterialTexture3DPlacement runtime_placement = {0};
    SceneEditorMaterialFacePlacement face_placement = {0};
    double mask = 0.0;
    double island_u = 0.0;
    double island_v = 0.0;
    int face_group_index = -1;
    int local_triangle_index = -1;
    int seed_key = 0;

    if (!object || !hit || !payload || !payload->valid) return;
    local_triangle_index = hit->localTriangleIndex;
    face_group_index = local_triangle_index >= 0 ? local_triangle_index / 2 : -1;
    if (face_group_index >= 0 && hit->sceneObjectIndex >= 0) {
        face_placement = SceneEditorMaterialFacePlacementGetEffective(object,
                                                                      hit->sceneObjectIndex,
                                                                      face_group_index);
        runtime_placement = SceneEditorMaterialFacePlacementToRuntime(&face_placement);
        seed_key = ((hit->sceneObjectIndex + 1) * 19349663) ^
                   ((face_group_index + 1) * 83492791);
        if (seed_key == 0) seed_key = hit->triangleIndex + 1;
        SceneEditorMaterialFacePlacementResolveIslandUV(local_triangle_index,
                                                        hit->baryU,
                                                        hit->baryV,
                                                        hit->baryW,
                                                        &island_u,
                                                        &island_v);
        if (!RuntimeMaterialTexture3D_SamplePlacedUV(object,
                                                     island_u,
                                                     island_v,
                                                     seed_key,
                                                     &runtime_placement,
                                                     &sample)) {
            return;
        }
    } else if (!RuntimeMaterialTexture3D_Sample(object, hit, &sample)) {
        return;
    }

    mask = runtime_material_payload_3d_clamp01(sample.mask);
    payload->textureMask = mask;
    payload->textureU = sample.u;
    payload->textureV = sample.v;

    if (sample.kind == RUNTIME_MATERIAL_TEXTURE_3D_RUST) {
        double color_t = runtime_material_payload_3d_clamp01(mask *
            runtime_material_payload_3d_lerp(0.35, 1.35, sample.colorDepth));
        double damage_t = runtime_material_payload_3d_clamp01(mask *
            runtime_material_payload_3d_lerp(0.20, 1.25, sample.surfaceDamage));
        payload->baseColorR = runtime_material_payload_3d_lerp(payload->baseColorR, 0.74, color_t);
        payload->baseColorG = runtime_material_payload_3d_lerp(payload->baseColorG, 0.26, color_t);
        payload->baseColorB = runtime_material_payload_3d_lerp(payload->baseColorB, 0.08, color_t);
        payload->bsdf.reflectivity *= 1.0 - (0.90 * damage_t);
        payload->bsdf.roughness = runtime_material_payload_3d_lerp(payload->bsdf.roughness, 0.96, damage_t);
        payload->bsdf.specWeight *= 1.0 - (0.80 * damage_t);
        payload->bsdf.diffuseWeight = runtime_material_payload_3d_lerp(payload->bsdf.diffuseWeight, 0.90, damage_t);
        payload->transparency *= 1.0 - damage_t;
    } else if (sample.kind == RUNTIME_MATERIAL_TEXTURE_3D_FOG) {
        double color_t = runtime_material_payload_3d_clamp01(mask *
            runtime_material_payload_3d_lerp(0.20, 0.85, sample.colorDepth));
        double damage_t = runtime_material_payload_3d_clamp01(mask *
            runtime_material_payload_3d_lerp(0.15, 1.0, sample.surfaceDamage));
        payload->baseColorR = runtime_material_payload_3d_lerp(payload->baseColorR, 0.82, color_t);
        payload->baseColorG = runtime_material_payload_3d_lerp(payload->baseColorG, 0.86, color_t);
        payload->baseColorB = runtime_material_payload_3d_lerp(payload->baseColorB, 0.88, color_t);
        payload->bsdf.reflectivity *= 1.0 - (0.35 * damage_t);
        payload->bsdf.roughness = runtime_material_payload_3d_lerp(payload->bsdf.roughness, 1.0, damage_t);
        payload->bsdf.specWeight *= 1.0 - (0.35 * damage_t);
        payload->bsdf.diffuseWeight = runtime_material_payload_3d_lerp(payload->bsdf.diffuseWeight, 0.70, damage_t);
        payload->transparency *= 1.0 - (0.25 * damage_t);
    }

    runtime_material_payload_3d_refresh_derived(payload);
}

void RuntimeMaterialPayload3D_Reset(RuntimeMaterialPayload3D* payload) {
    if (!payload) return;
    memset(payload, 0, sizeof(*payload));
    payload->sceneObjectIndex = -1;
    payload->materialId = -1;
}

static bool runtime_material_payload_3d_resolve(int scene_object_index,
                                                const HitInfo3D* hit,
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
    payload.baseColorR = payload.bsdf.baseColorR;
    payload.baseColorG = payload.bsdf.baseColorG;
    payload.baseColorB = payload.bsdf.baseColorB;
    payload.emissive = payload.bsdf.emissive;
    payload.transparency =
        material ? runtime_material_payload_3d_clamp01(
                       material->transparency *
                       runtime_material_payload_3d_clamp01(object_copy.alpha))
                 : 0.0;
    payload.valid = true;
    runtime_material_payload_3d_apply_texture(&object_copy, hit, &payload);

    *out_payload = payload;
    return true;
}

bool RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(int scene_object_index,
                                                          RuntimeMaterialPayload3D* out_payload) {
    return runtime_material_payload_3d_resolve(scene_object_index, NULL, out_payload);
}

bool RuntimeMaterialPayload3D_ResolveFromHit(const HitInfo3D* hit,
                                             RuntimeMaterialPayload3D* out_payload) {
    if (!hit || !out_payload) return false;
    return runtime_material_payload_3d_resolve(hit->sceneObjectIndex, hit, out_payload);
}
