# Planned route timing

In the Render workspace, open **Paths**, select a route, and choose **Plan
movement limits** in the inspector. Attach a follower first. The follower button
cycles through that route's attached objects, camera and light.

Set maximum speed in world units/second and acceleration/braking in world
units/second squared. Waypoints use normalized arc progress (0 to 1), speed
magnitude, a hold duration, and either earliest-feasible or fixed arrival time.
All times are local seconds from the timeline range's first frame. Changing the
frame rate changes that relationship, so an applied plan locks its clock.

**Preview feasibility** reports effective limits and arrival/departure times
without changing the scene. Curvature can lower the usable speed and tangential
acceleration below requested limits. A fixed arrival that cannot be met is
refused with a conflict; adjust time, limits or route explicitly. The full plan
must fit inside the timeline range. Extend that range before applying a longer
plan. Fields replace their draft value when typed; Enter keeps the draft and
Escape cancels the field.

**Apply plan** is one retained undoable command. It preserves the original
progress track, including every key and handle. Planned progress becomes the
active timing mode of the existing binding, so it does not create another
position owner. **Restore original progress keys** removes the plan and resumes
that preserved timing. Repeated Apply replaces the same target's plan. Save,
undo/redo and fresh-process reopening use the normal scene document lifecycle.

Use **Insert after**, **Remove**, and the waypoint selector to author up to 32
waypoints. Reversal and hold waypoints must be at rest. Traversed hard corners
also require explicit zero-speed waypoints; **Draft stops at every route point**
provides an editable starting draft for those routes. Smooth linked joins can
pass through at a prescribed nonzero speed. First and last speeds must be zero,
so times before and after the plan hold their endpoint positions safely.

An applied plan retains its route, progress-source, world-scale and clock
dependencies. Editing those dependencies, rebinding or detaching is refused until
Restore. Then edit and Apply again. Unrelated routes, FOV, orientation, light
intensity and camera focus remain independently editable. An applied plan is
marked **Planned (saved keys)** in the timeline; its curve and playhead feedback
show the planned progress. The retained keys remain available for restoration.
No saved scene is silently retimed on load.

## Saved contract and evaluation

The optional `extensions.ray_tracing.authoring.motion_plans` array contains one
`ray_motion_plan_v1` entry per target (maximum 64). Entries retain requested limits,
2–32 normalized waypoints and a semantic dependency snapshot. Unknown schemas,
malformed/null arrays, duplicate targets, changed dependencies, unsupported
geometry and infeasible requirements fail validation. Derived geometry/schedules
are rebuilt deterministically and are never saved as another source of truth.

Objects, camera and light share the same immutable planned progress and precise
arc-distance sampling. The existing timeline supplies local seconds, including
rational rates, nonzero range origins and subframes. Evaluation is independent
of seek order. Plan identity participates in evaluated snapshot invalidation.
Legacy scenes without a plan keep the existing distance sampler and key behavior.

The numerical policy is described in [curved-route planning](motion_route_planning.md),
[waypoint scheduling](motion_timing_schedule.md) and [straight-leg timing](motion_timing_planner.md).
It is conservative kinematic planning with finite-precision numerical evidence;
it is not an exact interval-arithmetic proof or a time-optimal curved solver.
Acceleration can change discontinuously. Jerk, force/mass, collision response,
angular limits, simulation clips and event behavior are outside M5.

## Verification

M5 numerical and ASan/UBSan targets cover the timing, scheduling and route
modules. Native `--dm5` / `--dm5-reopen` acceptance covers real panel text focus,
Apply/Restore, corner stops, rejected edits, undo/redo, source preservation,
save/reopen, combined camera/light/object motion and arbitrary seeking.
The additional audit covers three scales, 30000/1001 fps, nonzero frame origin,
subframes, endpoint holds, malformed persistence and failed-cache reset.
`tests/integration/check_m5_combined_render.py` compares seven native/headless
poses and exact independently baked images. M2 source restoration, M4 combined
motion and XYZ trails, six frozen legacy images, timeline contracts and the
headless preflight/image-export gates remain regression surfaces.
