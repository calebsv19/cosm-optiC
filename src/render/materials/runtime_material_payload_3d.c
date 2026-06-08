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

static double runtime_material_payload_3d_lerp(double a, double b, double t) {
    return a + ((b - a) * runtime_material_payload_3d_clamp01(t));
}

static double runtime_material_payload_3d_clamp_positive(double value, double fallback) {
    if (value > 1e-6) {
        return value;
    }
    return fallback;
}

static void runtime_material_payload_3d_apply_authored_overlay_material(
    RuntimeMaterialPayload3D* payload,
    RuntimeMaterialTextureLayerKind overlay_kind,
    double overlay_alpha) {
    double t = runtime_material_payload_3d_clamp01(overlay_alpha);
    if (!payload || t <= 1e-9) {
        return;
    }
    switch (overlay_kind) {
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME:
            payload->baseColorR *= runtime_material_payload_3d_lerp(1.0, 0.82, t);
            payload->baseColorG *= runtime_material_payload_3d_lerp(1.0, 0.82, t);
            payload->baseColorB *= runtime_material_payload_3d_lerp(1.0, 0.82, t);
            payload->bsdf.roughness = runtime_material_payload_3d_clamp01(
                payload->bsdf.roughness + (0.22 * t));
            payload->bsdf.reflectivity = runtime_material_payload_3d_clamp01(
                payload->bsdf.reflectivity * runtime_material_payload_3d_lerp(1.0, 0.82, t));
            break;
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL:
            payload->baseColorR *= runtime_material_payload_3d_lerp(1.0, 0.92, t);
            payload->baseColorG *= runtime_material_payload_3d_lerp(1.0, 0.92, t);
            payload->baseColorB *= runtime_material_payload_3d_lerp(1.0, 0.92, t);
            payload->bsdf.roughness = runtime_material_payload_3d_clamp01(
                payload->bsdf.roughness * runtime_material_payload_3d_lerp(1.0, 0.55, t));
            if (payload->bsdf.roughness < 0.02) {
                payload->bsdf.roughness = 0.02;
            }
            payload->bsdf.reflectivity = runtime_material_payload_3d_clamp01(
                payload->bsdf.reflectivity + (0.26 * t));
            payload->bsdf.specWeight = runtime_material_payload_3d_clamp01(
                payload->bsdf.specWeight + (0.18 * t));
            payload->bsdf.diffuseWeight = runtime_material_payload_3d_clamp01(
                payload->bsdf.diffuseWeight * runtime_material_payload_3d_lerp(1.0, 0.88, t));
            break;
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST:
            payload->bsdf.roughness = runtime_material_payload_3d_clamp01(
                payload->bsdf.roughness + (0.30 * t));
            payload->bsdf.reflectivity = runtime_material_payload_3d_clamp01(
                payload->bsdf.reflectivity * runtime_material_payload_3d_lerp(1.0, 0.60, t));
            payload->bsdf.specWeight = runtime_material_payload_3d_clamp01(
                payload->bsdf.specWeight * runtime_material_payload_3d_lerp(1.0, 0.72, t));
            break;
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG:
            payload->bsdf.roughness = runtime_material_payload_3d_clamp01(
                payload->bsdf.roughness + (0.14 * t));
            payload->transparency *= runtime_material_payload_3d_lerp(1.0, 0.68, t);
            break;
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE:
        default:
            break;
    }
}

