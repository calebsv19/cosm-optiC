#include "editor/scene_editor_pane_host.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

enum {
    SCENE_EDITOR_PANE_ID_LEFT = 2101u,
    SCENE_EDITOR_PANE_ID_CENTER = 2102u,
    SCENE_EDITOR_PANE_ID_RIGHT = 2103u
};

enum {
    SCENE_EDITOR_MIN_WIDTH = 780,
    SCENE_EDITOR_MIN_HEIGHT = 420,
    SCENE_EDITOR_MIN_LEFT_WIDTH = 220,
    SCENE_EDITOR_MAX_LEFT_WIDTH = 420,
    SCENE_EDITOR_MIN_RIGHT_WIDTH = 240,
    SCENE_EDITOR_MAX_RIGHT_WIDTH = 460,
    SCENE_EDITOR_MIN_CENTER_WIDTH = 360,
    SCENE_EDITOR_MIN_PANE_HEIGHT = 200,
    SCENE_EDITOR_CONTENT_PADDING = 10,
    SCENE_EDITOR_PANE_HEADER_HEIGHT = 28,
    SCENE_EDITOR_MODE_ROUTER_HEIGHT = 44,
    SCENE_EDITOR_VIEWPORT_TOP_GAP = 8,
    SCENE_EDITOR_SPLITTER_HANDLE_THICKNESS = 8
};

static void scene_editor_pane_host_set_error(SceneEditorPaneHost* host, const char* fmt, ...) {
    va_list args;
    if (!host || !fmt) return;
    host->last_error[0] = '\0';
    va_start(args, fmt);
    (void)vsnprintf(host->last_error, sizeof(host->last_error), fmt, args);
    va_end(args);
}

