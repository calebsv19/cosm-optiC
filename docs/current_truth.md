# Ray Tracing Current Truth

Last updated: 2026-03-30

## Program Identity
- repository directory: `ray_tracing/`
- public product name in README: `RayTracing Project`
- primary runtime entry path today:
  - `src/app/animation.c` (`main()` delegates through `ray_tracing_app_main(...)`)
  - canonical lifecycle wrapper entry:
    - `include/ray_tracing/ray_tracing_app_main.h`
    - `src/app/ray_tracing_app_main.c`

## Current Structure
- required scaffold lanes are present:
  - `docs/`, `src/`, `include/`, `tests/`, `build/`
- normalized top-level support lanes are present:
  - `config/` (tracked defaults)
  - `assets/` (static asset/docs lane)
  - `data/` (runtime/snapshot lanes)
  - `tmp/` (temp/archive lane; ignored)
- active source subsystem lanes:
  - `app`, `camera`, `config`, `editor`, `engine`, `export`, `geo`, `import`, `material`, `path`, `render`, `scene`, `tools`, `ui`
- header strategy:
  - include-dominant (`60` headers in `include/`, `5` private headers in `src/`)

## Runtime/Verification Contract (Current)
- build:
  - `make -C ray_tracing clean && make -C ray_tracing`
- scaffold smoke gate (non-interactive):
  - `make -C ray_tracing run-headless-smoke`
- visual harness build gate:
  - `make -C ray_tracing visual-harness`

Stable test lane:
- `make -C ray_tracing test-stable`
- current composition:
  - `test`
  - `test-manifest-to-trace-export`
  - `test-fluid-pack-contract-parity`
  - `test-shared-theme-font-adapter`

Legacy test lane:
- `make -C ray_tracing test-legacy`
- current composition:
  - empty lane placeholder (returns success and prints status)

## Lifecycle Wrapper Snapshot
- locked lifecycle wrapper symbols are active:
  - `ray_tracing_app_bootstrap`
  - `ray_tracing_app_config_load`
  - `ray_tracing_app_state_seed`
  - `ray_tracing_app_subsystems_init`
  - `ray_tracing_runtime_start`
  - `ray_tracing_app_run_loop`
  - `ray_tracing_app_shutdown`
- behavior-preserving delegation path:
  - `main()` now calls `ray_tracing_app_main(...)`
  - previous startup/runtime body is retained in `ray_tracing_app_main_legacy(...)` in `src/app/animation.c`

Current baseline result (`RT-S0`):
- all commands pass
- one warning observed during build:
  - `src/render/material_bsdf.c`: unused function `SelectModel`

## Shared Dependency Snapshot
Shared libs consumed by current build:
- `core_base`, `core_io`, `core_data`, `core_pack`, `core_time`, `core_scene`, `core_trace`, `core_space`, `core_theme`, `core_font`
- `kit_viz`
- `vk_renderer`
- `timer_hud`

## Runtime Config and Generated State Snapshot
- tracked defaults/config state lives in:
  - `config/scene_config.json`
  - `config/animation_config.json`
  - `config/timer_hud_settings.json`
  - `config/materials/`
  - `config/objects/`
- mutable runtime state writes to ignored lanes:
  - `data/runtime/scene_config.json`
  - `data/runtime/animation_config.json`
  - `data/runtime/timer_hud_settings.json`
  - `data/runtime/render_metrics.dataset.json`
- generated outputs currently include:
  - `build/`
  - `data/runtime/frames/**/*.bmp` (ignored by lane)
  - `data/runtime/videos/*.mp4` (ignored by lane)
  - toolchain artifact lane:
    - `*.dSYM/` (ignored)
- runtime defaults-vs-state split is scaffold-locked (`RT-S4`)

## Active Scaffold Migration State
- private migration plan:
  - `../docs/private_program_docs/ray_tracing/2026-03-28_ray_tracing_scaffold_standardization_switchover_plan.md`
- baseline freeze:
  - `../docs/private_program_docs/ray_tracing/2026-03-28_rt_s0_baseline_freeze_and_mapping.md`
- completed phases:
  - `RT-S0`, `RT-S1`, `RT-S2`, `RT-S3`, `RT-S4`, `RT-S5`
- next phase:
  - scaffold migration complete; next structured lane is post-scaffold font-size standardization plan:
  - `../docs/private_program_docs/ray_tracing/2026-03-30_ray_tracing_post_scaffold_font_size_pass_plan.md`
  - current status in that lane:
    - `RT-F0` complete (font/text/input mapping + risk capture)
    - `RT-F1` complete (runtime zoom contract + persisted `textZoomStep` config key with clamp policy)
    - `RT-F2` complete (Cmd/Ctrl shortcut wiring + live menu/HUD font refresh)
    - `RT-F3` complete (menu/editor metric-driven layout and slider/control render-hit rect unification)
    - `RT-F4` complete (pane-level overlap/clip hardening for menu + object editor lists/scroll lanes)
    - `RT-F5` complete (final verification/docs/memory sync + wrap-up commit lane closed)
    - wrap-up commit title:
      - `Post-Scaffold Font Size Standardization`
  - `test-stable` remains the baseline non-interactive regression gate during the font pass
