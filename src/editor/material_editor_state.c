#include "editor/material_editor_internal.h"

#include <string.h>

#include "editor/material_editor_authored_texture_binding.h"
#include "editor/material_editor_face_preview.h"
#include "editor/material_editor_layer_model.h"
#include "editor/object_editor_selection_tracker.h"

static bool material_editor_triangle_address_equal(
    const SceneEditorMaterialPreviewTriangleAddress* lhs,
    const SceneEditorMaterialPreviewTriangleAddress* rhs) {
    if (!lhs || !rhs) return false;
    return lhs->sceneObjectIndex == rhs->sceneObjectIndex &&
           lhs->primitiveIndex == rhs->primitiveIndex &&
           lhs->triangleIndex == rhs->triangleIndex &&
           lhs->localTriangleIndex == rhs->localTriangleIndex &&
           lhs->faceGroupIndex == rhs->faceGroupIndex;
}

static bool material_editor_selected_face_group_seen(int face_group_index, int limit) {
    if (face_group_index < 0) return false;
    for (int i = 0; i < limit && i < s_material_editor_selected_triangle_count; ++i) {
        if (s_material_editor_selected_triangles[i].faceGroupIndex == face_group_index) {
            return true;
        }
    }
    return false;
}

bool material_editor_selected_face_group_exists(int face_group_index) {
    if (face_group_index < 0) return false;
    for (int i = 0; i < s_material_editor_selected_triangle_count; ++i) {
        if (s_material_editor_selected_triangles[i].faceGroupIndex == face_group_index) {
            return true;
        }
    }
    return false;
}

static int material_editor_find_face_group_info(const MaterialEditorFaceGroupInfo* groups,
                                                int group_count,
                                                int face_group_index) {
    if (!groups || face_group_index < 0) return -1;
    for (int i = 0; i < group_count; ++i) {
        if (groups[i].face_group_index == face_group_index) return i;
    }
    return -1;
}

int material_editor_find_selected_triangle(
    const SceneEditorMaterialPreviewTriangleAddress* address) {
    if (!address) return -1;
    for (int i = 0; i < s_material_editor_selected_triangle_count; ++i) {
        if (material_editor_triangle_address_equal(&s_material_editor_selected_triangles[i],
                                                   address)) {
            return i;
        }
    }
    return -1;
}

int material_editor_collect_focused_face_groups(MaterialEditorFaceGroupInfo* out_groups,
                                                int group_capacity) {
    SceneEditorMaterialPreviewTriangleAddress addresses[SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES];
    SceneEditorMaterialPreviewStats stats = {0};
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    int group_count = 0;
    if (!out_groups || group_capacity <= 0 || focused_object_index < 0) return 0;
    memset(out_groups, 0, sizeof(MaterialEditorFaceGroupInfo) * (size_t)group_capacity);
    memset(addresses, 0, sizeof(addresses));
    if (!SceneEditorMaterialPreviewResolveFocusedTriangles(focused_object_index,
                                                          NULL,
                                                          addresses,
                                                          SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES,
                                                          &stats)) {
        return 0;
    }
    for (int i = 0; i < stats.triangleCount; ++i) {
        int face_group = addresses[i].faceGroupIndex;
        int index = material_editor_find_face_group_info(out_groups, group_count, face_group);
        if (face_group < 0) continue;
        if (index < 0) {
            if (group_count >= group_capacity) break;
            index = group_count;
            out_groups[index].face_group_index = face_group;
            out_groups[index].representative = addresses[i];
            group_count += 1;
        }
        out_groups[index].triangle_count += 1;
        if (material_editor_find_selected_triangle(&addresses[i]) >= 0) {
            out_groups[index].selected_count += 1;
        }
    }
    return group_count;
}

bool material_editor_add_triangle_selection(
    const SceneEditorMaterialPreviewTriangleAddress* address) {
    if (!address) return false;
    if (material_editor_find_selected_triangle(address) >= 0) return true;
    if (s_material_editor_selected_triangle_count >= MATERIAL_EDITOR_MAX_SELECTED_TRIANGLES) {
        return false;
    }
    s_material_editor_selected_triangles[s_material_editor_selected_triangle_count] = *address;
    s_material_editor_selected_triangle_count += 1;
    return true;
}

