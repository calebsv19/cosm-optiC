#include "editor/scene_editor_digest_overlay_internal.h"

#include <math.h>

#include "config/config_manager.h"
#include "editor/editor_mode_router.h"
#include "editor/scene_editor_mesh_preview_render.h"

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

static bool scene_editor_digest_overlay_resolve_seed_screen_rect(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridgePrimitiveSeed* primitive,
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
        int sx = 0;
        int sy = 0;
        for (sx = -1; sx <= 1; sx += 2) {
            for (sy = -1; sy <= 1; sy += 2) {
                corners[corner_count][0] = primitive->origin_x +
                                           primitive->axis_u_x * half_w * (double)sx +
                                           primitive->axis_v_x * half_h * (double)sy;
                corners[corner_count][1] = primitive->origin_y +
                                           primitive->axis_u_y * half_w * (double)sx +
                                           primitive->axis_v_y * half_h * (double)sy;
                corners[corner_count][2] = primitive->origin_z +
                                           primitive->axis_u_z * half_w * (double)sx +
                                           primitive->axis_v_z * half_h * (double)sy;
                corner_count += 1;
            }
        }
    } else if (primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM ||
               primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_BOX) {
        double half_w = fmax(0.05, fabs(primitive->width) * 0.5);
        double half_h = fmax(0.05, fabs(primitive->height) * 0.5);
        double half_d = fmax(0.05, fabs(primitive->depth) * 0.5);
        int sx = 0;
        int sy = 0;
        int sz = 0;
        for (sx = -1; sx <= 1; sx += 2) {
            for (sy = -1; sy <= 1; sy += 2) {
                for (sz = -1; sz <= 1; sz += 2) {
                    corners[corner_count][0] = primitive->origin_x +
                                               primitive->axis_u_x * half_w * (double)sx +
                                               primitive->axis_v_x * half_h * (double)sy +
                                               primitive->normal_x * half_d * (double)sz;
                    corners[corner_count][1] = primitive->origin_y +
                                               primitive->axis_u_y * half_w * (double)sx +
                                               primitive->axis_v_y * half_h * (double)sy +
                                               primitive->normal_y * half_d * (double)sz;
                    corners[corner_count][2] = primitive->origin_z +
                                               primitive->axis_u_z * half_w * (double)sx +
                                               primitive->axis_v_z * half_h * (double)sy +
                                               primitive->normal_z * half_d * (double)sz;
                    corner_count += 1;
                }
            }
        }
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
    RuntimeSceneBridge3DPrimitiveSeedState seeds = {0};
    int pick_index = -1;
    double pick_area = 0.0;
    double fallback_dist2 = 0.0;
    int i = 0;

    if (!projector || !digest) return -1;
    pick_index = SceneEditorMeshPreviewPickObjectIndex(projector,
                                                       EDITOR_MODE_OBJECT,
                                                       -1,
                                                       mx,
                                                       my);
    if (pick_index >= 0) return pick_index;
    runtime_scene_bridge_get_last_3d_primitive_seed_state(&seeds);

    if (seeds.valid && seeds.primitive_count > 0) {
        for (i = 0; i < seeds.primitive_count; ++i) {
            const RuntimeSceneBridgePrimitiveSeed* primitive = &seeds.primitives[i];
            SDL_Rect rect = {0, 0, 0, 0};
            SDL_Point center = {0, 0};
            SDL_Rect expanded = {0, 0, 0, 0};
            double area = 0.0;
            double dx = 0.0;
            double dy = 0.0;
            double dist2 = 0.0;
            const int pad = 6;
            if (!scene_editor_digest_overlay_resolve_seed_screen_rect(projector, primitive, &rect, &center)) {
                continue;
            }
            expanded.x = rect.x - pad;
            expanded.y = rect.y - pad;
            expanded.w = rect.w + pad * 2;
            expanded.h = rect.h + pad * 2;
            area = (double)expanded.w * (double)expanded.h;
            if (scene_editor_digest_overlay_objects_point_in_rect(mx, my, &expanded)) {
                if (pick_index < 0 || area < pick_area) {
                    pick_index = primitive->scene_object_index;
                    pick_area = area;
                }
                continue;
            }
            dx = (double)mx - (double)center.x;
            dy = (double)my - (double)center.y;
            dist2 = dx * dx + dy * dy;
            if (dist2 <= 26.0 * 26.0) {
                if (pick_index < 0 || dist2 < fallback_dist2) {
                    pick_index = primitive->scene_object_index;
                    fallback_dist2 = dist2;
                }
            }
        }
        for (i = 0; i < digest->primitive_count && i < sceneSettings.objectCount; ++i) {
            const RuntimeSceneBridgePrimitiveDigest* primitive = &digest->primitives[i];
            SDL_Rect rect = {0, 0, 0, 0};
            SDL_Point center = {0, 0};
            SDL_Rect expanded = {0, 0, 0, 0};
            double area = 0.0;
            double dx = 0.0;
            double dy = 0.0;
            double dist2 = 0.0;
            const int pad = 6;
            if (!primitive->guide_only) continue;
            if (!SceneEditorDigestOverlayPrimitiveScreenRect(projector, primitive, &rect, &center)) {
                continue;
            }
            expanded.x = rect.x - pad;
            expanded.y = rect.y - pad;
            expanded.w = rect.w + pad * 2;
            expanded.h = rect.h + pad * 2;
            area = (double)expanded.w * (double)expanded.h;
            if (scene_editor_digest_overlay_objects_point_in_rect(mx, my, &expanded)) {
                if (pick_index < 0 || area < pick_area) {
                    pick_index = primitive->scene_object_index;
                    pick_area = area;
                }
                continue;
            }
            dx = (double)mx - (double)center.x;
            dy = (double)my - (double)center.y;
            dist2 = dx * dx + dy * dy;
            if (dist2 <= 26.0 * 26.0) {
                if (pick_index < 0 || dist2 < fallback_dist2) {
                    pick_index = primitive->scene_object_index;
                    fallback_dist2 = dist2;
                }
            }
        }
        return pick_index;
    }

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
