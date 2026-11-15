/* Path attribution only: no random draws or modifications to transport sums. */
#include "runtime_spectral_surface_path_internal.h"
#include "runtime_spectral_json.h"
#include <json-c/json.h>
void spectral_nonscattered_add(PathSignal *s,double value,int depth,int volume,bool opaque,int reflections,int transmissions) {
    if(volume || depth==0)return;
    int k=reflections+transmissions==0?0:opaque?4:transmissions==0?1:reflections==0?2:3;
    s->nonscattered_indirect[k]+=value;
}
void spectral_surface_pdf_audit(const RuntimeSpectralSurfacePath *c,RuntimeSpectralSurfacePathReadout *s,
    Vec3 at,const SourceSample *light,int order) {
    if(order<2 || c->profile->surface_path.native_sources || light->delta)return;
    ++s->surface_nee_after_multiple_scattering;
    double expected=spectral_path_source_pdf(c,RuntimeRay3D_Make(at,light->direction),light->distance);
    double error=fabs(expected-light->pdf)/fmax(1e-300,fmax(expected,light->pdf));
    s->surface_nee_pdf_relative_error_max=fmax(s->surface_nee_pdf_relative_error_max,error);
    s->surface_nee_pdf_mismatches+=error>1e-10;
}
static json_object *classes(const Vec3 *v,Vec3 target) {
    const char *names[]={"opaque_no_glass","glass_reflection_only","glass_transmission_only","glass_reflection_and_transmission","opaque_and_glass"};
    json_object *o=json_object_new_object();if(!o)return NULL;Vec3 sum=vec3(0,0,0);
    for(int k=0;k<5;++k) {if(!spectral_json_add(o,names[k],spectral_json_vec(v[k])))goto fail;sum=vec3_add(sum,v[k]);}
    if(!spectral_json_add(o,"recombination_residual_xyz",spectral_json_vec(vec3_sub(target,sum))))goto fail;
    return o;
fail:json_object_put(o);return NULL;
}
json_object *spectral_nonscattered_json(const RuntimeSpectralSurfacePathConfig *p,const RuntimeSpectralSurfacePathReadout *s) {
    json_object *o=json_object_new_object(),*a=NULL;if(!o)return NULL;
#define ADD(k,v) do {if(!spectral_json_add(o,k,v))goto fail;}while(0)
    ADD("measure",json_object_new_string("unexposed linear XYZ; disjoint classes recombine to surface_indirect; fog has zero scattering events but can attenuate"));
    ADD("classification",json_object_new_string("opaque means any non-glass material interaction; reflection_and_transmission includes any order, not only internal reflection"));
    ADD("film_xyz",classes(s->film_nonscattered_indirect,s->film_indirect));
    ADD("surface_nee_after_multiple_scattering",json_object_new_int64(s->surface_nee_after_multiple_scattering));
    ADD("surface_nee_pdf_mismatches",json_object_new_int64(s->surface_nee_pdf_mismatches));
    ADD("surface_nee_pdf_relative_error_max",json_object_new_double(s->surface_nee_pdf_relative_error_max));
    a=json_object_new_array();if(!a)goto fail;ADD("probes",a);
    for(int i=0;i<p->probe_count;++i) {
        json_object *row=classes(s->probes[i].nonscattered_indirect,s->probes[i].indirect);if(!row)goto fail;
        json_object_object_add(row,"x",json_object_new_int(p->probes[i].x));json_object_object_add(row,"y",json_object_new_int(p->probes[i].y));
        if(json_object_array_add(a,row)!=0){json_object_put(row);goto fail;}
    }
#undef ADD
    return o;
fail:json_object_put(o);return NULL;
}
