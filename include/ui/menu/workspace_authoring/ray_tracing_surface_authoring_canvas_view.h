#ifndef RAY_TRACING_SURFACE_AUTHORING_CANVAS_VIEW_H
#define RAY_TRACING_SURFACE_AUTHORING_CANVAS_VIEW_H

#include <stdbool.h>

#include <SDL2/SDL.h>

#include "ui/menu/workspace_authoring/ray_tracing_surface_authoring_canvas.h"

typedef struct RayTracingSurfaceAuthoringCanvasViewState {
    float zoom;
    int pan_x;
    int pan_y;
    int selected_node;
    bool panning;
    int pan_anchor_x;
    int pan_anchor_y;
    int pan_start_x;
    int pan_start_y;
} RayTracingSurfaceAuthoringCanvasViewState;

void ray_tracing_surface_authoring_canvas_view_reset(
    RayTracingSurfaceAuthoringCanvasViewState* state);
void ray_tracing_surface_authoring_canvas_view_panel_for_viewport(
    int width, int height, SDL_Rect* out_panel);
bool ray_tracing_surface_authoring_canvas_view_contains(
    const SDL_Rect* panel, int x, int y);
void ray_tracing_surface_authoring_canvas_view_begin_pan(
    RayTracingSurfaceAuthoringCanvasViewState* state, int x, int y);
void ray_tracing_surface_authoring_canvas_view_update_pan(
    RayTracingSurfaceAuthoringCanvasViewState* state, int x, int y);
void ray_tracing_surface_authoring_canvas_view_end_pan(
    RayTracingSurfaceAuthoringCanvasViewState* state);
void ray_tracing_surface_authoring_canvas_view_zoom(
    RayTracingSurfaceAuthoringCanvasViewState* state,
    const SDL_Rect* panel,
    int pointer_x,
    int pointer_y,
    int wheel_steps);
int ray_tracing_surface_authoring_canvas_view_select(
    RayTracingSurfaceAuthoringCanvasViewState* state,
    const RayTracingSurfaceAuthoringCanvasSnapshot* snapshot,
    const SDL_Rect* panel,
    int x,
    int y);
void ray_tracing_surface_authoring_canvas_view_project(
    const RayTracingSurfaceAuthoringCanvasViewState* state,
    const SDL_Rect* panel,
    int canvas_x,
    int canvas_y,
    int* out_x,
    int* out_y);

#endif
