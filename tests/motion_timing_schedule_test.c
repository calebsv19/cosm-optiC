#include "motion/motion_timing_schedule.h"
#include "animation/timeline_clock.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void near(double a,double b){assert(fabs(a-b)<1e-9*fmax(1,fmax(fabs(a),fabs(b))));}
static MotionTimingSchedule build(MotionTimingScheduleRequest r) {
    MotionTimingSchedule s;MotionTimingConflict c;
    MotionTimingStatus status=MotionTimingScheduleBuild(&r,&s,&c);
    if(status!=MOTION_TIMING_OK)fprintf(stderr,"schedule: %s point %zu status %d\n",MotionTimingConflictLabel(c.kind),c.waypoint,status);
    assert(status==MOTION_TIMING_OK);return s;
}
static MotionTimingScheduleSample sample(const MotionTimingSchedule *s,double t) {
    MotionTimingScheduleSample v;assert(MotionTimingScheduleSampleAt(s,t,&v)==MOTION_TIMING_OK);return v;
}
static void reject(MotionTimingScheduleRequest r,MotionTimingConflictKind kind) {
    MotionTimingSchedule s,before;memset(&s,0x5a,sizeof(s));before=s;MotionTimingConflict c;
    assert(MotionTimingScheduleBuild(&r,&s,&c)!=MOTION_TIMING_OK);assert(c.kind==kind);
    assert(!memcmp(&s,&before,sizeof(s)));
}
static void fixed_check(MotionTimingRequest r,double t) {
    MotionTimingFixedPlan p;MotionTimingStatus status=MotionTimingFixedBuild(&r,t,&p);
    if(status!=MOTION_TIMING_OK)fprintf(stderr,"fixed fail %d d=%g T=%.17g v0=%g v1=%g a=%g b=%g\n",status,r.displacement,t,r.start_speed,r.end_speed,r.acceleration,r.braking);
    assert(status==MOTION_TIMING_OK);
    double sign=r.displacement<0?-1:1,area=0;
    for(size_t i=1;i<p.count;++i){
        double dt=p.times[i]-p.times[i-1],a=(p.speeds[i]-p.speeds[i-1])/dt;
        assert(p.accelerations[i-1]<=r.acceleration && p.accelerations[i-1]>=-r.braking);
        assert(dt>0);assert(a<=r.acceleration*(1+1e-10)&&a>=-r.braking*(1+1e-10));
        assert(p.speeds[i]>=0&&p.speeds[i]<=r.max_speed*(1+1e-12));
        area+=(p.speeds[i]+p.speeds[i-1])*.5*dt;
        MotionTimingSample left,right;
        assert(MotionTimingFixedSample(&p,p.times[i],&right)==MOTION_TIMING_OK);
        assert(MotionTimingFixedSample(&p,nextafter(p.times[i],0),&left)==MOTION_TIMING_OK);
        near(left.displacement,right.displacement);near(left.velocity,right.velocity);
    }
    near(area,fabs(r.displacement));
    MotionTimingSample expected[101];
    for(int i=0;i<=100;++i) {
        assert(MotionTimingFixedSample(&p,t*(i/100.0),&expected[i])==MOTION_TIMING_OK);
        assert(sign*expected[i].velocity>=-1e-10&&fabs(expected[i].velocity)<=r.max_speed*(1+1e-10));
        if(i)assert(sign*(expected[i].displacement-expected[i-1].displacement)>=-1e-9);
    }
    near(expected[0].velocity,sign*r.start_speed);near(expected[100].velocity,sign*r.end_speed);
    near(expected[0].displacement,0);near(expected[100].displacement,r.displacement);
    for(int j=0;j<=100;++j){int i=100-j*37%101;MotionTimingSample v;assert(MotionTimingFixedSample(&p,t*(i/100.0),&v)==MOTION_TIMING_OK);assert(!memcmp(&v,&expected[i],sizeof(v)));}
}
int main(void) {
    MotionTimingRequest r={10,2,1,1,0,0};double lo,hi;
    assert(MotionTimingDurationRange(&r,&lo,&hi)==MOTION_TIMING_OK);near(lo,7);assert(isinf(hi));
    fixed_check(r,7);fixed_check(r,14);fixed_check(r,100);
    r=(MotionTimingRequest){1,2,1,1,2,2};
    assert(MotionTimingDurationRange(&r,&lo,&hi)==MOTION_TIMING_OK);near(lo,.5);near(hi,4-2*sqrt(3));
    fixed_check(r,lo);fixed_check(r,(lo+hi)/2);fixed_check(r,hi);
    MotionTimingFixedPlan p,before;memset(&p,0x5a,sizeof(p));before=p;
    assert(MotionTimingFixedBuild(&r,.4,&p)==MOTION_TIMING_INFEASIBLE);
    assert(MotionTimingFixedBuild(&r,1,&p)==MOTION_TIMING_INFEASIBLE);assert(!memcmp(&p,&before,sizeof(p)));
    /* Do not disguise an early arrival as an unrequested destination hold. */
    r=(MotionTimingRequest){2,2,1,1,2,0};
    assert(MotionTimingDurationRange(&r,&lo,&hi)==MOTION_TIMING_OK);near(lo,2);near(hi,2);
    fixed_check(r,2);assert(MotionTimingFixedBuild(&r,3,&p)==MOTION_TIMING_INFEASIBLE);
    r=(MotionTimingRequest){4,2,1,1,2,2};
    assert(MotionTimingDurationRange(&r,&lo,&hi)==MOTION_TIMING_OK);near(hi,4);
    fixed_check(r,4);assert(MotionTimingFixedBuild(&r,5,&p)==MOTION_TIMING_INFEASIBLE);
    r=(MotionTimingRequest){1,1,1,1,0,0};
    assert(MotionTimingFixedBuild(&r,1e20,&p)==MOTION_TIMING_NUMERIC_RANGE);
    for(int i=1;i<=300;++i) {
        double a=.5+(i%11)*.3,b=.7+(i%13)*.2,v=1+(i%7)*.4,v0=v*(i%5)/5,v1=v*(i%3)/3;
        double min=v1>v0?(v1*v1-v0*v0)/(2*a):(v0*v0-v1*v1)/(2*b);
        r=(MotionTimingRequest){(i%2?-1:1)*(min+.2+(i%17)*.1),v,a,b,v0,v1};
        assert(MotionTimingDurationRange(&r,&lo,&hi)==MOTION_TIMING_OK);
        fixed_check(r,lo);fixed_check(r,isfinite(hi)?lo+(hi-lo)*.4:lo*2.5);
    }
    MotionTimingScheduleRequest q={.start_time=10,.max_speed=2,.acceleration=1,.braking=1,.count=3,
        .points={{.position=0,.hold=1},{.position=10,.hold=2},{.position=0,.hold=3}}};
    MotionTimingSchedule s=build(q);
    near(s.arrivals[1],18);near(s.departures[1],20);near(s.arrivals[2],27);near(s.end_time,30);
    near(sample(&s,18).position,10);assert(sample(&s,18).stationary);near(sample(&s,18).acceleration,0);
    near(sample(&s,20).velocity,0);assert(sample(&s,20).acceleration<0);
    assert(sample(&s,27).stationary);near(sample(&s,27).acceleration,0);
    assert(sample(&s,30).stationary);
    q.points[1].speed=.5;reject(q,MOTION_CONFLICT_MOVING_HOLD);
    q.points[1].hold=0;reject(q,MOTION_CONFLICT_MOVING_REVERSAL);
    q.points[1].speed=0;q.points[1].hold=2;
    q.points[2].fixed_arrival=true;q.points[2].arrival=35;s=build(q);
    near(s.arrivals[1],18);near(s.arrivals[2],35);near(s.end_time,38);
    q.points[2].arrival=26;reject(q,MOTION_CONFLICT_ARRIVAL_EARLY);
    MotionTimingConflict conflict;
    assert(MotionTimingScheduleBuild(&q,&s,&conflict)==MOTION_TIMING_INFEASIBLE);near(conflict.earliest_arrival,27);assert(conflict.waypoint==2);
    /* Redistribute slack into earlier leg: final pass-through leg has finite
     * maximum duration, so greedy earliest-prefix scheduling would be wrong. */
    q=(MotionTimingScheduleRequest){.max_speed=2,.acceleration=1,.braking=1,.count=3,
        .points={{.position=0},{.position=10,.speed=2},{.position=11,.speed=2,.fixed_arrival=true,.arrival=20}}};
    s=build(q);near(s.arrivals[2],20);assert(s.arrivals[1]>19);
    MotionTimingScheduleSample left=sample(&s,s.arrivals[1]-1e-7),right=sample(&s,s.arrivals[1]);near(right.velocity,2);assert(fabs(left.velocity-right.velocity)<1e-6);
    q.points[1].fixed_arrival=true;q.points[1].arrival=6;q.points[2].arrival=8;reject(q,MOTION_CONFLICT_ARRIVAL_LATE);
    assert(MotionTimingScheduleBuild(&q,&s,&conflict)==MOTION_TIMING_INFEASIBLE);near(conflict.latest_arrival,6+4-2*sqrt(3));
    q.points[2].arrival=6.5;s=build(q);near(s.arrivals[1],6);near(s.arrivals[2],6.5);
    /* Duplicate rest waypoints and zero-duration legs; final hold explicit. */
    q=(MotionTimingScheduleRequest){.max_speed=2,.acceleration=1,.braking=1,.count=3,
        .points={{.position=4},{.position=4,.hold=2},{.position=4,.hold=1,.fixed_arrival=true,.arrival=5}}};
    s=build(q);near(s.end_time,6);assert(s.legs[1].stationary_seconds==3);
    for(int i=0;i<=60;++i){MotionTimingScheduleSample v=sample(&s,i*.1);near(v.position,4);near(v.velocity,0);}
    q.points[1].speed=1;reject(q,MOTION_CONFLICT_MOVING_HOLD);
    q.points[1].speed=0;q.points[1].hold=-1;reject(q,MOTION_CONFLICT_INPUT);
    q.points[1].hold=2;q.points[2].arrival=NAN;reject(q,MOTION_CONFLICT_INPUT);
    q.points[2].arrival=5;q.start_time=DBL_MAX;reject(q,MOTION_CONFLICT_ARRIVAL_EARLY);
    q=(MotionTimingScheduleRequest){.max_speed=2,.acceleration=1,.braking=1,.count=32};
    for(size_t i=0;i<32;++i)q.points[i]=(MotionTimingWaypoint){.position=(double)i,.hold=i%3==0?.1:0};
    q.points[31].fixed_arrival=true;q.points[31].arrival=100;s=build(q);near(s.arrivals[31],100);
    MotionTimingScheduleSample rows[101];
    for(int i=0;i<=100;++i)rows[i]=sample(&s,(double)i);
    for(int i=100;i>=0;--i){MotionTimingScheduleSample v=sample(&s,(double)i);near(v.position,rows[i].position);near(v.velocity,rows[i].velocity);near(v.acceleration,rows[i].acceleration);}
    MotionTimingScheduleSample result={.position=123},saved=result;
    assert(MotionTimingScheduleSampleAt(&s,-1,&result)==MOTION_TIMING_TIME_OUT_OF_RANGE);
    assert(MotionTimingScheduleSampleAt(&s,NAN,&result)==MOTION_TIMING_INVALID_INPUT);assert(!memcmp(&result,&saved,sizeof(result)));
    /* Rational frame/subframe mapping uses the existing timeline context. */
    TimelineEvaluationContext context;
    assert(TimelineEvaluationContextBuild((TimelineRate){30000,1001},(TimelineRange){500,4000},
           (TimelineSample){530,1,2},&context)==TIMELINE_STATUS_OK);
    MotionTimingScheduleSample mapped=sample(&s,context.local_time_seconds);
    near(mapped.position,sample(&s,30.5*1001/30000).position);
    /* Unit conversion scales all spatial quantities, leaving event times. */
    MotionTimingScheduleRequest scaled=q;scaled.max_speed*=3;scaled.acceleration*=3;scaled.braking*=3;
    for(size_t i=0;i<scaled.count;++i){scaled.points[i].position*=3;scaled.points[i].speed*=3;}
    MotionTimingSchedule larger=build(scaled);
    for(int i=0;i<=100;++i){MotionTimingScheduleSample v=sample(&larger,(double)i);near(v.position,3*rows[i].position);near(v.velocity,3*rows[i].velocity);}
    q.count=33;reject(q,MOTION_CONFLICT_INPUT);
    q=(MotionTimingScheduleRequest){.max_speed=2,.acceleration=1,.braking=1,.count=2,
        .points={{.position=0,.speed=2},{.position=.1,.speed=0}}};
    reject(q,MOTION_CONFLICT_ENDPOINT_SPEED);
    q.points[0].speed=0;q.points[1].position=0;s=build(q);near(s.end_time,0);assert(sample(&s,0).stationary);
    q.points[0].hold=1;q.start_time=DBL_MAX;reject(q,MOTION_CONFLICT_NUMERIC);
    q.start_time=0;q.acceleration=INFINITY;reject(q,MOTION_CONFLICT_INPUT);

    puts("M5.3 scheduling PASS: fixed-time envelopes, 300 varied cases, anchors/slack, holds/reversal/pass-through, conflicts, capacity, persistence-free seeking and atomic refusal");
}
