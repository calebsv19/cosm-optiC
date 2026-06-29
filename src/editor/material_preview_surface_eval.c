#include "editor/material_preview_surface_eval.h"

#include <math.h>
#include <string.h>

#include "editor/scene_editor_material_face_metrics.h"
#include "editor/scene_editor_material_face_placement.h"
#include "editor/scene_editor_material_stack.h"
#include "material/material_manager.h"
#include "render/material_bsdf.h"
#include "render/runtime_material_texture_3d.h"

static double material_preview_surface_eval_clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static double material_preview_surface_eval_legacy_alpha_transparency_bridge(
    const Material* material,
    double display_alpha) {
    if (!material) {
        return 0.0;
    }
    return material_preview_surface_eval_clamp01(
        material->transparency * material_preview_surface_eval_clamp01(display_alpha));
}

static RuntimeMaterialSurfaceEval material_preview_surface_eval_base(
    const SceneObject* object) {
    RuntimeMaterialSurfaceEval base_eval = {0};
    MaterialBSDF bsdf;
    const Material* material = NULL;
    double transparency = 0.0;
    memset(&bsdf, 0, sizeof(bsdf));
    if (!object) return base_eval;
    MaterialBSDFInitFromSceneObject(object, &bsdf);
    material = MaterialManagerGet(object->material_id);
    transparency =
        material_preview_surface_eval_legacy_alpha_transparency_bridge(material, object->alpha);
    return RuntimeMaterialSurfaceEvalMakeBase(bsdf.baseColorR,
                                              bsdf.baseColorG,
                                              bsdf.baseColorB,
                                              bsdf.roughness,
                                              bsdf.reflectivity,
                                              bsdf.specWeight,
                                              bsdf.diffuseWeight,
                                              transparency);
}

static bool material_preview_surface_eval_apply_face_override_to_stack(
    const SceneObject* object,
    int scene_object_index,
    const SceneEditorMaterialFacePlacement* placement,
    RuntimeMaterialTextureStack* stack) {
    RuntimeMaterialTextureLayer layer = {0};
    if (!object || !placement || !stack) return false;
    if (!SceneEditorMaterialStackGetEffectiveObjectStack(object, scene_object_index, stack)) {
        return false;
    }
    if (placement->layerIndex < 0 || placement->layerIndex >= stack->layerCount) {
        return false;
    }
    layer = stack->layers[placement->layerIndex];
    layer.placement.textureId = placement->textureId;
    layer.placement.offsetU = placement->offsetU;
    layer.placement.offsetV = placement->offsetV;
    layer.placement.scale = placement->scale;
    layer.placement.strength = placement->strength;
    layer.placement.rotation = placement->rotation;
    layer.params = placement->params;
    layer.placement.params = placement->params;
    stack->layers[placement->layerIndex] = RuntimeMaterialTextureLayerNormalize(layer);
    *stack = RuntimeMaterialTextureStackNormalize(*stack);
    return true;
}

bool MaterialPreviewSurfaceEvaluateObject(const SceneObject* object,
                                          int scene_object_index,
                                          const RuntimeMaterialTextureLayer* preview_overlay,
                                          double u,
                                          double v,
                                          RuntimeMaterialSurfaceEval* out_eval) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialSurfaceEval base_eval = {0};
    RuntimeMaterialSurfaceEval surface_eval = {0};
    if (!object || !out_eval) return false;
    base_eval = material_preview_surface_eval_base(object);
    if (!SceneEditorMaterialStackGetEffectiveObjectStack(object, scene_object_index, &stack)) {
        RuntimeMaterialTextureStackBuildLegacyFromObject(object, &stack);
    }
    if (preview_overlay &&
        preview_overlay->kind != RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE &&
        stack.layerCount < RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS) {
        stack.layers[stack.layerCount++] =
            RuntimeMaterialTextureLayerNormalize(*preview_overlay);
        stack = RuntimeMaterialTextureStackNormalize(stack);
    }
    if (!RuntimeMaterialTextureStackEvaluatePlacedUV(&stack,
                                                     object,
                                                     u,
                                                     v,
                                                     object->textureSeed != 0
                                                         ? object->textureSeed
                                                         : scene_object_index + 1,
                                                     &base_eval,
                                                     &surface_eval)) {
        surface_eval = base_eval;
    }
    *out_eval = surface_eval;
    return true;
}

bool MaterialPreviewSurfaceEvaluateFace(const SceneObject* object,
                                        int scene_object_index,
                                        int face_group_index,
                                        double u,
                                        double v,
                                        RuntimeMaterialSurfaceEval* out_eval) {
    return MaterialPreviewSurfaceEvaluateFacePrimitive(object,
                                                       scene_object_index,
                                                       -1,
                                                       face_group_index,
                                                       u,
                                                       v,
                                                       out_eval);
}