bool material_editor_remove_face_group_selection(int scene_object_index, int face_group_index) {
    bool removed = false;
    if (face_group_index < 0) return false;
    for (int i = 0; i < s_material_editor_selected_triangle_count;) {
        if (s_material_editor_selected_triangles[i].sceneObjectIndex == scene_object_index &&
            s_material_editor_selected_triangles[i].faceGroupIndex == face_group_index) {
            for (int j = i; j + 1 < s_material_editor_selected_triangle_count; ++j) {
                s_material_editor_selected_triangles[j] = s_material_editor_selected_triangles[j + 1];
            }
            s_material_editor_selected_triangle_count -= 1;
            removed = true;
            continue;
        }
        i += 1;
    }
    return removed;
}

int material_editor_collect_face_group_triangles(
    const SceneEditorMaterialPreviewTriangleAddress* address,
    SceneEditorMaterialPreviewTriangleAddress* out_addresses,
    int address_capacity) {
    SceneEditorMaterialPreviewTriangleAddress all_addresses[SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES];
    SceneEditorMaterialPreviewStats stats = {0};
    int count = 0;
    if (!address || !out_addresses || address_capacity <= 0) return 0;
    memset(all_addresses, 0, sizeof(all_addresses));
    if (!SceneEditorMaterialPreviewResolveFocusedTriangles(address->sceneObjectIndex,
                                                          NULL,
                                                          all_addresses,
                                                          SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES,
                                                          &stats)) {
        return 0;
    }
    for (int i = 0; i < stats.triangleCount && count < address_capacity; ++i) {
        if (all_addresses[i].sceneObjectIndex == address->sceneObjectIndex &&
            all_addresses[i].faceGroupIndex == address->faceGroupIndex) {
            out_addresses[count] = all_addresses[i];
            count += 1;
        }
    }
    return count;
}

bool material_editor_face_group_fully_selected(
    const SceneEditorMaterialPreviewTriangleAddress* address) {
    SceneEditorMaterialPreviewTriangleAddress group_addresses[SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES];
    int group_count = material_editor_collect_face_group_triangles(address,
                                                                   group_addresses,
                                                                   SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES);
    if (!address || group_count <= 0) return false;
    for (int i = 0; i < group_count; ++i) {
        if (material_editor_find_selected_triangle(&group_addresses[i]) < 0) {
            return false;
        }
    }
    return true;
}

SceneObject* material_editor_focused_object(void) {
    int object_count = sceneSettings.objectCount;
    int selected = ObjectEditorSelectionTrackerCurrent(object_count);
    if (selected >= 0 && selected < object_count) {
        s_material_editor_focused_object_index = selected;
        return &sceneSettings.sceneObjects[selected];
    }
    if (s_material_editor_focused_object_index < 0 ||
        s_material_editor_focused_object_index >= object_count) {
        s_material_editor_focused_object_index = ObjectEditorSelectionTrackerLast(object_count);
    }
    if (s_material_editor_focused_object_index >= 0 &&
        s_material_editor_focused_object_index < object_count) {
        return &sceneSettings.sceneObjects[s_material_editor_focused_object_index];
    }
    return NULL;
}

bool material_editor_use_object_layer_controls(const SceneObject* obj) {
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    return obj &&
           s_material_editor_active_face_group_index < 0 &&
           MaterialEditorLayerModelGetEffectiveStack(obj, focused_object_index, &stack);
}

bool material_editor_get_active_layer(const SceneObject* obj,
                                      RuntimeMaterialTextureStack* out_stack,
                                      RuntimeMaterialTextureLayer* out_layer,
                                      int* out_index) {
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    int index = 0;
    if (!obj || !MaterialEditorLayerModelGetEffectiveStack(obj, focused_object_index, &stack)) {
        return false;
    }
    index = MaterialEditorLayerModelGetActiveIndex(obj, focused_object_index);
    if (index < 0 || index >= stack.layerCount) return false;
    if (out_stack) *out_stack = stack;
    if (out_layer) *out_layer = stack.layers[index];
    if (out_index) *out_index = index;
    return true;
}

int MaterialEditorResolveFocusedObjectIndex(void) {
    SceneObject* obj = material_editor_focused_object();
    if (!obj) return -1;
    return s_material_editor_focused_object_index;
}

void MaterialEditorSetFocusedObjectIndex(int index) {
    if (index >= 0 && index < sceneSettings.objectCount) {
        if (s_material_editor_focused_object_index != index) {
            MaterialEditorFacePreviewInvalidate();
            MaterialEditorClearTriangleSelection();
            s_group_scroll_offset = 0;
            s_layer_scroll_offset = 0;
            MaterialEditorLayerModelReset();
            MaterialEditorAuthoredTextureBindingReset();
        }
        s_material_editor_focused_object_index = index;
        ObjectEditorSelectionTrackerSetCurrent(index, sceneSettings.objectCount);
    }
}

