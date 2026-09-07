# STL smoothing: connectivity correction and validation

The initial slice added executable evidence without changing the normal algorithm.
The subsequent correction below now implements connected smooth normals. It builds on
the managed asset backend and updates shared `core_mesh_compile` to 0.7.1
without changing its API. Normal interpretation remains renderer-owned.

## Connectivity acceptance

Build `make BUILD_TOOLCHAIN=clang smooth-mesh-runtime-compile-tool` first.
Run the strict contract into a **new** output directory:

```sh
python3 tools/smooth_mesh_reflection/connectivity_contract.py \
  --out build/my_connectivity_acceptance
```

The strict command exited 1 on compiler 0.7.0, with two named failures
(now passing with 0.7.1):

- `smooth_disconnected_fans_split`: faces sharing only a point must not average
  their normals together. Compiler 0.7.0 emitted one diagonal normal;
  acceptance requires separate +Z and -Y normals at the common position.
- `opposed_faces_order_independent`: swapping two opposed coincident faces
  must not select the opposite fallback normal solely due to file order.
  This first gate permits a stable split-normal result. A future explicit
  rejection policy would require a separately specified diagnostic assertion.

Controls require crease-aware point-touch separation, continuous coplanar
smoothing, and no generated normals in flat mode. Each run retains the STL,
scene, compiled runtime and report. Use `--capture-known-defects` for a separate
baseline gate that succeeds only when **exactly** the two known failures remain.
That command is not acceptance; a fixed compiler must pass the strict command
and will fail the obsolete known-defect capture.

## Mixed real-asset baseline

```sh
python3 tools/smooth_mesh_reflection/mixed_asset_baseline.py \
  --out build/my_mixed_baseline \
  --dragon /path/to/dragon.authoring.json \
  --wrench /path/to/wrench.authoring.json \
  --dresser /path/to/dresser.authoring.json
```

Inputs are existing mesh authoring documents. Their source STLs are copied
through managed intake; originals are unchanged. The output is a standalone
scene directory containing all retained source and compiled dependencies.
Private assets are not checked into the repository. Source axes are preserved;
objects are uniformly fitted and grounded for inspection.

The scene contains a smooth dragon, a flat wrench, a 60-degree crease-aware
dresser, a reflective floor, and fixed lighting. All three subjects use the
same rough-metal material to expose normal differences. It renders configured
normals, a forced-flat diagnostic, then configured normals again at 640x400.
The forced-flat diagnostic does not rewrite the saved object policies.

The report requires exact configured-repeat image hashes and different configured
versus flat hashes. Route mismatches are retained for every run; any mismatch
causes exit 1 after all comparison frames and the report have been written. It records
source/authoring/compiler/runtime/renderer hashes and triangle counts. These
checks prove reproducibility and mode participation, not local highlight
continuity or artistic acceptance. Inspect the images separately. This first
baseline is one material/camera setup; later slices add close-ups, matte and
perfect-specular material cases, and preview comparisons.

## Next correction

Use edge-connected normal islands for smooth generation, preserving the
angle-weighted calculation and flat behavior. Specify how opposed/duplicate
faces and non-manifold edges are classified before changing cancellation
fallbacks. Keep source triangle/material identity, avoid automatic geometry
repair, and measure vertex/memory growth. A source fix should turn both strict
failures green, retain the controls, and then repeat the same mixed scene with
an independently identified compiler build. Do not lower thresholds to accept
changed behavior. Scale tolerance, editor LOD consistency, and performance
optimization remain subsequent slices.

## Initial Main Edit readback

The initial full-resolution dragon baseline exposed acceleration-route
mismatches (781 in its first configured render; 845,294 total scene triangles).
This is a separately reported blocker, not a normal-generation acceptance pass.
The retained scene includes all three direct subjects and their floor reflections.
Before attributing all image artifacts to shading, isolate mismatched rays and
compare the route results on this same retained geometry. Do not disable the
parity check merely to make the baseline green.

The connectivity capture reproduces exactly two failures, while all three
controls pass. `make capture-smooth-mesh-connectivity-before` captures that known
state; `make test-smooth-mesh-connectivity` remains strict and is now green.
Neither target is added to the ordinary stable suite.

The completed three-render capture repeats the configured image byte-for-byte.
Mismatch counts are configured 781, forced-flat 784, and configured-repeat 781.
The configured and forced-flat image hashes differ. The route issue therefore
occurs in flat mode too; its cause is not established by this slice.

## Correction implementation

Shared `core_mesh_compile` 0.7.1 uses the existing edge-connected island builder
for Smooth as well as Crease-aware. It only joins consistently oriented edge
neighbours, excludes coincident duplicate/opposed faces, and retains angle
weighting. Point-only contacts stay split. Normal validation precedes committing
triangle-index changes. Flat mode is unchanged. This is not a manifold repair
or a general topology certification pass.

Three bounded renderer corrections accompany it:

- the BLAS near-bound tolerance depends on the near bound, not the far distance;
- triangle parallel rejection uses a relative edge/direction determinant scale,
  removing the fixed-area cutoff that disagreed across local/world mesh scales;
- BVH and TLAS candidate bounds include the existing 1e-9 tie window, clamped to
  the caller's far bound, so traversal order cannot prematurely exclude a tied
  triangle before the existing deterministic ID rule executes.

`RAY_TRACING_PARITY_RAY_PROBE=1` prints the first three mismatches per trace
context, with origin/direction, interval, and both hit IDs/distances. Captured
rays showed dresser triangle pairs at distances differing by approximately
1e-16. Parity comparisons and their thresholds were not relaxed.

Focused regressions cover scale-invariant hits and parallel misses, tied-hit
selection with caller far-limit preservation, unbounded-ray near exclusion,
point-contact normal splitting and opposed-sheet preservation. The historical
`capture-known-defects` command is expected to fail with the corrected compiler;
it must not be used as an acceptance test.

### Corrected full-scene readback

The corrected 845,294-triangle matrix reports zero parity mismatches for all
three runs (configured, forced-flat, configured-repeat). Configured renders
repeat byte-for-byte. The forced-flat image hash exactly matches the 0.7.0
before-state, while the configured shading hash changes. Source hashes and
triangle counts remain unchanged for dragon, wrench and dresser. This is a
reproducibility/correctness check, not final close-up visual acceptance or a
performance benchmark; render timings varied across cold and warm runs.

Retained local outputs: `build/smoothing_correction_mixed/baseline_report.json`,
`configured.png`, and the three full render summaries. Strict connectivity,
shared/vendored compiler suites, geometry, BVH, mesh-builder, managed asset and
fixture checks pass. Installed-app refresh remains a separate step.

## Close-up validation

Retained close-ups in `build/smoothing_closeups/` cover dragon flat/smooth,
wrench flat/crease/smooth, and dresser flat/crease/smooth, plus a direct-light
dragon pair. All ten renders have zero route mismatches. The edge audit found
43 wrench and 624 dresser edges above 60 degrees; flat/crease modes preserve
normal discontinuities at those edges. Triangle coordinates are unchanged.
Disney cavity grain is tracked separately in `disney_pixel_quality_first_pass.md`.