static void runtime_material_payload_3d_apply_authored_base_material(
    RuntimeMaterialPayload3D* payload,
    RuntimeMaterialTextureLayerKind base_kind,
    double base_alpha) {
    double t = runtime_material_payload_3d_clamp01(base_alpha);
    if (!payload || t <= 1e-9) {
        return;
    }
    switch (base_kind) {
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_BRUSHED_METAL:
            payload->bsdf.roughness = runtime_material_payload_3d_lerp(
                payload->bsdf.roughness, 0.18, t);
            payload->bsdf.reflectivity = runtime_material_payload_3d_lerp(
                payload->bsdf.reflectivity, 0.76, t);
            payload->bsdf.specWeight = runtime_material_payload_3d_lerp(
                payload->bsdf.specWeight, 0.68, t);
            payload->bsdf.diffuseWeight = runtime_material_payload_3d_lerp(
                payload->bsdf.diffuseWeight, 0.32, t);
            break;
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD:
            payload->bsdf.roughness = runtime_material_payload_3d_lerp(
                payload->bsdf.roughness, 0.58, t);
            payload->bsdf.reflectivity = runtime_material_payload_3d_lerp(
                payload->bsdf.reflectivity, 0.08, t);
            payload->bsdf.specWeight = runtime_material_payload_3d_lerp(
                payload->bsdf.specWeight, 0.16, t);
            payload->bsdf.diffuseWeight = runtime_material_payload_3d_lerp(
                payload->bsdf.diffuseWeight, 0.88, t);
            break;
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_BRICK:
            payload->bsdf.roughness = runtime_material_payload_3d_lerp(
                payload->bsdf.roughness, 0.86, t);
            payload->bsdf.reflectivity = runtime_material_payload_3d_lerp(
                payload->bsdf.reflectivity, 0.03, t);
            payload->bsdf.specWeight = runtime_material_payload_3d_lerp(
                payload->bsdf.specWeight, 0.08, t);
            break;
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_CONCRETE:
            payload->bsdf.roughness = runtime_material_payload_3d_lerp(
                payload->bsdf.roughness, 0.78, t);
            payload->bsdf.reflectivity = runtime_material_payload_3d_lerp(
                payload->bsdf.reflectivity, 0.04, t);
            payload->bsdf.specWeight = runtime_material_payload_3d_lerp(
                payload->bsdf.specWeight, 0.10, t);
            break;
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_STONE:
            payload->bsdf.roughness = runtime_material_payload_3d_lerp(
                payload->bsdf.roughness, 0.70, t);
            payload->bsdf.reflectivity = runtime_material_payload_3d_lerp(
                payload->bsdf.reflectivity, 0.07, t);
            payload->bsdf.specWeight = runtime_material_payload_3d_lerp(
                payload->bsdf.specWeight, 0.18, t);
            break;
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID:
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE:
        default:
            break;
    }
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

static void runtime_material_payload_3d_apply_surface_eval(
    RuntimeMaterialPayload3D* payload,
    const RuntimeMaterialSurfaceEval* surface_eval) {
    if (!payload || !surface_eval) return;
    payload->textureMask = runtime_material_payload_3d_clamp01(surface_eval->textureMask);
    payload->textureU = surface_eval->textureU;
    payload->textureV = surface_eval->textureV;
    payload->baseColorR = surface_eval->colorR;
    payload->baseColorG = surface_eval->colorG;
    payload->baseColorB = surface_eval->colorB;
    payload->bsdf.reflectivity = surface_eval->reflectivity;
    payload->bsdf.roughness = surface_eval->roughness;
    payload->bsdf.specWeight = surface_eval->specWeight;
    payload->bsdf.diffuseWeight = surface_eval->diffuseWeight;
    payload->transparency = surface_eval->transparency;
    runtime_material_payload_3d_refresh_derived(payload);
}

static bool runtime_material_payload_3d_resolve_object_texture_uv(const HitInfo3D* hit,
                                                                  double* out_u,
                                                                  double* out_v) {
    double ax = 0.0;
    double ay = 0.0;
    double az = 0.0;
    if (!hit || !hit->hasObjectTextureCoord || !out_u || !out_v) return false;

    ax = fabs(hit->normal.x);
    ay = fabs(hit->normal.y);
    az = fabs(hit->normal.z);
    if (az >= ax && az >= ay) {
        *out_u = hit->objectTextureCoord.x;
        *out_v = hit->objectTextureCoord.y;
    } else if (ay >= ax) {
        *out_u = hit->objectTextureCoord.x;
        *out_v = hit->objectTextureCoord.z;
    } else {
        *out_u = hit->objectTextureCoord.y;
        *out_v = hit->objectTextureCoord.z;
    }
    return true;
}

static void runtime_material_payload_3d_apply_texture(
    const SceneObject* object,
    const HitInfo3D* hit,
    RuntimeMaterialPayload3D* payload) {
    RuntimeMaterialAuthoredTextureSample authored_sample = {0};
    RuntimeMaterialAuthoredTextureSample authored_overlay_sample = {0};
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialSurfaceEval base_eval = {0};
    RuntimeMaterialSurfaceEval surface_eval = {0};
    RuntimeMaterialTexture3DPlacement runtime_placement = {0};
    SceneEditorMaterialFacePlacement face_placement = {0};
    double island_u = 0.0;
    double island_v = 0.0;
    double object_u = 0.0;
    double object_v = 0.0;
    double authored_alpha = 0.0;
    double authored_overlay_alpha = 0.0;
    int face_group_index = -1;
    int local_triangle_index = -1;
    int seed_key = 0;
    bool overlay_only = false;
    char authored_overlay_material_intent[RUNTIME_MATERIAL_AUTHORED_TEXTURE_INTENT_CAPACITY];
    RuntimeMaterialAuthoredTextureFaceMetadata authored_metadata;
    char authored_base_material_intent[RUNTIME_MATERIAL_AUTHORED_TEXTURE_INTENT_CAPACITY];
    RuntimeMaterialTextureLayerKind authored_overlay_kind = RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE;
    RuntimeMaterialTextureLayerKind authored_base_kind = RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE;
    bool has_authored_metadata = false;

    if (!object || !hit || !payload || !payload->valid) return;
    base_eval = RuntimeMaterialSurfaceEvalMakeBase(payload->baseColorR,
                                                   payload->baseColorG,
                                                   payload->baseColorB,
                                                   payload->bsdf.roughness,
                                                   payload->bsdf.reflectivity,
                                                   payload->bsdf.specWeight,
                                                   payload->bsdf.diffuseWeight,
                                                   payload->transparency);
    if (runtime_material_payload_3d_resolve_object_texture_uv(hit, &object_u, &object_v) &&
        SceneEditorMaterialStackGetEffectiveObjectStack(object, hit->sceneObjectIndex, &stack) &&
        RuntimeMaterialTextureStackEvaluatePlacedUV(&stack,
                                                    object,
                                                    object_u,
                                                    object_v,
                                                    ((hit->sceneObjectIndex + 1) * 19349663),
                                                    &base_eval,
                                                    &surface_eval)) {
        runtime_material_payload_3d_apply_surface_eval(payload, &surface_eval);
        return;
    }

    local_triangle_index = hit->localTriangleIndex;
    face_group_index = local_triangle_index >= 0 ? local_triangle_index / 2 : -1;
    memset(authored_overlay_material_intent, 0, sizeof(authored_overlay_material_intent));
    memset(authored_base_material_intent, 0, sizeof(authored_base_material_intent));
    memset(&authored_metadata, 0, sizeof(authored_metadata));
    if (face_group_index >= 0 && hit->sceneObjectIndex >= 0) {
        has_authored_metadata = RuntimeMaterialAuthoredTextureGetFaceMetadata(hit->sceneObjectIndex,
                                                                              face_group_index,
                                                                              &authored_metadata);
        if (has_authored_metadata && authored_metadata.baseMaterialIntentKind[0]) {
            snprintf(authored_base_material_intent,
                     sizeof(authored_base_material_intent),
                     "%s",
                     authored_metadata.baseMaterialIntentKind);
            authored_base_kind =
                RuntimeMaterialTextureLayerKindFromStableId(authored_base_material_intent);
        }
        if (has_authored_metadata && authored_metadata.overlayMaterialIntentKind[0]) {
            snprintf(authored_overlay_material_intent,
                     sizeof(authored_overlay_material_intent),
                     "%s",
                     authored_metadata.overlayMaterialIntentKind);
            authored_overlay_kind =
                RuntimeMaterialTextureLayerKindFromStableId(authored_overlay_material_intent);
        }
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
            authored_alpha = runtime_material_payload_3d_clamp01(authored_sample.alpha);
            payload->baseColorR =
                payload->baseColorR + ((authored_sample.colorR - payload->baseColorR) * authored_alpha);
            payload->baseColorG =
                payload->baseColorG + ((authored_sample.colorG - payload->baseColorG) * authored_alpha);
            payload->baseColorB =
                payload->baseColorB + ((authored_sample.colorB - payload->baseColorB) * authored_alpha);
            payload->transparency *= (1.0 - authored_alpha);
            runtime_material_payload_3d_apply_authored_base_material(payload,
                                                                     authored_base_kind,
                                                                     authored_alpha);
            runtime_material_payload_3d_refresh_derived(payload);
            overlay_only = true;
            base_eval = RuntimeMaterialSurfaceEvalMakeBase(payload->baseColorR,
                                                           payload->baseColorG,
                                                           payload->baseColorB,
                                                           payload->bsdf.roughness,
                                                           payload->bsdf.reflectivity,
                                                           payload->bsdf.specWeight,
                                                           payload->bsdf.diffuseWeight,
                                                           payload->transparency);
            base_eval.active = true;
            base_eval.textureMask = authored_alpha;
            base_eval.textureU = island_u;
            base_eval.textureV = island_v;
        }
        if (RuntimeMaterialAuthoredTextureSampleOverlayFace(hit->sceneObjectIndex,
                                                            face_group_index,
                                                            island_u,
                                                            island_v,
                                                            &authored_overlay_sample)) {
            authored_overlay_alpha = runtime_material_payload_3d_clamp01(authored_overlay_sample.alpha);
            payload->baseColorR = runtime_material_payload_3d_lerp(payload->baseColorR,
                                                                   authored_overlay_sample.colorR,
                                                                   authored_overlay_alpha);
            payload->baseColorG = runtime_material_payload_3d_lerp(payload->baseColorG,
                                                                   authored_overlay_sample.colorG,
                                                                   authored_overlay_alpha);
            payload->baseColorB = runtime_material_payload_3d_lerp(payload->baseColorB,
                                                                   authored_overlay_sample.colorB,
                                                                   authored_overlay_alpha);
            if (authored_overlay_kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE &&
                RuntimeMaterialAuthoredTextureGetOverlayMaterialIntent(hit->sceneObjectIndex,
                                                                       authored_overlay_material_intent,
                                                                       sizeof(authored_overlay_material_intent))) {
                authored_overlay_kind =
                    RuntimeMaterialTextureLayerKindFromStableId(authored_overlay_material_intent);
            }
            runtime_material_payload_3d_apply_authored_overlay_material(payload,
                                                                        authored_overlay_kind,
                                                                        authored_overlay_alpha);
            runtime_material_payload_3d_refresh_derived(payload);
            overlay_only = true;
            base_eval = RuntimeMaterialSurfaceEvalMakeBase(payload->baseColorR,
                                                           payload->baseColorG,
                                                           payload->baseColorB,
                                                           payload->bsdf.roughness,
                                                           payload->bsdf.reflectivity,
                                                           payload->bsdf.specWeight,
                                                           payload->bsdf.diffuseWeight,
                                                           payload->transparency);
            base_eval.active = true;
            if (authored_overlay_alpha > base_eval.textureMask) {
                base_eval.textureMask = authored_overlay_alpha;
            }
            base_eval.textureU = island_u;
            base_eval.textureV = island_v;
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
        if (overlay_only) {
            if (!RuntimeMaterialTextureStackEvaluateOverlayPlacedUV(&stack,
                                                                    object,
                                                                    island_u,
                                                                    island_v,
                                                                    seed_key,
                                                                    &base_eval,
                                                                    &surface_eval)) {
                payload->textureMask = authored_alpha;
                payload->textureU = island_u;
                payload->textureV = island_v;
                return;
            }
        } else {
            if (!RuntimeMaterialTextureStackEvaluatePlacedUV(&stack,
                                                             object,
                                                             island_u,
                                                             island_v,
                                                             seed_key,
                                                             &base_eval,
                                                             &surface_eval)) {
                return;
            }
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

    runtime_material_payload_3d_apply_surface_eval(payload, &surface_eval);
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
    payload.opticalIor = material ? runtime_material_payload_3d_clamp_positive(material->ior, 1.0)
                                  : runtime_material_payload_3d_clamp_positive(payload.bsdf.ior, 1.0);
    payload.absorptionDistance =
        material ? runtime_material_payload_3d_clamp_positive(material->absorption_distance, 1.0)
                 : 1.0;
    payload.thinWalled = material ? material->thin_walled : false;
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
