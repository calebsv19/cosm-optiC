#include "editor/scene_editor_spectral_profile.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_render_control.h"
#include "editor/scene_editor_command_guard.h"
#include "scene_editor_document_transaction.h"
#include "render/runtime_spectral_scene_profile.h"
#include "scene_editor_receipt_json.h"
#include <stdio.h>
#include <string.h>

static json_object *member(json_object *o,const char *name) {return json_object_object_get(o,name);}
static json_object *ray(json_object *root) {return member(member(root,"extensions"),"ray_tracing");}
static json_object *ensure_ray(json_object *root) {
    const char *keys[]={"extensions","ray_tracing"};
    for(int i=0;i<2;++i) {
        json_object *child=member(root,keys[i]);
        if(!child) {
            child=json_object_new_object();
            if(!child)return NULL;
            if(json_object_object_add(root,keys[i],child)!=0) {json_object_put(child);return NULL;}
        }
        if(!json_object_is_type(child,json_type_object))return NULL;
        root=child;
    }
    return root;
}
json_object *SceneEditorSpectralProfileRead(void) {
    if(!SceneEditorDocumentIsOpen())return NULL;
    json_object *profile=member(ray(document_authoring_root()),"spectral_transport_v1"),*copy=NULL;
    if(profile && (strlen(json_object_to_json_string_ext(profile,JSON_C_TO_STRING_PLAIN))>48000 ||
       json_object_deep_copy(profile,&copy,NULL)!=0)) {if(copy)json_object_put(copy);return NULL;}
    if(!copy)copy=json_tokener_parse("{\"quality_profile\":\"surface_path_v1\"}");
    json_object *out=json_object_new_object();
    if(!out || !SceneEditorReceiptJsonAdd(out,"saved",json_object_new_boolean(profile!=NULL)) ||
       !SceneEditorReceiptJsonAdd(out,"ordinary_default",json_object_new_string("surface_path_v1")) ||
       !SceneEditorReceiptJsonAdd(out,"legacy_optout",json_object_new_string("legacy_direct_v1"))) {
        if(copy)json_object_put(copy);if(out)json_object_put(out);return NULL;
    }
    if(!SceneEditorReceiptJsonAdd(out,"profile",copy)) {json_object_put(out);return NULL;}
    return out;
}
bool SceneEditorSpectralProfileSet(json_object *profile,unsigned long long revision,char *diag,size_t size) {
    if(!SceneEditorDocumentIsOpen() || revision!=SceneEditorDocumentRevision() ||
       SceneEditorRenderActive() || SceneEditorCommandInteractionActive()) {
        snprintf(diag,size,"V3 profile edit needs the current scene revision and an idle editor/render context.");return false;
    }
    if(!json_object_is_type(profile,json_type_object) ||
       strlen(json_object_to_json_string_ext(profile,JSON_C_TO_STRING_PLAIN))>48000) {
        snprintf(diag,size,"V3 profile must be a bounded JSON object.");return false;
    }
    json_object *candidate=NULL,*copy=NULL;
    if(json_object_deep_copy(document_authoring_root(),&candidate,NULL)!=0 ||
       json_object_deep_copy(profile,&copy,NULL)!=0)goto allocation;
    json_object *target=ensure_ray(candidate);
    if(!target || json_object_object_add(target,"spectral_transport_v1",copy)!=0)goto allocation;
    copy=NULL;
    RuntimeSpectralPencil parsed={0};
    bool valid=RuntimeSpectralProfile_FromScene(candidate,SceneEditorDocumentPath(),false,&parsed) &&
        (parsed.surface_path.enabled || parsed.legacy_direct);
    json_object_put(candidate);candidate=NULL;
    if(!valid) {snprintf(diag,size,"Invalid ordinary V3 profile; use surface_path_v1 or explicit legacy_direct_v1. Spectral tables and scene-light ownership must agree.");return false;}
    if(!document_begin_command(diag,size))return false;
    if(json_object_deep_copy(profile,&copy,NULL)!=0 || !ensure_ray(document_authoring_root()) ||
       json_object_object_add(ray(document_authoring_root()),"spectral_transport_v1",copy)!=0) {
        if(copy)json_object_put(copy);copy=NULL;document_rollback_command();goto allocation;
    }
    return document_finish_command(diag,size);
allocation:
    if(copy)json_object_put(copy);if(candidate)json_object_put(candidate);
    snprintf(diag,size,"Cannot allocate the V3 profile transaction.");return false;
}
