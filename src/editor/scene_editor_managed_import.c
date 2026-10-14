#include "editor/scene_editor_managed_import.h"
#include "editor/scene_editor_document.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
static struct {char id[65],scene[PATH_MAX],message[512];const char *state;} receipt;
void SceneEditorManagedImportStarted(const char *id,const char *scene) {
    snprintf(receipt.id,sizeof(receipt.id),"%s",id);
    snprintf(receipt.scene,sizeof(receipt.scene),"%s",scene);
    receipt.message[0]=0;receipt.state="compiling";
}
void SceneEditorManagedImportFinished(bool success,const char *message) {
    if(!receipt.state || strcmp(receipt.state,"compiling")) return;
    receipt.state=success?"completed":"failed";
    snprintf(receipt.message,sizeof(receipt.message),"%s",message?message:"");
}
void SceneEditorManagedImportCanceled(void) {
    if(receipt.state && !strcmp(receipt.state,"compiling")) {
        receipt.state="canceled";snprintf(receipt.message,sizeof(receipt.message),"Import canceled; no candidate adopted.");
    }
}
const char *SceneEditorManagedImportLatestId(void) {return receipt.id;}
bool SceneEditorManagedImportRead(const char *id,json_object *response,char *message,size_t size) {
    if(!id || !receipt.state || strcmp(id,receipt.id) || strcmp(receipt.scene,SceneEditorDocumentPath())) {
        snprintf(message,size,"Import receipt unavailable for this scene; only the latest import is retained.");return false;
    }
    json_object *o=json_object_new_object();
    json_object_object_add(o,"import_id",json_object_new_string(receipt.id));
    json_object_object_add(o,"scene_path",json_object_new_string(receipt.scene));
    json_object_object_add(o,"state",json_object_new_string(receipt.state));
    json_object_object_add(o,"message",json_object_new_string(receipt.message));
    if(!strcmp(receipt.state,"completed")) json_object_object_add(o,"object_id",json_object_new_string(receipt.id));
    json_object_object_add(response,"import",o);return true;
}
