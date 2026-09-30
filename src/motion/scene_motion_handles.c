#include "motion/scene_motion_paths.h"
#include <math.h>
#include <string.h>

static double length(const double v[3]) { return hypot(hypot(v[0],v[1]),v[2]); }
const char *MotionHandleModeLabel(MotionHandleMode mode) {
  switch(mode) {
    case MOTION_HANDLE_LINKED: return "Handles: Smooth / Linked";
    case MOTION_HANDLE_CORNER: return "Handles: Corner";
    default: return "Handles: Independent";
  }
}
bool MotionPathEditHandle(MotionPathPoint *p, bool incoming, const double value[3]) {
  if (!p || !value) return false;
  for(int k=0;k<3;++k) if(!isfinite(value[k]) || fabs(value[k])>1e12) return false;
  double *edited=incoming?p->incoming:p->outgoing;
  double *other=incoming?p->outgoing:p->incoming;
  double old_length=length(other), new_length=length(value);
  memcpy(edited,value,3*sizeof(double));
  /* Pulling a collapsed corner handle explicitly starts independent shaping. */
  if(p->handle_mode==MOTION_HANDLE_CORNER && new_length>0)
    p->handle_mode=MOTION_HANDLE_INDEPENDENT;
  if(p->handle_mode==MOTION_HANDLE_LINKED && new_length>1e-12) {
    if(old_length<=1e-12) old_length=new_length;
    for(int k=0;k<3;++k) other[k]=-edited[k]*old_length/new_length;
  }
  return true;
}
bool MotionPathSetHandleMode(MotionPathPoint *p, MotionHandleMode mode) {
  if(!p || mode<MOTION_HANDLE_INDEPENDENT || mode>MOTION_HANDLE_CORNER) return false;
  p->handle_mode=mode;
  if(mode==MOTION_HANDLE_CORNER) {
    memset(p->incoming,0,sizeof(p->incoming));memset(p->outgoing,0,sizeof(p->outgoing));
  } else if(mode==MOTION_HANDLE_LINKED) {
    /* Outgoing is the explicit alignment reference, incoming if it is zero.
     * Both zero stays degenerate until the user pulls a handle. */
    double v[3];bool in=length(p->outgoing)<=1e-12;
    memcpy(v,in?p->incoming:p->outgoing,sizeof(v));
    return MotionPathEditHandle(p,in,v);
  }
  return true;
}

bool MotionPathSmoothPoint(MotionPath *path, size_t index) {
  if (!path || path->count<2 || index>=path->count) return false;
  MotionPathPoint *p=&path->points[index];
  double before[3]={0},after[3]={0},direction[3];
  for(int k=0;k<3;++k) {
    if(index) before[k]=p->position[k]-path->points[index-1].position[k];
    if(index+1<path->count) after[k]=path->points[index+1].position[k]-p->position[k];
    direction[k]=before[k]+after[k];
  }
  double in_length=length(p->incoming),out_length=length(p->outgoing);
  double n=length(direction);
  if(in_length>1e-12 || out_length>1e-12) {
    for(int k=0;k<3;++k) direction[k]=out_length>1e-12?p->outgoing[k]:-p->incoming[k];
    n=length(direction);
  } else if(n<=1e-12) {
    memcpy(direction,length(after)>1e-12?after:before,sizeof(direction));n=length(direction);
  }
  if(!isfinite(n) || n<=1e-12) return false;
  if(in_length<=1e-12) in_length=(length(before)>1e-12?length(before):length(after))/3;
  if(out_length<=1e-12) out_length=(length(after)>1e-12?length(after):length(before))/3;
  for(int k=0;k<3;++k) {
    p->incoming[k]=-direction[k]/n*in_length;
    p->outgoing[k]=direction[k]/n*out_length;
  }
  p->handle_mode=MOTION_HANDLE_LINKED;
  if(index) path->points[index-1].linear=false;
  if(index+1<path->count) p->linear=false;
  return true;
}
