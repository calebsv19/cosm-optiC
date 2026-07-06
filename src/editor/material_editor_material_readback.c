#include "editor/material_editor_internal.h"

#include <stdio.h>
#include <string.h>

#include "editor/material_editor_authored_texture_binding.h"
#include "editor/material_editor_layer_model.h"
#include "editor/scene_editor_material_graph.h"
#include "editor/scene_editor_material_stack.h"
#include "material/material.h"
#include "material/material_manager.h"
#include "render/material_bsdf.h"

static const char* material_editor_preset_label(int material_id) {
    switch (material_id) {
        case MATERIAL_PRESET_DEFAULT:
            return "Default";
        case MATERIAL_PRESET_MIRROR:
            return "Mirror";
        case MATERIAL_PRESET_ROUGH_METAL:
            return "Rough Metal";
        case MATERIAL_PRESET_GLOSSY:
            return "Glossy";
        case MATERIAL_PRESET_EMISSIVE:
            return "Emissive";
        case MATERIAL_PRESET_TRANSPARENT:
            return "Transparent";
        default:
            break;
    }
    return "Preset";
}

static double material_editor_readback_clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static double material_editor_readback_alpha_transparency_bridge(
    const Material* material,
    double display_alpha) {
    if (!material) return 0.0;
    return material_editor_readback_clamp01(
        material->transparency * material_editor_readback_clamp01(display_alpha));
}

static RuntimeMaterialSurfaceEval material_editor_readback_base_surface_eval(
    const SceneObject* object) {
    RuntimeMaterialSurfaceEval base_eval = {0};
    MaterialBSDF bsdf;
    const Material* material = NULL;
    double transparency = 0.0;
    memset(&bsdf, 0, sizeof(bsdf));
    if (!object) return base_eval;
    MaterialBSDFInitFromSceneObject(object, &bsdf);
    material = MaterialManagerGet(object->material_id);
    transparency = material_editor_readback_alpha_transparency_bridge(material, object->alpha);
    if (object->material_id == MATERIAL_PRESET_TRANSPARENT) {
        double glass_transmission = transparency;
        if (SceneObjectResolveGlassTransport(object, &glass_transmission, NULL, NULL, NULL)) {
            transparency =
                material_editor_readback_clamp01(glass_transmission * object->alpha);
        }
    }
    if (object->material_id == MATERIAL_PRESET_MIRROR) {
        double mirror_reflectivity = bsdf.reflectivity;
        double mirror_roughness = bsdf.roughness;
        double mirror_specular = bsdf.specWeight;
        int mirror_tint = object->color & 0xFFFFFF;
        if (SceneObjectResolveMirrorResponse(object,
                                             &mirror_reflectivity,
                                             &mirror_roughness,
                                             &mirror_specular,
                                             &mirror_tint)) {
            bsdf.reflectivity = material_editor_readback_clamp01(mirror_reflectivity);
            bsdf.roughness = material_editor_readback_clamp01(mirror_roughness);
            if (bsdf.roughness < 0.02) bsdf.roughness = 0.02;
            bsdf.specWeight = material_editor_readback_clamp01(mirror_specular);
            bsdf.baseColorR = (double)((mirror_tint >> 16) & 0xFF) / 255.0;
            bsdf.baseColorG = (double)((mirror_tint >> 8) & 0xFF) / 255.0;
            bsdf.baseColorB = (double)(mirror_tint & 0xFF) / 255.0;
        }
    }
    return RuntimeMaterialSurfaceEvalMakeBase(bsdf.baseColorR,
                                              bsdf.baseColorG,
                                              bsdf.baseColorB,
                                              bsdf.roughness,
                                              bsdf.reflectivity,
                                              bsdf.specWeight,
                                              bsdf.diffuseWeight,
                                              transparency);
}

static double material_editor_readback_abs(double value) {
    return value < 0.0 ? -value : value;
}

static double material_editor_readback_color_delta(RuntimeMaterialSurfaceEval a,
                                                   RuntimeMaterialSurfaceEval b) {
    return (material_editor_readback_abs(a.colorR - b.colorR) +
            material_editor_readback_abs(a.colorG - b.colorG) +
            material_editor_readback_abs(a.colorB - b.colorB)) /
           3.0;
}

