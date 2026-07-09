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
make -C ray_tracing test-ray-tracing-render-headless-tlas-blas-repeated-instance-stress
make -C ray_tracing test-ray-tracing-material-preview-headless
make -C ray_tracing test-ray-tracing-caustic-probe-matrix
make -C ray_tracing test-ray-tracing-spatial-caustic-phase4-matrix
make -C ray_tracing test-ray-tracing-spatial-caustic-phase6-surface-matrix
make -C ray_tracing test-ray-tracing-spatial-caustic-phase7-calibration-matrix
make -C ray_tracing test-ray-tracing-spatial-caustic-phase8-receiver-policy-matrix
make -C ray_tracing test-ray-tracing-spatial-caustic-phase9-transmitted-receiver-matrix
make -C ray_tracing test-ray-tracing-spatial-caustic-phase10-tangent-receiver-matrix
make -C ray_tracing test-ray-tracing-spatial-caustic-visual-sphere-mist-matrix
make -C ray_tracing test-ray-tracing-emissive-light-preview-matrix
```

`test-ray-tracing-caustic-probe-matrix` is the local L4 caustic-readiness proof
target. It renders the canonical overhead-light, glass-sphere, matte-receiver
fixture and writes baseline receiver metrics under `_private_workspace_artifacts/`.
The original L4 baseline proved the old no-solver state. The current Disney v2
request now runs the L5 analytic caustic policy by default, while direct-light
and emission-transparency cells remain baseline comparisons.

`test-ray-tracing-spatial-caustic-phase4-matrix` is the local spatial-caustic
Phase 4 proof target. It generates a compact uniform raw VF3D fog field under
`_private_workspace_artifacts/`, renders off/bootstrap/transport/combined
reference cells, and validates that physical transport populates and scatters
from the caustic volume cache without falling back to the Phase 3 temporary
bootstrap bridge.

Phase 5 surface-cache replacement is covered by focused runner groups:
`TEST_RUNNER_GROUP=runtime_caustic_surface_cache_3d make -C ray_tracing test`
proves receiver-cache lifecycle/deposit/sample behavior, and
`TEST_RUNNER_GROUP=runtime_caustic_transport_3d make -C ray_tracing test` proves
transport can populate a surface cache and Disney v2 can sample it with the
analytic caustic sidecar disabled.

`test-ray-tracing-spatial-caustic-phase6-surface-matrix` is the local Phase 6
surface-calibration proof target. It renders off, analytic-only,
transport-surface-cache-only, and combined transport+analytic cells on the
glass-sphere receiver fixture with a receiver-facing calibration camera, records
receiver ROI metrics, and validates that the transport surface cache and
analytic sidecar are independently measurable. This proves receiver-cache
deposit/sample plumbing; visual energy calibration and transmitted receiver
sampling remain later boundaries.

`test-ray-tracing-spatial-caustic-phase7-calibration-matrix` is the local Phase
7 visual calibration proof target. It compares off, default physical
surface-cache transport, calibrated physical surface-cache transport, and
calibrated transport plus analytic sidecar. The proof records full-frame luma
deltas versus off and requires the calibrated physical-cache cell to broaden and
increase visible positive frame deltas without relying on the analytic sidecar.

`test-ray-tracing-spatial-caustic-phase8-receiver-policy-matrix` is the local
Phase 8 receiver-policy proof target. It generates raised-sphere variants of
the glass-sphere receiver fixture so the receiver is no longer tangent to the
glass, disables the Phase 7 receiver fallback, and validates that physical
surface-cache-only transport still records, samples, and visibly changes the
receiver versus paired off baselines.

`test-ray-tracing-spatial-caustic-phase9-transmitted-receiver-matrix` is the
local Phase 9 transmitted-receiver proof target. It uses the canonical
glass-sphere receiver camera path, keeps the analytic sidecar off, binds the
physical surface cache, and validates that caustic receiver records can be
sampled through Disney v2 primary transmission instead of only at direct
primary receiver hits.

`test-ray-tracing-spatial-caustic-phase10-tangent-receiver-matrix` is the
local Phase 10 tangent-receiver proof target. It keeps the canonical tangent
glass-sphere/floor fixture, disables the legacy receiver fallback, and requires
the transport-backed surface cache to find real receiver geometry, accept
records, produce rendered surface-cache samples, and report zero fallback use.

`test-ray-tracing-spatial-caustic-visual-sphere-mist-matrix` is the local
scene-iteration visual proof target after Phase 10. It generates a high-quality
glass-sphere room with colored matte floor/walls plus low-density raw VF3D
mist, then renders no-caustic, physical surface-cache-only, and physical
surface+volume-cache cells. The surface-only cell is a deposit/no-fallback
diagnostic for this beauty camera; the surface+volume cell is the visible
funnel proof and must brighten versus the no-caustic mist baseline. The proof
writes PNG frames, a contact sheet, and a diagnostic report under
`_private_workspace_artifacts/` and requires physical transport without
analytic sidecar or tangent receiver fallback.

`test-ray-tracing-spatial-caustic-mesh-dielectric-lens-fixture` is the local
S4Q/S4R/S4T/S4U mesh-dielectric visual/debug proof target. It renders no-caustic,
triangle-target, and explicit `mesh_dielectric_lens` cells against an authored
closed mesh-backed lens, audits the referenced runtime mesh sidecar for closed
manifold topology and consistent shared-edge orientation, and writes an open
negative sidecar that must fail the same topology audit despite declaring
closed/manifold metadata. It writes debug JSONL and requires the mesh provider
to resolve, emit two-interface closed entry/exit paths with a traversal-profile
override, record accepted traversal and zero reject buckets with measured
inside-distance/entry-cosine/exit-cosine ranges, deposit into the volume cache,
and brighten the mist versus the no-caustic baseline.

`test-ray-tracing-spatial-caustic-imported-lens-wall-preview` is the local S4V
imported closed-lens wall-caustic proof target. It generates a high-resolution
closed biconvex runtime mesh sidecar, audits that sidecar for closed manifold
topology, renders no-lens, lens/no-caustic, and fixed-scale lens/caustic cells
with a vivid blue receiver wall, and writes the diagnostic sheet, signed
caustic heatmap, diffs, and report under
`_private_workspace_artifacts/agent_runs/ray_tracing/`. The fixture keeps the
real light on the optical axis, omits scene-geometry light markers that could
intercept transport rays, pins the preview route to `flattened_bvh`, and
requires mesh-dielectric path emission, surface-cache deposits, and visible wall
whitening versus the lens/no-caustic baseline. The fixed reference caustic scale
is `0.0025`; broader gain brackets belong in separate calibration fixtures so
this target remains a causality diagnostic.

`test-ray-tracing-spatial-caustic-imported-lens-distance-matrix` is the local
S4V lens-distance comparison target. It keeps the light and wall fixed, moves
the imported biconvex lens through six optical-axis `y` positions, and renders
three calibrated surface-caustic scales per position: `0.001`, `0.0025`, and
`0.005`. The output matrix is ordered by lens distance in rows and energy scale
in columns, and the report records emitted paths, deposits, wall positive and
saturated pixels, percentile/max luma delta, centroid, and approximate
footprint radius.

`test-ray-tracing-spatial-caustic-plano-convex-lens-distance-matrix` is the
stronger closed-lens visual proof for S4V. It generates a high-resolution
closed plano-convex spherical-cap mesh with the curved face toward the light
and a flat exit face toward the wall, fixes the surface-caustic energy scale at
`0.0025`, and renders ten optical-axis lens-distance positions. The fixture is
intended to separate "closed mesh traversal works" from "the lens prescription
is visually useful" by using a center-thick lens that should show clearer
distance-dependent wall deposition than the shallow biconvex control.

`test-ray-tracing-spatial-caustic-plano-convex-heatmap-diagnostic` is the
caustic-only diagnostic for the same plano-convex proof. It renders no-caustic
baselines, subtracts them from fixed-energy caustic renders, and writes a
heatmap sheet with rendered preview, `caustic_on - caustic_off` heatmaps at
surface footprint scales `1.0`, `2.0`, and `5.0`, plus a debug post-exit
receiver-crossing hit map from `caustic_transport_debug_paths.jsonl`. This is
the preferred fixture for diagnosing whether a visual wall blob is caused by
post-exit ray distribution or by broad surface-cache splat/composite behavior.
The report also emits aperture-sample and receiver-hit symmetry errors so
accepted path bias is visible before further lens-geometry tuning.

`test-ray-tracing-spatial-caustic-lens-shape-comparison` is the fixed-distance
S4V shape-comparison diagnostic. It keeps the light, wall, lens center, material
override, caustic scale (`0.0025`), and surface footprint policy fixed while
rendering flat slab, biconvex, plano-convex, and biconcave closed meshes. Each
shape renders a paired lens/no-caustic baseline plus a lens/caustic frame; the
output sheet orders rendered caustic frames on the first row and signed
caustic-only heatmaps on the second row. The report requires closed topology,
mesh-dielectric path emission, surface-cache deposits, visible positive
receiver deltas, and nonzero distribution spread across lens shapes.

`test-ray-tracing-spatial-caustic-lens-focal-sweep-diagnostic` is the
receiver-distance optical bench diagnostic. It keeps the light, lens center,
glass override, caustic scale (`0.0025`), and footprint policy fixed, then
sweeps the vivid receiver wall along the optical axis for plano-convex and
biconcave closed lenses. Each receiver distance renders paired lens/no-caustic
and lens/caustic cells; the output sheet contains render rows, signed
caustic-only heatmap rows, and nominal debug-hit rows. The fixture intentionally
sets the plane transform position, not only the primitive frame origin, because
the runtime scene bridge treats transform position as the effective plane
origin. The render/heatmap curves validate receiver-distance response, while
the debug-hit rows are currently marked as nominal-provider diagnostics because
`lens_receiver_crossing` still uses the mesh provider's internal receiver
distance rather than the actual surface receiver hit.

`test-ray-tracing-spatial-caustic-ball-lens-focal-crossing` is the stronger
focus-crossing diagnostic for a generated closed ball lens. It uses a darker
blue receiver wall, a small distant on-axis sphere light, an explicit
low-contrast ball-lens traversal profile (`material_ior=1.20`), caustic scale
`0.0025`, and a narrower surface footprint scale (`1.5`) while sweeping the
receiver wall through the expected focus region. The caustic debug JSONL now
exports actual `surface_receiver_*` fields from the surface-cache deposit path,
so the output sheet leads with receiver-space caustic distribution maps instead
of full-scene camera renders. The second row shows an analytic Snell sphere
reference bundle for the same light, lens IOR, lens radius, and receiver planes;
the third row keeps camera-space signed heatmaps for comparison. The report
records actual surface-hit radius, reference radius, focus-distance error,
positive-delta p95, centroid, emitted/deposited path counts, and U-shape scores;
missing path transport or measured/reference focus disagreement is a hard
failure, while weak focus-crossing evidence is reported as a warning so the PNG
remains available for diagnosis.

`test-ray-tracing-emissive-light-preview-matrix` is the local emitter-light
preview proof target. It renders flat wall-panel, complex emissive prism, and
dual-panel room variants, plus flush/diffuse-bounce, offset/tilted-panel, and
thin light-box wall-halo variants, while parking the first authored/moving
light far away at negligible intensity. The proof writes a contact sheet plus
`emitter_preview_report.json` under `_private_workspace_artifacts/` and checks
registered-light counts, rect versus mesh-emissive policy, sampler-only complex
mesh readback, emissive-area candidate stats, receiver ROI luma, and wall ROI
luma. Flat rect panel cells also assert one-sided emission-profile readback so
scene requests can distinguish directional wall panels from thin lightbox or
wall-washer geometry. The focused
`runtime_lighting_materials_direct_light` C group owns the R1 energy policy
proof that one-sided rect panels light their front side and produce zero finite
direct-light contribution from the back side.

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

The TLAS/BLAS repeated-instance stress target is part of `test-stable`; it
generates routine reports under
`ray_tracing/build/agent_runs/ray_tracing/tlas_blas_repeated_instance_stress/`
and verifies static runtime mesh repeats use one cached BLAS asset while TLAS
instance counts scale with repeated objects.

Operator-invoked visual/review lanes are intentionally outside routine stable
smokes unless they are listed in `STABLE_TEST_TARGETS`. Examples include the
water long/object-coupling review targets and local worker-backed preview
matrix execution. Keep expensive visual proof runs manual unless a specific
slice promotes one into the stable lane.

The authoritative stable target list is `STABLE_TEST_TARGETS` in
`ray_tracing/make/rules-test.mk`.
