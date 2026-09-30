/* Read-only timing feedback; no evaluated sample is written to authoring. */
#include "scene_editor_motion_feedback.h"
#include "motion/scene_motion_paths.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static bool slope(const TimelineKeyframe *a,const TimelineKeyframe *b,bool end,double *out) {
  if(a->interpolation_to_next==TIMELINE_INTERPOLATION_STEP){*out=0;return true;}
  if(a->interpolation_to_next==TIMELINE_INTERPOLATION_LINEAR){*out=(b->value.as.scalar-a->value.as.scalar)/((double)b->frame-a->frame);return true;}
  double dx=end?b->incoming_frame_offset:a->outgoing_frame_offset;
  double dy=end?b->incoming_value_offset:a->outgoing_value_offset;
  if(fabs(dx)<1e-12)return false;
  *out=dy/dx;return isfinite(*out);
}
bool SceneEditorMotionFeedback(const TimelineDocument *doc,size_t selected,TimelineSample sample,char *text,size_t size) {
  if(!doc || selected>=doc->track_count || !text || !size)return false;
  const TimelineTrack *t=&doc->tracks[selected];
  if(t->value_type!=TIMELINE_VALUE_SCALAR || !t->enabled)return false;
  TimelineEvaluationContext context;TimelineEvaluationResult value;
  if(TimelineEvaluationContextBuild(doc->rate,doc->range,sample,&context)!=TIMELINE_STATUS_OK || TimelineTrackEvaluate(t,&context,&value)!=TIMELINE_STATUS_OK)return false;
  double derivative=value.derivative_per_frame;bool valid=value.derivative_valid,jump=false,cusp=false;
  bool held=value.held;
  for(size_t i=0;i<t->key_count && value.exact_key;++i)if(context.absolute_frame_position==(double)t->keys[i].frame) {
    double left=0,right=0;bool l=i?slope(&t->keys[i-1],&t->keys[i],true,&left):false;
    bool r=i+1<t->key_count?slope(&t->keys[i],&t->keys[i+1],false,&right):false;
    jump=i && t->keys[i-1].interpolation_to_next==TIMELINE_INTERPOLATION_STEP && t->keys[i-1].value.as.scalar!=t->keys[i].value.as.scalar;
    cusp=i && i+1<t->key_count && l && r && fabs(left-right)>1e-8*fmax(1,fmax(fabs(left),fabs(right)));
    valid=(l || r) && !jump && !cusp;derivative=r?right:left;
    if(i && i+1<t->key_count && (!l || !r))valid=false;
  }
  if(t->key_count==1){held=true;derivative=0;valid=true;}
  /* A constant cubic/linear segment is a hold even without Step interpolation. */
  for(size_t i=0;i+1<t->key_count;++i) {
    const TimelineKeyframe *a=&t->keys[i],*b=&t->keys[i+1];
    if(context.absolute_frame_position>a->frame && context.absolute_frame_position<b->frame &&
       a->value.as.scalar==b->value.as.scalar &&
       (a->interpolation_to_next!=TIMELINE_INTERPOLATION_CUBIC_BEZIER ||
        (a->outgoing_value_offset==0 && b->incoming_value_offset==0))) held=true;
  }
  if(held){derivative=0;valid=true;}
  double rate=derivative*(double)doc->rate.frames_per_second_numerator/doc->rate.frames_per_second_denominator;
  bool route=!strcmp(t->property_id,MOTION_PROGRESS_PROPERTY) || !strcmp(t->property_id,MOTION_CAMERA_PROGRESS_PROPERTY) || !strcmp(t->property_id,MOTION_LIGHT_PROGRESS_PROPERTY);
  const char *state=jump?"jump: speed undefined":cusp?"velocity discontinuity":!valid?"slope undefined":held?"hold interval":fabs(rate)<1e-9?"zero rate":"moving";
  if(route) {
    TimelineVec3 position;double length,parameter;
    if(!MotionPathsRuntimeTargetSample(t->target_id,value.value.as.scalar,&position,&length,&parameter))return false;
    if(valid)snprintf(text,size,"Progress %.4g | Speed %.4g world/s | %s",value.value.as.scalar,fabs(rate)*length,state);
    else snprintf(text,size,"Progress %.4g | %s",value.value.as.scalar,state);
  } else {
    const char *unit=t->unit==TIMELINE_UNIT_WORLD_DISTANCE?"world":t->unit==TIMELINE_UNIT_RADIANS?"rad":t->unit==TIMELINE_UNIT_DEGREES?"deg":t->unit==TIMELINE_UNIT_RELATIVE_INTENSITY?"intensity":"value";
    if(valid)snprintf(text,size,"%.4g %s | Rate %.4g %s/s | %s",value.value.as.scalar,unit,rate,unit,state);
    else snprintf(text,size,"%.4g %s | %s",value.value.as.scalar,unit,state);
  }
  return true;
}
