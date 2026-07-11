#include "editor/object_editor_motion.h"

#include "app/animation.h"
#include "camera/camera_path_3d.h"
#include "import/runtime_scene_bridge.h"
#include "scene/object_manager.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct ObjectEditorMotionStore {
    int track_count;
    RuntimeMotionTrack3D tracks[RUNTIME_MOTION_TRACK_3D_MAX_TRACKS];
} ObjectEditorMotionStore;

static ObjectEditorMotionStore g_object_editor_motion = {0};

static double object_editor_motion_scale(double value, double scale) {
    if (!(scale > 0.0) || !isfinite(scale)) {
        scale = 1.0;
    }
    return value * scale;
}

static int object_editor_motion_find_index(const char* object_id) {
    if (!object_id || !object_id[0]) {
        return -1;
    }
    for (int i = 0; i < g_object_editor_motion.track_count; ++i) {
        RuntimeMotionTrack3D* track = &g_object_editor_motion.tracks[i];
        if (track->used && strcmp(track->object_id, object_id) == 0) {
            return i;
        }
    }
    return -1;
}

static RuntimeMotionTrack3D* object_editor_motion_find_or_add(const char* object_id) {
    int index = object_editor_motion_find_index(object_id);
    RuntimeMotionTrack3D* track = NULL;
    if (index >= 0) {
        return &g_object_editor_motion.tracks[index];
    }
    if (!object_id || !object_id[0] ||
        g_object_editor_motion.track_count >= RUNTIME_MOTION_TRACK_3D_MAX_TRACKS) {
        return NULL;
    }
    track = &g_object_editor_motion.tracks[g_object_editor_motion.track_count++];
    memset(track, 0, sizeof(*track));
    track->used = true;
    RuntimeMotionTrack3DCopyString(track->object_id, sizeof(track->object_id), object_id);
    return track;
}

static void object_editor_motion_seed_default_path(RuntimeMotionTrack3D* track,
                                                   double x,
                                                   double y,
                                                   double z) {
    if (!track) return;
    memset(&track->position_path, 0, sizeof(track->position_path));
    memset(&track->position_path_3d, 0, sizeof(track->position_path_3d));
    track->position_path.mode = BEZIER_CUBIC;
    (void)CameraPath3D_InsertPoint(&track->position_path_3d,
                                   &track->position_path,
                                   x,
                                   y,
                                   z,
                                   0.0);
    track->has_position_path = track->position_path.numPoints > 0;
}

static void object_editor_motion_seed_default_rotation(RuntimeMotionTrack3D* track,
                                                       double yaw_radians) {
    RuntimeMotionTrack3DRotationKeyframe* keyframe = NULL;
    if (!track) return;
    track->rotation_keyframe_count = 1;
    keyframe = &track->rotation_keyframes[0];
    memset(keyframe, 0, sizeof(*keyframe));
    keyframe->t = 0.0;
    keyframe->yaw_radians = yaw_radians;
    keyframe->pitch_radians = 0.0;
    keyframe->roll_radians = 0.0;
    track->has_rotation_keyframes = true;
}

static json_object* object_editor_motion_path_to_json(const RuntimeMotionTrack3D* track,
                                                      double authored_to_runtime_scale) {
    json_object* path_obj = NULL;
    json_object* points = NULL;
    if (!track || !track->has_position_path || track->position_path.numPoints <= 0) {
        return NULL;
    }
    path_obj = json_object_new_object();
    points = json_object_new_array();
    if (!path_obj || !points) {
        if (points) json_object_put(points);
        if (path_obj) json_object_put(path_obj);
        return NULL;
    }
    json_object_object_add(path_obj,
                           "mode",
                           json_object_new_string(track->position_path.mode == BEZIER_QUADRATIC
                                                      ? "BEZIER_QUADRATIC"
                                                      : "BEZIER_CUBIC"));
    for (int i = 0; i < track->position_path.numPoints && i < MAX_BEZIER_POINTS; ++i) {
        json_object* point = json_object_new_object();
        if (!point) {
            json_object_put(path_obj);
            return NULL;
        }
        json_object_object_add(point,
                               "x",
                               json_object_new_double(object_editor_motion_scale(
                                   track->position_path.points[i].x,
                                   authored_to_runtime_scale)));
        json_object_object_add(point,
                               "y",
                               json_object_new_double(object_editor_motion_scale(
                                   track->position_path.points[i].y,
                                   authored_to_runtime_scale)));
        json_object_object_add(point,
                               "z",
                               json_object_new_double(object_editor_motion_scale(
                                   track->position_path_3d.point_z[i],
                                   authored_to_runtime_scale)));
        json_object_object_add(point,
                               "rotation",
                               json_object_new_double(track->position_path.rotations[i]));
        json_object_object_add(point,
                               "handleLink",
                               json_object_new_boolean(track->position_path.handleLink[i]));
        if (i < track->position_path.numPoints - 1) {
            json_object* velocity1 = json_object_new_object();
            if (velocity1) {
                json_object_object_add(velocity1,
                                       "vx",
                                       json_object_new_double(object_editor_motion_scale(
                                           track->position_path.handles[i][0].vx,
                                           authored_to_runtime_scale)));
                json_object_object_add(velocity1,
                                       "vy",
                                       json_object_new_double(object_editor_motion_scale(
                                           track->position_path.handles[i][0].vy,
                                           authored_to_runtime_scale)));
                json_object_object_add(velocity1,
                                       "vz",
                                       json_object_new_double(object_editor_motion_scale(
                                           track->position_path_3d.handles_vz[i][0],
                                           authored_to_runtime_scale)));
                json_object_object_add(point, "velocity1", velocity1);
            }
        }
        if (i > 0) {
            json_object* velocity2 = json_object_new_object();
            if (velocity2) {
                json_object_object_add(velocity2,
                                       "vx",
                                       json_object_new_double(object_editor_motion_scale(
                                           track->position_path.handles[i - 1][1].vx,
                                           authored_to_runtime_scale)));
                json_object_object_add(velocity2,
                                       "vy",
                                       json_object_new_double(object_editor_motion_scale(
                                           track->position_path.handles[i - 1][1].vy,
                                           authored_to_runtime_scale)));
                json_object_object_add(velocity2,
                                       "vz",
                                       json_object_new_double(object_editor_motion_scale(
                                           track->position_path_3d.handles_vz[i - 1][1],
                                           authored_to_runtime_scale)));
                json_object_object_add(point, "velocity2", velocity2);
            }
        }
        json_object_array_add(points, point);
    }
    json_object_object_add(path_obj, "points", points);
    return path_obj;
}

