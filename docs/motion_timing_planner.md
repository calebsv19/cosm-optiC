# Detached straight-route timing (M5.1–M5.2)

This is an engineering API, not an enabled editor feature. The straight-route
planner is detached from scene persistence and evaluation. M1–M4 scene behavior,
timing keys, follower bindings and direct-XYZ trails remain unchanged.

M5.3 fixed-arrival/hold/departure scheduling now extends this API; see
[waypoint scheduling](motion_timing_schedule.md). The contract below remains the
minimum-duration single-leg foundation.

## Contract

`motion/motion_timing_plan.h` builds minimum-duration monotone motion along a
straight route. The caller supplies signed displacement in world units, maximum
speed in world units/second, positive acceleration and braking limits in world
units/second squared, and nonnegative start/end speed magnitudes along travel.
Negative displacement reverses signed output velocity and acceleration. An
internal reversal is not part of one move. A later waypoint scheduler must
join such moves with an explicit zero-speed reversal.

All inputs must be finite; limits must be strictly positive and endpoint speeds
must not exceed the maximum. Zero displacement is permitted only at rest and
produces a zero-duration stationary plan. Insufficient distance to change the
endpoint speed returns `MOTION_TIMING_INFEASIBLE`; nothing is clamped to make an
impossible request appear successful. NaN, infinity and invalid limits are
invalid input. Intermediate overflow, underflow/loss of resolvable phases, or
distance reconstruction error returns `MOTION_TIMING_NUMERIC_RANGE`.

The profile has up to three constant-acceleration phases: accelerate, cruise,
brake. Short moves use a lower peak speed without cruising. Existing scene
objects are not read or modified. Storage is fixed size; there is no allocation,
new clock, iterative playback state, or position ownership change.

`MotionTimingPlanSample` takes an unmodified successful plan and seconds in
the closed interval `[0, duration]`. It returns signed displacement from zero,
velocity and acceleration. Samples outside the interval are rejected rather
than silently extending nonzero endpoint velocities into a stationary hold.
At internal phase boundaries acceleration is right-sided; at the final endpoint
it is left-sided. Position and velocity are continuous within floating-point
tolerance; acceleration may jump. There is no jerk or continuous-acceleration
guarantee. The zero-duration sample has zero displacement/velocity/acceleration.
Every failed build or sample leaves its output unchanged.

The limits describe acceleration along a route. On a straight line this is the
entire spatial acceleration. Curved routes also require turning acceleration;
this API does not prove limits on curved geometry. No claim is made about
collisions, force, mass, orientation, or camera angular acceleration.

## Numerical and timeline boundaries

The planner solves in double precision, checks integrated distance against
`128 * DBL_EPSILON * abs(displacement)`, and rejects unresolvable phase timing.
It is not interval arithmetic or a cross-platform bitwise reproducibility
promise. Tests use an independent bisection peak-speed oracle, known analytic
cases, integrated distance, phase-boundary continuity and scale-aware derivative
checks. Finite-difference test tolerances account for subtraction cancellation;
direct speed/acceleration checks are separate.

Seconds come from the existing `TimelineEvaluationContext.local_time_seconds`,
relative to the eventual move origin. No frames are rounded or keys emitted by
this API. Fractional frames and rational rates (including 30000/1001) are tested.
Changing a scene's geometric scale with fixed world limits changes the travel
time. Converting all distances, velocities and accelerations by the same unit
factor preserves duration. Caller-side route projection is future integration.

## Compatibility rules for later integration

- No load-time planning, automatic retiming, or schema fields are introduced in
  this slice. Missing future planning metadata must keep legacy authored timing.
- Applying a future plan must be explicit, revision-checked, atomic and undoable,
  preserving prior timing and the existing single position owner. It must not
  silently convert direct XYZ animation or alter reusable route geometry.
- Future saved planning data must carry versioned intent and source dependencies.
  Unsupported versions/policies must fail visibly, never fall back to a falsely
  valid limits claim. Loading must not silently regenerate or change motion.
- Geometry, world scale, timeline rate or timing edits must invalidate or
  explicitly revalidate planned-limit status. Baked curves need verification of
  their actual evaluated motion, not just the original planner profile.
- Fixed arrival times and intermediate waypoints/holds are implemented in the
  detached M5.3 scheduler. Curves/corners, saved plan schema and UI apply/replan
  behavior remain subsequent M5 slices.

## Verification

From the repository root:

```sh
make TOOLCHAIN=clang test-motion-timing-plan
make TOOLCHAIN=clang test-motion-timing-plan-sanitize
make TOOLCHAIN=clang test-scene-timeline-entity-contract test-scene-editor-timeline-view
```

The numerical gate covers analytic triangular/trapezoidal/asymmetric profiles,
accelerate-only, brake-only, cruise-only, rest, nonzero endpoint speeds, reverse
travel, 400 generated cases, arbitrary/reverse seeking, time-reversal symmetry,
unit conversion and world-distance changes, invalid input and atomic refusal.
Sanitizers cover address and undefined behavior errors. There is no renderer or
native UI acceptance claim for an API not yet used by those consumers.
