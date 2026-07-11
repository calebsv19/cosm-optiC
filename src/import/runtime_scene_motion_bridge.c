#include "import/runtime_scene_motion_bridge.h"

#include "config/config_scene_path_io.h"
#include "import/runtime_scene_bridge.h"
#include "scene/object_manager.h"

#include <json-c/json.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static RuntimeMotionTrack3DSummary g_last_object_motion_summary = {0};

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const char *json_string_field(json_object *owner, const char *key) {
    json_object *field = NULL;
    if (!owner || !key ||
        !json_object_object_get_ex(owner, key, &field) ||
        !json_object_is_type(field, json_type_string)) {
        return NULL;
    }
    return json_object_get_string(field);
}

static bool json_bool_field_default(json_object *owner, const char *key, bool fallback) {
    json_object *field = NULL;
    if (!owner || !key ||
        !json_object_object_get_ex(owner, key, &field) ||
        !json_object_is_type(field, json_type_boolean)) {
        return fallback;
    }
    return json_object_get_boolean(field);
}

static bool json_double_field(json_object *owner, const char *key, double *out_value) {
    json_object *field = NULL;
    if (!owner || !key || !out_value ||
        !json_object_object_get_ex(owner, key, &field) ||
        !(json_object_is_type(field, json_type_double) ||
          json_object_is_type(field, json_type_int))) {
        return false;
    }
    *out_value = json_object_get_double(field);
    return true;
}

static const char *json_nested_string_field(json_object *owner,
                                            const char *object_key,
                                            const char *field_key) {
    json_object *nested = NULL;
    if (!owner || !object_key || !field_key ||
        !json_object_object_get_ex(owner, object_key, &nested) ||
        !json_object_is_type(nested, json_type_object)) {
        return NULL;
    }
    return json_string_field(nested, field_key);
}

static bool runtime_scene_motion_bridge_object_id_exists(const char *object_id) {
    char candidate[RUNTIME_MOTION_TRACK_3D_OBJECT_ID_SIZE];
    if (!object_id || !object_id[0]) {
        return false;
    }
    for (int i = 0; i < MAX_OBJECTS; ++i) {
        if (!runtime_scene_bridge_get_last_object_id_for_scene_index(
                i,
                candidate,
                sizeof(candidate))) {
            continue;
        }
        if (strcmp(candidate, object_id) == 0) {
            return true;
        }
    }
    return false;
}

static bool runtime_scene_motion_bridge_is_duplicate_enabled_object(
    const RuntimeMotionTrack3DSummary *summary,
    const char *object_id) {
    if (!summary || !object_id || !object_id[0]) {
        return false;
    }
    for (int i = 0; i < summary->stored_tracks; ++i) {
        const RuntimeMotionTrack3D *track = &summary->tracks[i];
        if (track->used && track->enabled && strcmp(track->object_id, object_id) == 0) {
            return true;
        }
    }
    return false;
}

static void runtime_scene_motion_bridge_scale_path(Path *path, double world_scale) {
    if (!path) return;
    for (int i = 0; i < path->numPoints && i < MAX_BEZIER_POINTS; ++i) {
        path->points[i].x *= world_scale;
        path->points[i].y *= world_scale;
        if (i < MAX_BEZIER_POINTS - 1) {
            path->handles[i][0].vx *= world_scale;
            path->handles[i][0].vy *= world_scale;
            path->handles[i][1].vx *= world_scale;
            path->handles[i][1].vy *= world_scale;
        }
    }
}

