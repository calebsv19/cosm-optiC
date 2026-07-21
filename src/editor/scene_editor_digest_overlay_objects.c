#include "editor/scene_editor_digest_overlay_internal.h"

#include <math.h>
#include <string.h>

#include "config/config_manager.h"
#include "editor/editor_mode_router.h"
#include "editor/material_editor.h"
#include "editor/scene_editor_material_preview.h"
#include "editor/scene_editor_mesh_preview_render.h"
#include "scene/object_manager.h"

#define SCENE_EDITOR_DIGEST_OVERLAY_BOUNDS_DRAW_RATIO_LIMIT (3.0)

static void scene_editor_digest_overlay_draw_dashed_screen_line(SDL_Renderer* renderer,
                                                                int x0,
                                                                int y0,
                                                                int x1,
                                                                int y1) {
    double dx = 0.0;
    double dy = 0.0;
    double length = 0.0;
    double ux = 0.0;
    double uy = 0.0;
    double offset = 0.0;
    const int dash_len = 7;
    const int gap_len = 5;
    if (!renderer) return;
    dx = (double)(x1 - x0);
    dy = (double)(y1 - y0);
    length = hypot(dx, dy);
    if (length <= 1.0) {
        SDL_RenderDrawPoint(renderer, x0, y0);
        SDL_RenderDrawPoint(renderer, x1, y1);
        return;
    }
    ux = dx / length;
    uy = dy / length;
    while (offset < length) {
        double dash_end = offset + (double)dash_len;
        int ax = 0;
        int ay = 0;
        int bx = 0;
        int by = 0;
        if (dash_end > length) dash_end = length;
        ax = (int)lround((double)x0 + ux * offset);
        ay = (int)lround((double)y0 + uy * offset);
        bx = (int)lround((double)x0 + ux * dash_end);
        by = (int)lround((double)y0 + uy * dash_end);
        SDL_RenderDrawLine(renderer, ax, ay, bx, by);
        offset = dash_end + (double)gap_len;
    }
}

static void scene_editor_digest_overlay_draw_dashed_line3(
    SDL_Renderer* renderer,
    const SceneEditorDigestOverlayProjector* projector,
    double ax,
    double ay,
    double az,
    double bx,
    double by,
    double bz,
    SDL_Color color) {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    if (!renderer || !projector) return;
    if (!SceneEditorDigestOverlayProjectPoint(projector, ax, ay, az, &x0, &y0)) return;
    if (!SceneEditorDigestOverlayProjectPoint(projector, bx, by, bz, &x1, &y1)) return;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    scene_editor_digest_overlay_draw_dashed_screen_line(renderer, x0, y0, x1, y1);
}

static void scene_editor_digest_overlay_draw_seed_selection_marker(
    SDL_Renderer* renderer,
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridgePrimitiveSeed* primitive,
    SDL_Color color) {
    int cx = 0;
    int cy = 0;
    const int outer_r = 5;
    const int inner_r = 2;
    SDL_Rect outer_box = {0, 0, 0, 0};
    SDL_Rect inner_box = {0, 0, 0, 0};
    if (!renderer || !projector || !primitive) return;
    if (!SceneEditorDigestOverlayProjectPoint(projector,
                                              primitive->origin_x,
                                              primitive->origin_y,
                                              primitive->origin_z,
                                              &cx,
                                              &cy)) {
        return;
    }
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    outer_box = (SDL_Rect){cx - outer_r, cy - outer_r, outer_r * 2, outer_r * 2};
    inner_box = (SDL_Rect){cx - inner_r, cy - inner_r, inner_r * 2, inner_r * 2};
    SDL_RenderDrawRect(renderer, &outer_box);
    SDL_RenderFillRect(renderer, &inner_box);
}