static json_object* object_editor_motion_rotation_keyframes_to_json(
    const RuntimeMotionTrack3D* track) {
    json_object* keyframes = NULL;
    if (!track || !track->has_rotation_keyframes || track->rotation_keyframe_count <= 0) {
        return NULL;
    }
    keyframes = json_object_new_array();
    if (!keyframes) {
        return NULL;
    }
    for (int i = 0; i < track->rotation_keyframe_count &&
                    i < RUNTIME_MOTION_TRACK_3D_MAX_ROTATION_KEYFRAMES;
         ++i) {
        const RuntimeMotionTrack3DRotationKeyframe* src = &track->rotation_keyframes[i];
        json_object* keyframe = json_object_new_object();
        if (!keyframe) {
            json_object_put(keyframes);
            return NULL;
        }
        json_object_object_add(keyframe, "t", json_object_new_double(src->t));
        json_object_object_add(keyframe,
                               "yaw_degrees",
                               json_object_new_double(src->yaw_radians * 180.0 / M_PI));
        json_object_object_add(keyframe,
                               "pitch_degrees",
                               json_object_new_double(src->pitch_radians * 180.0 / M_PI));
        json_object_object_add(keyframe,
                               "roll_degrees",
                               json_object_new_double(src->roll_radians * 180.0 / M_PI));
        json_object_array_add(keyframes, keyframe);
    }
    return keyframes;
}

void ObjectEditorMotionReset(void) {
    memset(&g_object_editor_motion, 0, sizeof(g_object_editor_motion));
}

void ObjectEditorMotionHydrateFromRuntimeSummary(const RuntimeMotionTrack3DSummary* summary) {
    ObjectEditorMotionReset();
    if (!summary || !summary->valid) {
        return;
    }
    for (int i = 0; i < summary->stored_tracks &&
                    i < RUNTIME_MOTION_TRACK_3D_MAX_TRACKS;
         ++i) {
        g_object_editor_motion.tracks[g_object_editor_motion.track_count++] =
            summary->tracks[i];
    }
}

int ObjectEditorMotionTrackCount(void) {
    return g_object_editor_motion.track_count;
}

const RuntimeMotionTrack3D* ObjectEditorMotionFindTrack(const char* object_id) {
    int index = object_editor_motion_find_index(object_id);
    if (index < 0) {
        return NULL;
    }
    return &g_object_editor_motion.tracks[index];
}

bool ObjectEditorMotionSetObjectStatic(const char* object_id) {
    RuntimeMotionTrack3D* track = object_editor_motion_find_or_add(object_id);
    if (!track) {
        return false;
    }
    track->enabled = false;
    track->mode = RUNTIME_MOTION_TRACK_3D_MODE_AUTHORED_PATH;
    track->supported_mode = true;
    RuntimeMotionTrack3DCopyString(track->mode_label,
                                   sizeof(track->mode_label),
                                   RuntimeMotionTrack3DModeLabel(track->mode));
    RuntimeMotionTrack3DCopyString(track->timing_domain,
                                   sizeof(track->timing_domain),
                                   "normalized_t");
    RuntimeMotionTrack3DCopyString(track->wrap_label,
                                   sizeof(track->wrap_label),
                                   "hold");
    return true;
}

