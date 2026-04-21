# src › app

Application lifecycle orchestration.

- `animation.c` – Runtime entry and lifecycle orchestration (`main`, init/run/shutdown, wrapper handoff, core loop ownership).
- `animation_fluid_scene.c` – Fluid/scene bundle apply helpers and default camera/path seeding for imported manifests.
- `animation_input_helpers.c` – Input-side helper actions (fluid overlay toggles and text-zoom shortcut handling).
- `animation_output.c` – Output/export helpers (frame capture and optional render-metrics dataset export).
- `data_paths.c` – Canonical input/output/default path resolution and manifest root discovery helpers.
- `ray_tracing_app_main.c` – Wrapper-owned staged lifecycle/dispatch boundary and diagnostics lane.
- `scene_loop_policy.c` – Mode-split wait policy helper for menu/editor idle vs active behavior.
- `scene_loop_diag.c` – Schema-1 `LoopDiag` emission helper for loop idle calibration parity.
