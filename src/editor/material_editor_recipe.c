#include "editor/material_editor_internal.h"

#include <stdio.h>
#include <string.h>

#include "editor/material_editor_face_preview.h"
#include "editor/material_editor_layer_model.h"
#include "editor/object_editor_object_ops.h"
#include "editor/scene_editor_material_graph.h"
#include "editor/scene_editor_material_stack.h"
#include "material/material.h"

static const RuntimeMaterialTextureLayerKind s_recipe_surface_cycle[] = {
    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID,
    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD,
    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_BRUSHED_METAL,
    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_STONE,
    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_CONCRETE
};

static const RuntimeMaterialTextureLayerKind s_recipe_finish_cycle[] = {
    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE,
    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST,
    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME,
    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL,
    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG,
    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SCRATCHES,
    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_EDGE_WEAR
};

static const char* material_editor_recipe_family_label(int material_id) {
    switch (material_id) {
        case MATERIAL_PRESET_TRANSPARENT:
            return "Glass";
        case MATERIAL_PRESET_MIRROR:
            return "Mirror";
        case MATERIAL_PRESET_ROUGH_METAL:
            return "Metal";
        case MATERIAL_PRESET_GLOSSY:
            return "Glossy Plastic";
        case MATERIAL_PRESET_EMISSIVE:
            return "Emissive";
        case MATERIAL_PRESET_DEFAULT:
        default:
            return "Diffuse";
    }
}

static int material_editor_recipe_next_family_id(int material_id) {
    switch (material_id) {
        case MATERIAL_PRESET_DEFAULT:
            return MATERIAL_PRESET_TRANSPARENT;
        case MATERIAL_PRESET_TRANSPARENT:
            return MATERIAL_PRESET_MIRROR;
        case MATERIAL_PRESET_MIRROR:
            return MATERIAL_PRESET_ROUGH_METAL;
        case MATERIAL_PRESET_ROUGH_METAL:
            return MATERIAL_PRESET_GLOSSY;
        case MATERIAL_PRESET_GLOSSY:
            return MATERIAL_PRESET_EMISSIVE;
        case MATERIAL_PRESET_EMISSIVE:
        default:
            return MATERIAL_PRESET_DEFAULT;
    }
}

static RuntimeMaterialTextureLayerKind material_editor_recipe_surface_kind(
    const SceneObject* obj,
    int focused_index,
    bool* out_has_stack) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    if (out_has_stack) *out_has_stack = false;
    if (MaterialEditorLayerModelGetEffectiveStack(obj, focused_index, &stack) &&
        stack.layerCount > 0) {
        if (out_has_stack) *out_has_stack = SceneEditorMaterialStackHasObjectStack(focused_index);
        if (RuntimeMaterialTextureLayerKindIsBase(stack.layers[0].kind)) {
            return stack.layers[0].kind;
        }
    }
    return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID;
}

static RuntimeMaterialTextureLayerKind material_editor_recipe_finish_kind(const SceneObject* obj,
                                                                          int focused_index) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    if (!MaterialEditorLayerModelGetEffectiveStack(obj, focused_index, &stack)) {
        return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE;
    }
    for (int i = 1; i < stack.layerCount; ++i) {
        if (RuntimeMaterialTextureLayerKindIsOverlay(stack.layers[i].kind)) {
            return stack.layers[i].kind;
        }
    }
    return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE;
}

static int material_editor_recipe_kind_index(const RuntimeMaterialTextureLayerKind* kinds,
                                             int count,
                                             RuntimeMaterialTextureLayerKind kind) {
    for (int i = 0; i < count; ++i) {
        if (kinds[i] == kind) return i;
    }
    return 0;
}

static void material_editor_recipe_add_option(MaterialEditorRecipeOption* options,
                                              int capacity,
                                              int* count,
                                              const char* label,
                                              int material_id,
                                              RuntimeMaterialTextureLayerKind kind,
                                              bool selected) {
    MaterialEditorRecipeOption* option = NULL;
    if (!options || !count || *count < 0 || *count >= capacity || !label) return;
    option = &options[*count];
    memset(option, 0, sizeof(*option));
    snprintf(option->label, sizeof(option->label), "%s", label);
    option->material_id = material_id;
    option->layer_kind = kind;
    option->selected = selected;
    option->compatible = true;
    *count += 1;
}

