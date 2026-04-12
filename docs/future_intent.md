# Ray Tracing Future Intent

Last updated: 2026-04-10

## Scaffold Alignment Intent
1. Keep the current strong subsystem split while normalizing scaffold contracts.
2. Introduce locked lifecycle wrapper symbols without breaking runtime behavior.
3. Normalize verification into explicit scaffold aliases/lane names.
4. Keep lane naming/runtime-state policy stable after outlier normalization (`config`, `assets`, `data/runtime`, `tmp`).

## Planned Structural Intent
- `RT-S1` (completed):
  - added public scaffold docs:
    - `docs/current_truth.md`
    - `docs/future_intent.md`
    - `docs/README.md`
  - updated root docs pointer in `README.md`

- `RT-S2` (completed):
  - added explicit scaffold verification aliases:
    - `run-headless-smoke`
    - `visual-harness`
  - normalized test lanes:
    - `test-stable` (deterministic migration gate)
    - `test-legacy` (placeholder quarantine lane, currently empty)

- `RT-S3` (completed):
  - introduce canonical wrapper entry API:
    - `include/ray_tracing/ray_tracing_app_main.h`
    - `src/app/ray_tracing_app_main.c`
  - lock lifecycle stage symbols:
    - `ray_tracing_app_bootstrap`
    - `ray_tracing_app_config_load`
    - `ray_tracing_app_state_seed`
    - `ray_tracing_app_subsystems_init`
    - `ray_tracing_runtime_start`
    - `ray_tracing_app_run_loop`
    - `ray_tracing_app_shutdown`
  - keep behavior-preserving delegation:
    - `main()` delegates to `ray_tracing_app_main(...)`
    - legacy startup/runtime body remains in `ray_tracing_app_main_legacy(...)`

- `RT-S4` (completed):
  - normalized top-level naming outliers:
    - `Configs/` -> `config/`
    - `Animations/` -> `assets/animations/` docs lane + `data/runtime/{frames,videos}` generated lanes
    - `Other files/` -> `tmp/migration_backups/other_files_legacy/`
  - locked runtime/temp/generated ignore policy:
    - `tmp/`
    - `data/runtime/`
    - `data/snapshots/`
    - `*.dSYM/`
  - locked defaults-vs-runtime state persistence:
    - runtime-first config load fallback: `data/runtime` -> `config` -> legacy `Configs`
    - config saves now target `data/runtime/*`
  - updated build/tool defaults:
    - `make video` defaults to `data/runtime/frames/default` and `data/runtime/videos/output.mp4`

- `RT-S5` (completed):
  - closeout sync for private/public/global scaffold trackers completed
  - final verification gates passed (`clean/build`, `run-headless-smoke`, `visual-harness`, `test-stable`)
  - final scaffold closeout commit recorded:
    - `f1b666c` (`Project Scaffold Standardization`)

## Post-Scaffold Direction
- scaffold migration is complete for `ray_tracing`.
- completed post-scaffold lanes:
  - font-size standardization (`RT-F0` through `RT-F5`) is complete
  - wrap-up commit title used:
    - `Post-Scaffold Font Size Standardization`
  - completed trio 2D/3D parity propagation with `line_drawing`:
    - `../../docs/private_program_docs/ray_tracing/2026-03-30_ray_tracing_2d_3d_parity_with_line_drawing_plan.md`
  - current status:
    - `RT-U0` complete (baseline freeze and risk map)
    - `RT-U1` complete (space mode runtime contract + menu selector)
    - `RT-U2` complete (mode adapter seam for camera/world/ray routing)
    - `RT-U3` complete (additive scene/object upgrade with optional `z` compatibility)
    - `RT-U4` complete (backend separation + explicit mode routing)
    - `RT-U5` complete (UX + editor parity layer)
    - `RT-U6` complete (verification/docs/memory closeout)
  - closeout execution log:
    - `../../docs/private_program_docs/ray_tracing/2026-03-30_rt_u6_verification_docs_memory_closeout.md`
  - completed trio shared-scene bridge lane (`TP-S3`):
    - `../../docs/private_program_docs/ray_tracing/2026-04-01_rt_s3_pre_deep_readiness.md`
    - `../../docs/private_program_docs/ray_tracing/2026-04-01_rt_s3_deep_runtime_mapping.md`
    - `../../docs/private_program_docs/ray_tracing/2026-04-01_rt_s3_writeback_guardrails_closeout.md`

## Trio Interop Next Intent
- trio rollout milestone status:
  - `TP-S3` complete in `ray_tracing`
  - `TP-S4` complete in `physics_sim`
  - `TP-S5` complete (fixture-driven roundtrip and namespace-preservation interop validation)

## Connection Pass Intent
- completed:
  - `RT-CP0` baseline routing/ownership map captured
  - `RT-CP1` wrapper context + guarded stage-transition hardening landed
  - `RT-CP2` wrapper runtime dispatch seam extraction landed with typed dispatch request/outcome contract and behavior parity
  - `RT-CP3` wrapper-side dispatch flow split landed (`prepare`/`execute`/`finalize`)
  - `RT-CP4` deterministic wrapper lifecycle ownership release ordering landed
  - `RT-CP5` closeout (docs/tracker/memory sync) landed:
    - `../../docs/private_program_docs/ray_tracing/2026-04-01_ray_tracing_connection_pass_cp0_cp5_execution.md`
  - `W1` + `W2` wrapper diagnostics standardization landed:
    - `../../docs/private_program_docs/ray_tracing/2026-04-02_ray_tracing_w1_w2_wrapper_hardening.md`
