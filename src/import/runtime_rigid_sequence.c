#ifdef RAY_RIGID_RESULT_ENABLED
#include "runtime_rigid_result_internal.h"
#include "import/sequence_clock_json.h"
#include "import/scene_timeline_document_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static json_object *get(json_object *o,const char *k) {json_object *v=NULL;if(o)json_object_object_get_ex(o,k,&v);return v;}
static bool fail(char *d,size_t n,const char *s){if(d&&n)snprintf(d,n,"%s",s);return false;}
bool RuntimeRigidSequencePrepare(json_object *scene,RuntimeRigidResult *r,char *d,size_t n) {
    json_object *a=get(get(get(scene,"extensions"),"ray_tracing"),"authoring"),*s=NULL;
    if(!a||!json_object_object_get_ex(a,"sequence",&s))return true;
    if(!SequenceClockFromJson(s,&r->sequence))return fail(d,n,"invalid sequence clock contract");
    const SequenceClock *c=&r->sequence;
    json_object *id=get(scene,"scene_id");
    if(!json_object_is_type(id,json_type_string)||strcmp(c->source.id,json_object_get_string(id))||
       strcmp(c->source.revision,r->revision)||c->source.last_tick>r->result.endpoint_tick||
       (uint64_t)c->source.rate.frames_per_second_numerator!=
       (uint64_t)c->source.rate.frames_per_second_denominator*240u)
        return fail(d,n,"sequence source identity/rate/envelope does not match authenticated rigid result");
    json_object *timeline=get(a,"scene_timeline");
    if(timeline) {
        TimelineDocument *t=malloc(sizeof(*t));
        bool ok=t&&SceneTimelineDocumentFromJson(timeline,t)==TIMELINE_STATUS_OK&&
          t->range.start_frame==c->range.start_frame&&t->range.frame_count==c->range.frame_count&&
          t->rate.frames_per_second_numerator==c->rate.frames_per_second_numerator&&
          t->rate.frames_per_second_denominator==c->rate.frames_per_second_denominator;
        free(t);if(!ok)return fail(d,n,"sequence and scene presentation clocks must match");
    }
    r->sequence_revision=SequenceClockFingerprint(c);r->has_sequence=true;return true;
}
TimelineStatus RuntimeRigidSequenceEvaluate(const RuntimeRigidResult *r,const TimelineEvaluationContext *c,
                                           SequenceEvaluation *out,bool *present) {
    if(!c||!out||!present)return TIMELINE_STATUS_INVALID_ARGUMENT;
    if(!r||!r->has_sequence){*present=false;return TIMELINE_STATUS_OK;}
    if(c->range.start_frame!=r->sequence.range.start_frame||c->range.frame_count!=r->sequence.range.frame_count||
       c->rate.frames_per_second_numerator!=r->sequence.rate.frames_per_second_numerator||
       c->rate.frames_per_second_denominator!=r->sequence.rate.frames_per_second_denominator)
        return TIMELINE_STATUS_INVALID_SNAPSHOT;
    SequenceEvaluation value;
    if(SequenceClockEvaluate(&r->sequence,c->sample,&value)!=SEQUENCE_OK)return TIMELINE_STATUS_INVALID_SNAPSHOT;
    *out=value;*present=true;return TIMELINE_STATUS_OK;
}
#endif
