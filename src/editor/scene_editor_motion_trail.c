/* Derived direct-key view and one XYZ command. No separate serialized route. */
#include "editor/scene_editor_motion_trail.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_document_timeline.h"
#include "editor/scene_editor_object_timeline.h"
#include "import/runtime_scene_object_timeline.h"
#include "motion/scene_motion_paths.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static int frame_compare(const void *a,const void *b) {
  int64_t x=*(const int64_t*)a,y=*(const int64_t*)b;return (x>y)-(x<y);
}
bool SceneEditorMotionTrailRead(const char *target,SceneEditorMotionTrail *out) {
  if(!target || !out || strncmp(target,"object/",7))return false;
  SceneEditorDocumentObjectInfo info;
  if(!SceneEditorDocumentObjectById(target+7,&info) || !info.visible)return false;
  static TimelineDocument doc;
  if(SceneEditorDocumentGetTimeline(&doc)!=TIMELINE_STATUS_OK)return false;
  memset(out,0,sizeof(*out));unsigned axes=0;
  for(size_t i=0;i<doc.track_count;++i) {
    const TimelineTrack *t=&doc.tracks[i];if(!t->enabled || strcmp(t->target_id,target))continue;
    if(!strcmp(t->property_id,MOTION_PROGRESS_PROPERTY))return false;
    int axis=RuntimeObjectTimelineAxis(t->property_id);if(axis<0)continue;
    if(axes&(1u<<axis))return false;
    out->axes[axis]=*t;axes|=1u<<axis;
    for(size_t k=0;k<t->key_count;++k)out->frames[out->count++]=t->keys[k].frame;
  }
  if(axes!=7)return false;
  qsort(out->frames,out->count,sizeof(out->frames[0]),frame_compare);
  size_t n=0;for(size_t i=0;i<out->count;++i)if(!i || out->frames[i]!=out->frames[i-1])out->frames[n++]=out->frames[i];out->count=n;
  snprintf(out->target,sizeof(out->target),"%s",target);out->rate=doc.rate;out->range=doc.range;out->revision=SceneEditorDocumentRevision();return true;
}
bool SceneEditorMotionTrailSample(const SceneEditorMotionTrail *trail,double frame,double xyz[3]) {
  if(!trail || !xyz || !isfinite(frame))return false;
  TimelineEvaluationContext c;
  if(TimelineEvaluationContextBuild(trail->rate,trail->range,(TimelineSample){trail->range.start_frame,0,1},&c)!=TIMELINE_STATUS_OK)return false;
  c.absolute_frame_position=frame;c.local_frame_position=frame-trail->range.start_frame;
  c.normalized_t=trail->range.frame_count>1?c.local_frame_position/(trail->range.frame_count-1):0;
  c.local_time_seconds=c.local_frame_position*trail->rate.frames_per_second_denominator/trail->rate.frames_per_second_numerator;
  for(int a=0;a<3;++a){TimelineEvaluationResult v;if(TimelineTrackEvaluate(&trail->axes[a],&c,&v)!=TIMELINE_STATUS_OK)return false;xyz[a]=v.value.as.scalar;}
  return true;
}
bool SceneEditorMotionTrailSetKey(const char *target,int64_t frame,const double xyz[3],unsigned long long revision,char *message,size_t size) {
  SceneEditorMotionTrail trail;
  if(!xyz || !target || !message || !size)return false;
  if(revision!=SceneEditorDocumentRevision() || !SceneEditorMotionTrailRead(target,&trail)) {
    snprintf(message,size,"XYZ trail changed or source incomplete; no edit applied.");return false;
  }
  if(!SceneEditorObjectTimelineEditable(target,message,size))return false;
  int64_t end;if(TimelineRangeEndFrame(trail.range,&end)!=TIMELINE_STATUS_OK || frame<trail.range.start_frame || frame>end)return false;
  static TimelineDocument doc;if(SceneEditorDocumentGetTimeline(&doc)!=TIMELINE_STATUS_OK)return false;
  for(int a=0;a<3;++a) {
    if(!isfinite(xyz[a]))return false;
    TimelineTrack *t=&trail.axes[a];size_t k=0;while(k<t->key_count && t->keys[k].frame<frame)++k;
    if(k<t->key_count && t->keys[k].frame==frame)t->keys[k].value=TimelineValueScalar(xyz[a]);
    else {
      TimelineKeyframe key={.frame=frame,.value=TimelineValueScalar(xyz[a]),.interpolation_to_next=TIMELINE_INTERPOLATION_LINEAR};
      size_t prior=k?k-1:0;
      if(t->keys[prior].tangent_mode!=TIMELINE_TANGENT_BROKEN && t->keys[prior].interpolation_to_next==TIMELINE_INTERPOLATION_CUBIC_BEZIER){key.tangent_mode=t->keys[prior].tangent_mode;key.interpolation_to_next=TIMELINE_INTERPOLATION_CUBIC_BEZIER;}
      if(TimelineTrackInsertKey(t,key,&k)!=TIMELINE_STATUS_OK){snprintf(message,size,"XYZ trail key capacity reached; nothing changed.");return false;}
    }
    if(TimelineTrackRecomputeTangents(t)!=TIMELINE_STATUS_OK)return false;
    for(size_t i=0;i+1<t->key_count;++i) {
      TimelineKeyframe *l=&t->keys[i],*r=&t->keys[i+1];double span=(double)r->frame-l->frame,extent=l->outgoing_frame_offset-r->incoming_frame_offset;
      if(extent>span){double scale=span/extent;l->outgoing_frame_offset*=scale;l->outgoing_value_offset*=scale;r->incoming_frame_offset*=scale;r->incoming_value_offset*=scale;}
    }
    bool found=false;for(size_t i=0;i<doc.track_count;++i)if(!strcmp(doc.tracks[i].track_id,t->track_id)){doc.tracks[i]=*t;found=true;break;}if(!found)return false;
  }
  return SceneEditorDocumentSetTimeline(&doc,revision,message,size);
}
