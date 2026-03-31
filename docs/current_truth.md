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
- space mode runtime contract is active (`RT-U1`):
  - canonical enum/state: `SPACE_MODE_2D`, `SPACE_MODE_3D` via `AnimationConfig.spaceMode`
  - persisted keys:
    - `config/animation_config.json` default includes `"spaceMode": 0`
    - `data/runtime/animation_config.json` stores mutable runtime `spaceMode`
    - loader fallback key supported: `space_mode`
  - menu-level selector is visible in right-column controls:
    - `Space: 2D` / `Space: 3D (Scaffold)`
  - editor tool mode remains separate (`Editor: Path/Scene/Camera`)
- mode adapter seam is active (`RT-U2`):
  - canonical adapter files:
    - `include/render/space_mode_adapter.h`
    - `src/render/space_mode_adapter.c`
  - editor and render callsites now route screen/world conversion through adapter APIs
  - key render ray/hit setup paths now use adapter entrypoints (`MakeRay`, `MakeOffsetRay`, `ResetHit`)
  - current behavior remains 2D-equivalent in both `SPACE_MODE_2D` and `SPACE_MODE_3D` until later slices add 3D-specific projection/ray behavior
- backend separation + mode routing is active (`RT-U4`):
  - canonical runtime route files:
    - `include/render/ray_tracing_mode_backend.h`
    - `src/render/ray_tracing_mode_backend.c`
  - route contract now centralizes:
    - mode lane selection (`2D` canonical, `3D` controlled)
    - projection fallback (`SPACE_MODE_3D` -> controlled 2D projection lane)
    - integrator/tile/cache routing flags consumed by `ray_tracing2.c`
  - `ray_tracing2.c` render/event paths now consume backend route state instead of scattered mode branches
  - route coverage in tests:
    - `tests/test_runner.c` (`route2d_*`, `route3d_*` assertions)
- UX/editor parity layer is active (`RT-U5`):
  - canonical editor router files:
    - `include/editor/editor_mode_router.h`
    - `src/editor/editor_mode_router.c`
  - editor mode cycle/clamp behavior is centralized through router policy in menu/editor flows
  - editor conversion wrappers now use backend-routed view context builders for consistent controlled-3D fallback behavior
  - mode-aware hints now surface controlled-3D state explicitly:
    - menu `Space` control label/hint
    - scene editor hint banner
    - editor HUD route status text
  - router coverage in tests:
    - `tests/test_runner.c` (`test_editor_mode_router_*`)
- additive object depth contract is active (`RT-U3`):
  - `SceneObject` includes `z` metadata (`include/scene/object_manager.h`)
  - scene save path writes `objects[].z`
  - scene load path accepts optional `z`, defaults to `0.0` when omitted
  - compatibility tests now cover:
    - `z` roundtrip persistence
    - omitted-`z` fallback behavior

## Active Scaffold Migration State
- private migration plan:
  - `../docs/private_program_docs/ray_tracing/2026-03-28_ray_tracing_scaffold_standardization_switchover_plan.md`
- baseline freeze:
  - `../docs/private_program_docs/ray_tracing/2026-03-28_rt_s0_baseline_freeze_and_mapping.md`
- completed phases:
  - `RT-S0`, `RT-S1`, `RT-S2`, `RT-S3`, `RT-S4`, `RT-S5`
- active post-scaffold lanes:
  - completed font-size standardization lane:
    - `../docs/private_program_docs/ray_tracing/2026-03-30_ray_tracing_post_scaffold_font_size_pass_plan.md`
    - `RT-F0` through `RT-F5` complete
  - active trio 2D/3D parity lane:
    - `../docs/private_program_docs/ray_tracing/2026-03-30_ray_tracing_2d_3d_parity_with_line_drawing_plan.md`
    - `RT-U0` complete (baseline freeze + risk map)
    - `RT-U1` complete (space mode runtime contract + menu selector)
    - `RT-U2` complete (mode adapter seam for camera/world/ray routing)
    - `RT-U3` complete (additive scene/object `z` contract upgrade)
    - `RT-U4` complete (backend separation + explicit mode routing)
    - `RT-U5` complete (UX/editor parity layer)
    - `RT-U6` complete (verification/docs/memory closeout)
    - closeout log:
      - `../docs/private_program_docs/ray_tracing/2026-03-30_rt_u6_verification_docs_memory_closeout.md`
  - `test-stable` remains the baseline non-interactive regression gate during parity rollout slices
