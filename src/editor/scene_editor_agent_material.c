#include "editor/scene_editor_optical_response.h"
/* Thin stable-ID adapter. The retained document remains the transaction,
 * validation, runtime publication, history and persistence owner. */
#include "editor/scene_editor_agent_material.h"
#include "editor/scene_editor_document.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_object *member(json_object *o, const char *key) {
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
bool SceneEditorAgentMaterialReadOnly(const char *name) {
    return !strcmp(name,"material.response_read") || !strcmp(name,"material.list") || !strcmp(name,"material.read");
}
static bool fields(json_object *p,const char *const *names,size_t count) {
    if(!json_object_is_type(p,json_type_object) || json_object_object_length(p)!=(int)count) return false;
    for(size_t i=0;i<count;++i) if(!member(p,names[i])) return false;
    return true;
}
static bool bounded(json_object *value,char *message,size_t size) {
    if(strlen(json_object_to_json_string_ext(value,JSON_C_TO_STRING_PLAIN))<=48000) return true;
    snprintf(message,size,"Material readback exceeds local control capacity; retained source is unchanged.");
    return false;
}
bool SceneEditorAgentMaterialDispatch(const char *name,json_object *p,json_object *response,
    char *message,size_t size) {
    if(!strcmp(name,"material.list")) {
        if(!fields(p,NULL,0)) goto invalid;
        json_object *list=json_object_new_array();
        for(int i=0;i<SceneEditorDocumentObjectCount();++i) {
            SceneEditorDocumentObjectInfo info;
            if(!SceneEditorDocumentObjectAt(i,&info)) continue;
            json_object *o=json_object_new_object();
            json_object_object_add(o,"object_id",json_object_new_string(info.id));
            json_object_object_add(o,"name",json_object_new_string(info.name));
            json_object_object_add(o,"type",json_object_new_string(info.type));
            json_object_object_add(o,"locked",json_object_new_boolean(info.locked));
            json_object_object_add(o,"has_material_row",json_object_new_boolean(SceneEditorDocumentSurfaceMaterialJSONSize(info.runtime_index)>0));
            json_object_array_add(list,o);
        }
        if(!bounded(list,message,size)) {json_object_put(list);return false;}
        json_object_object_add(response,"objects",list);
        json_object *presets=json_tokener_parse("[\"noise3d\",\"triplanar_checker\",\"solid\",\"composition\"]");
        json_object_object_add(response,"presets",presets);return true;
    }
    const char *id=string(p,"object_id");SceneEditorDocumentObjectInfo info;
    if(!id || !id[0] || !SceneEditorDocumentObjectById(id,&info)) {
        snprintf(message,size,"Unknown or invalid stable object_id.");return false;
    }
    if(!strcmp(name,"material.response_read")) {
        const char *keys[]={"object_id"};if(!fields(p,keys,1))goto invalid;
        json_object *read=SceneEditorOpticalResponseRead(info.runtime_index,message,size);if(!read)return false;
        if(!bounded(read,message,size)){json_object_put(read);return false;}
        json_object_object_add(response,"response",read);return true;
    }
    if(!strcmp(name,"material.response_set")) {
        const char *keys[]={"object_id","settings","expected_revision"};if(!fields(p,keys,3))goto invalid;
        return SceneEditorOpticalResponseSet(info.runtime_index,member(p,"settings"),SceneEditorDocumentRevision(),message,size);
    }
    if(!strcmp(name,"material.read")) {
        const char *keys[]={"object_id"};if(!fields(p,keys,1)) goto invalid;
        size_t length=SceneEditorDocumentSurfaceMaterialJSONSize(info.runtime_index);
        if(length>48000 || SceneEditorDocumentSurfaceMappingJSONSize(info.runtime_index)>48000) {snprintf(message,size,"Material readback exceeds local control capacity; retained source is unchanged.");return false;}
        char *text=length?malloc(length):NULL;json_object *row=NULL;
        if(length) {
            if(!text || !SceneEditorDocumentGetSurfaceMaterialJSON(info.runtime_index,text,length)) {free(text);goto invalid;}
            row=json_tokener_parse(text);free(text);if(!row) goto invalid;
        }
        char mapping[48001];
        json_object *result=json_object_new_object();
        json_object_object_add(result,"object_id",json_object_new_string(info.id));
        json_object_object_add(result,"locked",json_object_new_boolean(info.locked));
        json_object_object_add(result,"retained",row); /* NULL means legacy/rowless, never synthesizes source. */
        json_object_object_add(result,"mapping",SceneEditorDocumentGetSurfaceMappingJSON(info.runtime_index,mapping,sizeof(mapping))?json_tokener_parse(mapping):NULL);
        if(!bounded(result,message,size)) {json_object_put(result);return false;}
        json_object_object_add(response,"material",result);return true;
    }
    unsigned long long revision=SceneEditorDocumentRevision();
    if(!strcmp(name,"material.preset")) {
        const char *keys[]={"object_id","preset","replace","expected_revision"};
        const char *preset=string(p,"preset");json_object *replace=member(p,"replace");
        if(!fields(p,keys,4) || !preset || !json_object_is_type(replace,json_type_boolean)) goto invalid;
        return SceneEditorDocumentMaterialPreset(info.runtime_index,preset,json_object_get_boolean(replace),revision,message,size);
    }
    if(!strcmp(name,"material.assign")) {
        const char *keys[]={"object_id","source_object_id","expected_revision"};
        const char *source=string(p,"source_object_id");SceneEditorDocumentObjectInfo from;
        if(!fields(p,keys,3) || !source || !SceneEditorDocumentObjectById(source,&from)) goto invalid;
        return SceneEditorDocumentMaterialAssign(info.runtime_index,from.runtime_index,revision,message,size);
    }
    if(!strcmp(name,"material.set_graph")) {
        const char *keys[]={"object_id","graph","expected_revision"};json_object *graph=member(p,"graph");
        if(!fields(p,keys,3) || !json_object_is_type(graph,json_type_object)) goto invalid;
        return SceneEditorDocumentSetSurfaceGraph(info.runtime_index,json_object_to_json_string_ext(graph,JSON_C_TO_STRING_PLAIN),revision,message,size);
    }
    if(!strcmp(name,"material.set_layer_value")) {
        const char *keys[]={"object_id","layer_id","group","property","value","expected_revision"};
        const char *layer=string(p,"layer_id"),*group=string(p,"group"),*property=string(p,"property");
        json_object *value=member(p,"value");
        if(!fields(p,keys,6) || !layer || !group || !property ||
            (!json_object_is_type(value,json_type_double) && !json_object_is_type(value,json_type_int)) ||
            !isfinite(json_object_get_double(value))) goto invalid;
        return SceneEditorDocumentSetSurfaceLayerValue(info.runtime_index,layer,group,property,json_object_get_double(value),revision,message,size);
    }
    const char *keys[]={"object_id","expected_revision"};if(!fields(p,keys,2)) goto invalid;
    if(!strcmp(name,"material.duplicate")) return SceneEditorDocumentMaterialDuplicate(info.runtime_index,revision,message,size);
    if(!strcmp(name,"material.reset_source")) return SceneEditorDocumentMaterialResetSource(info.runtime_index,revision,message,size);
    if(!strcmp(name,"material.reset_mapping")) return SceneEditorDocumentMaterialResetMapping(info.runtime_index,revision,message,size);
invalid:
    snprintf(message,size,"Invalid or unsupported material command arguments.");return false;
}