static void scene_editor_digest_overlay_draw_seed_plane(SDL_Renderer* renderer,
                                                        const SceneEditorDigestOverlayProjector* projector,
                                                        const RuntimeSceneBridgePrimitiveSeed* primitive,
                                                        SDL_Color color) {
    double corners[4][3] = {{0.0}};
    double half_w = 0.0;
    double half_h = 0.0;
    int corner = 0;
    int sx = 0;
    int sy = 0;
    static const int edges[4][2] = {
        {0, 1}, {1, 3}, {3, 2}, {2, 0}
    };
    if (!renderer || !projector || !primitive || !primitive->has_dimensions) return;
    half_w = fmax(0.05, fabs(primitive->width) * 0.5);
    half_h = fmax(0.05, fabs(primitive->height) * 0.5);
    for (sx = -1; sx <= 1; sx += 2) {
        for (sy = -1; sy <= 1; sy += 2) {
            corners[corner][0] = primitive->origin_x +
                                 primitive->axis_u_x * half_w * (double)sx +
                                 primitive->axis_v_x * half_h * (double)sy;
            corners[corner][1] = primitive->origin_y +
                                 primitive->axis_u_y * half_w * (double)sx +
                                 primitive->axis_v_y * half_h * (double)sy;
            corners[corner][2] = primitive->origin_z +
                                 primitive->axis_u_z * half_w * (double)sx +
                                 primitive->axis_v_z * half_h * (double)sy;
            corner += 1;
        }
    }
    for (corner = 0; corner < 4; ++corner) {
        int a = edges[corner][0];
        int b = edges[corner][1];
        SceneEditorDigestOverlayDrawLine3(renderer,
                                          projector,
                                          corners[a][0], corners[a][1], corners[a][2],
                                          corners[b][0], corners[b][1], corners[b][2],
                                          color);
    }
}

static void scene_editor_digest_overlay_draw_seed_plane_guide(
    SDL_Renderer* renderer,
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridgePrimitiveSeed* primitive,
    SDL_Color color) {
    double corners[4][3] = {{0.0}};
    double half_w = 0.0;
    double half_h = 0.0;
    int corner = 0;
    int sx = 0;
    int sy = 0;
    static const int edges[4][2] = {
        {0, 1}, {1, 3}, {3, 2}, {2, 0}
    };
    if (!renderer || !projector || !primitive || !primitive->has_dimensions) return;
    half_w = fmax(0.05, fabs(primitive->width) * 0.5);
    half_h = fmax(0.05, fabs(primitive->height) * 0.5);
    for (sx = -1; sx <= 1; sx += 2) {
        for (sy = -1; sy <= 1; sy += 2) {
            corners[corner][0] = primitive->origin_x +
                                 primitive->axis_u_x * half_w * (double)sx +
                                 primitive->axis_v_x * half_h * (double)sy;
            corners[corner][1] = primitive->origin_y +
                                 primitive->axis_u_y * half_w * (double)sx +
                                 primitive->axis_v_y * half_h * (double)sy;
            corners[corner][2] = primitive->origin_z +
                                 primitive->axis_u_z * half_w * (double)sx +
                                 primitive->axis_v_z * half_h * (double)sy;
            corner += 1;
        }
    }
    for (corner = 0; corner < 4; ++corner) {
        int a = edges[corner][0];
        int b = edges[corner][1];
        scene_editor_digest_overlay_draw_dashed_line3(renderer,
                                                      projector,
                                                      corners[a][0], corners[a][1], corners[a][2],
                                                      corners[b][0], corners[b][1], corners[b][2],
                                                      color);
    }
}

static SDL_Color SceneEditorColorFromPackedRGB(int packed, Uint8 alpha) {
    SDL_Color out;
    out.r = (Uint8)((packed >> 16) & 0xFF);
    out.g = (Uint8)((packed >> 8) & 0xFF);
    out.b = (Uint8)(packed & 0xFF);
    out.a = alpha;
    return out;
}

static SDL_Color SceneEditorDigestOverlayResolvePrimitiveColor(int primitive_index) {
    SDL_Color color = {220, 200, 88, 240};
    if (primitive_index >= 0 && primitive_index < sceneSettings.objectCount) {
        SceneObject* obj = &sceneSettings.sceneObjects[primitive_index];
        if (obj->color != 0) {
            color = SceneEditorColorFromPackedRGB(obj->color, 240);
        }
    }
    return color;
}

static void SceneEditorDigestOverlayDrawSelectionMarker(
    SDL_Renderer* renderer,
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridgePrimitiveDigest* primitive,
    SDL_Color color) {
    int cx = 0;
    int cy = 0;
    const int outer_r = 5;
    const int inner_r = 2;
    SDL_Rect outer_box = {0, 0, 0, 0};
    SDL_Rect inner_box = {0, 0, 0, 0};
    if (!renderer || !projector || !primitive) return;
    if (!SceneEditorDigestOverlayProjectPoint(projector,
                                              primitive->origin_x,
                                              primitive->origin_y,
                                              primitive->origin_z,
                                              &cx,
                                              &cy)) {
        return;
    }
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    outer_box = (SDL_Rect){cx - outer_r, cy - outer_r, outer_r * 2, outer_r * 2};
    inner_box = (SDL_Rect){cx - inner_r, cy - inner_r, inner_r * 2, inner_r * 2};
    SDL_RenderDrawRect(renderer, &outer_box);
    SDL_RenderFillRect(renderer, &inner_box);
}

