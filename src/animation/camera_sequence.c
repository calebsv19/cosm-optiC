#include "animation/camera_sequence.h"
#include "sequence_time_internal.h"
#include <limits.h>
#include <string.h>
static bool id(const char *s, bool camera) {
    size_t n=strnlen(s,64);if(!n||n==64)return false;
    if(camera&&(strncmp(s,"camera/",7)||!s[7]))return false;
    for(size_t i=0;i<n;i++)if(!((s[i]>='a'&&s[i]<='z')||(s[i]>='A'&&s[i]<='Z')||(s[i]>='0'&&s[i]<='9')||s[i]=='/'||s[i]=='_'||s[i]=='-'))return false;
    return true;
}
static size_t find(const CameraSequence *s,const char *name){for(size_t i=0;i<s->camera_count;i++)if(!strcmp(s->cameras[i].id,name))return i;return s->camera_count;}
static bool ratio(SequenceTime t){SequenceTime o;return SequenceTimeReduce(t,&o)==SEQUENCE_OK;}
bool CameraSequenceValidate(const CameraSequence *s) {
    if(!s||!TimelineRateIsValid(s->rate)||!TimelineRangeIsValid(s->range)||!s->camera_count||s->camera_count>CAMERA_SEQUENCE_CAPACITY||!s->shot_count||s->shot_count>CAMERA_SHOT_CAPACITY)return false;
    for(size_t i=0;i<s->camera_count;i++) {
        const CameraClock *c=&s->cameras[i];
        if(!id(c->id,true)||c->policy<CAMERA_CLOCK_CONTINUOUS||c->policy>CAMERA_CLOCK_CLIP||!ratio(c->offset)||!ratio(c->speed)||!c->speed.numerator||!c->duration_frames||c->duration_frames>s->range.frame_count)return false;
        for(size_t j=0;j<i;j++)if(!strcmp(c->id,s->cameras[j].id))return false;
    }
    uint64_t end=0;
    for(size_t i=0;i<s->shot_count;i++) {
        const CameraShot *shot=&s->shots[i];size_t c=find(s,shot->camera_id);
        if(!id(shot->id,false)||c==s->camera_count||shot->start_frame!=end||!shot->frame_count||shot->frame_count>s->range.frame_count-end||!ratio(shot->clip_start))return false;
        if(s->cameras[c].policy!=CAMERA_CLOCK_CLIP&&shot->clip_start.numerator)return false;
        for(size_t j=0;j<i;j++)if(!strcmp(shot->id,s->shots[j].id))return false;
        end+=shot->frame_count;
    }
    return end==s->range.frame_count;
}
#define OK(x) do {if((x)!=SEQUENCE_OK)return false;}while(0)
bool CameraSequenceEvaluate(const CameraSequence *s,TimelineSample sample,const char *name,CameraSequenceSample *out) {
    if(!out||!CameraSequenceValidate(s))return false;
    TimelineEvaluationContext ctx;if(TimelineEvaluationContextBuild(s->rate,s->range,sample,&ctx)!=TIMELINE_STATUS_OK)return false;
    if(ctx.local_frame>=s->range.frame_count)return false;
    CameraSequenceSample v={0};
    for(size_t i=0;i<s->shot_count;i++)if(ctx.local_frame>=s->shots[i].start_frame&&ctx.local_frame-s->shots[i].start_frame<s->shots[i].frame_count){v.shot_index=i;break;}
    v.camera_index=find(s,name?name:s->shots[v.shot_index].camera_id);
    if(v.camera_index==s->camera_count)return false;
    const CameraClock *c=&s->cameras[v.camera_index];v.selected=!strcmp(c->id,s->shots[v.shot_index].camera_id);
    SequenceTime frame={1,1},frames,seconds_per_frame={s->rate.frames_per_second_denominator,s->rate.frames_per_second_numerator};
    OK(SequenceTimeAdd((SequenceTime){ctx.local_frame,1},(SequenceTime){sample.subframe_numerator,sample.subframe_denominator},&frames));
    OK(SequenceTimeMultiply(frames,seconds_per_frame,&v.presentation_time));
    SequenceTime elapsed=v.presentation_time;
    if(c->policy!=CAMERA_CLOCK_CONTINUOUS) {
        elapsed=(SequenceTime){0,1};
        for(size_t i=0;i<s->shot_count;i++) {
            const CameraShot *shot=&s->shots[i];if(strcmp(shot->camera_id,c->id))continue;
            SequenceTime start={shot->start_frame,1};if(SequenceTimeCompare(frames,start)<0)break;
            SequenceTime duration={shot->frame_count,1},local;
            OK(SequenceTimeSubtract(frames,start,&local));if(SequenceTimeCompare(local,duration)>0)local=duration;
            OK(SequenceTimeMultiply(local,seconds_per_frame,&local));
            if(c->policy==CAMERA_CLOCK_SELECTED)OK(SequenceTimeAdd(elapsed,local,&elapsed));
            else OK(SequenceTimeAdd(shot->clip_start,local,&elapsed));
        }
    }
    OK(SequenceTimeMultiply(elapsed,c->speed,&elapsed));OK(SequenceTimeAdd(c->offset,elapsed,&v.motion_time));
    SequenceTime end;OK(SequenceTimeMultiply((SequenceTime){c->duration_frames-1,1},seconds_per_frame,&end));
    v.held_at_end=SequenceTimeCompare(v.motion_time,end)>0;if(v.held_at_end)v.motion_time=end;
    OK(SequenceTimeMultiply(v.motion_time,(SequenceTime){s->rate.frames_per_second_numerator,s->rate.frames_per_second_denominator},&frame));
    uint64_t whole=frame.numerator/frame.denominator,rem=frame.numerator%frame.denominator;
    if(whole>INT64_MAX||frame.denominator>UINT32_MAX)return false;
    /* duration_frames <= presentation count: range validity proves safe addition. */
    v.motion_sample=(TimelineSample){s->range.start_frame+(int64_t)whole,(uint32_t)rem,(uint32_t)frame.denominator};
    *out=v;return true;
}
