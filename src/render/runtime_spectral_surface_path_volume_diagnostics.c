#include "runtime_spectral_surface_path_internal.h"
#include <json-c/json.h>

/* Fixed-size aggregate ledger. No random draws or changes to path arithmetic.
 * Contributions are scalar unit-source path values before spectral XYZ weights.
 * Source hits exclude delta chains and photon-owned paths. */
void spectral_volume_diagnostic(const RuntimeSpectralSurfacePathConfig *p,
    RuntimeSpectralSurfacePathReadout *s,Vec3 at,int order,int proposal,int outcome,double value) {
    if(p->sky || p->native_sources || order<1)return;
    double scale=.5*hypot(p->width,p->height);
    if(!(scale>0))return;
    double distance=fabs(vec3_dot(vec3_sub(at,p->origin),p->normal))/scale;
    int d=distance<.01?0:distance<.1?1:distance<1?2:3;
    int o=order==1?0:order==2?1:order<=4?2:3;
    RuntimeSpectralVolumeLightBucket *b=&s->volume_light[4*o+d];
    if(outcome==3) {
        ++b->source_hits;b->hit_sum+=value;b->hit_sum2+=value*value;b->hit_max=fmax(b->hit_max,value);
    } else {
        ++b->attempts;b->angular+=proposal>0;b->angular_missed+=proposal>0 && outcome==0;b->rectangle+=proposal==2;
        b->missed+=outcome==0;b->occluded+=outcome==1;b->visible+=outcome==2;
        b->nee_sum+=value;b->nee_sum2+=value*value;b->nee_max=fmax(b->nee_max,value);
    }
}
struct json_object *spectral_volume_diagnostics_json(const RuntimeSpectralSurfacePathReadout *s) {
    json_object *o=json_object_new_object(),*a=json_object_new_array();
    if(!o || !a){if(o)json_object_put(o);if(a)json_object_put(a);return NULL;}
    json_object_object_add(o,"contribution_measure",json_object_new_string("unit-source scalar before spectral XYZ weights; not film variance"));
    json_object_object_add(o,"distance_measure",json_object_new_string("absolute emitter-plane distance / rectangle half diagonal"));
    json_object_object_add(o,"source_hit_scope",json_object_new_string("non-delta volume emitter hits only; excludes map-owned paths"));
    json_object_object_add(o,"buckets",a);
    const char *orders[]={"1","2","3-4","5+"},*distances[]={"[0,.01)","[.01,.1)","[.1,1)","[1,infinity)"};
    for(int i=0;i<16;++i) {
        const RuntimeSpectralVolumeLightBucket *b=&s->volume_light[i];json_object *v=json_object_new_object();
        if(!v){json_object_put(o);return NULL;}
        json_object_array_add(a,v);
        json_object_object_add(v,"order",json_object_new_string(orders[i/4]));
        json_object_object_add(v,"distance",json_object_new_string(distances[i%4]));
#define COUNT(k) json_object_object_add(v,#k,json_object_new_int64(b->k))
#define VALUE(k) json_object_object_add(v,#k,json_object_new_double(b->k))
        COUNT(attempts);COUNT(angular);COUNT(angular_missed);COUNT(rectangle);COUNT(missed);COUNT(occluded);COUNT(visible);COUNT(source_hits);
        VALUE(nee_sum);VALUE(nee_sum2);VALUE(nee_max);VALUE(hit_sum);VALUE(hit_sum2);VALUE(hit_max);
#undef COUNT
#undef VALUE
    }
    return o;
}
