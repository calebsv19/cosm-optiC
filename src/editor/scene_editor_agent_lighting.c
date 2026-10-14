#include "editor/scene_editor_agent_lighting.h"
#include "editor/scene_editor_lighting.h"
#include "editor/scene_editor_document.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static json_object* get(json_object* p,const char* key) {json_object* v=NULL;if(json_object_is_type(p,json_type_object)) json_object_object_get_ex(p,key,&v);return v;}
static bool number(json_object* v,double* out) {if(!json_object_is_type(v,json_type_int) && !json_object_is_type(v,json_type_double)) return false;*out=json_object_get_double(v);return isfinite(*out);}
bool SceneEditorAgentLightingReadOnly(const char* name) {return !strcmp(name,"lighting.read");}
bool SceneEditorAgentLightingDispatch(const char* name,json_object* p,json_object* response,char* message,size_t size) {
    if(!json_object_is_type(p,json_type_object)) goto invalid;
    if(!strcmp(name,"lighting.read")) {
        if(json_object_object_length(p)) goto invalid;json_object* read=SceneEditorLightingRead();
        if(strlen(json_object_to_json_string_ext(read,JSON_C_TO_STRING_PLAIN))>48000) {json_object_put(read);snprintf(message,size,"Lighting readback exceeds local control capacity.");return false;}
        json_object_object_add(response,"lighting",read);return true;
    }
    if(!strcmp(name,"lighting.set_primary")) {
        if(json_object_object_length(p)!=4) goto invalid;json_object* a=get(p,"position_m");double xyz[3],intensity,radius;
        if(!json_object_is_type(a,json_type_array) || json_object_array_length(a)!=3 || !number(get(p,"intensity"),&intensity) || !number(get(p,"radius_m"),&radius)) goto invalid;
        for(int i=0;i<3;++i) if(!number(json_object_array_get_idx(a,i),&xyz[i])) goto invalid;
        return SceneEditorLightingSetPrimary(xyz,intensity,radius,SceneEditorDocumentRevision(),message,size);
    }
    if(!strcmp(name,"lighting.set_environment")) {
        if(json_object_object_length(p)!=2 || !get(p,"settings")) goto invalid;
        return SceneEditorLightingSetEnvironment(get(p,"settings"),SceneEditorDocumentRevision(),message,size);
    }
invalid:snprintf(message,size,"Invalid or unsupported lighting command arguments.");return false;
}
