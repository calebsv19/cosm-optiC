#include "ui/menu_pane_host.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

enum {
    MENU_PANE_MIN_SCENE_WIDTH = 350,
    MENU_PANE_MAX_SCENE_WIDTH = 470,
    MENU_PANE_MIN_WORKSPACE_WIDTH = 300,
    MENU_PANE_MIN_HEALTH_WIDTH = 250,
    MENU_PANE_MAX_HEALTH_WIDTH = 480,
    MENU_PANE_MIN_HEIGHT = 360,
    MENU_PANE_DEFAULT_SCENE_WIDTH = 390,
    MENU_PANE_DEFAULT_HEALTH_WIDTH = 340,
    MENU_PANE_GAP = 18,
    MENU_PANE_SPLITTER_THICKNESS = 10
};

static void menu_pane_host_set_error(RayTracingMenuPaneHost* host,
                                     const char* fmt,
                                     ...) {
    va_list args;
    if (!host || !fmt) return;
    host->last_error[0] = '\0';
    va_start(args, fmt);
    (void)vsnprintf(host->last_error, sizeof(host->last_error), fmt, args);
    va_end(args);
}

static int menu_pane_host_clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static float menu_pane_host_clamp_float(float value,
                                        float min_value,
                                        float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static SDL_Rect menu_pane_host_to_sdl(CorePaneRect rect) {
    SDL_Rect out;
    out.x = (int)lroundf(rect.x);
    out.y = (int)lroundf(rect.y);
    out.w = (int)lroundf(rect.width);
    out.h = (int)lroundf(rect.height);
    if (out.w < 0) out.w = 0;
    if (out.h < 0) out.h = 0;
    return out;
}

static bool menu_pane_host_find_leaf(const RayTracingMenuPaneHost* host,
                                     uint32_t pane_id,
                                     CorePaneRect* out_rect) {
    uint32_t i;
    if (!host || !out_rect) return false;
    for (i = 0u; i < host->leaf_count; ++i) {
        if (host->leaves[i].id == pane_id) {
            *out_rect = host->leaves[i].rect;
            return true;
        }
    }
    return false;
}

static SDL_Rect menu_pane_host_trim_left(SDL_Rect rect, int amount) {
    if (amount < 0) amount = 0;
    if (amount > rect.w) amount = rect.w;
    rect.x += amount;
    rect.w -= amount;
    return rect;
}

static SDL_Rect menu_pane_host_trim_right(SDL_Rect rect, int amount) {
    if (amount < 0) amount = 0;
    if (amount > rect.w) amount = rect.w;
    rect.w -= amount;
    return rect;
}

static bool menu_pane_host_assign_layout(RayTracingMenuPaneHost* host) {
    CorePaneRect scene = {0};
    CorePaneRect workspace = {0};
    CorePaneRect health = {0};
    const int gap_left = MENU_PANE_GAP / 2;
    const int gap_right = MENU_PANE_GAP - gap_left;

    if (!host) return false;
    if (!menu_pane_host_find_leaf(host, RAY_TRACING_MENU_PANE_ID_SCENE, &scene) ||
        !menu_pane_host_find_leaf(host, RAY_TRACING_MENU_PANE_ID_WORKSPACE, &workspace) ||
        !menu_pane_host_find_leaf(host, RAY_TRACING_MENU_PANE_ID_HEALTH, &health)) {
        menu_pane_host_set_error(host, "pane solve missing expected main-menu leaf");
        return false;
    }

    host->layout.scene_rect =
        menu_pane_host_trim_right(menu_pane_host_to_sdl(scene), gap_left);
    host->layout.workspace_rect =
        menu_pane_host_trim_right(
            menu_pane_host_trim_left(menu_pane_host_to_sdl(workspace), gap_right),
            gap_left);
    host->layout.health_rect =
        menu_pane_host_trim_left(menu_pane_host_to_sdl(health), gap_right);
    return true;
}

static void menu_pane_host_sync_targets(RayTracingMenuPaneHost* host) {
    CorePaneRect rect = {0};
    if (!host) return;
    if (menu_pane_host_find_leaf(host, RAY_TRACING_MENU_PANE_ID_SCENE, &rect)) {
        host->target_scene_width = (int)lroundf(rect.width);
    }
    if (menu_pane_host_find_leaf(host, RAY_TRACING_MENU_PANE_ID_HEALTH, &rect)) {
        host->target_health_width = (int)lroundf(rect.width);
    }
}

static void menu_pane_host_seed_graph(RayTracingMenuPaneHost* host) {
    if (!host) return;
    host->node_count = 5u;
    host->root_index = 0u;
    host->nodes[0] = (CorePaneNode){
        .type = CORE_PANE_NODE_SPLIT,
        .id = 1u,
        .axis = CORE_PANE_AXIS_HORIZONTAL,
        .ratio_01 = 0.34f,
        .child_a = 1u,
        .child_b = 2u,
        .constraints = {
            (float)MENU_PANE_MIN_SCENE_WIDTH,
            (float)(MENU_PANE_MIN_WORKSPACE_WIDTH + MENU_PANE_MIN_HEALTH_WIDTH)
        }
    };
    host->nodes[1] = (CorePaneNode){
        .type = CORE_PANE_NODE_LEAF,
        .id = RAY_TRACING_MENU_PANE_ID_SCENE
    };
    host->nodes[2] = (CorePaneNode){
        .type = CORE_PANE_NODE_SPLIT,
        .id = 2u,
        .axis = CORE_PANE_AXIS_HORIZONTAL,
        .ratio_01 = 0.55f,
        .child_a = 3u,
        .child_b = 4u,
        .constraints = {
            (float)MENU_PANE_MIN_WORKSPACE_WIDTH,
            (float)MENU_PANE_MIN_HEALTH_WIDTH
        }
    };
    host->nodes[3] = (CorePaneNode){
        .type = CORE_PANE_NODE_LEAF,
        .id = RAY_TRACING_MENU_PANE_ID_WORKSPACE
    };
    host->nodes[4] = (CorePaneNode){
        .type = CORE_PANE_NODE_LEAF,
        .id = RAY_TRACING_MENU_PANE_ID_HEALTH
    };
}

static bool menu_pane_host_solve(RayTracingMenuPaneHost* host) {
    CorePaneValidationReport report;
    if (!host) return false;
    memset(&report, 0, sizeof(report));
    if (!core_pane_validate_graph(host->nodes,
                                  host->node_count,
                                  host->root_index,
                                  host->bounds,
                                  &report)) {
        menu_pane_host_set_error(host,
                                 "pane graph invalid code=%s node=%u rel=%u",
                                 core_pane_validation_code_string(report.code),
                                 report.node_index,
                                 report.related_index);
        return false;
    }
    if (!core_pane_solve(host->nodes,
                         host->node_count,
                         host->root_index,
                         host->bounds,
                         host->leaves,
                         (uint32_t)(sizeof(host->leaves) / sizeof(host->leaves[0])),
                         &host->leaf_count)) {
        menu_pane_host_set_error(host, "main-menu pane solve failed");
        return false;
    }
    if (!menu_pane_host_assign_layout(host)) return false;
    menu_pane_host_sync_targets(host);
    host->initialized = true;
    host->last_error[0] = '\0';
    return true;
}

bool ray_tracing_menu_pane_host_rebuild(RayTracingMenuPaneHost* host,
                                        SDL_Rect bounds) {
    int scene_width;
    int health_width;
    int workspace_width;
    int remaining_width;
    int deficit;
    float root_min;
    float root_max;
    float nested_min;
    float nested_max;

    if (!host || bounds.w <= 0 || bounds.h < MENU_PANE_MIN_HEIGHT) {
        if (host) menu_pane_host_set_error(host, "invalid menu pane bounds %dx%d", bounds.w, bounds.h);
        return false;
    }
    if (bounds.w < MENU_PANE_MIN_SCENE_WIDTH + MENU_PANE_MIN_WORKSPACE_WIDTH +
                       MENU_PANE_MIN_HEALTH_WIDTH) {
        menu_pane_host_set_error(host, "insufficient menu pane width %d", bounds.w);
        return false;
    }
    if (!host->initialized) {
        menu_pane_host_seed_graph(host);
    }

    scene_width = host->target_scene_width > 0
                      ? host->target_scene_width
                      : MENU_PANE_DEFAULT_SCENE_WIDTH;
    health_width = host->target_health_width > 0
                       ? host->target_health_width
                       : MENU_PANE_DEFAULT_HEALTH_WIDTH;
    scene_width = menu_pane_host_clamp_int(scene_width,
                                          MENU_PANE_MIN_SCENE_WIDTH,
                                          MENU_PANE_MAX_SCENE_WIDTH);
    health_width = menu_pane_host_clamp_int(health_width,
                                           MENU_PANE_MIN_HEALTH_WIDTH,
                                           MENU_PANE_MAX_HEALTH_WIDTH);
    workspace_width = bounds.w - scene_width - health_width;
    if (workspace_width < MENU_PANE_MIN_WORKSPACE_WIDTH) {
        deficit = MENU_PANE_MIN_WORKSPACE_WIDTH - workspace_width;
        health_width = menu_pane_host_clamp_int(health_width - deficit,
                                               MENU_PANE_MIN_HEALTH_WIDTH,
                                               MENU_PANE_MAX_HEALTH_WIDTH);
        workspace_width = bounds.w - scene_width - health_width;
    }
    if (workspace_width < MENU_PANE_MIN_WORKSPACE_WIDTH) {
        deficit = MENU_PANE_MIN_WORKSPACE_WIDTH - workspace_width;
        scene_width = menu_pane_host_clamp_int(scene_width - deficit,
                                              MENU_PANE_MIN_SCENE_WIDTH,
                                              MENU_PANE_MAX_SCENE_WIDTH);
        workspace_width = bounds.w - scene_width - health_width;
    }
    if (workspace_width < MENU_PANE_MIN_WORKSPACE_WIDTH) {
        menu_pane_host_set_error(host, "cannot satisfy menu pane minima in width %d", bounds.w);
        return false;
    }

    host->bounds = (CorePaneRect){
        (float)bounds.x,
        (float)bounds.y,
        (float)bounds.w,
        (float)bounds.h
    };
    host->nodes[0].constraints = (CorePaneConstraints){
        (float)MENU_PANE_MIN_SCENE_WIDTH,
        (float)(MENU_PANE_MIN_WORKSPACE_WIDTH + MENU_PANE_MIN_HEALTH_WIDTH)
    };
    root_min = (float)MENU_PANE_MIN_SCENE_WIDTH / (float)bounds.w;
    root_max = (float)(bounds.w - MENU_PANE_MIN_WORKSPACE_WIDTH -
                       MENU_PANE_MIN_HEALTH_WIDTH) / (float)bounds.w;
    host->nodes[0].ratio_01 = menu_pane_host_clamp_float(
        (float)scene_width / (float)bounds.w,
        root_min,
        root_max);

    remaining_width = bounds.w - scene_width;
    host->nodes[2].constraints = (CorePaneConstraints){
        (float)MENU_PANE_MIN_WORKSPACE_WIDTH,
        (float)MENU_PANE_MIN_HEALTH_WIDTH
    };
    nested_min = (float)MENU_PANE_MIN_WORKSPACE_WIDTH / (float)remaining_width;
    nested_max = (float)(remaining_width - MENU_PANE_MIN_HEALTH_WIDTH) /
                 (float)remaining_width;
    host->nodes[2].ratio_01 = menu_pane_host_clamp_float(
        (float)workspace_width / (float)remaining_width,
        nested_min,
        nested_max);
    return menu_pane_host_solve(host);
}

bool ray_tracing_menu_pane_host_init(RayTracingMenuPaneHost* host,
                                     SDL_Rect bounds) {
    if (!host) return false;
    memset(host, 0, sizeof(*host));
    host->target_scene_width = MENU_PANE_DEFAULT_SCENE_WIDTH;
    host->target_health_width = MENU_PANE_DEFAULT_HEALTH_WIDTH;
    kit_pane_splitter_interaction_init(&host->splitter_interaction,
                                       (float)MENU_PANE_SPLITTER_THICKNESS);
    menu_pane_host_seed_graph(host);
    return ray_tracing_menu_pane_host_rebuild(host, bounds);
}

void ray_tracing_menu_pane_host_set_targets(RayTracingMenuPaneHost* host,
                                            int scene_width,
                                            int health_width) {
    if (!host) return;
    if (scene_width > 0) host->target_scene_width = scene_width;
    if (health_width > 0) host->target_health_width = health_width;
}

void ray_tracing_menu_pane_host_update_pointer(RayTracingMenuPaneHost* host,
                                               float pointer_x,
                                               float pointer_y) {
    CoreResult result;
    if (!host || !host->initialized) return;
    result = kit_pane_splitter_interaction_set_hover(&host->splitter_interaction,
                                                     host->nodes,
                                                     host->node_count,
                                                     host->root_index,
                                                     host->bounds,
                                                     pointer_x,
                                                     pointer_y);
    if (result.code != CORE_OK && result.code != CORE_ERR_NOT_FOUND) {
        menu_pane_host_set_error(host, "menu splitter hover failed (%d)", (int)result.code);
    }
}

bool ray_tracing_menu_pane_host_begin_splitter_drag(RayTracingMenuPaneHost* host,
                                                    float pointer_x,
                                                    float pointer_y) {
    CoreResult result;
    if (!host || !host->initialized) return false;
    result = kit_pane_splitter_interaction_begin_drag(&host->splitter_interaction,
                                                      host->nodes,
                                                      host->node_count,
                                                      host->root_index,
                                                      host->bounds,
                                                      pointer_x,
                                                      pointer_y);
    return result.code == CORE_OK;
}

bool ray_tracing_menu_pane_host_update_splitter_drag(RayTracingMenuPaneHost* host,
                                                     float pointer_x,
                                                     float pointer_y) {
    CoreResult result;
    int changed = 0;
    if (!host || !host->initialized || !host->splitter_interaction.drag_active) {
        return false;
    }
    result = kit_pane_splitter_interaction_update_drag(&host->splitter_interaction,
                                                       host->nodes,
                                                       host->node_count,
                                                       pointer_x,
                                                       pointer_y,
                                                       &changed);
    if (result.code != CORE_OK) {
        menu_pane_host_set_error(host, "menu splitter drag failed (%d)", (int)result.code);
        return false;
    }
    if (changed && !menu_pane_host_solve(host)) return false;
    return changed != 0;
}

void ray_tracing_menu_pane_host_end_splitter_drag(RayTracingMenuPaneHost* host) {
    if (!host) return;
    kit_pane_splitter_interaction_end_drag(&host->splitter_interaction);
}

bool ray_tracing_menu_pane_host_splitter_drag_active(
    const RayTracingMenuPaneHost* host) {
    return host && host->splitter_interaction.drag_active;
}

uint32_t ray_tracing_menu_pane_host_splitter_visuals(
    const RayTracingMenuPaneHost* host,
    RayTracingMenuPaneSplitterVisual* out_visuals,
    uint32_t cap) {
    CorePaneSplitterHit hits[RAY_TRACING_MENU_PANE_SPLITTER_CAP];
    uint32_t hit_count = 0u;
    uint32_t i;
    if (!host || !host->initialized || !out_visuals || cap == 0u) return 0u;
    if (!core_pane_collect_splitter_hits(host->nodes,
                                        host->node_count,
                                        host->root_index,
                                        host->bounds,
                                        (float)MENU_PANE_SPLITTER_THICKNESS,
                                        hits,
                                        RAY_TRACING_MENU_PANE_SPLITTER_CAP,
                                        &hit_count)) {
        return 0u;
    }
    if (hit_count > cap) hit_count = cap;
    for (i = 0u; i < hit_count; ++i) {
        out_visuals[i].rect = menu_pane_host_to_sdl(hits[i].splitter_bounds);
        out_visuals[i].hovered =
            host->splitter_interaction.hover_active &&
            host->splitter_interaction.hover_hit.node_index == hits[i].node_index;
        out_visuals[i].active =
            host->splitter_interaction.drag_active &&
            host->splitter_interaction.drag_hit.node_index == hits[i].node_index;
    }
    return hit_count;
}

const RayTracingMenuPaneLayout* ray_tracing_menu_pane_host_layout(
    const RayTracingMenuPaneHost* host) {
    if (!host || !host->initialized) return NULL;
    return &host->layout;
}

const char* ray_tracing_menu_pane_host_last_error(
    const RayTracingMenuPaneHost* host) {
    if (!host) return "menu pane host unavailable";
    return host->last_error;
}
