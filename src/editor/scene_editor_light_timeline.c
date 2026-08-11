#include "editor/scene_editor_light_timeline.h"

#include "animation/timeline_document.h"
#include "config/config_manager.h"
#include "editor/scene_editor_light_timeline_panel.h"
#include "import/runtime_scene_bridge.h"
#include "import/runtime_scene_light_timeline_io.h"
#include "import/runtime_scene_light_timeline_bridge.h"
#include "scene_editor_light_timeline_edit.h"
#include "scene_editor_light_timeline_curve_edit.h"
#include "scene_editor_light_timeline_evaluation.h"
#include "scene_editor_light_timeline_history.h"
#include "scene_editor_light_timeline_selection.h"
#include "scene_editor_light_timeline_tracks.h"
#include "scene_editor_light_timeline_view.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct SceneEditorLightTimelineState {
    SceneEditorLightTimelineSelection selection;
    bool target_locked;
    int hovered_light_index;
    int64_t current_frame;
    bool scrubbing;
    bool dragging_key;
    bool dragging_path_point;
    SceneEditorLightTimelineHandle dragging_handle;
    bool panning_graph;
    bool playing;
    uint64_t playback_last_ms;
    double playback_frame_fraction;
    int pan_anchor_x;
    double pan_anchor_start;
    int selected_key_index;
    int selected_path_point_index;
    int mouse_x;
    int mouse_y;
    bool pointer_over_panel;
    SceneEditorLightTimelineView view;
    SceneEditorLightTimelineEvaluation evaluation;
    SceneEditorLightTimelineLane lane;
    SceneEditorLightTimelineHistory history;
} SceneEditorLightTimelineState;

static SceneEditorLightTimelineState g_light_timeline = {.hovered_light_index = -1,
                                                         .selected_key_index = -1,
                                                         .selected_path_point_index = -1};

#define SCENE_EDITOR_LIGHT_TIMELINE_SUBFRAME_DENOMINATOR 1000000u

static TimelineSample current_sample(void) {
    TimelineSample sample = {g_light_timeline.current_frame, 0u,
                             SCENE_EDITOR_LIGHT_TIMELINE_SUBFRAME_DENOMINATOR};
    double fraction = g_light_timeline.playback_frame_fraction;
    if (!isfinite(fraction) || fraction <= 0.0) return sample;
    if (fraction >= 1.0) fraction = nextafter(1.0, 0.0);
    sample.subframe_numerator =
        (uint32_t)llround(
            fraction *
            (double)SCENE_EDITOR_LIGHT_TIMELINE_SUBFRAME_DENOMINATOR);
    if (sample.subframe_numerator >= sample.subframe_denominator) {
        sample.subframe_numerator = sample.subframe_denominator - 1u;
    }
    return sample;
}

static bool capture_sample(TimelineSample sample) {
    if (!scene_editor_light_timeline_evaluation_capture(
            &g_light_timeline.evaluation, sample)) {
        return false;
    }
    g_light_timeline.current_frame =
        g_light_timeline.evaluation.result.snapshot.frame.sample.absolute_frame;
    return true;
}

static bool capture_current_sample(void) {
    return capture_sample(current_sample());
}

static bool point_in_rect(int x, int y, const SDL_Rect* rect) {
    return rect && x >= rect->x && x < rect->x + rect->w &&
           y >= rect->y && y < rect->y + rect->h;
}

static int pick_light(const SceneEditorDigestOverlayProjector* projector,
                      int mouse_x, int mouse_y) {
    RuntimeSceneBridge3DLightSeedState lights;
    int best = -1;
    double best_distance = 14.0 * 14.0;
    runtime_scene_bridge_get_last_3d_light_seed_state(&lights);
    if (!lights.valid || !projector) return -1;
    for (int i = 0; i < lights.light_count; ++i) {
        int x = 0, y = 0;
        double position_x = 0.0;
        double position_y = 0.0;
        double position_z = 0.0;
        scene_editor_light_timeline_evaluation_light_position(
            &g_light_timeline.evaluation, &lights, i, &position_x, &position_y,
            &position_z);
        if (!lights.lights[i].id[0] ||
            !SceneEditorDigestOverlayProjectPoint(projector,
                                                  position_x,
                                                  position_y,
                                                  position_z,
                                                  &x, &y)) continue;
        double dx = (double)x - mouse_x;
        double dy = (double)y - mouse_y;
        double distance = dx * dx + dy * dy;
        if (distance <= best_distance) {
            best_distance = distance;
            best = i;
        }
    }
    return best;
}

void SceneEditorLightTimelineReset(void) {
    memset(&g_light_timeline, 0, sizeof(g_light_timeline));
    scene_editor_light_timeline_selection_reset(&g_light_timeline.selection);
    g_light_timeline.hovered_light_index = -1;
    g_light_timeline.selected_key_index = -1;
    g_light_timeline.selected_path_point_index = -1;
    scene_editor_light_timeline_view_reset(&g_light_timeline.view);
}

static bool current_document(RuntimeSceneLightTimelineDocument* out_document,
                             TimelineTrack** out_track) {
    if (!out_document || !RuntimeSceneLightTimelineGetLast(out_document) ||
        out_document->progress_track_index >= out_document->timeline.track_count) return false;
    if (out_track) *out_track = &out_document->timeline.tracks[out_document->progress_track_index];
    return true;
}

static bool current_lane_document(
    RuntimeSceneLightTimelineDocument* out_document,
    TimelineTrack** out_track) {
    TimelineTrack* track = NULL;
    if (!out_document || !RuntimeSceneLightTimelineGetLast(out_document) ||
        scene_editor_light_timeline_lane_track(
            out_document, g_light_timeline.lane, &track, NULL) !=
            TIMELINE_STATUS_OK) {
        return false;
    }
    if (out_track) *out_track = track;
    return true;
}

static TimelineStatus commit_track(RuntimeSceneLightTimelineDocument* document,
                                   const TimelineTrack* track) {
    size_t track_index = SIZE_MAX;
    if (!document || !track ||
        RuntimeSceneLightTimelineFindTrack(
            document, track->property_id, &track_index) !=
            TIMELINE_STATUS_OK) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    RuntimeSceneLightTimelineDocument candidate = *document;
    candidate.timeline.tracks[track_index] = *track;
    TimelineStatus status =
        RuntimeSceneLightTimelineValidateDocument(&candidate);
    if (status != TIMELINE_STATUS_OK) return status;
    status = RuntimeSceneLightTimelineSetLast(&candidate);
    if (status == TIMELINE_STATUS_OK) {
        (void)capture_current_sample();
    }
    return status;
}

