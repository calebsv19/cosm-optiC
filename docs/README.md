# optiC Docs Index

Start here for public repository documentation.

Public identity:
- packaged desktop product: `optiC`
- repository/program key: `ray_tracing`

Last audited: 2026-07-08.

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
- experimental `Disney v2` remains isolated behind `disney_v2` and now has
  bounded proof support for transparent camera-through accumulation,
  thin-walled/solid glass handling, medium-stack diagnostics, solid
  interior-return contribution, recursive per-vertex BSDF lobe resampling,
  recursive mirror/glossy reflection, ordinary emissive-material endpoint hits,
  primary and recursive emissive-area sampling, cached emissive-light
  candidates, Disney-v2-specific edge-safe denoise/temporal reconstruction,
  default analytic caustic sidecar contribution for the glass-sphere probe,
  and SU4 plane/prism/runtime-mesh mirror surface-unification visual proof;
  remaining promotion blockers are recursive emissive-area policy for larger
  emitter sets, BRDF-evaluated direct-light estimator quality, and repeated
  release-candidate threshold signoff
- Phase 4.1/4.2 headless-agent render request preflight CLI
- first headless PhysicsSim Water Basin sidecar bridge: RayTracing can import
  `scene_bundle.json.water_source`, select animated water heightfield frames,
  append native `3D` water-surface triangles, and report water diagnostics
- explicit environment-light request overrides for headless native `3D`
  renders:
  - mode: `off` / `top_fill` / `ambient`
  - strengths: `ambient_strength` and `top_fill_strength`
  - authored environment fields: `environment_preset`, `background_brightness`,
    and `background_color`
  - resolved headless `environment_lighting` summary fields explain whether
    ambient fill, background miss radiance, or top-fill can contribute
- headless render-cost diagnostics now document the current default/opt-in env
  policy for trace-cost ledgers, frame-dataflow ledgers, R4 temporal risk
  early-stop, temporal budget heatmaps, direct-light probes, and
  reflected-transmission probes
- native `3D` zero-thickness plane surfaces now shade from both camera sides:
  plane-generated triangles are marked `twoSided`, ray hits orient shading
  normals against the incoming ray, and solid rect-prism/runtime-mesh geometry
  keeps explicit single-sided triangle metadata
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
- `../AGENTS.md`: repo-local start point for fresh agents.
- `docs/AGENT_CONTROL.md`: supported local agent workflow, safe commands,
  failure triage, and forbidden release/registry/remote actions.
- `docs/AGENT_DEMO_PACK.md`: canonical RT-R2 demo pack with stable fixtures,
  expected outputs, and the local queue-bundle dry run.
- `docs/current_truth.md`: current runtime contract and active boundaries.
- `docs/future_intent.md`: near-term renderer and workflow direction.
- `docs/headless_agent_render_cli.md`: `ray_tracing_agent_render_request_v1`
  request schema, preflight/render CLI command, PhysicsSim VF3D handoff,
  water-surface sidecar handoff, and the current render-cost/temporal-pruning
  diagnostic env policy.
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
- `docs/smooth_mesh_reflection_quality.md`: imported STL tessellation versus
  smooth-normal policy, deterministic fixture ladder, reflection matrix, and
  large-mesh acceptance gates.

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
- `make -C ray_tracing visual-artifact`
  - unattended source-run first-frame proof; renders and validates
    `ray_tracing/visual_artifacts/source_first_frame/frames/frame_0000.bmp`
    and writes metrics beside it under the ignored `visual_artifacts/` root
- `make -C ray_tracing test-legacy`
- `make -C ray_tracing package-linux-worker-self-test`
- `make -C ray_tracing package-desktop-self-test`
- `make -C ray_tracing package-desktop-refresh`
- `make -C ray_tracing release-bundle-audit`
- Linux worker packaging now follows the Linux build-host architecture by
  default (`linux-x86_64` or `linux-aarch64`)
- Linux worker package self-test validates native ELF architecture and exact
  platform/capability parity across both worker manifests
- Linux PC worker refreshes must use a package whose manifest reports
  `platform=linux-x86_64`; do not treat an Apple Silicon Mac as limited to
  `linux-aarch64` worker artifacts when the x86_64 package/toolchain lane is
  selected

## Public Runtime Docs
- `README.md` (repo root): product/runtime overview and build/run flow.
- `AGENTS.md` (repo root): fresh-agent read order, safe local commands, and
  mutation boundaries.
- `docs/AGENT_CONTROL.md`: first local headless render workflow and agent
  operating rules.
- `docs/AGENT_DEMO_PACK.md`: selected demo scenes, commands, output roots, and
  worker-bundle dry-run contract.
- `docs/KEYBINDS.md`: current runtime keybind reference.

Public product-facing docs should treat `optiC` as the primary app name and
use `ray_tracing` where repo/runtime identifiers need to stay exact.

## Review Sets
- `docs/render_review_sets/disney_v2_d218_denoise_on_off_visual_proof/index.md`:
  repo-local Disney v2 denoise on/off visual proof.
- private visual-matrix outputs under
  `_private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_visual_matrix/`
  now include primitive glass, transparent interior, mirror/glossy, high-noise
  emitter, imported-mesh pressure, skull-local, and SU4 mirror
  surface-unification matrices.

## Private Planning Docs
- Private scaffold plans and internal execution docs are in the workspace private docs bucket:
  - `../../docs/private_program_docs/ray_tracing/`
