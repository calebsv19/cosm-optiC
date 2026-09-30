#include "motion/motion_timing_plan.h"
#include <float.h>
#include <math.h>
#include <stddef.h>

const char *MotionTimingStatusLabel(MotionTimingStatus status) {
    switch (status) {
    case MOTION_TIMING_OK: return "ok";
    case MOTION_TIMING_INVALID_INPUT: return "invalid limits or endpoint speeds";
    case MOTION_TIMING_INFEASIBLE: return "insufficient distance for endpoint speeds";
    case MOTION_TIMING_NUMERIC_RANGE: return "motion exceeds numerical resolution";
    case MOTION_TIMING_TIME_OUT_OF_RANGE: return "time outside planned interval";
    }
    return "unknown timing status";
}

static double change_distance(double low, double high, double limit) {
    return ((high - low) / limit) * (high * .5 + low * .5);
}

MotionTimingStatus MotionTimingPlanBuild(const MotionTimingRequest *r,
                                        MotionTimingPlan *out) {
    if (!r || !out || !isfinite(r->displacement) ||
        !isfinite(r->max_speed) || !isfinite(r->acceleration) ||
        !isfinite(r->braking) || !isfinite(r->start_speed) ||
        !isfinite(r->end_speed) || r->max_speed <= 0 ||
        r->acceleration <= 0 || r->braking <= 0 ||
        r->start_speed < 0 || r->end_speed < 0 ||
        r->start_speed > r->max_speed || r->end_speed > r->max_speed)
        return MOTION_TIMING_INVALID_INPUT;
    MotionTimingPlan p = {.request = *r};
    double distance = fabs(r->displacement);
    double v0 = r->start_speed, v1 = r->end_speed;
    if (distance == 0) {
        if (v0 != 0 || v1 != 0) return MOTION_TIMING_INFEASIBLE;
        *out = p;
        return MOTION_TIMING_OK;
    }
    double minimum = v1 > v0 ? change_distance(v0, v1, r->acceleration)
                             : change_distance(v1, v0, r->braking);
    if (!isfinite(minimum)) return MOTION_TIMING_NUMERIC_RANGE;
    if (distance < minimum) return MOTION_TIMING_INFEASIBLE;

    double up = change_distance(v0, r->max_speed, r->acceleration);
    double down = change_distance(v1, r->max_speed, r->braking);
    if (!isfinite(up) || !isfinite(down) || !isfinite(up + down))
        return MOTION_TIMING_NUMERIC_RANGE;
    double peak = r->max_speed;
    int triangular = up + down > distance;
    if (triangular) {
        /* Scale velocities before squaring, avoiding overflow in v*v.
         * Reciprocal acceleration weights keep the solve symmetric. */
        double scale = r->max_speed;
        double sum = r->acceleration + r->braking;
        if (!isfinite(sum)) return MOTION_TIMING_NUMERIC_RANGE;
        double wa = r->braking / sum, wb = r->acceleration / sum;
        double base = wa * (v0 / scale) * (v0 / scale) +
                      wb * (v1 / scale) * (v1 / scale);
        double gain = (distance / scale) * (2 * wa * (r->acceleration / scale));
        peak = scale * sqrt(base + gain);
        if (!isfinite(peak) || peak <= 0) return MOTION_TIMING_NUMERIC_RANGE;
        /* Only absorb roundoff at the already-validated endpoint bounds. */
        peak = fmin(r->max_speed, fmax(fmax(v0, v1), peak));
        up = change_distance(v0, peak, r->acceleration);
        down = change_distance(v1, peak, r->braking);
    }
    double coast = distance - up - down;
    double tolerance = 128 * DBL_EPSILON * distance;
    if (coast < -tolerance) return MOTION_TIMING_NUMERIC_RANGE;
    if (triangular && fabs(coast) > tolerance) return MOTION_TIMING_NUMERIC_RANGE;
    coast = triangular ? 0 : fmax(0, coast);
    p.peak_speed = peak;
    p.accelerate_time = (peak - v0) / r->acceleration;
    p.cruise_time = coast / peak;
    p.brake_time = (peak - v1) / r->braking;
    p.accelerate_distance = up;
    p.cruise_distance = coast;
    p.duration = p.accelerate_time + p.cruise_time + p.brake_time;
    if (!isfinite(p.duration) || p.duration <= 0 ||
        (peak > v0 && p.accelerate_time == 0) ||
        (peak > v1 && p.brake_time == 0) ||
        !isfinite(up + coast + down) ||
        fabs(up + coast + down - distance) > tolerance ||
        (coast > 0 && p.cruise_time == 0) ||
        (p.brake_time > 0 && p.duration == p.accelerate_time + p.cruise_time) ||
        (p.cruise_time > 0 && p.accelerate_time + p.cruise_time == p.accelerate_time))
        return MOTION_TIMING_NUMERIC_RANGE;
    *out = p;
    return MOTION_TIMING_OK;
}

MotionTimingStatus MotionTimingPlanSample(const MotionTimingPlan *p, double t,
                                         MotionTimingSample *out) {
    if (!p || !out || !isfinite(t) || !isfinite(p->duration) || p->duration < 0)
        return MOTION_TIMING_INVALID_INPUT;
    if (t < 0 || t > p->duration) return MOTION_TIMING_TIME_OUT_OF_RANGE;
    MotionTimingSample s = {0};
    double direction = p->request.displacement < 0 ? -1 : 1;
    if (p->duration == 0) { *out = s; return MOTION_TIMING_OK; }
    if (t < p->accelerate_time) {
        s.velocity = p->request.start_speed + p->request.acceleration * t;
        s.displacement = t * (p->request.start_speed * .5 + s.velocity * .5);
        s.acceleration = p->request.acceleration;
    } else if (t < p->accelerate_time + p->cruise_time) {
        s.velocity = p->peak_speed;
        s.displacement = p->accelerate_distance + p->peak_speed * (t - p->accelerate_time);
    } else if (p->brake_time > 0) {
        /* Evaluate backward from the end for precision near arrival. */
        double remaining = p->duration - t;
        s.velocity = p->request.end_speed + p->request.braking * remaining;
        s.displacement = fabs(p->request.displacement) -
                         remaining * (p->request.end_speed * .5 + s.velocity * .5);
        s.acceleration = -p->request.braking;
    } else {
        s.displacement = fabs(p->request.displacement);
        s.velocity = p->request.end_speed;
        s.acceleration = p->cruise_time > 0 ? 0 : p->request.acceleration;
    }
    s.displacement *= direction;
    s.velocity *= direction;
    s.acceleration *= direction;
    if (!isfinite(s.displacement) || !isfinite(s.velocity) || !isfinite(s.acceleration))
        return MOTION_TIMING_NUMERIC_RANGE;
    *out = s;
    return MOTION_TIMING_OK;
}
