#include "editor/scene_editor_atmosphere.h"
#include "editor/scene_editor_document.h"
#include "app/scene_atmosphere_runtime.h"
#include "app/scene_atmosphere_assets.h"
#include "scene_editor_document_transaction.h"
#include <stdio.h>

bool SceneEditorAtmosphereRead(SceneAtmosphereAttachment* out) {
    return SceneEditorDocumentIsOpen() && SceneAtmosphereAttachmentParse(
        SceneAtmosphereAuthoring(document_authoring_root()),out,NULL,0);
}
static json_object* member(json_object* root,const char* key) {
    json_object* value=NULL;
    if(!root || !json_object_is_type(root,json_type_object)) return NULL;
    if(!json_object_object_get_ex(root,key,&value)) {
        value=json_object_new_object();json_object_object_add(root,key,value);
    }
    return json_object_is_type(value,json_type_object)?value:NULL;
}
static void preserve_unknown(json_object* previous,json_object* replacement) {
    if(!previous || !replacement || !json_object_is_type(previous,json_type_object) ||
       !json_object_is_type(replacement,json_type_object)) return;
    json_object_object_foreach(previous,key,value) {
        json_object* next=NULL;
        if(!json_object_object_get_ex(replacement,key,&next))
            json_object_object_add(replacement,key,json_object_get(value));
        else if(json_object_is_type(value,json_type_object) && json_object_is_type(next,json_type_object))
            preserve_unknown(value,next);
    }
}
static bool commit(const SceneAtmosphereAttachment* value,unsigned long long revision,char* d,size_t n) {
    if(!SceneEditorDocumentIsOpen() || SceneEditorDocumentRevision()!=revision) {
        if(d && n) snprintf(d,n,"Scene changed; refresh atmosphere before editing.");return false;
    }
    json_object* encoded=NULL;
    if(value) {
        if(!value->present || !(encoded=SceneAtmosphereAttachmentToJson(value))) {
            if(d && n) snprintf(d,n,"Invalid atmosphere attachment.");return false;
        }
    }
    if(!document_begin_command(d,n)) {if(encoded)json_object_put(encoded);return false;}
    json_object* a=member(member(member(document_authoring_root(),"extensions"),"ray_tracing"),"authoring");
    if(!a) {if(encoded)json_object_put(encoded);document_rollback_command();return false;}
    if(encoded) {
        json_object* previous=NULL;json_object_object_get_ex(a,"atmosphere_attachment",&previous);
        preserve_unknown(previous,encoded);
        json_object_object_add(a,"atmosphere_attachment",encoded);
    }
    else json_object_object_del(a,"atmosphere_attachment");
    return document_finish_command(d,n);
}
bool SceneEditorAtmosphereSet(const SceneAtmosphereAttachment* v,unsigned long long rev,char* d,size_t n) {
    if(!SceneAtmosphereAssetsValidate(v,SceneEditorDocumentPath(),d,n)) return false;
    return commit(v,rev,d,n);
}
bool SceneEditorAtmosphereRemove(unsigned long long rev,char* d,size_t n) {return commit(NULL,rev,d,n);}
