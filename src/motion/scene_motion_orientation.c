/* Stateless route orientation: local forward axis -> increasing route tangent.
 * World +Y is the up reference; vertical tangents use +Z. No banking/history. */
#include "motion/scene_motion_paths.h"
#include <math.h>
static double dot(const double a[3],const double b[3]) {return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
static bool norm(double a[3]) {double n=sqrt(dot(a,a));if(n<1e-12)return false;for(int i=0;i<3;++i)a[i]/=n;return true;}
static void cross(const double a[3],const double b[3],double c[3]) {c[0]=a[1]*b[2]-a[2]*b[1];c[1]=a[2]*b[0]-a[0]*b[2];c[2]=a[0]*b[1]-a[1]*b[0];}
bool MotionPathsRuntimeRotation(const char *target,double progress,TimelineVec3 *out) {
  MotionPathBinding b;if(!out || !MotionPathsRuntimeBinding(target,&b) || !b.follow_direction || b.target_id[0])return false;
  double t=fmax(0,fmin(1,progress)),f[3];TimelineVec3 a,c;bool found=false;
  for(double step=1e-5;step<=1.01;step*=10) {
    if(!MotionPathsRuntimeTargetPosition(target,fmax(0,t-step),&a) || !MotionPathsRuntimeTargetPosition(target,fmin(1,t+step),&c))return false;
    f[0]=c.x-a.x;f[1]=c.y-a.y;f[2]=c.z-a.z;if(norm(f)){found=true;break;}
  }
  if(!found)return false; /* Stationary route preserves authored orientation. */
  double up[3]={0,1,0};if(fabs(dot(f,up))>0.999){up[1]=0;up[2]=1;}
  double right[3],u[3];cross(up,f,right);norm(right);cross(f,right,u);
  /* Model basis uses the chosen forward and a perpendicular model-up axis. */
  double mf[3]={0},mu[3]={0},mr[3];mf[b.forward_axis/2]=(b.forward_axis%2)?-1:1;
  mu[b.forward_axis/2==1?2:1]=1;cross(mu,mf,mr);
  double m[3][3],o[3][3],r[3][3];
  double x=b.rotation_offset[0]*0.017453292519943295,y=b.rotation_offset[1]*0.017453292519943295,z=b.rotation_offset[2]*0.017453292519943295;
  double sx=sin(x),cx=cos(x),sy=sin(y),cy=cos(y),sz=sin(z),cz=cos(z);
  double offset[3][3]={{cz*cy,cz*sy*sx-sz*cx,cz*sy*cx+sz*sx},{sz*cy,sz*sy*sx+cz*cx,sz*sy*cx-cz*sx},{-sy,cy*sx,cy*cx}};
  for(int i=0;i<3;++i)for(int j=0;j<3;++j){m[i][j]=right[i]*mr[j]+u[i]*mu[j]+f[i]*mf[j];o[i][j]=offset[i][j];}
  for(int i=0;i<3;++i)for(int j=0;j<3;++j){r[i][j]=0;for(int k=0;k<3;++k)r[i][j]+=m[i][k]*o[k][j];}
  double ry=asin(fmax(-1,fmin(1,-r[2][0]))),rx,rz;
  if(fabs(cos(ry))>1e-8){rx=atan2(r[2][1],r[2][2]);rz=atan2(r[1][0],r[0][0]);}
  else {rx=0;rz=atan2(-r[0][1],r[1][1]);}
  *out=(TimelineVec3){rx,ry,rz};return true;
}
