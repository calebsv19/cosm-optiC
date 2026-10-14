#include "editor/scene_editor_object_timeline.h"
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
static json_object *retain_intent(json_object *old,json_object *entry){
 if(!old)return entry;json_object *out=NULL;if(json_object_deep_copy(old,&out,NULL)!=0)return entry;
 json_object_object_foreach(entry,key,value){
  if(!strcmp(key,"waypoints")){json_object *prior=NULL;json_object_object_get_ex(old,key,&prior);json_object *rows=json_object_new_array();
   for(size_t i=0;i<json_object_array_length(value);++i){json_object *row=NULL,*v=json_object_array_get_idx(value,i);if(json_object_is_type(prior,json_type_array)&&json_object_array_length(prior)==json_object_array_length(value))json_object_deep_copy(json_object_array_get_idx(prior,i),&row,NULL);if(!row)row=json_object_new_object();json_object_object_foreach(v,k,x){json_object_object_add(row,k,json_object_get(x));}json_object_array_add(rows,row);}json_object_object_add(out,key,rows);
  }else json_object_object_add(out,key,json_object_get(value));
 }json_object_put(entry);return out;
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
  if(entry) entry=retain_intent(MotionPlanFind(a,target),entry);
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
  if (!target || !*target) return fail(m,n,"Choose an attached follower.");
  if (!SceneEditorObjectTimelineEditable(target,m,n)) return false;
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
  if (!target || !*target) return fail(m,n,"Choose an attached follower.");
  if (!SceneEditorObjectTimelineEditable(target,m,n)) return false;
  if (!change(target, NULL, rev, m, n))
    return false;
  if (m && n)
    snprintf(m, n,
             "Original progress keys restored. Limits no longer applied. Undo "
             "is available.");
  return true;
}