static TimelineStatus insert_lane_key(
    SceneEditorLightTimelineLane lane,
    int64_t frame,
    double value,
    bool allow_lazy_intensity) {
    RuntimeSceneLightTimelineDocument document;
    TimelineTrack* current = NULL;
    TimelineTrack candidate;
    TimelineTrack original;
    TimelineKeyframe key;
    size_t index = 0u;
    bool created = false;
    TimelineStatus status;
    if (!isfinite(value) || value < 0.0 ||
        (lane == SCENE_EDITOR_LIGHT_TIMELINE_LANE_MOTION && value > 1.0) ||
        !RuntimeSceneLightTimelineGetLast(&document)) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    status = scene_editor_light_timeline_lane_track(
        &document, lane, &current, NULL);
    if (status == TIMELINE_STATUS_TARGET_NOT_FOUND &&
        lane == SCENE_EDITOR_LIGHT_TIMELINE_LANE_INTENSITY &&
        allow_lazy_intensity) {
        RuntimeSceneBridge3DLightSeedState lights;
        RuntimeSceneLightTimelineTarget target;
        size_t track_index = 0u;
        runtime_scene_bridge_get_last_3d_light_seed_state(&lights);
        status = RuntimeSceneLightTimelineResolveTarget(
            lights.lights, (size_t)lights.light_count,
            document.timeline.tracks[document.progress_track_index].target_id,
            &target);
        if (status != TIMELINE_STATUS_OK) return status;
        status = scene_editor_light_timeline_ensure_intensity_track(
            &document, lights.lights[target.light_index].intensity,
            &track_index);
        if (status != TIMELINE_STATUS_OK) return status;
        current = &document.timeline.tracks[track_index];
        created = true;
    } else if (status != TIMELINE_STATUS_OK) {
        return status;
    }
    if (frame < document.timeline.range.start_frame ||
        (uint64_t)(frame - document.timeline.range.start_frame) >=
            document.timeline.range.frame_count) return TIMELINE_STATUS_FRAME_OUT_OF_RANGE;
    original = *current;
    candidate = *current;
    if (created &&
        (frame == candidate.keys[0].frame ||
         frame == candidate.keys[candidate.key_count - 1u].frame)) {
        index = frame == candidate.keys[0].frame
            ? 0u : candidate.key_count - 1u;
        status = TimelineTrackMoveScalarKey(
            &candidate, index, frame, value);
    } else {
        memset(&key, 0, sizeof(key));
        key.frame = frame;
        key.value = TimelineValueScalar(value);
        key.interpolation_to_next = TIMELINE_INTERPOLATION_LINEAR;
        status = TimelineTrackInsertKey(&candidate, key, &index);
    }
    if (status != TIMELINE_STATUS_OK) return status;
    status = scene_editor_light_timeline_constrain_adjacent_handles(
        &document, &candidate, index);
    if (status != TIMELINE_STATUS_OK) return status;
    if (created) {
        document.timeline.tracks[document.intensity_track_index] = candidate;
        status = RuntimeSceneLightTimelineSetLast(&document);
        if (status == TIMELINE_STATUS_OK) (void)capture_current_sample();
    } else {
        status = commit_track(&document, &candidate);
    }
    if (status != TIMELINE_STATUS_OK) return status;
    status = scene_editor_light_timeline_history_record(
        &g_light_timeline.history, created ? NULL : &original, &candidate);
    if (status != TIMELINE_STATUS_OK) return status;
    g_light_timeline.selected_key_index = (int)index;
    return TIMELINE_STATUS_OK;
}

TimelineStatus SceneEditorLightTimelineInsertKey(int64_t frame, double progress) {
    return insert_lane_key(
        SCENE_EDITOR_LIGHT_TIMELINE_LANE_MOTION, frame, progress, false);
}

TimelineStatus SceneEditorLightTimelineInsertIntensityKey(
    int64_t frame, double intensity) {
    return insert_lane_key(
        SCENE_EDITOR_LIGHT_TIMELINE_LANE_INTENSITY,
        frame, intensity, true);
}

TimelineStatus SceneEditorLightTimelineSelectLane(
    SceneEditorLightTimelineLane lane) {
    RuntimeSceneLightTimelineDocument document;
    if ((lane != SCENE_EDITOR_LIGHT_TIMELINE_LANE_MOTION &&
         lane != SCENE_EDITOR_LIGHT_TIMELINE_LANE_INTENSITY) ||
        !RuntimeSceneLightTimelineGetLast(&document)) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    g_light_timeline.lane = lane;
    g_light_timeline.selected_key_index = -1;
    g_light_timeline.selected_path_point_index = -1;
    g_light_timeline.dragging_key = false;
    g_light_timeline.dragging_path_point = false;
    g_light_timeline.dragging_handle =
        SCENE_EDITOR_LIGHT_TIMELINE_HANDLE_NONE;
    return TIMELINE_STATUS_OK;
}

SceneEditorLightTimelineLane SceneEditorLightTimelineSelectedLane(void) {
    return g_light_timeline.lane;
}

TimelineStatus SceneEditorLightTimelineDeleteSelectedKey(void) {
    RuntimeSceneLightTimelineDocument document;
    TimelineTrack* current = NULL;
    TimelineTrack candidate;
    int index = g_light_timeline.selected_key_index;
    if (!current_lane_document(&document, &current) || index < 0 ||
        (size_t)index >= current->key_count) return TIMELINE_STATUS_INVALID_ARGUMENT;
    if (current->key_count <= 2u) return TIMELINE_STATUS_INVALID_TRACK;
    candidate = *current;
    TimelineStatus status = TimelineTrackRemoveKey(&candidate, (size_t)index);
    if (status != TIMELINE_STATUS_OK) return status;
    if (candidate.key_count > 0u) {
        size_t adjacent = (size_t)index < candidate.key_count
            ? (size_t)index
            : candidate.key_count - 1u;
        status = scene_editor_light_timeline_constrain_adjacent_handles(
            &document, &candidate, adjacent);
        if (status != TIMELINE_STATUS_OK) return status;
    }
    status = commit_track(&document, &candidate);
    if (status != TIMELINE_STATUS_OK) return status;
    (void)scene_editor_light_timeline_history_record(
        &g_light_timeline.history, current, &candidate);
    if ((size_t)index >= candidate.key_count) index = (int)candidate.key_count - 1;
    g_light_timeline.selected_key_index = index;
    return TIMELINE_STATUS_OK;
}

TimelineStatus SceneEditorLightTimelineSetSelectedInterpolation(
    TimelineInterpolation interpolation) {
    RuntimeSceneLightTimelineDocument document;
    TimelineTrack* current = NULL;
    TimelineTrack candidate;
    int index = g_light_timeline.selected_key_index;
    TimelineStatus status;
    if (!current_lane_document(&document, &current) || index < 0 ||
        (size_t)index + 1u >= current->key_count) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    status = scene_editor_light_timeline_set_interpolation(
        &document, current, (size_t)index, interpolation, &candidate);
    if (status != TIMELINE_STATUS_OK) return status;
    if (memcmp(&candidate, current, sizeof(candidate)) == 0) {
        return TIMELINE_STATUS_OK;
    }
    status = commit_track(&document, &candidate);
    if (status != TIMELINE_STATUS_OK) return status;
    (void)scene_editor_light_timeline_history_record(
        &g_light_timeline.history, current, &candidate);
    return TIMELINE_STATUS_OK;
}

