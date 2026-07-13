#ifndef RAY_TRACING_MENU_PANE_HOST_H
#define RAY_TRACING_MENU_PANE_HOST_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdint.h>

#include "core_pane.h"
#include "kit_pane.h"

enum {
    RAY_TRACING_MENU_PANE_ID_SCENE = 3101u,
    RAY_TRACING_MENU_PANE_ID_WORKSPACE = 3102u,
    RAY_TRACING_MENU_PANE_ID_HEALTH = 3103u,
    RAY_TRACING_MENU_PANE_SPLITTER_CAP = 2
};

typedef struct RayTracingMenuPaneLayout {
    SDL_Rect scene_rect;
    SDL_Rect workspace_rect;
    SDL_Rect health_rect;
} RayTracingMenuPaneLayout;

typedef struct RayTracingMenuPaneSplitterVisual {
    SDL_Rect rect;
    bool hovered;
    bool active;
} RayTracingMenuPaneSplitterVisual;

typedef struct RayTracingMenuPaneHost {
    CorePaneNode nodes[5];
    uint32_t node_count;
    uint32_t root_index;
    CorePaneLeafRect leaves[3];
    uint32_t leaf_count;
    CorePaneRect bounds;
    int target_scene_width;
    int target_health_width;
    KitPaneSplitterInteraction splitter_interaction;
    bool initialized;
    RayTracingMenuPaneLayout layout;
    char last_error[160];
} RayTracingMenuPaneHost;

bool ray_tracing_menu_pane_host_init(RayTracingMenuPaneHost* host,
                                     SDL_Rect bounds);
bool ray_tracing_menu_pane_host_rebuild(RayTracingMenuPaneHost* host,
                                        SDL_Rect bounds);
void ray_tracing_menu_pane_host_set_targets(RayTracingMenuPaneHost* host,
                                            int scene_width,
                                            int health_width);
void ray_tracing_menu_pane_host_update_pointer(RayTracingMenuPaneHost* host,
                                               float pointer_x,
                                               float pointer_y);
bool ray_tracing_menu_pane_host_begin_splitter_drag(RayTracingMenuPaneHost* host,
                                                    float pointer_x,
                                                    float pointer_y);
bool ray_tracing_menu_pane_host_update_splitter_drag(RayTracingMenuPaneHost* host,
                                                     float pointer_x,
                                                     float pointer_y);
void ray_tracing_menu_pane_host_end_splitter_drag(RayTracingMenuPaneHost* host);
bool ray_tracing_menu_pane_host_splitter_drag_active(
    const RayTracingMenuPaneHost* host);
uint32_t ray_tracing_menu_pane_host_splitter_visuals(
    const RayTracingMenuPaneHost* host,
    RayTracingMenuPaneSplitterVisual* out_visuals,
    uint32_t cap);
const RayTracingMenuPaneLayout* ray_tracing_menu_pane_host_layout(
    const RayTracingMenuPaneHost* host);
const char* ray_tracing_menu_pane_host_last_error(
    const RayTracingMenuPaneHost* host);

#endif
