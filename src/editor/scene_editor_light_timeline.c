#include "editor/scene_editor_light_timeline.h"

#include "animation/timeline_document.h"
#include "config/config_manager.h"
#include "editor/scene_editor_light_timeline_panel.h"
#include "import/runtime_scene_bridge.h"
#include "import/runtime_scene_light_timeline_io.h"
#include "import/runtime_scene_light_timeline_bridge.h"
#include "scene_editor_light_timeline_edit.h"
#include "scene_editor_light_timeline_evaluation.h"
#include "scene_editor_light_timeline_view.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct SceneEditorLightTimelineState {
    int selected_light_index;
    int hovered_light_index;
    int64_t current_frame;
    bool scrubbing;
    bool dragging_key;
    bool dragging_path_point;
    int dragging_handle;
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
    TimelineTrack undo_tracks[24];
    size_t undo_count;
    TimelineTrack redo_tracks[24];
    size_t redo_count;
} SceneEditorLightTimelineState;

static SceneEditorLightTimelineState g_light_timeline = {.selected_light_index = -1,
                                                         .hovered_light_index = -1,
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
    g_light_timeline.selected_light_index = -1;
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

static void history_push(TimelineTrack* stack, size_t* count,
                         const TimelineTrack* track) {
    if (!stack || !count || !track) return;
    if (*count >= 24u) {
        memmove(&stack[0], &stack[1], 23u * sizeof(stack[0]));
        *count = 23u;
    }
    stack[(*count)++] = *track;
}

static TimelineStatus commit_track(RuntimeSceneLightTimelineDocument* document,
                                   const TimelineTrack* track) {
    if (!document || !track || document->progress_track_index >= document->timeline.track_count) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    RuntimeSceneLightTimelineDocument candidate = *document;
    candidate.timeline.tracks[candidate.progress_track_index] = *track;
    TimelineStatus status = TimelineDocumentValidate(&candidate.timeline);
    if (status != TIMELINE_STATUS_OK) return status;
    status = RuntimeSceneLightTimelineSetLast(&candidate);
    if (status == TIMELINE_STATUS_OK) {
        (void)capture_current_sample();
    }
    return status;
}

TimelineStatus SceneEditorLightTimelineInsertKey(int64_t frame, double progress) {
    RuntimeSceneLightTimelineDocument document;
    TimelineTrack* current = NULL;
    TimelineTrack candidate;
    TimelineKeyframe key;
    size_t index = 0u;
    if (!isfinite(progress) || progress < 0.0 || progress > 1.0 ||
        !current_document(&document, &current)) return TIMELINE_STATUS_INVALID_ARGUMENT;
    if (frame < document.timeline.range.start_frame ||
        (uint64_t)(frame - document.timeline.range.start_frame) >=
            document.timeline.range.frame_count) return TIMELINE_STATUS_FRAME_OUT_OF_RANGE;
    candidate = *current;
    memset(&key, 0, sizeof(key));
    key.frame = frame;
    key.value = TimelineValueScalar(progress);
    key.interpolation_to_next = TIMELINE_INTERPOLATION_LINEAR;
    TimelineStatus status = TimelineTrackInsertKey(&candidate, key, &index);
    if (status != TIMELINE_STATUS_OK) return status;
    status = commit_track(&document, &candidate);
    if (status != TIMELINE_STATUS_OK) return status;
    history_push(g_light_timeline.undo_tracks, &g_light_timeline.undo_count, current);
    g_light_timeline.redo_count = 0u;
    g_light_timeline.selected_key_index = (int)index;
    return TIMELINE_STATUS_OK;
}

TimelineStatus SceneEditorLightTimelineDeleteSelectedKey(void) {
    RuntimeSceneLightTimelineDocument document;
    TimelineTrack* current = NULL;
    TimelineTrack candidate;
    int index = g_light_timeline.selected_key_index;
    if (!current_document(&document, &current) || index < 0 ||
        (size_t)index >= current->key_count) return TIMELINE_STATUS_INVALID_ARGUMENT;
    if (current->key_count <= 2u) return TIMELINE_STATUS_INVALID_TRACK;
    candidate = *current;
    TimelineStatus status = TimelineTrackRemoveKey(&candidate, (size_t)index);
    if (status != TIMELINE_STATUS_OK) return status;
    status = commit_track(&document, &candidate);
    if (status != TIMELINE_STATUS_OK) return status;
    history_push(g_light_timeline.undo_tracks, &g_light_timeline.undo_count, current);
    g_light_timeline.redo_count = 0u;
    if ((size_t)index >= candidate.key_count) index = (int)candidate.key_count - 1;
    g_light_timeline.selected_key_index = index;
    return TIMELINE_STATUS_OK;
}

static bool history_restore(bool undo) {
    RuntimeSceneLightTimelineDocument document;
    TimelineTrack* current = NULL;
    TimelineTrack* source = undo ? g_light_timeline.undo_tracks : g_light_timeline.redo_tracks;
    size_t* source_count = undo ? &g_light_timeline.undo_count : &g_light_timeline.redo_count;
    TimelineTrack* destination = undo ? g_light_timeline.redo_tracks : g_light_timeline.undo_tracks;
    size_t* destination_count = undo ? &g_light_timeline.redo_count : &g_light_timeline.undo_count;
    if (*source_count == 0u || !current_document(&document, &current)) return false;
    TimelineTrack restored = source[*source_count - 1u];
    if (commit_track(&document, &restored) != TIMELINE_STATUS_OK) return false;
    history_push(destination, destination_count, current);
    *source_count -= 1u;
    if (g_light_timeline.selected_key_index >= (int)restored.key_count) {
        g_light_timeline.selected_key_index = (int)restored.key_count - 1;
    }
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
    history_push(g_light_timeline.undo_tracks,
                 &g_light_timeline.undo_count, current);
    g_light_timeline.redo_count = 0u;
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
    RuntimeSceneBridge3DLightSeedState lights;
    RuntimeSceneLightTimelineDocument document;
    SceneEditorLightTimelineReset();
    runtime_scene_bridge_get_last_3d_light_seed_state(&lights);
    if (!RuntimeSceneLightTimelineGetLast(&document) ||
        document.progress_track_index >= document.timeline.track_count) return;
    const char* target = document.timeline.tracks[document.progress_track_index].target_id;
    const char* id = strncmp(target, "light/", 6u) == 0 ? target + 6 : NULL;
    if (!id || !id[0]) return;
    for (int i = 0; i < lights.light_count; ++i) {
        if (strcmp(lights.lights[i].id, id) == 0) {
            g_light_timeline.selected_light_index = i;
            g_light_timeline.current_frame = document.timeline.range.start_frame;
            g_light_timeline.playback_frame_fraction = 0.0;
            (void)capture_current_sample();
            return;
        }
    }
}

bool SceneEditorLightTimelineHasSelectedLight(void) {
    return g_light_timeline.selected_light_index >= 0;
}

TimelineStatus SceneEditorLightTimelineSelectTargetId(const char* target_id) {
    RuntimeSceneBridge3DLightSeedState lights;
    RuntimeSceneLightTimelineTarget target;
    runtime_scene_bridge_get_last_3d_light_seed_state(&lights);
    TimelineStatus status = RuntimeSceneLightTimelineResolveTarget(
        lights.lights, (size_t)lights.light_count, target_id, &target);
    if (status != TIMELINE_STATUS_OK) return status;
    g_light_timeline.selected_light_index = (int)target.light_index;
    return TIMELINE_STATUS_OK;
}

const char* SceneEditorLightTimelineSelectedTargetId(void) {
    static char target[TIMELINE_ID_CAPACITY];
    RuntimeSceneBridge3DLightSeedState lights;
    target[0] = '\0';
    runtime_scene_bridge_get_last_3d_light_seed_state(&lights);
    if (g_light_timeline.selected_light_index < 0 ||
        g_light_timeline.selected_light_index >= lights.light_count ||
        !lights.lights[g_light_timeline.selected_light_index].id[0]) return target;
    snprintf(target, sizeof(target), "light/%s",
             lights.lights[g_light_timeline.selected_light_index].id);
    return target;
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
        return scene_editor_pane_host_set_timeline_visible(pane_host, false);
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
    return scene_editor_pane_host_set_timeline_visible(pane_host, true);
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

static void graph_to_key_values(const RuntimeSceneLightTimelineDocument* document,
                                const SDL_Rect* graph, int x, int y,
                                int64_t* out_frame, double* out_value) {
    double nx = scene_editor_light_timeline_view_normalized_at_x(
        &g_light_timeline.view, graph, x);
    double ny = graph->h > 0 ? (double)(y - graph->y) / (double)graph->h : 0.0;
    if (ny < 0.0) ny = 0.0;
    if (ny > 1.0) ny = 1.0;
    if (out_frame) {
        *out_frame = document->timeline.range.start_frame +
            (int64_t)llround(nx * (double)(document->timeline.range.frame_count - 1u));
    }
    if (out_value) *out_value = 1.0 - ny;
}

static bool move_selected_key(int x, int y, const SDL_Rect* graph) {
    RuntimeSceneLightTimelineDocument document;
    TimelineTrack* current = NULL;
    TimelineTrack candidate;
    int64_t frame = 0;
    double value = 0.0;
    int index = g_light_timeline.selected_key_index;
    if (!current_document(&document, &current) || index < 0 ||
        (size_t)index >= current->key_count) return false;
    graph_to_key_values(&document, graph, x, y, &frame, &value);
    if (index == 0) {
        frame = current->keys[0].frame;
        value = current->keys[0].value.as.scalar;
    } else if ((size_t)index + 1u == current->key_count) {
        frame = current->keys[index].frame;
        value = current->keys[index].value.as.scalar;
    } else {
        int64_t minimum_frame = current->keys[index - 1].frame + 1;
        int64_t maximum_frame = current->keys[index + 1].frame - 1;
        double minimum_progress =
            current->keys[index - 1].value.as.scalar;
        double maximum_progress =
            current->keys[index + 1].value.as.scalar;
        if (frame < minimum_frame) frame = minimum_frame;
        if (frame > maximum_frame) frame = maximum_frame;
        if (value < minimum_progress) value = minimum_progress;
        if (value > maximum_progress) value = maximum_progress;
        value = scene_editor_light_timeline_snap_progress_to_anchor(
            &document, value,
            graph->h > 0 ? 8.0 / (double)graph->h : 0.0,
            NULL);
        if (value < minimum_progress) value = minimum_progress;
        if (value > maximum_progress) value = maximum_progress;
    }
    candidate = *current;
    if (TimelineTrackMoveScalarKey(&candidate, (size_t)index, frame, value) !=
        TIMELINE_STATUS_OK) return false;
    return commit_track(&document, &candidate) == TIMELINE_STATUS_OK;
}

static bool insert_key_at_graph_position(
    const SDL_Rect* graph,
    int x,
    int y,
    bool preserve_shape) {
    RuntimeSceneLightTimelineDocument document;
    TimelineTrack* track = NULL;
    double normalized;
    double progress = 0.0;
    int64_t requested;
    int64_t frame;
    if (!graph || !current_document(&document, &track)) return false;
    normalized = scene_editor_light_timeline_view_normalized_at_x(
        &g_light_timeline.view, graph, x);
    requested = document.timeline.range.start_frame +
        (int64_t)llround(normalized *
            (double)(document.timeline.range.frame_count - 1u));
    if (!scene_editor_light_timeline_nearest_open_frame(
            &document, track, requested, &frame)) {
        return false;
    }
    if (preserve_shape) {
        if (!scene_editor_light_timeline_evaluate_progress_at_frame(
                &document, track, frame, &progress)) {
            return false;
        }
    } else {
        size_t right = 0u;
        progress = scene_editor_light_timeline_progress_at_y(graph, y);
        progress = scene_editor_light_timeline_snap_progress_to_anchor(
            &document, progress,
            graph->h > 0 ? 8.0 / (double)graph->h : 0.0,
            NULL);
        while (right < track->key_count &&
               track->keys[right].frame < frame) {
            right += 1u;
        }
        if (right == 0u || right >= track->key_count) return false;
        if (progress < track->keys[right - 1u].value.as.scalar) {
            progress = track->keys[right - 1u].value.as.scalar;
        }
        if (progress > track->keys[right].value.as.scalar) {
            progress = track->keys[right].value.as.scalar;
        }
    }
    return SceneEditorLightTimelineInsertKey(frame, progress) ==
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
        if (event->type == SDL_KEYDOWN && current_document(&document, &track)) {
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
            current_document(&document, &track)) {
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
            if (current_document(&document, &track)) {
                if (point_in_rect(
                        event->button.x, event->button.y,
                        &geometry.constant_speed_button)) {
                    (void)apply_traversal_mode(
                        SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_CONSTANT_SPEED);
                    return true;
                }
                if (point_in_rect(
                        event->button.x, event->button.y,
                        &geometry.equal_segments_button)) {
                    (void)apply_traversal_mode(
                        SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_EQUAL_SEGMENTS);
                    return true;
                }
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
                {
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
                            history_push(
                                g_light_timeline.undo_tracks,
                                &g_light_timeline.undo_count, track);
                            g_light_timeline.redo_count = 0u;
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
                int key_index = scene_editor_light_timeline_pick_key(
                    &g_light_timeline.view, &document, track, &graph,
                    event->button.x, event->button.y);
                if (key_index >= 0) {
                    stop_playback();
                    g_light_timeline.selected_key_index = key_index;
                    g_light_timeline.selected_path_point_index = -1;
                    history_push(g_light_timeline.undo_tracks,
                                 &g_light_timeline.undo_count, track);
                    g_light_timeline.redo_count = 0u;
                    g_light_timeline.dragging_key = true;
                    return true;
                }
            }
            if (current_document(&document, &track) &&
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
            g_light_timeline.dragging_handle = 0;
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
        SceneEditorDigestOverlayProjector projector;
        runtime_scene_bridge_get_last_3d_digest_state(&digest);
        if (digest.valid && SceneEditorDigestOverlayBuildProjector(
                                &digest, &layout->viewport_rect, nav_state, &projector)) {
            int picked = pick_light(&projector, event->button.x, event->button.y);
            if (picked >= 0) {
                g_light_timeline.selected_light_index = picked;
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
    runtime_scene_bridge_get_last_3d_light_seed_state(&lights);
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
        int radius = (i == g_light_timeline.selected_light_index) ? 8 : 6;
        SDL_Rect marker = {x - radius, y - radius, radius * 2, radius * 2};
        SDL_Color color = (i == g_light_timeline.selected_light_index)
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
