# Motion modules

- `scene_motion_paths.c`: saved route/binding validation and runtime arc-distance sampling.
- `scene_motion_handles.c`: explicit spatial handle policies.
- `runtime_motion_track_3d.c`: legacy camera/light spatial track adapter.
- `motion_timing_plan.c`: detached straight-route M5 timing solver. No scene or
  timeline mutation, UI, persistence, simulation loop, or live consumer yet.
  See `docs/motion_timing_planner.md` and `test-motion-timing-plan`.

Shared reuse: existing timeline context owns frame-to-seconds conversion.
`core_time` owns the monotonic clock; `core_math` owns generic primitives. Neither
owns this authored movement policy. Keep the planner app-local until a proven
cross-app contract warrants extraction; no shared module or vendored copy changes.
