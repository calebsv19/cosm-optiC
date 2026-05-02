#include "app/preview_retained_scene_renderer.h"

#include <math.h>
#include <string.h>

#include "config/config_manager.h"

static SDL_Color preview_retained_scene_primitive_color(int primitive_index) {
    static const SDL_Color k_palette[] = {
        {240, 240, 240, 255},
        {255, 208, 120, 255},
        {120, 198, 255, 255},
        {144, 232, 180, 255}
    };
    const int palette_count = (int)(sizeof(k_palette) / sizeof(k_palette[0]));
    int index = primitive_index;
    if (index >= 0 && index < sceneSettings.objectCount) {
        SceneObject* obj = &sceneSettings.sceneObjects[index];
        return (SDL_Color){
            SceneObjectColorR(obj),
            SceneObjectColorG(obj),
            SceneObjectColorB(obj),
            SceneObjectAlphaByte(obj)
        };
    }
    if (index < 0) index = 0;
    return k_palette[index % palette_count];
}

static void preview_retained_scene_accumulate_extents(double x,
                                                      double y,
                                                      double z,
                                                      bool* seeded,
                                                      double* min_x,
                                                      double* min_y,
                                                      double* min_z,
                                                      double* max_x,
                                                      double* max_y,
                                                      double* max_z) {
    if (!seeded || !min_x || !min_y || !min_z || !max_x || !max_y || !max_z) return;
    if (!*seeded) {
        *min_x = *max_x = x;
        *min_y = *max_y = y;
        *min_z = *max_z = z;
        *seeded = true;
        return;
    }
    if (x < *min_x) *min_x = x;
    if (x > *max_x) *max_x = x;
    if (y < *min_y) *min_y = y;
    if (y > *max_y) *max_y = y;
    if (z < *min_z) *min_z = z;
    if (z > *max_z) *max_z = z;
}

static void preview_retained_scene_append_segment(
    PreviewRetainedSceneLineSegment* segments,
    int max_segments,
    int* io_count,
    double ax,
    double ay,
    double az,
    double bx,
    double by,
    double bz,
    SDL_Color color) {
    PreviewRetainedSceneLineSegment* segment = NULL;
    if (!segments || !io_count || *io_count < 0) return;
    if (*io_count >= max_segments) return;
    segment = &segments[*io_count];
    segment->ax = ax;
    segment->ay = ay;
    segment->az = az;
    segment->bx = bx;
    segment->by = by;
    segment->bz = bz;
    segment->color = color;
    *io_count += 1;
}

static void preview_retained_scene_append_box(
    PreviewRetainedSceneLineSegment* segments,
    int max_segments,
    int* io_count,
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
        preview_retained_scene_append_segment(segments,
                                              max_segments,
                                              io_count,
                                              corners[a][0],
                                              corners[a][1],
                                              corners[a][2],
                                              corners[b][0],
                                              corners[b][1],
                                              corners[b][2],
                                              color);
    }
}

static void preview_retained_scene_append_plane_seed(
    PreviewRetainedSceneLineSegment* segments,
    int max_segments,
    int* io_count,
    const RuntimeSceneBridgePrimitiveSeed* primitive,
    SDL_Color color) {
    double half_w = 0.0;
    double half_h = 0.0;
    double corners[4][3] = {{0.0}};
    if (!primitive || !primitive->has_dimensions) return;
    half_w = fmax(0.05, fabs(primitive->width) * 0.5);
    half_h = fmax(0.05, fabs(primitive->height) * 0.5);

    corners[0][0] = primitive->origin_x - primitive->axis_u_x * half_w - primitive->axis_v_x * half_h;
    corners[0][1] = primitive->origin_y - primitive->axis_u_y * half_w - primitive->axis_v_y * half_h;
    corners[0][2] = primitive->origin_z - primitive->axis_u_z * half_w - primitive->axis_v_z * half_h;
    corners[1][0] = primitive->origin_x + primitive->axis_u_x * half_w - primitive->axis_v_x * half_h;
    corners[1][1] = primitive->origin_y + primitive->axis_u_y * half_w - primitive->axis_v_y * half_h;
    corners[1][2] = primitive->origin_z + primitive->axis_u_z * half_w - primitive->axis_v_z * half_h;
    corners[2][0] = primitive->origin_x + primitive->axis_u_x * half_w + primitive->axis_v_x * half_h;
    corners[2][1] = primitive->origin_y + primitive->axis_u_y * half_w + primitive->axis_v_y * half_h;
    corners[2][2] = primitive->origin_z + primitive->axis_u_z * half_w + primitive->axis_v_z * half_h;
    corners[3][0] = primitive->origin_x - primitive->axis_u_x * half_w + primitive->axis_v_x * half_h;
    corners[3][1] = primitive->origin_y - primitive->axis_u_y * half_w + primitive->axis_v_y * half_h;
    corners[3][2] = primitive->origin_z - primitive->axis_u_z * half_w + primitive->axis_v_z * half_h;

    preview_retained_scene_append_segment(segments, max_segments, io_count,
                                          corners[0][0], corners[0][1], corners[0][2],
                                          corners[1][0], corners[1][1], corners[1][2],
                                          color);
    preview_retained_scene_append_segment(segments, max_segments, io_count,
                                          corners[1][0], corners[1][1], corners[1][2],
                                          corners[2][0], corners[2][1], corners[2][2],
                                          color);
    preview_retained_scene_append_segment(segments, max_segments, io_count,
                                          corners[2][0], corners[2][1], corners[2][2],
                                          corners[3][0], corners[3][1], corners[3][2],
                                          color);
    preview_retained_scene_append_segment(segments, max_segments, io_count,
                                          corners[3][0], corners[3][1], corners[3][2],
                                          corners[0][0], corners[0][1], corners[0][2],
                                          color);
}