static bool material_editor_recipe_surface_compatible(int material_id,
                                                      RuntimeMaterialTextureLayerKind kind) {
    switch (material_id) {
        case MATERIAL_PRESET_TRANSPARENT:
            return kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID;
        case MATERIAL_PRESET_MIRROR:
            return kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID ||
                   kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_BRUSHED_METAL;
        case MATERIAL_PRESET_ROUGH_METAL:
            return kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID ||
                   kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_BRUSHED_METAL;
        case MATERIAL_PRESET_GLOSSY:
            return kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID ||
                   kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD ||
                   kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_STONE;
        case MATERIAL_PRESET_EMISSIVE:
            return kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID;
        case MATERIAL_PRESET_DEFAULT:
        default:
            return RuntimeMaterialTextureLayerKindIsBase(kind);
    }
}

static bool material_editor_recipe_finish_compatible(int material_id,
                                                     RuntimeMaterialTextureLayerKind kind) {
    if (kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE) return true;
    switch (material_id) {
        case MATERIAL_PRESET_TRANSPARENT:
            return kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG ||
                   kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME ||
                   kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL ||
                   kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SCRATCHES;
        case MATERIAL_PRESET_MIRROR:
            return kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME ||
                   kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SCRATCHES;
        case MATERIAL_PRESET_ROUGH_METAL:
            return kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST ||
                   kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME ||
                   kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL ||
                   kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SCRATCHES ||
                   kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_EDGE_WEAR;
        case MATERIAL_PRESET_GLOSSY:
            return kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME ||
                   kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL ||
                   kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SCRATCHES;
        case MATERIAL_PRESET_EMISSIVE:
            return false;
        case MATERIAL_PRESET_DEFAULT:
        default:
            return RuntimeMaterialTextureLayerKindIsOverlay(kind);
    }
}

static bool material_editor_recipe_get_stack(SceneObject* obj,
                                             int focused_index,
                                             RuntimeMaterialTextureStack* out_stack) {
    if (!obj || focused_index < 0 || !out_stack) return false;
    if (!MaterialEditorLayerModelEnsureEditableStack(obj, focused_index, out_stack)) {
        return false;
    }
    if (out_stack->layerCount <= 0) {
        out_stack->layers[0] =
            RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID);
        out_stack->layerCount = 1;
    }
    return true;
}

static bool material_editor_recipe_set_surface_for_focused(RuntimeMaterialTextureLayerKind kind) {
    SceneObject* obj = material_editor_focused_object();
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialTextureLayer previous;
    int focused_index = MaterialEditorResolveFocusedObjectIndex();
    if (!RuntimeMaterialTextureLayerKindIsBase(kind)) return false;
    if (!material_editor_recipe_get_stack(obj, focused_index, &stack)) return false;
    previous = stack.layers[0];
    stack.layers[0] = RuntimeMaterialTextureLayerMakeBase(kind);
    stack.layers[0].enabled = previous.enabled;
    stack.layers[0].opacity = previous.opacity;
    stack.layers[0].placement.strength = previous.placement.strength;
    stack.layers[0] = RuntimeMaterialTextureLayerNormalize(stack.layers[0]);
    if (!MaterialEditorLayerModelSetStack(focused_index, &stack)) return false;
    MarkObjectDirty(obj);
    MaterialEditorFacePreviewInvalidate();
    return true;
}

