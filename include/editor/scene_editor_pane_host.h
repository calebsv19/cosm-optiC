#ifndef RAY_TRACING_SCENE_EDITOR_PANE_HOST_H
#define RAY_TRACING_SCENE_EDITOR_PANE_HOST_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdint.h>

#include "core_pane.h"
#include "kit_pane.h"

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
    CorePaneNode nodes[5];
    uint32_t node_count;
    uint32_t root_index;
    CorePaneLeafRect leaves[3];
    uint32_t leaf_count;
    float bounds_width;
    float bounds_height;
    int target_left_width;
    int target_right_width;
    KitPaneSplitterInteraction splitter_interaction;
    bool initialized;
    SceneEditorPaneLayout layout;
    char last_error[160];
} SceneEditorPaneHost;

bool scene_editor_pane_host_init(SceneEditorPaneHost* host, int width, int height);
bool scene_editor_pane_host_rebuild(SceneEditorPaneHost* host, int width, int height);
void scene_editor_pane_host_set_targets(SceneEditorPaneHost* host,
                                        int left_width,
                                        int right_width);
void scene_editor_pane_host_update_pointer(SceneEditorPaneHost* host,
                                           float pointer_x,
                                           float pointer_y);
bool scene_editor_pane_host_begin_splitter_drag(SceneEditorPaneHost* host,
                                                float pointer_x,
                                                float pointer_y);
bool scene_editor_pane_host_update_splitter_drag(SceneEditorPaneHost* host,
                                                 float pointer_x,
                                                 float pointer_y);
void scene_editor_pane_host_end_splitter_drag(SceneEditorPaneHost* host);
bool scene_editor_pane_host_splitter_drag_active(const SceneEditorPaneHost* host);
bool scene_editor_pane_host_visible_splitter(const SceneEditorPaneHost* host,
                                             CorePaneRect* out_rect,
                                             bool* out_hovered,
                                             bool* out_active);
const SceneEditorPaneLayout* scene_editor_pane_host_layout(const SceneEditorPaneHost* host);
const char* scene_editor_pane_host_last_error(const SceneEditorPaneHost* host);

#endif
