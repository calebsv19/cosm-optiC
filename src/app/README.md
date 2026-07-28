# src › app

Application lifecycle orchestration.

- `animation.c` – Runtime entry and lifecycle orchestration (`main`, init/run/shutdown, wrapper handoff, core loop ownership).
- `ray_tracing_runtime_host.c` – App-owned SDL/window/renderer/render-context/font-runtime/TimerHUD host contract with reverse-order teardown plus read-only lifecycle snapshot, clean-state validation, and last-failure diagnostic seams.
- `animation_fluid_scene.c` – Fluid/scene bundle apply helpers and default camera/path seeding for imported manifests.
- `animation_input_helpers.c` – Input-side helper actions (fluid overlay toggles and text-zoom shortcut handling).
- `animation_output.c` – Output/export helpers (frame capture and optional render-metrics dataset export).
- `preview_transport.c` – UI-free Preview transport controller. It owns
  play/pause, Loop-by-default or Bounce policy, explicit direction, exact
  rational seeks, and deterministic authored-range endpoint behavior while
  returning the canonical `TimelineSample` consumed by evaluated-scene capture.
- `preview_workspace.c` – PVI-2 interaction controller for Preview playback
  controls and the exact-frame scrubber. It pauses during a drag, preserves
  prior play state for resume, and delegates all sampling policy to
  `PreviewTransport` without mutating authored animation globals.
- `preview_workspace_render.c` – SDL-only presentation for the Preview
  transport bar; it renders retained workspace state without owning sampling.
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
- `ray_tracing_durable_io.c` – App-owned durable publication helper for recovery-critical files. It owns same-directory temporary output, file sync, atomic rename, parent-directory sync, and structural BMP validation before a rendered frame becomes resumable evidence.
- `ray_tracing_frame_recovery.c` – Completed-frame recovery scanner. It accepts only the structurally valid contiguous requested-frame prefix and reports corrupt or noncontiguous output sets for fail-closed resume policy.
- `ray_tracing_job_recovery.c` – Boot/invocation-time persistent job-root reconciliation. It refreshes dead local jobs without executing them and emits exact-digest recovery descriptors for coordinator review.
- `ray_tracing_recovery_authority.c` – Phase E manual resume authority and output-generation fence. It validates coordinator token bindings, consumes tokens once, resolves portable fence artifacts, and gates durable publication after lease loss.
- `ray_tracing_checkpoint_transaction.c` – Staged schema-2 checkpoint publication with explicit before-write, temporary-write, file-sync, rename, and directory-sync fault boundaries.
- `ray_tracing_checkpoint_reference.c` – Lightweight worker-side discovery of the newest immutable checkpoint reference without linking renderer state.
- `ray_tracing_temporal_checkpoint.c` – Schema-2 native `3D` tile-batch recovery codec. It binds immutable request/scene/asset/runtime/renderer/sampling identities, retains two complete generations, restores exact per-tile accumulation and adaptive state, and falls back from a corrupt newest generation.
- `ray_tracing_sha256.c` – Small app-local SHA-256 identity helper used to bind canonical requests, renderer executables, committed frame references, and completion summaries across the worker boundary.
- `ray_tracing_worker_protocol.c` – Version-1 process-neutral JSON contract for capabilities, requests, progress, dirty regions, cancellation, completion, interruption, and checkpoint references. It owns strict protocol/capability/checkpoint-range validation, fail-closed negotiation, canonical `WORKER_VERSION` projection, and durable event publication.
- `ray_tracing_worker_client.c` – Thin spawned-worker adapter used by the detached job runner. It negotiates capabilities, binds immutable request/build digests, writes the worker request, preserves an explicit direct fallback, and durably requests cancellation before signaling.
- `ray_tracing_worker_runtime.c` – UI-free worker runtime that validates the protocol request and immutable digests, supervises the existing headless renderer, projects durable protocol events, forwards cancellation, and reports interruption without owning queue or fleet lease policy.
- `ray_tracing_app_main.c` – Wrapper-owned staged lifecycle control plane (`bootstrap` through `shutdown`) and runtime handoff diagnostics lane with stable stage/error labels.
- `scene_loop_policy.c` – Mode-split wait policy helper for menu/editor idle vs active behavior.
- `scene_loop_diag.c` – Schema-1 `LoopDiag` emission helper for loop idle calibration parity.
- `starter_scene_profile.c` – Versioned, scene-agnostic starter-profile loader and non-destructive activation planner. It validates safe relative template/working paths and distinguishes seed, activate, preserve-user-selection, and incomplete-copy states.
- `starter_scene_startup.c` – Fresh-install startup adapter for the packaged optiC studio preset. It activates the launcher-seeded writable showcase only when no saved animation config exists and requests direct entry into the 3D Object editor.