static bool material_editor_recipe_set_finish_for_focused(RuntimeMaterialTextureLayerKind kind) {
    SceneObject* obj = material_editor_focused_object();
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    int focused_index = MaterialEditorResolveFocusedObjectIndex();
    int first_overlay = -1;
    if (kind != RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE &&
        !RuntimeMaterialTextureLayerKindIsOverlay(kind)) {
        return false;
    }
    if (!material_editor_recipe_get_stack(obj, focused_index, &stack)) return false;
    for (int i = 1; i < stack.layerCount; ++i) {
        if (RuntimeMaterialTextureLayerKindIsOverlay(stack.layers[i].kind)) {
            first_overlay = i;
            break;
        }
    }
    if (kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE) {
        if (first_overlay >= 0) {
            for (int i = first_overlay; i + 1 < stack.layerCount; ++i) {
                stack.layers[i] = stack.layers[i + 1];
            }
            memset(&stack.layers[stack.layerCount - 1],
                   0,
                   sizeof(stack.layers[stack.layerCount - 1]));
            stack.layerCount -= 1;
        }
    } else if (first_overlay >= 0) {
        stack.layers[first_overlay] = RuntimeMaterialTextureLayerMakeOverlay(kind);
    } else {
        if (stack.layerCount >= RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS) return false;
        stack.layers[stack.layerCount] = RuntimeMaterialTextureLayerMakeOverlay(kind);
        stack.layerCount += 1;
    }
    if (!MaterialEditorLayerModelSetStack(focused_index, &stack)) return false;
    MarkObjectDirty(obj);
    MaterialEditorFacePreviewInvalidate();
    return true;
}

bool MaterialEditorBuildRecipeReadback(MaterialEditorRecipeReadback* out_readback) {
    SceneObject* obj = material_editor_focused_object();
    int focused_index = MaterialEditorResolveFocusedObjectIndex();
    bool has_stack = false;
    if (!out_readback) return false;
    memset(out_readback, 0, sizeof(*out_readback));
    out_readback->material_id = obj ? obj->material_id : MATERIAL_PRESET_DEFAULT;
    out_readback->surface_kind =
        material_editor_recipe_surface_kind(obj, focused_index, &has_stack);
    out_readback->finish_kind = material_editor_recipe_finish_kind(obj, focused_index);
    out_readback->has_custom_stack = has_stack;
    out_readback->has_finish_overlay =
        out_readback->finish_kind != RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE;
    out_readback->graph_backed = focused_index >= 0 &&
                                 SceneEditorMaterialGraphHasObjectGraph(focused_index);
    snprintf(out_readback->family_label,
             sizeof(out_readback->family_label),
             "%s",
             material_editor_recipe_family_label(out_readback->material_id));
    snprintf(out_readback->surface_label,
             sizeof(out_readback->surface_label),
             "%s%s",
             RuntimeMaterialTextureLayerKindDisplayName(out_readback->surface_kind),
             out_readback->surface_kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID
                 ? " Tint"
                 : "");
    snprintf(out_readback->finish_label,
             sizeof(out_readback->finish_label),
             "%s",
             out_readback->has_finish_overlay
                 ? RuntimeMaterialTextureLayerKindDisplayName(out_readback->finish_kind)
                 : "Clear");
    snprintf(out_readback->header_label,
             sizeof(out_readback->header_label),
             "Material %s | Surface %s | Finish %s",
             out_readback->family_label,
             out_readback->surface_label,
             out_readback->finish_label);
    snprintf(out_readback->detail_label,
             sizeof(out_readback->detail_label),
             "%s%s%s",
             out_readback->has_custom_stack ? "editable stack" : "preset stack seed",
             out_readback->graph_backed ? " | graph-backed" : "",
             out_readback->has_finish_overlay ? " | overlay finish" : "");
    return obj != NULL;
}

const char* MaterialEditorRecipeAxisLabel(MaterialEditorRecipeAxis axis) {
    switch (axis) {
        case MATERIAL_EDITOR_RECIPE_AXIS_FAMILY:
            return "Family";
        case MATERIAL_EDITOR_RECIPE_AXIS_SURFACE:
            return "Surface";
        case MATERIAL_EDITOR_RECIPE_AXIS_FINISH:
            return "Finish";
        case MATERIAL_EDITOR_RECIPE_AXIS_NONE:
        default:
            return "";
    }
}

