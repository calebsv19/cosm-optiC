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
