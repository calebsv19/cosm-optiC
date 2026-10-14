/* Static pose adapter to the retained UI camera-path command. */
#include "editor/scene_editor_camera_authoring.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_document_timeline.h"
#include "motion/scene_motion_paths.h"
#include "config/config_manager.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool refuse(char* message,size_t size,const char* reason) {
    if(message && size) snprintf(message,size,"%s",reason);return false;
}
bool SceneEditorDocumentStaticCameraEditable(char* message,size_t size) {
    if(!SceneEditorDocumentIsOpen()) return refuse(message,size,"No open scene.");
    if(sceneSettings.cameraPath.numPoints>1) return refuse(message,size,"Animated camera path: static pose editing is refused.");
    MotionPathBinding binding;
    if(MotionPathsRuntimeBinding("camera/main",&binding)) return refuse(message,size,"Camera motion binding: static pose editing is refused.");
    TimelineDocument *doc=calloc(1,sizeof(*doc));
    if(!doc) return refuse(message,size,"Camera timeline validation allocation failed.");
    TimelineStatus status=SceneEditorDocumentGetTimeline(doc);bool animated=false;
    if(status==TIMELINE_STATUS_OK) for(size_t i=0;i<doc->track_count;++i)
        if(doc->tracks[i].key_count && !strncmp(doc->tracks[i].property_id,"camera/",7)) animated=true;
    free(doc);
    if(status!=TIMELINE_STATUS_OK && status!=TIMELINE_STATUS_TARGET_NOT_FOUND)
        return refuse(message,size,"Camera timeline could not be validated.");
    if(animated) return refuse(message,size,"Camera timeline channels: static pose editing is refused.");
    if(message && size) message[0]=0;return true;
}
bool SceneEditorDocumentGetStaticCameraPose(double position[3],double* yaw,double* pitch) {
    if(!SceneEditorDocumentIsOpen() || !position || !yaw || !pitch) return false;
    bool path=sceneSettings.cameraPath.numPoints>0;
    position[0]=path?sceneSettings.cameraPath.points[0].x:sceneSettings.camera.x;
    position[1]=path?sceneSettings.cameraPath.points[0].y:sceneSettings.camera.y;
    position[2]=path?sceneSettings.cameraPath3D.point_z[0]:sceneSettings.cameraZ;
    *yaw=(path?sceneSettings.cameraPath.rotations[0]:sceneSettings.camera.rotation)*57.29577951308232;
    *pitch=path?sceneSettings.cameraPath3D.point_pitch[0]*57.29577951308232:0;
    return isfinite(position[0]) && isfinite(position[1]) && isfinite(position[2]) && isfinite(*yaw) && isfinite(*pitch);
}
bool SceneEditorDocumentSetStaticCameraPose(const double position[3],double yaw,double pitch,
    unsigned long long revision,char* message,size_t size) {
    if(!position || !isfinite(position[0]) || !isfinite(position[1]) || !isfinite(position[2]) ||
       !isfinite(yaw) || !isfinite(pitch) || pitch<-90 || pitch>90 || revision!=SceneEditorDocumentRevision())
        return refuse(message,size,"Invalid static camera pose or stale scene revision.");
    if(!SceneEditorDocumentStaticCameraEditable(message,size)) return false;
    Path path=sceneSettings.cameraPath;CameraPath3D depth=sceneSettings.cameraPath3D;
    if(!path.numPoints) {memset(&path,0,sizeof(path));memset(&depth,0,sizeof(depth));path.mode=BEZIER_CUBIC;}
    path.numPoints=1;path.points[0].x=position[0];path.points[0].y=position[1];depth.point_z[0]=position[2];
    path.rotationSet[0]=true;path.rotations[0]=yaw/57.29577951308232;depth.point_pitch[0]=pitch/57.29577951308232;
    return SceneEditorDocumentSetCameraPath(&path,&depth,revision,message,size);
}