static int pane_host_clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static float pane_host_clamp_float(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static CorePaneRect scene_editor_pane_host_bounds_rect(float width, float height) {
    return (CorePaneRect){0.0f, 0.0f, width, height};
}

static SDL_Rect pane_host_inset_rect(SDL_Rect rect, int inset) {
    SDL_Rect out = rect;
    out.x += inset;
    out.y += inset;
    out.w -= inset * 2;
    out.h -= inset * 2;
    if (out.w < 0) out.w = 0;
    if (out.h < 0) out.h = 0;
    return out;
}

static SDL_Rect pane_host_reserve_top_space(SDL_Rect rect, int top) {
    SDL_Rect out = rect;
    if (top <= 0 || out.h <= 0) return out;
    if (top > out.h) top = out.h;
    out.y += top;
    out.h -= top;
    return out;
}

static SDL_Rect scene_editor_pane_rect_to_sdl(CorePaneRect rect) {
    SDL_Rect out = {0};
    out.x = (int)lroundf(rect.x);
    out.y = (int)lroundf(rect.y);
    out.w = (int)lroundf(rect.width);
    out.h = (int)lroundf(rect.height);
    if (out.w < 0) out.w = 0;
    if (out.h < 0) out.h = 0;
    return out;
}

static bool scene_editor_pane_host_find_rect_for_pane_id(const SceneEditorPaneHost* host,
                                                         uint32_t pane_id,
                                                         CorePaneRect* out_rect) {
    uint32_t i = 0u;

    if (!host || !out_rect || pane_id == 0u) return false;
    for (i = 0u; i < host->leaf_count; ++i) {
        if (host->leaves[i].id == pane_id) {
            *out_rect = host->leaves[i].rect;
            return true;
        }
    }
    return false;
}

static void scene_editor_pane_host_sync_targets_from_leaves(SceneEditorPaneHost* host) {
    CorePaneRect rect = {0};

    if (!host) return;
    if (scene_editor_pane_host_find_rect_for_pane_id(host, SCENE_EDITOR_PANE_ID_LEFT, &rect)) {
        host->target_left_width = (int)lroundf(rect.width);
    }
    if (scene_editor_pane_host_find_rect_for_pane_id(host, SCENE_EDITOR_PANE_ID_RIGHT, &rect)) {
        host->target_right_width = (int)lroundf(rect.width);
    }
}

static bool scene_editor_pane_host_assign_layout(SceneEditorPaneHost* host) {
    CorePaneRect left_rect = {0};
    CorePaneRect center_rect = {0};
    CorePaneRect right_rect = {0};
    SDL_Rect viewport = {0};
    int mode_h = SCENE_EDITOR_MODE_ROUTER_HEIGHT;

    if (!host) return false;
    if (!scene_editor_pane_host_find_rect_for_pane_id(host, SCENE_EDITOR_PANE_ID_LEFT, &left_rect) ||
        !scene_editor_pane_host_find_rect_for_pane_id(host, SCENE_EDITOR_PANE_ID_CENTER, &center_rect) ||
        !scene_editor_pane_host_find_rect_for_pane_id(host, SCENE_EDITOR_PANE_ID_RIGHT, &right_rect)) {
        scene_editor_pane_host_set_error(host, "pane solve missing expected leaf");
        return false;
    }

    memset(&host->layout, 0, sizeof(host->layout));
    host->layout.left_pane_rect = scene_editor_pane_rect_to_sdl(left_rect);
    host->layout.center_pane_rect = scene_editor_pane_rect_to_sdl(center_rect);
    host->layout.right_pane_rect = scene_editor_pane_rect_to_sdl(right_rect);

    host->layout.left_content_rect =
        pane_host_reserve_top_space(pane_host_inset_rect(host->layout.left_pane_rect,
                                                         SCENE_EDITOR_CONTENT_PADDING),
                                    SCENE_EDITOR_PANE_HEADER_HEIGHT);
    host->layout.center_content_rect =
        pane_host_reserve_top_space(pane_host_inset_rect(host->layout.center_pane_rect,
                                                         SCENE_EDITOR_CONTENT_PADDING),
                                    SCENE_EDITOR_PANE_HEADER_HEIGHT);
    host->layout.right_content_rect =
        pane_host_reserve_top_space(pane_host_inset_rect(host->layout.right_pane_rect,
                                                         SCENE_EDITOR_CONTENT_PADDING),
                                    SCENE_EDITOR_PANE_HEADER_HEIGHT);

    mode_h = pane_host_clamp_int(mode_h, 32, host->layout.center_content_rect.h / 3);
    host->layout.mode_router_rect = (SDL_Rect){
        host->layout.center_content_rect.x,
        host->layout.center_content_rect.y,
        host->layout.center_content_rect.w,
        mode_h
    };

    viewport = host->layout.center_content_rect;
    viewport.y += mode_h + SCENE_EDITOR_VIEWPORT_TOP_GAP;
    viewport.h -= mode_h + SCENE_EDITOR_VIEWPORT_TOP_GAP;
    if (viewport.h < 0) viewport.h = 0;
    host->layout.viewport_rect = viewport;
    return true;
}

static bool scene_editor_pane_host_solve_current(SceneEditorPaneHost* host, float width, float height) {
    CorePaneRect bounds;
    CorePaneValidationReport report;

    if (!host) return false;

    bounds = scene_editor_pane_host_bounds_rect(width, height);
    memset(&report, 0, sizeof(report));
    if (!core_pane_validate_graph(host->nodes,
                                  host->node_count,
                                  host->root_index,
                                  bounds,
                                  &report)) {
        scene_editor_pane_host_set_error(host,
                                         "pane graph invalid code=%s node=%u rel=%u",
                                         core_pane_validation_code_string(report.code),
                                         report.node_index,
                                         report.related_index);
        return false;
    }

    if (!core_pane_solve(host->nodes,
                         host->node_count,
                         host->root_index,
                         bounds,
                         host->leaves,
                         sizeof(host->leaves) / sizeof(host->leaves[0]),
                         &host->leaf_count)) {
        scene_editor_pane_host_set_error(host, "pane solve failed");
        return false;
    }

    if (!scene_editor_pane_host_assign_layout(host)) {
        return false;
    }

    scene_editor_pane_host_sync_targets_from_leaves(host);
    host->bounds_width = width;
    host->bounds_height = height;
    host->initialized = true;
    host->last_error[0] = '\0';
    return true;
}

static void scene_editor_pane_host_seed_graph(SceneEditorPaneHost* host) {
    if (!host) return;

    host->node_count = 5u;
    host->root_index = 0u;
    host->nodes[0] = (CorePaneNode){
        .type = CORE_PANE_NODE_SPLIT,
        .id = 1u,
        .axis = CORE_PANE_AXIS_HORIZONTAL,
        .ratio_01 = 0.25f,
        .child_a = 1u,
        .child_b = 2u,
        .constraints = {220.0f, 600.0f}
    };
    host->nodes[1] = (CorePaneNode){
        .type = CORE_PANE_NODE_LEAF,
        .id = SCENE_EDITOR_PANE_ID_LEFT
    };
    host->nodes[2] = (CorePaneNode){
        .type = CORE_PANE_NODE_SPLIT,
        .id = 2u,
        .axis = CORE_PANE_AXIS_HORIZONTAL,
        .ratio_01 = 0.65f,
        .child_a = 3u,
        .child_b = 4u,
        .constraints = {360.0f, 240.0f}
    };
    host->nodes[3] = (CorePaneNode){
        .type = CORE_PANE_NODE_LEAF,
        .id = SCENE_EDITOR_PANE_ID_CENTER
    };
    host->nodes[4] = (CorePaneNode){
        .type = CORE_PANE_NODE_LEAF,
        .id = SCENE_EDITOR_PANE_ID_RIGHT
    };
}

bool scene_editor_pane_host_rebuild(SceneEditorPaneHost* host, int width, int height) {
    int left_w = 0;
    int right_w = 0;
    int center_w = 0;
    int deficit = 0;
    int trim_right = 0;
    int trim_left = 0;
    float remaining_width = 0.0f;
    float left_ratio_min = 0.0f;
    float left_ratio_max = 0.0f;
    float center_ratio_min = 0.0f;
    float center_ratio_max = 0.0f;

    if (!host) return false;
    if (width < SCENE_EDITOR_MIN_WIDTH || height < SCENE_EDITOR_MIN_HEIGHT) {
        scene_editor_pane_host_set_error(host, "invalid editor bounds %dx%d", width, height);
        return false;
    }
    if (height < SCENE_EDITOR_MIN_PANE_HEIGHT) {
        scene_editor_pane_host_set_error(host, "insufficient editor height %d", height);
        return false;
    }

    left_w = host->target_left_width > 0 ? host->target_left_width : 286;
    right_w = host->target_right_width > 0 ? host->target_right_width : 312;
    left_w = pane_host_clamp_int(left_w, SCENE_EDITOR_MIN_LEFT_WIDTH, SCENE_EDITOR_MAX_LEFT_WIDTH);
    right_w = pane_host_clamp_int(right_w, SCENE_EDITOR_MIN_RIGHT_WIDTH, SCENE_EDITOR_MAX_RIGHT_WIDTH);

    center_w = width - left_w - right_w;
    if (center_w < SCENE_EDITOR_MIN_CENTER_WIDTH) {
        deficit = SCENE_EDITOR_MIN_CENTER_WIDTH - center_w;
        trim_right = deficit / 2;
        trim_left = deficit - trim_right;
        right_w = pane_host_clamp_int(right_w - trim_right,
                                      SCENE_EDITOR_MIN_RIGHT_WIDTH,
                                      SCENE_EDITOR_MAX_RIGHT_WIDTH);
        left_w = pane_host_clamp_int(left_w - trim_left,
                                     SCENE_EDITOR_MIN_LEFT_WIDTH,
                                     SCENE_EDITOR_MAX_LEFT_WIDTH);
        center_w = width - left_w - right_w;
        if (center_w < SCENE_EDITOR_MIN_CENTER_WIDTH) {
            scene_editor_pane_host_set_error(host,
                                             "cannot satisfy pane minimums in %dx%d",
                                             width,
                                             height);
            return false;
        }
    }

    remaining_width = (float)(width - left_w);
    if (remaining_width <= 0.0f) {
        scene_editor_pane_host_set_error(host, "invalid pane remainder in %dx%d", width, height);
        return false;
    }

    host->nodes[0].constraints = (CorePaneConstraints){220.0f, 600.0f};
    left_ratio_min = 220.0f / (float)width;
    left_ratio_max = ((float)width - 600.0f) / (float)width;
    host->nodes[0].ratio_01 = pane_host_clamp_float((float)left_w / (float)width,
                                                    left_ratio_min,
                                                    left_ratio_max);

    host->nodes[2].constraints = (CorePaneConstraints){360.0f, 240.0f};
    center_ratio_min = 360.0f / remaining_width;
    center_ratio_max = (remaining_width - 240.0f) / remaining_width;
    host->nodes[2].ratio_01 = pane_host_clamp_float((float)center_w / remaining_width,
                                                    center_ratio_min,
                                                    center_ratio_max);

    return scene_editor_pane_host_solve_current(host, (float)width, (float)height);
}

bool scene_editor_pane_host_init(SceneEditorPaneHost* host, int width, int height) {
    if (!host) return false;
    memset(host, 0, sizeof(*host));
    host->target_left_width = 286;
    host->target_right_width = 312;
    kit_pane_splitter_interaction_init(&host->splitter_interaction,
                                       (float)SCENE_EDITOR_SPLITTER_HANDLE_THICKNESS);
    scene_editor_pane_host_seed_graph(host);
    return scene_editor_pane_host_rebuild(host, width, height);
}

void scene_editor_pane_host_set_targets(SceneEditorPaneHost* host,
                                        int left_width,
                                        int right_width) {
    if (!host) return;
    if (left_width > 0) host->target_left_width = left_width;
    if (right_width > 0) host->target_right_width = right_width;
}

void scene_editor_pane_host_update_pointer(SceneEditorPaneHost* host,
                                           float pointer_x,
                                           float pointer_y) {
    CoreResult result;

    if (!host || !host->initialized) return;
    result = kit_pane_splitter_interaction_set_hover(&host->splitter_interaction,
                                                     host->nodes,
                                                     host->node_count,
                                                     host->root_index,
                                                     scene_editor_pane_host_bounds_rect(host->bounds_width,
                                                                                        host->bounds_height),
                                                     pointer_x,
                                                     pointer_y);
    if (result.code != CORE_OK && result.code != CORE_ERR_NOT_FOUND) {
        scene_editor_pane_host_set_error(host, "splitter hover failed (%d)", (int)result.code);
    }
}

bool scene_editor_pane_host_begin_splitter_drag(SceneEditorPaneHost* host,
                                                float pointer_x,
                                                float pointer_y) {
    CoreResult result;

    if (!host || !host->initialized) return false;
    result = kit_pane_splitter_interaction_begin_drag(&host->splitter_interaction,
                                                      host->nodes,
                                                      host->node_count,
                                                      host->root_index,
                                                      scene_editor_pane_host_bounds_rect(host->bounds_width,
                                                                                         host->bounds_height),
                                                      pointer_x,
                                                      pointer_y);
    return result.code == CORE_OK;
}

bool scene_editor_pane_host_update_splitter_drag(SceneEditorPaneHost* host,
                                                 float pointer_x,
                                                 float pointer_y) {
    CoreResult result;
    int changed = 0;

    if (!host || !host->initialized || !host->splitter_interaction.drag_active) return false;
    result = kit_pane_splitter_interaction_update_drag(&host->splitter_interaction,
                                                       host->nodes,
                                                       host->node_count,
                                                       pointer_x,
                                                       pointer_y,
                                                       &changed);
    if (result.code != CORE_OK) {
        scene_editor_pane_host_set_error(host, "splitter drag update failed (%d)", (int)result.code);
        return false;
    }
    if (changed) {
        if (!scene_editor_pane_host_solve_current(host, host->bounds_width, host->bounds_height)) {
            return false;
        }
    }
    return changed != 0;
}

void scene_editor_pane_host_end_splitter_drag(SceneEditorPaneHost* host) {
    if (!host) return;
    kit_pane_splitter_interaction_end_drag(&host->splitter_interaction);
}

bool scene_editor_pane_host_splitter_drag_active(const SceneEditorPaneHost* host) {
    if (!host) return false;
    return host->splitter_interaction.drag_active != 0;
}

bool scene_editor_pane_host_visible_splitter(const SceneEditorPaneHost* host,
                                             CorePaneRect* out_rect,
                                             bool* out_hovered,
                                             bool* out_active) {
    int hovered = 0;
    int active = 0;

    if (out_hovered) *out_hovered = false;
    if (out_active) *out_active = false;
    if (!host) return false;
    if (!kit_pane_splitter_interaction_current(&host->splitter_interaction,
                                               out_rect,
                                               &hovered,
                                               &active)) {
        return false;
    }
    if (out_hovered) *out_hovered = hovered != 0;
    if (out_active) *out_active = active != 0;
    return true;
}

const SceneEditorPaneLayout* scene_editor_pane_host_layout(const SceneEditorPaneHost* host) {
    if (!host || !host->initialized) return NULL;
    return &host->layout;
}

const char* scene_editor_pane_host_last_error(const SceneEditorPaneHost* host) {
    if (!host || host->last_error[0] == '\0') return "";
    return host->last_error;
}
