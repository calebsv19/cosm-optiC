# optiC Docs Index

Start here for public repository documentation.

Public identity:
- packaged desktop product: `optiC`
- repository/program key: `ray_tracing`

Last audited: 2026-06-07.

Current public focus:
- dual-toolchain compiler-units rollout now starts at the runtime-scene import
  bridge, with explicit Clang-vs-`fisiCs` build/package selection
  and current sema coverage on:
  - `src/import/runtime_scene_bridge.c`
  - `src/import/runtime_scene_bridge_authoring.c`
  - `src/app/animation_fluid_scene.c`
  - `src/render/runtime_light_emitter_3d.c`
  - `src/render/runtime_volume_3d_sampling.c`
  - `src/render/runtime_native_3d_sampling.c`
  - `src/render/runtime_volume_3d.c`
  - `src/render/runtime_volume_3d_integrate.c`
  - `src/render/runtime_volume_3d_scatter.c`
  - `src/render/runtime_direct_light_3d.c`
  - `src/render/runtime_visibility_3d.c`
  - `src/render/runtime_camera_3d_rays.c`
  - `src/render/runtime_ray_3d.c`
- shipped native `3D` runtime ladder through `Disney`
- Phase 4.1/4.2 headless-agent render request preflight CLI
- explicit environment-light request overrides for headless native `3D`
  renders:
  - mode: `off` / `top_fill` / `ambient`
  - strengths: `ambient_strength` and `top_fill_strength`
- deep-render start/resume and export workflow
- hardened worker-backed frame-range continuation workflow:
  seed -> `start_stage = ray_tracing` continuation -> optional publish
  backfill -> grouped visualizer inspection
- worker-backed RayTracing chunks can now preserve one shared animation
  sampling window across separate jobs through
  `--ray-sampling-frame-offset` / `--ray-sampling-frame-count` and
  `bin/plan_ray_tracing_frame_chunks.py`
- tiny trio worker proofs now also cover preferred-home-server routing with VPS
  fallback for cheap validation runs
- menu/editor control-surface truth for native `3D` scene and atmosphere selection
- Material-mode right-pane Active Face Preview for selected generated face groups,
  including alpha inspection, real face-aspect sizing, grounded texture sampling,
  and stable world-up orientation
- narrower native `3D` verification lanes for config persistence, prepared-render parity/scatter, and geometry contracts
- post-`I6` renderer-lane selection before VF3D / `physics_sim` ingestion

## Current State
- `docs/current_truth.md`: current runtime contract and active boundaries.
- `docs/future_intent.md`: near-term renderer and workflow direction.
- `docs/headless_agent_render_cli.md`: `ray_tracing_agent_render_request_v1`
  request schema and preflight CLI command.
- `docs/headless_continuation_visualizer_workflow.md`: operator workflow for
  seed render -> RayTracing-only continuation -> cancel/publish-backfill ->
  visualizer grouping, including shared sampling-window chunks for continuous
  camera/light motion across separate worker jobs.
- `docs/render_review_sets/README.md`: local repo-doc review sets from detached
  renders. These are not the live visualizer website lane.
- `docs/desktop_packaging.md`: `.app` packaging commands, launcher diagnostics, and release-readiness workflow.
- `docs/memory_check_audit.md`: default-off fisiCs memory-check audit
  command, report interpretation, and latest clean RayTracing BVH harness
  summary.

Live visualizer website publication is a separate pipeline:
- stage a `visualizer-run/v1` drop under
  `_private_workspace_artifacts/codework_visualizer_runs/<drop_id>/`
- upload/import it through `skills/codework-visualizer-drop/`
- or use `tools/publish_render_outputs.sh --mode visualizer|both` as the
  higher-level entrypoint
- or use `tools/publish_latest_render_run.sh` when the latest completed run and
  its last frame are the intended publish target

Current verification contract:
- `make -C ray_tracing toolchain-contract`
- `make -C ray_tracing dump-sema-runtime-scene-bridge`
- `make -C ray_tracing dump-sema-runtime-scene-bridge-authoring`
- `make -C ray_tracing dump-sema-animation-fluid-scene`
- `make -C ray_tracing dump-sema-runtime-light-emitter-3d`
- `make -C ray_tracing dump-sema-runtime-volume-3d-sampling`
- `make -C ray_tracing dump-sema-runtime-native-3d-sampling`
- `make -C ray_tracing dump-sema-runtime-volume-3d`
- `make -C ray_tracing dump-sema-runtime-volume-3d-integrate`
- `make -C ray_tracing dump-sema-runtime-volume-3d-scatter`
- `make -C ray_tracing dump-sema-runtime-direct-light-3d`
- `make -C ray_tracing dump-sema-runtime-visibility-3d`
- `make -C ray_tracing dump-sema-runtime-camera-3d-rays`
- `make -C ray_tracing dump-sema-runtime-ray-3d`
- `make -C ray_tracing clang-build`
- `make -C ray_tracing fisics-build`
- `make -C ray_tracing memory-check-audit`
- `make -C ray_tracing clean && make -C ray_tracing`
- `make -C ray_tracing test-stable`
- `make -C ray_tracing run-headless-smoke`
  - currently routes through `test-stable` rather than a separate runtime-only lane
- `make -C ray_tracing visual-harness`
  - build-only readiness gate, not an unattended execution surface
- `make -C ray_tracing test-legacy`
- `make -C ray_tracing package-linux-worker-self-test`
- `make -C ray_tracing package-desktop-self-test`
- `make -C ray_tracing package-desktop-refresh`
- `make -C ray_tracing release-bundle-audit`
- Linux worker packaging now follows the Linux build-host architecture by
  default (`linux-x86_64` or `linux-aarch64`)

## Public Runtime Docs
- `README.md` (repo root): product/runtime overview and build/run flow.
- `docs/KEYBINDS.md`: current runtime keybind reference.

Public product-facing docs should treat `optiC` as the primary app name and
use `ray_tracing` where repo/runtime identifiers need to stay exact.

## Review Sets
- `docs/render_review_sets/grime_screen_motion_review_v2/index.md`: latest published
  detached Disney motion review for the grime-screen scene.

## Private Planning Docs
- Private scaffold plans and internal execution docs are in the workspace private docs bucket:
  - `../../docs/private_program_docs/ray_tracing/`