- next:
  - optional `RT-CP6+`: deeper runtime/update/render/shutdown ownership extraction from `animation.c`

## Maintainability Decomposition Status
- completed in the recent decomposition tranche:
  - config file I/O helpers extracted to `include/config/config_file_io.h` + `src/config/io/config_file_io.c`
  - runtime helper slices extracted from `animation.c` into:
    - `src/app/animation_fluid_scene.c`
    - `src/app/animation_input_helpers.c`
    - `src/app/animation_output.c`
    - `src/app/data_paths.c`
  - editor/render helper splits landed:
    - `src/editor/object_editor_panels.c`
    - `src/render/pipeline/ray_tracing2_preview.c`
  - menu decomposition moved to focused lanes:
    - `src/ui/menu/sdl_menu_input.c`
    - `src/ui/menu/sdl_menu_render.c`
    - `src/ui/menu/sdl_menu_state.c`
- next:
  - keep extracting high-churn helper families out of `src/app/animation.c` while preserving wrapper ownership and existing behavior.

## RS1 Render Split Intent
- started:
  - `RS1-S0`/`RS1-S1` diagnostics-contract adoption landed in `animation.c` via shared `kit_runtime_diag`.
  - opt-in runtime diagnostics gate is available with `RAY_TRACING_RUNTIME_DIAG=1`.
- in progress:
  - `RS1-S2` completed in legacy loop:
    - explicit frame derive/submit seam extraction (`DeriveRenderInputs` + `SubmitRenderFrame`)
    - diagnostics timing now reports explicit derive/submit windows
  - `RS1-S3` completed:
    - frame submit handoff now routes through wrapper-owned helper (`ray_tracing_app_render_submit`)
    - wrapper tracks render-submit attempt/success/rejection counters in exit diagnostics
    - behavior-preserving fallback keeps direct submit path if wrapper handoff is unavailable
  - `RS1-S4` completed:
    - frame update/route handoffs now route through wrapper-owned helpers:
      - `ray_tracing_app_frame_update`
      - `ray_tracing_app_frame_route`
    - wrapper exit diagnostics now include frame update/route attempt/success/rejection counters
    - behavior-preserving fallback keeps direct update/route path if wrapper handoff is unavailable
  - `RS1-S5` completed:
    - frame event-intake handoff now routes through wrapper-owned helper:
      - `ray_tracing_app_frame_events`
    - wrapper exit diagnostics now include frame event-intake attempt/success/rejection counters
    - behavior-preserving fallback keeps direct event handling path if wrapper handoff is unavailable
- next:
  - `RS1` closeout: verification/docs consolidation and optional maintain-only status for this lane.

## IR1 Input Routing Intent
- started:
  - editor-readiness setup landed in `animation.c`:
    - explicit `InputFrame_Intake -> InputFrame_Normalize -> InputFrame_Route` helpers
    - explicit raw/normalized frame model (`RayTracingInputFrame`)
    - explicit invalidation output stage (`InputFrame_Invalidate`) and frame runner (`RunInputRoutingFrame`)
  - scene editor top-level routing seam landed in `scene_editor.c`:
    - explicit `system -> chrome -> pane` dispatch staging
    - explicit routing result + invalidation policy hook for pane-target growth
  - `IR1-S2` landed in `scene_editor.c`:
    - explicit normalized input contract (`action_class`, `route_policy`, `target_hint`)
    - explicit bounded per-event diagnostics contract (`SceneEditorInputDiagFrame`) with opt-in gate:
      - `RAY_TRACING_EDITOR_INPUT_DIAG=1`
  - `IR1-S3` landed in `scene_editor.c` + editor panes:
    - explicit pane hit-region classification before pane dispatch (`controls/list panel/canvas/drag`)
    - expanded invalidation classes in routed output (`target_ui/target_pane/target_interaction/full_exit`)
    - editor-local hit-region adapters added for pane-specific routing seams.
  - `IR1-S4` landed in editor pane handlers:
    - pane-local canonical action adapter enums/resolvers are now explicit in:
      - `bezier_editor.c`
      - `object_editor.c`
      - `camera_editor.c`
    - pane handlers now dispatch through local normalized action contracts before deeper mutations.
  - `IR1-S5` landed in scene editor:
    - active-pane routing now resolves typed pane commands before pane dispatch:
      - `SceneEditorPaneCommandKind`
      - `SceneEditorPaneCommand`
      - `SceneEditorResolvePaneCommand(...)`
    - explicit `resolve -> dispatch` pane command seam is now in place while preserving existing behavior.
  - behavior remains unchanged; this is a routing-seam setup slice.
- next:
  - continue IR1 with optional pane command payload enrichment and deeper mutation extraction behind existing command seams (keep behavior-preserving).

## Non-Goals During Scaffold Migration
- no feature-expansion work unrelated to scaffold alignment
- no shared subtree redesign inside scaffold migration slices
- no one-pass broad churn that mixes lane normalization with lifecycle refactors

## Release Readiness Next Intent
- next active release lane:
  - `RT-RL0` through `RT-RL5` execution plan:
    - `../../docs/private_program_docs/ray_tracing/2026-04-04_ray_tracing_release_readiness_rl0_rl5_execution_plan.md`
- completed now:
  - `RT-RL0` release contract freeze
  - `RT-RL1` bundle audit + Vulkan runtime hardening
  - `RT-RL2` signing/notary integration
  - `RT-RL3` release artifact + desktop refresh flow
  - `RT-RL4` release validation + docs sync
  - `RT-RL5` release closeout
- next:
  - hand off this release-ready pattern to next program lane (`RL0` pilot for next app in queue).