static bool history_restore(bool undo) {
    RuntimeSceneLightTimelineDocument document;
    TimelineStatus status;
    if (!RuntimeSceneLightTimelineGetLast(&document)) return false;
    status = undo
        ? scene_editor_light_timeline_history_undo(
              &g_light_timeline.history, &document)
        : scene_editor_light_timeline_history_redo(
              &g_light_timeline.history, &document);
    if (status != TIMELINE_STATUS_OK ||
        RuntimeSceneLightTimelineSetLast(&document) != TIMELINE_STATUS_OK) {
        return false;
    }
    {
        TimelineTrack* restored = NULL;
        if (scene_editor_light_timeline_lane_track(
                &document, g_light_timeline.lane, &restored, NULL) ==
                TIMELINE_STATUS_OK &&
            g_light_timeline.selected_key_index >=
                (int)restored->key_count) {
            g_light_timeline.selected_key_index =
                (int)restored->key_count - 1;
        } else if (!restored) {
            g_light_timeline.selected_key_index = -1;
        }
    }
    (void)capture_current_sample();
    return true;
}

bool SceneEditorLightTimelineUndo(void) { return history_restore(true); }
bool SceneEditorLightTimelineRedo(void) { return history_restore(false); }

bool SceneEditorLightTimelineInteractionActive(void) {
    return g_light_timeline.scrubbing || g_light_timeline.dragging_key ||
           g_light_timeline.dragging_path_point ||
           g_light_timeline.dragging_handle != 0 ||
           g_light_timeline.panning_graph || g_light_timeline.playing;
}

bool SceneEditorLightTimelinePlaying(void) {
    return g_light_timeline.playing;
}

bool SceneEditorLightTimelineCopyEvaluatedScene(
    RayEvaluatedSceneSnapshot* out_snapshot) {
    return scene_editor_light_timeline_evaluation_copy(
        &g_light_timeline.evaluation, out_snapshot);
}

static void stop_playback(void) {
    g_light_timeline.playing = false;
    g_light_timeline.playback_last_ms = 0u;
    g_light_timeline.playback_frame_fraction = 0.0;
}

static bool apply_traversal_mode(
    SceneEditorLightTimelineTraversalMode mode) {
    RuntimeSceneLightTimelineDocument document;
    TimelineTrack* current = NULL;
    TimelineTrack candidate;
    if (mode == SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_CUSTOM) {
        return true;
    }
    if (!current_document(&document, &current) ||
        !scene_editor_light_timeline_build_traversal_track(
            &document, current, mode, &candidate)) {
        return false;
    }
    if (candidate.key_count == current->key_count &&
        memcmp(candidate.keys, current->keys,
               current->key_count * sizeof(current->keys[0])) == 0) {
        return true;
    }
    stop_playback();
    if (commit_track(&document, &candidate) != TIMELINE_STATUS_OK) {
        return false;
    }
    (void)scene_editor_light_timeline_history_record(
        &g_light_timeline.history, current, &candidate);
    g_light_timeline.selected_key_index = -1;
    g_light_timeline.selected_path_point_index = -1;
    return true;
}

bool SceneEditorLightTimelineTogglePlayback(void) {
    RuntimeSceneLightTimelineDocument document;
    int64_t last_frame;
    if (!RuntimeSceneLightTimelineGetLast(&document) ||
        document.timeline.range.frame_count < 2u) {
        return false;
    }
    if (g_light_timeline.playing) {
        stop_playback();
        return true;
    }
    last_frame = document.timeline.range.start_frame +
        (int64_t)document.timeline.range.frame_count - 1;
    if (g_light_timeline.current_frame >= last_frame) {
        g_light_timeline.current_frame = document.timeline.range.start_frame;
        g_light_timeline.playback_frame_fraction = 0.0;
        if (!capture_current_sample()) {
            return false;
        }
    }
    g_light_timeline.playing = true;
    g_light_timeline.playback_last_ms = SDL_GetTicks64();
    if (g_light_timeline.playback_last_ms == 0u) {
        g_light_timeline.playback_last_ms = 1u;
    }
    g_light_timeline.playback_frame_fraction = 0.0;
    return true;
}

bool SceneEditorLightTimelineAdvancePlayback(void) {
    RuntimeSceneLightTimelineDocument document;
    uint64_t now;
    uint64_t elapsed_ms;
    double fps;
    double frames;
    int64_t advance;
    int64_t first_frame;
    int64_t last_frame;
    if (!g_light_timeline.playing) return false;
    if (!RuntimeSceneLightTimelineGetLast(&document) ||
        document.timeline.range.frame_count < 2u ||
        document.timeline.rate.frames_per_second_denominator == 0u) {
        stop_playback();
        return false;
    }
    now = SDL_GetTicks64();
    elapsed_ms = now >= g_light_timeline.playback_last_ms
        ? now - g_light_timeline.playback_last_ms
        : now;
    if (elapsed_ms == 0u) return false;
    if (elapsed_ms > 250u) elapsed_ms = 250u;
    g_light_timeline.playback_last_ms = now;
    fps = (double)document.timeline.rate.frames_per_second_numerator /
          (double)document.timeline.rate.frames_per_second_denominator;
    frames = g_light_timeline.playback_frame_fraction +
             (double)elapsed_ms * fps / 1000.0;
    advance = (int64_t)floor(frames);
    g_light_timeline.playback_frame_fraction = frames - (double)advance;
    first_frame = document.timeline.range.start_frame;
    last_frame = first_frame +
        (int64_t)document.timeline.range.frame_count - 1;
    g_light_timeline.current_frame += advance;
    if (g_light_timeline.current_frame > last_frame) {
        int64_t span = last_frame - first_frame + 1;
        g_light_timeline.current_frame =
            first_frame +
            (g_light_timeline.current_frame - first_frame) % span;
    }
    if (!capture_current_sample()) {
        stop_playback();
        return false;
    }
    return true;
}

static void sync_spatial_path_from_editor(void) {
    RuntimeSceneLightTimelineDocument document;
    if (!RuntimeSceneLightTimelineGetLast(&document)) return;
    if (memcmp(&document.spatial_path, &sceneSettings.bezierPath,
               sizeof(document.spatial_path)) == 0 &&
        memcmp(&document.spatial_path_3d, &sceneSettings.bezierPath3D,
               sizeof(document.spatial_path_3d)) == 0) return;
    if (sceneSettings.bezierPath.numPoints < 2) return;
    document.spatial_path = sceneSettings.bezierPath;
    document.spatial_path_3d = sceneSettings.bezierPath3D;
    if (RuntimeSceneLightTimelineSetLast(&document) == TIMELINE_STATUS_OK) {
        (void)capture_current_sample();
    }
}

void SceneEditorLightTimelineSyncRuntime(void) {
    RuntimeSceneLightTimelineDocument document;
    SceneEditorLightTimelineReset();
    if (!RuntimeSceneLightTimelineGetLast(&document) ||
        document.progress_track_index >= document.timeline.track_count) return;
    const char* target = document.timeline.tracks[document.progress_track_index].target_id;
    if (scene_editor_light_timeline_selection_bind(
            &g_light_timeline.selection, target) != TIMELINE_STATUS_OK) return;
    g_light_timeline.current_frame = document.timeline.range.start_frame;
    g_light_timeline.playback_frame_fraction = 0.0;
    (void)capture_current_sample();
}

