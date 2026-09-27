#include "editor/scene_editor_camera_authoring.h"
#include "editor/scene_editor_document.h"
#include "scene_editor_document_transaction.h"
#include "config/config_manager.h"
#include "config/config_scene_path_io.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static json_object* ensure_object(json_object* parent,const char* key) {
    json_object* value=NULL;
    if(!parent || !json_object_is_type(parent,json_type_object)) return NULL;
    if(json_object_object_get_ex(parent,key,&value)) return json_object_is_type(value,json_type_object)?value:NULL;
    value=json_object_new_object();
    if(value) json_object_object_add(parent,key,value);
    return value;
}
static bool valid_path(const Path* path,const CameraPath3D* depth) {
    if(!path || !depth || path->numPoints<0 || path->numPoints>MAX_BEZIER_POINTS ||
        (path->mode!=BEZIER_QUADRATIC && path->mode!=BEZIER_CUBIC)) return false;
    for(int i=0;i<path->numPoints;++i) {
        if(!isfinite(path->points[i].x) || !isfinite(path->points[i].y) ||
            !isfinite(path->rotations[i]) || !isfinite(depth->point_z[i]) ||
            !isfinite(depth->point_pitch[i]) || fabs(depth->point_pitch[i])>1.5707963267948966) return false;
        if(i+1<path->numPoints) for(int j=0;j<2;++j)
            if(!isfinite(path->handles[i][j].vx) || !isfinite(path->handles[i][j].vy) ||
                !isfinite(depth->handles_vz[i][j])) return false;
    }
    return true;
}
bool SceneEditorDocumentSetCameraPath(const Path* path,const CameraPath3D* depth,
    unsigned long long revision,char* diagnostics,size_t size) {
    double scale=SceneEditorDocumentWorldScale();
    if(!SceneEditorDocumentIsOpen() || revision!=SceneEditorDocumentRevision() ||
        !valid_path(path,depth) || !isfinite(scale) || scale<=0) {
        if(diagnostics && size) snprintf(diagnostics,size,"invalid camera path or stale scene revision");
        return false;
    }
    Path authored=*path;
    CameraPath3D authored_depth=*depth;
    for(int i=0;i<authored.numPoints;++i) {
        authored.points[i].x/=scale;authored.points[i].y/=scale;
        if(i+1<authored.numPoints) for(int j=0;j<2;++j) {
            authored.handles[i][j].vx/=scale;authored.handles[i][j].vy/=scale;
        }
    }
    CameraPath3D_ScaleWorldUnits(&authored_depth,&authored,1.0/scale);
    if(!valid_path(&authored,&authored_depth)) return false;
    json_object* path_json=config_scene_path_to_json_object(&authored);
    json_object* depth_json=CameraPath3D_ToJsonObject(&authored_depth,&authored);
    if(!path_json || !depth_json || !document_begin_command(diagnostics,size)) {
        if(path_json) json_object_put(path_json);
        if(depth_json) json_object_put(depth_json);
        return false;
    }
    json_object* root=ensure_object(ensure_object(ensure_object(document_authoring_root(),"extensions"),"ray_tracing"),"authoring");
    if(!root) {
        json_object_put(path_json);json_object_put(depth_json);document_rollback_command();return false;
    }
    json_object_object_add(root,"camera_path",path_json);
    json_object_object_add(root,"camera_path_depth",depth_json);
    return document_finish_command(diagnostics,size);
}

static struct {
    bool active, retained;
    unsigned long long revision;
    Path path;
    CameraPath3D depth;
} gesture;
bool SceneEditorCameraGestureActive(void) {return gesture.active;}
bool SceneEditorCameraGestureBegin(void) {
    if(gesture.active) return false;
    gesture.active=true;
    gesture.retained=SceneEditorDocumentIsOpen();
    gesture.revision=SceneEditorDocumentRevision();
    gesture.path=sceneSettings.cameraPath;
    gesture.depth=sceneSettings.cameraPath3D;
    return true;
}
bool SceneEditorCameraGestureValid(void) {
    if(!gesture.active) return false;
    if(gesture.retained && (!SceneEditorDocumentIsOpen() || gesture.revision!=SceneEditorDocumentRevision())) {
        gesture.active=false;return false;
    }
    return true;
}
void SceneEditorCameraGestureCancel(void) {
    if(SceneEditorCameraGestureValid()) {
        sceneSettings.cameraPath=gesture.path;
        sceneSettings.cameraPath3D=gesture.depth;
    }
    gesture.active=false;
}
bool SceneEditorCameraGestureCommit(void) {
    if(!SceneEditorCameraGestureValid()) return false;
    if(!memcmp(&gesture.path,&sceneSettings.cameraPath,sizeof(Path)) &&
        !memcmp(&gesture.depth,&sceneSettings.cameraPath3D,sizeof(CameraPath3D))) {
        gesture.active=false;return true;
    }
    char diagnostics[256];
    bool ok=!gesture.retained || SceneEditorDocumentSetCameraPath(&sceneSettings.cameraPath,
        &sceneSettings.cameraPath3D,gesture.revision,diagnostics,sizeof(diagnostics));
    if(!ok) SceneEditorCameraGestureCancel();
    gesture.active=false;
    return ok;
}
