#include "motion/scene_motion_paths.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static int shape;static uint64_t revision=1;
uint64_t MotionPathsRuntimeRevision(void){return revision;}
uint64_t MotionPlansRuntimeRevision(void){return 1;}
bool MotionPathsRuntimeBinding(const char *id,MotionPathBinding *b){(void)id;memset(b,0,sizeof(*b));b->enabled=true;b->follow_direction=true;return true;}
bool MotionPathsRuntimeTargetPosition(const char *id,double t,TimelineVec3 *v){(void)id;
 if(shape==0){double a[3]={58.667247772,-17.158084869,73.846939},b[3]={60.387010504,-38.258197767,73.846939},c[3]={53.897965306,-53.830850795,73.846939},d[3]={41.151258730,-62.585579834,73.846939},s=1-t,r[3];for(int i=0;i<3;++i)r[i]=s*s*s*a[i]+3*s*s*t*b[i]+3*s*t*t*c[i]+t*t*t*d[i];*v=(TimelineVec3){r[0],r[1],r[2]};}
 else if(shape==1)*v=(TimelineVec3){cos(t*6.283185307),sin(t*6.283185307),t*4};
 else if(shape==2)*v=(TimelineVec3){sin(t*6.283185307),0,cos(t*6.283185307)};
 else *v=(TimelineVec3){0,0,0};return true;}
static double dot(const double *a,const double *b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
int main(void){
 for(shape=0;shape<3;++shape){++revision;MotionFrame saved[1001],previous;
  for(int j=0;j<=1000;++j){assert(MotionPathsRuntimeFrame("object/test",j/1000.,&saved[j]));MotionFrame f=saved[j];assert(fabs(dot(f.up,f.forward))<1e-12);assert(fabs(dot(f.up,f.up)-1)<1e-12);if(j)assert(dot(f.up,previous.up)>.999);previous=f;}
  for(int j=1000;j>=0;--j){MotionFrame f;assert(MotionPathsRuntimeFrame("object/test",j/1000.,&f));assert(!memcmp(&f,&saved[j],sizeof(f)));}
  for(int j=0;j<=1000;++j){int k=(j*313)%1001;MotionFrame f;assert(MotionPathsRuntimeFrame("object/test",k/1000.,&f));assert(!memcmp(&f,&saved[k],sizeof(f)));}
 }
 ++revision;MotionFrame f;assert(!MotionPathsRuntimeFrame("object/test",.4,&f));
 double forward[3]={0,0,1},up[3]={0,0,1};assert(MotionFrameSeed(&f,forward,up));MotionFrameRoll(&f,6.283185307);assert(fabs(dot(f.up,f.forward))<1e-12);
 puts("Motion orientation PASS: saved-plane curve, helix, vertical loop, seek-order invariance, orthonormal frames and stationary fallback");return 0;
}
