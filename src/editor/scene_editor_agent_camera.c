/* MCP argument/readback adapter; shared static UI pose command owns edits. */
#include "editor/scene_editor_agent_camera.h"
#include "editor/scene_editor_camera_authoring.h"
#include "editor/scene_editor_document.h"
#include "scene_editor_document_transaction.h"
#include "config/config_manager.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static json_object* member(json_object* o,const char* key) {
    json_object* v=NULL;if(json_object_is_type(o,json_type_object)) json_object_object_get_ex(o,key,&v);return v;
}
static bool fields(json_object* p,const char*const* names,size_t count) {
    if(!json_object_is_type(p,json_type_object) || json_object_object_length(p)!=(int)count) return false;
    for(size_t i=0;i<count;++i) if(!member(p,names[i])) return false;return true;
}
static bool number(json_object* v,double* out) {
    if(!json_object_is_type(v,json_type_int) && !json_object_is_type(v,json_type_double)) return false;
    *out=json_object_get_double(v);return isfinite(*out);
}
static json_object* vector(const double v[3]) {
    json_object* a=json_object_new_array();for(int i=0;i<3;++i) json_object_array_add(a,json_object_new_double(v[i]));return a;
}
bool SceneEditorAgentCameraReadOnly(const char* name) {return !strcmp(name,"camera.read");}
bool SceneEditorAgentCameraDispatch(const char* name,json_object* p,json_object* response,char* message,size_t size) {
    if(!strcmp(name,"camera.read")) {
        if(!fields(p,NULL,0)) goto invalid;
        double xyz[3],yaw,pitch;char reason[256];
        if(!SceneEditorDocumentGetStaticCameraPose(xyz,&yaw,&pitch)) goto invalid;
        json_object* camera=json_object_new_object();
        bool editable=SceneEditorDocumentStaticCameraEditable(reason,sizeof(reason));
        json_object_object_add(camera,"static_editable",json_object_new_boolean(editable));
        json_object_object_add(camera,"refusal_reason",json_object_new_string(reason));
        json_object_object_add(camera,"path_points",json_object_new_int(sceneSettings.cameraPath.numPoints));
        json_object_object_add(camera,"position_m",vector(xyz));
        json_object_object_add(camera,"yaw_degrees",json_object_new_double(yaw));
        json_object_object_add(camera,"pitch_degrees",json_object_new_double(pitch));
        json_object_object_add(camera,"world_scale",json_object_new_double(SceneEditorDocumentWorldScale()));
        json_object* author=member(member(member(document_authoring_root(),"extensions"),"ray_tracing"),"authoring");
        json_object* retained=json_object_new_object();
        for(int i=0;i<2;++i) {const char* key=i?"camera_path_depth":"camera_path";json_object_object_add(retained,key,json_object_get(member(author,key)));}
        json_object_object_add(camera,"retained",retained);
        if(strlen(json_object_to_json_string_ext(camera,JSON_C_TO_STRING_PLAIN))>48000) {json_object_put(camera);snprintf(message,size,"Camera readback exceeds local control capacity.");return false;}
        json_object_object_add(response,"camera",camera);return true;
    }
    if(!strcmp(name,"camera.set_pose")) {
        const char* keys[]={"position_m","yaw_degrees","pitch_degrees","expected_revision"};double xyz[3],yaw,pitch;
        json_object* a=member(p,"position_m");
        if(!fields(p,keys,4) || !json_object_is_type(a,json_type_array) || json_object_array_length(a)!=3 || !number(member(p,"yaw_degrees"),&yaw) || !number(member(p,"pitch_degrees"),&pitch)) goto invalid;
        for(int i=0;i<3;++i) if(!number(json_object_array_get_idx(a,i),&xyz[i])) goto invalid;
        return SceneEditorDocumentSetStaticCameraPose(xyz,yaw,pitch,SceneEditorDocumentRevision(),message,size);
    }
invalid:
    snprintf(message,size,"Invalid or unsupported camera command arguments.");return false;
}
