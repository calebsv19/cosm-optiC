#include "motion/scene_motion_plans.h"
#include "import/runtime_scene_object_timeline.h"
#include "motion/scene_motion_paths.h"
#include "import/runtime_scene_timeline.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
int RuntimeObjectTimelineAxis(const char* property) {
    const char* names[]={"object/transform/position_x","object/transform/position_y","object/transform/position_z"};
    for(int i=0;i<3;++i) if(property && !strcmp(property,names[i])) return i;
    return -1;
}
static json_object* member(json_object* o,const char* key) {json_object* v=NULL;if(o) json_object_object_get_ex(o,key,&v);return v;}
static const char* string(json_object* o,const char* key) {json_object* v=member(o,key);return json_object_is_type(v,json_type_string)?json_object_get_string(v):"";}
static bool refuse(char* message,size_t size,const char* id,const char* why) {if(message && size) snprintf(message,size,"Object animation %s: %s",id,why);return false;}
bool RuntimeObjectTimelineValidate(json_object* scene,const TimelineDocument* doc,char* message,size_t size) {
    json_object* objects=member(scene,"objects");
    json_object* authoring=member(member(member(scene,"extensions"),"ray_tracing"),"authoring");
    json_object* motions=member(authoring,"object_motion_tracks");
    for(size_t i=0;i<doc->track_count;++i) {
        const TimelineTrack* t=&doc->tracks[i];if(!t->enabled || (RuntimeObjectTimelineAxis(t->property_id)<0 && strcmp(t->property_id,MOTION_PROGRESS_PROPERTY))) continue;
        const char* id=t->target_id+7;size_t matches=0;json_object* object=NULL;
        for(size_t j=0;json_object_is_type(objects,json_type_array) && j<json_object_array_length(objects);++j) {
            json_object* o=json_object_array_get_idx(objects,j);if(!strcmp(string(o,"object_id"),id)) {++matches;object=o;}
        }
        if(matches!=1) return refuse(message,size,id,"missing or duplicate object identity");
        const char* type=string(object,"object_type");
        if(strcmp(type,"mesh_asset_instance") && strcmp(type,"plane_primitive") && strcmp(type,"rect_prism_primitive") && strcmp(type,"triangle_mesh") && strcmp(type,"box") && strcmp(type,"rect_prism") && strcmp(type,"plane"))
            return refuse(message,size,id,"position animation supports mesh, box and plane objects");
        unsigned axes=0;
        for(size_t j=0;j<doc->track_count;++j) {
            const TimelineTrack* other=&doc->tracks[j];int axis=RuntimeObjectTimelineAxis(other->property_id);
            if(other->enabled && !strcmp(t->target_id,other->target_id) && axis>=0) axes|=1u<<axis;
        }
        if(strcmp(t->property_id,MOTION_PROGRESS_PROPERTY) && axes!=7) return refuse(message,size,id,"X, Y and Z channels must be enabled together");
        for(size_t j=0;json_object_is_type(motions,json_type_array) && j<json_object_array_length(motions);++j) {
            json_object* m=json_object_array_get_idx(motions,j),*enabled=member(m,"enabled");
            if(!strcmp(string(m,"object_id"),id) && (!enabled || json_object_get_boolean(enabled)))
                return refuse(message,size,id,"existing motion or simulation already owns this transform");
        }
    }
    return true;
}
/* The runtime document is a derived cache. No samples write authored transforms. */
TimelineStatus RuntimeObjectTimelinePosition(const char* id,const TimelineEvaluationContext* context,TimelineVec3* out) {
    if(!id || !context || !out) return TIMELINE_STATUS_INVALID_ARGUMENT;
    const TimelineDocument* d=RuntimeSceneTimelineRead();if(!d) return TIMELINE_STATUS_TARGET_NOT_FOUND;
    TimelineStatus status=TIMELINE_STATUS_OK;double xyz[3]={0};unsigned axes=0;
    for(size_t i=0;status==TIMELINE_STATUS_OK && i<d->track_count;++i) {
        const TimelineTrack* t=&d->tracks[i];int axis=RuntimeObjectTimelineAxis(t->property_id);
        if(!t->enabled || strncmp(t->target_id,"object/",7) || strcmp(t->target_id+7,id)) continue;
        if(!strcmp(t->property_id,MOTION_PROGRESS_PROPERTY)) {
            TimelineEvaluationResult result;status=TimelineTrackEvaluate(t,context,&result);
            if(status!=TIMELINE_STATUS_OK)return status;
            if(!MotionPlansRuntimeEvaluate(context,&result))return TIMELINE_STATUS_INVALID_SNAPSHOT;
            return MotionPathsRuntimePosition(id,result.value.as.scalar,out)?TIMELINE_STATUS_OK:TIMELINE_STATUS_TARGET_NOT_FOUND;
        }
        if(axis<0)continue;
        TimelineEvaluationResult result;status=TimelineTrackEvaluate(t,context,&result);
        if(status==TIMELINE_STATUS_OK) {xyz[axis]=result.value.as.scalar;axes|=1u<<axis;}
    }
    if(status!=TIMELINE_STATUS_OK) return status;
    if(axes!=7) return axes?TIMELINE_STATUS_INVALID_TRACK:TIMELINE_STATUS_TARGET_NOT_FOUND;
    *out=(TimelineVec3){xyz[0],xyz[1],xyz[2]};return TIMELINE_STATUS_OK;
}
bool RuntimeObjectTimelineHasMotion(void) {
    const TimelineDocument* d=RuntimeSceneTimelineRead();
    bool found=false;if(d)
        for(size_t i=0;i<d->track_count;++i) if(d->tracks[i].enabled && (RuntimeObjectTimelineAxis(d->tracks[i].property_id)>=0 || !strcmp(d->tracks[i].property_id,MOTION_PROGRESS_PROPERTY))) {found=true;break;}
    return found;
}
bool RuntimeObjectTimelinePositionAtT(const char* id,double t,TimelineVec3* out) {
    TimelineRate rate;TimelineRange range;TimelineEvaluationContext context;
    if(!isfinite(t) || RuntimeSceneTimelineClock(&rate,&range)!=TIMELINE_STATUS_OK ||
       TimelineEvaluationContextBuild(rate,range,(TimelineSample){range.start_frame,0,1},&context)!=TIMELINE_STATUS_OK) return false;
    /* Existing geometry builders accept normalized time. Preserve its full double
       precision when evaluating; exact sample capture uses Position directly. */
    context.normalized_t=fmax(0,fmin(1,t));context.local_frame_position=context.normalized_t*(range.frame_count-1);
    context.absolute_frame_position=range.start_frame+context.local_frame_position;
    context.local_time_seconds=context.local_frame_position*rate.frames_per_second_denominator/rate.frames_per_second_numerator;
    return RuntimeObjectTimelinePosition(id,&context,out)==TIMELINE_STATUS_OK;
}
bool RuntimeObjectTimelineRotation(const char* id,const TimelineEvaluationContext* context,TimelineVec3* out) {
    const TimelineDocument* d=RuntimeSceneTimelineRead();if(!d)return false;
    for(size_t i=0;i<d->track_count;++i) {
        const TimelineTrack* t=&d->tracks[i];
        if(!t->enabled || strncmp(t->target_id,"object/",7) || strcmp(t->target_id+7,id) || strcmp(t->property_id,MOTION_PROGRESS_PROPERTY))continue;
        TimelineEvaluationResult result;
        return TimelineTrackEvaluate(t,context,&result)==TIMELINE_STATUS_OK && MotionPlansRuntimeEvaluate(context,&result) &&
            MotionPathsRuntimeRotation(t->target_id,result.value.as.scalar,out);
    }
    return false;
}
bool RuntimeObjectTimelineRotationAtT(const char* id,double t,TimelineVec3* out) {
    TimelineRate rate;TimelineRange range;TimelineEvaluationContext context;
    if(!isfinite(t) || RuntimeSceneTimelineClock(&rate,&range)!=TIMELINE_STATUS_OK ||
       TimelineEvaluationContextBuild(rate,range,(TimelineSample){range.start_frame,0,1},&context)!=TIMELINE_STATUS_OK)return false;
    context.normalized_t=fmax(0,fmin(1,t));context.local_frame_position=context.normalized_t*(range.frame_count-1);
    context.absolute_frame_position=range.start_frame+context.local_frame_position;
    context.local_time_seconds=context.local_frame_position*rate.frames_per_second_denominator/rate.frames_per_second_numerator;
    return RuntimeObjectTimelineRotation(id,&context,out);
}
TimelineStatus RuntimeObjectTimelineCapture(const TimelineEvaluationContext* context,RayEvaluatedObjectTransform* transforms,size_t capacity,size_t* count) {
    const TimelineDocument* d=RuntimeSceneTimelineRead();if(!d) return TIMELINE_STATUS_OK;
    TimelineStatus status=TIMELINE_STATUS_OK;
    for(size_t i=0;status==TIMELINE_STATUS_OK && i<d->track_count;++i) {
        const TimelineTrack* t=&d->tracks[i];if(!t->enabled || (RuntimeObjectTimelineAxis(t->property_id)!=0 && strcmp(t->property_id,MOTION_PROGRESS_PROPERTY))) continue;
        for(size_t j=0;j<*count;++j) if(!strcmp(transforms[j].target_id,t->target_id+7)) status=TIMELINE_STATUS_DUPLICATE_OWNERSHIP;
        if(status!=TIMELINE_STATUS_OK) break;
        if(*count>=capacity) {status=TIMELINE_STATUS_CAPACITY_EXCEEDED;break;}
        RayEvaluatedObjectTransform value={.valid=true,.source=RAY_EVALUATED_OBJECT_TRANSFORM_SCENE_TIMELINE,.has_position=true,.frame=*context};
        snprintf(value.target_id,sizeof(value.target_id),"%s",t->target_id+7);
        status=RuntimeObjectTimelinePosition(value.target_id,context,&value.position);
        if(status==TIMELINE_STATUS_OK) {value.has_rotation=RuntimeObjectTimelineRotation(value.target_id,context,&value.rotation_radians);transforms[(*count)++]=value;}
    }
    return status;
}
