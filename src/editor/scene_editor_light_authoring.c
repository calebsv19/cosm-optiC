#include "editor/scene_editor_light_authoring.h"
#include "editor/scene_editor_document.h"
#include "scene_editor_document_transaction.h"
#include "import/runtime_scene_light_timeline_io.h"
#include "config/config_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool SceneEditorDocumentSetLightPath(const Path* path,const CameraPath3D* depth,
    unsigned long long revision,char* diagnostics,size_t size) {
    if(!path || !depth || !SceneEditorDocumentIsOpen() || revision!=SceneEditorDocumentRevision()) return false;
    RuntimeSceneLightTimelineDocument* light=malloc(sizeof(*light));
    if(!light) return false;
    bool loaded=RuntimeSceneLightTimelineGetLast(light);
    json_object* serialized=NULL;
    if(loaded) {
        light->spatial_path=*path;light->spatial_path_3d=*depth;
        serialized=RuntimeSceneLightTimelineToJsonObject(light,SceneEditorDocumentWorldScale());
    }
    free(light);
    if(!serialized) {
        if(diagnostics && size) snprintf(diagnostics,size,"Light path requires a valid retained light timeline and spatial path.");
        return false;
    }
    if(!document_begin_command(diagnostics,size)) {json_object_put(serialized);return false;}
    json_object *extensions=NULL,*ray=NULL,*authoring=NULL;
    json_object_object_get_ex(document_authoring_root(),"extensions",&extensions);
    if(extensions) json_object_object_get_ex(extensions,"ray_tracing",&ray);
    if(ray) json_object_object_get_ex(ray,"authoring",&authoring);
    if(!json_object_is_type(authoring,json_type_object)) {
        json_object_put(serialized);document_rollback_command();return false;
    }
    /* Keep the legacy spatial carrier during migration. Shared scene_timeline
     * remains the sole owner of temporal channels, and is not rewritten here. */
    json_object_object_add(authoring,"light_timeline",serialized);
    return document_finish_command(diagnostics,size);
}
static struct {
    bool active,retained;
    unsigned long long revision;
    Path path;
    CameraPath3D depth;
} gesture;
bool SceneEditorLightGestureActive(void) {return gesture.active;}
bool SceneEditorLightGestureBegin(void) {
    if(gesture.active) return false;
    gesture.active=true;gesture.retained=SceneEditorDocumentIsOpen();
    gesture.revision=SceneEditorDocumentRevision();
    gesture.path=sceneSettings.bezierPath;gesture.depth=sceneSettings.bezierPath3D;
    return true;
}
bool SceneEditorLightGestureValid(void) {
    if(gesture.active && gesture.retained &&
       (!SceneEditorDocumentIsOpen() || gesture.revision!=SceneEditorDocumentRevision())) gesture.active=false;
    return gesture.active;
}
void SceneEditorLightGestureCancel(void) {
    if(SceneEditorLightGestureValid()) {
        sceneSettings.bezierPath=gesture.path;sceneSettings.bezierPath3D=gesture.depth;
    }
    gesture.active=false;
}
bool SceneEditorLightGestureCommit(void) {
    if(!SceneEditorLightGestureValid()) return false;
    if(!memcmp(&gesture.path,&sceneSettings.bezierPath,sizeof(Path)) &&
       !memcmp(&gesture.depth,&sceneSettings.bezierPath3D,sizeof(CameraPath3D))) {
        gesture.active=false;return true;
    }
    char diagnostics[256];
    bool ok=!gesture.retained || SceneEditorDocumentSetLightPath(&sceneSettings.bezierPath,
        &sceneSettings.bezierPath3D,gesture.revision,diagnostics,sizeof(diagnostics));
    if(!ok) SceneEditorLightGestureCancel();
    gesture.active=false;return ok;
}