bool SceneEditorLightTimelineHasSelectedLight(void) {
    RuntimeSceneBridge3DLightSeedState lights;
    size_t light_index = 0u;
    runtime_scene_bridge_get_last_3d_light_seed_state(&lights);
    return lights.valid &&
        scene_editor_light_timeline_selection_resolve(
            &g_light_timeline.selection, lights.lights,
            (size_t)lights.light_count, &light_index) == TIMELINE_STATUS_OK;
}

TimelineStatus SceneEditorLightTimelineSelectTargetId(const char* target_id) {
    RuntimeSceneBridge3DLightSeedState lights;
    RuntimeSceneLightTimelineTarget resolved;
    const char* selected =
        scene_editor_light_timeline_selection_target_id(
            &g_light_timeline.selection);
    if (g_light_timeline.target_locked && selected[0] &&
        strcmp(selected, target_id ? target_id : "") != 0) {
        return TIMELINE_STATUS_OWNERSHIP_MISMATCH;
    }
    runtime_scene_bridge_get_last_3d_light_seed_state(&lights);
    TimelineStatus status = RuntimeSceneLightTimelineResolveTarget(
        lights.lights, (size_t)lights.light_count, target_id, &resolved);
    if (status != TIMELINE_STATUS_OK) return status;
    return scene_editor_light_timeline_selection_bind(
        &g_light_timeline.selection, target_id);
}

const char* SceneEditorLightTimelineSelectedTargetId(void) {
    return scene_editor_light_timeline_selection_target_id(
        &g_light_timeline.selection);
}

static bool create_default_document_for_selected(void) {
    RuntimeSceneLightTimelineDocument document;
    TimelineTrack track;
    const char* target = SceneEditorLightTimelineSelectedTargetId();
    int fps = animSettings.fps > 0 ? animSettings.fps : 30;
    int frame_count = animSettings.frameLimit >= 3
        ? animSettings.frameLimit
        : fps * 5 + 1;
    if (!target[0] || sceneSettings.bezierPath.numPoints < 2) return false;
    memset(&document, 0, sizeof(document));
    memset(&track, 0, sizeof(track));
    if (TimelineDocumentInit(&document.timeline, (TimelineRate){(uint32_t)fps, 1u},
                             (TimelineRange){0, (uint64_t)frame_count}) != TIMELINE_STATUS_OK ||
        TimelineTrackInit(&track, "selected_light_progress", target,
                          "light/path_progress", TIMELINE_VALUE_SCALAR) != TIMELINE_STATUS_OK ||
        TimelineTrackSetUnit(&track, TIMELINE_UNIT_UNITLESS) != TIMELINE_STATUS_OK) {
        return false;
    }
    document.spatial_path = sceneSettings.bezierPath;
    document.spatial_path_3d = sceneSettings.bezierPath3D;
    if (TimelineTrackAddKey(
            &track, 0, TimelineValueScalar(0.0),
            TIMELINE_INTERPOLATION_LINEAR) != TIMELINE_STATUS_OK ||
        TimelineTrackAddKey(
            &track, frame_count - 1, TimelineValueScalar(1.0),
            TIMELINE_INTERPOLATION_STEP) != TIMELINE_STATUS_OK) {
        return false;
    }
    if (TimelineDocumentAddTrack(&document.timeline, &track) !=
        TIMELINE_STATUS_OK) {
        return false;
    }
    document.progress_track_index = 0u;
    document.valid = true;
    return RuntimeSceneLightTimelineSetLast(&document) == TIMELINE_STATUS_OK;
}

bool SceneEditorLightTimelineToggle(SceneEditorPaneHost* pane_host) {
    RuntimeSceneLightTimelineDocument document;
    if (!pane_host || !SceneEditorLightTimelineHasSelectedLight()) return false;
    if (scene_editor_pane_host_timeline_visible(pane_host)) {
        stop_playback();
        g_light_timeline.scrubbing = false;
        g_light_timeline.dragging_path_point = false;
        g_light_timeline.panning_graph = false;
        if (!scene_editor_pane_host_set_timeline_visible(pane_host, false)) {
            return false;
        }
        g_light_timeline.target_locked = false;
        return true;
    }
    if (!RuntimeSceneLightTimelineGetLast(&document)) {
        if (!create_default_document_for_selected()) return false;
    } else {
        const char* target = document.timeline.tracks[document.progress_track_index].target_id;
        if (strcmp(target, SceneEditorLightTimelineSelectedTargetId()) != 0) return false;
    }
    if (!RuntimeSceneLightTimelineGetLast(&document) ||
        !scene_editor_light_timeline_ensure_editable_default_range(&document)) {
        return false;
    }
    g_light_timeline.current_frame = document.timeline.range.start_frame;
    g_light_timeline.playback_frame_fraction = 0.0;
    scene_editor_light_timeline_view_reset(&g_light_timeline.view);
    (void)capture_current_sample();
    if (!scene_editor_pane_host_set_timeline_visible(pane_host, true)) {
        return false;
    }
    g_light_timeline.target_locked = true;
    return true;
}

static bool scrub_to_x(int x, const SDL_Rect* rect) {
    RuntimeSceneLightTimelineDocument document;
    double normalized;
    int64_t frame;
    if (!rect || rect->w <= 1 || !RuntimeSceneLightTimelineGetLast(&document)) return false;
    stop_playback();
    normalized = scene_editor_light_timeline_view_normalized_at_x(
        &g_light_timeline.view, rect, x);
    frame = document.timeline.range.start_frame +
            (int64_t)llround(normalized * (double)(document.timeline.range.frame_count - 1u));
    g_light_timeline.current_frame = frame;
    g_light_timeline.playback_frame_fraction = 0.0;
    return capture_current_sample();
}

static bool base_intensity_for_document(
    const RuntimeSceneLightTimelineDocument* document,
    double* out_intensity) {
    RuntimeSceneBridge3DLightSeedState lights;
    RuntimeSceneLightTimelineTarget target;
    TimelineStatus status;
    if (!document || !out_intensity ||
        document->progress_track_index >= document->timeline.track_count) {
        return false;
    }
    runtime_scene_bridge_get_last_3d_light_seed_state(&lights);
    status = RuntimeSceneLightTimelineResolveTarget(
        lights.lights, (size_t)lights.light_count,
        document->timeline.tracks[document->progress_track_index].target_id,
        &target);
    if (status != TIMELINE_STATUS_OK) return false;
    *out_intensity = lights.lights[target.light_index].intensity;
    return isfinite(*out_intensity) && *out_intensity >= 0.0;
}

