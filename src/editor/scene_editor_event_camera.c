#include "editor/scene_editor_event_camera.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_timeline.h"
#include "import/runtime_event_camera.h"
#include "scene_editor_document_transaction.h"
#include <stdio.h>
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
static bool fail(char *d, size_t n, const char *s) {
  if (d && n)
    snprintf(d, n, "%s", s);
  return false;
}
bool SceneEditorEventCameraCandidates(json_object *p, json_object *response,
                                      char *d, size_t n) {
  if (!SceneEditorDocumentIsOpen() ||
      !json_object_is_type(p, json_type_object) ||
      json_object_object_length(p) != 1 || !get(p, "intent"))
    return fail(d, n, "Expected one event intent on the open scene.");
  json_object *proposals = RuntimeEventCameraPropose(document_authoring_root(),
                                                     get(p, "intent"), d, n);
  if (!proposals)
    return false;
  json_object_object_add(proposals, "revision",
                         json_object_new_int64(SceneEditorDocumentRevision()));
  json_object_object_add(response, "event_candidates", proposals);
  return true;
}
bool SceneEditorEventCameraCommand(const char *action, json_object *p,
                                   unsigned long long rev, char *d, size_t n) {
  bool select = !strcmp(action, "event_select"),
       remove = !strcmp(action, "event_remove");
  if ((!select && !remove) || !json_object_is_type(p, json_type_object) ||
      json_object_object_length(p) != (select ? 3 : 1) ||
      !SceneEditorDocumentIsOpen() || SceneEditorDocumentRevision() != rev)
    return fail(d, n, "Stale event command or invalid fields.");
  json_object *entry = NULL;
  if (select) {
    json_object *index = get(p, "candidate");
    if (!json_object_is_type(index, json_type_int) ||
        json_object_get_int(index) < 0 || json_object_get_int(index) > 2 ||
        strlen(str(p, "proposal_sha256")) != 64 || !get(p, "intent"))
      return fail(d, n,
                  "Expected intent, candidate and reviewed proposal hash.");
    entry = RuntimeEventCameraSelect(
        document_authoring_root(), get(p, "intent"), json_object_get_int(index),
        str(p, "proposal_sha256"), d, n);
    if (!entry)
      return false;
  }
  if (!document_begin_command(d, n)) {
    if (entry)
      json_object_put(entry);
    return false;
  }
  bool ok = false;
  if (select) {
    ok = RuntimeEventCameraAttach(document_authoring_root(), entry, d, n);
    json_object_put(entry);
  } else {
    json_object *a = get(get(get(document_authoring_root(), "extensions"),
                             "ray_tracing"),
                         "authoring"),
                *list = get(a, "event_cameras");
    for (size_t i = 0; list && i < json_object_array_length(list); i++)
      if (!strcmp(str(json_object_array_get_idx(list, i), "id"),
                  str(p, "id"))) {
        json_object_array_del_idx(list, i, 1);
        if (!json_object_array_length(list))
          json_object_object_del(a, "event_cameras");
        ok = true;
        break;
      }
  }
  if (!ok) {
    document_rollback_command();
    return false;
  }
  if (!document_finish_command(d, n))
    return false;
  SceneEditorTimelinePause();
  return true;
}
