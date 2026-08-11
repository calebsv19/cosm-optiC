#include "ui/menu/workspace_authoring/ray_tracing_surface_authoring_canvas_view.h"

#include <math.h>

enum {
    CANVAS_WIDTH = 760,
    CANVAS_HEIGHT = 560,
    CANVAS_ORIGIN_X = 14,
    CANVAS_ORIGIN_Y = 56,
    CANVAS_FOOTER_HEIGHT = 30
};

static float clamp_zoom(float zoom) {
    if (zoom < 0.5f) return 0.5f;
    if (zoom > 2.5f) return 2.5f;
    return zoom;
}

static bool node_rect(const RayTracingSurfaceAuthoringCanvasNode* node,
                      const RayTracingSurfaceAuthoringCanvasViewState* state,
                      const SDL_Rect* panel,
                      SDL_Rect* out_rect) {
    int x = 0;
    int y = 0;
    int width;
    if (!node || !state || !panel || !out_rect || panel->w <= 28 || panel->h <= 86) return false;
    ray_tracing_surface_authoring_canvas_view_project(state, panel, node->x, node->y, &x, &y);
    width = (node->kind[0] == 's') ? 142 : (node->kind[0] == 'l') ? 166 : 176;
    *out_rect = (SDL_Rect){x, y, (int)lroundf((float)width * state->zoom),
                           (int)lroundf(44.0f * state->zoom)};
    return true;
}

void ray_tracing_surface_authoring_canvas_view_reset(
    RayTracingSurfaceAuthoringCanvasViewState* state) {
    if (!state) return;
    *state = (RayTracingSurfaceAuthoringCanvasViewState){
        .zoom = 1.0f,
        .selected_node = -1
    };
}

void ray_tracing_surface_authoring_canvas_view_panel_for_viewport(
    int width, int height, SDL_Rect* out_panel) {
    int left = 286;
    int right = 312;
    if (!out_panel) return;
    if (width < 780) {
        left = 220;
        right = 240;
    }
    *out_panel = (SDL_Rect){left + 10, 10, width - left - right - 20, height - 20};
    if (out_panel->w < 0) out_panel->w = 0;
    if (out_panel->h < 0) out_panel->h = 0;
}

bool ray_tracing_surface_authoring_canvas_view_contains(
    const SDL_Rect* panel, int x, int y) {
    SDL_Rect graph;
    if (!panel) return false;
    graph = (SDL_Rect){panel->x + CANVAS_ORIGIN_X, panel->y + CANVAS_ORIGIN_Y,
                       panel->w - 28, panel->h - CANVAS_ORIGIN_Y - CANVAS_FOOTER_HEIGHT};
    return graph.w > 0 && graph.h > 0 && x >= graph.x && x < graph.x + graph.w &&
           y >= graph.y && y < graph.y + graph.h;
}

void ray_tracing_surface_authoring_canvas_view_begin_pan(
    RayTracingSurfaceAuthoringCanvasViewState* state, int x, int y) {
    if (!state) return;
    state->panning = true;
    state->pan_anchor_x = x;
    state->pan_anchor_y = y;
    state->pan_start_x = state->pan_x;
    state->pan_start_y = state->pan_y;
}

void ray_tracing_surface_authoring_canvas_view_update_pan(
    RayTracingSurfaceAuthoringCanvasViewState* state, int x, int y) {
    if (!state || !state->panning) return;
    state->pan_x = state->pan_start_x + x - state->pan_anchor_x;
    state->pan_y = state->pan_start_y + y - state->pan_anchor_y;
}

void ray_tracing_surface_authoring_canvas_view_end_pan(
    RayTracingSurfaceAuthoringCanvasViewState* state) {
    if (state) state->panning = false;
}

void ray_tracing_surface_authoring_canvas_view_zoom(
    RayTracingSurfaceAuthoringCanvasViewState* state,
    const SDL_Rect* panel,
    int pointer_x,
    int pointer_y,
    int wheel_steps) {
    float old_zoom;
    float new_zoom;
    if (!state || !panel || wheel_steps == 0) return;
    old_zoom = clamp_zoom(state->zoom);
    new_zoom = clamp_zoom(old_zoom + (float)wheel_steps * 0.1f);
    if (new_zoom == old_zoom) return;
    state->pan_x = pointer_x - (int)lroundf((float)(pointer_x - panel->x - CANVAS_ORIGIN_X - state->pan_x) * new_zoom / old_zoom) - panel->x - CANVAS_ORIGIN_X;
    state->pan_y = pointer_y - (int)lroundf((float)(pointer_y - panel->y - CANVAS_ORIGIN_Y - state->pan_y) * new_zoom / old_zoom) - panel->y - CANVAS_ORIGIN_Y;
    state->zoom = new_zoom;
}

int ray_tracing_surface_authoring_canvas_view_select(
    RayTracingSurfaceAuthoringCanvasViewState* state,
    const RayTracingSurfaceAuthoringCanvasSnapshot* snapshot,
    const SDL_Rect* panel,
    int x,
    int y) {
    size_t i;
    if (!state || !snapshot || !panel) return -1;
    for (i = 0u; i < snapshot->node_count; ++i) {
        SDL_Rect rect;
        if (node_rect(&snapshot->nodes[i], state, panel, &rect) &&
            x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h) {
            state->selected_node = (int)i;
            return (int)i;
        }
    }
    state->selected_node = -1;
    return -1;
}

void ray_tracing_surface_authoring_canvas_view_project(
    const RayTracingSurfaceAuthoringCanvasViewState* state,
    const SDL_Rect* panel,
    int canvas_x,
    int canvas_y,
    int* out_x,
    int* out_y) {
    float zoom = state ? clamp_zoom(state->zoom) : 1.0f;
    float scale_x;
    float scale_y;
    if (!panel || !out_x || !out_y) return;
    scale_x = (float)(panel->w - 28) / (float)CANVAS_WIDTH;
    scale_y = (float)(panel->h - CANVAS_ORIGIN_Y - CANVAS_FOOTER_HEIGHT) / (float)CANVAS_HEIGHT;
    *out_x = panel->x + CANVAS_ORIGIN_X + (state ? state->pan_x : 0) + (int)lroundf((float)canvas_x * scale_x * zoom);
    *out_y = panel->y + CANVAS_ORIGIN_Y + (state ? state->pan_y : 0) + (int)lroundf((float)canvas_y * scale_y * zoom);
}
