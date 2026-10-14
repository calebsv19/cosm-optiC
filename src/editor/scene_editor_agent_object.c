/* Stable-ID adapter only; UI document commands own transactions and persistence. */
#include "editor/scene_editor_agent_object.h"
#include "editor/scene_editor_document.h"
#include "import/runtime_scene_bridge.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_object *member(json_object *o,const char *key) {
    json_object *v=NULL;
    if(json_object_is_type(o,json_type_object)) json_object_object_get_ex(o,key,&v);
    return v;
}
static const char *string(json_object *o,const char *key) {
    json_object *v=member(o,key);
    if(!json_object_is_type(v,json_type_string)) return NULL;
    const char *s=json_object_get_string(v);
    return strlen(s)==(size_t)json_object_get_string_len(v)?s:NULL;
}
static bool fields(json_object *p,const char *const *keys,size_t count) {
    if(!json_object_is_type(p,json_type_object) || json_object_object_length(p)!=(int)count) return false;
    for(size_t i=0;i<count;++i) if(!member(p,keys[i])) return false;
    return true;
}
static bool vector(json_object *p,const char *key,double out[3]) {
    json_object *a=member(p,key);
    if(!json_object_is_type(a,json_type_array) || json_object_array_length(a)!=3) return false;
    for(int i=0;i<3;++i) {
        json_object *v=json_object_array_get_idx(a,i);
        if(!json_object_is_type(v,json_type_int) && !json_object_is_type(v,json_type_double)) return false;
        out[i]=json_object_get_double(v);if(!isfinite(out[i])) return false;
    }
    return true;
}
static bool bounded(json_object *v,char *message,size_t size) {
    if(strlen(json_object_to_json_string_ext(v,JSON_C_TO_STRING_PLAIN))<=48000) return true;
    snprintf(message,size,"Object readback exceeds local control capacity; scene unchanged.");return false;
}
bool SceneEditorAgentObjectReadOnly(const char *name) {
    return !strcmp(name,"object.list") || !strcmp(name,"object.read");
}
static void result_id(json_object *response,int index) {
    char id[128];
    if(runtime_scene_bridge_get_last_object_id_for_scene_index(index,id,sizeof(id)))
        json_object_object_add(response,"object_id",json_object_new_string(id));
}
bool SceneEditorAgentObjectDispatch(const char *name,json_object *p,json_object *response,char *message,size_t size) {
    if(!strcmp(name,"object.list")) {
        if(!fields(p,NULL,0)) goto invalid;
        json_object *list=json_object_new_array();
        for(int i=0;i<SceneEditorDocumentObjectCount();++i) {
            SceneEditorDocumentObjectInfo info;if(!SceneEditorDocumentObjectAt(i,&info)) continue;
            json_object *o=json_object_new_object();
            json_object_object_add(o,"object_id",json_object_new_string(info.id));
            json_object_object_add(o,"name",json_object_new_string(info.name));
            json_object_object_add(o,"type",json_object_new_string(info.type));
            json_object_object_add(o,"visible",json_object_new_boolean(info.visible));
            json_object_object_add(o,"locked",json_object_new_boolean(info.locked));
            json_object_object_add(o,"has_runtime_instance",json_object_new_boolean(info.runtime_index>=0));
            json_object_array_add(list,o);
        }
        if(!bounded(list,message,size)) {json_object_put(list);return false;}
        json_object_object_add(response,"objects",list);return true;
    }
    if(!strcmp(name,"object.create")) {
        const char *keys[]={"kind","expected_revision"};const char *kind=string(p,"kind");int index=-1;
        if(!fields(p,keys,2) || !kind) goto invalid;
        bool ok=SceneEditorDocumentCreatePrimitive(kind,&index,message,size);
        if(ok) result_id(response,index);
        return ok;
    }
    const char *id=string(p,"object_id");SceneEditorDocumentObjectInfo info;
    if(!id || !id[0] || !SceneEditorDocumentObjectById(id,&info)) {
        snprintf(message,size,"Unknown or invalid stable object_id.");return false;
    }
    if(!strcmp(name,"object.read")) {
        const char *keys[]={"object_id"};if(!fields(p,keys,1)) goto invalid;
        size_t length=SceneEditorDocumentObjectJSONSize(id);
        if(!length || length>48000) {snprintf(message,size,"Object readback exceeds local control capacity or is unavailable.");return false;}
        char *text=malloc(length);
        if(!text || !SceneEditorDocumentGetObjectJSON(id,text,length)) {free(text);goto invalid;}
        json_object *o=json_tokener_parse(text);free(text);if(!o) goto invalid;
        json_object_object_add(response,"object",o);return true;
    }
    unsigned long long revision=SceneEditorDocumentRevision();
    if(!strcmp(name,"object.rename")) {
        const char *keys[]={"object_id","name","expected_revision"};const char *label=string(p,"name");
        if(!fields(p,keys,3) || !label) goto invalid;
        return SceneEditorDocumentRenameById(id,label,revision,message,size);
    }
    if(!strcmp(name,"object.set_flag")) {
        const char *keys[]={"object_id","flag","value","expected_revision"};const char *flag=string(p,"flag");json_object *v=member(p,"value");
        if(!fields(p,keys,4) || !flag || !json_object_is_type(v,json_type_boolean)) goto invalid;
        return SceneEditorDocumentSetFlag(id,flag,json_object_get_boolean(v),revision,message,size);
    }
    if(info.runtime_index<0) {snprintf(message,size,"Object has no editable native runtime instance.");return false;}
    if(!strcmp(name,"object.set_transform")) {
        const char *keys[]={"object_id","position","rotation_degrees","scale","expected_revision"};SceneEditorDocumentTransform t;
        if(!fields(p,keys,5) || !vector(p,"position",t.position) || !vector(p,"rotation_degrees",t.rotation_degrees) || !vector(p,"scale",t.scale)) goto invalid;
        return SceneEditorDocumentSetTransformForSceneIndex(info.runtime_index,&t,message,size);
    }
    const char *keys[]={"object_id","expected_revision"};if(!fields(p,keys,2)) goto invalid;
    if(!strcmp(name,"object.duplicate")) {
        int index=-1;bool ok=SceneEditorDocumentDuplicateForSceneIndex(info.runtime_index,&index,message,size);
        if(ok) result_id(response,index);
        return ok;
    }
    if(!strcmp(name,"object.remove")) return SceneEditorDocumentRemoveForSceneIndex(info.runtime_index,message,size);
invalid:
    snprintf(message,size,"Invalid or unsupported object command arguments.");return false;
}
