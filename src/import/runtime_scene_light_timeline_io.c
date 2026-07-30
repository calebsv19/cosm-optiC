#include "import/runtime_scene_light_timeline_io.h"

#include "camera/camera_path_3d.h"
#include "config/config_scene_path_io.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static RuntimeSceneLightTimelineDocument g_last_light_timeline;

static void light_timeline_diag(char* out, size_t size, const char* message) {
    if (out && size > 0u) snprintf(out, size, "%s", message ? message : "unknown");
}

static bool json_integer(json_object* owner, const char* key, int64_t* out) {
    json_object* value = NULL;
    if (!owner || !key || !out || !json_object_object_get_ex(owner, key, &value) ||
        !json_object_is_type(value, json_type_int)) return false;
    *out = json_object_get_int64(value);
    return true;
}

static bool json_number(json_object* owner, const char* key, double* out) {
    json_object* value = NULL;
    if (!owner || !key || !out || !json_object_object_get_ex(owner, key, &value) ||
        !(json_object_is_type(value, json_type_int) ||
          json_object_is_type(value, json_type_double))) return false;
    *out = json_object_get_double(value);
    return isfinite(*out);
}

static const char* json_string(json_object* owner, const char* key) {
    json_object* value = NULL;
    if (!owner || !key || !json_object_object_get_ex(owner, key, &value) ||
        !json_object_is_type(value, json_type_string)) return NULL;
    return json_object_get_string(value);
}

static TimelineInterpolation interpolation_from_label(const char* label) {
    if (label && strcmp(label, "step") == 0) return TIMELINE_INTERPOLATION_STEP;
    if (label && strcmp(label, "linear") == 0) return TIMELINE_INTERPOLATION_LINEAR;
    if (label && strcmp(label, "cubic_bezier") == 0) {
        return TIMELINE_INTERPOLATION_CUBIC_BEZIER;
    }
    return (TimelineInterpolation)-1;
}

static void scale_path(Path* path, double scale) {
    if (!path) return;
    for (int i = 0; i < path->numPoints && i < MAX_BEZIER_POINTS; ++i) {
        path->points[i].x *= scale;
        path->points[i].y *= scale;
        if (i < path->numPoints - 1) {
            path->handles[i][0].vx *= scale;
            path->handles[i][0].vy *= scale;
            path->handles[i][1].vx *= scale;
            path->handles[i][1].vy *= scale;
        }
    }
}

static bool parse_handle(json_object* key, const char* name,
                         double* frame_offset, double* value_offset) {
    json_object* handle = NULL;
    if (!json_object_object_get_ex(key, name, &handle)) {
        *frame_offset = 0.0;
        *value_offset = 0.0;
        return true;
    }
    return json_object_is_type(handle, json_type_object) &&
           json_number(handle, "frame_offset", frame_offset) &&
           json_number(handle, "value_offset", value_offset);
}

static TimelineStatus parse_progress_track(json_object* track_obj,
                                           const char* target_id,
                                           TimelineTrack* out_track) {
    json_object* keys = NULL;
    TimelineTrack track;
    const char* track_id = json_string(track_obj, "id");
    if (!track_obj || !target_id || !track_id || !track_id[0] ||
        !json_object_object_get_ex(track_obj, "keys", &keys) ||
        !json_object_is_type(keys, json_type_array) ||
        json_object_array_length(keys) == 0u) {
        return TIMELINE_STATUS_INVALID_TRACK;
    }
    memset(&track, 0, sizeof(track));
    TimelineStatus status = TimelineTrackInit(&track, track_id, target_id,
                                              "light/path_progress",
                                              TIMELINE_VALUE_SCALAR);
    if (status != TIMELINE_STATUS_OK) return status;
    status = TimelineTrackSetUnit(&track, TIMELINE_UNIT_UNITLESS);
    if (status != TIMELINE_STATUS_OK) return status;
    json_object* enabled = NULL;
    if (json_object_object_get_ex(track_obj, "enabled", &enabled)) {
        if (!json_object_is_type(enabled, json_type_boolean)) {
            return TIMELINE_STATUS_INVALID_TRACK;
        }
        track.enabled = json_object_get_boolean(enabled);
    }
    const size_t count = json_object_array_length(keys);
    if (count > TIMELINE_TRACK_KEY_CAPACITY) return TIMELINE_STATUS_CAPACITY_EXCEEDED;
    for (size_t i = 0u; i < count; ++i) {
        json_object* key = json_object_array_get_idx(keys, i);
        int64_t frame = 0;
        double value = 0.0;
        const char* interpolation_label = NULL;
        TimelineInterpolation interpolation;
        double in_frame = 0.0, in_value = 0.0, out_frame = 0.0, out_value = 0.0;
        if (!key || !json_object_is_type(key, json_type_object) ||
            !json_integer(key, "frame", &frame) || !json_number(key, "value", &value)) {
            return TIMELINE_STATUS_INVALID_TRACK;
        }
        interpolation_label = json_string(key, "interpolation");
        interpolation = interpolation_from_label(interpolation_label);
        if ((int)interpolation < 0) return TIMELINE_STATUS_UNSUPPORTED_INTERPOLATION;
        status = TimelineTrackAddKey(&track, frame, TimelineValueScalar(value), interpolation);
        if (status != TIMELINE_STATUS_OK) return status;
        if (!parse_handle(key, "incoming_handle", &in_frame, &in_value) ||
            !parse_handle(key, "outgoing_handle", &out_frame, &out_value)) {
            return TIMELINE_STATUS_INVALID_TRACK;
        }
        status = TimelineTrackSetScalarTemporalHandles(&track, i, in_frame, in_value,
                                                       out_frame, out_value);
        if (status != TIMELINE_STATUS_OK) return status;
    }
    *out_track = track;
    return TIMELINE_STATUS_OK;
}

