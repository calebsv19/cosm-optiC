# src › app

Application lifecycle orchestration.

- `animation.c` – Runtime entry and lifecycle orchestration (`main`, init/run/shutdown, wrapper handoff, core loop ownership).
- `ray_tracing_runtime_host.c` – App-owned SDL/window/renderer/render-context/font-runtime/TimerHUD host contract with reverse-order teardown plus read-only lifecycle snapshot, clean-state validation, and last-failure diagnostic seams.
- `animation_fluid_scene.c` – Fluid/scene bundle apply helpers and default camera/path seeding for imported manifests.
- `animation_input_helpers.c` – Input-side helper actions (fluid overlay toggles and text-zoom shortcut handling).
- `animation_output.c` – Output/export helpers (frame capture and optional render-metrics dataset export).
- `data_paths.c` – Canonical input/output/default path resolution and manifest root discovery helpers.
- `render_export_batch.c` – App-owned export batch adapter for frame counting, highest-existing-frame discovery, frame clearing, and MP4 generation.
- `ray_tracing_job_runner_status.c` – Detached RayTracing job status owner. It owns `RayTracingDetachedJobRecord` defaults, queued/start/fail/cancel transitions, JSON status/shared report persistence, progress merge, stalled/completed refresh, and PID/status readback helpers used by the local headless job runner.
- `ray_tracing_app_main.c` – Wrapper-owned staged lifecycle control plane (`bootstrap` through `shutdown`) and runtime handoff diagnostics lane with stable stage/error labels.
- `scene_loop_policy.c` – Mode-split wait policy helper for menu/editor idle vs active behavior.
- `scene_loop_diag.c` – Schema-1 `LoopDiag` emission helper for loop idle calibration parity.
