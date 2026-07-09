#include "editor/scene_editor_material_preview.h"
#include "editor/scene_editor_material_preview_internal.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "config/config_manager.h"
#include "editor/scene_editor_material_face_metrics.h"
#include "editor/scene_editor_material_face_placement.h"
#include "editor/scene_editor_material_stack.h"
#include "render/runtime_material_texture_stack_3d.h"
#include "scene/object_manager.h"

#define SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TEXTURE_BLOCKS 4096
#define SCENE_EDITOR_MATERIAL_PREVIEW_MAX_SAMPLE_STEP 16

static double scene_editor_material_preview_clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static bool scene_editor_material_preview_color_from_surface_eval(
    const RuntimeMaterialSurfaceEval* surface_eval,
    SDL_Color base_color,
    SDL_Color* out_color,
    double* out_mask) {
    SDL_Color color = base_color;
    double mask = 0.0;
    if (!out_color) return false;
    if (surface_eval && surface_eval->active) {
        mask = scene_editor_material_preview_clamp01(surface_eval->textureMask);
        color.r = (Uint8)lround(scene_editor_material_preview_clamp01(surface_eval->colorR) * 255.0);
        color.g = (Uint8)lround(scene_editor_material_preview_clamp01(surface_eval->colorG) * 255.0);
        color.b = (Uint8)lround(scene_editor_material_preview_clamp01(surface_eval->colorB) * 255.0);
    }
    color.a = base_color.a;
    *out_color = color;
    if (out_mask) *out_mask = mask;
    return mask > 1e-9;
}

static bool scene_editor_material_preview_apply_face_override_to_stack(
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

bool SceneEditorMaterialPreviewEvaluateTextureColor(const SceneObject* object,
                                                    int triangle_index,
                                                    double bary_v,
                                                    double bary_w,
                                                    SDL_Color base_color,
                                                    SDL_Color* out_color,
                                                    double* out_mask) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialSurfaceEval base_eval =
        RuntimeMaterialSurfaceEvalMakeBase((double)base_color.r / 255.0,
                                           (double)base_color.g / 255.0,
                                           (double)base_color.b / 255.0,
                                           0.5,
                                           0.0,
                                           0.0,
                                           1.0,
                                           0.0);
    RuntimeMaterialSurfaceEval surface_eval = {0};
    RuntimeMaterialTextureStackBuildLegacyFromObject(object, &stack);
    RuntimeMaterialTextureStackEvaluatePlacedUV(&stack,
                                                object,
                                                bary_v,
                                                bary_w,
                                                triangle_index + 1,
                                                &base_eval,
                                                &surface_eval);
    return scene_editor_material_preview_color_from_surface_eval(&surface_eval,
                                                                 base_color,
                                                                 out_color,
                                                                 out_mask);
}

bool SceneEditorMaterialPreviewEvaluateTextureColorForFace(
    const SceneObject* object,
    int scene_object_index,
    int primitive_index,
    int face_group_index,
    int local_triangle_index,
    int triangle_index,
    double bary_u,
    double bary_v,
    double bary_w,
                                                    SDL_Color base_color,
                                                    SDL_Color* out_color,
                                                    double* out_mask) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialSurfaceEval base_eval =
        RuntimeMaterialSurfaceEvalMakeBase((double)base_color.r / 255.0,
                                           (double)base_color.g / 255.0,
                                           (double)base_color.b / 255.0,
                                           0.5,
                                           0.0,
                                           0.0,
                                           1.0,
                                           0.0);
    RuntimeMaterialSurfaceEval surface_eval = {0};
    SceneEditorMaterialFacePlacement placement = {0};
    RuntimeMaterialTexture3DPlacement runtime_placement = {0};
    bool has_face_override =
        SceneEditorMaterialFacePlacementHasOverride(scene_object_index, face_group_index);
    double island_u = 0.0;
    double island_v = 0.0;
    double grounded_u = 0.0;
    double grounded_v = 0.0;
    int seed_key = ((scene_object_index + 1) * 19349663) ^ ((face_group_index + 1) * 83492791);
    if (seed_key == 0) seed_key = triangle_index + 1;
    SceneEditorMaterialFacePlacementResolveIslandUV(local_triangle_index,
                                                    bary_u,
                                                    bary_v,
                                                    bary_w,
                                                    &island_u,
                                                    &island_v);
    SceneEditorMaterialFaceMetricsResolveGroundedUV(primitive_index,
                                                    scene_object_index,
                                                    face_group_index,
                                                    island_u,
                                                    island_v,
                                                    &grounded_u,
                                                    &grounded_v);
    if (has_face_override) {
        placement = SceneEditorMaterialFacePlacementGetEffective(object,
                                                                scene_object_index,
                                                                face_group_index);
        if (!scene_editor_material_preview_apply_face_override_to_stack(object,
                                                                        scene_object_index,
                                                                        &placement,
                                                                        &stack)) {
            runtime_placement = SceneEditorMaterialFacePlacementToRuntime(&placement);
            RuntimeMaterialTextureStackBuildLegacyFromPlacement(&runtime_placement, &stack);
        }
    } else {
        SceneEditorMaterialStackGetEffectiveObjectStack(object, scene_object_index, &stack);
    }
    RuntimeMaterialTextureStackEvaluatePlacedUV(&stack,
                                                object,
                                                grounded_u,
                                                grounded_v,
                                                seed_key,
                                                &base_eval,
                                                &surface_eval);
    return scene_editor_material_preview_color_from_surface_eval(&surface_eval,
                                                                 base_color,
                                                                 out_color,
                                                                 out_mask);
}
