#include "app/scene_atmosphere_optics.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool fail(char* d,size_t n,const char* text) {
    if(d && n) snprintf(d,n,"%s",text);return false;
}
void SceneAtmosphereOpticsDefaults(SceneAtmosphereOptics* v) {
    *v=(SceneAtmosphereOptics){.cell_centered=true,.sky_illumination=true,
        .reference_concentration_kg_m3=.0002,.mass_extinction_m2_kg=1250,
        .scattering_albedo=.65,.anisotropy=.35};
}
bool SceneAtmosphereOpticsValidate(const SceneAtmosphereOptics* v,char* d,size_t n) {
    if(!v) return fail(d,n,"Missing atmosphere optics.");
    if(!v->present) return true;
    if(!isfinite(v->reference_concentration_kg_m3) || v->reference_concentration_kg_m3<=0 ||
       !isfinite(v->mass_extinction_m2_kg) || v->mass_extinction_m2_kg<0 ||
       !isfinite(v->reference_concentration_kg_m3*v->mass_extinction_m2_kg) ||
       !isfinite(v->scattering_albedo) || v->scattering_albedo<0 || v->scattering_albedo>1 ||
       !isfinite(v->anisotropy) || fabs(v->anisotropy)>.95 ||
       !isfinite(v->sky_error_budget) || v->sky_error_budget<0 || v->sky_error_budget>1e-4)
        return fail(d,n,"Physical optics require positive concentration, nonnegative extinction, albedo 0..1, anisotropy -0.95..0.95 and sky budget 0..0.0001.");
    return true;
}
bool SceneAtmosphereOpticsParse(json_object* o,SceneAtmosphereOptics* out,char* d,size_t n) {
    SceneAtmosphereOptics v;SceneAtmosphereOpticsDefaults(&v);
    if(!o) {*out=v;return true;}
    if(!json_object_is_type(o,json_type_object)) return fail(d,n,"Atmosphere optics must be an object.");
    const char* keys[]={"profile","sampling_layout","reference_concentration_kg_m3","mass_extinction_m2_kg",
        "scattering_albedo","anisotropy","sky_illumination","sky_error_budget"};
    json_object_object_foreach(o,key,value) {
        (void)value;bool known=false;
        for(size_t i=0;i<8;++i) if(!strcmp(key,keys[i])) known=true;
        if(!known) return fail(d,n,"Unknown atmosphere optical field.");
    }
    json_object *profile=NULL,*layout=NULL,*sky=NULL,*number=NULL;
    if(!json_object_object_get_ex(o,"profile",&profile) || !json_object_is_type(profile,json_type_string) ||
       (strcmp(json_object_get_string(profile),"physical_smoke_v1") && strcmp(json_object_get_string(profile),"legacy_artistic_v1")) ||
       !json_object_object_get_ex(o,"sampling_layout",&layout) || !json_object_is_type(layout,json_type_string) ||
       (strcmp(json_object_get_string(layout),"cell_centered") && strcmp(json_object_get_string(layout),"legacy_node")) ||
       !json_object_object_get_ex(o,"sky_illumination",&sky) || !json_object_is_type(sky,json_type_boolean))
        return fail(d,n,"Atmosphere optics need an explicit supported profile, sampling layout and sky boolean.");
    if(strlen(json_object_get_string(profile))!=(size_t)json_object_get_string_len(profile) ||
       strlen(json_object_get_string(layout))!=(size_t)json_object_get_string_len(layout))
        return fail(d,n,"Embedded null in atmosphere optical vocabulary.");
    v.present=true;v.physical=!strcmp(json_object_get_string(profile),"physical_smoke_v1");
    v.cell_centered=!strcmp(json_object_get_string(layout),"cell_centered");
    v.sky_illumination=json_object_get_boolean(sky);
    double* fields[]={&v.reference_concentration_kg_m3,&v.mass_extinction_m2_kg,&v.scattering_albedo,&v.anisotropy};
    for(size_t i=0;i<4;++i) {
        if(!json_object_object_get_ex(o,keys[i+2],&number) ||
           (!json_object_is_type(number,json_type_int) && !json_object_is_type(number,json_type_double)))
            return fail(d,n,"Atmosphere optical coefficients must be JSON numbers.");
        *fields[i]=json_object_get_double(number);
    }
    if(json_object_object_get_ex(o,"sky_error_budget",&number)) {
        if(!json_object_is_type(number,json_type_int) && !json_object_is_type(number,json_type_double))
            return fail(d,n,"Sky error budget must be a JSON number.");
        v.sky_error_budget=json_object_get_double(number);
    }
    if(!SceneAtmosphereOpticsValidate(&v,d,n)) return false;
    *out=v;return true;
}
json_object* SceneAtmosphereOpticsToJson(const SceneAtmosphereOptics* v) {
    if(!v || !v->present || !SceneAtmosphereOpticsValidate(v,NULL,0)) return NULL;
    json_object* o=json_object_new_object();
    json_object_object_add(o,"profile",json_object_new_string(v->physical?"physical_smoke_v1":"legacy_artistic_v1"));
    json_object_object_add(o,"sampling_layout",json_object_new_string(v->cell_centered?"cell_centered":"legacy_node"));
    json_object_object_add(o,"reference_concentration_kg_m3",json_object_new_double(v->reference_concentration_kg_m3));
    json_object_object_add(o,"mass_extinction_m2_kg",json_object_new_double(v->mass_extinction_m2_kg));
    json_object_object_add(o,"scattering_albedo",json_object_new_double(v->scattering_albedo));
    json_object_object_add(o,"anisotropy",json_object_new_double(v->anisotropy));
    json_object_object_add(o,"sky_illumination",json_object_new_boolean(v->sky_illumination));
    json_object_object_add(o,"sky_error_budget",json_object_new_double(v->sky_error_budget));return o;
}
