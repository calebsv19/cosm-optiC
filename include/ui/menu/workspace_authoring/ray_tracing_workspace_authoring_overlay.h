#ifndef RAY_TRACING_WORKSPACE_AUTHORING_OVERLAY_H
#define RAY_TRACING_WORKSPACE_AUTHORING_OVERLAY_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdint.h>

#include "editor/scene_editor_pane_host.h"
#include "ui/menu/workspace_authoring/ray_tracing_workspace_authoring_host.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RayTracingWorkspaceAuthoringPaneRow {
    SDL_Rect pane_rect;
    const char* pane_label;
    const char* module_key;
    const char* module_label;
} RayTracingWorkspaceAuthoringPaneRow;

uint32_t ray_tracing_workspace_authoring_overlay_build_pane_rows(
    int width,
    int height,
    const SceneEditorPaneLayout* scene_layout,
    RayTracingWorkspaceAuthoringPaneRow* out_rows,
    uint32_t cap);

void ray_tracing_workspace_authoring_overlay_draw(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const RayTracingWorkspaceAuthoringHostState* host,
    int width,
    int height,
    const SceneEditorPaneLayout* scene_layout);

#ifdef __cplusplus
}
#endif

#endif
