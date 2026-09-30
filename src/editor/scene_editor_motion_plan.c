#include "editor/scene_editor_motion_plan.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_timeline.h"
#include "scene_editor_document_transaction.h"
#include <stdio.h>
#include <string.h>
static json_object *author(void) {
  return MotionPlansAuthor(document_authoring_root());
}
static bool fail(char *m, size_t n, const char *why) {
  if (m && n)
    snprintf(m, n, "%s", why);
  return false;
}
bool SceneEditorMotionPlanRead(const char *target,
                               MotionTimingScheduleRequest *r) {
  return MotionPlanReadRequest(MotionPlanFind(author(), target), r);
}
bool SceneEditorMotionPlanPreview(const char *target,
                                  const MotionTimingScheduleRequest *r,
                                  MotionRouteSchedule *s, char *m, size_t n) {
  json_object *e = MotionPlanCreate(author(), SceneEditorDocumentWorldScale(),
                                    target, r, s, m, n);
  if (!e)
    return false;
  json_object_put(e);
  return true;
}
static bool change(const char *target, json_object *entry,
                   unsigned long long revision, char *m, size_t n) {
  if (!SceneEditorDocumentIsOpen() ||
      revision != SceneEditorDocumentRevision()) {
    if (entry)
      json_object_put(entry);
    return fail(m, n, "Scene changed; reload the movement draft.");
  }
  if (!entry && !MotionPlanFind(author(), target))
    return fail(m, n, "No applied plan to restore.");
  if (!document_begin_command(m, n)) {
    if (entry)
      json_object_put(entry);
    return false;
  }
  json_object *a = author(), *old = NULL, *list = json_object_new_array();
  json_object_object_get_ex(a, "motion_plans", &old);
  for (size_t i = 0; old && i < json_object_array_length(old); ++i) {
    json_object *e = json_object_array_get_idx(old, i), *id = NULL;
    json_object_object_get_ex(e, "target", &id);
    if (strcmp(json_object_get_string(id), target))
      json_object_array_add(list, json_object_get(e));
  }
  if (entry)
    json_object_array_add(list, entry);
  if (json_object_array_length(list))
    json_object_object_add(a, "motion_plans", list);
  else {
    json_object_object_del(a, "motion_plans");
    json_object_put(list);
  }
  if (!document_finish_command(m, n))
    return false;
  SceneEditorTimelinePause();
  return true;
}
bool SceneEditorMotionPlanApply(const char *target,
                                const MotionTimingScheduleRequest *r,
                                unsigned long long rev, char *m, size_t n) {
  if (rev != SceneEditorDocumentRevision())
    return fail(m, n, "Scene changed; reload the movement draft.");
  json_object *entry = MotionPlanCreate(
      author(), SceneEditorDocumentWorldScale(), target, r, NULL, m, n);
  if (!entry)
    return false;
  if (!change(target, entry, rev, m, n))
    return false;
  if (m && n)
    snprintf(m, n,
             "Plan applied. Original progress keys preserved. Undo or Restore "
             "is available.");
  return true;
}
bool SceneEditorMotionPlanRestore(const char *target, unsigned long long rev,
                                  char *m, size_t n) {
  if (!change(target, NULL, rev, m, n))
    return false;
  if (m && n)
    snprintf(m, n,
             "Original progress keys restored. Limits no longer applied. Undo "
             "is available.");
  return true;
}
