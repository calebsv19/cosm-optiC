#include "editor/scene_editor_digest_overlay_internal.h"

#include <math.h>
#include <string.h>

#include "editor/object_editor_motion.h"

#define SCENE_EDITOR_MOTION_OVERLAY_SAMPLE_STEPS 48

static bool scene_editor_motion_overlay_track_visible(const RuntimeMotionTrack3D* track) {
    if (!track || !track->used || !track->enabled) return false;
    if (track->mode != RUNTIME_MOTION_TRACK_3D_MODE_AUTHORED_PATH) return false;
    return track->has_position_path && track->position_path.numPoints > 0;
}

static SDL_Rect scene_editor_motion_overlay_marker_rect(int x, int y, int radius) {
    return (SDL_Rect){x - radius, y - radius, radius * 2, radius * 2};
}

bool SceneEditorDigestOverlayResolveMotionTrackMetrics(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeMotionTrack3D* track,
    SceneEditorMotionOverlayMetrics* out_metrics) {
    SceneEditorMotionOverlayMetrics metrics = {0};
    int center_x = 0;
    int center_y = 0;
    int previous_x = 0;
    int previous_y = 0;
    bool have_previous = false;
    if (out_metrics) {
        *out_metrics = metrics;
    }
    if (!projector || !scene_editor_motion_overlay_track_visible(track)) {
        return false;
    }

    metrics.control_point_count = track->position_path.numPoints;
    metrics.has_path_curve = track->position_path.numPoints >= 2;
    if (SceneEditorDigestOverlayProjectPoint(projector,
                                             track->position_path.points[0].x,
                                             track->position_path.points[0].y,
                                             track->position_path_3d.point_z[0],
                                             &center_x,
                                             &center_y)) {
        metrics.visible = true;
        metrics.projected_control_point_count += 1;
        metrics.center_marker_bounds =
            scene_editor_motion_overlay_marker_rect(center_x, center_y, 7);
    }

    for (int i = 1; i < track->position_path.numPoints && i < MAX_BEZIER_POINTS; ++i) {
        int px = 0;
        int py = 0;
        if (SceneEditorDigestOverlayProjectPoint(projector,
                                                 track->position_path.points[i].x,
                                                 track->position_path.points[i].y,
                                                 track->position_path_3d.point_z[i],
                                                 &px,
                                                 &py)) {
            metrics.visible = true;
            metrics.projected_control_point_count += 1;
        }
    }

    if (metrics.has_path_curve) {
        for (int i = 0; i <= SCENE_EDITOR_MOTION_OVERLAY_SAMPLE_STEPS; ++i) {
            double t = (double)i / (double)SCENE_EDITOR_MOTION_OVERLAY_SAMPLE_STEPS;
            Point p = GetPositionAlongPathNormalized((Path*)&track->position_path, t);
            double z = CameraPath3D_GetPositionZNormalized(&track->position_path,
                                                           &track->position_path_3d,
                                                           t);
            int sx = 0;
            int sy = 0;
            if (!SceneEditorDigestOverlayProjectPoint(projector, p.x, p.y, z, &sx, &sy)) {
                have_previous = false;
                continue;
            }
            if (have_previous && (sx != previous_x || sy != previous_y)) {
                metrics.sampled_segment_count += 1;
                metrics.visible = true;
            }
            previous_x = sx;
            previous_y = sy;
            have_previous = true;
        }
    }

    if (out_metrics) {
        *out_metrics = metrics;
    }
    return metrics.visible;
}

static void scene_editor_motion_overlay_draw_marker(SDL_Renderer* renderer,
                                                    int x,
                                                    int y,
                                                    int radius,
                                                    SDL_Color fill,
                                                    SDL_Color outline) {
    SDL_Rect outer = scene_editor_motion_overlay_marker_rect(x, y, radius);
    SDL_Rect inner = scene_editor_motion_overlay_marker_rect(x, y, radius - 3);
    if (!renderer || radius < 4) return;
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, &outer);
    SDL_SetRenderDrawColor(renderer, outline.r, outline.g, outline.b, outline.a);
    SDL_RenderDrawRect(renderer, &outer);
    SDL_RenderDrawRect(renderer, &inner);
}

static void scene_editor_motion_overlay_draw_track(SDL_Renderer* renderer,
                                                   const SceneEditorDigestOverlayProjector* projector,
                                                   const RuntimeMotionTrack3D* track) {
    SDL_Color path_color = {255, 205, 88, 232};
    SDL_Color handle_color = {255, 236, 156, 245};
    SDL_Color center_color = {255, 166, 68, 250};
    SDL_Color outline_color = {22, 25, 31, 255};
    int previous_x = 0;
    int previous_y = 0;
    bool have_previous = false;
    if (!renderer || !projector || !scene_editor_motion_overlay_track_visible(track)) return;

    if (track->position_path.numPoints >= 2) {
        SDL_SetRenderDrawColor(renderer,
                               path_color.r,
                               path_color.g,
                               path_color.b,
                               path_color.a);
        for (int i = 0; i <= SCENE_EDITOR_MOTION_OVERLAY_SAMPLE_STEPS; ++i) {
            double t = (double)i / (double)SCENE_EDITOR_MOTION_OVERLAY_SAMPLE_STEPS;
            Point p = GetPositionAlongPathNormalized((Path*)&track->position_path, t);
            double z = CameraPath3D_GetPositionZNormalized(&track->position_path,
                                                           &track->position_path_3d,
                                                           t);
            int sx = 0;
            int sy = 0;
            if (!SceneEditorDigestOverlayProjectPoint(projector, p.x, p.y, z, &sx, &sy)) {
                have_previous = false;
                continue;
            }
            if (have_previous) {
                SDL_RenderDrawLine(renderer, previous_x, previous_y, sx, sy);
            }
            previous_x = sx;
            previous_y = sy;
            have_previous = true;
        }
    }

    for (int i = 0; i < track->position_path.numPoints && i < MAX_BEZIER_POINTS; ++i) {
        int px = 0;
        int py = 0;
        SDL_Color fill = (i == 0) ? center_color : handle_color;
        int radius = (i == 0) ? 7 : 5;
        if (!SceneEditorDigestOverlayProjectPoint(projector,
                                                  track->position_path.points[i].x,
                                                  track->position_path.points[i].y,
                                                  track->position_path_3d.point_z[i],
                                                  &px,
                                                  &py)) {
            continue;
        }
        scene_editor_motion_overlay_draw_marker(renderer, px, py, radius, fill, outline_color);
    }
}

void SceneEditorDigestOverlayRenderMotionLayer(SDL_Renderer* renderer,
                                               const SceneEditorDigestOverlayProjector* projector,
                                               int selected_object_index) {
    char object_id[RUNTIME_MOTION_TRACK_3D_OBJECT_ID_SIZE];
    const RuntimeMotionTrack3D* track = NULL;
    if (!renderer || !projector || selected_object_index < 0) return;
    if (!ObjectEditorMotionObjectIdForSceneIndex(selected_object_index,
                                                 object_id,
                                                 sizeof(object_id))) {
        return;
    }
    track = ObjectEditorMotionFindTrack(object_id);
    scene_editor_motion_overlay_draw_track(renderer, projector, track);
}
