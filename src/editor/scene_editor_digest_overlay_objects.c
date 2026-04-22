#include "editor/scene_editor_digest_overlay_internal.h"

#include <math.h>

#include "config/config_manager.h"
#include "scene/object_manager.h"

static bool scene_editor_digest_overlay_objects_point_in_rect(int x, int y, const SDL_Rect* rect) {
    if (!rect) return false;
    return x >= rect->x && y >= rect->y && x < rect->x + rect->w && y < rect->y + rect->h;
}

static bool SceneEditorDigestOverlayPrimitiveScreenRect(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridgePrimitiveDigest* primitive,
    SDL_Rect* out_rect,
    SDL_Point* out_center) {
    double corners[8][3] = {{0.0}};
    int corner_count = 0;
    int min_x = 0;
    int min_y = 0;
    int max_x = 0;
    int max_y = 0;
    int center_x = 0;
    int center_y = 0;
    int i = 0;
    bool seeded = false;

    if (!projector || !primitive || !out_rect || !out_center) return false;

    if (!primitive->has_dimensions) {
        if (!SceneEditorDigestOverlayProjectPoint(projector,
                                                  primitive->origin_x,
                                                  primitive->origin_y,
                                                  primitive->origin_z,
                                                  &center_x,
                                                  &center_y)) {
            return false;
        }
        out_rect->x = center_x - 14;
        out_rect->y = center_y - 14;
        out_rect->w = 28;
        out_rect->h = 28;
        out_center->x = center_x;
        out_center->y = center_y;
        return true;
    }

    if (primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE) {
        double half_w = fmax(0.05, fabs(primitive->width) * 0.5);
        double half_h = fmax(0.05, fabs(primitive->height) * 0.5);
        corners[0][0] = primitive->origin_x - half_w; corners[0][1] = primitive->origin_y - half_h; corners[0][2] = primitive->origin_z;
        corners[1][0] = primitive->origin_x + half_w; corners[1][1] = primitive->origin_y - half_h; corners[1][2] = primitive->origin_z;
        corners[2][0] = primitive->origin_x + half_w; corners[2][1] = primitive->origin_y + half_h; corners[2][2] = primitive->origin_z;
        corners[3][0] = primitive->origin_x - half_w; corners[3][1] = primitive->origin_y + half_h; corners[3][2] = primitive->origin_z;
        corner_count = 4;
    } else if (primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM ||
               primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_BOX) {
        double half_w = fmax(0.05, fabs(primitive->width) * 0.5);
        double half_h = fmax(0.05, fabs(primitive->height) * 0.5);
        double half_d = fmax(0.05, fabs(primitive->depth) * 0.5);
        corners[0][0] = primitive->origin_x - half_w; corners[0][1] = primitive->origin_y - half_h; corners[0][2] = primitive->origin_z - half_d;
        corners[1][0] = primitive->origin_x + half_w; corners[1][1] = primitive->origin_y - half_h; corners[1][2] = primitive->origin_z - half_d;
        corners[2][0] = primitive->origin_x + half_w; corners[2][1] = primitive->origin_y + half_h; corners[2][2] = primitive->origin_z - half_d;
        corners[3][0] = primitive->origin_x - half_w; corners[3][1] = primitive->origin_y + half_h; corners[3][2] = primitive->origin_z - half_d;
        corners[4][0] = primitive->origin_x - half_w; corners[4][1] = primitive->origin_y - half_h; corners[4][2] = primitive->origin_z + half_d;
        corners[5][0] = primitive->origin_x + half_w; corners[5][1] = primitive->origin_y - half_h; corners[5][2] = primitive->origin_z + half_d;
        corners[6][0] = primitive->origin_x + half_w; corners[6][1] = primitive->origin_y + half_h; corners[6][2] = primitive->origin_z + half_d;
        corners[7][0] = primitive->origin_x - half_w; corners[7][1] = primitive->origin_y + half_h; corners[7][2] = primitive->origin_z + half_d;
        corner_count = 8;
    } else {
        return false;
    }

    if (!SceneEditorDigestOverlayProjectPoint(projector,
                                              primitive->origin_x,
                                              primitive->origin_y,
                                              primitive->origin_z,
                                              &center_x,
                                              &center_y)) {
        center_x = projector->viewport.x + projector->viewport.w / 2;
        center_y = projector->viewport.y + projector->viewport.h / 2;
    }
    for (i = 0; i < corner_count; ++i) {
        int sx = 0;
        int sy = 0;
        if (!SceneEditorDigestOverlayProjectPoint(projector,
                                                  corners[i][0],
                                                  corners[i][1],
                                                  corners[i][2],
                                                  &sx,
                                                  &sy)) {
            continue;
        }
        if (!seeded) {
            min_x = max_x = sx;
            min_y = max_y = sy;
            seeded = true;
        } else {
            if (sx < min_x) min_x = sx;
            if (sx > max_x) max_x = sx;
            if (sy < min_y) min_y = sy;
            if (sy > max_y) max_y = sy;
        }
    }
    if (!seeded) return false;

    out_rect->x = min_x;
    out_rect->y = min_y;
    out_rect->w = (max_x - min_x) + 1;
    out_rect->h = (max_y - min_y) + 1;
    out_center->x = center_x;
    out_center->y = center_y;
    return true;
}

