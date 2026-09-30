# Motion modules

- `scene_motion_paths.c`: saved route/binding validation and runtime arc-distance sampling.
- `scene_motion_handles.c`: explicit spatial handle policies.
- `runtime_motion_track_3d.c`: legacy camera/light spatial track adapter.
- `motion_timing_plan.c`: pure straight-route M5 timing solver, consumed by the
  retained planner without owning scene mutation or a new clock.
  See `docs/motion_timing_planner.md` and `test-motion-timing-plan`.

Shared reuse: existing timeline context owns frame-to-seconds conversion.
`core_time` owns the monotonic clock; `core_math` owns generic primitives. Neither
owns this authored movement policy. Keep the planner app-local until a proven
cross-app contract warrants extraction; no shared module or vendored copy changes.

- `motion_timing_duration.c`: fixed-duration feasible speed envelopes and duration
  bounds for straight legs; no implicit stationary interval on a nonzero leg.
- `motion_timing_schedule.c`: bounded waypoint arrival/hold/departure scheduling,
  conflict readback and deterministic sample selection. See
  `docs/motion_timing_schedule.md` and `test-motion-timing-schedule`.

- `motion_route_geometry.c`: bounded arc/curvature analysis and distance sampling
  for new planned motion, including singular-endpoint rest bounds.
- `motion_route_schedule.c`: conservative curved-route timing budget, explicit
  corner stops and XYZ samples. See `docs/motion_route_planning.md`.

- `scene_motion_plans.c`: versioned intent/dependency validation and immutable
  runtime plan caches; shared planned progress for all existing route followers.
  See `docs/motion_plans.md`. Editor commands and draft UI live in the focused
  `scene_editor_motion_plan` / `scene_editor_motion_plan_panel` modules.