static void graph_to_key_values(const RuntimeSceneLightTimelineDocument* document,
                                const TimelineTrack* track,
                                const SDL_Rect* graph, int x, int y,
                                int64_t* out_frame, double* out_value) {
    double nx = scene_editor_light_timeline_view_normalized_at_x(
        &g_light_timeline.view, graph, x);
    double fallback = 1.0;
    double minimum = 0.0;
    double maximum = 1.0;
    (void)base_intensity_for_document(document, &fallback);
    scene_editor_light_timeline_lane_value_range(
        g_light_timeline.lane, track, fallback, &minimum, &maximum);
    if (out_frame) {
        *out_frame = document->timeline.range.start_frame +
            (int64_t)llround(nx * (double)(document->timeline.range.frame_count - 1u));
    }
    if (out_value) {
        *out_value = scene_editor_light_timeline_lane_value_at_y(
            graph, y, minimum, maximum);
    }
}

static bool move_selected_key(int x, int y, const SDL_Rect* graph) {
    RuntimeSceneLightTimelineDocument document;
    TimelineTrack* current = NULL;
    TimelineTrack candidate;
    int64_t frame = 0;
    double value = 0.0;
    int index = g_light_timeline.selected_key_index;
    if (!current_lane_document(&document, &current) || index < 0 ||
        (size_t)index >= current->key_count) return false;
    graph_to_key_values(&document, current, graph, x, y, &frame, &value);
    if (index == 0) {
        frame = current->keys[0].frame;
        if (g_light_timeline.lane ==
            SCENE_EDITOR_LIGHT_TIMELINE_LANE_MOTION) {
            value = current->keys[0].value.as.scalar;
        }
    } else if ((size_t)index + 1u == current->key_count) {
        frame = current->keys[index].frame;
        if (g_light_timeline.lane ==
            SCENE_EDITOR_LIGHT_TIMELINE_LANE_MOTION) {
            value = current->keys[index].value.as.scalar;
        }
    } else {
        int64_t minimum_frame = current->keys[index - 1].frame + 1;
        int64_t maximum_frame = current->keys[index + 1].frame - 1;
        if (frame < minimum_frame) frame = minimum_frame;
        if (frame > maximum_frame) frame = maximum_frame;
        if (g_light_timeline.lane ==
            SCENE_EDITOR_LIGHT_TIMELINE_LANE_MOTION) {
            double minimum_progress =
                current->keys[index - 1].value.as.scalar;
            double maximum_progress =
                current->keys[index + 1].value.as.scalar;
            if (value < minimum_progress) value = minimum_progress;
            if (value > maximum_progress) value = maximum_progress;
            value = scene_editor_light_timeline_snap_progress_to_anchor(
                &document, value,
                graph->h > 0 ? 8.0 / (double)graph->h : 0.0,
                NULL);
            if (value < minimum_progress) value = minimum_progress;
            if (value > maximum_progress) value = maximum_progress;
        }
    }
    candidate = *current;
    if (TimelineTrackMoveScalarKey(&candidate, (size_t)index, frame, value) !=
        TIMELINE_STATUS_OK) return false;
    if (scene_editor_light_timeline_constrain_adjacent_handles(
            &document, &candidate, (size_t)index) !=
        TIMELINE_STATUS_OK) {
        return false;
    }
    if (commit_track(&document, &candidate) != TIMELINE_STATUS_OK) {
        return false;
    }
    (void)scene_editor_light_timeline_history_update_after(
        &g_light_timeline.history, &candidate);
    return true;
}

static bool insert_key_at_graph_position(
    const SDL_Rect* graph,
    int x,
    int y,
    bool preserve_shape) {
    RuntimeSceneLightTimelineDocument document;
    TimelineTrack* track = NULL;
    double normalized;
    double value = 0.0;
    double fallback = 1.0;
    double minimum = 0.0;
    double maximum = 1.0;
    int64_t requested;
    int64_t frame;
    TimelineStatus status;
    if (!graph || !RuntimeSceneLightTimelineGetLast(&document)) return false;
    normalized = scene_editor_light_timeline_view_normalized_at_x(
        &g_light_timeline.view, graph, x);
    requested = document.timeline.range.start_frame +
        (int64_t)llround(normalized *
            (double)(document.timeline.range.frame_count - 1u));
    status = scene_editor_light_timeline_lane_track(
        &document, g_light_timeline.lane, &track, NULL);
    if (status == TIMELINE_STATUS_TARGET_NOT_FOUND &&
        g_light_timeline.lane ==
            SCENE_EDITOR_LIGHT_TIMELINE_LANE_INTENSITY) {
        int64_t first = document.timeline.range.start_frame;
        int64_t last = first +
            (int64_t)document.timeline.range.frame_count - 1;
        if (!base_intensity_for_document(&document, &fallback)) return false;
        frame = requested < first ? first
                                  : requested > last ? last : requested;
        value = preserve_shape
            ? fallback
            : scene_editor_light_timeline_lane_value_at_y(
                  graph, y, 0.0, fallback > 0.0 ? fallback * 1.25 : 1.0);
        return SceneEditorLightTimelineInsertIntensityKey(frame, value) ==
            TIMELINE_STATUS_OK;
    }
    if (status != TIMELINE_STATUS_OK) return false;
    if (!scene_editor_light_timeline_nearest_open_frame(
            &document, track, requested, &frame)) {
        return false;
    }
    if (preserve_shape) {
        TimelineEvaluationContext context;
        TimelineEvaluationResult result;
        if (TimelineEvaluationContextBuild(
                document.timeline.rate, document.timeline.range,
                (TimelineSample){frame, 0u, 1u}, &context) !=
                TIMELINE_STATUS_OK ||
            TimelineTrackEvaluate(track, &context, &result) !=
                TIMELINE_STATUS_OK) {
            return false;
        }
        value = result.value.as.scalar;
    } else {
        (void)base_intensity_for_document(&document, &fallback);
        scene_editor_light_timeline_lane_value_range(
            g_light_timeline.lane, track, fallback, &minimum, &maximum);
        value = scene_editor_light_timeline_lane_value_at_y(
            graph, y, minimum, maximum);
        if (g_light_timeline.lane ==
            SCENE_EDITOR_LIGHT_TIMELINE_LANE_MOTION) {
            size_t right = 0u;
            value = scene_editor_light_timeline_snap_progress_to_anchor(
                &document, value,
                graph->h > 0 ? 8.0 / (double)graph->h : 0.0,
                NULL);
            while (right < track->key_count &&
                   track->keys[right].frame < frame) {
                right += 1u;
            }
            if (right == 0u || right >= track->key_count) return false;
            if (value < track->keys[right - 1u].value.as.scalar) {
                value = track->keys[right - 1u].value.as.scalar;
            }
            if (value > track->keys[right].value.as.scalar) {
                value = track->keys[right].value.as.scalar;
            }
        }
    }
    return (g_light_timeline.lane ==
                SCENE_EDITOR_LIGHT_TIMELINE_LANE_INTENSITY
            ? SceneEditorLightTimelineInsertIntensityKey(frame, value)
            : SceneEditorLightTimelineInsertKey(frame, value)) ==
        TIMELINE_STATUS_OK;
}