static void SceneEditorDigestOverlayDrawBox(SDL_Renderer* renderer,
                                            const SceneEditorDigestOverlayProjector* projector,
                                            double min_x,
                                            double min_y,
                                            double min_z,
                                            double max_x,
                                            double max_y,
                                            double max_z,
                                            SDL_Color color) {
    static const int k_edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };
    double corners[8][3];
    int i = 0;
    if (!renderer || !projector) return;
    corners[0][0] = min_x; corners[0][1] = min_y; corners[0][2] = min_z;
    corners[1][0] = max_x; corners[1][1] = min_y; corners[1][2] = min_z;
    corners[2][0] = max_x; corners[2][1] = max_y; corners[2][2] = min_z;
    corners[3][0] = min_x; corners[3][1] = max_y; corners[3][2] = min_z;
    corners[4][0] = min_x; corners[4][1] = min_y; corners[4][2] = max_z;
    corners[5][0] = max_x; corners[5][1] = min_y; corners[5][2] = max_z;
    corners[6][0] = max_x; corners[6][1] = max_y; corners[6][2] = max_z;
    corners[7][0] = min_x; corners[7][1] = max_y; corners[7][2] = max_z;
    for (i = 0; i < 12; ++i) {
        int a = k_edges[i][0];
        int b = k_edges[i][1];
        SceneEditorDigestOverlayDrawLine3(renderer,
                                          projector,
                                          corners[a][0], corners[a][1], corners[a][2],
                                          corners[b][0], corners[b][1], corners[b][2],
                                          color);
    }
}

static void SceneEditorDigestOverlayDrawConstructionPlane(SDL_Renderer* renderer,
                                                          const SceneEditorDigestOverlayProjector* projector,
                                                          const RuntimeSceneBridge3DDigestState* digest,
                                                          SDL_Color color) {
    double min_x = 0.0;
    double max_x = 0.0;
    double min_y = 0.0;
    double max_y = 0.0;
    double plane_z = 0.0;
    if (!renderer || !projector || !digest) return;
    if (!digest->has_construction_plane) return;
    if (!SceneEditorDigestOverlayResolveExtents(digest,
                                                &min_x,
                                                &min_y,
                                                NULL,
                                                &max_x,
                                                &max_y,
                                                NULL,
                                                NULL)) {
        return;
    }
    plane_z = digest->construction_plane_offset;
    SceneEditorDigestOverlayDrawLine3(renderer, projector, min_x, min_y, plane_z, max_x, min_y, plane_z, color);
    SceneEditorDigestOverlayDrawLine3(renderer, projector, max_x, min_y, plane_z, max_x, max_y, plane_z, color);
    SceneEditorDigestOverlayDrawLine3(renderer, projector, max_x, max_y, plane_z, min_x, max_y, plane_z, color);
    SceneEditorDigestOverlayDrawLine3(renderer, projector, min_x, max_y, plane_z, min_x, min_y, plane_z, color);
}

static void SceneEditorDigestOverlayDrawPrism(SDL_Renderer* renderer,
                                              const SceneEditorDigestOverlayProjector* projector,
                                              const RuntimeSceneBridgePrimitiveDigest* primitive,
                                              SDL_Color color) {
    double half_w = 0.0;
    double half_h = 0.0;
    double half_d = 0.0;
    if (!renderer || !projector || !primitive) return;
    if (!(primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM ||
          primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_BOX)) {
        return;
    }
    half_w = fmax(0.05, fabs(primitive->width) * 0.5);
    half_h = fmax(0.05, fabs(primitive->height) * 0.5);
    half_d = fmax(0.05, fabs(primitive->depth) * 0.5);
    SceneEditorDigestOverlayDrawBox(renderer,
                                    projector,
                                    primitive->origin_x - half_w,
                                    primitive->origin_y - half_h,
                                    primitive->origin_z - half_d,
                                    primitive->origin_x + half_w,
                                    primitive->origin_y + half_h,
                                    primitive->origin_z + half_d,
                                    color);
}

