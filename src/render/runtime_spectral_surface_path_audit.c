/* Bounded sample attribution. Observes transport without consuming RNG draws. */
#include "runtime_spectral_surface_path_internal.h"
#include "runtime_spectral_json.h"
#include <json-c/json.h>

void spectral_path_audit_sample(RuntimeSpectralSurfacePathReadout *out,int limit,int sample,
    double nm,double weight,double px,double py,Vec3 color,const PathSignal *s,
    unsigned long long hits,unsigned long long volumes) {
    double total=((s->direct+s->indirect)+s->volume)+(s->caustic+s->caustic_bounce);
    RuntimeSpectralSampleAudit a={.sample=sample,.wavelength_nm=nm,.wavelength_weight=weight,
        .pixel_x=px,.pixel_y=py,.xyz=vec3_scale(color,total),.source_hits=hits,.volume_events=volumes};
    double values[11]={total,s->direct,s->indirect,s->volume,s->caustic,s->caustic_bounce,s->volume_caustic,
        s->volume_order[0],s->volume_order[1],s->volume_order[2],s->volume_order[3]};
    for(int i=0;i<11;++i)a.component_y[i]=values[i]*color.y;
    out->sample_audit_sum=vec3_add(out->sample_audit_sum,a.xyz);++out->sample_audit_observed;
    if(limit<1 || limit>SPECTRAL_SAMPLE_AUDIT_LIMIT)return;
    int at=0;while(at<out->sample_audit_count && out->sample_audit[at].xyz.y>=a.xyz.y)++at;
    if(at>=limit)return;
    int count=out->sample_audit_count<limit?out->sample_audit_count+1:limit;
    for(int i=count-1;i>at;--i)out->sample_audit[i]=out->sample_audit[i-1];
    out->sample_audit[at]=a;out->sample_audit_count=count;
}
json_object *spectral_path_audit_json(const RuntimeSpectralSurfacePathConfig *p,const RuntimeSpectralSurfacePathReadout *s) {
    json_object *o=json_object_new_object(),*rows=NULL;if(!o)return NULL;
#define A(k,v) do {if(!spectral_json_add(o,k,v))goto fail;}while(0)
    A("enabled",json_object_new_boolean(p->sample_audit_enabled));
    A("x",json_object_new_int(p->sample_audit_x));A("y",json_object_new_int(p->sample_audit_y));
    A("observed_samples",json_object_new_int(s->sample_audit_observed));
    A("retained_limit",json_object_new_int(p->sample_audit_limit));
    A("measure",json_object_new_string("unexposed_XYZ_per_sample_before_dividing_by_samples"));
    A("ranking",json_object_new_string("descending_total_Y; ties retain earlier sample"));
    A("component_order",json_object_new_string("total,direct,surface_indirect,volume,caustic,caustic_bounce,volume_caustic,scatter_1,scatter_2,scatter_3_4,scatter_5_plus"));
    A("mean_xyz",spectral_json_vec(s->sample_audit_observed?vec3_scale(s->sample_audit_sum,1./s->sample_audit_observed):vec3(0,0,0)));
    rows=json_object_new_array();if(!rows)goto fail;
    for(int i=0;i<s->sample_audit_count;++i) {
        const RuntimeSpectralSampleAudit *a=&s->sample_audit[i];json_object *row=json_object_new_object(),*ys=json_object_new_array();
        if(!row || !ys) {if(row)json_object_put(row);if(ys)json_object_put(ys);goto fail;}
        bool ok=spectral_json_add(row,"sample",json_object_new_int(a->sample)) &&
            spectral_json_add(row,"vacuum_nm",json_object_new_double(a->wavelength_nm)) &&
            spectral_json_add(row,"wavelength_weight",json_object_new_double(a->wavelength_weight)) &&
            spectral_json_add(row,"pixel_x",json_object_new_double(a->pixel_x)) && spectral_json_add(row,"pixel_y",json_object_new_double(a->pixel_y)) &&
            spectral_json_add(row,"xyz",spectral_json_vec(a->xyz)) &&
            spectral_json_add(row,"source_hits",json_object_new_int64(a->source_hits)) && spectral_json_add(row,"volume_events",json_object_new_int64(a->volume_events));
        for(int k=0;ok && k<11;++k) {json_object *n=json_object_new_double(a->component_y[k]);if(!n || json_object_array_add(ys,n)!=0) {if(n)json_object_put(n);ok=false;}}
        if(!ok) {json_object_put(row);json_object_put(ys);goto fail;}
        if(!spectral_json_add(row,"component_Y",ys)) {json_object_put(row);goto fail;}
        if(json_object_array_add(rows,row)!=0) {json_object_put(row);goto fail;}
    }
    if(!spectral_json_add(o,"top_samples",rows)) {rows=NULL;goto fail;}rows=NULL;
#undef A
    return o;
fail:if(rows)json_object_put(rows);json_object_put(o);return NULL;
}