TimelineStatus RuntimeSceneLightTimelineParseAuthoring(
    json_object* authoring, double world_scale,
    RuntimeSceneLightTimelineDocument* out_document,
    char* out_diagnostics, size_t diagnostics_size) {
    RuntimeSceneLightTimelineDocument candidate;
    json_object *root = NULL, *rate = NULL, *range = NULL, *spatial = NULL;
    json_object *path_obj = NULL, *depth_obj = NULL, *track_obj = NULL;
    int64_t version = 0, fps_n = 0, fps_d = 0, start = 0, count = 0;
    const char* target_id = NULL;
    TimelineTrack progress;
    TimelineStatus status;
    if (!authoring || !out_document || !isfinite(world_scale) || world_scale <= 0.0) {
        light_timeline_diag(out_diagnostics, diagnostics_size, "invalid_input");
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    memset(&candidate, 0, sizeof(candidate));
    if (!json_object_object_get_ex(authoring, "light_timeline", &root)) {
        light_timeline_diag(out_diagnostics, diagnostics_size, "light_timeline_missing");
        return TIMELINE_STATUS_TARGET_NOT_FOUND;
    }
    if (!json_object_is_type(root, json_type_object) ||
        !json_integer(root, "version", &version) ||
        version != RUNTIME_SCENE_LIGHT_TIMELINE_SCHEMA_VERSION ||
        !json_object_object_get_ex(root, "rate", &rate) ||
        !json_object_is_type(rate, json_type_object) ||
        !json_integer(rate, "numerator", &fps_n) ||
        !json_integer(rate, "denominator", &fps_d) ||
        !json_object_object_get_ex(root, "range", &range) ||
        !json_object_is_type(range, json_type_object) ||
        !json_integer(range, "start_frame", &start) ||
        !json_integer(range, "frame_count", &count) ||
        fps_n <= 0 || fps_d <= 0 || fps_n > UINT32_MAX || fps_d > UINT32_MAX ||
        count <= 0) {
        light_timeline_diag(out_diagnostics, diagnostics_size, "invalid_clock_contract");
        return TIMELINE_STATUS_INVALID_RANGE;
    }
    target_id = json_string(root, "target_id");
    if (!target_id || strncmp(target_id, "light/", 6u) != 0 || !target_id[6]) {
        light_timeline_diag(out_diagnostics, diagnostics_size, "invalid_light_target_id");
        return TIMELINE_STATUS_INVALID_ID;
    }
    status = TimelineDocumentInit(&candidate.timeline,
                                  (TimelineRate){(uint32_t)fps_n, (uint32_t)fps_d},
                                  (TimelineRange){start, (uint64_t)count});
    if (status != TIMELINE_STATUS_OK) return status;
    if (!json_object_object_get_ex(root, "progress_track", &track_obj) ||
        !json_object_is_type(track_obj, json_type_object)) {
        light_timeline_diag(out_diagnostics, diagnostics_size, "progress_track_missing");
        return TIMELINE_STATUS_INVALID_TRACK;
    }
    status = parse_progress_track(track_obj, target_id, &progress);
    if (status != TIMELINE_STATUS_OK) return status;
    status = TimelineDocumentAddTrack(&candidate.timeline, &progress);
    if (status != TIMELINE_STATUS_OK) return status;
    candidate.progress_track_index = 0u;
    status = TimelineLightMotionValidateProgressTrack(
        &candidate.timeline.tracks[candidate.progress_track_index],
        &candidate.timeline.range);
    if (status != TIMELINE_STATUS_OK) {
        light_timeline_diag(out_diagnostics, diagnostics_size,
                            "invalid_path_progress_track");
        return status;
    }

    if (json_object_object_get_ex(root, "spatial_path", &spatial) &&
        json_object_is_type(spatial, json_type_object)) {
        (void)json_object_object_get_ex(spatial, "path", &path_obj);
        (void)json_object_object_get_ex(spatial, "depth", &depth_obj);
    } else {
        (void)json_object_object_get_ex(authoring, "light_path", &path_obj);
        (void)json_object_object_get_ex(authoring, "light_path_depth", &depth_obj);
        candidate.migrated_legacy_spatial_path = path_obj != NULL;
    }
    if (!path_obj || !config_scene_load_path_from_json_object(path_obj,
                                                               &candidate.spatial_path,
                                                               false) ||
        candidate.spatial_path.numPoints < 2) {
        light_timeline_diag(out_diagnostics, diagnostics_size, "invalid_spatial_path");
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    if (!depth_obj || !CameraPath3D_LoadFromJsonObject(depth_obj,
                                                       &candidate.spatial_path_3d,
                                                       &candidate.spatial_path,
                                                       false)) {
        CameraPath3D_Reset(&candidate.spatial_path_3d);
        CameraPath3D_SyncDefaults(&candidate.spatial_path_3d,
                                  &candidate.spatial_path, 0.0);
    }
    scale_path(&candidate.spatial_path, world_scale);
    CameraPath3D_ScaleWorldUnits(&candidate.spatial_path_3d,
                                 &candidate.spatial_path, world_scale);
    candidate.valid = true;
    *out_document = candidate;
    light_timeline_diag(out_diagnostics, diagnostics_size,
                        candidate.migrated_legacy_spatial_path ? "ok_legacy_path" : "ok");
    return TIMELINE_STATUS_OK;
}

static json_object* handle_json(double frame_offset, double value_offset) {
    json_object* handle = json_object_new_object();
    if (!handle) return NULL;
    json_object_object_add(handle, "frame_offset", json_object_new_double(frame_offset));
    json_object_object_add(handle, "value_offset", json_object_new_double(value_offset));
    return handle;
}

json_object* RuntimeSceneLightTimelineToJsonObject(
    const RuntimeSceneLightTimelineDocument* document, double world_scale) {
    json_object *root = NULL, *rate = NULL, *range = NULL, *spatial = NULL;
    json_object *track = NULL, *keys = NULL;
    Path authored_path;
    CameraPath3D authored_path3d;
    const TimelineTrack* progress = NULL;
    if (!document || !document->valid || !isfinite(world_scale) || world_scale <= 0.0 ||
        TimelineDocumentValidate(&document->timeline) != TIMELINE_STATUS_OK ||
        document->progress_track_index >= document->timeline.track_count) return NULL;
    progress = &document->timeline.tracks[document->progress_track_index];
    if (TimelineLightMotionValidateProgressTrack(
            progress, &document->timeline.range) != TIMELINE_STATUS_OK) {
        return NULL;
    }
    authored_path = document->spatial_path;
    authored_path3d = document->spatial_path_3d;
    scale_path(&authored_path, 1.0 / world_scale);
    CameraPath3D_ScaleWorldUnits(&authored_path3d, &authored_path, 1.0 / world_scale);
    root = json_object_new_object(); rate = json_object_new_object();
    range = json_object_new_object(); spatial = json_object_new_object();
    track = json_object_new_object(); keys = json_object_new_array();
    if (!root || !rate || !range || !spatial || !track || !keys) goto fail;
    json_object_object_add(root, "version", json_object_new_int(RUNTIME_SCENE_LIGHT_TIMELINE_SCHEMA_VERSION));
    json_object_object_add(rate, "numerator", json_object_new_int64(document->timeline.rate.frames_per_second_numerator));
    json_object_object_add(rate, "denominator", json_object_new_int64(document->timeline.rate.frames_per_second_denominator));
    json_object_object_add(root, "rate", rate); rate = NULL;
    json_object_object_add(range, "start_frame", json_object_new_int64(document->timeline.range.start_frame));
    json_object_object_add(range, "frame_count", json_object_new_int64((int64_t)document->timeline.range.frame_count));
    json_object_object_add(root, "range", range); range = NULL;
    json_object_object_add(root, "target_id", json_object_new_string(progress->target_id));
    json_object_object_add(spatial, "path", config_scene_path_to_json_object(&authored_path));
    json_object_object_add(spatial, "depth", CameraPath3D_ToJsonObject(&authored_path3d, &authored_path));
    json_object_object_add(root, "spatial_path", spatial); spatial = NULL;
    json_object_object_add(track, "id", json_object_new_string(progress->track_id));
    json_object_object_add(track, "enabled", json_object_new_boolean(progress->enabled));
    for (size_t i = 0u; i < progress->key_count; ++i) {
        const TimelineKeyframe* key = &progress->keys[i];
        json_object* key_obj = json_object_new_object();
        if (!key_obj) goto fail;
        json_object_object_add(key_obj, "frame", json_object_new_int64(key->frame));
        json_object_object_add(key_obj, "value", json_object_new_double(key->value.as.scalar));
        json_object_object_add(key_obj, "interpolation", json_object_new_string(TimelineInterpolationLabel(key->interpolation_to_next)));
        json_object_object_add(key_obj, "incoming_handle", handle_json(key->incoming_frame_offset, key->incoming_value_offset));
        json_object_object_add(key_obj, "outgoing_handle", handle_json(key->outgoing_frame_offset, key->outgoing_value_offset));
        json_object_array_add(keys, key_obj);
    }
    json_object_object_add(track, "keys", keys); keys = NULL;
    json_object_object_add(root, "progress_track", track); track = NULL;
    return root;
fail:
    if (rate) json_object_put(rate); if (range) json_object_put(range);
    if (spatial) json_object_put(spatial); if (track) json_object_put(track);
    if (keys) json_object_put(keys); if (root) json_object_put(root);
    return NULL;
}

TimelineStatus RuntimeSceneLightTimelineEvaluate(
    const RuntimeSceneLightTimelineDocument* document, TimelineSample sample,
    TimelineLightMotionSample* out_sample) {
    TimelineEvaluationContext context;
    TimelineStatus status;
    if (!document || !document->valid || !out_sample ||
        document->progress_track_index >= document->timeline.track_count) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    status = TimelineEvaluationContextBuild(document->timeline.rate,
                                            document->timeline.range,
                                            sample, &context);
    if (status != TIMELINE_STATUS_OK) return status;
    return TimelineLightMotionEvaluate(
        &document->timeline.tracks[document->progress_track_index],
        &document->spatial_path, &document->spatial_path_3d, &context, out_sample);
}

void RuntimeSceneLightTimelineResetLast(void) {
    memset(&g_last_light_timeline, 0, sizeof(g_last_light_timeline));
}

TimelineStatus RuntimeSceneLightTimelineApplyAuthoring(
    json_object* authoring, double world_scale,
    char* out_diagnostics, size_t diagnostics_size) {
    RuntimeSceneLightTimelineDocument candidate;
    TimelineStatus status;
    memset(&candidate, 0, sizeof(candidate));
    RuntimeSceneLightTimelineResetLast();
    status = RuntimeSceneLightTimelineParseAuthoring(authoring, world_scale,
                                                     &candidate,
                                                     out_diagnostics,
                                                     diagnostics_size);
    if (status == TIMELINE_STATUS_TARGET_NOT_FOUND) return status;
    if (status != TIMELINE_STATUS_OK) return status;
    g_last_light_timeline = candidate;
    return TIMELINE_STATUS_OK;
}

bool RuntimeSceneLightTimelineGetLast(RuntimeSceneLightTimelineDocument* out_document) {
    if (!out_document || !g_last_light_timeline.valid) return false;
    *out_document = g_last_light_timeline;
    return true;
}

TimelineStatus RuntimeSceneLightTimelineSetLast(
    const RuntimeSceneLightTimelineDocument* document) {
    if (!document || !document->valid ||
        document->progress_track_index >= document->timeline.track_count) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    TimelineStatus status = TimelineDocumentValidate(&document->timeline);
    if (status != TIMELINE_STATUS_OK) return status;
    status = TimelineLightMotionValidateProgressTrack(
        &document->timeline.tracks[document->progress_track_index],
        &document->timeline.range);
    if (status != TIMELINE_STATUS_OK) return status;
    if (document->spatial_path.numPoints < 2) return TIMELINE_STATUS_INVALID_ARGUMENT;
    g_last_light_timeline = *document;
    return TIMELINE_STATUS_OK;
}

TimelineStatus RuntimeSceneLightTimelineInspectLast(
    TimelineSample sample, TimelineLightMotionSample* out_sample) {
    if (!g_last_light_timeline.valid) return TIMELINE_STATUS_TARGET_NOT_FOUND;
    return RuntimeSceneLightTimelineEvaluate(&g_last_light_timeline, sample, out_sample);
}
