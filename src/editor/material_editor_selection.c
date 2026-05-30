#include "editor/material_editor_internal.h"

#include "editor/material_editor_face_preview.h"

bool MaterialEditorSetTriangleSelection(const SceneEditorMaterialPreviewTriangleAddress* address) {
    if (!address) return false;
    s_material_editor_selected_triangles[0] = *address;
    s_material_editor_selected_triangle_count = 1;
    s_material_editor_active_face_group_index = address->faceGroupIndex;
    MaterialEditorFacePreviewInvalidate();
    return true;
}

bool MaterialEditorToggleTriangleSelection(const SceneEditorMaterialPreviewTriangleAddress* address) {
    int existing = material_editor_find_selected_triangle(address);
    if (!address) return false;
    if (existing >= 0) {
        int removed_face_group = s_material_editor_selected_triangles[existing].faceGroupIndex;
        for (int i = existing; i + 1 < s_material_editor_selected_triangle_count; ++i) {
            s_material_editor_selected_triangles[i] = s_material_editor_selected_triangles[i + 1];
        }
        s_material_editor_selected_triangle_count -= 1;
        if (s_material_editor_selected_triangle_count < 0) {
            s_material_editor_selected_triangle_count = 0;
        }
        if (!material_editor_selected_face_group_exists(removed_face_group) &&
            s_material_editor_active_face_group_index == removed_face_group) {
            s_material_editor_active_face_group_index = -1;
        }
        MaterialEditorFacePreviewInvalidate();
        return true;
    }
    if (!material_editor_add_triangle_selection(address)) return false;
    s_material_editor_active_face_group_index = address->faceGroupIndex;
    MaterialEditorFacePreviewInvalidate();
    return true;
}

bool MaterialEditorSetFaceGroupSelection(const SceneEditorMaterialPreviewTriangleAddress* address) {
    SceneEditorMaterialPreviewTriangleAddress group_addresses[SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES];
    int group_count = material_editor_collect_face_group_triangles(address,
                                                                   group_addresses,
                                                                   SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES);
    if (!address || group_count <= 0) return false;
    MaterialEditorClearTriangleSelection();
    for (int i = 0; i < group_count; ++i) {
        if (!material_editor_add_triangle_selection(&group_addresses[i])) return false;
    }
    s_material_editor_active_face_group_index = address->faceGroupIndex;
    MaterialEditorFacePreviewInvalidate();
    return true;
}

bool MaterialEditorToggleFaceGroupSelection(const SceneEditorMaterialPreviewTriangleAddress* address) {
    SceneEditorMaterialPreviewTriangleAddress group_addresses[SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES];
    int group_count = 0;
    if (!address) return false;
    if (material_editor_face_group_fully_selected(address)) {
        bool removed = material_editor_remove_face_group_selection(address->sceneObjectIndex,
                                                                   address->faceGroupIndex);
        if (removed && s_material_editor_active_face_group_index == address->faceGroupIndex) {
            s_material_editor_active_face_group_index = -1;
        }
        if (removed) {
            MaterialEditorFacePreviewInvalidate();
        }
        return removed;
    }
    group_count = material_editor_collect_face_group_triangles(address,
                                                              group_addresses,
                                                              SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES);
    if (group_count <= 0) return false;
    for (int i = 0; i < group_count; ++i) {
        if (!material_editor_add_triangle_selection(&group_addresses[i])) return false;
    }
    s_material_editor_active_face_group_index = address->faceGroupIndex;
    MaterialEditorFacePreviewInvalidate();
    return true;
}
