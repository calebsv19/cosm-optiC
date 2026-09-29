#include "editor/scene_editor_motion_paths.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_document_timeline.h"
#include "editor/scene_editor_timeline.h"
#include "import/runtime_scene_object_timeline.h"
#include "import/scene_timeline_document_io.h"
#include "scene_editor_document_transaction.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static json_object *member(json_object *o, const char *k, bool create) {
  json_object *v = NULL;
  if (o && !json_object_object_get_ex(o, k, &v) && create) {
    v = json_object_new_object();
    json_object_object_add(o, k, v);
  }
  return v;
}
static json_object *author(bool create) {
  return member(member(member(document_authoring_root(), "extensions", create),
                       "ray_tracing", create),
                "authoring", create);
}
static bool fail(char *m, size_t n, const char *why) {
  if (m && n)
    snprintf(m, n, "%s", why);
  return false;
}
bool SceneEditorMotionPathsRead(MotionPaths *out) {
  return SceneEditorDocumentIsOpen() &&
         MotionPathsParse(author(false), out, NULL, 0);
}
static bool commit(const MotionPaths *paths, const TimelineDocument *timeline,
                   unsigned long long rev, char *m, size_t n) {
  if (rev != SceneEditorDocumentRevision())
    return fail(m, n, "Scene changed; retry the edit.");
  json_object *encoded = MotionPathsToJson(paths);
  if (!encoded)
    return fail(m, n, "Invalid path capacity.");
  MotionPaths validated;
  json_object *check = json_object_new_object();
  json_object_object_add(check, "motion_paths", json_object_get(encoded));
  bool ok = MotionPathsParse(check, &validated, m, n);
  json_object_put(check);
  if (!ok) {
    json_object_put(encoded);
    return false;
  }
  if (!document_begin_command(m, n)) {
    json_object_put(encoded);
    return false;
  }
  json_object *root = author(true);
  if (!root) {
    json_object_put(encoded);
    document_rollback_command();
    return false;
  }
  json_object_object_add(root, "motion_paths", encoded);
  if (timeline) {
    json_object *encoded_timeline = SceneTimelineDocumentToJson(timeline);
    if (!encoded_timeline) {
      document_rollback_command();
      return fail(m, n, "Invalid timeline.");
    }
    json_object_object_add(root, "scene_timeline", encoded_timeline);
  }
  return document_finish_command(m, n);
}
bool SceneEditorMotionPathsSet(const MotionPaths *p, unsigned long long rev,
                               char *m, size_t n) {
  return commit(p, NULL, rev, m, n);
}
bool SceneEditorMotionPathCreate(const char *name, const double origin[3],
                                 double length, unsigned long long rev,
                                 char *id, size_t id_size, char *m, size_t n) {
  MotionPaths d;
  if (!SceneEditorMotionPathsRead(&d) || d.count >= MOTION_PATH_CAPACITY)
    return fail(m, n, "Path capacity reached (16).");
  if (!name || !*name || strlen(name) >= sizeof(d.paths[0].name) ||
      !isfinite(length) || length <= 0)
    return fail(m, n, "Enter a name and positive path length.");
  MotionPath p = {.count = 2};
  snprintf(p.name, sizeof(p.name), "%s", name);
  unsigned serial = 1;
  bool used;
  do {
    snprintf(p.id, sizeof(p.id), "path-%u", serial++);
    used = false;
    for (size_t i = 0; i < d.count; ++i)
      if (!strcmp(d.paths[i].id, p.id))
        used = true;
  } while (used);
  for (int i = 0; i < 2; ++i) {
    snprintf(p.points[i].id, sizeof(p.points[i].id), "point-%d", i + 1);
    for (int k = 0; k < 3; ++k)
      p.points[i].position[k] = origin[k];
    p.points[i].position[0] += i * length;
    p.points[i].incoming[0] = -length / 3;
    p.points[i].outgoing[0] = length / 3;
  }
  d.paths[d.count++] = p;
  if (!commit(&d, NULL, rev, m, n))
    return false;
  snprintf(id, id_size, "%s", p.id);
  return true;
}
bool SceneEditorMotionPathBind(const char *object_id, const char *path_id,
                               bool attach, unsigned long long rev, char *m,
                               size_t n) {
  if (rev != SceneEditorDocumentRevision())
    return fail(m, n, "Scene changed; retry attachment.");
  SceneEditorDocumentObjectInfo info;
  if (!SceneEditorDocumentObjectById(object_id, &info) || info.locked ||
      !info.visible)
    return fail(m, n, "Choose an unlocked visible object.");
  MotionPaths paths;
  static TimelineDocument doc;
  if (!SceneEditorMotionPathsRead(&paths))
    return false;
  if (SceneEditorDocumentGetTimeline(&doc) != TIMELINE_STATUS_OK) {
    TimelineDocumentInit(&doc, (TimelineRate){24, 1}, (TimelineRange){0, 120});
  }
  size_t b = 0;
  for (; b < paths.binding_count; ++b)
    if (!strcmp(paths.bindings[b].object_id, object_id))
      break;
  if (b == paths.binding_count) {
    if (!attach || b >= MOTION_BINDING_CAPACITY)
      return fail(m, n, "No binding or binding capacity reached.");
    ++paths.binding_count;
    memset(&paths.bindings[b], 0, sizeof(paths.bindings[b]));
  }
  bool was_attached = paths.bindings[b].enabled;
  if (attach) {
    bool found = false;
    for (size_t i = 0; i < paths.count; ++i)
      if (!strcmp(paths.paths[i].id, path_id))
        found = true;
    if (!found)
      return fail(m, n, "Choose a path first.");
    snprintf(paths.bindings[b].path_id, sizeof(paths.bindings[b].path_id), "%s",
             path_id);
  }
  if (strlen(object_id) + 7 >= TIMELINE_ID_CAPACITY)
    return fail(m, n, "Object ID is too long for timeline.");
  snprintf(paths.bindings[b].object_id, sizeof(paths.bindings[b].object_id),
           "%s", object_id);
  paths.bindings[b].enabled = attach;
  char target[64];
  snprintf(target, sizeof(target), "object/%s", object_id);
  MotionPathBinding *binding = &paths.bindings[b];
  /* Rebinding an active follower must not overwrite its original source. */
  if (attach && !was_attached) {
    binding->restore_known = true;
    binding->restore_count = 0;
    memset(binding->restore_xyz_tracks, 0, sizeof(binding->restore_xyz_tracks));
    for (size_t i = 0; i < doc.track_count; ++i) {
      const TimelineTrack *t = &doc.tracks[i];
      if (!strcmp(t->target_id, target) && t->enabled &&
          RuntimeObjectTimelineAxis(t->property_id) >= 0) {
        if (binding->restore_count == 3)
          return fail(m, n, "Invalid prior XYZ ownership.");
        snprintf(binding->restore_xyz_tracks[binding->restore_count++],
                 TIMELINE_ID_CAPACITY, "%s", t->track_id);
      }
    }
  }
  if (!attach && !was_attached)
    return fail(m, n, "Object is already detached.");
  /* Old v1 bindings have no recoverable predecessor. Their UI explicitly
   * offers static detach; never guess that disabled historical keys were active. */
  bool legacy_static = !attach && !binding->restore_known;
  if (legacy_static) {
    binding->restore_known = true;
    binding->restore_count = 0;
  }
  if (!attach) {
    for (size_t j = 0; j < binding->restore_count; ++j) {
      bool found = false;
      for (size_t i = 0; i < doc.track_count; ++i)
        if (!strcmp(doc.tracks[i].target_id, target) &&
            RuntimeObjectTimelineAxis(doc.tracks[i].property_id) >= 0 &&
            !strcmp(doc.tracks[i].track_id, binding->restore_xyz_tracks[j]))
          found = true;
      if (!found)
        return fail(m, n, "Prior XYZ track is missing; restore it before detaching.");
    }
  }
  size_t selected = SIZE_MAX;
  bool has_xyz = false;
  for (size_t i = 0; i < doc.track_count; ++i) {
    TimelineTrack *t = &doc.tracks[i];
    if (strcmp(t->target_id, target))
      continue;
    if (RuntimeObjectTimelineAxis(t->property_id) >= 0) {
      t->enabled = false;
      for (size_t j = 0; !attach && j < binding->restore_count; ++j)
        if (!strcmp(t->track_id, binding->restore_xyz_tracks[j]))
          t->enabled = true;
      has_xyz |= t->enabled;
    }
    if (!strcmp(t->property_id, MOTION_PROGRESS_PROPERTY)) {
      t->enabled = attach;
      selected = i;
    }
  }
  if (attach && selected == SIZE_MAX) {
    TimelineTrack t;
    char tid[64];
    unsigned serial = 1;
    bool used;
    do {
      snprintf(tid, sizeof(tid), "path-progress-%u", serial++);
      used = false;
      for (size_t i = 0; i < doc.track_count; ++i)
        if (!strcmp(tid, doc.tracks[i].track_id))
          used = true;
    } while (used);
    TimelineTrackInit(&t, tid, target, MOTION_PROGRESS_PROPERTY,
                      TIMELINE_VALUE_SCALAR);
    TimelineTrackSetUnit(&t, TIMELINE_UNIT_UNITLESS);
    TimelineTrackAddKey(&t, doc.range.start_frame, TimelineValueScalar(0),
                        TIMELINE_INTERPOLATION_LINEAR);
    if (doc.range.frame_count > 1)
      TimelineTrackAddKey(&t, doc.range.start_frame + doc.range.frame_count - 1,
                          TimelineValueScalar(1),
                          TIMELINE_INTERPOLATION_LINEAR);
    selected = doc.track_count;
    if (TimelineDocumentAddTrack(&doc, &t) != TIMELINE_STATUS_OK)
      return fail(m, n, "Timeline channel capacity reached.");
  }
  if (!commit(&paths, &doc, rev, m, n))
    return false;
  SceneEditorTimelinePause();
  if (attach)
    SceneEditorTimelineSelectTrack(selected);
  snprintf(m, n, "%s",
           attach    ? "Attached on path. Edit Path progress keys (0 to 1)."
           : legacy_static ? "Detached to static; old binding had no saved prior source. XYZ keys remain disabled."
           : has_xyz ? "Detached; prior XYZ keys restored."
                     : "Detached; static placement restored.");
  return true;
}
