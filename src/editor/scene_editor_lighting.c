/* Retained lighting policy shared by native inspector/menu and MCP adapters. */
#include "editor/scene_editor_lighting.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_document_timeline.h"
#include "scene_editor_document_transaction.h"
#include "config/config_manager.h"
#include "config/config_scene_path_io.h"
#include "motion/scene_motion_paths.h"
#include "import/runtime_scene_bridge.h"
#include "import/runtime_scene_light_timeline_io.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static json_object* get(json_object* p,const char* key) {
    json_object* v=NULL;if(json_object_is_type(p,json_type_object)) json_object_object_get_ex(p,key,&v);return v;
}
static json_object* object(json_object* p,const char* key) {
    json_object* v=get(p,key);if(v) return json_object_is_type(v,json_type_object)?v:NULL;
    if(!p) return NULL;v=json_object_new_object();json_object_object_add(p,key,v);return v;
}
static json_object* author(bool create) {
    json_object* p=document_authoring_root();const char* keys[]={"extensions","ray_tracing","authoring"};
    for(int i=0;i<3;++i) p=create?object(p,keys[i]):get(p,keys[i]);return p;
}
static bool refuse(char* message,size_t size,const char* text) {if(message && size) snprintf(message,size,"%s",text);return false;}
static json_object* primary(void) {
    json_object* lights=get(document_authoring_root(),"lights");
    return json_object_is_type(lights,json_type_array)?json_object_array_get_idx(lights,0):NULL;
}
static const char* primary_id(void) {
    json_object* p=primary();const char* keys[]={"id","light_id","object_id"};
    for(int i=0;i<3;++i) {json_object* v=get(p,keys[i]);if(json_object_is_type(v,json_type_string) && json_object_get_string(v)[0]) return json_object_get_string(v);}
    return "authored_light_0";
}
bool SceneEditorLightingPrimaryEditable(char* message,size_t size) {
    if(!SceneEditorDocumentIsOpen() || !json_object_is_type(primary(),json_type_object)) return refuse(message,size,"Static primary editing requires an existing authored light.");
    json_object* enabled=get(primary(),"enabled"),*moving=get(primary(),"moving");
    if((enabled && !json_object_get_boolean(enabled)) || (moving && json_object_get_boolean(moving))) return refuse(message,size,"Disabled or moving primary light: static editing is refused.");
    const char* id=primary_id();RuntimeSceneBridge3DLightSeedState seeds={0};
    runtime_scene_bridge_get_last_3d_light_seed_state(&seeds);
    if(!seeds.valid || seeds.light_count<1 || strcmp(seeds.lights[0].id,id)) return refuse(message,size,"Primary light runtime identity is ambiguous.");
    if((seeds.lights[0].kind!=RUNTIME_LIGHT_SOURCE_3D_KIND_POINT && seeds.lights[0].kind!=RUNTIME_LIGHT_SOURCE_3D_KIND_SPHERE) || seeds.lights[0].radiometryMode!=RUNTIME_LIGHT_RADIOMETRY_LEGACY_INTENSITY)
        return refuse(message,size,"Primary light shape/radiometry requires a dedicated editor command.");
    for(int i=1;i<seeds.light_count;++i) if(!strcmp(seeds.lights[i].id,id)) return refuse(message,size,"Duplicate primary light identity.");
    json_object* a=author(false);
    if(sceneSettings.bezierPath.numPoints>1 || get(a,"light_timeline")) return refuse(message,size,"Animated light path/timeline: static editing is refused.");
    char target[128];snprintf(target,sizeof(target),"light/%s",id);MotionPathBinding binding;
    if(MotionPathsRuntimeBinding(target,&binding) || MotionPathsRuntimeBinding("light/main",&binding)) return refuse(message,size,"Light motion binding: static editing is refused.");
    TimelineDocument* doc=calloc(1,sizeof(*doc));if(!doc) return refuse(message,size,"Light timeline validation allocation failed.");
    TimelineStatus status=SceneEditorDocumentGetTimeline(doc);bool animated=false;
    if(status==TIMELINE_STATUS_OK) for(size_t i=0;i<doc->track_count;++i)
        if(doc->tracks[i].key_count && !strncmp(doc->tracks[i].target_id,"light/",6) &&
           (!strcmp(doc->tracks[i].target_id,target) || !strcmp(doc->tracks[i].target_id,"light/main"))) animated=true;
    free(doc);
    if(status!=TIMELINE_STATUS_OK && status!=TIMELINE_STATUS_TARGET_NOT_FOUND) return refuse(message,size,"Light timeline could not be validated.");
    if(animated) return refuse(message,size,"Primary light timeline channels: static editing is refused.");
    if(message && size) message[0]=0;return true;
}
bool SceneEditorLightingGetPrimary(double xyz[3],double* intensity,double* radius) {
    RuntimeSceneBridge3DLightSeedState seeds={0};runtime_scene_bridge_get_last_3d_light_seed_state(&seeds);
    if(!seeds.valid || seeds.light_count<1) return false;
    xyz[0]=seeds.lights[0].position.x;xyz[1]=seeds.lights[0].position.y;xyz[2]=seeds.lights[0].position.z;
    *intensity=seeds.lights[0].intensity;*radius=seeds.lights[0].radius;
    if(get(author(false),"light_path") && sceneSettings.bezierPath.numPoints>0) {
        xyz[0]=sceneSettings.bezierPath.points[0].x;xyz[1]=sceneSettings.bezierPath.points[0].y;xyz[2]=sceneSettings.bezierPath3D.point_z[0];*intensity=animSettings.lightIntensity;*radius=animSettings.lightRadius;
    }
    return true;
}
static json_object* vector(const double v[3]) {json_object* a=json_object_new_array();for(int i=0;i<3;++i) json_object_array_add(a,json_object_new_double(v[i]));return a;}
json_object* SceneEditorLightingRead(void) {
    json_object* result=json_object_new_object(),*light=json_object_new_object(),*env=json_object_new_object();char reason[256];double xyz[3],intensity,radius;
    bool available=SceneEditorLightingGetPrimary(xyz,&intensity,&radius),editable=SceneEditorLightingPrimaryEditable(reason,sizeof(reason));
    json_object_object_add(light,"available",json_object_new_boolean(available));json_object_object_add(light,"static_editable",json_object_new_boolean(editable));json_object_object_add(light,"refusal_reason",json_object_new_string(reason));
    json_object_object_add(light,"id",json_object_new_string(primary_id()));
    if(available) {json_object_object_add(light,"position_m",vector(xyz));json_object_object_add(light,"intensity",json_object_new_double(intensity));json_object_object_add(light,"radius_m",json_object_new_double(radius));}
    json_object_object_add(light,"retained",json_object_get(primary()));
    json_object_object_add(result,"primary",light);json_object_object_add(result,"world_scale",json_object_new_double(SceneEditorDocumentWorldScale()));
    json_object_object_add(env,"light_mode",json_object_new_int(animSettings.environmentLightMode));
    json_object_object_add(env,"environment_preset",json_object_new_int(animSettings.environmentPreset));
    json_object_object_add(env,"ambient_strength",json_object_new_double(animSettings.environmentBrightness/255.));
    json_object_object_add(env,"top_fill_strength",json_object_new_double(animSettings.topFillStrength));
    json_object_object_add(env,"background_brightness_auto",json_object_new_boolean(animSettings.environmentBackgroundBrightnessAuto));
    json_object_object_add(env,"background_brightness",json_object_new_double(animSettings.environmentBackgroundBrightnessAuto?animSettings.environmentBrightness/255.:animSettings.environmentBackgroundBrightness));
    double rgb[3]={animSettings.environmentBackgroundColorR,animSettings.environmentBackgroundColorG,animSettings.environmentBackgroundColorB};json_object_object_add(env,"background_color",vector(rgb));
    json_object_object_add(result,"environment",env);json_object_object_add(result,"retained_environment",json_object_get(get(author(false),"environment")));return result;
}
bool SceneEditorLightingSetPrimary(const double xyz[3],double intensity,double radius,unsigned long long revision,char* message,size_t size) {
    double scale=SceneEditorDocumentWorldScale();
    if(!xyz || !isfinite(xyz[0]) || !isfinite(xyz[1]) || !isfinite(xyz[2]) || !isfinite(intensity) || intensity<0 || intensity>20 || !isfinite(radius) || radius<0 || radius>25 || !isfinite(scale) || scale<=0 || revision!=SceneEditorDocumentRevision()) return refuse(message,size,"Invalid primary light values or stale scene revision.");
    for(int i=0;i<3;++i) if(!isfinite(xyz[i]/scale)) return refuse(message,size,"Primary light position overflows authored units.");
    if(!isfinite(radius/scale)) return refuse(message,size,"Primary light radius overflows authored units.");
    if(!SceneEditorLightingPrimaryEditable(message,size) || !document_begin_command(message,size)) return false;
    json_object* p=primary(),*pos=get(p,"position");
    if(!pos) pos=object(object(p,"transform"),"position");
    if(!json_object_is_type(pos,json_type_object)) {document_rollback_command();return refuse(message,size,"Unsupported primary light position carrier.");}
    const char* names[]={"x","y","z"};for(int i=0;i<3;++i) json_object_object_add(pos,names[i],json_object_new_double(xyz[i]/scale));
    json_object_object_add(p,"intensity",json_object_new_double(intensity));json_object_object_add(p,"radius",json_object_new_double(radius/scale));
    json_object* a=author(true),*settings=object(a,"light_settings");if(!settings) {document_rollback_command();return false;}
    json_object_object_add(settings,"intensity",json_object_new_double(intensity));/* Legacy light_settings.radius is stored in runtime world units; the root
     * light radius is authored-space and is scaled by the light-seed loader. */
    json_object_object_add(settings,"radius",json_object_new_double(radius));
    /* The existing UI path is the render-position carrier; keep it static. */
    Path path=sceneSettings.bezierPath;CameraPath3D depth=sceneSettings.bezierPath3D;
    if(!path.numPoints) {memset(&path,0,sizeof(path));memset(&depth,0,sizeof(depth));path.mode=BEZIER_CUBIC;}
    path.numPoints=1;path.points[0].x=xyz[0]/scale;path.points[0].y=xyz[1]/scale;depth.point_z[0]=xyz[2]/scale;
    json_object_object_add(a,"light_path",config_scene_path_to_json_object(&path));json_object_object_add(a,"light_path_depth",CameraPath3D_ToJsonObject(&depth,&path));
    return document_finish_command(message,size);
}
bool SceneEditorLightingSetPrimaryValue(const char* property,double value) {
    double xyz[3],intensity,radius;char message[256];if(!SceneEditorLightingGetPrimary(xyz,&intensity,&radius)) return false;
    if(!strcmp(property,"intensity")) intensity=value;else if(!strcmp(property,"radius_m")) radius=value;else return false;
    return SceneEditorLightingSetPrimary(xyz,intensity,radius,SceneEditorDocumentRevision(),message,sizeof(message));
}
static bool number(json_object* v,double low,double high) {
    return (json_object_is_type(v,json_type_int) || json_object_is_type(v,json_type_double)) && isfinite(json_object_get_double(v)) && json_object_get_double(v)>=low && json_object_get_double(v)<=high;
}
bool SceneEditorLightingSetEnvironment(json_object* patch,unsigned long long revision,char* message,size_t size) {
    if(!SceneEditorDocumentIsOpen() || revision!=SceneEditorDocumentRevision() || !json_object_is_type(patch,json_type_object) || !json_object_object_length(patch)) return refuse(message,size,"Invalid environment patch or stale scene revision.");
    json_object_object_foreach(patch,key,v) {
        bool ok=false;
        if(!strcmp(key,"light_mode") || !strcmp(key,"environment_preset")) ok=json_object_is_type(v,json_type_int) && number(v,0,2);
        else if(!strcmp(key,"background_brightness_auto")) ok=json_object_is_type(v,json_type_boolean);
        else if(!strcmp(key,"ambient_strength")) ok=number(v,0,1);
        else if(!strcmp(key,"top_fill_strength")) ok=number(v,0,20);
        else if(!strcmp(key,"background_brightness")) ok=number(v,0,4);
        else if(!strcmp(key,"background_color")) {ok=json_object_is_type(v,json_type_array) && json_object_array_length(v)==3;for(int i=0;ok && i<3;++i) ok=number(json_object_array_get_idx(v,i),0,1);}
        if(!ok) return refuse(message,size,"Unsupported environment property or invalid typed value.");
    }
    if(!document_begin_command(message,size)) return false;
    json_object* env=object(author(true),"environment");if(!env) {document_rollback_command();return false;}
    /* Adding an environment object must retain every currently effective value:
     * its loader supplies defaults for missing background/preset fields. */
    const char* defaults[]={"light_mode","environment_preset","ambient_strength","top_fill_strength","background_brightness_auto","background_brightness","background_color"};
    json_object* snapshot=SceneEditorLightingRead(),*effective=get(snapshot,"environment");
    for(int i=0;i<7;++i) if(!get(env,defaults[i])) json_object_object_add(env,defaults[i],json_object_get(get(effective,defaults[i])));
    json_object_put(snapshot);
    json_object_object_foreach(patch,key2,v2) {
        if(!strcmp(key2,"background_color")) {
            json_object* rgb=object(env,"background_color");
            if(!rgb) {rgb=json_object_new_object();json_object_object_add(env,"background_color",rgb);}
            const char* names[]={"r","g","b"};for(int i=0;i<3;++i) json_object_object_add(rgb,names[i],json_object_get(json_object_array_get_idx(v2,i)));
        } else json_object_object_add(env,key2,json_object_get(v2));
    }
    json_object* ambient=get(env,"ambient_strength");json_object_object_add(env,"ambient_brightness",json_object_new_double(255.*json_object_get_double(ambient)));
    return document_finish_command(message,size);
}
bool SceneEditorLightingEnvironmentValue(const char* property,double value) {
    json_object* patch=json_object_new_object();json_object* v=(!strcmp(property,"light_mode") || !strcmp(property,"environment_preset"))?json_object_new_int((int)value):!strcmp(property,"background_brightness_auto")?json_object_new_boolean(value!=0):json_object_new_double(value);
    json_object_object_add(patch,property,v);if(!strcmp(property,"background_brightness")) json_object_object_add(patch,"background_brightness_auto",json_object_new_boolean(false));
    char message[256];bool ok=SceneEditorLightingSetEnvironment(patch,SceneEditorDocumentRevision(),message,sizeof(message));json_object_put(patch);return ok;
}
bool SceneEditorLightingEnvironmentColor(double r,double g,double b) {
    double rgb[3]={r,g,b};json_object* patch=json_object_new_object();json_object_object_add(patch,"background_color",vector(rgb));char message[256];bool ok=SceneEditorLightingSetEnvironment(patch,SceneEditorDocumentRevision(),message,sizeof(message));json_object_put(patch);return ok;
}
