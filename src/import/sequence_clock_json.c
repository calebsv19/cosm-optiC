#include "import/sequence_clock_json.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static json_object *get(json_object *o,const char *key) {
    json_object *v=NULL;if(o)json_object_object_get_ex(o,key,&v);return v;
}
static bool object(json_object *o,int count) {
    return json_object_is_type(o,json_type_object) && json_object_object_length(o)==count;
}
static bool integer(json_object *o,int64_t *out) {
    if(!json_object_is_type(o,json_type_int))return false;
    const char *s=json_object_get_string(o);char *end=NULL;errno=0;
    long long v=strtoll(s,&end,10);
    if(errno || !end || *end)return false;
    *out=(int64_t)v;return true;
}
static bool natural(json_object *o,uint64_t *out) {
    int64_t v;if(!integer(o,&v)||v<0)return false;*out=(uint64_t)v;return true;
}
static bool string(json_object *o,char *out,size_t size) {
    if(!json_object_is_type(o,json_type_string))return false;
    const char *s=json_object_get_string(o);size_t n=(size_t)json_object_get_string_len(o);
    if(!n||n>=size||strlen(s)!=n)return false;
    memcpy(out,s,n+1);return true;
}
static bool ratio(json_object *o,SequenceTime *out) {
    return object(o,2)&&natural(get(o,"numerator"),&out->numerator)&&
        natural(get(o,"denominator"),&out->denominator);
}
static bool rate(json_object *o,TimelineRate *out) {
    SequenceTime v;
    if(!ratio(o,&v)||v.numerator>UINT32_MAX||v.denominator>UINT32_MAX)return false;
    *out=(TimelineRate){(uint32_t)v.numerator,(uint32_t)v.denominator};return true;
}
bool SequenceClockFromJson(json_object *root,SequenceClock *out) {
    SequenceClock c={0};char schema[32];
    if(!out||!object(root,6)||!string(get(root,"schema"),schema,sizeof(schema))||
        strcmp(schema,"ray_sequence_clock_v1")||!rate(get(root,"rate"),&c.rate))return false;
    c.version=SEQUENCE_CLOCK_VERSION;
    json_object *range=get(root,"range"),*source=get(root,"source"),*segments=get(root,"segments");
    int64_t version;
    if(!integer(get(root,"version"),&version)||version!=1||!object(range,2)||
       !integer(get(range,"start_frame"),&c.range.start_frame)||
       !natural(get(range,"frame_count"),&c.range.frame_count)||!object(source,5)||
       !string(get(source,"id"),c.source.id,sizeof(c.source.id))||
       !string(get(source,"revision"),c.source.revision,sizeof(c.source.revision))||
       !rate(get(source,"rate"),&c.source.rate)||
       !natural(get(source,"first_tick"),&c.source.first_tick)||
       !natural(get(source,"last_tick"),&c.source.last_tick)||
       !json_object_is_type(segments,json_type_array))return false;
    c.segment_count=json_object_array_length(segments);
    if(c.segment_count>SEQUENCE_SEGMENT_CAPACITY)return false;
    for(size_t i=0;i<c.segment_count;++i) {
        json_object *s=json_object_array_get_idx(segments,i);SequenceSegment *v=&c.segments[i];char kind[16];
        if(!object(s,6)||!string(get(s,"id"),v->id,sizeof(v->id))||
           !natural(get(s,"start_frame"),&v->start_frame)||
           !natural(get(s,"frame_count"),&v->frame_count)||
           !string(get(s,"kind"),kind,sizeof(kind))||
           !ratio(get(s,"source_start"),&v->source_start)||!ratio(get(s,"speed"),&v->speed))return false;
        if(!strcmp(kind,"play"))v->kind=SEQUENCE_PLAY;
        else if(!strcmp(kind,"hold"))v->kind=SEQUENCE_HOLD;
        else return false;
    }
    if(SequenceClockValidate(&c)!=SEQUENCE_OK)return false;
    *out=c;return true;
}
static void number(json_object *o,const char *key,uint64_t value) {
    json_object_object_add(o,key,json_object_new_int64((int64_t)value));
}
static void text(json_object *o,const char *key,const char *value) {
    json_object_object_add(o,key,json_object_new_string(value));
}
static json_object *rational(uint64_t n,uint64_t d) {
    json_object *o=json_object_new_object();number(o,"numerator",n);number(o,"denominator",d);return o;
}
json_object *SequenceClockToJson(const SequenceClock *c) {
    if(SequenceClockValidate(c)!=SEQUENCE_OK)return NULL;
    json_object *o=json_object_new_object(),*range=json_object_new_object(),
        *source=json_object_new_object(),*list=json_object_new_array();
    text(o,"schema","ray_sequence_clock_v1");number(o,"version",1);
    json_object_object_add(o,"rate",rational(c->rate.frames_per_second_numerator,c->rate.frames_per_second_denominator));
    json_object_object_add(range,"start_frame",json_object_new_int64(c->range.start_frame));
    number(range,"frame_count",c->range.frame_count);json_object_object_add(o,"range",range);
    text(source,"id",c->source.id);text(source,"revision",c->source.revision);
    json_object_object_add(source,"rate",rational(c->source.rate.frames_per_second_numerator,c->source.rate.frames_per_second_denominator));
    number(source,"first_tick",c->source.first_tick);number(source,"last_tick",c->source.last_tick);
    json_object_object_add(o,"source",source);
    for(size_t i=0;i<c->segment_count;++i) {
        const SequenceSegment *s=&c->segments[i];json_object *v=json_object_new_object();
        text(v,"id",s->id);number(v,"start_frame",s->start_frame);number(v,"frame_count",s->frame_count);
        text(v,"kind",s->kind==SEQUENCE_PLAY?"play":"hold");
        json_object_object_add(v,"source_start",rational(s->source_start.numerator,s->source_start.denominator));
        json_object_object_add(v,"speed",rational(s->speed.numerator,s->speed.denominator));
        json_object_array_add(list,v);
    }
    json_object_object_add(o,"segments",list);return o;
}
uint64_t SequenceClockFingerprint(const SequenceClock *c) {
    json_object *o=SequenceClockToJson(c);if(!o)return 0;
    const unsigned char *s=(const unsigned char *)json_object_to_json_string_ext(o,JSON_C_TO_STRING_PLAIN);
    uint64_t hash=UINT64_C(14695981039346656037);
    while(*s){hash^=*s++;hash*=UINT64_C(1099511628211);}
    json_object_put(o);return hash;
}
