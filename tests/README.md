# ray_tracing Tests

This directory contains optiC unit tests, integration runners, and deterministic
fixtures. It is intentionally broader than a single test style because the
program has interactive editor, renderer, import, headless, worker-package, and
visual-proof surfaces.

The `runtime_mesh_asset_loader` group covers the shared editor mesh-preview
contract, direct four-mode controls, stable projected-triangle picking, and the
bounded coherent LOD store. Set `RAY_TRACING_MESH_PREVIEW_COMPLEX_SCENE` to a
runtime-scene path to add an opt-in skull/dragon-class proof requiring a source
mesh of at least 100,000 triangles and a valid LOD no larger than the editor's
18,000-triangle budget.

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
make -C ray_tracing test-starter-scene-profile-contract
make -C ray_tracing test-ray-tracing-runtime-host-lifecycle-contract
make -C ray_tracing test-ray-tracing-core-sim-runtime-frame-contract
make -C ray_tracing test-scene-editor-pane-host-contract
make -C ray_tracing test-scene-editor-viewport3d-bridge-contract
make -C ray_tracing test-scene-editor-viewport-nav-contract
make -C ray_tracing test-scene-editor-primitive-preview-geometry
```

The primitive-preview geometry target pins editor-only plane tessellation and
rect-prism face coverage, including the rule that guide primitives stay
outline-only. Solid/Material integration uses these bounded triangles in the
same depth surface as imported mesh preview LODs.

The viewport3d bridge target proves Ray orientation conversion, canonical pan,
anchor zoom, orbit, frame, resize, validation, and invalid-input nonmutation against shared
`core_viewport3d >= 0.1.0`. The retained viewport-navigation target remains the
rollback oracle for the EVN3 canonical pan and anchor fixtures; the integrated
`runtime_scene_editor` group supplies frame, orbit, resize/projector, and input
adapter coverage.
The same `runtime_scene_editor` group also proves whole-object projected-origin
stable-key ranking and capture-radius rejection through `core_screen_pick >= 0.1.0`.

Scene-project worker snapshot lane:

```bash
python3 -m unittest \
  ray_tracing/tests/test_scene_project_contract.py \
  ray_tracing/tests/test_scene_project_worker_export.py \
  ray_tracing/tests/test_scene_project_worker_receipt.py
python3 ray_tracing/tools/export_worker_queue_fixture.py \
  --fixture \
  --mode scene-plus-physics-cache \
  --output-root ray_tracing/visual_artifacts/worker_queue_exports/r2b_fixture \
  --item-name ray-tracing-r2b-scene-project-20260712a \
  --force
```

The contract fixture proves three-owner validation, LineDrawing top-level and
nested active-pointer compatibility, relocation-stable content identity,
declared-hash checks, forward unknown fields, unsafe-path rejection, legacy
explicit inputs, CLI report output, and relocated worker render preparation.
The worker-export fixture proves selected-frame VF3D/pack and water attachment
copying, canonical worker attachment paths, source lineage, unsafe-path
rejection, live inline-file-limit compliance, optional job-scoped worker
targeting, and missing-frame failure before partial output creation. The
existing `scene-only` fixture remains a separate regression gate.
The worker-receipt fixture proves terminal evidence/job matching, retained-run
idempotency, offline hash validation, path containment, and preservation of
LineDrawing authoring plus PhysicsSim cache ownership.

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
make -C ray_tracing test-ray-tracing-ppm10-product-ab-fixture
make -C ray_tracing test-ray-tracing-emissive-light-preview-matrix
```

`test-ray-tracing-caustic-probe-matrix` is the local L4 caustic-readiness proof
target. It renders the canonical overhead-light, glass-sphere, matte-receiver
fixture and writes baseline receiver metrics under `_private_workspace_artifacts/`.
The original L4 baseline proved the old no-solver state. Ordinary Disney v2
requests now default caustics to off; this fixture explicitly enables the L5
analytic policy while direct-light and emission-transparency cells remain
baseline comparisons.

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
analytic caustic sidecar disabled. Debug-record assertions now stop safely and
clean up when record zero is absent, so a transport regression reports the
failed assertion instead of dereferencing a null debug path.

