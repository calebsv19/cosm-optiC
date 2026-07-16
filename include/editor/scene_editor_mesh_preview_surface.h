#pragma once

#include <stdbool.h>

#include <SDL2/SDL.h>

#include "editor/scene_editor_digest_overlay.h"
#include "editor/scene_editor_mesh_preview_render.h"

bool SceneEditorMeshPreviewSurfaceRender(
    SDL_Renderer* renderer,
    const SceneEditorDigestOverlayProjector* projector,
    int active_editor_mode,
    int selected_object_index,
    int hover_object_index,
    SceneEditorMeshDisplayMode mode,
    SceneEditorMeshPreviewFrameStats* out_stats);
void SceneEditorMeshPreviewSurfaceReset(SDL_Renderer* renderer);