void MaterialEditorClearTriangleSelection(void) {
    memset(s_material_editor_selected_triangles, 0, sizeof(s_material_editor_selected_triangles));
    s_material_editor_selected_triangle_count = 0;
    s_material_editor_active_face_group_index = -1;
    MaterialEditorResetGroupListLayout();
    MaterialEditorFacePreviewInvalidate();
}

int MaterialEditorSelectedTriangleCount(void) {
    return s_material_editor_selected_triangle_count;
}

int MaterialEditorSelectedFaceGroupCount(void) {
    int count = 0;
    for (int i = 0; i < s_material_editor_selected_triangle_count; ++i) {
        int face_group = s_material_editor_selected_triangles[i].faceGroupIndex;
        if (face_group >= 0 && !material_editor_selected_face_group_seen(face_group, i)) {
            count += 1;
        }
    }
    return count;
}

int MaterialEditorFocusedFaceGroupCount(void) {
    MaterialEditorFaceGroupInfo groups[MATERIAL_EDITOR_MAX_FACE_GROUPS];
    return material_editor_collect_focused_face_groups(groups, MATERIAL_EDITOR_MAX_FACE_GROUPS);
}

int MaterialEditorGetActiveFaceGroupIndex(void) {
    return s_material_editor_active_face_group_index;
}

bool MaterialEditorSetActiveFaceGroupIndex(int face_group_index) {
    if (!material_editor_selected_face_group_exists(face_group_index)) return false;
    s_material_editor_active_face_group_index = face_group_index;
    MaterialEditorFacePreviewInvalidate();
    return true;
}

bool MaterialEditorSetFaceGroupSelectionByIndex(int face_group_index) {
    MaterialEditorFaceGroupInfo groups[MATERIAL_EDITOR_MAX_FACE_GROUPS];
    int group_count = material_editor_collect_focused_face_groups(groups, MATERIAL_EDITOR_MAX_FACE_GROUPS);
    int index = material_editor_find_face_group_info(groups, group_count, face_group_index);
    if (index < 0) return false;
    return MaterialEditorSetFaceGroupSelection(&groups[index].representative);
}

int MaterialEditorGetActiveLayerIndex(void) {
    SceneObject* obj = material_editor_focused_object();
    return MaterialEditorLayerModelGetActiveIndex(obj, MaterialEditorResolveFocusedObjectIndex());
}

bool MaterialEditorSetActiveLayerIndex(int layer_index) {
    SceneObject* obj = material_editor_focused_object();
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    if (!obj || focused_object_index < 0) return false;
    if (!MaterialEditorLayerModelEnsureEditableStack(obj, focused_object_index, NULL)) {
        return false;
    }
    if (s_material_editor_active_face_group_index >= 0) {
        MaterialEditorClearTriangleSelection();
    }
    return MaterialEditorLayerModelSetActiveIndex(obj, focused_object_index, layer_index);
}

int MaterialEditorFocusedLayerCount(void) {
    SceneObject* obj = material_editor_focused_object();
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    if (!MaterialEditorLayerModelGetEffectiveStack(obj,
                                                   MaterialEditorResolveFocusedObjectIndex(),
                                                   &stack)) {
        return 0;
    }
    return stack.layerCount;
}

bool MaterialEditorGetSelectedTriangle(int index, SceneEditorMaterialPreviewTriangleAddress* out_address) {
    if (!out_address) return false;
    if (index < 0 || index >= s_material_editor_selected_triangle_count) return false;
    *out_address = s_material_editor_selected_triangles[index];
    return true;
}

MaterialEditorViewMode MaterialEditorGetViewMode(void) {
    return s_material_editor_view_mode;
}

void MaterialEditorSetViewMode(MaterialEditorViewMode mode) {
    if (mode == MATERIAL_EDITOR_VIEW_FOCUSED_ORIGIN ||
        mode == MATERIAL_EDITOR_VIEW_SCENE_PLACEMENT) {
        s_material_editor_view_mode = mode;
    }
}

bool MaterialEditorGetSolidFacesEnabled(void) {
    return s_material_editor_solid_faces_enabled;
}

void MaterialEditorSetSolidFacesEnabled(bool enabled) {
    s_material_editor_solid_faces_enabled = enabled;
}

bool MaterialEditorToggleSolidFaces(void) {
    s_material_editor_solid_faces_enabled = !s_material_editor_solid_faces_enabled;
    MaterialEditorFacePreviewInvalidate();
    return s_material_editor_solid_faces_enabled;
}