bool MaterialPreviewSurfaceEvaluateFacePrimitive(const SceneObject* object,
                                                 int scene_object_index,
                                                 int primitive_index,
                                                 int face_group_index,
                                                 double u,
                                                 double v,
                                                 RuntimeMaterialSurfaceEval* out_eval) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialSurfaceEval base_eval = {0};
    RuntimeMaterialSurfaceEval surface_eval = {0};
    SceneEditorMaterialFacePlacement placement = {0};
    RuntimeMaterialTexture3DPlacement runtime_placement = {0};
    bool has_face_override = false;
    int seed_key = 0;
    double grounded_u = u;
    double grounded_v = v;
    if (!object || !out_eval || scene_object_index < 0 || face_group_index < 0) return false;
    base_eval = material_preview_surface_eval_base(object);
    has_face_override =
        SceneEditorMaterialFacePlacementHasOverride(scene_object_index, face_group_index);
    seed_key = ((scene_object_index + 1) * 19349663) ^ ((face_group_index + 1) * 83492791);
    if (seed_key == 0) seed_key = scene_object_index + 1;
    SceneEditorMaterialFaceMetricsResolveGroundedUV(primitive_index,
                                                    scene_object_index,
                                                    face_group_index,
                                                    u,
                                                    v,
                                                    &grounded_u,
                                                    &grounded_v);
    if (!SceneEditorMaterialStackGetEffectiveObjectStack(object,
                                                        scene_object_index,
                                                        &stack)) {
        RuntimeMaterialTextureStackBuildLegacyFromObject(object, &stack);
    }
    if (has_face_override) {
        if (!SceneEditorMaterialFacePlacementApplyOverridesToStack(object,
                                                                   scene_object_index,
                                                                   face_group_index,
                                                                   &stack)) {
            placement = SceneEditorMaterialFacePlacementGetEffective(object,
                                                                     scene_object_index,
                                                                     face_group_index);
            if (!material_preview_surface_eval_apply_face_override_to_stack(object,
                                                                            scene_object_index,
                                                                            &placement,
                                                                            &stack)) {
                runtime_placement = SceneEditorMaterialFacePlacementToRuntime(&placement);
                RuntimeMaterialTextureStackBuildLegacyFromPlacement(&runtime_placement, &stack);
            }
        }
    }
    if (!RuntimeMaterialTextureStackEvaluatePlacedUV(&stack,
                                                     object,
                                                     grounded_u,
                                                     grounded_v,
                                                     seed_key,
                                                     &base_eval,
                                                     &surface_eval)) {
        surface_eval = base_eval;
    }
    *out_eval = surface_eval;
    return true;
}

void MaterialPreviewSurfaceShadePixel(const RuntimeMaterialSurfaceEval* eval,
                                     const SceneObject* object,
                                     double u,
                                     double v,
                                     Uint8 bg_r,
                                     Uint8 bg_g,
                                     Uint8 bg_b,
                                     Uint8* out_r,
                                     Uint8* out_g,
                                     Uint8* out_b) {
    double x = (u - 0.5) * 1.6;
    double y = (v - 0.5) * 1.6;
    double nz = 1.0 / sqrt(1.0 + x * x + y * y);
    double nx = x * nz;
    double ny = y * nz;
    double lx = -0.42;
    double ly = -0.58;
    double lz = 0.70;
    double ll = sqrt(lx * lx + ly * ly + lz * lz);
    double ndotl = 0.0;
    double hx = 0.0;
    double hy = 0.0;
    double hz = 0.0;
    double hlen = 0.0;
    double spec = 0.0;
    double shininess = 0.0;
    double diffuse = 0.0;
    double emissive = 0.0;
    double alpha = 1.0 -
                   material_preview_surface_eval_clamp01(eval ? eval->transparency : 0.0);
    double base_r = eval ? eval->colorR : 1.0;
    double base_g = eval ? eval->colorG : 1.0;
    double base_b = eval ? eval->colorB : 1.0;
    double roughness = eval ? material_preview_surface_eval_clamp01(eval->roughness)
                            : material_preview_surface_eval_clamp01(object->roughness);
    double reflectivity = eval ? material_preview_surface_eval_clamp01(eval->reflectivity)
                               : material_preview_surface_eval_clamp01(object->reflectivity);
    lx /= ll;
    ly /= ll;
    lz /= ll;
    ndotl = fmax(0.0, nx * lx + ny * ly + nz * lz);
    diffuse = 0.30 + ndotl * 0.70;
    hx = lx;
    hy = ly;
    hz = lz + 1.0;
    hlen = sqrt(hx * hx + hy * hy + hz * hz);
    hx /= hlen;
    hy /= hlen;
    hz /= hlen;
    shininess = 8.0 + ((1.0 - roughness) * 88.0);
    spec = pow(fmax(0.0, nx * hx + ny * hy + nz * hz), shininess) *
           (0.08 + reflectivity * 0.92);
    emissive = material_preview_surface_eval_clamp01(object->emissiveStrength) * 0.28;
    base_r = material_preview_surface_eval_clamp01(base_r * diffuse + spec + emissive);
    base_g = material_preview_surface_eval_clamp01(base_g * diffuse + spec + emissive);
    base_b = material_preview_surface_eval_clamp01(base_b * diffuse + spec + emissive);
    *out_r = (Uint8)lround(((double)bg_r * (1.0 - alpha)) + (base_r * 255.0 * alpha));
    *out_g = (Uint8)lround(((double)bg_g * (1.0 - alpha)) + (base_g * 255.0 * alpha));
    *out_b = (Uint8)lround(((double)bg_b * (1.0 - alpha)) + (base_b * 255.0 * alpha));
}