static void material_editor_populate_effective_layer_readback(
    MaterialEditorActiveLayerReadback* out_readback,
    const SceneObject* obj,
    const RuntimeMaterialTextureStack* stack,
    int active_index,
    int focused_index) {
    RuntimeMaterialTextureStack without_layer = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialSurfaceEval base_eval = {0};
    RuntimeMaterialSurfaceEval with_eval = {0};
    RuntimeMaterialSurfaceEval without_eval = {0};
    int seed = 0;
    bool with_active = false;
    bool without_active = false;
    if (!out_readback || !obj || !stack || active_index < 0 ||
        active_index >= stack->layerCount) {
        return;
    }
    without_layer = RuntimeMaterialTextureStackNormalize(*stack);
    without_layer.layers[active_index].enabled = false;
    without_layer = RuntimeMaterialTextureStackNormalize(without_layer);
    base_eval = material_editor_readback_base_surface_eval(obj);
    seed = obj->textureSeed != 0 ? obj->textureSeed : focused_index + 1;
    if (seed == 0) seed = 1;
    with_active = RuntimeMaterialTextureStackEvaluatePlacedUV(stack,
                                                              obj,
                                                              0.5,
                                                              0.5,
                                                              seed,
                                                              &base_eval,
                                                              &with_eval);
    without_active = RuntimeMaterialTextureStackEvaluatePlacedUV(&without_layer,
                                                                 obj,
                                                                 0.5,
                                                                 0.5,
                                                                 seed,
                                                                 &base_eval,
                                                                 &without_eval);
    if (!with_active) with_eval = base_eval;
    if (!without_active) without_eval = base_eval;
    out_readback->has_effective_readback = true;
    out_readback->effective_mask =
        material_editor_readback_clamp01(with_eval.layerMasks[active_index]);
    if (RuntimeMaterialTextureLayerKindIsBase(stack->layers[active_index].kind)) {
        double amount = material_editor_readback_clamp01(stack->layers[active_index].opacity *
                                                         stack->layers[active_index]
                                                             .placement.strength);
        if (amount > out_readback->effective_mask) {
            out_readback->effective_mask = amount;
        }
    }
    out_readback->effective_roughness_delta = with_eval.roughness - without_eval.roughness;
    out_readback->effective_reflectivity_delta =
        with_eval.reflectivity - without_eval.reflectivity;
    out_readback->effective_specular_delta = with_eval.specWeight - without_eval.specWeight;
    out_readback->effective_diffuse_delta =
        with_eval.diffuseWeight - without_eval.diffuseWeight;
    out_readback->effective_transparency_delta =
        with_eval.transparency - without_eval.transparency;
    out_readback->effective_color_delta =
        material_editor_readback_color_delta(with_eval, without_eval);
    snprintf(out_readback->effective_summary,
             sizeof(out_readback->effective_summary),
             "Effective @ center: mask %.2f | color d %.2f",
             out_readback->effective_mask,
             out_readback->effective_color_delta);
    snprintf(out_readback->effective_delta_summary,
             sizeof(out_readback->effective_delta_summary),
             "dR %+0.2f dRefl %+0.2f dSpec %+0.2f dDiff %+0.2f dTrans %+0.2f",
             out_readback->effective_roughness_delta,
             out_readback->effective_reflectivity_delta,
             out_readback->effective_specular_delta,
             out_readback->effective_diffuse_delta,
             out_readback->effective_transparency_delta);
}

bool MaterialEditorBuildMaterialReadback(MaterialEditorMaterialReadback* out_readback) {
    SceneObject* obj = material_editor_focused_object();
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    char manifest_path[256];
    char binding_mode[64];
    int face_count = 0;
    int focused_index = MaterialEditorResolveFocusedObjectIndex();
    bool has_stack = false;
    bool has_graph = false;
    bool has_binding = false;
    if (!out_readback) return false;
    memset(out_readback, 0, sizeof(*out_readback));
    out_readback->scene_object_index = focused_index;
    out_readback->material_id = obj ? obj->material_id : MaterialManagerDefaultId();
    snprintf(out_readback->save_request_label,
             sizeof(out_readback->save_request_label),
             "save_preset_request_deferred");
    out_readback->save_request_deferred = true;
    if (!obj || focused_index < 0) {
        snprintf(out_readback->preset_label, sizeof(out_readback->preset_label), "No object");
        snprintf(out_readback->state_label, sizeof(out_readback->state_label), "No material focus");
        snprintf(out_readback->source_label, sizeof(out_readback->source_label), "none");
        return false;
    }

    out_readback->preset_valid =
        obj->material_id >= 0 && obj->material_id < MaterialManagerCount();
    snprintf(out_readback->preset_label,
             sizeof(out_readback->preset_label),
             "%s #%d",
             out_readback->preset_valid ? material_editor_preset_label(obj->material_id)
                                        : "Invalid preset",
             obj->material_id);

    has_stack = SceneEditorMaterialStackGetObjectStack(focused_index, &stack);
    has_graph = SceneEditorMaterialGraphHasObjectGraph(focused_index);
    has_binding = MaterialEditorAuthoredTextureBindingGetSummary(focused_index,
                                                                 manifest_path,
                                                                 sizeof(manifest_path),
                                                                 binding_mode,
                                                                 sizeof(binding_mode),
                                                                 &face_count);
    out_readback->custom_stack = has_stack;
    out_readback->authored_texture_bound = has_binding;
    out_readback->graph_backed = has_graph;
    out_readback->stack_layer_count = has_stack ? stack.layerCount : 0;

    if (has_graph) {
        snprintf(out_readback->state_label,
                 sizeof(out_readback->state_label),
                 "Graph-backed custom");
        snprintf(out_readback->source_label,
                 sizeof(out_readback->source_label),
                 "graph_document + compiled_stack");
    } else if (has_stack || has_binding) {
        snprintf(out_readback->state_label,
                 sizeof(out_readback->state_label),
                 "Customized material");
        snprintf(out_readback->source_label,
                 sizeof(out_readback->source_label),
                 has_binding ? "custom_stack + authored_texture" : "custom_stack");
    } else {
        snprintf(out_readback->state_label,
                 sizeof(out_readback->state_label),
                 "Preset material");
        snprintf(out_readback->source_label,
                 sizeof(out_readback->source_label),
                 "object_preset_compat");
    }
    return true;
}

