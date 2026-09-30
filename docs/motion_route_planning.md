# Curved-route planning (M5.4)

This numerical adapter extends the [waypoint scheduler](motion_timing_schedule.md)
to the existing reusable XYZ paths. The [retained planner](motion_plans.md) now integrates it explicitly with scene
evaluation, saved intent and editor controls.

`MotionRouteGeometryBuild` makes a world-scaled, immutable derived analysis.
Adaptive de Casteljau subdivision bounds each cubic's arc length between its
chord and control-polygon lengths. The sum of leaf errors is retained in readback;
subdivision requests a polygon-relative gap of 1e-6 per segment and is bounded by
24 levels / 32768 leaves. Five-point Gauss integration supplies an estimate within
that bracket. Sampling refines the inverse distance within its leaf rather than
using linear parameter interpolation. Its inverse tolerance is 1e-13 of route
length. This is floating-point numerical evidence, not interval arithmetic or
an exact-real arithmetic guarantee. Capacity/unresolved geometry fails explicitly.
The legacy runtime's existing distance sampler is not changed.

Derivative control bounds yield conservative curvature bounds on regular leaves.
At a curved zero-handle endpoint, curvature can diverge. A separate bound on
curvature times distance from a required rest point handles that case using
v^2 <= 2*a*distance. Interior cusps which cannot be covered by an authored rest
point are refused for planning; split or reshape the route explicitly. Collinear
monotone eased segments remain supported, including zero endpoint handles.

`MotionRouteScheduleBuild` takes waypoint positions as world arc distances.
Every traversed corner and singular rest endpoint must be an explicit zero-speed
waypoint. It does not insert stops silently. Smooth linked tangent directions
permit pass-through even when handle lengths differ. Partial traversals near a
singular endpoint must also satisfy that endpoint's speed envelope.

The adapter retains requested limits and exposes conservative effective limits.
Curved routes divide acceleration budget between tangential and normal components
and apply a 1 percent numerical margin. Regular curvature imposes a speed cap;
rest-origin curvature bounds may additionally reduce acceleration/braking. Their
vector sum fits the respective requested acceleration and braking limits. A
prescribed waypoint speed above the effective cap is refused, not silently
changed. The existing scheduler then verifies fixed arrival feasibility under
these effective limits. This policy is conservative, not a time-optimal curved
path solver. It guarantees neither continuous acceleration nor bounded jerk.

`MotionRouteScheduleSample` returns XYZ position, velocity and acceleration from
that timeline and the derived route. It includes turning acceleration and handles
reverse travel and the appropriate side of a corner at departure/arrival.
A singular endpoint is sampled only at rest. No second position owner, simulation
loop, mass, force, collision or angular-acceleration behavior is introduced.

Verification commands:

```sh
make TOOLCHAIN=clang test-motion-route-geometry test-motion-route-geometry-sanitize
make TOOLCHAIN=clang test-motion-route-schedule test-motion-route-schedule-sanitize
make TOOLCHAIN=clang test-motion-timing-schedule test-motion-timing-plan
```

Proof includes independent high-resolution length integration, sampled curvature
bounds, scale/translation, straight/corner/reversal paths, singular endpoint and
interior-cusp classification, explicit stop refusal, combined acceleration
budgets, position/velocity finite differences, acceleration finite differences
away from temporal discontinuities, partial-traversal speed refusal, unequal
linked handles, asymmetric limits and ASan/UBSan. Native/headless compilation also
passes. Live consumer, retained UI, persistence and final rendering acceptance are
covered by the M5.5–M5.6 [retained planner](motion_plans.md) tests.