bool ObjectEditorMotionSetObjectAuthored(const char* object_id,
                                         double x,
                                         double y,
                                         double z,
                                         double yaw_radians) {
    RuntimeMotionTrack3D* track = object_editor_motion_find_or_add(object_id);
    if (!track) {
        return false;
    }
    track->enabled = true;
    track->matched_object = true;
    track->duplicate_object = false;
    track->mode = RUNTIME_MOTION_TRACK_3D_MODE_AUTHORED_PATH;
    track->supported_mode = true;
    RuntimeMotionTrack3DCopyString(track->mode_label,
                                   sizeof(track->mode_label),
                                   RuntimeMotionTrack3DModeLabel(track->mode));
    RuntimeMotionTrack3DCopyString(track->timing_domain,
                                   sizeof(track->timing_domain),
                                   "normalized_t");
    RuntimeMotionTrack3DCopyString(track->wrap_label,
                                   sizeof(track->wrap_label),
                                   "hold");
    if (!track->has_position_path) {
        object_editor_motion_seed_default_path(track, x, y, z);
    }
    if (!track->has_rotation_keyframes) {
        object_editor_motion_seed_default_rotation(track, yaw_radians);
    }
    return true;
}

bool ObjectEditorMotionObjectIdForSceneIndex(int scene_object_index,
                                             char* out_object_id,
                                             size_t out_object_id_size) {
    if (out_object_id && out_object_id_size > 0u) {
        out_object_id[0] = '\0';
    }
    if (scene_object_index < 0 || scene_object_index >= sceneSettings.objectCount) {
        return false;
    }
    return runtime_scene_bridge_get_last_object_id_for_scene_index(scene_object_index,
                                                                   out_object_id,
                                                                   out_object_id_size);
}

bool ObjectEditorMotionSetSelectedObjectStatic(int scene_object_index) {
    char object_id[RUNTIME_MOTION_TRACK_3D_OBJECT_ID_SIZE];
    if (!ObjectEditorMotionObjectIdForSceneIndex(scene_object_index,
                                                 object_id,
                                                 sizeof(object_id))) {
        return false;
    }
    return ObjectEditorMotionSetObjectStatic(object_id);
}

bool ObjectEditorMotionSetSelectedObjectAuthored(int scene_object_index) {
    char object_id[RUNTIME_MOTION_TRACK_3D_OBJECT_ID_SIZE];
    const SceneObject* object = NULL;
    if (scene_object_index < 0 || scene_object_index >= sceneSettings.objectCount) {
        return false;
    }
    if (!ObjectEditorMotionObjectIdForSceneIndex(scene_object_index,
                                                 object_id,
                                                 sizeof(object_id))) {
        return false;
    }
    object = &sceneSettings.sceneObjects[scene_object_index];
    return ObjectEditorMotionSetObjectAuthored(object_id,
                                               object->x,
                                               object->y,
                                               object->z,
                                               object->rotation);
}

json_object* ObjectEditorMotionBuildAuthoringTracksJson(double authored_to_runtime_scale) {
    json_object* tracks = NULL;
    if (g_object_editor_motion.track_count <= 0) {
        return NULL;
    }
    tracks = json_object_new_array();
    if (!tracks) {
        return NULL;
    }
    for (int i = 0; i < g_object_editor_motion.track_count; ++i) {
        const RuntimeMotionTrack3D* track = &g_object_editor_motion.tracks[i];
        json_object* entry = NULL;
        json_object* timing = NULL;
        if (!track->used || !track->object_id[0]) {
            continue;
        }
        entry = json_object_new_object();
        timing = json_object_new_object();
        if (!entry || !timing) {
            if (timing) json_object_put(timing);
            if (entry) json_object_put(entry);
            json_object_put(tracks);
            return NULL;
        }
        json_object_object_add(entry, "object_id", json_object_new_string(track->object_id));
        json_object_object_add(entry, "enabled", json_object_new_boolean(track->enabled));
        json_object_object_add(entry,
                               "mode",
                               json_object_new_string(track->mode_label[0]
                                                          ? track->mode_label
                                                          : RuntimeMotionTrack3DModeLabel(track->mode)));
        json_object_object_add(timing,
                               "domain",
                               json_object_new_string(track->timing_domain[0]
                                                          ? track->timing_domain
                                                          : "normalized_t"));
        json_object_object_add(timing,
                               "wrap",
                               json_object_new_string(track->wrap_label[0]
                                                          ? track->wrap_label
                                                          : "hold"));
        json_object_object_add(entry, "timing", timing);
        if (track->has_position_path) {
            json_object* path = object_editor_motion_path_to_json(
                track,
                authored_to_runtime_scale);
            if (path) {
                json_object_object_add(entry, "path", path);
            }
        }
        if (track->has_rotation_keyframes) {
            json_object* keyframes = object_editor_motion_rotation_keyframes_to_json(track);
            if (keyframes) {
                json_object_object_add(entry, "rotation_keyframes", keyframes);
            }
        }
        json_object_array_add(tracks, entry);
    }
    if (json_object_array_length(tracks) == 0) {
        json_object_put(tracks);
        return NULL;
    }
    return tracks;
}
