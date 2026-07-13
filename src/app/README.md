# src › app

Application lifecycle orchestration.

- `animation.c` – Runtime entry and lifecycle orchestration (`main`, init/run/shutdown, wrapper handoff, core loop ownership).
- `ray_tracing_runtime_host.c` – App-owned SDL/window/renderer/render-context/font-runtime/TimerHUD host contract with reverse-order teardown plus read-only lifecycle snapshot, clean-state validation, and last-failure diagnostic seams.
- `animation_fluid_scene.c` – Fluid/scene bundle apply helpers and default camera/path seeding for imported manifests.
- `animation_input_helpers.c` – Input-side helper actions (fluid overlay toggles and text-zoom shortcut handling).
- `animation_output.c` – Output/export helpers (frame capture and optional render-metrics dataset export).
- `ray_tracing_deep_render_frame_request.c` – Move-only immutable Deep Render frame handoff. It adopts one prepared frame, copies timing/camera/light/output identity, sanitizes caller cancellation pointers, rejects unowned dynamic volume/water dependencies, and emits a generation-bound S9 dispatch snapshot.
- `ray_tracing_deep_render_session.c` – App-owned Deep Render session state machine. It owns one frame request at a time across prepare/render/save/cancel transitions and advances frame index/generation only after confirmed frame save.
- `ray_tracing_deep_render_listener.c` – Main-thread Deep Render listener. It copies only matching-generation dirty progress into a retained display buffer, invokes presentation through a desktop callback, and reports terminal job publication without joining or advancing the session.
- `ray_tracing_deep_render_completion.c` – Deep Render terminal commit controller. It reaps normally completed jobs, validates the generation and complete retained image, writes/verifies the immutable output path, and advances exactly one session frame only after successful commit.
- `ray_tracing_deep_render_cancellation.c` – Main-thread Deep Render cancellation controller. It requests worker cancellation once, preserves non-blocking listener ticks until matching terminal publication, joins only a terminal job, and then releases the frame request into the canceled session state.
- `ray_tracing_deep_render_desktop_render.c` – Worker-owned native `3D` Deep Render unit. It prepares one immutable frame request, owns reusable render/host buffers, runs the tiled renderer, and publishes generation-bound dirty progress plus the final frame.
- `ray_tracing_deep_render_desktop_host.c` – Main-thread opt-in Deep Render host. It selects only supported native tiled routes, presents retained progress, commits ordered frame completion, drains cancellation before shutdown, and preserves synchronous fallback for dynamic volume/water or unsupported routes.
- `data_paths.c` – Canonical input/output/default path resolution and manifest root discovery helpers.
- `render_export_batch.c` – App-owned export batch adapter for frame counting, highest-existing-frame discovery, frame clearing, and MP4 generation.
- `ray_tracing_job_runner_status.c` – Detached RayTracing job status owner. It owns `RayTracingDetachedJobRecord` defaults, queued/start/fail/cancel transitions, JSON status/shared report persistence, progress merge, stalled/completed refresh, and PID/status readback helpers used by the local headless job runner.
- `ray_tracing_app_main.c` – Wrapper-owned staged lifecycle control plane (`bootstrap` through `shutdown`) and runtime handoff diagnostics lane with stable stage/error labels.
- `scene_loop_policy.c` – Mode-split wait policy helper for menu/editor idle vs active behavior.
- `scene_loop_diag.c` – Schema-1 `LoopDiag` emission helper for loop idle calibration parity.