`TEST_RUNNER_GROUP=runtime_caustic_photon_trace_3d make -C ray_tracing test`
is the focused PPM-1/PPM-8 photon-mapper trace proof. It covers isolated photon
path state, closed sphere/ball-lens emission plus entry/exit dielectric events,
refracted branch/PDF readback, post-exit parity with the existing sphere optics
helper, emitted/final/rejected flux reconciliation, max-depth rejection, and
the PPM-8 mesh-dielectric path entrypoint fed by PPM-7 emitted photon samples.
The PPM-8 coverage stores traced mesh-dielectric output into the existing
surface photon map and volume beam map helpers while keeping render contribution
outside this module.

`TEST_RUNNER_GROUP=runtime_caustic_photon_scene_trace_3d make -C ray_tracing test`
is the focused PPM-20 plus completed PPM-21 general traversal proof. It retains
the two-interface glass-slab TLAS and descriptor-oracle checks, unresolved
material diagnostics, and opaque-hit payload coverage. Additional one-hit
fixtures inject deterministic lobe/direction samples and prove diffuse,
glossy, perfect-specular, transmission, emissive, and zero-throughput absorbed
events, including selected lobe, branch/angular/full path PDF, outgoing
direction, throughput, terminal reason, and shared-TLAS route readback. No
recursive continuation, map storage, or render contribution occurs. The seeded
fixture additionally proves replay-stable renderer-owned samples and explicit
roulette survival/termination state through the same one-hit TLAS seam.

`TEST_RUNNER_GROUP=runtime_caustic_photon_bsdf_policy_3d make -C ray_tracing test`
is the focused PPM-21 slice-one mixed-BSDF policy proof. It validates malformed
payload rejection, diffuse and mirror/specular cases, Fresnel-adjusted mixed
diffuse/glossy/transmission candidate weights, normalized branch PDFs,
sample-selected throughput compensation, expected scattered-versus-absorbed
RGB ledgers, emissive termination, and black-surface absorption. The group does
not exercise RNG, scene-loop selection, ray continuation, map storage, roulette,
or production defaults.

`TEST_RUNNER_GROUP=runtime_caustic_photon_bsdf_sampling_3d make -C ray_tracing test`
is the focused PPM-21 seeded sampling and roulette proof. It validates stable
sample replay from photon identity plus depth, independent lobe/direction/
roulette dimensions, mixed-lobe population frequencies, RGB expected-energy
reconciliation, shared path-depth-policy roulette decisions, survival
reweighting, termination accounting, and statistical unbiasedness. PPM-23
adds nested-medium eta selection and attenuation in the continuation suite;
production-default integration remains later work.

`TEST_RUNNER_GROUP=runtime_caustic_photon_path_transport_3d make -C ray_tracing test`
is the focused PPM-22 continuation and completed PPM-23 integration proof. The
PPM-22.1 fixture traces a seeded
photon between reflective surfaces for three shared-TLAS hits, resolves material
state and records a distinct sample stream at every depth, applies geometric-
normal ray offsets, preserves cumulative throughput/PDF, reproduces the same
path on replay, and terminates through the explicit max-depth ledger. PPM-22.2
adds distinct-object mirror-to-mirror and glass-to-mirror fixtures that prove
ordered object
and material resolution, reflective/refractive direction, dielectric event
counts, cumulative branch PDF, and throughput across object boundaries.
PPM-22.3 adds an outward-normal closed glass
slab with an internally launched above-critical-angle path and proves four
same-object TIR continuations, alternating reflected directions, unit branch
PDFs, preserved throughput, no false self-hit, and max-depth termination.
PPM-22.4 traces transparent object 51 into diffuse receiver object 52, preserves
the incoming receiver flux and pre-event path PDF, and transactionally stores
one receiver record plus both unique traversed beam segments. The same fixture
proves per-path/batch accounting and zero map mutation for insufficient beam
capacity, invalid traces, emissive terminal-before-storage paths, and
transparent-only paths.

