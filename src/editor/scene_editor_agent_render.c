#include "editor/scene_editor_agent_render.h"
#include "editor/scene_editor_render_control.h"
#include "editor/scene_editor_document.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
static json_object* get(json_object* p,const char* key) {json_object* v=NULL;if(json_object_is_type(p,json_type_object)) json_object_object_get_ex(p,key,&v);return v;}
static bool integer(json_object* p,const char* key,int* out) {json_object* v=get(p,key);if(!json_object_is_type(v,json_type_int) || json_object_get_int64(v)<0 || json_object_get_int64(v)>INT_MAX) return false;*out=json_object_get_int(v);return true;}
bool SceneEditorAgentRenderReadOnly(const char* name) {return !strcmp(name,"render.read") || !strcmp(name,"render.status") || !strcmp(name,"render.results");}
bool SceneEditorAgentRenderDispatch(const char* name,json_object* p,json_object* response,char* message,size_t size) {
    if(!json_object_is_type(p,json_type_object)) goto invalid;
    if(!strcmp(name,"render.read")) {if(json_object_object_length(p)) goto invalid;json_object_object_add(response,"render",SceneEditorRenderRead());return true;}
    if(!strcmp(name,"render.status") || !strcmp(name,"render.results")) {
        if(json_object_object_length(p)!=1 || !json_object_is_type(get(p,"job_id"),json_type_string)) goto invalid;
        json_object* r=SceneEditorRenderJobRead(json_object_get_string(get(p,"job_id")),!strcmp(name,"render.results"),message,size);if(!r) return false;json_object_object_add(response,"render",r);return true;
    }
    if(!strcmp(name,"render.cancel")) {
        if(json_object_object_length(p)!=2 || !json_object_is_type(get(p,"job_id"),json_type_string)) goto invalid;
        return SceneEditorRenderCancel(json_object_get_string(get(p,"job_id")),message,size);
    }
    if(!strcmp(name,"render.start") || !strcmp(name,"render.start_seeded")) {
        SceneEditorRenderOptions o={0};bool seeded=!strcmp(name,"render.start_seeded");json_object* mode=get(p,"integrator"),*id=get(p,"request_id");
        json_object *deposition=get(p,"spectral_deposition");
        if(deposition) {
            if(!json_object_is_type(deposition,json_type_string) || !json_object_is_type(mode,json_type_string) ||
               strcmp(json_object_get_string(mode),"disney_v3_spectral")) goto invalid;
            const char *value=json_object_get_string(deposition);
            if(!strcmp(value,"surface")) o.deposition=EDITOR_SPECTRAL_SURFACE;
            else if(!strcmp(value,"surface_volume")) o.deposition=EDITOR_SPECTRAL_SURFACE_VOLUME;
            else if(strcmp(value,"direct")) goto invalid;
        }
        json_object *photons=get(p,"photons_per_source_override"),*exposure=get(p,"exposure_stops");
        if(photons && (!o.deposition || !integer(p,"photons_per_source_override",&o.spectral_photons))) goto invalid;
        if(exposure) {
            if(!o.deposition || !json_object_is_type(exposure,json_type_int) ||
               json_object_get_int64(exposure)<-1 || json_object_get_int64(exposure)>2) goto invalid;
            o.spectral_exposure_stops=json_object_get_int(exposure);
        }
        if(json_object_object_length(p)!=(seeded?8:7)+(deposition!=NULL)+(exposure!=NULL)+(photons!=NULL) || !json_object_is_type(mode,json_type_string) || !json_object_is_type(id,json_type_string) || !integer(p,"width",&o.width) || !integer(p,"height",&o.height) || !integer(p,"temporal_frames",&o.temporal_frames) || !integer(p,"frame",&o.frame)) goto invalid;
        if(seeded) {
            json_object *sequence=get(p,"sample_sequence");
            if(!json_object_is_type(sequence,json_type_int)||json_object_get_int64(sequence)<1||json_object_get_int64(sequence)>UINT32_MAX)goto invalid;
            o.sample_sequence=(uint32_t)json_object_get_int64(sequence);
        }
        const char* label=json_object_get_string(mode);o.integrator=!strcmp(label,"direct_light")?0:!strcmp(label,"disney_v2")?5:!strcmp(label,"disney_v3_spectral")?6:-1;
        bool ok=SceneEditorRenderStart(json_object_get_string(id),&o,SceneEditorDocumentRevision(),message,size);
        json_object_object_add(response,"render",SceneEditorRenderRead());return ok;
    }
invalid:snprintf(message,size,"Invalid or unsupported render command arguments.");return false;
}
