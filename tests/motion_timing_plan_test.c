#include "motion/motion_timing_plan.h"
#include "animation/timeline_clock.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void near(double a, double b) {
    if (fabs(a-b)>2e-10*fmax(1,fmax(fabs(a),fabs(b)))) fprintf(stderr,"near mismatch %.17g vs %.17g\n",a,b);
    assert(fabs(a - b) <= 2e-10 * fmax(1, fmax(fabs(a), fabs(b))));
}
static MotionTimingPlan build(MotionTimingRequest r) {
    MotionTimingPlan p;
    MotionTimingStatus status = MotionTimingPlanBuild(&r, &p);
    if (status != MOTION_TIMING_OK)
        fprintf(stderr, "Unexpected %s: d=%g vmax=%g a=%g b=%g v0=%g v1=%g\n",
                MotionTimingStatusLabel(status), r.displacement, r.max_speed,
                r.acceleration, r.braking, r.start_speed, r.end_speed);
    assert(status == MOTION_TIMING_OK);
    return p;
}
static MotionTimingSample sample(const MotionTimingPlan *p, double t) {
    MotionTimingSample s;
    assert(MotionTimingPlanSample(p, t, &s) == MOTION_TIMING_OK);
    return s;
}
static void verify(MotionTimingRequest r) {
    MotionTimingPlan p = build(r), again = build(r);
    assert(!memcmp(&p, &again, sizeof(p)));
    double direction = r.displacement < 0 ? -1 : 1;
    MotionTimingSample start = sample(&p, 0), end = sample(&p, p.duration);
    near(start.displacement, 0); near(end.displacement, r.displacement);
    near(start.velocity, direction * r.start_speed);
    near(end.velocity, direction * r.end_speed);
    if (p.duration == 0) return;
    /* Independent monotone search for the highest reachable peak. This
     * avoids using the production closed-form triangular solve as oracle. */
    double lo = fmax(r.start_speed,r.end_speed), hi = r.max_speed;
    for (int iteration=0; iteration<80; ++iteration) {
        double mid = (lo+hi)*.5;
        double required = (mid*mid-r.start_speed*r.start_speed)/(2*r.acceleration) +
                          (mid*mid-r.end_speed*r.end_speed)/(2*r.braking);
        if (required > fabs(r.displacement)) hi=mid; else lo=mid;
    }
    near(p.peak_speed,lo);
    /* Integrate each phase independently: velocity area equals route length. */
    near((r.start_speed + p.peak_speed) * .5 * p.accelerate_time +
         p.peak_speed * p.cruise_time +
         (r.end_speed + p.peak_speed) * .5 * p.brake_time, fabs(r.displacement));
    const double edges[] = {0, p.accelerate_time,
                           p.accelerate_time + p.cruise_time, p.duration};
    for (int k = 1; k < 3; ++k) {
        double t = edges[k];
        if (t <= 0 || t >= p.duration) continue;
        double dt = fmin(t, p.duration - t) * 1e-8;
        MotionTimingSample l = sample(&p, t - dt), h = sample(&p, t + dt);
        assert(fabs(h.velocity - l.velocity) <=
               2.1 * dt * fmax(r.acceleration, r.braking) + 1e-10);
        assert(fabs(h.displacement - l.displacement) <= 2.1 * dt * r.max_speed + 1e-10);
    }
    MotionTimingSample saved[101];
    for (int i = 0; i <= 100; ++i) {
        double t = p.duration * (i / 100.0);
        saved[i] = sample(&p, t);
        double v = direction * saved[i].velocity, a = direction * saved[i].acceleration;
        assert(v >= -1e-10 && v <= r.max_speed * (1 + 1e-12));
        assert(a <= r.acceleration && a >= -r.braking);
        if (i) assert(direction * (saved[i].displacement - saved[i-1].displacement) >= -1e-10);
    }
    /* A coprime permutation covers all saved times, including reverse jumps. */
    for (int j = 0; j < 101; ++j) {
        int i = (100 - j * 37 % 101);
        MotionTimingSample s = sample(&p, p.duration * (i / 100.0));
        assert(!memcmp(&s, &saved[i], sizeof(s)));
    }
    /* Derivative checks strictly inside each nonempty phase. */
    for (int k = 0; k < 3; ++k) {
        double span = edges[k+1] - edges[k];
        if (span <= p.duration * 1e-10) continue;
        double t = (edges[k] + edges[k+1]) * .5, dt = span * 1e-4;
        MotionTimingSample l = sample(&p,t-dt), m = sample(&p,t), h = sample(&p,t+dt);
        /* Differencing large positions over short spans loses precision.
         * Bound that cancellation explicitly; direct limit tests stay strict. */
        double vp = (h.displacement-l.displacement)/(2*dt);
        double ap = (h.velocity-l.velocity)/(2*dt);
        double position_error = 32*DBL_EPSILON*fmax(fabs(h.displacement),fabs(l.displacement))/dt;
        double velocity_error = 32*DBL_EPSILON*fmax(fabs(h.velocity),fabs(l.velocity))/dt;
        assert(fabs(vp-m.velocity) <= position_error+1e-9*fmax(1,fabs(m.velocity)));
        assert(fabs(ap-m.acceleration) <= velocity_error+1e-9*fmax(1,fabs(m.acceleration)));
    }
}
static void refusal(MotionTimingRequest r, MotionTimingStatus expected) {
    MotionTimingPlan p, before;
    memset(&p, 0x5a, sizeof(p)); before = p;
    assert(MotionTimingPlanBuild(&r, &p) == expected);
    assert(!memcmp(&p, &before, sizeof(p)));
}
int main(void) {
    MotionTimingRequest r = {10, 2, 1, 1, 0, 0};
    MotionTimingPlan p = build(r);
    near(p.duration, 7); near(p.accelerate_time, 2); near(p.cruise_time, 3);
    near(sample(&p, 1).displacement, .5); near(sample(&p, 4).displacement, 6);
    near(sample(&p, 6).displacement, 9.5);
    r.displacement = 1; p = build(r); near(p.duration, 2); near(p.peak_speed, 1);
    r = (MotionTimingRequest){6, 3, 3, 1, 0, 0}; p = build(r);
    near(p.duration, 4); near(p.peak_speed, 3); near(p.cruise_time, 0);
    r = (MotionTimingRequest){8, 4, 2, 2, 4, 0}; p = build(r);
    near(p.accelerate_time, 0); near(p.cruise_time, 1); near(p.brake_time, 2);
    verify((MotionTimingRequest){0, 1, 1, 1, 0, 0});
    verify((MotionTimingRequest){4, 4, 2, 2, 0, 4}); /* accelerate only */
    verify((MotionTimingRequest){-4, 4, 2, 2, 4, 0}); /* brake only */
    verify((MotionTimingRequest){4, 2, 1, 1, 2, 2}); /* cruise only */
    verify((MotionTimingRequest){1e-6, .01, .001, .002, 0, 0});
    verify((MotionTimingRequest){1e6, 1e3, 1e2, 2e2, 0, 0});
    for (int i = 1; i <= 400; ++i) {
        double a = .2 + (i % 17) * .3, b = .3 + (i % 13) * .4;
        double vmax = 1 + (i % 23) * .4;
        double v0 = vmax * (i % 7) / 7, v1 = vmax * (i % 11) / 11;
        double minimum = v1 > v0 ? (v1*v1-v0*v0)/(2*a) : (v0*v0-v1*v1)/(2*b);
        verify((MotionTimingRequest){(i%2?-1:1)*(minimum+.01+(i%31)*.7),vmax,a,b,v0,v1});
    }
    r = (MotionTimingRequest){10, 2, 1, 1, 0, 0}; p = build(r);
    /* Existing rational timeline context provides seconds, not a new clock. */
    TimelineEvaluationContext c;
    assert(TimelineEvaluationContextBuild((TimelineRate){24,1}, (TimelineRange){100,241},
           (TimelineSample){124,1,2}, &c) == TIMELINE_STATUS_OK);
    near(sample(&p,c.local_time_seconds).displacement, .5*pow(24.5/24,2));
    assert(TimelineEvaluationContextBuild((TimelineRate){30000,1001}, (TimelineRange){100,241},
           (TimelineSample){130,0,1}, &c) == TIMELINE_STATUS_OK);
    near(sample(&p,c.local_time_seconds).displacement, .5*1.001*1.001);
    MotionTimingRequest scaled = r;
    scaled.displacement *= 3; scaled.max_speed *= 3; scaled.acceleration *= 3; scaled.braking *= 3;
    MotionTimingPlan q = build(scaled); near(p.duration,q.duration);
    near(sample(&q,1.25).displacement,3*sample(&p,1.25).displacement);
    scaled = r; scaled.displacement *= 3; q = build(scaled);
    assert(q.duration > p.duration); /* Changing geometry alone is not unit conversion. */
    /* Reverse traversal and swap acceleration/braking plus endpoint speeds:
     * the resulting trajectory is the time reversal of the original. */
    r = (MotionTimingRequest){12,4,2,3,.5,1.25}; p=build(r);
    scaled=(MotionTimingRequest){-12,4,3,2,1.25,.5};q=build(scaled);
    near(p.duration,q.duration);
    for(int i=0;i<=100;++i) {
        double t=p.duration*(i/100.0);
        MotionTimingSample f=sample(&p,t), rev=sample(&q,q.duration*(1-i/100.0));
        near(f.displacement-12,rev.displacement);near(f.velocity,-rev.velocity);
    }
    refusal((MotionTimingRequest){1,4,1,1,4,0},MOTION_TIMING_INFEASIBLE);
    refusal((MotionTimingRequest){0,4,1,1,1,1},MOTION_TIMING_INFEASIBLE);
    refusal((MotionTimingRequest){1,2,1,1,3,0},MOTION_TIMING_INVALID_INPUT);
    refusal((MotionTimingRequest){1,2,0,1,0,0},MOTION_TIMING_INVALID_INPUT);
    refusal((MotionTimingRequest){NAN,2,1,1,0,0},MOTION_TIMING_INVALID_INPUT);
    refusal((MotionTimingRequest){1,INFINITY,1,1,0,0},MOTION_TIMING_INVALID_INPUT);
    refusal((MotionTimingRequest){1,2,1,1,-1,0},MOTION_TIMING_INVALID_INPUT);
    refusal((MotionTimingRequest){DBL_MAX,DBL_MAX,DBL_MIN,1,0,0},MOTION_TIMING_NUMERIC_RANGE);
    refusal((MotionTimingRequest){DBL_MIN,DBL_MIN,DBL_MAX,DBL_MAX,0,0},MOTION_TIMING_NUMERIC_RANGE);
    MotionTimingSample s = {1,2,3}, before = s;
    assert(MotionTimingPlanSample(&p,-1,&s)==MOTION_TIMING_TIME_OUT_OF_RANGE);
    assert(MotionTimingPlanSample(&p,p.duration+1,&s)==MOTION_TIMING_TIME_OUT_OF_RANGE);
    assert(MotionTimingPlanSample(&p,NAN,&s)==MOTION_TIMING_INVALID_INPUT);
    assert(!memcmp(&s,&before,sizeof(s)));
    assert(MotionTimingPlanBuild(NULL,&p)==MOTION_TIMING_INVALID_INPUT);
    puts("M5 straight timing PASS: analytic profiles, 400 asymmetric cases, limits, continuity, seeking, rational frame time, scale, atomic refusal");
}