static void preview_retained_scene_append_prism_seed(
    PreviewRetainedSceneLineSegment* segments,
    int max_segments,
    int* io_count,
    const RuntimeSceneBridgePrimitiveSeed* primitive,
    SDL_Color color) {
    static const int k_edges[12][2] = {
        {0, 1}, {1, 3}, {3, 2}, {2, 0},
        {4, 5}, {5, 7}, {7, 6}, {6, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };
    double half_w = 0.0;
    double half_h = 0.0;
    double half_d = 0.0;
    double corners[8][3] = {{0.0}};
    int corner = 0;
    int sx = 0;
    int sy = 0;
    int sz = 0;
    int i = 0;
    if (!primitive || !primitive->has_dimensions) return;
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
    for (i = 0; i < 12; ++i) {
        int a = k_edges[i][0];
        int b = k_edges[i][1];
        preview_retained_scene_append_segment(segments, max_segments, io_count,
                                              corners[a][0], corners[a][1], corners[a][2],
                                              corners[b][0], corners[b][1], corners[b][2],
                                              color);
    }
}

static bool preview_retained_scene_resolve_seed_extents(
    const RuntimeSceneBridge3DPrimitiveSeedState* seeds,
    double* out_min_x,
    double* out_min_y,
    double* out_min_z,
    double* out_max_x,
    double* out_max_y,
    double* out_max_z) {
    bool seeded = false;
    int i = 0;
    if (!seeds || !seeds->valid || seeds->primitive_count <= 0) return false;
    for (i = 0; i < seeds->primitive_count; ++i) {
        const RuntimeSceneBridgePrimitiveSeed* primitive = &seeds->primitives[i];
        if (primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE && primitive->has_dimensions) {
            double half_w = fabs(primitive->width) * 0.5;
            double half_h = fabs(primitive->height) * 0.5;
            int sx = 0;
            int sy = 0;
            for (sx = -1; sx <= 1; sx += 2) {
                for (sy = -1; sy <= 1; sy += 2) {
                    preview_retained_scene_accumulate_extents(
                        primitive->origin_x + primitive->axis_u_x * half_w * (double)sx +
                            primitive->axis_v_x * half_h * (double)sy,
                        primitive->origin_y + primitive->axis_u_y * half_w * (double)sx +
                            primitive->axis_v_y * half_h * (double)sy,
                        primitive->origin_z + primitive->axis_u_z * half_w * (double)sx +
                            primitive->axis_v_z * half_h * (double)sy,
                        &seeded,
                        out_min_x, out_min_y, out_min_z,
                        out_max_x, out_max_y, out_max_z);
                }
            }
        } else if ((primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM ||
                    primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_BOX) &&
                   primitive->has_dimensions) {
            double half_w = fabs(primitive->width) * 0.5;
            double half_h = fabs(primitive->height) * 0.5;
            double half_d = fabs(primitive->depth) * 0.5;
            int sx = 0;
            int sy = 0;
            int sz = 0;
            for (sx = -1; sx <= 1; sx += 2) {
                for (sy = -1; sy <= 1; sy += 2) {
                    for (sz = -1; sz <= 1; sz += 2) {
                        preview_retained_scene_accumulate_extents(
                            primitive->origin_x + primitive->axis_u_x * half_w * (double)sx +
                                primitive->axis_v_x * half_h * (double)sy +
                                primitive->normal_x * half_d * (double)sz,
                            primitive->origin_y + primitive->axis_u_y * half_w * (double)sx +
                                primitive->axis_v_y * half_h * (double)sy +
                                primitive->normal_y * half_d * (double)sz,
                            primitive->origin_z + primitive->axis_u_z * half_w * (double)sx +
                                primitive->axis_v_z * half_h * (double)sy +
                                primitive->normal_z * half_d * (double)sz,
                            &seeded,
                            out_min_x, out_min_y, out_min_z,
                            out_max_x, out_max_y, out_max_z);
                    }
                }
            }
        } else {
            preview_retained_scene_accumulate_extents(primitive->origin_x,
                                                      primitive->origin_y,
                                                      primitive->origin_z,
                                                      &seeded,
                                                      out_min_x, out_min_y, out_min_z,
                                                      out_max_x, out_max_y, out_max_z);
        }
    }
    return seeded;
}

static void preview_retained_scene_append_plane_rect(
    PreviewRetainedSceneLineSegment* segments,
    int max_segments,
    int* io_count,
    double min_x,
    double min_y,
    double max_x,
    double max_y,
    double z,
    SDL_Color color) {
    preview_retained_scene_append_segment(segments, max_segments, io_count,
                                          min_x, min_y, z,
                                          max_x, min_y, z,
                                          color);
    preview_retained_scene_append_segment(segments, max_segments, io_count,
                                          max_x, min_y, z,
                                          max_x, max_y, z,
                                          color);
    preview_retained_scene_append_segment(segments, max_segments, io_count,
                                          max_x, max_y, z,
                                          min_x, max_y, z,
                                          color);
    preview_retained_scene_append_segment(segments, max_segments, io_count,
                                          min_x, max_y, z,
                                          min_x, min_y, z,
                                          color);
}

static bool preview_retained_scene_project_camera_point(const PreviewCameraProjector* projector,
                                                        double camera_x,
                                                        double camera_y,
                                                        double camera_z,
                                                        int* out_screen_x,
                                                        int* out_screen_y) {
    double ndc_x = 0.0;
    double ndc_y = 0.0;
    double sx = 0.0;
    double sy = 0.0;
    if (out_screen_x) *out_screen_x = 0;
    if (out_screen_y) *out_screen_y = 0;
    if (!projector) return false;
    if (camera_z <= projector->near_plane) return false;

    ndc_x = camera_x / (camera_z * projector->tan_half_fov_x);
    ndc_y = camera_y / (camera_z * projector->tan_half_fov_y);
    sx = (double)projector->viewport.x +
         (ndc_x + 1.0) * 0.5 * (double)projector->viewport.w;
    sy = (double)projector->viewport.y +
         (1.0 - (ndc_y + 1.0) * 0.5) * (double)projector->viewport.h;

    if (out_screen_x) *out_screen_x = (int)lround(sx);
    if (out_screen_y) *out_screen_y = (int)lround(sy);
    return true;
}

static bool preview_retained_scene_clip_line_to_near_plane(
    const PreviewCameraProjector* projector,
    double* io_ax,
    double* io_ay,
    double* io_az,
    double* io_bx,
    double* io_by,
    double* io_bz) {
    double da = 0.0;
    double db = 0.0;
    double t = 0.0;
    if (!projector || !io_ax || !io_ay || !io_az || !io_bx || !io_by || !io_bz) return false;

    da = *io_az;
    db = *io_bz;
    if (da <= projector->near_plane && db <= projector->near_plane) {
        return false;
    }
    if (da > projector->near_plane && db > projector->near_plane) {
        return true;
    }
    if (fabs(db - da) <= 1e-9) {
        return false;
    }

    t = (projector->near_plane - da) / (db - da);
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;

    if (da <= projector->near_plane) {
        *io_ax = *io_ax + (*io_bx - *io_ax) * t;
        *io_ay = *io_ay + (*io_by - *io_ay) * t;
        *io_az = projector->near_plane;
    } else {
        *io_bx = *io_ax + (*io_bx - *io_ax) * t;
        *io_by = *io_ay + (*io_by - *io_ay) * t;
        *io_bz = projector->near_plane;
    }
    return true;
}

static void preview_retained_scene_draw_segment(SDL_Renderer* renderer,
                                                const PreviewCameraProjector* projector,
                                                const PreviewRetainedSceneLineSegment* segment) {
    double ax = 0.0;
    double ay = 0.0;
    double az = 0.0;
    double bx = 0.0;
    double by = 0.0;
    double bz = 0.0;
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;

    if (!renderer || !projector || !segment) return;

    PreviewCameraProjectorWorldToCamera(projector,
                                        segment->ax,
                                        segment->ay,
                                        segment->az,
                                        &ax,
                                        &ay,
                                        &az);
    PreviewCameraProjectorWorldToCamera(projector,
                                        segment->bx,
                                        segment->by,
                                        segment->bz,
                                        &bx,
                                        &by,
                                        &bz);
    if (!preview_retained_scene_clip_line_to_near_plane(projector,
                                                        &ax,
                                                        &ay,
                                                        &az,
                                                        &bx,
                                                        &by,
                                                        &bz)) {
        return;
    }
    if (!preview_retained_scene_project_camera_point(projector, ax, ay, az, &x0, &y0)) return;
    if (!preview_retained_scene_project_camera_point(projector, bx, by, bz, &x1, &y1)) return;

    SDL_SetRenderDrawColor(renderer,
                           segment->color.r,
                           segment->color.g,
                           segment->color.b,
                           segment->color.a);
    SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
}

static void preview_retained_scene_draw_filled_circle(SDL_Renderer* renderer,
                                                      int center_x,
                                                      int center_y,
                                                      int radius,
                                                      SDL_Color color) {
    if (!renderer || radius <= 0) return;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int dy = -radius; dy <= radius; ++dy) {
        int span = (int)floor(sqrt((double)(radius * radius - dy * dy)));
        SDL_RenderDrawLine(renderer,
                           center_x - span,
                           center_y + dy,
                           center_x + span,
                           center_y + dy);
    }
}