static bool runtime_scene_motion_bridge_parse_position_path(json_object *entry,
                                                            double world_scale,
                                                            RuntimeMotionTrack3D *track) {
    json_object *path_obj = NULL;
    json_object *points = NULL;
    Path path = {0};
    CameraPath3D path3d = {0};
    if (!entry || !track ||
        !json_object_object_get_ex(entry, "path", &path_obj) ||
        !json_object_is_type(path_obj, json_type_object)) {
        return false;
    }
    if (!config_scene_load_path_from_json_object(path_obj, &path, true) ||
        path.numPoints <= 0) {
        return false;
    }
    runtime_scene_motion_bridge_scale_path(&path, world_scale);
    CameraPath3D_Reset(&path3d);
    CameraPath3D_SyncDefaults(&path3d, &path, 0.0);
    if (json_object_object_get_ex(path_obj, "points", &points) &&
        json_object_is_type(points, json_type_array)) {
        int point_count = json_object_array_length(points);
        if (point_count > path.numPoints) point_count = path.numPoints;
        for (int i = 0; i < point_count; ++i) {
            json_object *point = json_object_array_get_idx(points, i);
            double z = 0.0;
            if (json_double_field(point, "z", &z)) {
                path3d.point_z[i] = z * world_scale;
            }
            if (i < point_count - 1) {
                json_object *velocity1 = NULL;
                double vz = 0.0;
                if (json_object_object_get_ex(point, "velocity1", &velocity1) &&
                    json_object_is_type(velocity1, json_type_object) &&
                    json_double_field(velocity1, "vz", &vz)) {
                    path3d.handles_vz[i][0] = vz * world_scale;
                }
            }
            if (i > 0) {
                json_object *velocity2 = NULL;
                double vz = 0.0;
                if (json_object_object_get_ex(point, "velocity2", &velocity2) &&
                    json_object_is_type(velocity2, json_type_object) &&
                    json_double_field(velocity2, "vz", &vz)) {
                    path3d.handles_vz[i - 1][1] = vz * world_scale;
                }
            }
        }
    }
    track->position_path = path;
    track->position_path_3d = path3d;
    track->has_position_path = true;
    return true;
}

static bool runtime_scene_motion_bridge_parse_rotation_keyframes(
    json_object *entry,
    RuntimeMotionTrack3D *track) {
    json_object *keyframes = NULL;
    size_t keyframe_count = 0u;
    if (!entry || !track ||
        !json_object_object_get_ex(entry, "rotation_keyframes", &keyframes) ||
        !json_object_is_type(keyframes, json_type_array)) {
        return false;
    }
    keyframe_count = json_object_array_length(keyframes);
    if (keyframe_count == 0u) {
        return false;
    }
    if (keyframe_count > RUNTIME_MOTION_TRACK_3D_MAX_ROTATION_KEYFRAMES) {
        keyframe_count = RUNTIME_MOTION_TRACK_3D_MAX_ROTATION_KEYFRAMES;
    }
    for (size_t i = 0u; i < keyframe_count; ++i) {
        json_object *keyframe = json_object_array_get_idx(keyframes, i);
        RuntimeMotionTrack3DRotationKeyframe *dst =
            &track->rotation_keyframes[track->rotation_keyframe_count];
        double yaw_degrees = 0.0;
        double pitch_degrees = 0.0;
        double roll_degrees = 0.0;
        if (!keyframe || !json_object_is_type(keyframe, json_type_object)) {
            continue;
        }
        (void)json_double_field(keyframe, "t", &dst->t);
        if (dst->t < 0.0) dst->t = 0.0;
        if (dst->t > 1.0) dst->t = 1.0;
        (void)json_double_field(keyframe, "yaw_degrees", &yaw_degrees);
        (void)json_double_field(keyframe, "pitch_degrees", &pitch_degrees);
        (void)json_double_field(keyframe, "roll_degrees", &roll_degrees);
        dst->yaw_radians = yaw_degrees * M_PI / 180.0;
        dst->pitch_radians = pitch_degrees * M_PI / 180.0;
        dst->roll_radians = roll_degrees * M_PI / 180.0;
        track->rotation_keyframe_count += 1;
    }
    track->has_rotation_keyframes = track->rotation_keyframe_count > 0;
    return track->has_rotation_keyframes;
}

void runtime_scene_motion_bridge_reset(void) {
    RuntimeMotionTrack3DSummaryInit(&g_last_object_motion_summary,
                                    "object_motion_tracks_missing");
}

