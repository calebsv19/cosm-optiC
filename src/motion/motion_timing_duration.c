#include "motion/motion_timing_duration.h"
#include <float.h>
#include <math.h>
#include <stdbool.h>

/* All admissible monotone speed profiles lie between these envelopes:
 * upper=min(vmax,v0+a*t,v1+b*(T-t)), lower=max(0,v0-b*t,v1-a*(T-t)).
 * Their convex combination preserves endpoints, slopes and speed bounds.
 * Normalized time keeps breakpoint comparisons independent of duration. */
typedef struct Envelope {
    size_t count;
    double u[8], low[8], high[8], low_area, high_area;
} Envelope;
static bool envelopes(const MotionTimingRequest *r,double t,Envelope *e) {
    double a=r->acceleration*t,b=r->braking*t;
    if(!isfinite(a)||!isfinite(b))return false;
    double c[6]={r->max_speed,r->start_speed,r->end_speed+b,0,r->start_speed,r->end_speed-a};
    double m[6]={0,a,-b,0,-b,a};
    for(int i=0;i<6;++i)if(!isfinite(c[i]))return false;
    *e=(Envelope){.count=2,.u={0,1}};
    for(int group=0;group<6;group+=3)for(int i=group;i<group+3;++i)for(int j=i+1;j<group+3;++j) {
        double den=m[i]-m[j];if(den==0)continue;
        if(!isfinite(den))return false;
        double u=(c[j]-c[i])/den;
        if(u>0 && u<1) {
            if(u<=32*DBL_EPSILON || u>=1-32*DBL_EPSILON) {
                /* Roundoff intersections at an endpoint have negligible
                 * speed change; a real unresolved ramp must be refused. */
                if(fabs(den)*fmin(u,1-u)<=64*DBL_EPSILON*r->max_speed)continue;
                return false;
            }
            size_t k=0;while(k<e->count && e->u[k]<u)++k;
            if((k<e->count && fabs(e->u[k]-u)<=32*DBL_EPSILON) ||
               (k>0 && fabs(e->u[k-1]-u)<=32*DBL_EPSILON))continue;
            for(size_t n=e->count;n>k;--n)e->u[n]=e->u[n-1];
            e->u[k]=u;++e->count;
        }
    }
    for(size_t i=0;i<e->count;++i) {
        double u=e->u[i];
        /* Endpoint-based expressions avoid cancellation at u=1. */
        e->low[i]=fmax(0,fmax(r->start_speed-b*u,r->end_speed-a*(1-u)));
        e->high[i]=fmin(r->max_speed,fmin(r->start_speed+a*u,r->end_speed+b*(1-u)));
        double eps=64*DBL_EPSILON*r->max_speed;
        if(e->low[i]>e->high[i]+eps)return false;
        if(i) {
            double dt=(e->u[i]-e->u[i-1])*t;
            e->low_area+=(e->low[i]*.5+e->low[i-1]*.5)*dt;
            e->high_area+=(e->high[i]*.5+e->high[i-1]*.5)*dt;
        }
    }
    return isfinite(e->low_area)&&isfinite(e->high_area);
}
MotionTimingStatus MotionTimingDurationRange(const MotionTimingRequest *r,double *minimum,double *maximum) {
    if(!minimum||!maximum||minimum==maximum)return MOTION_TIMING_INVALID_INPUT;
    MotionTimingPlan fastest;
    MotionTimingStatus status=MotionTimingPlanBuild(r,&fastest);
    if(status!=MOTION_TIMING_OK)return status;
    double d=fabs(r->displacement);
    double stop=(r->start_speed/r->braking)*(r->start_speed*.5)+
                (r->end_speed/r->acceleration)*(r->end_speed*.5);
    if(!isfinite(stop))return MOTION_TIMING_NUMERIC_RANGE;
    double longest=INFINITY;
    if(d>0 && d<=stop) {
        double lo=fastest.duration,hi=r->start_speed/r->braking+r->end_speed/r->acceleration;
        if(!isfinite(hi)||hi<lo)return MOTION_TIMING_NUMERIC_RANGE;
        for(int i=0;i<80 && d<stop;++i) {
            double mid=lo+(hi-lo)*.5;Envelope e;
            if(!envelopes(r,mid,&e))return MOTION_TIMING_NUMERIC_RANGE;
            if(e.low_area>d)hi=mid;else lo=mid;
        }
        longest=d==stop?hi:lo;
    }
    *minimum=fastest.duration;*maximum=longest;return MOTION_TIMING_OK;
}
MotionTimingStatus MotionTimingFixedBuild(const MotionTimingRequest *r,double t,MotionTimingFixedPlan *out) {
    if(!out||!isfinite(t)||t<0)return MOTION_TIMING_INVALID_INPUT;
    double shortest,longest;MotionTimingStatus status=MotionTimingDurationRange(r,&shortest,&longest);
    if(status!=MOTION_TIMING_OK)return status;
    if(t<shortest||t>longest)return MOTION_TIMING_INFEASIBLE;
    MotionTimingFixedPlan p={.request=*r,.duration=t};
    if(t==0){p.count=1;*out=p;return MOTION_TIMING_OK;}
    Envelope e;if(!envelopes(r,t,&e))return MOTION_TIMING_NUMERIC_RANGE;
    double d=fabs(r->displacement),eps=256*DBL_EPSILON*fmax(d,e.low_area);
    if(d<e.low_area-eps||d>e.high_area+eps)return MOTION_TIMING_INFEASIBLE;
    double span=e.high_area-e.low_area;
    double blend=span>0?fmax(0,fmin(1,(d-e.low_area)/span)):0;
    p.count=e.count;
    for(size_t i=0;i<p.count;++i) {
        p.times[i]=e.u[i]*t;
        p.speeds[i]=(1-blend)*e.low[i]+blend*e.high[i];
        if(i) {
            double dt=p.times[i]-p.times[i-1];
            if(dt<=0)return MOTION_TIMING_NUMERIC_RANGE;
            /* Obtain slopes from the active lines, not cancellation-prone
             * differences between nearby speed samples. */
            double mid=(e.u[i]+e.u[i-1])*.5;
            double upper[3]={r->max_speed,r->start_speed+r->acceleration*t*mid,r->end_speed+r->braking*t*(1-mid)};
            double lower[3]={0,r->start_speed-r->braking*t*mid,r->end_speed-r->acceleration*t*(1-mid)};
            double us[3]={0,r->acceleration,-r->braking},ls[3]={0,-r->braking,r->acceleration};
            int u=0,l=0;for(int k=1;k<3;++k){if(upper[k]<upper[u])u=k;if(lower[k]>lower[l])l=k;}
            p.accelerations[i-1]=(1-blend)*ls[l]+blend*us[u];
            p.distances[i]=p.distances[i-1]+(p.speeds[i]*.5+p.speeds[i-1]*.5)*dt;
            if(p.speeds[i]==0&&p.speeds[i-1]==0)p.stationary_seconds+=dt;
        }
    }
    if(fabs(p.distances[p.count-1]-d)>eps || (d>0 && p.stationary_seconds>0))return MOTION_TIMING_NUMERIC_RANGE;
    *out=p;return MOTION_TIMING_OK;
}
MotionTimingStatus MotionTimingFixedSample(const MotionTimingFixedPlan *p,double t,MotionTimingSample *out) {
    if(!p||!out||!isfinite(t)||p->count<1||p->count>8)return MOTION_TIMING_INVALID_INPUT;
    if(t<0||t>p->duration)return MOTION_TIMING_TIME_OUT_OF_RANGE;
    MotionTimingSample s={0};
    if(p->count==1){*out=s;return MOTION_TIMING_OK;}
    size_t i=0;while(i+2<p->count&&t>=p->times[i+1])++i;
    double local=t-p->times[i];
    s.acceleration=p->accelerations[i];
    s.velocity=p->speeds[i]+s.acceleration*local;
    s.displacement=p->distances[i]+local*(p->speeds[i]*.5+s.velocity*.5);
    if(t==p->duration){s.displacement=fabs(p->request.displacement);s.velocity=p->request.end_speed;}
    double direction=p->request.displacement<0?-1:1;
    s.displacement*=direction;s.velocity*=direction;s.acceleration*=direction;
    if(!isfinite(s.displacement)||!isfinite(s.velocity)||!isfinite(s.acceleration))return MOTION_TIMING_NUMERIC_RANGE;
    *out=s;return MOTION_TIMING_OK;
}