int SceneEditorDigestOverlayPickObjectIndex(const SceneEditorDigestOverlayProjector* projector,
                                            const RuntimeSceneBridge3DDigestState* digest,
                                            int mx,
                                            int my) {
    int pick_index = -1;
    double pick_area = 0.0;
    double fallback_dist2 = 0.0;
    int i = 0;

    if (!projector || !digest) return -1;

    for (i = 0; i < digest->primitive_count && i < sceneSettings.objectCount; ++i) {
        SDL_Rect rect = {0, 0, 0, 0};
        SDL_Point center = {0, 0};
        SDL_Rect expanded = {0, 0, 0, 0};
        double area = 0.0;
        double dx = 0.0;
        double dy = 0.0;
        double dist2 = 0.0;
        const int pad = 6;

        if (!SceneEditorDigestOverlayPrimitiveScreenRect(projector, &digest->primitives[i], &rect, &center)) {
            continue;
        }

        expanded.x = rect.x - pad;
        expanded.y = rect.y - pad;
        expanded.w = rect.w + pad * 2;
        expanded.h = rect.h + pad * 2;
        area = (double)expanded.w * (double)expanded.h;
        if (scene_editor_digest_overlay_objects_point_in_rect(mx, my, &expanded)) {
            if (pick_index < 0 || area < pick_area) {
                pick_index = i;
                pick_area = area;
            }
            continue;
        }

        dx = (double)mx - (double)center.x;
        dy = (double)my - (double)center.y;
        dist2 = dx * dx + dy * dy;
        if (dist2 <= 26.0 * 26.0) {
            if (pick_index < 0 || dist2 < fallback_dist2) {
                pick_index = i;
                fallback_dist2 = dist2;
            }
        }
    }

    return pick_index;
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
    if (!digest->has_scene_bounds || !digest->has_construction_plane) return;
    min_x = digest->bounds_min_x;
    max_x = digest->bounds_max_x;
    min_y = digest->bounds_min_y;
    max_y = digest->bounds_max_y;
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

void SceneEditorDigestOverlayRenderObjectLayer(SDL_Renderer* renderer,
                                               const SceneEditorDigestOverlayProjector* projector,
                                               const RuntimeSceneBridge3DDigestState* digest,
                                               int active_mode,
                                               int selected_object_index,
                                               int hover_object_index) {
    int i = 0;
    if (!renderer || !projector || !digest) return;

    if (digest->has_scene_bounds) {
        SDL_Color bounds_color = digest->bounds_enabled
                                     ? (SDL_Color){90, 130, 190, 220}
                                     : (SDL_Color){70, 84, 102, 170};
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

    SceneEditorDigestOverlayDrawConstructionPlane(renderer,
                                                  projector,
                                                  digest,
                                                  (SDL_Color){205, 176, 106, 220});

    for (i = 0; i < digest->primitive_count; ++i) {
        const RuntimeSceneBridgePrimitiveDigest* primitive = &digest->primitives[i];
        SDL_Color primitive_color = SceneEditorDigestOverlayResolvePrimitiveColor(i);
        bool is_selected = (active_mode == 1 && selected_object_index == i);
        bool is_hover = (active_mode == 1 && hover_object_index == i);
        SDL_Color highlight_color = is_selected
                                        ? (SDL_Color){255, 120, 70, 255}
                                        : (SDL_Color){84, 224, 255, 245};
        if (primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE && primitive->has_dimensions) {
            double half_w = fmax(0.05, fabs(primitive->width) * 0.5);
            double half_h = fmax(0.05, fabs(primitive->height) * 0.5);
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
            if (is_selected || is_hover) {
                SceneEditorDigestOverlayDrawSelectionMarker(renderer, projector, primitive, highlight_color);
            }
        } else {
            SceneEditorDigestOverlayDrawPrism(renderer,
                                              projector,
                                              primitive,
                                              primitive_color);
            if (is_selected || is_hover) {
                SceneEditorDigestOverlayDrawSelectionMarker(renderer, projector, primitive, highlight_color);
            }
        }
    }
}
