#ifndef RUNTIME_SPECTRAL_JSON_H
#define RUNTIME_SPECTRAL_JSON_H
#include <json-c/json.h>
#include "math/vec3.h"
/* Required report fields: own the value on both success and failure. */
static inline bool spectral_json_add(json_object *o,const char *key,json_object *v) {
    if(!o || !v) {if(v)json_object_put(v);return false;}
    if(json_object_object_add(o,key,v)!=0) {json_object_put(v);return false;}
    return true;
}
static inline json_object *spectral_json_vec(Vec3 v) {
    json_object *a=json_object_new_array();if(!a)return NULL;
    double values[]={v.x,v.y,v.z};
    for(int i=0;i<3;++i) {json_object *n=json_object_new_double(values[i]);
        if(!n || json_object_array_add(a,n)!=0) {if(n)json_object_put(n);json_object_put(a);return NULL;}}
    return a;
}
#endif