static void SceneEditorDigestOverlayDrawPrismGuide(SDL_Renderer* renderer,
                                                   const SceneEditorDigestOverlayProjector* projector,
                                                   const RuntimeSceneBridgePrimitiveDigest* primitive,
                                                   SDL_Color color) {
    double half_w = 0.0;
    double half_h = 0.0;
    double half_d = 0.0;
    if (!renderer || !projector || !primitive) return;
    if (!(primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM ||
          primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_BOX)) {
        return;
    }
    half_w = fmax(0.05, fabs(primitive->width) * 0.5);
    half_h = fmax(0.05, fabs(primitive->height) * 0.5);
    half_d = fmax(0.05, fabs(primitive->depth) * 0.5);
    scene_editor_digest_overlay_draw_dashed_line3(renderer, projector,
                                                  primitive->origin_x - half_w, primitive->origin_y - half_h, primitive->origin_z - half_d,
                                                  primitive->origin_x + half_w, primitive->origin_y - half_h, primitive->origin_z - half_d,
                                                  color);
    scene_editor_digest_overlay_draw_dashed_line3(renderer, projector,
                                                  primitive->origin_x + half_w, primitive->origin_y - half_h, primitive->origin_z - half_d,
                                                  primitive->origin_x + half_w, primitive->origin_y + half_h, primitive->origin_z - half_d,
                                                  color);
    scene_editor_digest_overlay_draw_dashed_line3(renderer, projector,
                                                  primitive->origin_x + half_w, primitive->origin_y + half_h, primitive->origin_z - half_d,
                                                  primitive->origin_x - half_w, primitive->origin_y + half_h, primitive->origin_z - half_d,
                                                  color);
    scene_editor_digest_overlay_draw_dashed_line3(renderer, projector,
                                                  primitive->origin_x - half_w, primitive->origin_y + half_h, primitive->origin_z - half_d,
                                                  primitive->origin_x - half_w, primitive->origin_y - half_h, primitive->origin_z - half_d,
                                                  color);
    scene_editor_digest_overlay_draw_dashed_line3(renderer, projector,
                                                  primitive->origin_x - half_w, primitive->origin_y - half_h, primitive->origin_z + half_d,
                                                  primitive->origin_x + half_w, primitive->origin_y - half_h, primitive->origin_z + half_d,
                                                  color);
    scene_editor_digest_overlay_draw_dashed_line3(renderer, projector,
                                                  primitive->origin_x + half_w, primitive->origin_y - half_h, primitive->origin_z + half_d,
                                                  primitive->origin_x + half_w, primitive->origin_y + half_h, primitive->origin_z + half_d,
                                                  color);
    scene_editor_digest_overlay_draw_dashed_line3(renderer, projector,
                                                  primitive->origin_x + half_w, primitive->origin_y + half_h, primitive->origin_z + half_d,
                                                  primitive->origin_x - half_w, primitive->origin_y + half_h, primitive->origin_z + half_d,
                                                  color);
    scene_editor_digest_overlay_draw_dashed_line3(renderer, projector,
                                                  primitive->origin_x - half_w, primitive->origin_y + half_h, primitive->origin_z + half_d,
                                                  primitive->origin_x - half_w, primitive->origin_y - half_h, primitive->origin_z + half_d,
                                                  color);
    scene_editor_digest_overlay_draw_dashed_line3(renderer, projector,
                                                  primitive->origin_x - half_w, primitive->origin_y - half_h, primitive->origin_z - half_d,
                                                  primitive->origin_x - half_w, primitive->origin_y - half_h, primitive->origin_z + half_d,
                                                  color);
    scene_editor_digest_overlay_draw_dashed_line3(renderer, projector,
                                                  primitive->origin_x + half_w, primitive->origin_y - half_h, primitive->origin_z - half_d,
                                                  primitive->origin_x + half_w, primitive->origin_y - half_h, primitive->origin_z + half_d,
                                                  color);
    scene_editor_digest_overlay_draw_dashed_line3(renderer, projector,
                                                  primitive->origin_x + half_w, primitive->origin_y + half_h, primitive->origin_z - half_d,
                                                  primitive->origin_x + half_w, primitive->origin_y + half_h, primitive->origin_z + half_d,
                                                  color);
    scene_editor_digest_overlay_draw_dashed_line3(renderer, projector,
                                                  primitive->origin_x - half_w, primitive->origin_y + half_h, primitive->origin_z - half_d,
                                                  primitive->origin_x - half_w, primitive->origin_y + half_h, primitive->origin_z + half_d,
                                                  color);
}

