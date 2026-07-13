#ifndef RAY_TRACING_MENU_WORKSPACE_H
#define RAY_TRACING_MENU_WORKSPACE_H

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "core_pane_module.h"

typedef enum MenuWorkspaceModule {
    MENU_WORKSPACE_RENDER = 0,
    MENU_WORKSPACE_OUTPUT = 1,
    MENU_WORKSPACE_RUN = 2,
    MENU_WORKSPACE_MODULE_COUNT = 3
} MenuWorkspaceModule;

typedef struct MenuWorkspaceLayout {
    SDL_Rect frame_rect;
    SDL_Rect tab_rects[MENU_WORKSPACE_MODULE_COUNT];
    SDL_Rect content_rect;
} MenuWorkspaceLayout;

typedef struct MenuWorkspaceHost {
    CorePaneModuleRegistry registry;
    CorePaneModuleDescriptor descriptors[MENU_WORKSPACE_MODULE_COUNT];
    CorePaneModuleBinding active_binding;
    MenuWorkspaceModule active_module;
    bool initialized;
    char last_error[128];
} MenuWorkspaceHost;

bool menu_workspace_host_init(MenuWorkspaceHost* host);
bool menu_workspace_host_select(MenuWorkspaceHost* host,
                                MenuWorkspaceModule module);
const char* menu_workspace_module_label(MenuWorkspaceModule module);
void menu_workspace_build_layout(SDL_Rect frame_rect,
                                 MenuWorkspaceLayout* out_layout);
int menu_workspace_tab_at_point(const MenuWorkspaceLayout* layout,
                                int x,
                                int y);

#endif
