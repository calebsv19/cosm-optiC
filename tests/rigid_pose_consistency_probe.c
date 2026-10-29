/* Independent one-step kinematic oracle for the native scene adapter.
 * Accepted world-space angular velocity must advance the accepted orientation
 * over dt. No sleeping or energy/gyro claim is implied by this pose contract. */
#include "interop/rigid_scene_runtime.h"
#include <math.h>
#include <stdio.h>
static BallCollisionQuat3D expected(BallCollisionQuat3D q,BallCollisionVec3 w,double dt){
    double speed=sqrt(w.x*w.x+w.y*w.y+w.z*w.z);
    double h=.5*speed*dt,k=speed>0?sin(h)/speed:0;
    double a=cos(h),x=w.x*k,y=w.y*k,z=w.z*k;
    BallCollisionQuat3D r={a*q.w-x*q.x-y*q.y-z*q.z,
        a*q.x+x*q.w+y*q.z-z*q.y,a*q.y-x*q.z+y*q.w+z*q.x,
        a*q.z+x*q.y-y*q.x+z*q.w};
    double n=sqrt(r.w*r.w+r.x*r.x+r.y*r.y+r.z*r.z);
    return (BallCollisionQuat3D){r.w/n,r.x/n,r.y/n,r.z/n};
}
int main(int argc,char **argv){
    if(argc!=3)return 2;
    BallRigidSceneRuntime *s=ball_rigid_scene_runtime_open(argv[1],argv[2]);
    if(!s || s->faulted)return 2;
    double error=0;unsigned long samples=0,impulse_ticks=0;
    while(s->loop.tick_index<s->contract.end_tick){
        BallCollisionQuat3D old[BALL_RIGID3D_COMPOUND_WORLD_MAX_BODIES];BallCollisionVec3 spin[BALL_RIGID3D_COMPOUND_WORLD_MAX_BODIES];
        for(size_t i=0;i<s->body_count;++i){old[i]=s->bodies[i].body.orientation;spin[i]=s->bodies[i].body.angular_velocity_radps;}
        if(!ball_rigid_scene_runtime_step(s))return 2;
        for(size_t i=0;i<s->body_count;++i){
            const BallRigidBody3D *b=&s->bodies[i].body;
            if(b->is_static)continue;
            BallCollisionQuat3D e=expected(old[i],b->angular_velocity_radps,s->loop.policy.fixed_dt_seconds),q=b->orientation;
            double sign=e.w*q.w+e.x*q.x+e.y*q.y+e.z*q.z<0?-1:1;
            double delta=fmax(fmax(fabs(e.w-sign*q.w),fabs(e.x-sign*q.x)),fmax(fabs(e.y-sign*q.y),fabs(e.z-sign*q.z)));
            error=fmax(error,delta);++samples;
            if(b->angular_velocity_radps.x!=spin[i].x || b->angular_velocity_radps.y!=spin[i].y || b->angular_velocity_radps.z!=spin[i].z)++impulse_ticks;
        }
    }
    printf("{\"ok\":%s,\"samples\":%lu,\"angular_impulse_samples\":%lu,\"maximum_quaternion_component_error\":%.17g,\"tolerance\":1e-12}\n",error<=1e-12?"true":"false",samples,impulse_ticks,error);
    ball_rigid_scene_runtime_free(s);return error<=1e-12?0:1;
}