int MaterialEditorBuildRecipeOptions(MaterialEditorRecipeAxis axis,
                                     MaterialEditorRecipeOption* out_options,
                                     int option_capacity) {
    MaterialEditorRecipeReadback readback = {0};
    int count = 0;
    if (!out_options || option_capacity <= 0) return 0;
    memset(out_options, 0, sizeof(out_options[0]) * (size_t)option_capacity);
    if (!MaterialEditorBuildRecipeReadback(&readback)) return 0;
    if (axis == MATERIAL_EDITOR_RECIPE_AXIS_FAMILY) {
        static const int family_ids[] = {
            MATERIAL_PRESET_DEFAULT,
            MATERIAL_PRESET_TRANSPARENT,
            MATERIAL_PRESET_MIRROR,
            MATERIAL_PRESET_ROUGH_METAL,
            MATERIAL_PRESET_GLOSSY,
            MATERIAL_PRESET_EMISSIVE
        };
        for (int i = 0; i < (int)(sizeof(family_ids) / sizeof(family_ids[0])); ++i) {
            material_editor_recipe_add_option(out_options,
                                              option_capacity,
                                              &count,
                                              material_editor_recipe_family_label(family_ids[i]),
                                              family_ids[i],
                                              RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE,
                                              readback.material_id == family_ids[i]);
        }
    } else if (axis == MATERIAL_EDITOR_RECIPE_AXIS_SURFACE) {
        for (int i = 0; i < (int)(sizeof(s_recipe_surface_cycle) /
                                  sizeof(s_recipe_surface_cycle[0])); ++i) {
            RuntimeMaterialTextureLayerKind kind = s_recipe_surface_cycle[i];
            char label[64];
            if (!material_editor_recipe_surface_compatible(readback.material_id, kind)) continue;
            snprintf(label,
                     sizeof(label),
                     "%s%s",
                     RuntimeMaterialTextureLayerKindDisplayName(kind),
                     kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID ? " Tint" : "");
            material_editor_recipe_add_option(out_options,
                                              option_capacity,
                                              &count,
                                              label,
                                              MATERIAL_PRESET_DEFAULT,
                                              kind,
                                              readback.surface_kind == kind);
        }
    } else if (axis == MATERIAL_EDITOR_RECIPE_AXIS_FINISH) {
        for (int i = 0; i < (int)(sizeof(s_recipe_finish_cycle) /
                                  sizeof(s_recipe_finish_cycle[0])); ++i) {
            RuntimeMaterialTextureLayerKind kind = s_recipe_finish_cycle[i];
            const char* label = kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE
                                    ? "Clear"
                                    : RuntimeMaterialTextureLayerKindDisplayName(kind);
            if (!material_editor_recipe_finish_compatible(readback.material_id, kind)) continue;
            material_editor_recipe_add_option(out_options,
                                              option_capacity,
                                              &count,
                                              label,
                                              MATERIAL_PRESET_DEFAULT,
                                              kind,
                                              readback.finish_kind == kind);
        }
    }
    return count;
}

void MaterialEditorSetRecipeMenuAxis(MaterialEditorRecipeAxis axis) {
    if (axis < MATERIAL_EDITOR_RECIPE_AXIS_NONE ||
        axis > MATERIAL_EDITOR_RECIPE_AXIS_FINISH) {
        axis = MATERIAL_EDITOR_RECIPE_AXIS_NONE;
    }
    s_material_editor_recipe_menu_axis = axis;
}

MaterialEditorRecipeAxis MaterialEditorGetRecipeMenuAxis(void) {
    return s_material_editor_recipe_menu_axis;
}

bool MaterialEditorToggleRecipeMenuAxis(MaterialEditorRecipeAxis axis) {
    if (axis < MATERIAL_EDITOR_RECIPE_AXIS_FAMILY ||
        axis > MATERIAL_EDITOR_RECIPE_AXIS_FINISH) {
        MaterialEditorSetRecipeMenuAxis(MATERIAL_EDITOR_RECIPE_AXIS_NONE);
        return false;
    }
    if (s_material_editor_recipe_menu_axis == axis) {
        MaterialEditorSetRecipeMenuAxis(MATERIAL_EDITOR_RECIPE_AXIS_NONE);
        return false;
    }
    MaterialEditorSetIdentityPopoverOpen(false);
    MaterialEditorSetRecipeMenuAxis(axis);
    return true;
}

