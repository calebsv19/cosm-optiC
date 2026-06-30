# ray_tracing Tests

This directory contains optiC unit tests, integration runners, and deterministic
fixtures. It is intentionally broader than a single test style because the
program has interactive editor, renderer, import, headless, worker-package, and
visual-proof surfaces.

## Layout

- `test_runner.c`, `test_runner_registry.*`: C test runner and group registry.
- `test_*.c`, `*_suite.c`, and matching headers: C unit/contract suites for
  config, editor, runtime scene, native `3D`, materials, import, water, and
  render metrics behavior.
- `integration/`: shell and Python runners for headless render, job-runner,
  visual matrix, water review, Disney v2 review, and trio scene-contract flows.
- `fixtures/`: request JSON, scene bundles, mesh assets, Disney v2 matrix
  fixtures, water/import fixtures, and threshold configs consumed by tests and
  integration runners.
- `scene_small.json`: small scene fixture retained for legacy/simple contracts.

## Ownership Boundaries

- Unit suites should keep deterministic, local assertions close to the C
  contracts they exercise.
- Integration runners may generate outputs under `ray_tracing/build/agent_runs/`
  or other ignored build/runtime roots.
- Curated public review artifacts belong under `ray_tracing/docs/render_review_sets/`.
- Private recovered worker outputs and large manual review packages belong under
  `_private_workspace_artifacts/`, not in this test tree.
- Remote worker, VPS, Linux PC, and visualizer publication checks are not direct
  test responsibilities unless routed through the approved handoff lanes.

## Large-File Notes

The current R0 pass records these as future candidates only:

- `test_runtime_lighting_materials_transport_suite.c`
- `test_runtime_scene_editor.c`
- `test_runtime_lighting_materials_payload_suite.c`
- `test_runtime_diffuse_temporal.c`

Do not split them opportunistically during unrelated test edits. Use a dedicated
R5 testability or large-file decomposition pass when changing their structure.

## Running

Use the narrowest target that proves the behavior under change. The broad
stable lane is useful at checkpoints, but most maintenance should start from a
focused target.

Fast C and contract lanes:

```bash
make -C ray_tracing test
make -C ray_tracing test-runtime-scene-bridge-contract
make -C ray_tracing test-ray-tracing-runtime-host-lifecycle-contract
make -C ray_tracing test-ray-tracing-core-sim-runtime-frame-contract
make -C ray_tracing test-scene-editor-pane-host-contract
```

Headless request/render/material lanes:

```bash
make -C ray_tracing test-ray-tracing-render-headless-preflight
make -C ray_tracing test-ray-tracing-render-headless-image-export
make -C ray_tracing test-ray-tracing-material-preview-headless
make -C ray_tracing test-ray-tracing-caustic-probe-matrix
make -C ray_tracing test-ray-tracing-emissive-light-preview-matrix
```

`test-ray-tracing-caustic-probe-matrix` is the local L4 caustic-readiness proof
target. It renders the canonical overhead-light, glass-sphere, matte-receiver
fixture and writes baseline receiver metrics under `_private_workspace_artifacts/`.
The original L4 baseline proved the old no-solver state. The current Disney v2
request now runs the L5 analytic caustic policy by default, while direct-light
and emission-transparency cells remain baseline comparisons.

`test-ray-tracing-emissive-light-preview-matrix` is the local emitter-light
preview proof target. It renders flat wall-panel, complex emissive prism, and
dual-panel room variants, plus flush/diffuse-bounce, offset/tilted-panel, and
thin light-box wall-halo variants, while parking the first authored/moving
light far away at negligible intensity. The proof writes a contact sheet plus
`emitter_preview_report.json` under `_private_workspace_artifacts/` and checks
registered-light counts, rect versus mesh-emissive policy, sampler-only complex
mesh readback, emissive-area candidate stats, receiver ROI luma, and wall ROI
luma.

R6 demo proof:

```bash
make -C ray_tracing visual-artifact
```

`visual-artifact` is the demo proof target rather than a stable-lane test
member. It renders one source-run BMP under
`ray_tracing/visual_artifacts/source_first_frame/frames/frame_0000.bmp`,
validates the BMP as parseable and nonblank, writes
`artifact_validation.json`, and prints the final artifact path.

Job-runner, publish, package, and release probes:

```bash
make -C ray_tracing test-ray-tracing-job-runner-smoke
make -C ray_tracing test-ray-tracing-job-runner-bundle-smoke
make -C ray_tracing test-ray-tracing-job-runner-policy
make -C ray_tracing test-ray-tracing-publish-helper-validation
make -C ray_tracing test-ray-tracing-repo-doc-redaction
make -C ray_tracing test-ray-tracing-linux-worker-package-validator
make -C ray_tracing test-ray-tracing-release-contract-redaction
make -C ray_tracing package-linux-worker-self-test
```

Stable and smoke checkpoints:

```bash
make -C ray_tracing test-stable
make -C ray_tracing run-headless-smoke
make -C ray_tracing test-legacy
```

`run-headless-smoke` currently routes through `test-stable`. `test-legacy` is
empty unless a future compatibility lane is added.

Operator-invoked visual/review lanes are intentionally outside routine stable
smokes unless they are listed in `STABLE_TEST_TARGETS`. Examples include the
water long/object-coupling review targets and local worker-backed preview
matrix execution. Keep expensive visual proof runs manual unless a specific
slice promotes one into the stable lane.

The authoritative stable target list is `STABLE_TEST_TARGETS` in
`ray_tracing/make/rules-test.mk`.
