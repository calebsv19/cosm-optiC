#ifndef RAY_TRACING_SCENE_EDITOR_PANE_HOST_H
#define RAY_TRACING_SCENE_EDITOR_PANE_HOST_H

#include <SDL2/SDL.h>
#include <stdbool.h>

typedef struct SceneEditorPaneLayout {
    SDL_Rect left_pane_rect;
    SDL_Rect center_pane_rect;
    SDL_Rect right_pane_rect;
    SDL_Rect left_content_rect;
    SDL_Rect center_content_rect;
    SDL_Rect right_content_rect;
    SDL_Rect mode_router_rect;
    SDL_Rect viewport_rect;
} SceneEditorPaneLayout;

typedef struct SceneEditorPaneHost {
    int target_left_width;
    int target_right_width;
    bool initialized;
    SceneEditorPaneLayout layout;
    char last_error[160];
} SceneEditorPaneHost;

bool scene_editor_pane_host_init(SceneEditorPaneHost* host, int width, int height);
bool scene_editor_pane_host_rebuild(SceneEditorPaneHost* host, int width, int height);
void scene_editor_pane_host_set_targets(SceneEditorPaneHost* host,
                                        int left_width,
                                        int right_width);
const SceneEditorPaneLayout* scene_editor_pane_host_layout(const SceneEditorPaneHost* host);
const char* scene_editor_pane_host_last_error(const SceneEditorPaneHost* host);

#endif
