/* Stable-ID inspection and thin adapter to the existing asynchronous UI import. */
#include "editor/scene_editor_agent_mesh.h"
#include "editor/scene_editor_managed_import.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_mesh_preview_store.h"
#include "scene_editor_document_transaction.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static json_object *get(json_object *o,const char *key) {
    json_object *v=NULL;if(json_object_is_type(o,json_type_object)) json_object_object_get_ex(o,key,&v);return v;
}
static const char *string(json_object *o,const char *key) {
    json_object *v=get(o,key);if(!json_object_is_type(v,json_type_string)) return NULL;
    const char *s=json_object_get_string(v);return strlen(s)==(size_t)json_object_get_string_len(v)?s:NULL;
}
static bool fields(json_object *p,const char *const *keys,size_t count) {
    if(!json_object_is_type(p,json_type_object) || json_object_object_length(p)!=(int)count) return false;
    for(size_t i=0;i<count;i++) if(!get(p,keys[i])) return false;return true;
}
static json_object *vector(CoreObjectVec3 v) {
    json_object *o=json_object_new_array();json_object_array_add(o,json_object_new_double(v.x));
    json_object_array_add(o,json_object_new_double(v.y));json_object_array_add(o,json_object_new_double(v.z));return o;
}
bool SceneEditorAgentMeshReadOnly(const char *name) {return !strcmp(name,"mesh.read") || !strcmp(name,"mesh.import_status");}
bool SceneEditorAgentMeshDispatch(const char *name,json_object *p,json_object *response,char *message,size_t size) {
    if(!strcmp(name,"mesh.import")) {
        const char *keys[]={"source_path","source_to_asset_scale","expected_revision"};const char *path=string(p,"source_path");json_object *v=get(p,"source_to_asset_scale");
        if(!fields(p,keys,3) || !path || !(json_object_is_type(v,json_type_double)||json_object_is_type(v,json_type_int))) goto invalid;
        if(!SceneEditorManagedImportBegin(path,json_object_get_double(v),SceneEditorDocumentRevision(),message,size)) return false;
        return SceneEditorManagedImportRead(SceneEditorManagedImportLatestId(),response,message,size);
    }
    if(!strcmp(name,"mesh.import_status")) {
        const char *keys[]={"import_id"};if(!fields(p,keys,1)) goto invalid;
        return SceneEditorManagedImportRead(string(p,"import_id"),response,message,size);
    }
    if(strcmp(name,"mesh.read")) goto invalid;
    const char *keys[]={"object_id"};const char *id=string(p,"object_id");SceneEditorDocumentObjectInfo info;
    if(!fields(p,keys,1) || !id || !SceneEditorDocumentObjectById(id,&info)) goto invalid;
    json_object *root=document_authoring_root(),*objects=get(root,"objects"),*object=NULL;
    for(size_t i=0;i<json_object_array_length(objects);i++) {
        json_object *o=json_object_array_get_idx(objects,i);const char *key=string(o,"object_id");if(key && !strcmp(key,id)) {object=o;break;}
    }
    json_object *binding=get(get(get(object,"extensions"),"ray_tracing"),"managed_mesh");
    const char *asset_id=string(binding,"asset_id");
    json_object *catalog=get(get(get(root,"extensions"),"ray_tracing"),"managed_mesh_assets");
    json_object *asset=asset_id?get(get(catalog,"assets"),asset_id):NULL;
    json_object *o=json_object_new_object();
    json_object_object_add(o,"object_id",json_object_new_string(id));
    json_object_object_add(o,"managed",json_object_new_boolean(asset!=NULL && binding!=NULL));
    json_object_object_add(o,"asset",asset?json_object_get(asset):NULL);
    json_object_object_add(o,"binding",binding?json_object_get(binding):NULL);
    json_object_object_add(o,"topology_validation",json_object_new_string("not_performed"));
    json_object_object_add(o,"asset_files_checked",json_object_new_boolean(false));
    bool found=false;
    for(int i=0;i<SceneEditorMeshPreviewStoreInstanceCount();i++) {
        const RayTracingRuntimeMeshAssetInstance *instance=SceneEditorMeshPreviewStoreGetInstance(i);
        if(!instance || strcmp(instance->object_id,id)) continue;
        const CoreMeshAssetRuntimeContract *c=SceneEditorMeshPreviewStoreGetContract(instance->asset_index);
        if(!c) continue;
        json_object_object_add(o,"vertex_count",json_object_new_int64(c->vertex_count));
        json_object_object_add(o,"triangle_count",json_object_new_int64(c->triangle_count));
        json_object_object_add(o,"local_bounds_min",vector(c->local_bounds.min));
        json_object_object_add(o,"local_bounds_max",vector(c->local_bounds.max));
        json_object_object_add(o,"preview_bounds_fallback",json_object_new_boolean(SceneEditorMeshPreviewStoreInstanceUsesBoundsFallback(i)));
        found=true;break;
    }
    json_object_object_add(o,"runtime_available",json_object_new_boolean(found));
    if(strlen(json_object_to_json_string_ext(o,JSON_C_TO_STRING_PLAIN))>48000) {
        json_object_put(o);snprintf(message,size,"Mesh inspection exceeds local control capacity; scene unchanged.");return false;
    }
    json_object_object_add(response,"mesh",o);return true;
invalid:
    snprintf(message,size,"Invalid or unsupported managed mesh arguments.");return false;
}