The glass-to-mirror fixture proves a solid entry above air. The same-object TIR
fixture now begins from an explicit glass stack and proves stack-derived
glass-to-air eta with four successful no-change transitions. PPM-23.2 adds a
closed air/glass/water/glass/air scene that proves four ordered object/material
identities, push/push/pop/pop transitions, `1.0/1.5`, `1.5/1.33`, `1.33/1.5`,
and `1.5/1.0` interface pairs, refraction shape, return to air, and exact replay.
PPM-23.3 applies authored Beer-Lambert absorption over measured segments before
BSDF selection, records pre/post throughput and medium-absorbed flux without
changing path PDF, preserves attenuation and stack state across TIR, and stores
beam records with the traced segment medium identity. PPM-23.4 verifies that
malformed medium transitions terminate through `fail_closed` with exact reason
and depth readback before any map mutation.

`TEST_RUNNER_GROUP=runtime_caustic_photon_medium_stack_3d make -C ray_tracing test`
is the independent PPM-23 medium-state proof. It validates explicit air
initialization, material metadata conversion, ordered glass/water entry and
exit, non-mutating nested interface-IOR resolution, TIR no-change,
duplicate-entry and wrong-exit mismatch records, bounded overflow, air
underflow, invalid entries, counters, stable reason labels, and measured-distance
segment transmittance for authored absorption. No scene traversal, direction
sampling, or map mutation occurs in this group.

`TEST_RUNNER_GROUP=runtime_caustic_photon_medium_acceptance_3d make -C ray_tracing test`
is the integrated PPM-23.4 malformed-boundary acceptance proof. Duplicate entry,
non-top exit, underflow, and overflow scenarios must retain their initial stack,
report `medium_transition_rejected` plus the exact `fail_closed` reason/depth,
leave the trace invalid, and produce zero surface/beam map records and zero
store attempts.

`TEST_RUNNER_GROUP=runtime_caustic_photon_map_3d make -C ray_tracing test`
is the focused PPM-2 surface photon-map proof. It covers map allocation,
explicit surface-hit storage/query, PDF-normalized query flux, diagnostics,
storing a closed sphere/ball-lens PPM-1 trace receiver, receiver-identity
filtering, and capacity rejection before any render-path contribution.

`TEST_RUNNER_GROUP=runtime_caustic_beam_map_3d make -C ray_tracing test`
is the focused PPM-3 volume beam-map proof. It covers map allocation, explicit
beam-segment storage/query, diagnostics, storing a closed sphere/ball-lens
PPM-1 trace segment, medium filtering, capacity rejection, and the
no-render-contribution guard before any volume-cache deposit or render-path
contribution.

`TEST_RUNNER_GROUP=runtime_caustic_photon_emit_3d make -C ray_tracing test`
is the focused PPM-7 photon-emission distribution proof. It covers
deterministic emission from finite/emissive light-set entries, stable photon
ids, seeds, source PDFs, wavelength buckets, flux diagnostics, fixture-safe
surface photon-map proxy population, map-capacity reject accounting, and the
guard that render contribution remains gated outside this module.

