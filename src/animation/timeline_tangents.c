/* Retained scalar-key tangent policies. Units are value per frame; evaluation
 * converts derivatives to seconds using the document rate. Loading old handles
 * never invokes this policy. */
#include "animation/timeline_track.h"
#include <math.h>
const char* TimelineTangentModeLabel(TimelineTangentMode mode) {
    switch(mode) {
      case TIMELINE_TANGENT_AUTO_SMOOTH:return "auto_smooth";
      case TIMELINE_TANGENT_AUTO_CLAMPED:return "auto_clamped";
      case TIMELINE_TANGENT_FLAT:return "flat";
      default:return "broken";
    }
}
TimelineStatus TimelineTrackRecomputeTangents(TimelineTrack* t) {
    if(!t) return TIMELINE_STATUS_INVALID_ARGUMENT;
    for(size_t i=0;i<t->key_count;++i) {
      TimelineKeyframe* k=&t->keys[i];
      if(k->tangent_mode<TIMELINE_TANGENT_BROKEN || k->tangent_mode>TIMELINE_TANGENT_FLAT)
        return TIMELINE_STATUS_INVALID_TRACK;
      if(k->tangent_mode==TIMELINE_TANGENT_BROKEN) continue;
      if(t->value_type!=TIMELINE_VALUE_SCALAR) return TIMELINE_STATUS_UNSUPPORTED_INTERPOLATION;
      double left=i?(double)k->frame-(double)t->keys[i-1].frame:0;
      double right=i+1<t->key_count?(double)t->keys[i+1].frame-(double)k->frame:0;
      if((i && left<=0) || (i+1<t->key_count && right<=0)) return TIMELINE_STATUS_UNSORTED_KEYS;
      double a=i?(k->value.as.scalar-t->keys[i-1].value.as.scalar)/left:0;
      double b=i+1<t->key_count?(t->keys[i+1].value.as.scalar-k->value.as.scalar)/right:0;
      double slope=0;
      if(k->tangent_mode!=TIMELINE_TANGENT_FLAT) {
        if(!i) slope=b;else if(i+1==t->key_count) slope=a;
        else if(k->tangent_mode==TIMELINE_TANGENT_AUTO_SMOOTH) slope=(right*a+left*b)/(left+right);
        else if(a*b>0) {
          double w1=2*right+left,w2=right+2*left;
          slope=(w1+w2)/(w1/a+w2/b);
          slope=copysign(fmin(fabs(slope),3*fmin(fabs(a),fabs(b))),slope);
        }
      }
      k->incoming_frame_offset=-left/3;k->outgoing_frame_offset=right/3;
      k->incoming_value_offset=k->incoming_frame_offset*slope;
      k->outgoing_value_offset=k->outgoing_frame_offset*slope;
    }
    return TIMELINE_STATUS_OK;
}
