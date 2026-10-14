#include "editor/scene_editor_optical_response.h"
#include "editor/scene_editor_document.h"
#include "scene_editor_document_transaction.h"
#include "config/config_manager.h"
#include "scene/object_manager.h"
#include "material/material_manager.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static json_object *get(json_object *o,const char *key) {json_object *v=NULL;if(json_object_is_type(o,json_type_object))json_object_object_get_ex(o,key,&v);return v;}
static json_object *ensure(json_object *o,const char *key) {
    if(!json_object_is_type(o,json_type_object))return NULL;
    json_object *v=get(o,key);if(v)return json_object_is_type(v,json_type_object)?v:NULL;
    v=json_object_new_object();json_object_object_add(o,key,v);return v;
}
static json_object *rows(bool create) {
    json_object *root=document_authoring_root(),*ext=create?ensure(root,"extensions"):get(root,"extensions");
    json_object *ray=create?ensure(ext,"ray_tracing"):get(ext,"ray_tracing");
    json_object *author=create?ensure(ray,"authoring"):get(ray,"authoring");if(!author)return NULL;
    json_object *v=get(author,"object_materials");if(!v && create){v=json_object_new_array();json_object_object_add(author,"object_materials",v);}return json_object_is_type(v,json_type_array)?v:NULL;
}
static json_object *row_for(const char *id,bool create,int index) {
    json_object *list=rows(create);if(!list)return NULL;
    for(size_t i=0;i<json_object_array_length(list);i++) {
        json_object *row=json_object_array_get_idx(list,i);const char *key=json_object_get_string(get(row,"object_id"));if(key && !strcmp(key,id))return row;
    }
    if(!create)return NULL;
    const SceneObject *obj=&sceneSettings.sceneObjects[index];json_object *row=json_object_new_object();
    json_object_object_add(row,"object_id",json_object_new_string(id));json_object_object_add(row,"material_id",json_object_new_int(obj->material_id));
    json_object_object_add(row,"object_color",json_object_new_int(obj->color));json_object_object_add(row,"roughness",json_object_new_double(obj->roughness));
    json_object_object_add(row,"reflectivity",json_object_new_double(obj->reflectivity));json_object_object_add(row,"alpha",json_object_new_double(obj->alpha));
    json_object_array_add(list,row);return row;
}
static const char *object_id(int index) {
    for(int i=0;i<SceneEditorDocumentObjectCount();i++) {static SceneEditorDocumentObjectInfo info;if(SceneEditorDocumentObjectAt(i,&info) && info.runtime_index==index)return info.id;}return NULL;
}
static bool valid_index(int index) {return index>=0 && index<sceneSettings.objectCount && object_id(index);}
static void put_double(json_object *o,const char *key,double value) {json_object_object_add(o,key,json_object_new_double(value));}
static void set_alias(json_object *row,const char *key,const char *alias,json_object *v) {
    json_object_object_del(row,alias);if(v)json_object_object_add(row,key,json_object_get(v));else json_object_object_del(row,key);
}
json_object *SceneEditorOpticalResponseRead(int index,char *message,size_t size) {
    if(!valid_index(index)){snprintf(message,size,"Object has no native optical response.");return NULL;}
    SceneObject *obj=&sceneSettings.sceneObjects[index];double transmission,ior,distance;bool thin;
    SceneObjectResolveGlassTransport(obj,&transmission,&ior,&distance,&thin);
    json_object *row=row_for(object_id(index),false,index),*o=json_object_new_object(),*effective=json_object_new_object();
    json_object_object_add(o,"family",json_object_new_string(obj->material_id==MATERIAL_PRESET_TRANSPARENT?"glass":"other"));
    json_object_object_add(o,"material_id",json_object_new_int(obj->material_id));
    json_object_object_add(o,"retained",row?json_tokener_parse(json_object_to_json_string_ext(row,JSON_C_TO_STRING_PLAIN)):NULL);
    put_double(effective,"transmission",transmission);put_double(effective,"ior",ior);put_double(effective,"absorption_distance",distance);
    put_double(effective,"base_roughness",obj->roughness);
    json_object_object_add(effective,"thin_walled",json_object_new_boolean(thin));
    json_object_object_add(effective,"interface_tint",json_object_new_int(obj->hasGlassInterfaceTintOverride?obj->glassInterfaceTint:obj->color));
    json_object_object_add(effective,"absorption_color",json_object_new_int(obj->hasGlassAbsorptionColorOverride?obj->glassAbsorptionColor:obj->color));
    json_object_object_add(o,"effective",effective);
    json_object_object_add(o,"transport_override",json_object_new_boolean(obj->hasGlassTransportOverride));
    json_object_object_add(o,"roughness_source",json_object_new_string(get(row,"surface_graph")?"graph output/regions when supplied; base fallback; spatial value evaluated at hit":"base and existing surface sampling/layers"));
    json_object_object_add(o,"scope",json_object_new_string("object transport; graph/source/mapping preserved"));return o;
}
bool SceneEditorOpticalResponseSet(int index,json_object *settings,unsigned long long revision,char *message,size_t size) {
    if(!valid_index(index) || revision!=SceneEditorDocumentRevision() || !SceneEditorDocumentObjectEditable(index,message,size))return false;
    if(!json_object_is_type(settings,json_type_object) || !json_object_object_length(settings))goto invalid;
    bool glass=false;json_object_object_foreach(settings,key,v) {
        if(!strcmp(key,"base_preset")){if(!json_object_is_type(v,json_type_string)||json_object_get_string_len(v)!=5||strcmp(json_object_get_string(v),"glass"))goto invalid;glass=true;}
        else if(!strcmp(key,"thin_walled")){if(!json_object_is_type(v,json_type_boolean))goto invalid;}
        else if(!strcmp(key,"interface_tint")||!strcmp(key,"absorption_color")){if(v && (!json_object_is_type(v,json_type_int)||json_object_get_int64(v)<0||json_object_get_int64(v)>0xFFFFFF))goto invalid;}
        else {
            if(!(json_object_is_type(v,json_type_int)||json_object_is_type(v,json_type_double)))goto invalid;
            double n=json_object_get_double(v);if(!isfinite(n))goto invalid;
            if(!strcmp(key,"transmission")||!strcmp(key,"base_roughness")){if(n<0||n>1)goto invalid;}
            else if(!strcmp(key,"ior")){if(n<1||n>2.5)goto invalid;}
            else if(!strcmp(key,"absorption_distance")){if(n<.25||n>8)goto invalid;}
            else goto invalid;
        }
    }
    if(!glass && sceneSettings.sceneObjects[index].material_id!=MATERIAL_PRESET_TRANSPARENT){snprintf(message,size,"Select base_preset glass before editing dielectric response.");return false;}
    const char *id=object_id(index);char stable_id[128];snprintf(stable_id,sizeof(stable_id),"%s",id);
    SceneObject before=sceneSettings.sceneObjects[index];
    if(glass && before.material_id!=MATERIAL_PRESET_TRANSPARENT) {before.material_id=MATERIAL_PRESET_TRANSPARENT;before.hasGlassTransportOverride=false;}
    double trans,ior,distance;bool thin;SceneObjectResolveGlassTransport(&before,&trans,&ior,&distance,&thin);
    if(!document_begin_command(message,size))return false;
    json_object *row=row_for(stable_id,true,index);if(!row){document_rollback_command();snprintf(message,size,"Retained material row unavailable.");return false;}
    if(glass)json_object_object_add(row,"material_id",json_object_new_int(MATERIAL_PRESET_TRANSPARENT));
    bool transport=get(settings,"transmission")||get(settings,"ior")||get(settings,"absorption_distance")||get(settings,"thin_walled");
    if(transport) {
        /* Seed omitted transport from resolved meaning, never from stale row aliases. */
        json_object_object_del(row,"glassTransportOverride");json_object_object_add(row,"glass_transport_override",json_object_new_boolean(true));
        const char *keys[]={"transmission","ior","absorption_distance","thin_walled"};
        const char *canonical[]={"glass_transmission","glass_ior","glass_absorption_distance","glass_thin_walled"};
        const char *aliases[]={"glassTransmission","glassIor","glassAbsorptionDistance","glassThinWalled"};
        json_object *fallback[]={json_object_new_double(trans),json_object_new_double(ior),json_object_new_double(distance),json_object_new_boolean(thin)};
        for(int i=0;i<4;i++){set_alias(row,canonical[i],aliases[i],get(settings,keys[i])?get(settings,keys[i]):fallback[i]);json_object_put(fallback[i]);}
    }
    json_object_object_foreach(settings,edit_key,value) {
        if(!strcmp(edit_key,"base_roughness"))json_object_object_add(row,"roughness",json_object_get(value));
        else if(!strcmp(edit_key,"interface_tint"))set_alias(row,"glass_interface_tint","glassInterfaceTint",value);
        else if(!strcmp(edit_key,"absorption_color"))set_alias(row,"glass_absorption_color","glassAbsorptionColor",value);
    }
    return document_finish_command(message,size);
invalid:
    snprintf(message,size,"Unsupported optical settings: transmission/roughness 0..1, IOR 1..2.5, absorption distance .25..8, thin_walled boolean, RGB24 tint/color or null.");return false;
}
