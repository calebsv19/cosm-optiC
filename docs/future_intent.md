# Ray Tracing Future Intent

Last updated: 2026-04-01

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
    - `../docs/private_program_docs/ray_tracing/2026-03-30_ray_tracing_2d_3d_parity_with_line_drawing_plan.md`
  - current status:
    - `RT-U0` complete (baseline freeze and risk map)
    - `RT-U1` complete (space mode runtime contract + menu selector)
    - `RT-U2` complete (mode adapter seam for camera/world/ray routing)
    - `RT-U3` complete (additive scene/object upgrade with optional `z` compatibility)
    - `RT-U4` complete (backend separation + explicit mode routing)
    - `RT-U5` complete (UX + editor parity layer)
    - `RT-U6` complete (verification/docs/memory closeout)
  - closeout execution log:
    - `../docs/private_program_docs/ray_tracing/2026-03-30_rt_u6_verification_docs_memory_closeout.md`
  - completed trio shared-scene bridge lane (`TP-S3`):
    - `../docs/private_program_docs/ray_tracing/2026-04-01_rt_s3_pre_deep_readiness.md`
    - `../docs/private_program_docs/ray_tracing/2026-04-01_rt_s3_deep_runtime_mapping.md`
    - `../docs/private_program_docs/ray_tracing/2026-04-01_rt_s3_writeback_guardrails_closeout.md`

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
    - `../docs/private_program_docs/ray_tracing/2026-04-01_ray_tracing_connection_pass_cp0_cp5_execution.md`
- next:
  - optional `RT-CP6+`: deeper runtime/update/render/shutdown ownership extraction from `animation.c`

## Non-Goals During Scaffold Migration
- no feature-expansion work unrelated to scaffold alignment
- no shared subtree redesign inside scaffold migration slices
- no one-pass broad churn that mixes lane normalization with lifecycle refactors
