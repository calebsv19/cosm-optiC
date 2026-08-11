#include "ui/menu/workspace_authoring/ray_tracing_surface_authoring_canvas.h"
#include "ui/menu/workspace_authoring/ray_tracing_surface_authoring_canvas_view.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    RayTracingSurfaceAuthoringCanvasSnapshot snapshot;
    RayTracingSurfaceAuthoringCanvasViewState view;
    SDL_Rect panel;
    int pan_start_x;
    int pan_start_y;
    assert(RayTracingSurfaceAuthoringCanvasSnapshot_DefaultCube(&snapshot));
    assert(snapshot.valid && snapshot.node_count == 9u && snapshot.edge_count == 8u);
    assert(snapshot.read_only && snapshot.can_select && snapshot.can_zoom && snapshot.can_pan);
    assert(!snapshot.can_edit && !snapshot.can_save && !snapshot.can_promote);
    assert(strcmp(snapshot.document_id, "cube_surface_v1") == 0);
    assert(RayTracingSurfaceAuthoringCanvasSnapshot_LoadJsonFile(
        "tests/fixtures/procedural_surface_authoring_document_v1/cube_composition.canvas.json",
        &snapshot));
    assert(snapshot.valid && snapshot.node_count == 9u && snapshot.edge_count == 8u);
    assert(strcmp(snapshot.nodes[8].kind, "attachment") == 0);
    ray_tracing_surface_authoring_canvas_view_reset(&view);
    ray_tracing_surface_authoring_canvas_view_panel_for_viewport(1280, 720, &panel);
    assert(ray_tracing_surface_authoring_canvas_view_contains(&panel, 350, 380));
    assert(ray_tracing_surface_authoring_canvas_view_select(
               &view, &snapshot, &panel, 350, 380) == 0);
    ray_tracing_surface_authoring_canvas_view_zoom(&view, &panel, 350, 380, 3);
    assert(view.zoom > 1.0f && view.selected_node == 0);
    pan_start_x = view.pan_x;
    pan_start_y = view.pan_y;
    ray_tracing_surface_authoring_canvas_view_begin_pan(&view, 350, 380);
    ray_tracing_surface_authoring_canvas_view_update_pan(&view, 370, 400);
    ray_tracing_surface_authoring_canvas_view_end_pan(&view);
    assert(!view.panning && view.pan_x == pan_start_x + 20 && view.pan_y == pan_start_y + 20);
    puts("surface_authoring_canvas loader=ok read_only=ok selection=ok zoom=ok pan=ok");
    return 0;
}
