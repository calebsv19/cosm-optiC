#include "app/scene_atmosphere_attachment.h"
#include "animation/timeline_track.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool fail(char* d, size_t n, const char* why) {
    if (d && n) snprintf(d, n, "%s", why);
    return false;
}
static json_object* get(json_object* o, const char* key) {
    json_object* v = NULL;
    if (o) json_object_object_get_ex(o, key, &v);
    return v;
}
static bool relative_path(const char* p) {
    if (!p || !*p || p[0]=='/' || strchr(p,'\\') || strchr(p,':')) return false;
    for (const char* s=p; *s;) {
        const char* end=strchr(s,'/'); size_t n=end?(size_t)(end-s):strlen(s);
        if (!n || (n==1 && s[0]=='.') || (n==2 && s[0]=='.' && s[1]=='.')) return false;
        if (!end) break;
        s=end+1;
        if (!*s) return false;
    }
    return true;
}
void SceneAtmosphereAttachmentDefaults(SceneAtmosphereAttachment* v) {
    if (!v) return;
    memset(v,0,sizeof(*v));
    v->enabled=v->visible=v->affects_lighting=true;
    strcpy(v->id,"atmosphere/main"); strcpy(v->source_kind,"manifest");
    v->source_rate=(TimelineRate){24,1};
    v->density_scale=v->density_gamma=v->scatter_gain=v->step_scale=1;
    v->absorption_gain=0.1;
    v->opacity=1;
    for(int i=0;i<3;++i) v->tint[i]=v->scale[i]=1;
}
bool SceneAtmosphereAttachmentValidate(const SceneAtmosphereAttachment* v,char* d,size_t n) {
    if (!v) return fail(d,n,"attachment is missing");
    if (!v->present) return true;
    if (!memchr(v->id,0,sizeof(v->id)) || !v->id[0] || strlen(v->id)>=TIMELINE_ID_CAPACITY ||
        !memchr(v->source_path,0,sizeof(v->source_path)) || !relative_path(v->source_path) ||
        !memchr(v->source_kind,0,sizeof(v->source_kind))) return fail(d,n,"attachment needs an ID and project-relative source path");
    if (strcmp(v->source_kind,"manifest") && strcmp(v->source_kind,"vf3d") &&
        strcmp(v->source_kind,"pack") && strcmp(v->source_kind,"scene_bundle"))
        return fail(d,n,"unsupported atmosphere source kind");
    if (v->animated && (!strcmp(v->source_kind,"vf3d") || !strcmp(v->source_kind,"pack")))
        return fail(d,n,"single-file attachments are static; use a manifest for animation");
    if (!TimelineRateIsValid(v->source_rate) || v->source_rate.frames_per_second_numerator>1000000 ||
        v->source_rate.frames_per_second_denominator>1000000 ||
        v->timeline_start < -1000000000LL || v->timeline_start > 1000000000LL ||
        v->source_start<0 || v->source_start>1000000000LL) return fail(d,n,"invalid cache rate or frame offset");
    const double values[]={v->density_scale,v->density_gamma,v->absorption_gain,v->scatter_gain,v->step_scale};
    if(!isfinite(v->opacity) || v->opacity<0 || v->opacity>1)
        return fail(d,n,"atmosphere opacity must be between 0 and 1");
    const double low[]={0,0.01,0,0,0.01}, high[]={1000,100,1000,1000,100};
    for(size_t i=0;i<5;++i) if(!isfinite(values[i]) || values[i]<low[i] || values[i]>high[i])
        return fail(d,n,"atmosphere appearance value is outside its supported range");
    for(int i=0;i<3;++i) if(!isfinite(v->tint[i]) || v->tint[i]<0 || v->tint[i]>100 ||
        !isfinite(v->translation[i]) || fabs(v->translation[i])>1e9 ||
        !isfinite(v->rotation_degrees[i]) || fabs(v->rotation_degrees[i])>36000 ||
        !isfinite(v->scale[i]) || v->scale[i]<1e-6 || v->scale[i]>1e6)
        return fail(d,n,"invalid atmosphere tint or placement");
    return true;
}
static bool boolean(json_object* o,const char* key,bool* out) {
    json_object* v=get(o,key); if(!v) return true;
    if(!json_object_is_type(v,json_type_boolean)) return false;
    *out=json_object_get_boolean(v); return true;
}
static bool number(json_object* o,const char* key,double* out) {
    json_object* v=get(o,key); if(!v) return true;
    if(!json_object_is_type(v,json_type_double) && !json_object_is_type(v,json_type_int)) return false;
    *out=json_object_get_double(v); return isfinite(*out);
}
static bool integer(json_object* o,const char* key,int64_t* out) {
    json_object* v=get(o,key); if(!v) return true;
    if(!json_object_is_type(v,json_type_int)) return false;
    *out=json_object_get_int64(v); return true;
}
static bool string(json_object* o,const char* key,char* out,size_t n) {
    json_object* v=get(o,key); if(!v) return true;
    if(!json_object_is_type(v,json_type_string)) return false;
    const char* s=json_object_get_string(v); size_t len=(size_t)json_object_get_string_len(v);
    if(len>=n || strlen(s)!=len) return false;
    memcpy(out,s,len+1); return true;
}
static bool vector(json_object* o,const char* key,double out[3]) {
    json_object* a=get(o,key); if(!a) return true;
    if(!json_object_is_type(a,json_type_array) || json_object_array_length(a)!=3) return false;
    for(int i=0;i<3;++i) {
        json_object* v=json_object_array_get_idx(a,(size_t)i);
        if(!json_object_is_type(v,json_type_double) && !json_object_is_type(v,json_type_int)) return false;
        out[i]=json_object_get_double(v); if(!isfinite(out[i])) return false;
    }
    return true;
}
bool SceneAtmosphereAttachmentParse(json_object* author,SceneAtmosphereAttachment* out,char* d,size_t n) {
    SceneAtmosphereAttachment v; SceneAtmosphereAttachmentDefaults(&v);
    if(!out) return fail(d,n,"missing attachment output");
    json_object* o=get(author,"atmosphere_attachment");
    if(!o) {*out=v;return true;}
    json_object* schema=get(o,"schema_version");
    if(!json_object_is_type(o,json_type_object) || !schema ||
        !json_object_is_type(schema,json_type_string) ||
        strcmp(json_object_get_string(schema),"ray_atmosphere_attachment_v1"))
        return fail(d,n,"unsupported atmosphere attachment schema");
    v.present=true;
    json_object* playback=get(o,"playback"), *appearance=get(o,"appearance"), *placement=get(o,"placement");
    json_object* groups[]={playback,appearance,placement};
    for(size_t i=0;i<3;++i) if(groups[i] && !json_object_is_type(groups[i],json_type_object)) return fail(d,n,"invalid attachment group");
    int64_t rn=24,rd=1;
    char missing[16]="hold";
    if(!string(o,"id",v.id,sizeof(v.id)) || !string(o,"source_path",v.source_path,sizeof(v.source_path)) ||
        !string(o,"source_kind",v.source_kind,sizeof(v.source_kind)) ||
        !boolean(o,"enabled",&v.enabled) || !boolean(o,"visible",&v.visible) || !boolean(o,"affects_lighting",&v.affects_lighting) ||
        !boolean(playback,"animated",&v.animated) || !integer(playback,"timeline_start",&v.timeline_start) ||
        !integer(playback,"source_start",&v.source_start) || !integer(playback,"rate_numerator",&rn) || !integer(playback,"rate_denominator",&rd) ||
        !string(playback,"missing",missing,sizeof(missing)) || rn<1 || rd<1 || rn>1000000 || rd>1000000 ||
        (strcmp(missing,"hold") && strcmp(missing,"error")) ||
        !number(appearance,"density_scale",&v.density_scale) || !number(appearance,"density_gamma",&v.density_gamma) ||
        !number(appearance,"absorption_gain",&v.absorption_gain) || !number(appearance,"scatter_gain",&v.scatter_gain) ||
        !number(appearance,"step_scale",&v.step_scale) || !number(appearance,"opacity",&v.opacity) || !vector(appearance,"tint",v.tint) ||
        !vector(placement,"translation",v.translation) || !vector(placement,"rotation_degrees",v.rotation_degrees) || !vector(placement,"scale",v.scale))
        return fail(d,n,"invalid atmosphere attachment field type or value");
    v.source_rate=(TimelineRate){(uint32_t)rn,(uint32_t)rd};v.missing_is_error=!strcmp(missing,"error");
    if(!SceneAtmosphereAttachmentValidate(&v,d,n)) return false;
    *out=v;return true;
}
static json_object* array(const double values[3]) {
    json_object* a=json_object_new_array();
    for(int i=0;i<3;++i) json_object_array_add(a,json_object_new_double(values[i]));
    return a;
}
json_object* SceneAtmosphereAttachmentToJson(const SceneAtmosphereAttachment* v) {
    if(!v || !v->present || !SceneAtmosphereAttachmentValidate(v,NULL,0)) return NULL;
    json_object* o=json_object_new_object(),*p=json_object_new_object(),*a=json_object_new_object(),*t=json_object_new_object();
    json_object_object_add(o,"schema_version",json_object_new_string("ray_atmosphere_attachment_v1"));
    json_object_object_add(o,"id",json_object_new_string(v->id));
    json_object_object_add(o,"source_path",json_object_new_string(v->source_path));
    json_object_object_add(o,"source_kind",json_object_new_string(v->source_kind));
    json_object_object_add(o,"enabled",json_object_new_boolean(v->enabled));
    json_object_object_add(o,"visible",json_object_new_boolean(v->visible));
    json_object_object_add(o,"affects_lighting",json_object_new_boolean(v->affects_lighting));
    json_object_object_add(p,"animated",json_object_new_boolean(v->animated));
    json_object_object_add(p,"timeline_start",json_object_new_int64(v->timeline_start));
    json_object_object_add(p,"source_start",json_object_new_int64(v->source_start));
    json_object_object_add(p,"rate_numerator",json_object_new_int64(v->source_rate.frames_per_second_numerator));
    json_object_object_add(p,"rate_denominator",json_object_new_int64(v->source_rate.frames_per_second_denominator));
    json_object_object_add(p,"missing",json_object_new_string(v->missing_is_error?"error":"hold"));
    json_object_object_add(o,"playback",p);
    json_object_object_add(a,"density_scale",json_object_new_double(v->density_scale));
    json_object_object_add(a,"density_gamma",json_object_new_double(v->density_gamma));
    json_object_object_add(a,"absorption_gain",json_object_new_double(v->absorption_gain));
    json_object_object_add(a,"scatter_gain",json_object_new_double(v->scatter_gain));
    json_object_object_add(a,"step_scale",json_object_new_double(v->step_scale));
    json_object_object_add(a,"opacity",json_object_new_double(v->opacity));
    json_object_object_add(a,"tint",array(v->tint));json_object_object_add(o,"appearance",a);
    json_object_object_add(t,"translation",array(v->translation));
    json_object_object_add(t,"rotation_degrees",array(v->rotation_degrees));
    json_object_object_add(t,"scale",array(v->scale));json_object_object_add(o,"placement",t);
    return o;
}
bool SceneAtmosphereAttachmentRequestedFrame(const SceneAtmosphereAttachment* v,const TimelineEvaluationContext* f,int64_t* out,char* d,size_t n) {
    if(!out || !SceneAtmosphereAttachmentValidate(v,d,n) || !v->present || !f || !TimelineRateIsValid(f->rate) ||
        !isfinite(f->absolute_frame_position) || !f->sample.subframe_denominator ||
        f->sample.subframe_numerator>=f->sample.subframe_denominator)
        return fail(d,n,"invalid atmosphere frame context");
    long double mapped=(long double)v->source_start;
    if(v->animated) mapped+=((long double)f->sample.absolute_frame-v->timeline_start+
        (long double)f->sample.subframe_numerator/f->sample.subframe_denominator)*
        v->source_rate.frames_per_second_numerator*f->rate.frames_per_second_denominator/
        ((long double)v->source_rate.frames_per_second_denominator*f->rate.frames_per_second_numerator);
    if(!isfinite(mapped) || mapped < -1e12L || mapped > 1e12L) return fail(d,n,"cache frame mapping overflow");
    *out=(int64_t)floorl(mapped); return true;
}
bool SceneAtmosphereAttachmentSelectFrame(const SceneAtmosphereAttachment* v,int64_t requested,const int64_t* ids,size_t count,int64_t* out,char* d,size_t n) {
    if(!v || !ids || !count || !out || count>1000000) return fail(d,n,"cache has no valid frame IDs");
    for(size_t i=0;i<count;++i) if(ids[i]<0 || (i && ids[i]<=ids[i-1])) return fail(d,n,"cache frame IDs must be sorted and unique");
    size_t selected=0;
    for(size_t i=0;i<count && ids[i]<=requested;++i) selected=i;
    if(v->missing_is_error && ids[selected]!=requested) return fail(d,n,"requested cache frame is absent");
    *out=ids[selected];return true;
}
