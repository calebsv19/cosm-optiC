#include "editor/material_editor_internal.h"

#include <stdio.h>
#include <string.h>

#include "editor/material_editor_authored_texture_binding.h"
#include "editor/material_editor_layer_model.h"
#include "editor/scene_editor_material_graph.h"
#include "editor/scene_editor_material_stack.h"
#include "material/material.h"
#include "material/material_manager.h"

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
