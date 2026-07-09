#ifndef SCENE_EDITOR_MATERIAL_PREVIEW_H
#define SCENE_EDITOR_MATERIAL_PREVIEW_H

#include <stdbool.h>

#include <SDL2/SDL.h>

#include "editor/scene_editor_digest_overlay.h"
#include "scene/object_manager.h"

#define SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES 256

typedef struct SceneEditorMaterialPreviewTriangleAddress {
    int sceneObjectIndex;
    int primitiveIndex;
    int triangleIndex;
    int localTriangleIndex;
    int faceGroupIndex;
} SceneEditorMaterialPreviewTriangleAddress;

typedef struct SceneEditorMaterialPreviewStats {
    int triangleCount;
    int faceGroupCount;
    bool projected;
} SceneEditorMaterialPreviewStats;

bool SceneEditorMaterialPreviewResolveFocusedTriangles(
    int focused_object_index,
    const SceneEditorDigestOverlayProjector* projector,
    SceneEditorMaterialPreviewTriangleAddress* out_addresses,
    int address_capacity,
    SceneEditorMaterialPreviewStats* out_stats);

bool SceneEditorMaterialPreviewPickTriangle(
    int focused_object_index,
    const SceneEditorDigestOverlayProjector* projector,
    int screen_x,
    int screen_y,
    SceneEditorMaterialPreviewTriangleAddress* out_address);

bool SceneEditorMaterialPreviewEvaluateTextureColor(const SceneObject* object,
                                                    int triangle_index,
                                                    double bary_v,
                                                    double bary_w,
                                                    SDL_Color base_color,
                                                    SDL_Color* out_color,
                                                    double* out_mask);
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
    double* out_mask);

bool SceneEditorMaterialPreviewRenderFocusedObject(
    SDL_Renderer* renderer,
    const SceneEditorDigestOverlayProjector* projector,
    int focused_object_index,
    bool solid_faces);

bool SceneEditorMaterialPreviewRenderFocusedObjectWithSelection(
    SDL_Renderer* renderer,
    const SceneEditorDigestOverlayProjector* projector,
    int focused_object_index,
    bool solid_faces,
    const SceneEditorMaterialPreviewTriangleAddress* selected_triangles,
    int selected_triangle_count);

#endif
