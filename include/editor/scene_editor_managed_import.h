#ifndef SCENE_EDITOR_MANAGED_IMPORT_H
#define SCENE_EDITOR_MANAGED_IMPORT_H
#include <json-c/json.h>
#include <stdbool.h>
#include <stddef.h>
/* One latest asynchronous UI import receipt, scoped to its owning scene.
 * Scene document adoption remains the commit/history/persistence owner. */
void SceneEditorManagedImportStarted(const char *id,const char *scene);
void SceneEditorManagedImportFinished(bool success,const char *message);
void SceneEditorManagedImportCanceled(void);
bool SceneEditorManagedImportRead(const char *id,json_object *response,char *message,size_t size);
const char *SceneEditorManagedImportLatestId(void);
bool SceneEditorManagedImportBegin(const char *source,double scale,unsigned long long revision,char *message,size_t size);
#endif