static int pick_path_point(
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* track,
    const SDL_Rect* strip,
    int x,
    int y) {
    int best = -1;
    int best_distance = 9;
    if (!document || !track || !strip ||
        !point_in_rect(x, y, strip)) {
        return -1;
    }
    for (int i = 0; i < document->spatial_path.numPoints; ++i) {
        double progress;
        double frame_position;
        double normalized;
        int marker_x;
        int distance;
        if (!scene_editor_light_timeline_path_anchor_progress(
                document, i, &progress) ||
            !scene_editor_light_timeline_frame_at_progress(
                document, track, progress, &frame_position)) {
            continue;
        }
        normalized =
            (frame_position -
             (double)document->timeline.range.start_frame) /
            (double)(document->timeline.range.frame_count - 1u);
        marker_x = scene_editor_light_timeline_view_x_at_normalized(
            &g_light_timeline.view, strip, normalized);
        distance = abs(marker_x - x);
        if (distance <= best_distance) {
            best_distance = distance;
            best = i;
        }
    }
    return best;
}

static bool move_selected_path_point(int x, const SDL_Rect* strip) {
    RuntimeSceneLightTimelineDocument document;
    TimelineTrack* current = NULL;
    TimelineTrack candidate;
    double normalized;
    int64_t frame;
    size_t key_index = 0u;
    int point_index = g_light_timeline.selected_path_point_index;
    if (!strip || !current_document(&document, &current) ||
        point_index <= 0 ||
        point_index + 1 >= document.spatial_path.numPoints) {
        return false;
    }
    normalized = scene_editor_light_timeline_view_normalized_at_x(
        &g_light_timeline.view, strip, x);
    frame = document.timeline.range.start_frame +
        (int64_t)llround(
            normalized *
            (double)(document.timeline.range.frame_count - 1u));
    if (!scene_editor_light_timeline_set_path_anchor_frame(
            &document, current, point_index, frame,
            &candidate, &key_index) ||
        commit_track(&document, &candidate) != TIMELINE_STATUS_OK) {
        return false;
    }
    (void)scene_editor_light_timeline_history_update_after(
        &g_light_timeline.history, &candidate);
    g_light_timeline.selected_key_index = (int)key_index;
    return true;
}

