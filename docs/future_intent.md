# Ray Tracing Future Intent

Last updated: 2026-03-29

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
  - final scaffold closeout commit intentionally deferred until user visual confirmation:
    - `Project Scaffold Standardization`

## Non-Goals During Scaffold Migration
- no feature-expansion work unrelated to scaffold alignment
- no shared subtree redesign inside scaffold migration slices
- no one-pass broad churn that mixes lane normalization with lifecycle refactors
