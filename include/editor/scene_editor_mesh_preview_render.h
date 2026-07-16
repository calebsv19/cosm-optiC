#pragma once

#include <stdbool.h>
#include <stddef.h>

#include <SDL2/SDL.h>

#include "editor/scene_editor_digest_overlay.h"
#include "editor/scene_editor_mesh_preview_contract.h"

typedef struct SceneEditorMeshPreviewFrameStats {
    SceneEditorMeshDisplayMode mode;
    int rendered_instances;
    size_t rendered_triangles;
    size_t rendered_wire_segments;
    size_t rendered_outline_pixels;
    int rendered_bounds;
} SceneEditorMeshPreviewFrameStats;

void SceneEditorMeshPreviewRenderReset(SDL_Renderer* renderer);

SceneEditorMeshDisplayMode SceneEditorMeshPreviewModeGet(void);
void SceneEditorMeshPreviewModeSet(SceneEditorMeshDisplayMode mode);

void SceneEditorMeshPreviewLayoutModeButtons(
    const SDL_Rect* viewport,
    SDL_Rect out_buttons[SCENE_EDITOR_MESH_DISPLAY_COUNT]);
int SceneEditorMeshPreviewModeButtonAtPoint(const SDL_Rect* viewport, int x, int y);
bool SceneEditorMeshPreviewHandleModeClick(const SDL_Rect* viewport, int x, int y);

bool SceneEditorMeshPreviewRenderGeometry(
    SDL_Renderer* renderer,
    const SceneEditorDigestOverlayProjector* projector,
    int active_editor_mode,
    int selected_object_index,
    int hover_object_index,
    SceneEditorMeshPreviewFrameStats* out_stats);
void SceneEditorMeshPreviewRenderToolbar(
    SDL_Renderer* renderer,
    const SDL_Rect* viewport);

int SceneEditorMeshPreviewPickObjectIndex(
    const SceneEditorDigestOverlayProjector* projector,
    int active_editor_mode,
    int selected_object_index,
    int screen_x,
    int screen_y);
