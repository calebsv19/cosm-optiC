#include "motion/motion_route_schedule.h"
#include <math.h>
#include <string.h>
static MotionTimingStatus fail(MotionTimingConflict *c,MotionTimingConflictKind kind,size_t i) {
    if(c)*c=(MotionTimingConflict){.kind=kind,.waypoint=i};return MOTION_TIMING_INFEASIBLE;
}
MotionTimingStatus MotionRouteScheduleBuild(const MotionRouteGeometry *g,const MotionTimingScheduleRequest *r,MotionRouteSchedule *out,MotionTimingConflict *conflict) {
    if(!g||!r||!out||r->count<2||r->count>32||!isfinite(g->length)||g->length<=0)return MOTION_TIMING_INVALID_INPUT;
    if(!isfinite(r->max_speed)||!isfinite(r->acceleration)||!isfinite(r->braking)||r->max_speed<=0||r->acceleration<=0||r->braking<=0) {
        if(conflict)*conflict=(MotionTimingConflict){.kind=MOTION_CONFLICT_INPUT};return MOTION_TIMING_INVALID_INPUT;
    }
    if(!g->stopping_certificate)return fail(conflict,MOTION_CONFLICT_ROUTE_SINGULAR,0);
    MotionTimingScheduleRequest effective=*r;
    double k=g->regular_curvature_bound,w=g->stop_curvature_factor;
    if(k>0||w>0) {
        const double split=0.7071067811865475244,margin=.99;
        double normal=fmin(r->acceleration,r->braking)*split;
        effective.acceleration*=split*margin;effective.braking*=split*margin;
        if(w>0) {
            double tangential=normal/(2*w)*margin;
            effective.acceleration=fmin(effective.acceleration,tangential);
            effective.braking=fmin(effective.braking,tangential);
        }
        if(k>0)effective.max_speed=fmin(effective.max_speed,sqrt(normal/k))*margin;
    }
    for(size_t i=0;i<r->count;++i) {
        double x=r->points[i].position;
        if(!isfinite(x)||x<0||x>g->length)return fail(conflict,MOTION_CONFLICT_INPUT,i);
        if(r->points[i].speed>effective.max_speed)return fail(conflict,MOTION_CONFLICT_CURVE_SPEED,i);
        /* A partial traversal starting mid-segment must satisfy the same
         * rest-origin speed envelope used by the weighted curvature proof. */
        for(size_t segment=0;segment+1<g->path.count;++segment) {
            int origin=g->curvature_stop_point[segment];
            if(origin<0||x<g->point_distances[segment]||x>g->point_distances[segment+1])continue;
            double distance=fabs(x-g->point_distances[origin]);
            double speed_bound=sqrt(2*fmax(effective.acceleration,effective.braking)*distance);
            if(r->points[i].speed>speed_bound)return fail(conflict,MOTION_CONFLICT_CURVE_SPEED,i);
        }
        for(size_t j=0;j<g->path.count;++j)if(g->required_stop[j]) {
            double stop=g->point_distances[j];
            if(x==stop&&r->points[i].speed!=0)return fail(conflict,MOTION_CONFLICT_CORNER_STOP,i);
            if(i && stop>fmin(x,r->points[i-1].position)&&stop<fmax(x,r->points[i-1].position))
                return fail(conflict,MOTION_CONFLICT_CORNER_STOP,i);
        }
    }
    MotionRouteSchedule result={.requested_speed=r->max_speed,.requested_acceleration=r->acceleration,.requested_braking=r->braking,.curvature_bound=k,.stop_curvature_factor=w};
    MotionTimingStatus status=MotionTimingScheduleBuild(&effective,&result.timeline,conflict);
    if(status==MOTION_TIMING_OK)*out=result;
    return status;
}

bool MotionRouteScheduleSample(const MotionRouteGeometry *g,const MotionRouteSchedule *plan,double t,MotionRouteSample *out) {
    if(!g||!plan||!out)return false;
    MotionTimingScheduleSample timing;
    if(MotionTimingScheduleSampleAt(&plan->timeline,t,&timing)!=MOTION_TIMING_OK)return false;
    MotionRouteFrame frame;
    double distance=timing.position;
    if(distance<0 && distance>=-1e-12*g->length)distance=0;
    if(distance>g->length&&distance<=g->length*(1+1e-12))distance=g->length;
    bool left=timing.velocity<0||(timing.velocity==0&&timing.acceleration<0);
    if(!MotionRouteGeometryDistanceSide(g,distance,left,&frame)||(!frame.regular&&timing.velocity!=0))return false;
    MotionRouteSample result={.stationary=timing.stationary};
    for(int k=0;k<3;++k) {
        result.position[k]=frame.position[k];result.velocity[k]=frame.tangent[k]*timing.velocity;
        result.acceleration[k]=frame.tangent[k]*timing.acceleration+frame.curvature[k]*timing.velocity*timing.velocity;
        if(!isfinite(result.position[k])||!isfinite(result.velocity[k])||!isfinite(result.acceleration[k]))return false;
    }
    *out=result;return true;
}
