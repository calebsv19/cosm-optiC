/* Semantic locks cover this source/camera/hold, not unrelated views or lights.
 */
#include "import/runtime_camera_rig.h"
#include "import/runtime_inspection.h"
#include "import/scene_timeline_document_io.h"
#include "motion/scene_motion_paths.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static json_object *get(json_object *o, const char *k) {
  json_object *v = NULL;
  if (o)
    json_object_object_get_ex(o, k, &v);
  return v;
}
static const char *str(json_object *o, const char *k) {
  json_object *v = get(o, k);
  return json_object_is_type(v, json_type_string) ? json_object_get_string(v)
                                                  : "";
}
static json_object *clone(json_object *o) {
  json_object *c = NULL;
  if (o)
    json_object_deep_copy(o, &c, NULL);
  return c;
}
/* Normalize number spelling, including legacy int/double zero fields. Never
 * round a large integer through double. Values here are opaque lock records. */
static json_object *semantic(json_object *o) {
  if (!o)
    return NULL;
  enum json_type t = json_object_get_type(o);
  if (t == json_type_int) {
    char b[64];
    snprintf(b, sizeof(b), "%" PRId64, json_object_get_int64(o));
    return json_object_new_string(b);
  }
  if (t == json_type_double) {
    char b[64];
    double d = json_object_get_double(o);
    snprintf(b, sizeof(b), "%.17g", d == 0 ? 0 : d);
    return json_object_new_string(b);
  }
  if (t == json_type_object) {
    json_object *v = json_object_new_object();
    json_object_object_foreach(o, k, x) {
      json_object_object_add(v, k, semantic(x));
    }
    return v;
  }
  if (t == json_type_array) {
    json_object *v = json_object_new_array();
    for (size_t i = 0; i < json_object_array_length(o); i++)
      json_object_array_add(v, semantic(json_object_array_get_idx(o, i)));
    return v;
  }
  return clone(o);
}
json_object *RuntimeInspectionDependencies(json_object *scene,
                                           json_object *entry) {
  json_object *a = get(get(get(scene, "extensions"), "ray_tracing"),
                       "authoring"),
              *rig = get(a, "camera_rig"), *o = json_object_new_object();
  const char *source = str(entry, "source_camera_id"),
             *camera = str(entry, "camera_id");
  json_object_object_add(o, "world_scale", clone(get(scene, "world_scale")));
  json_object_object_add(
      o, "rigid_result",
      clone(get(get(get(scene, "extensions"), "ray_tracing"), "rigid_result")));
  json_object *seq = get(a, "sequence"), *s = json_object_new_object();
  const char *keys[] = {"source", "rate", "range"};
  for (int i = 0; i < 3; i++)
    json_object_object_add(s, keys[i], clone(get(seq, keys[i])));
  uint64_t start = json_object_get_uint64(get(entry, "start_frame"));
  json_object *segments = get(seq, "segments");
  for (size_t i = 0; segments && i < json_object_array_length(segments); i++) {
    json_object *v = json_object_array_get_idx(segments, i);
    if (json_object_get_uint64(get(v, "start_frame")) == start)
      json_object_object_add(s, "hold", clone(v));
  }
  json_object_object_add(o, "source", s);
  json_object *list = get(rig, "cameras"), *cameras = json_object_new_array();
  for (size_t i = 0; list && i < json_object_array_length(list); i++) {
    json_object *v = json_object_array_get_idx(list, i);
    if (!strcmp(str(v, "id"), source) || !strcmp(str(v, "id"), camera))
      json_object_array_add(cameras, clone(v));
  }
  json_object_object_add(o, "cameras", cameras);
  CameraSequenceSample mapping;
  TimelineSample at = {
      json_object_get_int64(get(get(rig, "range"), "start_frame")) +
          (int64_t)start,
      0, 1};
  if (!RuntimeCameraRigDeclaredClock(scene, source, at, &mapping)) {
    json_object_put(o);
    return NULL;
  }
  json_object *time = json_object_new_array();
  json_object_array_add(time,
                        json_object_new_int64(mapping.motion_time.numerator));
  json_object_array_add(time,
                        json_object_new_int64(mapping.motion_time.denominator));
  json_object_object_add(o, "source_camera_time", time);
  TimelineDocument *doc = malloc(sizeof(*doc));
  if (doc && SceneTimelineDocumentFromJson(get(a, "scene_timeline"), doc) ==
                 TIMELINE_STATUS_OK) {
    size_t n = 0;
    for (size_t i = 0; i < doc->track_count; i++)
      if (!strcmp(doc->tracks[i].target_id, source) ||
          !strcmp(doc->tracks[i].target_id, camera))
        doc->tracks[n++] = doc->tracks[i];
    doc->track_count = n;
    json_object_object_add(o, "camera_channels",
                           SceneTimelineDocumentToJson(doc));
  }
  free(doc);
  if(!strcmp(source,"camera/main")||!strcmp(camera,"camera/main")){
    json_object_object_add(o,"legacy_camera_path",clone(get(a,"camera_path")));json_object_object_add(o,"legacy_camera_depth",clone(get(a,"camera_path_depth")));
  }
  MotionPaths *paths = calloc(1, sizeof(*paths));
  if (paths && MotionPathsParse(a, paths, NULL, 0)) {
    size_t n = 0;
    for (size_t i = 0; i < paths->binding_count; i++)
      if (!strcmp(paths->bindings[i].target_id, source) ||
          !strcmp(paths->bindings[i].target_id, camera))
        paths->bindings[n++] = paths->bindings[i];
    paths->binding_count = n;
    for(size_t i=0;i<paths->binding_count;i++)if(paths->bindings[i].use_focus_target||paths->bindings[i].camera_orientation==2){json_object_object_add(o,"focus_target",clone(get(a,"camera_focus_target")));break;}
    n = 0;
    for (size_t i = 0; i < paths->count; i++) {
      bool used = false;
      for (size_t j = 0; j < paths->binding_count; j++)
        if (!strcmp(paths->paths[i].id, paths->bindings[j].path_id))
          used = true;
      if (used)
        paths->paths[n++] = paths->paths[i];
    }
    paths->count = n;
    json_object_object_add(o, "camera_routes", MotionPathsToJson(paths));
  }
  free(paths);
  list = get(a, "motion_plans");
  json_object *plans = json_object_new_array();
  for (size_t i = 0; list && i < json_object_array_length(list); i++) {
    json_object *v = json_object_array_get_idx(list, i);
    if (!strcmp(str(v, "target"), source) || !strcmp(str(v, "target"), camera))
      json_object_array_add(plans, clone(v));
  }
  json_object_object_add(o, "motion_plans", plans);
  json_object *out = semantic(o);
  json_object_put(o);
  return out;
}
bool RuntimeInspectionDependencyMatches(json_object *scene,
                                        json_object *entry) {
  json_object *now = RuntimeInspectionDependencies(scene, entry);
  bool ok = now && json_object_equal(now, get(entry, "dependency"));
  if (now)
    json_object_put(now);
  return ok;
}
