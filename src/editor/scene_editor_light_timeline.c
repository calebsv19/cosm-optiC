#include "editor/scene_editor_light_timeline.h"

#include "animation/timeline_document.h"
#include "config/config_manager.h"
#include "import/runtime_scene_bridge.h"
#include "import/runtime_scene_light_timeline_io.h"
#include "import/runtime_scene_light_timeline_bridge.h"
#include "render/font_runtime.h"
#include "render/text_draw.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct SceneEditorLightTimelineState {
    int selected_light_index;
    int hovered_light_index;
    int64_t current_frame;
    bool scrubbing;
    bool dragging_key;
    int dragging_handle;
    int selected_key_index;
    TimelineLightMotionSample sample;
    TimelineTrack undo_tracks[24];
    size_t undo_count;
    TimelineTrack redo_tracks[24];
    size_t redo_count;
} SceneEditorLightTimelineState;

static SceneEditorLightTimelineState g_light_timeline = {.selected_light_index = -1,
                                                         .hovered_light_index = -1,
                                                         .selected_key_index = -1};

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
        if (!lights.lights[i].id[0] ||
            !SceneEditorDigestOverlayProjectPoint(projector,
                                                  lights.lights[i].position.x,
                                                  lights.lights[i].position.y,
                                                  lights.lights[i].position.z,
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
        (void)runtime_scene_bridge_apply_light_timeline_sample(
            (TimelineSample){g_light_timeline.current_frame, 0u, 1u},
            &g_light_timeline.sample);
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
           g_light_timeline.dragging_handle != 0;
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
        (void)runtime_scene_bridge_apply_light_timeline_sample(
            (TimelineSample){g_light_timeline.current_frame, 0u, 1u},
            &g_light_timeline.sample);
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
            (void)runtime_scene_bridge_apply_light_timeline_sample(
                (TimelineSample){g_light_timeline.current_frame, 0u, 1u},
                &g_light_timeline.sample);
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
    int frame_count = animSettings.frameLimit > 1 ? animSettings.frameLimit : 101;
    if (!target[0] || sceneSettings.bezierPath.numPoints < 2) return false;
    memset(&document, 0, sizeof(document));
    memset(&track, 0, sizeof(track));
    if (TimelineDocumentInit(&document.timeline, (TimelineRate){(uint32_t)fps, 1u},
                             (TimelineRange){0, (uint64_t)frame_count}) != TIMELINE_STATUS_OK ||
        TimelineTrackInit(&track, "selected_light_progress", target,
                          "light/path_progress", TIMELINE_VALUE_SCALAR) != TIMELINE_STATUS_OK ||
        TimelineTrackSetUnit(&track, TIMELINE_UNIT_UNITLESS) != TIMELINE_STATUS_OK ||
        TimelineTrackAddKey(&track, 0, TimelineValueScalar(0.0),
                            TIMELINE_INTERPOLATION_LINEAR) != TIMELINE_STATUS_OK ||
        TimelineTrackAddKey(&track, frame_count - 1, TimelineValueScalar(1.0),
                            TIMELINE_INTERPOLATION_STEP) != TIMELINE_STATUS_OK ||
        TimelineDocumentAddTrack(&document.timeline, &track) != TIMELINE_STATUS_OK) return false;
    document.progress_track_index = 0u;
    document.spatial_path = sceneSettings.bezierPath;
    document.spatial_path_3d = sceneSettings.bezierPath3D;
    document.valid = true;
    return RuntimeSceneLightTimelineSetLast(&document) == TIMELINE_STATUS_OK;
}

bool SceneEditorLightTimelineToggle(SceneEditorPaneHost* pane_host) {
    RuntimeSceneLightTimelineDocument document;
    if (!pane_host || !SceneEditorLightTimelineHasSelectedLight()) return false;
    if (scene_editor_pane_host_timeline_visible(pane_host)) {
        g_light_timeline.scrubbing = false;
        return scene_editor_pane_host_set_timeline_visible(pane_host, false);
    }
    if (!RuntimeSceneLightTimelineGetLast(&document)) {
        if (!create_default_document_for_selected()) return false;
    } else {
        const char* target = document.timeline.tracks[document.progress_track_index].target_id;
        if (strcmp(target, SceneEditorLightTimelineSelectedTargetId()) != 0) return false;
    }
    if (!RuntimeSceneLightTimelineGetLast(&document)) return false;
    g_light_timeline.current_frame = document.timeline.range.start_frame;
    (void)runtime_scene_bridge_apply_light_timeline_sample(
        (TimelineSample){g_light_timeline.current_frame, 0u, 1u},
        &g_light_timeline.sample);
    return scene_editor_pane_host_set_timeline_visible(pane_host, true);
}

static bool scrub_to_x(int x, const SDL_Rect* rect) {
    RuntimeSceneLightTimelineDocument document;
    double normalized;
    int64_t frame;
    if (!rect || rect->w <= 32 || !RuntimeSceneLightTimelineGetLast(&document)) return false;
    normalized = ((double)x - (double)(rect->x + 16)) / (double)(rect->w - 32);
    if (normalized < 0.0) normalized = 0.0;
    if (normalized > 1.0) normalized = 1.0;
    frame = document.timeline.range.start_frame +
            (int64_t)llround(normalized * (double)(document.timeline.range.frame_count - 1u));
    if (runtime_scene_bridge_apply_light_timeline_sample(
            (TimelineSample){frame, 0u, 1u}, &g_light_timeline.sample) !=
        TIMELINE_STATUS_OK) return false;
    g_light_timeline.current_frame = frame;
    return true;
}

static SDL_Rect timeline_graph_rect(const SDL_Rect* rect) {
    if (!rect) return (SDL_Rect){0, 0, 0, 0};
    return (SDL_Rect){rect->x + 16, rect->y + 40,
                      rect->w > 32 ? rect->w - 32 : 0,
                      rect->h > 58 ? rect->h - 58 : 0};
}

static void graph_to_key_values(const RuntimeSceneLightTimelineDocument* document,
                                const SDL_Rect* graph, int x, int y,
                                int64_t* out_frame, double* out_value) {
    double nx = graph->w > 0 ? (double)(x - graph->x) / (double)graph->w : 0.0;
    double ny = graph->h > 0 ? (double)(y - graph->y) / (double)graph->h : 0.0;
    if (nx < 0.0) nx = 0.0;
    if (nx > 1.0) nx = 1.0;
    if (ny < 0.0) ny = 0.0;
    if (ny > 1.0) ny = 1.0;
    if (out_frame) {
        *out_frame = document->timeline.range.start_frame +
            (int64_t)llround(nx * (double)(document->timeline.range.frame_count - 1u));
    }
    if (out_value) *out_value = 1.0 - ny;
}

static void key_to_graph(const RuntimeSceneLightTimelineDocument* document,
                         const SDL_Rect* graph, const TimelineKeyframe* key,
                         int* out_x, int* out_y) {
    double nx = (double)(key->frame - document->timeline.range.start_frame) /
                (double)(document->timeline.range.frame_count - 1u);
    if (out_x) *out_x = graph->x + (int)llround(nx * graph->w);
    if (out_y) *out_y = graph->y + graph->h -
                        (int)llround(key->value.as.scalar * graph->h);
}

static int pick_key_index(const RuntimeSceneLightTimelineDocument* document,
                          const TimelineTrack* track, const SDL_Rect* graph,
                          int x, int y) {
    int best = -1;
    double best_distance = 10.0 * 10.0;
    for (size_t i = 0u; i < track->key_count; ++i) {
        int key_x = 0, key_y = 0;
        key_to_graph(document, graph, &track->keys[i], &key_x, &key_y);
        double dx = (double)key_x - x, dy = (double)key_y - y;
        double distance = dx * dx + dy * dy;
        if (distance <= best_distance) { best_distance = distance; best = (int)i; }
    }
    return best;
}

static int pick_selected_handle(const RuntimeSceneLightTimelineDocument* document,
                                const TimelineTrack* track, const SDL_Rect* graph,
                                int x, int y) {
    int index = g_light_timeline.selected_key_index;
    if (index < 0 || (size_t)index >= track->key_count) return 0;
    const TimelineKeyframe* key = &track->keys[index];
    const double frame_scale = graph->w /
        (double)(document->timeline.range.frame_count - 1u);
    int key_x = 0, key_y = 0;
    key_to_graph(document, graph, key, &key_x, &key_y);
    if (index > 0 && track->keys[index - 1].interpolation_to_next ==
                         TIMELINE_INTERPOLATION_CUBIC_BEZIER) {
        int hx = key_x + (int)llround(key->incoming_frame_offset * frame_scale);
        int hy = key_y - (int)llround(key->incoming_value_offset * graph->h);
        double dx = (double)hx - x, dy = (double)hy - y;
        if (dx * dx + dy * dy <= 9.0 * 9.0) return -1;
    }
    if ((size_t)index + 1u < track->key_count &&
        key->interpolation_to_next == TIMELINE_INTERPOLATION_CUBIC_BEZIER) {
        int hx = key_x + (int)llround(key->outgoing_frame_offset * frame_scale);
        int hy = key_y - (int)llround(key->outgoing_value_offset * graph->h);
        double dx = (double)hx - x, dy = (double)hy - y;
        if (dx * dx + dy * dy <= 9.0 * 9.0) return 1;
    }
    return 0;
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
    if (index == 0) frame = current->keys[0].frame;
    if ((size_t)index + 1u == current->key_count) frame = current->keys[index].frame;
    candidate = *current;
    if (TimelineTrackMoveScalarKey(&candidate, (size_t)index, frame, value) !=
        TIMELINE_STATUS_OK) return false;
    return commit_track(&document, &candidate) == TIMELINE_STATUS_OK;
}

static bool move_selected_handle(int x, int y, const SDL_Rect* graph) {
    RuntimeSceneLightTimelineDocument document;
    TimelineTrack* current = NULL;
    TimelineTrack candidate;
    int64_t handle_frame = 0;
    double handle_value = 0.0;
    int index = g_light_timeline.selected_key_index;
    if (!current_document(&document, &current) || index < 0 ||
        (size_t)index >= current->key_count || g_light_timeline.dragging_handle == 0) return false;
    graph_to_key_values(&document, graph, x, y, &handle_frame, &handle_value);
    candidate = *current;
    TimelineKeyframe* key = &candidate.keys[index];
    double frame_offset = (double)(handle_frame - key->frame);
    double value_offset = handle_value - key->value.as.scalar;
    if (g_light_timeline.dragging_handle < 0) {
        if (index <= 0) return false;
        double min_offset = (double)(candidate.keys[index - 1].frame - key->frame);
        if (frame_offset < min_offset) frame_offset = min_offset;
        if (frame_offset > 0.0) frame_offset = 0.0;
        key->incoming_frame_offset = frame_offset;
        key->incoming_value_offset = value_offset;
    } else {
        if ((size_t)index + 1u >= candidate.key_count) return false;
        double max_offset = (double)(candidate.keys[index + 1].frame - key->frame);
        if (frame_offset < 0.0) frame_offset = 0.0;
        if (frame_offset > max_offset) frame_offset = max_offset;
        key->outgoing_frame_offset = frame_offset;
        key->outgoing_value_offset = value_offset;
    }
    return commit_track(&document, &candidate) == TIMELINE_STATUS_OK;
}

static bool set_selected_interpolation(TimelineInterpolation interpolation) {
    RuntimeSceneLightTimelineDocument document;
    TimelineTrack* current = NULL;
    TimelineTrack candidate;
    int index = g_light_timeline.selected_key_index;
    if (!current_document(&document, &current) || index < 0 ||
        (size_t)index + 1u >= current->key_count) return false;
    candidate = *current;
    candidate.keys[index].interpolation_to_next = interpolation;
    if (interpolation == TIMELINE_INTERPOLATION_CUBIC_BEZIER &&
        candidate.keys[index].outgoing_frame_offset == 0.0 &&
        candidate.keys[index + 1].incoming_frame_offset == 0.0) {
        double third = (double)(candidate.keys[index + 1].frame -
                                candidate.keys[index].frame) / 3.0;
        candidate.keys[index].outgoing_frame_offset = third;
        candidate.keys[index + 1].incoming_frame_offset = -third;
    }
    if (commit_track(&document, &candidate) != TIMELINE_STATUS_OK) return false;
    history_push(g_light_timeline.undo_tracks, &g_light_timeline.undo_count, current);
    g_light_timeline.redo_count = 0u;
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
        SDL_Rect graph = timeline_graph_rect(&layout->timeline_rect);
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
            if (event->key.keysym.sym == SDLK_1) return set_selected_interpolation(TIMELINE_INTERPOLATION_STEP);
            if (event->key.keysym.sym == SDLK_2) return set_selected_interpolation(TIMELINE_INTERPOLATION_LINEAR);
            if (event->key.keysym.sym == SDLK_3) return set_selected_interpolation(TIMELINE_INTERPOLATION_CUBIC_BEZIER);
        }
        if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT &&
            point_in_rect(event->button.x, event->button.y, &layout->timeline_rect)) {
            if (current_document(&document, &track)) {
                int handle = pick_selected_handle(&document, track, &graph,
                                                  event->button.x, event->button.y);
                if (handle != 0) {
                    history_push(g_light_timeline.undo_tracks,
                                 &g_light_timeline.undo_count, track);
                    g_light_timeline.redo_count = 0u;
                    g_light_timeline.dragging_handle = handle;
                    return true;
                }
                int key_index = pick_key_index(&document, track, &graph,
                                               event->button.x, event->button.y);
                if (key_index >= 0) {
                    g_light_timeline.selected_key_index = key_index;
                    history_push(g_light_timeline.undo_tracks,
                                 &g_light_timeline.undo_count, track);
                    g_light_timeline.redo_count = 0u;
                    g_light_timeline.dragging_key = true;
                    return true;
                }
                if (event->button.clicks >= 2 && point_in_rect(
                        event->button.x, event->button.y, &graph)) {
                    int64_t frame = 0; double value = 0.0;
                    graph_to_key_values(&document, &graph, event->button.x,
                                        event->button.y, &frame, &value);
                    return SceneEditorLightTimelineInsertKey(frame, value) ==
                           TIMELINE_STATUS_OK;
                }
            }
            g_light_timeline.scrubbing = true;
            return scrub_to_x(event->button.x, &layout->timeline_rect);
        }
        if (event->type == SDL_MOUSEMOTION && g_light_timeline.dragging_key) {
            return move_selected_key(event->motion.x, event->motion.y, &graph);
        }
        if (event->type == SDL_MOUSEMOTION && g_light_timeline.dragging_handle != 0) {
            return move_selected_handle(event->motion.x, event->motion.y, &graph);
        }
        if (event->type == SDL_MOUSEMOTION && g_light_timeline.scrubbing) {
            return scrub_to_x(event->motion.x, &layout->timeline_rect);
        }
        if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT &&
            (g_light_timeline.scrubbing || g_light_timeline.dragging_key ||
             g_light_timeline.dragging_handle != 0)) {
            g_light_timeline.scrubbing = false;
            g_light_timeline.dragging_key = false;
            g_light_timeline.dragging_handle = 0;
            return true;
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
        if (!lights.lights[i].id[0] ||
            !SceneEditorDigestOverlayProjectPoint(projector,
                                                  lights.lights[i].position.x,
                                                  lights.lights[i].position.y,
                                                  lights.lights[i].position.z,
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
    const TimelineTrack* track;
    SDL_Rect rect;
    int graph_x0, graph_x1, graph_y0, graph_y1;
    char label[192];
    sync_spatial_path_from_editor();
    if (!renderer || !layout || !layout->timeline_visible ||
        !RuntimeSceneLightTimelineGetLast(&document)) return;
    rect = layout->timeline_rect;
    track = &document.timeline.tracks[document.progress_track_index];
    SDL_SetRenderDrawColor(renderer, 35, 38, 48, 255);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, 100, 108, 128, 255);
    SDL_RenderDrawRect(renderer, &rect);
    TTF_Font* font = ray_tracing_font_runtime_get_ui_regular(renderer, 14, 11);
    snprintf(label, sizeof(label), "%s  frame %lld  progress %.3f  speed %.3f world/s  |  double-click add, 1/2/3 curve",
             track->target_id, (long long)g_light_timeline.current_frame,
             g_light_timeline.sample.progress,
             g_light_timeline.sample.speed_valid
                 ? g_light_timeline.sample.world_speed_per_second : 0.0);
    if (font) ray_tracing_text_draw_utf8_at(renderer, font, label,
                                            rect.x + 12, rect.y + 8,
                                            (SDL_Color){225, 228, 238, 255});
    graph_x0 = rect.x + 16; graph_x1 = rect.x + rect.w - 16;
    graph_y0 = rect.y + 40; graph_y1 = rect.y + rect.h - 18;
    SDL_SetRenderDrawColor(renderer, 78, 84, 102, 255);
    SDL_RenderDrawLine(renderer, graph_x0, graph_y1, graph_x1, graph_y1);
    SDL_RenderDrawLine(renderer, graph_x0, graph_y0, graph_x0, graph_y1);
    int last_x = graph_x0, last_y = graph_y1;
    for (int x = graph_x0; x <= graph_x1; ++x) {
        double n = (double)(x - graph_x0) / (double)(graph_x1 - graph_x0);
        double frame_pos = (double)document.timeline.range.start_frame +
                           n * (double)(document.timeline.range.frame_count - 1u);
        TimelineEvaluationContext context;
        TimelineEvaluationResult result;
        int64_t frame = (int64_t)floor(frame_pos);
        uint32_t sub = (uint32_t)llround((frame_pos - (double)frame) * 1000000.0);
        if (sub >= 1000000u) { frame += 1; sub = 0u; }
        if (TimelineEvaluationContextBuild(document.timeline.rate, document.timeline.range,
                                           (TimelineSample){frame, sub, 1000000u},
                                           &context) == TIMELINE_STATUS_OK &&
            TimelineTrackEvaluate(track, &context, &result) == TIMELINE_STATUS_OK) {
            int y = graph_y1 - (int)llround(result.value.as.scalar * (graph_y1 - graph_y0));
            SDL_SetRenderDrawColor(renderer, 114, 204, 255, 255);
            if (x > graph_x0) SDL_RenderDrawLine(renderer, last_x, last_y, x, y);
            last_x = x; last_y = y;
        }
    }
    {
        enum { SPEED_SAMPLE_COUNT = 256 };
        double speeds[SPEED_SAMPLE_COUNT];
        double max_speed = 0.0;
        double fps = (double)document.timeline.rate.frames_per_second_numerator /
                     (double)document.timeline.rate.frames_per_second_denominator;
        for (int i = 0; i < SPEED_SAMPLE_COUNT; ++i) {
            double n = (double)i / (double)(SPEED_SAMPLE_COUNT - 1);
            double frame_pos = (double)document.timeline.range.start_frame +
                               n * (double)(document.timeline.range.frame_count - 1u);
            int64_t frame = (int64_t)floor(frame_pos);
            uint32_t sub = (uint32_t)llround((frame_pos - frame) * 1000000.0);
            TimelineEvaluationContext context;
            TimelineEvaluationResult result;
            speeds[i] = 0.0;
            if (sub >= 1000000u) { frame += 1; sub = 0u; }
            if (TimelineEvaluationContextBuild(document.timeline.rate,
                                               document.timeline.range,
                                               (TimelineSample){frame, sub, 1000000u},
                                               &context) == TIMELINE_STATUS_OK &&
                TimelineTrackEvaluate(track, &context, &result) == TIMELINE_STATUS_OK &&
                result.derivative_valid) {
                speeds[i] = fabs(result.derivative_per_frame) *
                            g_light_timeline.sample.path_length_world * fps;
                if (speeds[i] > max_speed) max_speed = speeds[i];
            }
        }
        if (max_speed > 0.0) {
            int prior_x = graph_x0;
            int prior_y = graph_y1;
            SDL_SetRenderDrawColor(renderer, 255, 142, 74, 210);
            for (int i = 0; i < SPEED_SAMPLE_COUNT; ++i) {
                int x = graph_x0 + (int)llround(
                    (double)i / (double)(SPEED_SAMPLE_COUNT - 1) *
                    (graph_x1 - graph_x0));
                int y = graph_y1 - (int)llround((speeds[i] / max_speed) *
                                                (graph_y1 - graph_y0) * 0.35);
                if (i > 0) SDL_RenderDrawLine(renderer, prior_x, prior_y, x, y);
                prior_x = x;
                prior_y = y;
            }
        }
    }
    for (size_t i = 0u; i < track->key_count; ++i) {
        double n = (double)(track->keys[i].frame - document.timeline.range.start_frame) /
                   (double)(document.timeline.range.frame_count - 1u);
        int x = graph_x0 + (int)llround(n * (graph_x1 - graph_x0));
        int y = graph_y1 - (int)llround(track->keys[i].value.as.scalar * (graph_y1 - graph_y0));
        SDL_Rect key = {x - 4, y - 4, 8, 8};
        SDL_SetRenderDrawColor(renderer,
                               (int)i == g_light_timeline.selected_key_index ? 255 : 228,
                               (int)i == g_light_timeline.selected_key_index ? 235 : 190,
                               (int)i == g_light_timeline.selected_key_index ? 128 : 70,
                               255);
        SDL_RenderFillRect(renderer, &key);
    }
    if (g_light_timeline.selected_key_index >= 0 &&
        (size_t)g_light_timeline.selected_key_index < track->key_count) {
        int index = g_light_timeline.selected_key_index;
        const TimelineKeyframe* key = &track->keys[index];
        double frame_scale = (double)(graph_x1 - graph_x0) /
            (double)(document.timeline.range.frame_count - 1u);
        int key_x = 0, key_y = 0;
        key_to_graph(&document, &(SDL_Rect){graph_x0, graph_y0,
                                           graph_x1 - graph_x0,
                                           graph_y1 - graph_y0},
                     key, &key_x, &key_y);
        SDL_SetRenderDrawColor(renderer, 190, 160, 255, 230);
        if (index > 0 && track->keys[index - 1].interpolation_to_next ==
                             TIMELINE_INTERPOLATION_CUBIC_BEZIER) {
            int hx = key_x + (int)llround(key->incoming_frame_offset * frame_scale);
            int hy = key_y - (int)llround(key->incoming_value_offset *
                                          (graph_y1 - graph_y0));
            SDL_Rect handle = {hx - 3, hy - 3, 6, 6};
            SDL_RenderDrawLine(renderer, key_x, key_y, hx, hy);
            SDL_RenderFillRect(renderer, &handle);
        }
        if ((size_t)index + 1u < track->key_count &&
            key->interpolation_to_next == TIMELINE_INTERPOLATION_CUBIC_BEZIER) {
            int hx = key_x + (int)llround(key->outgoing_frame_offset * frame_scale);
            int hy = key_y - (int)llround(key->outgoing_value_offset *
                                          (graph_y1 - graph_y0));
            SDL_Rect handle = {hx - 3, hy - 3, 6, 6};
            SDL_RenderDrawLine(renderer, key_x, key_y, hx, hy);
            SDL_RenderFillRect(renderer, &handle);
        }
    }
    double play_n = (double)(g_light_timeline.current_frame - document.timeline.range.start_frame) /
                    (double)(document.timeline.range.frame_count - 1u);
    int play_x = graph_x0 + (int)llround(play_n * (graph_x1 - graph_x0));
    SDL_SetRenderDrawColor(renderer, 255, 92, 92, 255);
    SDL_RenderDrawLine(renderer, play_x, rect.y + 30, play_x, graph_y1 + 4);
}
