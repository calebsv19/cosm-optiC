#include "app/ray_tracing_sequence_recovery.h"
#include "animation/sequence_time.h"
#include "app/ray_tracing_request_utils.h"
#include "app/ray_tracing_sha256.h"
#include "import/runtime_camera_rig.h"
#include "import/sequence_clock_json.h"
#include <string.h>
static json_object *get(json_object *o, const char *key) {
  json_object *v = NULL;
  if (o)
    json_object_object_get_ex(o, key, &v);
  return v;
}
static bool number(json_object *o, const char *key, uint64_t value) {
  json_object *v = get(o, key);
  return json_object_is_type(v, json_type_int) &&
         json_object_get_int64(v) >= 0 && json_object_get_uint64(v) == value;
}
static bool text(json_object *o, const char *key, const char *value) {
  json_object *v = get(o, key);
  return json_object_is_type(v, json_type_string) &&
         json_object_get_string_len(v) == (int)strlen(value) &&
         !strcmp(json_object_get_string(v), value);
}
static bool ratio(json_object *o, const char *key, SequenceTime value) {
  json_object *v = get(o, key);
  if (!json_object_is_type(v, json_type_array) ||
      json_object_array_length(v) != 2)
    return false;
  json_object *a = json_object_array_get_idx(v, 0),
              *b = json_object_array_get_idx(v, 1);
  return json_object_is_type(a, json_type_int) &&
         json_object_is_type(b, json_type_int) &&
         json_object_get_int64(a) >= 0 && json_object_get_int64(b) > 0 &&
         SequenceTimeCompare((SequenceTime){json_object_get_uint64(a),
                                            json_object_get_uint64(b)},
                             value) == 0;
}
bool ray_tracing_sequence_recovery_validate(
    const RayTracingAgentRenderRequest *r, int start, int count, char *d,
    size_t n) {
  if (!r || start < 0 || count < 0)
    return false;
  json_object *scene = json_object_from_file(r->runtime_scene_path);
  if (!scene)
    return false;
  json_object *a = get(get(get(scene, "extensions"), "ray_tracing"),
                       "authoring"),
              *seq = get(a, "sequence"), *rig = get(a, "camera_rig");
  if (!seq) {
    json_object_put(scene);
    return true;
  } /* legacy frame recovery unchanged */
  SequenceClock clock;
  char scene_hash[65];
  bool ok = SequenceClockFromJson(seq, &clock) &&
            ray_tracing_sha256_file(r->runtime_scene_path, scene_hash);
  for (int i = 0; ok && i < count; i++) {
    char path[PATH_MAX], sidecar[PATH_MAX], image_hash[65];
    int frame = start + i;
    int z = snprintf(path, sizeof(path), "%s/frames/frame_%04d.bmp",
                     r->output_root, frame);
    int y = snprintf(sidecar, sizeof(sidecar), "%s.sequence.json", path);
    if (z < 0 || z >= (int)sizeof(path) || y < 0 || y >= (int)sizeof(sidecar)) {
      ok = false;
      break;
    }
    json_object *row = json_object_from_file(sidecar);
    SequenceEvaluation sample;
    ok = row &&
         SequenceClockEvaluate(&clock, (TimelineSample){frame, 0, 1},
                               &sample) == SEQUENCE_OK &&
         ray_tracing_sha256_file(path, image_hash) &&
         text(row, "schema", "ray_sequence_frame_v1") &&
         text(row, "scene_sha256", scene_hash) &&
         text(row, "image_sha256", image_hash) && number(row, "frame", frame) &&
         ratio(row, "rate",
               (SequenceTime){clock.rate.frames_per_second_numerator,
                              clock.rate.frames_per_second_denominator}) &&
         ratio(row, "presentation_time", sample.presentation_time) &&
         ratio(row, "simulation_time", sample.simulation_time) &&
         number(row, "source_tick", sample.source_tick) &&
         text(row, "source_revision", clock.source.revision);
    if (ok && !get(a, "sequence_volume")) {
      json_object *bodies = get(row, "bodies");
      ok = json_object_is_type(bodies, json_type_array) &&
           json_object_array_length(bodies) > 0;
      for (size_t body = 0; ok && body < json_object_array_length(bodies);
           body++)
        ok = number(json_object_array_get_idx(bodies, body), "tick",
                    sample.source_tick);
    }
    if (ok && rig) {
      CameraSequenceSample c;
      ok = RuntimeCameraRigDeclaredClock(scene,
                                         r->camera_id[0] ? r->camera_id : NULL,
                                         (TimelineSample){frame, 0, 1}, &c);
      if (ok) {
        json_object *camera = json_object_array_get_idx(get(rig, "cameras"),
                                                        c.camera_index),
                    *shot = json_object_array_get_idx(get(rig, "shots"),
                                                      c.shot_index);
        const char *id = json_object_get_string(get(camera, "id")),
                   *shot_id = json_object_get_string(get(shot, "id"));
        const char *mode =
            json_object_get_string(get(get(camera, "clock"), "mode"));
        int policy = !mode                         ? 0
                     : !strcmp(mode, "continuous") ? CAMERA_CLOCK_CONTINUOUS
                     : !strcmp(mode, "selected")   ? CAMERA_CLOCK_SELECTED
                     : !strcmp(mode, "clip")       ? CAMERA_CLOCK_CLIP
                                                   : 0;
        ok = number(row, "camera_clock_policy", policy) && id && shot_id &&
             text(row, "camera_id", id) && text(row, "shot_id", shot_id) &&
             ratio(row, "camera_motion_time", c.motion_time) &&
             json_object_is_type(get(row, "camera_selected"),
                                 json_type_boolean) &&
             (bool)json_object_get_boolean(get(row, "camera_selected")) ==
                 c.selected;
      }
    }
    if (row)
      json_object_put(row);
    if (!ok && d && n)
      snprintf(d, n,
               "sequence receipt/hash/source/view mismatch at frame %d; "
               "preserve and reconcile output",
               frame);
  }
  json_object_put(scene);
  if (!ok && d && n && !d[0])
    snprintf(d, n, "invalid sequence recovery source");
  return ok;
}
