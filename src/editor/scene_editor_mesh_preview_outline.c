#include "editor/scene_editor_mesh_preview_outline.h"

#include <stdint.h>

#include "kit_viewport3d.h"

_Static_assert(sizeof(int) == sizeof(int32_t), "outline owner ids require 32-bit int");

SDL_Color SceneEditorMeshPreviewOutlineColor(int scene_object_index,
                                             int selected_object_index,
                                             int hover_object_index) {
    const KitViewport3dColor color = kit_viewport3d_outline_color(
        NULL,
        (int32_t)scene_object_index,
        (int32_t)selected_object_index,
        (int32_t)hover_object_index);
    return (SDL_Color){color.r, color.g, color.b, color.a};
}

size_t SceneEditorMeshPreviewApplyOutlines(uint8_t* rgba,
                                           const double* depth,
                                           const int* owner,
                                           int width,
                                           int height,
                                           int selected_object_index,
                                           int hover_object_index) {
    size_t count = 0u;
    const KitViewport3dOutlineParams params = {
        .rgba = rgba,
        .depth = depth,
        .owner = (const int32_t*)owner,
        .width = width,
        .height = height,
        .depth_format = KIT_VIEWPORT3D_DEPTH_F64,
        .relative_depth_threshold = 0.18,
        .selected_owner = (int32_t)selected_object_index,
        .hover_owner = (int32_t)hover_object_index,
        .outline_only = false,
        .palette = NULL
    };
    if (!kit_viewport3d_apply_outline(&params, &count)) return 0u;
    return count;
}
