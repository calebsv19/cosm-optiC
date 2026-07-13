#include "ui/menu_workspace.h"

#include <stdio.h>
#include <string.h>

#include "ui/menu_pane_host.h"

enum {
    MENU_WORKSPACE_MODULE_TYPE_BASE = 4200u,
    MENU_WORKSPACE_INSTANCE_BASE = 5200u,
    MENU_WORKSPACE_HEADER_HEIGHT = 48,
    MENU_WORKSPACE_TAB_GAP = 6,
    MENU_WORKSPACE_TAB_INSET = 10,
    MENU_WORKSPACE_CONTENT_GAP = 8
};

static const char* const kMenuWorkspaceKeys[MENU_WORKSPACE_MODULE_COUNT] = {
    "ray_tracing_render",
    "ray_tracing_output",
    "ray_tracing_run"
};

static const char* const kMenuWorkspaceLabels[MENU_WORKSPACE_MODULE_COUNT] = {
    "Render",
    "Output + Batch",
    "Run + Resume"
};

static bool menu_workspace_module_valid(MenuWorkspaceModule module) {
    return module >= MENU_WORKSPACE_RENDER &&
           module < MENU_WORKSPACE_MODULE_COUNT;
}

static bool menu_workspace_validate_active(MenuWorkspaceHost* host) {
    const uint32_t leaf_id = RAY_TRACING_MENU_PANE_ID_WORKSPACE;
    CorePaneModuleResult result;
    if (!host) return false;
    result = core_pane_module_validate_bindings(&host->registry,
                                                &host->active_binding,
                                                1u,
                                                &leaf_id,
                                                1u);
    if (result != CORE_PANE_MODULE_OK) {
        (void)snprintf(host->last_error,
                       sizeof(host->last_error),
                       "workspace module binding invalid: %d",
                       (int)result);
        return false;
    }
    host->last_error[0] = '\0';
    return true;
}

bool menu_workspace_host_init(MenuWorkspaceHost* host) {
    int i;
    if (!host) return false;
    memset(host, 0, sizeof(*host));
    if (core_pane_module_registry_init(&host->registry,
                                       host->descriptors,
                                       MENU_WORKSPACE_MODULE_COUNT) != CORE_PANE_MODULE_OK) {
        return false;
    }
    for (i = 0; i < MENU_WORKSPACE_MODULE_COUNT; ++i) {
        CorePaneModuleDescriptor descriptor = {
            .module_type_id = MENU_WORKSPACE_MODULE_TYPE_BASE + (uint32_t)i,
            .module_key = kMenuWorkspaceKeys[i],
            .display_name = kMenuWorkspaceLabels[i],
            .version_major = 1u,
            .version_minor = 0u,
            .capabilities = 0u,
            .default_config_variant = 0u,
            .provider_kind = CORE_PANE_MODULE_PROVIDER_INTERNAL,
            .render = NULL,
            .handle_keyboard = NULL,
            .handle_pointer = NULL
        };
        if (core_pane_module_register(&host->registry,
                                      &descriptor) != CORE_PANE_MODULE_OK) {
            (void)snprintf(host->last_error,
                           sizeof(host->last_error),
                           "workspace module registration failed: %d",
                           i);
            return false;
        }
    }
    host->initialized = true;
    return menu_workspace_host_select(host, MENU_WORKSPACE_RENDER);
}

bool menu_workspace_host_select(MenuWorkspaceHost* host,
                                MenuWorkspaceModule module) {
    if (!host || !host->initialized || !menu_workspace_module_valid(module)) {
        return false;
    }
    host->active_binding = (CorePaneModuleBinding){
        .instance_id = MENU_WORKSPACE_INSTANCE_BASE + (uint32_t)module,
        .pane_node_id = RAY_TRACING_MENU_PANE_ID_WORKSPACE,
        .module_type_id = MENU_WORKSPACE_MODULE_TYPE_BASE + (uint32_t)module,
        .config_variant = 0u,
        .runtime_flags = 0u
    };
    if (!menu_workspace_validate_active(host)) return false;
    host->active_module = module;
    return true;
}

const char* menu_workspace_module_label(MenuWorkspaceModule module) {
    if (!menu_workspace_module_valid(module)) return "Workspace";
    return kMenuWorkspaceLabels[module];
}

void menu_workspace_build_layout(SDL_Rect frame_rect,
                                 MenuWorkspaceLayout* out_layout) {
    MenuWorkspaceLayout layout;
    int tabs_width;
    int tab_width;
    int i;
    if (!out_layout) return;
    memset(&layout, 0, sizeof(layout));
    layout.frame_rect = frame_rect;
    tabs_width = frame_rect.w - MENU_WORKSPACE_TAB_INSET * 2;
    tab_width = (tabs_width - MENU_WORKSPACE_TAB_GAP *
                 (MENU_WORKSPACE_MODULE_COUNT - 1)) /
                MENU_WORKSPACE_MODULE_COUNT;
    if (tab_width < 1) tab_width = 1;
    for (i = 0; i < MENU_WORKSPACE_MODULE_COUNT; ++i) {
        layout.tab_rects[i] = (SDL_Rect){
            frame_rect.x + MENU_WORKSPACE_TAB_INSET +
                i * (tab_width + MENU_WORKSPACE_TAB_GAP),
            frame_rect.y + 7,
            tab_width,
            34
        };
    }
    layout.content_rect = (SDL_Rect){
        frame_rect.x,
        frame_rect.y + MENU_WORKSPACE_HEADER_HEIGHT + MENU_WORKSPACE_CONTENT_GAP,
        frame_rect.w,
        frame_rect.h - MENU_WORKSPACE_HEADER_HEIGHT - MENU_WORKSPACE_CONTENT_GAP
    };
    if (layout.content_rect.h < 0) layout.content_rect.h = 0;
    *out_layout = layout;
}

int menu_workspace_tab_at_point(const MenuWorkspaceLayout* layout,
                                int x,
                                int y) {
    int i;
    if (!layout) return -1;
    for (i = 0; i < MENU_WORKSPACE_MODULE_COUNT; ++i) {
        const SDL_Rect* rect = &layout->tab_rects[i];
        if (x >= rect->x && x <= rect->x + rect->w &&
            y >= rect->y && y <= rect->y + rect->h) {
            return i;
        }
    }
    return -1;
}