static void scene_editor_digest_overlay_draw_seed_prism(
    SDL_Renderer* renderer,
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridgePrimitiveSeed* primitive,
    SDL_Color color) {
    static const int edges[12][2] = {
        {0, 1}, {1, 3}, {3, 2}, {2, 0},
        {4, 5}, {5, 7}, {7, 6}, {6, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };
    double corners[8][3] = {{0.0}};
    double half_w = 0.0;
    double half_h = 0.0;
    double half_d = 0.0;
    int sx = 0;
    int sy = 0;
    int sz = 0;
    int corner = 0;
    int edge = 0;
    if (!renderer || !projector || !primitive || !primitive->has_dimensions) return;
    half_w = fmax(0.05, fabs(primitive->width) * 0.5);
    half_h = fmax(0.05, fabs(primitive->height) * 0.5);
    half_d = fmax(0.05, fabs(primitive->depth) * 0.5);
    for (sx = -1; sx <= 1; sx += 2) {
        for (sy = -1; sy <= 1; sy += 2) {
            for (sz = -1; sz <= 1; sz += 2) {
                corners[corner][0] = primitive->origin_x +
                                     primitive->axis_u_x * half_w * (double)sx +
                                     primitive->axis_v_x * half_h * (double)sy +
                                     primitive->normal_x * half_d * (double)sz;
                corners[corner][1] = primitive->origin_y +
                                     primitive->axis_u_y * half_w * (double)sx +
                                     primitive->axis_v_y * half_h * (double)sy +
                                     primitive->normal_y * half_d * (double)sz;
                corners[corner][2] = primitive->origin_z +
                                     primitive->axis_u_z * half_w * (double)sx +
                                     primitive->axis_v_z * half_h * (double)sy +
                                     primitive->normal_z * half_d * (double)sz;
                corner += 1;
            }
        }
    }
    for (edge = 0; edge < 12; ++edge) {
        int a = edges[edge][0];
        int b = edges[edge][1];
        SceneEditorDigestOverlayDrawLine3(renderer,
                                          projector,
                                          corners[a][0], corners[a][1], corners[a][2],
                                          corners[b][0], corners[b][1], corners[b][2],
                                          color);
    }
}

static void scene_editor_digest_overlay_draw_seed_prism_guide(
    SDL_Renderer* renderer,
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridgePrimitiveSeed* primitive,
    SDL_Color color) {
    static const int edges[12][2] = {
        {0, 1}, {1, 3}, {3, 2}, {2, 0},
        {4, 5}, {5, 7}, {7, 6}, {6, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };
    double corners[8][3] = {{0.0}};
    double half_w = 0.0;
    double half_h = 0.0;
    double half_d = 0.0;
    int sx = 0;
    int sy = 0;
    int sz = 0;
    int corner = 0;
    int edge = 0;
    if (!renderer || !projector || !primitive || !primitive->has_dimensions) return;
    half_w = fmax(0.05, fabs(primitive->width) * 0.5);
    half_h = fmax(0.05, fabs(primitive->height) * 0.5);
    half_d = fmax(0.05, fabs(primitive->depth) * 0.5);
    for (sx = -1; sx <= 1; sx += 2) {
        for (sy = -1; sy <= 1; sy += 2) {
            for (sz = -1; sz <= 1; sz += 2) {
                corners[corner][0] = primitive->origin_x +
                                     primitive->axis_u_x * half_w * (double)sx +
                                     primitive->axis_v_x * half_h * (double)sy +
                                     primitive->normal_x * half_d * (double)sz;
                corners[corner][1] = primitive->origin_y +
                                     primitive->axis_u_y * half_w * (double)sx +
                                     primitive->axis_v_y * half_h * (double)sy +
                                     primitive->normal_y * half_d * (double)sz;
                corners[corner][2] = primitive->origin_z +
                                     primitive->axis_u_z * half_w * (double)sx +
                                     primitive->axis_v_z * half_h * (double)sy +
                                     primitive->normal_z * half_d * (double)sz;
                corner += 1;
            }
        }
    }
    for (edge = 0; edge < 12; ++edge) {
        int a = edges[edge][0];
        int b = edges[edge][1];
        scene_editor_digest_overlay_draw_dashed_line3(renderer,
                                                      projector,
                                                      corners[a][0], corners[a][1], corners[a][2],
                                                      corners[b][0], corners[b][1], corners[b][2],
                                                      color);
    }
}

void SceneEditorDigestOverlayRenderObjectLayer(SDL_Renderer* renderer,
                                               const SceneEditorDigestOverlayProjector* projector,
                                               const RuntimeSceneBridge3DDigestState* digest,
                                               int active_mode,
                                               int selected_object_index,
                                               int hover_object_index) {
    RuntimeSceneBridge3DPrimitiveSeedState seeds = {0};
    int i = 0;
    bool material_focus_mode = (active_mode == EDITOR_MODE_MATERIAL);
    bool material_preview_rendered = false;
    bool preview_surface_composed = false;
    SceneEditorMeshDisplayMode preview_mode = SceneEditorMeshPreviewModeGet();
    SceneEditorMaterialPreviewTriangleAddress selected_triangles
        [SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES];
    int selected_triangle_count = 0;
    if (!renderer || !projector || !digest) return;
    runtime_scene_bridge_get_last_3d_primitive_seed_state(&seeds);

    if (active_mode == EDITOR_MODE_OBJECT || active_mode == EDITOR_MODE_MATERIAL) {
        SceneEditorMeshPreviewFrameStats mesh_stats = {0};
        preview_surface_composed = SceneEditorMeshPreviewRenderGeometry(
            renderer,
            projector,
            active_mode,
            selected_object_index,
            hover_object_index,
            &mesh_stats) &&
            (preview_mode == SCENE_EDITOR_MESH_DISPLAY_SOLID ||
             preview_mode == SCENE_EDITOR_MESH_DISPLAY_MATERIAL);
    }

    if (!material_focus_mode && digest->has_scene_bounds) {
        SDL_Color bounds_color = digest->bounds_enabled
                                     ? (SDL_Color){90, 130, 190, 118}
                                     : (SDL_Color){70, 84, 102, 82};
        double bounds_span = fmax(fabs(digest->bounds_max_x - digest->bounds_min_x),
                                  fmax(fabs(digest->bounds_max_y - digest->bounds_min_y),
                                       fabs(digest->bounds_max_z - digest->bounds_min_z)));
        bool draw_bounds = !(seeds.valid &&
                             seeds.primitive_count > 0 &&
                             projector->span_max > 0.0 &&
                             bounds_span > projector->span_max *
                                               SCENE_EDITOR_DIGEST_OVERLAY_BOUNDS_DRAW_RATIO_LIMIT);
        if (draw_bounds) {
            SceneEditorDigestOverlayDrawBox(renderer,
                                            projector,
                                            digest->bounds_min_x,
                                            digest->bounds_min_y,
                                            digest->bounds_min_z,
                                            digest->bounds_max_x,
                                            digest->bounds_max_y,
                                            digest->bounds_max_z,
                                            bounds_color);
        }
    }

    if (!material_focus_mode) {
        SceneEditorDigestOverlayDrawConstructionPlane(renderer,
                                                      projector,
                                                      digest,
                                                      (SDL_Color){205, 176, 106, 112});
    } else if (selected_object_index >= 0) {
        int material_selected_count = MaterialEditorSelectedTriangleCount();
        if (material_selected_count > SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES) {
            material_selected_count = SCENE_EDITOR_MATERIAL_PREVIEW_MAX_TRIANGLES;
        }
        memset(selected_triangles, 0, sizeof(selected_triangles));
        for (int selected_i = 0; selected_i < material_selected_count; ++selected_i) {
            if (MaterialEditorGetSelectedTriangle(selected_i,
                                                  &selected_triangles[selected_triangle_count])) {
                selected_triangle_count += 1;
            }
        }
        material_preview_rendered =
            SceneEditorMaterialPreviewRenderFocusedObjectWithSelection(
                renderer,
                projector,
                selected_object_index,
                MaterialEditorGetSolidFacesEnabled(),
                selected_triangles,
                selected_triangle_count);
    }

    if (seeds.valid && seeds.primitive_count > 0) {
        for (i = 0; i < seeds.primitive_count; ++i) {
            const RuntimeSceneBridgePrimitiveSeed* primitive = &seeds.primitives[i];
            SDL_Color primitive_color =
                SceneEditorDigestOverlayResolvePrimitiveColor(primitive->scene_object_index);
            bool is_selected = ((active_mode == EDITOR_MODE_OBJECT ||
                                 active_mode == EDITOR_MODE_MATERIAL) &&
                                selected_object_index == primitive->scene_object_index);
            bool is_hover = (active_mode == EDITOR_MODE_OBJECT &&
                             hover_object_index == primitive->scene_object_index);
            SDL_Color highlight_color = is_selected
                                            ? (SDL_Color){255, 120, 70, 255}
                                            : (SDL_Color){84, 224, 255, 245};
            primitive_color.a = is_selected ? 210u : (is_hover ? 160u : 88u);
            if (material_focus_mode && !is_selected) continue;
            if (primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE) {
                if (primitive->guide_only) {
                    scene_editor_digest_overlay_draw_seed_plane_guide(renderer,
                                                                      projector,
                                                                      primitive,
                                                                      primitive_color);
                } else if (!material_preview_rendered &&
                           SceneEditorMeshPreviewDrawsPrimitiveWire(
                               preview_mode,
                               primitive->guide_only,
                               preview_surface_composed)) {
                    scene_editor_digest_overlay_draw_seed_plane(renderer,
                                                                projector,
                                                                primitive,
                                                                primitive_color);
                }
            } else {
                if (primitive->guide_only) {
                    scene_editor_digest_overlay_draw_seed_prism_guide(renderer,
                                                                      projector,
                                                                      primitive,
                                                                      primitive_color);
                } else if (!material_preview_rendered &&
                           SceneEditorMeshPreviewDrawsPrimitiveWire(
                               preview_mode,
                               primitive->guide_only,
                               preview_surface_composed)) {
                    scene_editor_digest_overlay_draw_seed_prism(renderer,
                                                                projector,
                                                                primitive,
                                                                primitive_color);
                }
            }
            if (is_selected || is_hover) {
                scene_editor_digest_overlay_draw_seed_selection_marker(renderer,
                                                                       projector,
                                                                       primitive,
                                                                       highlight_color);
            }
        }
        for (i = 0; i < digest->primitive_count && i < sceneSettings.objectCount; ++i) {
            const RuntimeSceneBridgePrimitiveDigest* primitive = &digest->primitives[i];
            SDL_Color primitive_color =
                SceneEditorDigestOverlayResolvePrimitiveColor(primitive->scene_object_index);
            bool is_selected = ((active_mode == EDITOR_MODE_OBJECT ||
                                 active_mode == EDITOR_MODE_MATERIAL) &&
                                selected_object_index == primitive->scene_object_index);
            bool is_hover = (active_mode == EDITOR_MODE_OBJECT &&
                             hover_object_index == primitive->scene_object_index);
            SDL_Color highlight_color = is_selected
                                            ? (SDL_Color){255, 120, 70, 255}
                                            : (SDL_Color){84, 224, 255, 245};
            primitive_color.a = is_selected ? 210u : (is_hover ? 160u : 88u);
            if (material_focus_mode && !is_selected) continue;
            if (!primitive->guide_only) continue;
            if (primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE && primitive->has_dimensions) {
                double half_w = fmax(0.05, fabs(primitive->width) * 0.5);
                double half_h = fmax(0.05, fabs(primitive->height) * 0.5);
                scene_editor_digest_overlay_draw_dashed_line3(renderer,
                                                              projector,
                                                              primitive->origin_x - half_w, primitive->origin_y - half_h, primitive->origin_z,
                                                              primitive->origin_x + half_w, primitive->origin_y - half_h, primitive->origin_z,
                                                              primitive_color);
                scene_editor_digest_overlay_draw_dashed_line3(renderer,
                                                              projector,
                                                              primitive->origin_x + half_w, primitive->origin_y - half_h, primitive->origin_z,
                                                              primitive->origin_x + half_w, primitive->origin_y + half_h, primitive->origin_z,
                                                              primitive_color);
                scene_editor_digest_overlay_draw_dashed_line3(renderer,
                                                              projector,
                                                              primitive->origin_x + half_w, primitive->origin_y + half_h, primitive->origin_z,
                                                              primitive->origin_x - half_w, primitive->origin_y + half_h, primitive->origin_z,
                                                              primitive_color);
                scene_editor_digest_overlay_draw_dashed_line3(renderer,
                                                              projector,
                                                              primitive->origin_x - half_w, primitive->origin_y + half_h, primitive->origin_z,
                                                              primitive->origin_x - half_w, primitive->origin_y - half_h, primitive->origin_z,
                                                              primitive_color);
            } else {
                SceneEditorDigestOverlayDrawPrismGuide(renderer,
                                                       projector,
                                                       primitive,
                                                       primitive_color);
            }
            if (is_selected || is_hover) {
                SceneEditorDigestOverlayDrawSelectionMarker(renderer, projector, primitive, highlight_color);
            }
        }
        return;
    }

    for (i = 0; i < digest->primitive_count; ++i) {
        const RuntimeSceneBridgePrimitiveDigest* primitive = &digest->primitives[i];
        SDL_Color primitive_color =
            SceneEditorDigestOverlayResolvePrimitiveColor(primitive->scene_object_index);
        bool is_selected = ((active_mode == EDITOR_MODE_OBJECT ||
                             active_mode == EDITOR_MODE_MATERIAL) &&
                            selected_object_index == primitive->scene_object_index);
        bool is_hover = (active_mode == EDITOR_MODE_OBJECT &&
                         hover_object_index == primitive->scene_object_index);
        SDL_Color highlight_color = is_selected
                                        ? (SDL_Color){255, 120, 70, 255}
                                        : (SDL_Color){84, 224, 255, 245};
        primitive_color.a = is_selected ? 210u : (is_hover ? 160u : 88u);
        if (material_focus_mode && !is_selected) continue;
        if (primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE && primitive->has_dimensions) {
            double half_w = fmax(0.05, fabs(primitive->width) * 0.5);
            double half_h = fmax(0.05, fabs(primitive->height) * 0.5);
            if (primitive->guide_only) {
                scene_editor_digest_overlay_draw_dashed_line3(renderer,
                                                              projector,
                                                              primitive->origin_x - half_w, primitive->origin_y - half_h, primitive->origin_z,
                                                              primitive->origin_x + half_w, primitive->origin_y - half_h, primitive->origin_z,
                                                              primitive_color);
                scene_editor_digest_overlay_draw_dashed_line3(renderer,
                                                              projector,
                                                              primitive->origin_x + half_w, primitive->origin_y - half_h, primitive->origin_z,
                                                              primitive->origin_x + half_w, primitive->origin_y + half_h, primitive->origin_z,
                                                              primitive_color);
                scene_editor_digest_overlay_draw_dashed_line3(renderer,
                                                              projector,
                                                              primitive->origin_x + half_w, primitive->origin_y + half_h, primitive->origin_z,
                                                              primitive->origin_x - half_w, primitive->origin_y + half_h, primitive->origin_z,
                                                              primitive_color);
                scene_editor_digest_overlay_draw_dashed_line3(renderer,
                                                              projector,
                                                              primitive->origin_x - half_w, primitive->origin_y + half_h, primitive->origin_z,
                                                              primitive->origin_x - half_w, primitive->origin_y - half_h, primitive->origin_z,
                                                              primitive_color);
            } else {
                SceneEditorDigestOverlayDrawLine3(renderer,
                                                  projector,
                                                  primitive->origin_x - half_w, primitive->origin_y - half_h, primitive->origin_z,
                                                  primitive->origin_x + half_w, primitive->origin_y - half_h, primitive->origin_z,
                                                  primitive_color);
                SceneEditorDigestOverlayDrawLine3(renderer,
                                                  projector,
                                                  primitive->origin_x + half_w, primitive->origin_y - half_h, primitive->origin_z,
                                                  primitive->origin_x + half_w, primitive->origin_y + half_h, primitive->origin_z,
                                                  primitive_color);
                SceneEditorDigestOverlayDrawLine3(renderer,
                                                  projector,
                                                  primitive->origin_x + half_w, primitive->origin_y + half_h, primitive->origin_z,
                                                  primitive->origin_x - half_w, primitive->origin_y + half_h, primitive->origin_z,
                                                  primitive_color);
                SceneEditorDigestOverlayDrawLine3(renderer,
                                                  projector,
                                                  primitive->origin_x - half_w, primitive->origin_y + half_h, primitive->origin_z,
                                                  primitive->origin_x - half_w, primitive->origin_y - half_h, primitive->origin_z,
                                                  primitive_color);
            }
            if (is_selected || is_hover) {
                SceneEditorDigestOverlayDrawSelectionMarker(renderer, projector, primitive, highlight_color);
            }
        } else {
            if (primitive->guide_only) {
                SceneEditorDigestOverlayDrawPrismGuide(renderer,
                                                       projector,
                                                       primitive,
                                                       primitive_color);
            } else {
                SceneEditorDigestOverlayDrawPrism(renderer,
                                                  projector,
                                                  primitive,
                                                  primitive_color);
            }
            if (is_selected || is_hover) {
                SceneEditorDigestOverlayDrawSelectionMarker(renderer, projector, primitive, highlight_color);
            }
        }
    }
}
