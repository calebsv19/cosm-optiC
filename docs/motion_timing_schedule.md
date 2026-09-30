# Waypoint scheduling (M5.3)

This API extends the [straight-route timing foundation](motion_timing_planner.md).
It is consumed by the [curved-route adapter](motion_route_planning.md) and
[retained planner](motion_plans.md). Existing keys are preserved by explicit
Apply/Restore; no scene is automatically retimed.

## Inputs and ownership

`MotionTimingScheduleBuild` accepts 2–32 waypoints along one signed straight-axis
world coordinate. Each has a position, a nonnegative speed magnitude, a hold
length in seconds, and an optional fixed arrival time. The request specifies
start time and positive global speed, acceleration and braking limits.

The first waypoint arrives at start time; if its arrival is fixed, it must match
that time. Holds require zero speed. Direction changes at a waypoint require
zero speed. Same-direction pass-through preserves the requested nonzero speed.
Coincident positions require rest on both ends. Speeds are hard requirements,
not suggestions for the scheduler to change. Departure is arrival plus the
specified hold; the final hold is included in schedule end time.

An unanchored suffix travels in minimum time. Between fixed arrivals, all
requested holds are retained and remaining travel time is distributed within
each leg's feasible duration interval, starting with later legs. This is a
deterministic allocation policy, not an optimization of smoothness or energy.
It can slow an earlier leg when a later pass-through leg has only limited slack.
Multiple fixed arrivals divide the problem into independently bounded blocks.

## Fixed-duration solution and feasibility

For each leg the fixed-time solver constructs upper and lower admissible speed
envelopes from the endpoint speeds, speed cap, and acceleration/braking bounds.
The area under a speed curve is travelled distance. A convex combination of the
envelopes gives the requested distance at the requested time while retaining
endpoint speeds and limits. Slopes are obtained from the active analytic lines,
not from subtraction of nearly equal sampled speeds.

A leg's shortest duration comes from the M5.2 planner. Its longest duration is
finite when the distance cannot accommodate arbitrarily slow passage at the
specified endpoint speeds. Zero-speed travel at an instant is permitted; a
nonzero leg never inserts an unrequested stationary interval. In particular,
arriving early and waiting at the destination is not accepted as a later fixed
arrival. Request an explicit hold or change the timing/speed requirements.
A zero-distance leg is stationary throughout and exposes that time in readback.

Too-early and too-late fixed anchors report the offending waypoint, requested
arrival, earliest possible arrival and latest possible arrival (possibly
infinite), relative to the preceding fixed anchor. Conflict labels suggest more
time or explicit changes to limits/speeds/stops. Other conflicts identify invalid
input, moving holds, moving reversals, impossible endpoint-speed transitions,
or numerical-range failure. No suggestion is applied automatically. Failed
builds leave the output schedule byte-for-byte unchanged; optional conflict
readback is the only output modified on failure.

## Sampling and numerical scope

Successful schedules expose arrival/departure times and per-leg velocity phases.
`MotionTimingScheduleSampleAt` returns world position, signed velocity,
acceleration, waypoint association and stationary status at any time in the
closed schedule interval. Holds return zero velocity/acceleration. Internal
boundaries are right-sided; a final moving arrival is left-sided when there is
no final hold. Zero-duration waypoints are skipped in favor of the next event at
that time. Out-of-range or nonfinite samples fail without modifying output.

There is no playback accumulator, new clock, allocation, scene mutation, or new
position owner. Frame/subframe conversion uses the existing timeline context.
Unit conversion scales all spatial quantities together; fixed absolute times
and prescribed holds stay unchanged. Arbitrary and reverse seeks are reproducible
within one build; cross-platform bitwise identity is not promised.

Numerics use double precision. Normalized envelope breakpoints within
`32 * DBL_EPSILON` are merged in the interior; unresolved endpoint phases are
refused; integrated distance is checked within
`256 * DBL_EPSILON * max(distance, lower-envelope area)`. Absolute event-time
reconstruction uses a `128 * DBL_EPSILON * max(1, abs(time))` tolerance.
Unrepresentable durations, unresolved phases, overflow or failed reconstruction
are refused. Floating-point tolerances are not permission to relax user limits.
Position/velocity are continuous to numerical precision; acceleration may jump.
These are straight-line tangential limits only: no curved-route, jerk, angular
acceleration, collision or force guarantee is introduced.

## Verification

```sh
make TOOLCHAIN=clang test-motion-timing-schedule
make TOOLCHAIN=clang test-motion-timing-schedule-sanitize
make TOOLCHAIN=clang test-motion-timing-plan test-motion-timing-plan-sanitize
make TOOLCHAIN=clang test-scene-timeline-entity-contract test-scene-editor-timeline-view
```

Tests cover exact analytic duration ranges, early/late refusals, 300 varied
fixed-time profiles, analytic phase limits and integrated distance, continuity,
multiple anchors, redistribution into earlier legs, pass-through, stop/hold/
departure, zero-speed reversal, forbidden moving reversal, zero-distance legs,
capacity, arbitrary/reverse seeking, rational subframes, unit conversion,
numerical refusal and unchanged failure outputs. ASan/UBSan and the M5.2
numerical regression are included. No live editor/render acceptance is claimed
for an API without a live consumer.