bool MaterialEditorBuildActiveLayerReadback(MaterialEditorActiveLayerReadback* out_readback) {
    SceneObject* obj = material_editor_focused_object();
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialTextureLayer layer;
    int index = 0;
    int focused_index = MaterialEditorResolveFocusedObjectIndex();
    MaterialEditorMutationDestination destination =
        MaterialEditorMutationDestinationForFocusedTextureControls();
    const char* role = "Layer";
    const char* kind = "None";
    if (!out_readback) return false;
    memset(out_readback, 0, sizeof(*out_readback));
    out_readback->active_index = -1;
    snprintf(out_readback->source_label, sizeof(out_readback->source_label), "none");
    snprintf(out_readback->edit_owner_label, sizeof(out_readback->edit_owner_label), "none");
    if (!material_editor_get_active_layer(obj, &stack, &layer, &index)) {
        snprintf(out_readback->title, sizeof(out_readback->title), "No active layer");
        snprintf(out_readback->detail,
                 sizeof(out_readback->detail),
                 "Select or create a stack layer before editing response.");
        return false;
    }
    out_readback->has_layer = true;
    out_readback->active_index = index;
    out_readback->layer_count = stack.layerCount;
    out_readback->enabled = layer.enabled;
    out_readback->persisted_stack = SceneEditorMaterialStackHasObjectStack(focused_index);
    out_readback->base_layer = RuntimeMaterialTextureLayerKindIsBase(layer.kind);
    out_readback->opacity = layer.opacity;
    out_readback->strength = layer.placement.strength;
    out_readback->roughness_influence = layer.roughnessInfluence;
    out_readback->reflectivity_influence = layer.reflectivityInfluence;
    out_readback->specular_influence = layer.specularInfluence;
    out_readback->diffuse_influence = layer.diffuseInfluence;
    out_readback->transparency_influence = layer.transparencyInfluence;
    role = out_readback->base_layer ? "Base" : "Overlay";
    kind = RuntimeMaterialTextureLayerKindDisplayName(layer.kind);
    snprintf(out_readback->role_label, sizeof(out_readback->role_label), "%s", role);
    snprintf(out_readback->kind_label, sizeof(out_readback->kind_label), "%s", kind);
    snprintf(out_readback->layer_id,
             sizeof(out_readback->layer_id),
             "%s",
             layer.layerId[0] ? layer.layerId : "layer");
    if (focused_index >= 0 && s_material_editor_active_face_group_index >= 0) {
        snprintf(out_readback->source_label, sizeof(out_readback->source_label), "face selection");
    } else if (out_readback->persisted_stack) {
        snprintf(out_readback->source_label, sizeof(out_readback->source_label), "object stack");
    } else if (focused_index >= 0 && SceneEditorMaterialGraphHasObjectGraph(focused_index)) {
        snprintf(out_readback->source_label, sizeof(out_readback->source_label), "graph compiled stack");
    } else {
        snprintf(out_readback->source_label, sizeof(out_readback->source_label), "effective fallback");
    }
    snprintf(out_readback->edit_owner_label,
             sizeof(out_readback->edit_owner_label),
             "%s",
             MaterialEditorMutationDestinationLabel(destination));
    snprintf(out_readback->state_label,
             sizeof(out_readback->state_label),
             "%s | opacity %.2f | strength %.2f",
             layer.enabled ? "enabled" : "muted",
             out_readback->opacity,
             out_readback->strength);
    snprintf(out_readback->response_summary,
             sizeof(out_readback->response_summary),
             "R %+0.2f Refl %+0.2f Spec %+0.2f Diff %+0.2f Trans %+0.2f",
             out_readback->roughness_influence,
             out_readback->reflectivity_influence,
             out_readback->specular_influence,
             out_readback->diffuse_influence,
             out_readback->transparency_influence);
    material_editor_populate_effective_layer_readback(out_readback,
                                                      obj,
                                                      &stack,
                                                      index,
                                                      focused_index);
    snprintf(out_readback->title,
             sizeof(out_readback->title),
             "Editing %s %d/%d",
             role,
             index + 1,
             stack.layerCount);
    snprintf(out_readback->detail,
             sizeof(out_readback->detail),
             "%s | %s | %s -> %s",
             layer.displayName[0] ? layer.displayName : kind,
             out_readback->layer_id,
             out_readback->source_label,
             out_readback->edit_owner_label);
    return true;
}
