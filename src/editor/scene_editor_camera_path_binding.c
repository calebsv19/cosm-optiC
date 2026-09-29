/* Camera translation ownership. Legacy spatial data and lens/orientation tracks
 * stay authored; the runtime adapter retains their orientation semantics. */
#include "editor/scene_editor_motion_paths.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_document_timeline.h"
#include "editor/scene_editor_timeline.h"
#include "scene_editor_motion_paths_internal.h"
#include <stdio.h>
#include <string.h>
static bool driver(const char *p) {
  return !strcmp(p, "camera/path_progress") || !strcmp(p, "camera/position");
}
static bool fail(char *message, size_t size, const char *why) {
  if (message && size) snprintf(message, size, "%s", why);
  return false;
}
bool SceneEditorMotionPathBindCamera(const char *path_id, bool attach,
    unsigned long long revision, char *message, size_t size) {
  MotionPaths paths;
  static TimelineDocument doc;
  if (revision != SceneEditorDocumentRevision() || !SceneEditorMotionPathsRead(&paths))
    return fail(message, size, "Scene changed; retry camera attachment.");
  TimelineStatus status = SceneEditorDocumentGetTimeline(&doc);
  if (status == TIMELINE_STATUS_TARGET_NOT_FOUND)
    TimelineDocumentInit(&doc, (TimelineRate){24, 1}, (TimelineRange){0, 120});
  else if (status != TIMELINE_STATUS_OK)
    return fail(message, size, "Invalid scene timeline.");
  size_t index = 0;
  for (; index < paths.binding_count; ++index)
    if (!strcmp(paths.bindings[index].target_id, "camera/main")) break;
  if (index == paths.binding_count) {
    if (!attach || index == MOTION_BINDING_CAPACITY)
      return fail(message, size, "No camera binding or binding capacity reached.");
    memset(&paths.bindings[paths.binding_count++], 0, sizeof(paths.bindings[0]));
    snprintf(paths.bindings[index].target_id, TIMELINE_ID_CAPACITY, "camera/main");
  }
  MotionPathBinding *binding = &paths.bindings[index];
  if (attach) {
    bool found = false;
    for (size_t i = 0; i < paths.count; ++i)
      if (!strcmp(paths.paths[i].id, path_id)) found = true;
    if (!found) return fail(message, size, "Choose a route first.");
    snprintf(binding->path_id, sizeof(binding->path_id), "%s", path_id);
    if (!binding->enabled) {
      binding->restore_known = true;
      binding->restore_count = 0;
      for (size_t i = 0; i < doc.track_count; ++i) {
        const TimelineTrack *t = &doc.tracks[i];
        if (!strcmp(t->target_id, "camera/main") && t->enabled && driver(t->property_id)) {
          if (binding->restore_count == 3) return fail(message, size, "Conflicting camera sources.");
          snprintf(binding->restore_xyz_tracks[binding->restore_count++], TIMELINE_ID_CAPACITY, "%s", t->track_id);
        }
      }
    }
  } else if (!binding->enabled || !binding->restore_known) {
    return fail(message, size, "No saved camera source to restore.");
  }
  /* References are checked even on rebind: orientation needs the predecessor. */
  for (size_t j = 0; j < binding->restore_count; ++j) {
    bool found = false;
    for (size_t i = 0; i < doc.track_count; ++i)
      if (!strcmp(doc.tracks[i].target_id, "camera/main") && driver(doc.tracks[i].property_id) &&
          !strcmp(doc.tracks[i].track_id, binding->restore_xyz_tracks[j])) found = true;
    if (!found) return fail(message, size, "Prior camera source is missing.");
  }
  size_t progress = SIZE_MAX;
  for (size_t i = 0; i < doc.track_count; ++i) {
    TimelineTrack *t = &doc.tracks[i];
    if (strcmp(t->target_id, "camera/main")) continue;
    if (driver(t->property_id)) {
      t->enabled = false;
      for (size_t j = 0; !attach && j < binding->restore_count; ++j)
        if (!strcmp(t->track_id, binding->restore_xyz_tracks[j])) t->enabled = true;
    }
    if (!strcmp(t->property_id, MOTION_CAMERA_PROGRESS_PROPERTY)) {
      progress = i;
      t->enabled = attach;
    }
  }
  if (attach && progress == SIZE_MAX) {
    TimelineTrack track;
    char id[64]; unsigned serial = 1; bool used;
    do {
      snprintf(id, sizeof(id), "camera-route-%u", serial++); used = false;
      for (size_t i = 0; i < doc.track_count; ++i)
        if (!strcmp(doc.tracks[i].track_id, id)) used = true;
    } while (used);
    TimelineTrackInit(&track, id, "camera/main", MOTION_CAMERA_PROGRESS_PROPERTY, TIMELINE_VALUE_SCALAR);
    TimelineTrackSetUnit(&track, TIMELINE_UNIT_UNITLESS);
    TimelineTrackAddKey(&track, doc.range.start_frame, TimelineValueScalar(0), TIMELINE_INTERPOLATION_LINEAR);
    if (doc.range.frame_count > 1)
      TimelineTrackAddKey(&track, doc.range.start_frame + doc.range.frame_count - 1,
          TimelineValueScalar(1), TIMELINE_INTERPOLATION_LINEAR);
    progress = doc.track_count;
    if (TimelineDocumentAddTrack(&doc, &track) != TIMELINE_STATUS_OK)
      return fail(message, size, "Timeline channel capacity reached.");
  }
  binding->enabled = attach;
  if (!SceneEditorMotionPathsCommit(&paths, &doc, revision, message, size)) return false;
  SceneEditorTimelinePause();
  if (attach) SceneEditorTimelineSelectTrack(progress);
  snprintf(message, size, "%s", attach ? "Camera on route; orientation and lens retained." : "Prior camera position source restored.");
  return true;
}