int PreviewRetainedSceneBuildLineSegments(const RuntimeSceneBridge3DDigestState* digest,
                                          PreviewRetainedSceneLineSegment* out_segments,
                                          int max_segments) {
    RuntimeSceneBridge3DPrimitiveSeedState seeds = {0};
    int count = 0;
    int i = 0;
    double seed_min_x = 0.0;
    double seed_min_y = 0.0;
    double seed_min_z = 0.0;
    double seed_max_x = 0.0;
    double seed_max_y = 0.0;
    double seed_max_z = 0.0;
    bool has_seed_extents = false;

    if (out_segments && max_segments > 0) {
        memset(out_segments, 0, sizeof(*out_segments) * (size_t)max_segments);
    }
    if (!digest || !digest->valid || !out_segments || max_segments <= 0) return 0;
    runtime_scene_bridge_get_last_3d_primitive_seed_state(&seeds);
    has_seed_extents = preview_retained_scene_resolve_seed_extents(&seeds,
                                                                   &seed_min_x,
                                                                   &seed_min_y,
                                                                   &seed_min_z,
                                                                   &seed_max_x,
                                                                   &seed_max_y,
                                                                   &seed_max_z);

    if (digest->has_scene_bounds) {
        SDL_Color bounds_color = digest->bounds_enabled
                                     ? (SDL_Color){90, 130, 190, 220}
                                     : (SDL_Color){70, 84, 102, 170};
        preview_retained_scene_append_box(out_segments,
                                          max_segments,
                                          &count,
                                          has_seed_extents ? seed_min_x : digest->bounds_min_x,
                                          has_seed_extents ? seed_min_y : digest->bounds_min_y,
                                          has_seed_extents ? seed_min_z : digest->bounds_min_z,
                                          has_seed_extents ? seed_max_x : digest->bounds_max_x,
                                          has_seed_extents ? seed_max_y : digest->bounds_max_y,
                                          has_seed_extents ? seed_max_z : digest->bounds_max_z,
                                          bounds_color);
    }

    if (digest->has_construction_plane && (has_seed_extents || digest->has_scene_bounds)) {
        preview_retained_scene_append_plane_rect(out_segments,
                                                 max_segments,
                                                 &count,
                                                 has_seed_extents ? seed_min_x : digest->bounds_min_x,
                                                 has_seed_extents ? seed_min_y : digest->bounds_min_y,
                                                 has_seed_extents ? seed_max_x : digest->bounds_max_x,
                                                 has_seed_extents ? seed_max_y : digest->bounds_max_y,
                                                 digest->construction_plane_offset,
                                                 (SDL_Color){205, 176, 106, 220});
    }

    if (seeds.valid && seeds.primitive_count > 0) {
        for (i = 0; i < seeds.primitive_count; ++i) {
            const RuntimeSceneBridgePrimitiveSeed* primitive = &seeds.primitives[i];
            SDL_Color color = preview_retained_scene_primitive_color(primitive->scene_object_index);
            if (primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE) {
                preview_retained_scene_append_plane_seed(out_segments,
                                                         max_segments,
                                                         &count,
                                                         primitive,
                                                         color);
            } else if (primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM ||
                       primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_BOX) {
                preview_retained_scene_append_prism_seed(out_segments,
                                                         max_segments,
                                                         &count,
                                                         primitive,
                                                         color);
            }
        }
        return count;
    }

    for (i = 0; i < digest->primitive_count; ++i) {
        const RuntimeSceneBridgePrimitiveDigest* primitive = &digest->primitives[i];
        SDL_Color color = preview_retained_scene_primitive_color(i);
        if (primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE && primitive->has_dimensions) {
            double half_w = fmax(0.05, fabs(primitive->width) * 0.5);
            double half_h = fmax(0.05, fabs(primitive->height) * 0.5);
            preview_retained_scene_append_plane_rect(out_segments,
                                                     max_segments,
                                                     &count,
                                                     primitive->origin_x - half_w,
                                                     primitive->origin_y - half_h,
                                                     primitive->origin_x + half_w,
                                                     primitive->origin_y + half_h,
                                                     primitive->origin_z,
                                                     color);
        } else if (primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM ||
                   primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_BOX) {
            double half_w = fmax(0.05, fabs(primitive->width) * 0.5);
            double half_h = fmax(0.05, fabs(primitive->height) * 0.5);
            double half_d = fmax(0.05, fabs(primitive->depth) * 0.5);
            preview_retained_scene_append_box(out_segments,
                                              max_segments,
                                              &count,
                                              primitive->origin_x - half_w,
                                              primitive->origin_y - half_h,
                                              primitive->origin_z - half_d,
                                              primitive->origin_x + half_w,
                                              primitive->origin_y + half_h,
                                              primitive->origin_z + half_d,
                                              color);
        }
    }

    return count;
}

