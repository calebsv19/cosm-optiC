#include "render/runtime_material_payload_3d.h"

#include <math.h>
#include <string.h>

#include "config/config_manager.h"
#include "editor/scene_editor_material_face_placement.h"
#include "editor/scene_editor_material_stack.h"
#include "material/material_manager.h"
#include "render/runtime_material_authored_texture_3d.h"
#include "render/runtime_material_texture_stack_3d.h"

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
    RuntimeMaterialAuthoredTextureSample authored_sample = {0};
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialSurfaceEval base_eval = {0};
    RuntimeMaterialSurfaceEval surface_eval = {0};
    RuntimeMaterialTexture3DPlacement runtime_placement = {0};
    SceneEditorMaterialFacePlacement face_placement = {0};
    double island_u = 0.0;
    double island_v = 0.0;
    int face_group_index = -1;
    int local_triangle_index = -1;
    int seed_key = 0;

    if (!object || !hit || !payload || !payload->valid) return;
    base_eval = RuntimeMaterialSurfaceEvalMakeBase(payload->baseColorR,
                                                   payload->baseColorG,
                                                   payload->baseColorB,
                                                   payload->bsdf.roughness,
                                                   payload->bsdf.reflectivity,
                                                   payload->bsdf.specWeight,
                                                   payload->bsdf.diffuseWeight,
                                                   payload->transparency);
    local_triangle_index = hit->localTriangleIndex;
    face_group_index = local_triangle_index >= 0 ? local_triangle_index / 2 : -1;
    if (face_group_index >= 0 && hit->sceneObjectIndex >= 0) {
        bool has_face_override =
            SceneEditorMaterialFacePlacementHasOverride(hit->sceneObjectIndex, face_group_index);
        seed_key = ((hit->sceneObjectIndex + 1) * 19349663) ^
                   ((face_group_index + 1) * 83492791);
        if (seed_key == 0) seed_key = hit->triangleIndex + 1;
        SceneEditorMaterialFacePlacementResolveIslandUV(local_triangle_index,
                                                        hit->baryU,
                                                        hit->baryV,
                                                        hit->baryW,
                                                        &island_u,
                                                        &island_v);
        if (RuntimeMaterialAuthoredTextureSampleFace(hit->sceneObjectIndex,
                                                     face_group_index,
                                                     island_u,
                                                     island_v,
                                                     &authored_sample)) {
            double alpha = runtime_material_payload_3d_clamp01(authored_sample.alpha);
            payload->textureMask = alpha;
            payload->textureU = island_u;
            payload->textureV = island_v;
            payload->baseColorR =
                payload->baseColorR + ((authored_sample.colorR - payload->baseColorR) * alpha);
            payload->baseColorG =
                payload->baseColorG + ((authored_sample.colorG - payload->baseColorG) * alpha);
            payload->baseColorB =
                payload->baseColorB + ((authored_sample.colorB - payload->baseColorB) * alpha);
            payload->transparency *= (1.0 - alpha);
            runtime_material_payload_3d_refresh_derived(payload);
            return;
        }
        if (has_face_override) {
            face_placement = SceneEditorMaterialFacePlacementGetEffective(object,
                                                                          hit->sceneObjectIndex,
                                                                          face_group_index);
            runtime_placement = SceneEditorMaterialFacePlacementToRuntime(&face_placement);
            if (!RuntimeMaterialTextureStackBuildLegacyFromPlacement(&runtime_placement, &stack)) {
                return;
            }
        } else if (!SceneEditorMaterialStackGetEffectiveObjectStack(object,
                                                                    hit->sceneObjectIndex,
                                                                    &stack)) {
            return;
        }
        if (!RuntimeMaterialTextureStackEvaluatePlacedUV(&stack,
                                                         object,
                                                         island_u,
                                                         island_v,
                                                         seed_key,
                                                         &base_eval,
                                                         &surface_eval)) {
            return;
        }
    } else {
        seed_key = hit->triangleIndex + 1;
        if (!SceneEditorMaterialStackGetEffectiveObjectStack(object, hit->sceneObjectIndex, &stack) ||
            !RuntimeMaterialTextureStackEvaluatePlacedUV(&stack,
                                                         object,
                                                         hit->baryV,
                                                         hit->baryW,
                                                         seed_key,
                                                         &base_eval,
                                                         &surface_eval)) {
            return;
        }
    }

    payload->textureMask = runtime_material_payload_3d_clamp01(surface_eval.textureMask);
    payload->textureU = surface_eval.textureU;
    payload->textureV = surface_eval.textureV;
    payload->baseColorR = surface_eval.colorR;
    payload->baseColorG = surface_eval.colorG;
    payload->baseColorB = surface_eval.colorB;
    payload->bsdf.reflectivity = surface_eval.reflectivity;
    payload->bsdf.roughness = surface_eval.roughness;
    payload->bsdf.specWeight = surface_eval.specWeight;
    payload->bsdf.diffuseWeight = surface_eval.diffuseWeight;
    payload->transparency = surface_eval.transparency;
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
