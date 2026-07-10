#include "import/runtime_scene_motion_bridge.h"

#include "import/runtime_scene_bridge.h"
#include "scene/object_manager.h"

#include <json-c/json.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static RuntimeMotionTrack3DSummary g_last_object_motion_summary = {0};

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

void runtime_scene_motion_bridge_reset(void) {
    RuntimeMotionTrack3DSummaryInit(&g_last_object_motion_summary,
                                    "object_motion_tracks_missing");
}

void runtime_scene_motion_bridge_apply_authoring(json_object *authoring) {
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
        timing_domain = json_string_field(entry, "timing_domain");
        wrap_label = json_string_field(entry, "wrap");
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
    }
}

void runtime_scene_motion_bridge_get_last_summary(RuntimeMotionTrack3DSummary *out_summary) {
    if (!out_summary) {
        return;
    }
    *out_summary = g_last_object_motion_summary;
}