bool MaterialEditorApplyRecipeOptionForFocused(MaterialEditorRecipeAxis axis, int option_index) {
    SceneObject* obj = material_editor_focused_object();
    MaterialEditorRecipeOption options[MATERIAL_EDITOR_RECIPE_MENU_MAX_ITEMS];
    int option_count = 0;
    if (!obj) return false;
    option_count = MaterialEditorBuildRecipeOptions(axis,
                                                    options,
                                                    MATERIAL_EDITOR_RECIPE_MENU_MAX_ITEMS);
    if (option_index < 0 || option_index >= option_count) return false;
    if (axis == MATERIAL_EDITOR_RECIPE_AXIS_FAMILY) {
        ObjectEditorObjectAssignMaterial(obj, options[option_index].material_id);
        MaterialEditorFacePreviewInvalidate();
    } else if (axis == MATERIAL_EDITOR_RECIPE_AXIS_SURFACE) {
        if (!material_editor_recipe_set_surface_for_focused(options[option_index].layer_kind)) {
            return false;
        }
    } else if (axis == MATERIAL_EDITOR_RECIPE_AXIS_FINISH) {
        if (!material_editor_recipe_set_finish_for_focused(options[option_index].layer_kind)) {
            return false;
        }
    } else {
        return false;
    }
    MaterialEditorSetRecipeMenuAxis(MATERIAL_EDITOR_RECIPE_AXIS_NONE);
    return true;
}

bool MaterialEditorCycleRecipeFamilyForFocused(void) {
    SceneObject* obj = material_editor_focused_object();
    if (!obj) return false;
    ObjectEditorObjectAssignMaterial(obj, material_editor_recipe_next_family_id(obj->material_id));
    MaterialEditorFacePreviewInvalidate();
    return true;
}

bool MaterialEditorCycleRecipeSurfaceForFocused(void) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialTextureLayerKind current = RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID;
    RuntimeMaterialTextureLayerKind next = RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID;
    int focused_index = MaterialEditorResolveFocusedObjectIndex();
    int index = 0;
    if (!material_editor_recipe_get_stack(material_editor_focused_object(), focused_index, &stack)) {
        return false;
    }
    current = RuntimeMaterialTextureLayerKindIsBase(stack.layers[0].kind)
                  ? stack.layers[0].kind
                  : RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID;
    index = material_editor_recipe_kind_index(s_recipe_surface_cycle,
                                              (int)(sizeof(s_recipe_surface_cycle) /
                                                    sizeof(s_recipe_surface_cycle[0])),
                                              current);
    next = s_recipe_surface_cycle[(index + 1) %
                                  (int)(sizeof(s_recipe_surface_cycle) /
                                        sizeof(s_recipe_surface_cycle[0]))];
    return material_editor_recipe_set_surface_for_focused(next);
}

bool MaterialEditorCycleRecipeFinishForFocused(void) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialTextureLayerKind current = RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE;
    RuntimeMaterialTextureLayerKind next = RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE;
    int focused_index = MaterialEditorResolveFocusedObjectIndex();
    int index = 0;
    int first_overlay = -1;
    int cycle_count = (int)(sizeof(s_recipe_finish_cycle) / sizeof(s_recipe_finish_cycle[0]));
    if (!material_editor_recipe_get_stack(material_editor_focused_object(), focused_index, &stack)) {
        return false;
    }
    for (int i = 1; i < stack.layerCount; ++i) {
        if (RuntimeMaterialTextureLayerKindIsOverlay(stack.layers[i].kind)) {
            first_overlay = i;
            current = stack.layers[i].kind;
            break;
        }
    }
    index = material_editor_recipe_kind_index(s_recipe_finish_cycle, cycle_count, current);
    next = s_recipe_finish_cycle[(index + 1) % cycle_count];
    (void)first_overlay;
    return material_editor_recipe_set_finish_for_focused(next);
}
