#include "motion/motion_route_schedule.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static double magnitude(const double v[3]){return hypot(hypot(v[0],v[1]),v[2]);}
static void sample_check(const MotionRouteGeometry *g,const MotionRouteSchedule *p) {
    double duration=p->timeline.end_time-p->timeline.request.start_time;
    for(int j=0;j<=200;++j) {
        double t=p->timeline.request.start_time+duration*j/200;MotionRouteSample v;
        assert(MotionRouteScheduleSample(g,p,t,&v));
        assert(magnitude(v.velocity)<=p->requested_speed*(1+1e-9));
        assert(magnitude(v.acceleration)<=fmax(p->requested_acceleration,p->requested_braking)*(1+1e-8));
        if(j>0&&j<200) {
            double dt=duration*1e-5;MotionRouteSample l,h;
            assert(MotionRouteScheduleSample(g,p,t-dt,&l)&&MotionRouteScheduleSample(g,p,t+dt,&h));
            bool boundary=false;
            for(size_t leg=0;leg+1<p->timeline.request.count;++leg)for(size_t phase=0;phase<p->timeline.legs[leg].count;++phase)
                if(fabs(t-(p->timeline.departures[leg]+p->timeline.legs[leg].times[phase]))<2*dt)boundary=true;
            for(int k=0;k<3;++k) {
                assert(fabs((h.position[k]-l.position[k])/(2*dt)-v.velocity[k])<2e-5*fmax(1,p->timeline.request.max_speed));
                if(!boundary)assert(fabs((h.velocity[k]-l.velocity[k])/(2*dt)-v.acceleration[k])<1e-4*fmax(1,p->requested_acceleration));
            }
        }
    }
}
int main(void) {
    MotionRouteGeometry *g=malloc(sizeof(*g));assert(g);
    MotionPath p={.count=3};p.points[0].linear=p.points[1].linear=true;p.points[1].position[0]=3;p.points[2].position[0]=3;p.points[2].position[1]=4;
    assert(MotionRouteGeometryBuild(&p,1,g)==MOTION_ROUTE_OK);
    MotionTimingScheduleRequest r={.max_speed=2,.acceleration=1,.braking=1,.count=2,.points={{.position=0},{.position=7}}};
    MotionRouteSchedule plan,before;memset(&plan,0x5a,sizeof(plan));before=plan;MotionTimingConflict c;
    assert(MotionRouteScheduleBuild(g,&r,&plan,&c)==MOTION_TIMING_INFEASIBLE&&c.kind==MOTION_CONFLICT_CORNER_STOP);
    assert(!memcmp(&before,&plan,sizeof(plan)));
    r.count=3;r.points[1].position=3;r.points[2].position=7;
    assert(MotionRouteScheduleBuild(g,&r,&plan,&c)==MOTION_TIMING_OK);assert(plan.timeline.request.acceleration==1);
    r.points[0].position=7;r.points[2].position=0;
    assert(MotionRouteScheduleBuild(g,&r,&plan,&c)==MOTION_TIMING_OK);
    MotionRouteSample at_corner;assert(MotionRouteScheduleSample(g,&plan,plan.timeline.departures[1],&at_corner));
    assert(at_corner.acceleration[0]<0&&fabs(at_corner.acceleration[1])<1e-10);
    r.points[0].position=0;r.points[2].position=7;
    r.points[1].speed=.1;assert(MotionRouteScheduleBuild(g,&r,&plan,&c)==MOTION_TIMING_INFEASIBLE);r.points[1].speed=0;
    p=(MotionPath){.count=2};p.points[0].outgoing[0]=1;p.points[0].outgoing[1]=1;p.points[1].position[0]=3;p.points[1].position[2]=2;p.points[1].incoming[0]=-1;p.points[1].incoming[1]=1;
    assert(MotionRouteGeometryBuild(&p,1,g)==MOTION_ROUTE_OK&&g->stopping_certificate);
    r.count=2;r.max_speed=100;r.points[1].position=g->length;
    assert(MotionRouteScheduleBuild(g,&r,&plan,&c)==MOTION_TIMING_OK);
    sample_check(g,&plan);
    double a=plan.timeline.request.acceleration,v=plan.timeline.request.max_speed;
    assert(hypot(a,v*v*g->regular_curvature_bound)<1);
    p.points[0].outgoing[0]=p.points[0].outgoing[1]=0;
    assert(MotionRouteGeometryBuild(&p,1,g)==MOTION_ROUTE_OK&&g->stopping_certificate);
    r.points[1].position=g->length;
    assert(MotionRouteScheduleBuild(g,&r,&plan,&c)==MOTION_TIMING_OK);
    sample_check(g,&plan);
    a=plan.timeline.request.acceleration;double b=plan.timeline.request.braking;
    assert(hypot(a,2*fmax(a,b)*g->stop_curvature_factor)<1);
    r.points[0].speed=.1;assert(MotionRouteScheduleBuild(g,&r,&plan,&c)==MOTION_TIMING_INFEASIBLE);r.points[0].speed=0;
    r.points[0].position=g->length*1e-6;r.points[0].speed=.1;
    assert(MotionRouteScheduleBuild(g,&r,&plan,&c)==MOTION_TIMING_INFEASIBLE&&c.kind==MOTION_CONFLICT_CURVE_SPEED);
    r.points[0].position=0;r.points[0].speed=0;
    /* Linked direction with unequal handle lengths must pass through without
     * an extra stop; arc timing removes parameter-speed mismatch. */
    p.points[0].outgoing[0]=p.points[0].outgoing[1]=1;p.count=3;
    p.points[1].outgoing[0]=2;p.points[1].outgoing[1]=-2;
    p.points[2].position[0]=6;p.points[2].position[1]=1;p.points[2].position[2]=1;p.points[2].incoming[0]=-1;
    assert(MotionRouteGeometryBuild(&p,.5,g)==MOTION_ROUTE_OK&&g->stopping_certificate&&!g->required_stop[1]);
    r.points[1].position=g->length;r.acceleration=2;r.braking=.3;
    assert(MotionRouteScheduleBuild(g,&r,&plan,&c)==MOTION_TIMING_OK);sample_check(g,&plan);
    double normal=plan.timeline.request.max_speed*plan.timeline.request.max_speed*g->regular_curvature_bound;
    assert(hypot(normal,plan.timeline.request.acceleration)<=2&&hypot(normal,plan.timeline.request.braking)<=.3);
    r.acceleration=NAN;assert(MotionRouteScheduleBuild(g,&r,&plan,&c)==MOTION_TIMING_INVALID_INPUT);
    free(g);puts("M5.4 route budget PASS: explicit corners, endpoint rest bounds, combined tangential/normal budgets and atomic refusals");
}