bool SceneEditorLightTimelineHandleEvent(
    SDL_Event* event, SceneEditorPaneHost* pane_host,
    const SceneEditorPaneLayout* layout,
    const SceneEditorDigestOverlayNavState* nav_state) {
    if (!event || !pane_host || !layout) return false;
    if (layout->timeline_visible) {
        RuntimeSceneLightTimelineDocument document;
        TimelineTrack* track = NULL;
        SceneEditorLightTimelinePanelGeometry geometry;
        SDL_Rect graph;
        SDL_Rect path_point_strip;
        SDL_Rect speed_strip;
        scene_editor_light_timeline_panel_geometry(
            &layout->timeline_rect, &geometry);
        graph = geometry.timing_graph;
        path_point_strip = geometry.path_point_strip;
        speed_strip = geometry.speed_strip;
        if (event->type == SDL_KEYDOWN &&
            RuntimeSceneLightTimelineGetLast(&document)) {
            (void)scene_editor_light_timeline_lane_track(
                &document, g_light_timeline.lane, &track, NULL);
            SDL_Keymod mod = event->key.keysym.mod;
            bool command = (mod & (KMOD_CTRL | KMOD_GUI)) != 0;
            if (command && event->key.keysym.sym == SDLK_z) {
                return (mod & KMOD_SHIFT) ? SceneEditorLightTimelineRedo()
                                          : SceneEditorLightTimelineUndo();
            }
            if (command && event->key.keysym.sym == SDLK_y) {
                return SceneEditorLightTimelineRedo();
            }
            if (event->key.keysym.sym == SDLK_DELETE ||
                event->key.keysym.sym == SDLK_BACKSPACE) {
                return SceneEditorLightTimelineDeleteSelectedKey() == TIMELINE_STATUS_OK;
            }
            if (event->key.keysym.sym == SDLK_f) {
                scene_editor_light_timeline_view_reset(&g_light_timeline.view);
                return true;
            }
            if (event->key.keysym.sym == SDLK_a) {
                double normalized =
                    (double)(g_light_timeline.current_frame -
                             document.timeline.range.start_frame) /
                    (double)(document.timeline.range.frame_count - 1u);
                int x = scene_editor_light_timeline_view_x_at_normalized(
                    &g_light_timeline.view, &graph, normalized);
                return insert_key_at_graph_position(
                    &graph, x, graph.y + graph.h / 2, true);
            }
        }
        if (event->type == SDL_MOUSEWHEEL &&
            RuntimeSceneLightTimelineGetLast(&document)) {
            int mouse_x = g_light_timeline.mouse_x;
            int mouse_y = g_light_timeline.mouse_y;
            double minimum_span = document.timeline.range.frame_count > 2u
                ? 2.0 / (double)(document.timeline.range.frame_count - 1u)
                : 1.0;
            if (!point_in_rect(mouse_x, mouse_y, &graph)) return false;
            if ((SDL_GetModState() & KMOD_SHIFT) != 0) {
                scene_editor_light_timeline_view_pan(
                    &g_light_timeline.view,
                    (event->wheel.y < 0 ? 0.1 : -0.1) *
                        g_light_timeline.view.span_normalized);
            } else {
                double anchor =
                    scene_editor_light_timeline_view_normalized_at_x(
                        &g_light_timeline.view, &graph, mouse_x);
                scene_editor_light_timeline_view_zoom(
                    &g_light_timeline.view, anchor,
                    event->wheel.y > 0 ? 0.80 : 1.25,
                    minimum_span);
            }
            return true;
        }
        if (event->type == SDL_MOUSEBUTTONDOWN &&
            event->button.button == SDL_BUTTON_MIDDLE &&
            point_in_rect(event->button.x, event->button.y, &graph)) {
            g_light_timeline.panning_graph = true;
            g_light_timeline.pan_anchor_x = event->button.x;
            g_light_timeline.pan_anchor_start =
                g_light_timeline.view.start_normalized;
            return true;
        }
        if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT &&
            point_in_rect(event->button.x, event->button.y, &layout->timeline_rect)) {
            if (point_in_rect(event->button.x, event->button.y,
                              &geometry.motion_lane_button)) {
                (void)SceneEditorLightTimelineSelectLane(
                    SCENE_EDITOR_LIGHT_TIMELINE_LANE_MOTION);
                return true;
            }
            if (point_in_rect(event->button.x, event->button.y,
                              &geometry.intensity_lane_button)) {
                (void)SceneEditorLightTimelineSelectLane(
                    SCENE_EDITOR_LIGHT_TIMELINE_LANE_INTENSITY);
                return true;
            }
            if (RuntimeSceneLightTimelineGetLast(&document)) {
                (void)scene_editor_light_timeline_lane_track(
                    &document, g_light_timeline.lane, &track, NULL);
                if (point_in_rect(event->button.x, event->button.y,
                                  &geometry.add_key_button)) {
                    stop_playback();
                    double normalized =
                        (double)(g_light_timeline.current_frame -
                                 document.timeline.range.start_frame) /
                        (double)(document.timeline.range.frame_count - 1u);
                    int x = scene_editor_light_timeline_view_x_at_normalized(
                        &g_light_timeline.view, &graph, normalized);
                    (void)insert_key_at_graph_position(
                        &graph, x, graph.y + graph.h / 2, true);
                    return true;
                }
                if (point_in_rect(event->button.x, event->button.y,
                                  &geometry.play_button)) {
                    (void)SceneEditorLightTimelineTogglePlayback();
                    return true;
                }
                if (!track && event->button.clicks >= 2 &&
                    point_in_rect(event->button.x, event->button.y,
                                  &graph)) {
                    (void)insert_key_at_graph_position(
                        &graph, event->button.x, event->button.y, false);
                    return true;
                }
            }
            if (track) {
                if (point_in_rect(
                        event->button.x, event->button.y,
                        &geometry.constant_speed_button) &&
                    g_light_timeline.lane ==
                        SCENE_EDITOR_LIGHT_TIMELINE_LANE_MOTION) {
                    (void)apply_traversal_mode(
                        SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_CONSTANT_SPEED);
                    return true;
                }
                if (point_in_rect(
                        event->button.x, event->button.y,
                        &geometry.equal_segments_button) &&
                    g_light_timeline.lane ==
                        SCENE_EDITOR_LIGHT_TIMELINE_LANE_MOTION) {
                    (void)apply_traversal_mode(
                        SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_EQUAL_SEGMENTS);
                    return true;
                }
                if (point_in_rect(event->button.x, event->button.y,
                                  &geometry.step_button)) {
                    (void)SceneEditorLightTimelineSetSelectedInterpolation(
                        TIMELINE_INTERPOLATION_STEP);
                    return true;
                }
                if (point_in_rect(event->button.x, event->button.y,
                                  &geometry.linear_button)) {
                    (void)SceneEditorLightTimelineSetSelectedInterpolation(
                        TIMELINE_INTERPOLATION_LINEAR);
                    return true;
                }
                if (point_in_rect(event->button.x, event->button.y,
                                  &geometry.bezier_button)) {
                    (void)SceneEditorLightTimelineSetSelectedInterpolation(
                        TIMELINE_INTERPOLATION_CUBIC_BEZIER);
                    return true;
                }
                if (g_light_timeline.lane ==
                    SCENE_EDITOR_LIGHT_TIMELINE_LANE_MOTION) {
                    int path_point = pick_path_point(
                        &document, track, &path_point_strip,
                        event->button.x, event->button.y);
                    if (path_point >= 0) {
                        stop_playback();
                        g_light_timeline.selected_path_point_index =
                            path_point;
                        g_light_timeline.selected_key_index = -1;
                        if (path_point > 0 &&
                            path_point + 1 <
                                document.spatial_path.numPoints) {
                            (void)scene_editor_light_timeline_history_record(
                                &g_light_timeline.history, track, track);
                            g_light_timeline.dragging_path_point = true;
                            (void)move_selected_path_point(
                                event->button.x, &path_point_strip);
                        }
                        return true;
                    }
                }
                if (event->button.clicks >= 2 &&
                    point_in_rect(event->button.x, event->button.y,
                                  &graph)) {
                    (void)insert_key_at_graph_position(
                        &graph, event->button.x, event->button.y, false);
                    return true;
                }
                if (g_light_timeline.selected_key_index >= 0) {
                    double fallback = 1.0;
                    double minimum = 0.0;
                    double maximum = 1.0;
                    (void)base_intensity_for_document(
                        &document, &fallback);
                    scene_editor_light_timeline_lane_value_range(
                        g_light_timeline.lane, track, fallback,
                        &minimum, &maximum);
                    SceneEditorLightTimelineHandle handle =
                        scene_editor_light_timeline_pick_scalar_handle(
                            &g_light_timeline.view, &document, track,
                            (size_t)g_light_timeline.selected_key_index,
                            &graph, minimum, maximum,
                            event->button.x, event->button.y);
                    if (handle !=
                        SCENE_EDITOR_LIGHT_TIMELINE_HANDLE_NONE) {
                        stop_playback();
                        (void)scene_editor_light_timeline_history_record(
                            &g_light_timeline.history, track, track);
                        g_light_timeline.dragging_handle = handle;
                        return true;
                    }
                }
                double fallback = 1.0;
                double minimum = 0.0;
                double maximum = 1.0;
                (void)base_intensity_for_document(&document, &fallback);
                scene_editor_light_timeline_lane_value_range(
                    g_light_timeline.lane, track, fallback,
                    &minimum, &maximum);
                int key_index = scene_editor_light_timeline_pick_scalar_key(
                    &g_light_timeline.view, &document, track, &graph,
                    minimum, maximum,
                    event->button.x, event->button.y);
                if (key_index >= 0) {
                    stop_playback();
                    g_light_timeline.selected_key_index = key_index;
                    g_light_timeline.selected_path_point_index = -1;
                    (void)scene_editor_light_timeline_history_record(
                        &g_light_timeline.history, track, track);
                    g_light_timeline.dragging_key = true;
                    return true;
                }
            }
            if (g_light_timeline.lane ==
                    SCENE_EDITOR_LIGHT_TIMELINE_LANE_MOTION &&
                current_document(&document, &track) &&
                point_in_rect(event->button.x, event->button.y,
                              &speed_strip)) {
                double normalized =
                    scene_editor_light_timeline_view_normalized_at_x(
                        &g_light_timeline.view, &speed_strip,
                        event->button.x);
                int64_t frame = document.timeline.range.start_frame +
                    (int64_t)llround(normalized *
                        (double)(document.timeline.range.frame_count - 1u));
                for (size_t i = 0u; i + 1u < track->key_count; ++i) {
                    if (frame >= track->keys[i].frame &&
                        frame <= track->keys[i + 1u].frame) {
                        g_light_timeline.selected_key_index = (int)i;
                        g_light_timeline.selected_path_point_index = -1;
                        return true;
                    }
                }
            }
            if (point_in_rect(event->button.x, event->button.y,
                              &graph)) {
                g_light_timeline.scrubbing = true;
                return scrub_to_x(event->button.x, &graph);
            }
            return true;
        }
        if (event->type == SDL_MOUSEMOTION && g_light_timeline.dragging_key) {
            return move_selected_key(event->motion.x, event->motion.y, &graph);
        }
        if (event->type == SDL_MOUSEMOTION &&
            g_light_timeline.dragging_handle !=
                SCENE_EDITOR_LIGHT_TIMELINE_HANDLE_NONE) {
            RuntimeSceneLightTimelineDocument document;
            TimelineTrack* current = NULL;
            TimelineTrack candidate;
            double fallback = 1.0;
            double minimum = 0.0;
            double maximum = 1.0;
            if (!current_lane_document(&document, &current) ||
                g_light_timeline.selected_key_index < 0) {
                return false;
            }
            (void)base_intensity_for_document(&document, &fallback);
            scene_editor_light_timeline_lane_value_range(
                g_light_timeline.lane, current, fallback,
                &minimum, &maximum);
            if (scene_editor_light_timeline_move_scalar_handle(
                    &g_light_timeline.view, &document, current,
                    (size_t)g_light_timeline.selected_key_index,
                    g_light_timeline.dragging_handle, &graph,
                    minimum, maximum,
                    event->motion.x, event->motion.y,
                    &candidate) != TIMELINE_STATUS_OK ||
                commit_track(&document, &candidate) !=
                    TIMELINE_STATUS_OK) {
                return false;
            }
            (void)scene_editor_light_timeline_history_update_after(
                &g_light_timeline.history, &candidate);
            return true;
        }
        if (event->type == SDL_MOUSEMOTION &&
            g_light_timeline.dragging_path_point) {
            return move_selected_path_point(
                event->motion.x, &path_point_strip);
        }
        if (event->type == SDL_MOUSEMOTION &&
            g_light_timeline.panning_graph) {
            double delta = -(double)(event->motion.x -
                                     g_light_timeline.pan_anchor_x) /
                           (double)(graph.w > 0 ? graph.w : 1) *
                           g_light_timeline.view.span_normalized;
            g_light_timeline.view.start_normalized =
                g_light_timeline.pan_anchor_start;
            scene_editor_light_timeline_view_pan(
                &g_light_timeline.view, delta);
            return true;
        }
        if (event->type == SDL_MOUSEMOTION && g_light_timeline.scrubbing) {
            return scrub_to_x(event->motion.x, &graph);
        }
        if (event->type == SDL_MOUSEBUTTONUP &&
            (event->button.button == SDL_BUTTON_LEFT ||
             event->button.button == SDL_BUTTON_MIDDLE) &&
            (g_light_timeline.scrubbing || g_light_timeline.dragging_key ||
             g_light_timeline.dragging_path_point ||
             g_light_timeline.dragging_handle != 0 ||
             g_light_timeline.panning_graph)) {
            g_light_timeline.scrubbing = false;
            g_light_timeline.dragging_key = false;
            g_light_timeline.dragging_path_point = false;
            g_light_timeline.dragging_handle =
                SCENE_EDITOR_LIGHT_TIMELINE_HANDLE_NONE;
            g_light_timeline.panning_graph = false;
            return true;
        }
        if (event->type == SDL_MOUSEMOTION) {
            g_light_timeline.mouse_x = event->motion.x;
            g_light_timeline.mouse_y = event->motion.y;
            g_light_timeline.pointer_over_panel =
                point_in_rect(event->motion.x, event->motion.y,
                              &layout->timeline_rect);
            return g_light_timeline.pointer_over_panel;
        }
    }
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT &&
        point_in_rect(event->button.x, event->button.y, &layout->viewport_rect)) {
        RuntimeSceneBridge3DDigestState digest;
        RuntimeSceneBridge3DLightSeedState lights;
        SceneEditorDigestOverlayProjector projector;
        runtime_scene_bridge_get_last_3d_digest_state(&digest);
        runtime_scene_bridge_get_last_3d_light_seed_state(&lights);
        if (digest.valid && SceneEditorDigestOverlayBuildProjector(
                                &digest, &layout->viewport_rect, nav_state, &projector)) {
            int picked = pick_light(&projector, event->button.x, event->button.y);
            if (picked >= 0) {
                const char* active_target =
                    SceneEditorLightTimelineSelectedTargetId();
                char picked_target[TIMELINE_ID_CAPACITY];
                snprintf(picked_target, sizeof(picked_target), "light/%s",
                         lights.lights[picked].id);
                if (layout->timeline_visible && active_target[0] &&
                    strcmp(active_target, picked_target) != 0) {
                    return true;
                }
                (void)scene_editor_light_timeline_selection_select_index(
                    &g_light_timeline.selection, lights.lights,
                    (size_t)lights.light_count, (size_t)picked);
                return true;
            }
        }
    }
    return false;
}

