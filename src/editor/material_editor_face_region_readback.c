#include "editor/material_editor_internal.h"

#include <stdio.h>
#include <string.h>

#include "editor/material_editor_layer_model.h"
#include "editor/scene_editor_material_stack.h"

bool MaterialEditorBuildFaceRegionReadback(MaterialEditorFaceRegionReadback* out_readback) {
    SceneObject* obj = material_editor_focused_object();
    RuntimeMaterialTextureLayer active_layer = {0};
    int focused_index = MaterialEditorResolveFocusedObjectIndex();
    int active_group = MaterialEditorGetActiveFaceGroupIndex();
    bool has_object_stack = false;
    bool layer_override = false;
    bool legacy_override = false;
    if (!out_readback) return false;
    memset(out_readback, 0, sizeof(*out_readback));
    out_readback->focused_object_index = focused_index;
    out_readback->active_face_group_index = active_group;
    out_readback->focused_face_group_count = MaterialEditorFocusedFaceGroupCount();
    out_readback->selected_face_group_count = MaterialEditorSelectedFaceGroupCount();
    out_readback->has_active_face_group = active_group >= 0;

    if (!obj || focused_index < 0) {
        snprintf(out_readback->active_label,
                 sizeof(out_readback->active_label),
                 "No focused object");
        snprintf(out_readback->selection_label,
                 sizeof(out_readback->selection_label),
                 "Selected groups 0 / focused groups 0");
        snprintf(out_readback->layer_label, sizeof(out_readback->layer_label), "Layer none");
        snprintf(out_readback->override_label,
                 sizeof(out_readback->override_label),
                 "Override unavailable");
        return false;
    }

    if (active_group >= 0) {
        snprintf(out_readback->active_label,
                 sizeof(out_readback->active_label),
                 "Active Face #%d",
                 active_group);
    } else {
        snprintf(out_readback->active_label,
                 sizeof(out_readback->active_label),
                 "No active face group");
    }
    snprintf(out_readback->selection_label,
             sizeof(out_readback->selection_label),
             "Selected groups %d / focused groups %d",
             out_readback->selected_face_group_count,
             out_readback->focused_face_group_count);

    out_readback->has_active_layer = material_editor_get_active_layer(obj,
                                                                      NULL,
                                                                      &active_layer,
                                                                      NULL);
    has_object_stack = SceneEditorMaterialStackHasObjectStack(focused_index);
    if (out_readback->has_active_layer) {
        const char* kind = RuntimeMaterialTextureLayerKindDisplayName(active_layer.kind);
        snprintf(out_readback->layer_label,
                 sizeof(out_readback->layer_label),
                 "Layer %s | %s",
                 active_layer.displayName[0] ? active_layer.displayName : kind,
                 active_layer.layerId[0] ? active_layer.layerId : "layer");
    } else {
        snprintf(out_readback->layer_label,
                 sizeof(out_readback->layer_label),
                 "Layer object texture fallback");
    }

    if (active_group >= 0) {
        if (has_object_stack && out_readback->has_active_layer) {
            layer_override = SceneEditorMaterialFacePlacementHasOverrideForLayer(
                focused_index,
                active_group,
                active_layer.layerId);
        }
        legacy_override = SceneEditorMaterialFacePlacementHasOverrideForLayer(focused_index,
                                                                              active_group,
                                                                              "");
    }
    out_readback->has_layer_specific_override = layer_override;
    out_readback->has_legacy_override = legacy_override;
    out_readback->can_reset =
        has_object_stack && out_readback->has_active_layer ? layer_override : legacy_override;
    out_readback->can_copy_to_selected =
        active_group >= 0 && out_readback->selected_face_group_count > 1;

    if (layer_override) {
        snprintf(out_readback->override_label,
                 sizeof(out_readback->override_label),
                 "Override layer-specific");
    } else if (legacy_override) {
        snprintf(out_readback->override_label,
                 sizeof(out_readback->override_label),
                 "Override object-face");
    } else if (active_group >= 0) {
        snprintf(out_readback->override_label,
                 sizeof(out_readback->override_label),
                 "Override default inherited");
    } else {
        snprintf(out_readback->override_label,
                 sizeof(out_readback->override_label),
                 "Override select a face group");
    }
    return true;
}