`TEST_RUNNER_GROUP=runtime_caustic_photon_integration_3d make -C ray_tracing test`
is the focused PPM-4/PPM-6/PPM-11/PPM-12A/PPM-12C/PPM-13/PPM-16 through PPM-19 product integration proof. It covers
off/reference/production product labels, product-mode-to-caustic-settings
application, bounded sample/depth defaults, combined surface photon-map plus
volume beam-map query readback, default render-contribution suppression, and
explicit opt-in contribution/cache conversion into surface and volume caustic
caches plus renderer/headless callsite route and numeric readback. It also
proves the populated-callsite surface-map allocation, deterministic emission,
surface store, grid insertion, query, contribution, and cache-deposit counts,
and proves that existing `RuntimeCausticPhotonTrace3D` records can populate
both the surface photon map and volume beam map with
`population_source=trace_records`. It also proves the deterministic
mesh-dielectric trace-harvest helper used by the PPM-12C prepared-scene
headless probe: emitted photons are converted into solved lens paths, traced
into photon records, stored into both production maps, and queried for surface
plus volume contribution. It also proves the PPM-13 reusable
`RuntimeScene3D` mesh-dielectric descriptor batch used by the headless trace
cell. PPM-19 coverage additionally proves persistent owner generation and reuse,
retained map/population state, invalidation on geometry/light/material/volume/
budget or explicit rebuild changes, and preview/inspection/final budget labels.

`TEST_RUNNER_GROUP=ui_menu_contracts make -C ray_tracing test` includes the
desktop caustic-product contract. It proves the default-off cycle across both
retained reference products and `Photon Map (Experimental)`, the legacy
spatial-cache compatibility mapping, surface/volume selection, persisted
budget/depth/scale transfer, and that only the photon product produces an
enabled native render-prep population/contribution plan.

`test-ray-tracing-ppm10-product-ab-fixture` is the compact PPM-10 through PPM-19
product A/B fixture. It renders generated `off`, `reference`, explicit opt-in
`production`, explicit `production_populated`, and explicit
`production_trace_populated` cells from the same transparent mesh scene plus a
guarded Disney v2 `production_render_prep_populated` floor visual cell and a
generated `production_render_prep_wall_populated` vertical-receiver visual
cell, writes per-cell request JSON, summaries, PNG frames, a contact sheet, and
`ppm10_product_ab_report.json`, and validates source-specific counts. The
proxy-populated cell must report `population_source=surface_proxy`; the
trace-populated cell must report `population_source=trace_records` plus nonzero
trace-path, trace-record, beam-segment, surface-contributor, and
volume-contributor counts, one prepared-scene mesh-dielectric candidate, and no
fixture fallback. The render-prep cells must report trace-record population,
receiver lookup/acceptance counters, distribution-derived receiver footprint
radius, surface-map contribution, a successful prepared-cache deposit,
generated visual output, and positive surface-cache sampling contribution. The
render-prep path no longer writes a synthetic dielectric-centroid cache
footprint; it stores traced prepared-scene receiver hits through the production
integration receiver-policy adapter before cache conversion.

`TEST_RUNNER_GROUP=runtime_caustic_photon_scene_population_3d make -C ray_tracing test`
is the focused PPM-20 slice-two proof. It resolves an opaque receiver through
the shared TLAS route after the general scene tracer's dielectric exit, stores
one surface record and one beam segment through the existing map adapters, and
proves exact identity, position, flux, PDF, and segment parity against the
descriptor-trace adapters. It also proves transparent-receiver rejection before
storage and reason-coded partial-store accounting when one target map rejects.

The consolidated estimator and cache slice has ten focused stable groups:
`runtime_light_radiometry_3d`, `runtime_caustic_photon_direct_consumer_3d`,
`runtime_caustic_photon_estimator_3d`, `runtime_caustic_photon_provenance_3d`,
`runtime_caustic_photon_surface_provenance_filter_3d`,
`runtime_caustic_photon_volume_segment_normalization_3d`,
`runtime_caustic_photon_ppm28_3d`, `runtime_caustic_photon_ppm29_3d`,
`runtime_caustic_photon_ppm30_3d`, and
`runtime_caustic_photon_sparse_cache_3d`. Together they cover physical
radiometry, direct map consumption, convergent surface gathering, exact-once
path/provenance ownership, receiver filtering, finite volume segments,
kernel/proposal/phase normalization, and sparse/dense equivalence. They do not
include recovery-report oracles, low-discrepancy emission experiments, PVA
matrices, or rejected aperture/lens artifacts.

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