void PreviewRetainedSceneRender(SDL_Renderer* renderer,
                                const RuntimeSceneBridge3DDigestState* digest,
                                const PreviewCameraProjector* projector) {
    PreviewRetainedSceneLineSegment segments[PREVIEW_RETAINED_SCENE_MAX_LINE_SEGMENTS];
    int segment_count = 0;
    int i = 0;

    if (!renderer || !digest || !projector) return;
    segment_count = PreviewRetainedSceneBuildLineSegments(digest,
                                                          segments,
                                                          PREVIEW_RETAINED_SCENE_MAX_LINE_SEGMENTS);
    for (i = 0; i < segment_count; ++i) {
        preview_retained_scene_draw_segment(renderer, projector, &segments[i]);
    }
}

void PreviewRetainedSceneRenderLightMarker(SDL_Renderer* renderer,
                                           const PreviewCameraProjector* projector,
                                           double world_x,
                                           double world_y,
                                           double world_z,
                                           SDL_Color color,
                                           int radius_pixels) {
    double screen_x = 0.0;
    double screen_y = 0.0;
    double depth = 0.0;
    bool inside = false;
    int resolved_radius = radius_pixels;

    if (!renderer || !projector) return;
    if (resolved_radius <= 0) resolved_radius = 6;
    if (!PreviewCameraProjectorProjectPoint(projector,
                                            world_x,
                                            world_y,
                                            world_z,
                                            &screen_x,
                                            &screen_y,
                                            &depth,
                                            &inside)) {
        return;
    }
    (void)depth;
    if (!inside) return;

    preview_retained_scene_draw_filled_circle(renderer,
                                              (int)lround(screen_x),
                                              (int)lround(screen_y),
                                              resolved_radius + 2,
                                              (SDL_Color){255, 255, 255, 72});
    preview_retained_scene_draw_filled_circle(renderer,
                                              (int)lround(screen_x),
                                              (int)lround(screen_y),
                                              resolved_radius,
                                              color);
}