void runtime_scene_motion_bridge_apply_authoring(json_object *authoring,
                                                 double world_scale) {
    json_object *tracks = NULL;
    size_t track_count = 0u;

    runtime_scene_motion_bridge_reset();
    if (!authoring || !json_object_is_type(authoring, json_type_object)) {
        return;
    }
    if (!json_object_object_get_ex(authoring, "object_motion_tracks", &tracks)) {
        return;
    }

    g_last_object_motion_summary.has_object_motion_tracks = true;
    if (!json_object_is_type(tracks, json_type_array)) {
        g_last_object_motion_summary.valid = false;
        RuntimeMotionTrack3DSummarySetDiagnostics(&g_last_object_motion_summary,
                                                  "object_motion_tracks_not_array");
        return;
    }

    track_count = json_object_array_length(tracks);
    g_last_object_motion_summary.total_tracks = (int)track_count;
    RuntimeMotionTrack3DSummarySetDiagnostics(&g_last_object_motion_summary,
                                              track_count > RUNTIME_MOTION_TRACK_3D_MAX_TRACKS
                                                  ? "object_motion_tracks_truncated"
                                                  : "ok");

    for (size_t i = 0u; i < track_count; ++i) {
        json_object *entry = json_object_array_get_idx(tracks, i);
        const char *object_id = NULL;
        const char *mode_label = NULL;
        const char *timing_domain = NULL;
        const char *wrap_label = NULL;
        RuntimeMotionTrack3DMode mode = RUNTIME_MOTION_TRACK_3D_MODE_UNKNOWN;
        bool enabled = true;
        bool matched = false;
        bool duplicate = false;
        bool supported = false;
        RuntimeMotionTrack3D *track = NULL;

        if (!entry || !json_object_is_type(entry, json_type_object)) {
            if (g_last_object_motion_summary.stored_tracks <
                RUNTIME_MOTION_TRACK_3D_MAX_TRACKS) {
                track = &g_last_object_motion_summary
                             .tracks[g_last_object_motion_summary.stored_tracks++];
                track->used = true;
                track->enabled = true;
                RuntimeMotionTrack3DCopyString(track->mode_label,
                                               sizeof(track->mode_label),
                                               "invalid");
            }
            g_last_object_motion_summary.enabled_tracks += 1;
            g_last_object_motion_summary.unsupported_tracks += 1;
            continue;
        }

        object_id = json_string_field(entry, "object_id");
        mode_label = json_string_field(entry, "mode");
        timing_domain = json_nested_string_field(entry, "timing", "domain");
        if (!timing_domain) timing_domain = json_string_field(entry, "timing_domain");
        wrap_label = json_nested_string_field(entry, "timing", "wrap");
        if (!wrap_label) wrap_label = json_string_field(entry, "wrap");
        enabled = json_bool_field_default(entry, "enabled", true);
        mode = RuntimeMotionTrack3DModeFromLabel(mode_label);
        supported = RuntimeMotionTrack3DModeSupported(mode);
        if (enabled) {
            duplicate = runtime_scene_motion_bridge_is_duplicate_enabled_object(
                &g_last_object_motion_summary,
                object_id);
        }

        if (g_last_object_motion_summary.stored_tracks <
            RUNTIME_MOTION_TRACK_3D_MAX_TRACKS) {
            track = &g_last_object_motion_summary
                         .tracks[g_last_object_motion_summary.stored_tracks++];
            track->used = true;
            track->enabled = enabled;
            track->mode = mode;
            track->supported_mode = supported;
            RuntimeMotionTrack3DCopyString(track->object_id,
                                           sizeof(track->object_id),
                                           object_id);
            RuntimeMotionTrack3DCopyString(track->mode_label,
                                           sizeof(track->mode_label),
                                           mode_label ? mode_label
                                                      : RuntimeMotionTrack3DModeLabel(mode));
            RuntimeMotionTrack3DCopyString(track->timing_domain,
                                           sizeof(track->timing_domain),
                                           timing_domain);
            RuntimeMotionTrack3DCopyString(track->wrap_label,
                                           sizeof(track->wrap_label),
                                           wrap_label);
            if (mode == RUNTIME_MOTION_TRACK_3D_MODE_AUTHORED_PATH) {
                if (runtime_scene_motion_bridge_parse_position_path(entry,
                                                                    world_scale,
                                                                    track)) {
                    g_last_object_motion_summary.position_path_tracks += 1;
                }
                if (runtime_scene_motion_bridge_parse_rotation_keyframes(entry, track)) {
                    g_last_object_motion_summary.rotation_keyframe_tracks += 1;
                }
            }
        }

        if (!g_last_object_motion_summary.first_object_id[0] && object_id && object_id[0]) {
            RuntimeMotionTrack3DCopyString(g_last_object_motion_summary.first_object_id,
                                           sizeof(g_last_object_motion_summary.first_object_id),
                                           object_id);
        }
        if (mode == RUNTIME_MOTION_TRACK_3D_MODE_AUTHORED_PATH) {
            g_last_object_motion_summary.authored_path_tracks += 1;
        } else if (mode == RUNTIME_MOTION_TRACK_3D_MODE_PHYSICS) {
            g_last_object_motion_summary.physics_tracks += 1;
        }
        if (!enabled) {
            g_last_object_motion_summary.disabled_tracks += 1;
            continue;
        }

        g_last_object_motion_summary.enabled_tracks += 1;
        matched = runtime_scene_motion_bridge_object_id_exists(object_id);
        if (track) {
            track->matched_object = matched;
            track->duplicate_object = duplicate;
        }
        if (matched) {
            g_last_object_motion_summary.matched_tracks += 1;
        } else {
            g_last_object_motion_summary.unmatched_tracks += 1;
            if (!g_last_object_motion_summary.first_unmatched_object_id[0]) {
                RuntimeMotionTrack3DCopyString(
                    g_last_object_motion_summary.first_unmatched_object_id,
                    sizeof(g_last_object_motion_summary.first_unmatched_object_id),
                    object_id);
            }
        }
        if (!supported) {
            g_last_object_motion_summary.unsupported_tracks += 1;
            if (!g_last_object_motion_summary.first_unsupported_object_id[0]) {
                RuntimeMotionTrack3DCopyString(
                    g_last_object_motion_summary.first_unsupported_object_id,
                    sizeof(g_last_object_motion_summary.first_unsupported_object_id),
                    object_id);
            }
        }
        if (duplicate) {
            g_last_object_motion_summary.duplicate_tracks += 1;
        }
        if (track && RuntimeMotionTrack3DHasExecutableMotion(track)) {
            g_last_object_motion_summary.sampled_tracks += 1;
            g_last_object_motion_summary.has_executable_motion = true;
        }
    }
}

void runtime_scene_motion_bridge_get_last_summary(RuntimeMotionTrack3DSummary *out_summary) {
    if (!out_summary) {
        return;
    }
    *out_summary = g_last_object_motion_summary;
}

bool runtime_scene_motion_bridge_sample_object(const char *object_id,
                                               double normalized_t,
                                               RuntimeMotionTrack3DSample *out_sample) {
    RuntimeMotionTrack3DSample sample = {0};
    if (out_sample) *out_sample = sample;
    if (!object_id || !object_id[0] || !out_sample) {
        return false;
    }
    for (int i = 0; i < g_last_object_motion_summary.stored_tracks; ++i) {
        RuntimeMotionTrack3D *track = &g_last_object_motion_summary.tracks[i];
        if (!track->used || strcmp(track->object_id, object_id) != 0) {
            continue;
        }
        return RuntimeMotionTrack3DSampleTrack(track, normalized_t, out_sample);
    }
    return false;
}

bool runtime_scene_motion_bridge_has_executable_motion(void) {
    return g_last_object_motion_summary.has_executable_motion;
}