void SceneEditorLightTimelineRenderViewportProxies(
    SDL_Renderer* renderer, const SceneEditorDigestOverlayProjector* projector,
    int mouse_x, int mouse_y) {
    RuntimeSceneBridge3DLightSeedState lights;
    size_t selected_light_index = SIZE_MAX;
    runtime_scene_bridge_get_last_3d_light_seed_state(&lights);
    if (lights.valid) {
        (void)scene_editor_light_timeline_selection_resolve(
            &g_light_timeline.selection, lights.lights,
            (size_t)lights.light_count, &selected_light_index);
    }
    g_light_timeline.hovered_light_index = pick_light(projector, mouse_x, mouse_y);
    if (!renderer || !projector || !lights.valid) return;
    for (int i = 0; i < lights.light_count; ++i) {
        int x = 0, y = 0;
        double position_x = 0.0;
        double position_y = 0.0;
        double position_z = 0.0;
        scene_editor_light_timeline_evaluation_light_position(
            &g_light_timeline.evaluation, &lights, i, &position_x, &position_y,
            &position_z);
        if (!lights.lights[i].id[0] ||
            !SceneEditorDigestOverlayProjectPoint(projector,
                                                  position_x,
                                                  position_y,
                                                  position_z,
                                                  &x, &y)) continue;
        int radius = ((size_t)i == selected_light_index) ? 8 : 6;
        SDL_Rect marker = {x - radius, y - radius, radius * 2, radius * 2};
        SDL_Color color = ((size_t)i == selected_light_index)
                              ? (SDL_Color){255, 210, 76, 255}
                              : (i == g_light_timeline.hovered_light_index)
                                    ? (SDL_Color){255, 235, 150, 255}
                                    : (SDL_Color){255, 190, 64, 220};
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderDrawRect(renderer, &marker);
        SDL_RenderDrawLine(renderer, x - radius, y, x + radius, y);
        SDL_RenderDrawLine(renderer, x, y - radius, x, y + radius);
    }
}

void SceneEditorLightTimelineRenderPanel(SDL_Renderer* renderer,
                                         const SceneEditorPaneLayout* layout) {
    RuntimeSceneLightTimelineDocument document;
    SceneEditorLightTimelinePanelState state;
    sync_spatial_path_from_editor();
    if (!renderer || !layout || !layout->timeline_visible ||
        !RuntimeSceneLightTimelineGetLast(&document)) {
        return;
    }
    memset(&state, 0, sizeof(state));
    state.current_frame = g_light_timeline.current_frame;
    state.selected_key_index = g_light_timeline.selected_key_index;
    state.selected_path_point_index =
        g_light_timeline.selected_path_point_index;
    state.mouse_x = g_light_timeline.mouse_x;
    state.mouse_y = g_light_timeline.mouse_y;
    state.playing = g_light_timeline.playing;
    state.pointer_over_panel = g_light_timeline.pointer_over_panel;
    state.lane = g_light_timeline.lane;
    state.base_intensity = 1.0;
    (void)base_intensity_for_document(
        &document, &state.base_intensity);
    state.traversal_mode =
        scene_editor_light_timeline_classify_traversal(
            &document,
            &document.timeline.tracks[
                document.progress_track_index]);
    state.view = &g_light_timeline.view;
    state.evaluated_scene =
        scene_editor_light_timeline_evaluation_snapshot(
            &g_light_timeline.evaluation);
    scene_editor_light_timeline_panel_render(
        renderer, &layout->timeline_rect, &document, &state);
}
