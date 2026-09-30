#include "motion/motion_timing_schedule.h"
#include <float.h>
#include <math.h>

const char *MotionTimingConflictLabel(MotionTimingConflictKind k) {
    switch(k) {
    case MOTION_CONFLICT_NONE:return "ok";
    case MOTION_CONFLICT_INPUT:return "invalid waypoint or limits";
    case MOTION_CONFLICT_MOVING_HOLD:return "hold requires zero waypoint speed";
    case MOTION_CONFLICT_MOVING_REVERSAL:return "reversal requires zero waypoint speed";
    case MOTION_CONFLICT_ENDPOINT_SPEED:return "insufficient distance for waypoint speeds";
    case MOTION_CONFLICT_ARRIVAL_EARLY:return "arrival too early; allow more time or explicitly revise limits/speeds";
    case MOTION_CONFLICT_ARRIVAL_LATE:return "arrival too late; use an earlier time or explicitly revise waypoint speeds/stops";
    case MOTION_CONFLICT_NUMERIC:return "schedule exceeds numerical resolution";
    }
    return "unknown conflict";
}
static MotionTimingStatus fail(MotionTimingConflict *c,MotionTimingConflictKind kind,size_t point,MotionTimingStatus status) {
    if(c){*c=(MotionTimingConflict){.kind=kind,.waypoint=point};}
    return status;
}
MotionTimingStatus MotionTimingScheduleBuild(const MotionTimingScheduleRequest *r,MotionTimingSchedule *out,MotionTimingConflict *conflict) {
    if(!r||!out||r->count<2||r->count>MOTION_TIMING_WAYPOINT_CAPACITY||!isfinite(r->start_time)||
       !isfinite(r->max_speed)||!isfinite(r->acceleration)||!isfinite(r->braking)||
       r->max_speed<=0||r->acceleration<=0||r->braking<=0)
        return fail(conflict,MOTION_CONFLICT_INPUT,0,MOTION_TIMING_INVALID_INPUT);
    MotionTimingSchedule s={.request=*r};
    double durations[31]={0},maximum[31]={0};MotionTimingRequest legs[31];
    for(size_t i=0;i<r->count;++i) {
        const MotionTimingWaypoint *w=&r->points[i];
        if(!isfinite(w->position)||!isfinite(w->speed)||!isfinite(w->hold)||w->speed<0||w->speed>r->max_speed||w->hold<0||
           (w->fixed_arrival&&!isfinite(w->arrival))||(i==0&&w->fixed_arrival&&w->arrival!=r->start_time))
            return fail(conflict,MOTION_CONFLICT_INPUT,i,MOTION_TIMING_INVALID_INPUT);
        if(w->hold>0&&w->speed!=0)return fail(conflict,MOTION_CONFLICT_MOVING_HOLD,i,MOTION_TIMING_INFEASIBLE);
        if(i&&i+1<r->count&&w->speed>0) {
            double left=w->position-r->points[i-1].position,right=r->points[i+1].position-w->position;
            if((left<0&&right>0)||(left>0&&right<0))
                return fail(conflict,MOTION_CONFLICT_MOVING_REVERSAL,i,MOTION_TIMING_INFEASIBLE);
        }
        if(i) {
            legs[i-1]=(MotionTimingRequest){w->position-r->points[i-1].position,r->max_speed,r->acceleration,r->braking,r->points[i-1].speed,w->speed};
            MotionTimingStatus status=MotionTimingDurationRange(&legs[i-1],&durations[i-1],&maximum[i-1]);
            if(status!=MOTION_TIMING_OK)return fail(conflict,status==MOTION_TIMING_INFEASIBLE?MOTION_CONFLICT_ENDPOINT_SPEED:MOTION_CONFLICT_NUMERIC,i,status);
        }
    }
    size_t anchor=0;double anchor_time=r->start_time;
    for(size_t i=1;i<r->count;++i)if(r->points[i].fixed_arrival) {
        double earliest=anchor_time,latest=anchor_time;
        for(size_t j=anchor;j<i;++j){earliest+=r->points[j].hold+durations[j];latest+=r->points[j].hold+maximum[j];}
        double requested=r->points[i].arrival;
        if(!isfinite(earliest))return fail(conflict,MOTION_CONFLICT_NUMERIC,i,MOTION_TIMING_NUMERIC_RANGE);
        if(requested<earliest||requested>latest) {
            if(conflict)*conflict=(MotionTimingConflict){requested<earliest?MOTION_CONFLICT_ARRIVAL_EARLY:MOTION_CONFLICT_ARRIVAL_LATE,i,requested,earliest,latest};
            return MOTION_TIMING_INFEASIBLE;
        }
        double slack=requested-earliest;
        for(size_t j=i;j>anchor&&slack>0;) {
            --j;double extra=fmin(slack,maximum[j]-durations[j]);
            durations[j]+=extra;slack-=extra;
        }
        if(slack>64*DBL_EPSILON*fmax(1,fabs(requested)))return fail(conflict,MOTION_CONFLICT_NUMERIC,i,MOTION_TIMING_NUMERIC_RANGE);
        anchor=i;anchor_time=requested;
    }
    s.arrivals[0]=r->start_time;
    for(size_t i=0;i<r->count;++i) {
        if(i&&r->points[i].fixed_arrival) {
            double expected=r->points[i].arrival;
            if(fabs(s.arrivals[i]-expected)>128*DBL_EPSILON*fmax(1,fabs(expected)))return fail(conflict,MOTION_CONFLICT_NUMERIC,i,MOTION_TIMING_NUMERIC_RANGE);
            s.arrivals[i]=expected;
        }
        s.departures[i]=s.arrivals[i]+r->points[i].hold;
        if(!isfinite(s.departures[i])||(r->points[i].hold>0&&s.departures[i]==s.arrivals[i]))return fail(conflict,MOTION_CONFLICT_NUMERIC,i,MOTION_TIMING_NUMERIC_RANGE);
        if(i+1<r->count) {
            s.arrivals[i+1]=s.departures[i]+durations[i];
            if(!isfinite(s.arrivals[i+1])||(durations[i]>0&&s.arrivals[i+1]==s.departures[i]))return fail(conflict,MOTION_CONFLICT_NUMERIC,i+1,MOTION_TIMING_NUMERIC_RANGE);
            MotionTimingStatus status=MotionTimingFixedBuild(&legs[i],durations[i],&s.legs[i]);
            if(status!=MOTION_TIMING_OK)return fail(conflict,MOTION_CONFLICT_NUMERIC,i+1,status);
        }
    }
    s.end_time=s.departures[r->count-1];*out=s;
    if(conflict)*conflict=(MotionTimingConflict){0};return MOTION_TIMING_OK;
}
MotionTimingStatus MotionTimingScheduleSampleAt(const MotionTimingSchedule *s,double t,MotionTimingScheduleSample *out) {
    if(!s||!out||!isfinite(t)||s->request.count<2||s->request.count>32)return MOTION_TIMING_INVALID_INPUT;
    if(t<s->arrivals[0]||t>s->end_time)return MOTION_TIMING_TIME_OUT_OF_RANGE;
    MotionTimingScheduleSample result={0};size_t n=s->request.count;
    for(size_t i=0;i<n;++i) {
        if(t<s->departures[i] || (i==n-1&&s->request.points[i].hold>0)) {
            result.position=s->request.points[i].position;result.waypoint=i;result.stationary=true;*out=result;return MOTION_TIMING_OK;
        }
        if(i+1<n&&(t<s->arrivals[i+1]||(i+2==n&&s->request.points[i+1].hold==0&&t==s->arrivals[i+1]))) {
            MotionTimingSample v;double local=t-s->departures[i];
            /* Subtraction of the absolute origin can differ by a few ulps. */
            local=fmax(0,fmin(s->legs[i].duration,local));
            MotionTimingStatus status=MotionTimingFixedSample(&s->legs[i],local,&v);
            if(status!=MOTION_TIMING_OK)return status;
            result=(MotionTimingScheduleSample){s->request.points[i].position+v.displacement,v.velocity,v.acceleration,i,v.velocity==0&&v.acceleration==0};
            if(!isfinite(result.position))return MOTION_TIMING_NUMERIC_RANGE;
            *out=result;return MOTION_TIMING_OK;
        }
    }
    result.position=s->request.points[n-1].position;result.waypoint=n-1;result.stationary=true;*out=result;return MOTION_TIMING_OK;
}
