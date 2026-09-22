# T4 secondary detail and preview cost

Status: T4 source implementation `51cfb1f` is adopted in Main Edit by clean
fast-forward from T3 closeout `0a4c361`. The measurement gates below were fixed
before renderer optimization. The initial adoption findings and subsequent cleanup are recorded below.
Cleanup source `1f49771` closes the three requested implementation/test gaps.

## Order and ownership

1. Freeze image references, budgets and baseline receipts.
2. Measure fixed-resolution geometry preparation, raster/shading, document edits
   and memory before choosing an optimization.
3. Add bounded ideal secondary footprints; optimize the measured preview cost.
4. Repeat the same fixtures and existing T3 compatibility/recovery checks.

Reuse existing `core_time` clocks, `core_authored_texture` filtering/graphs and
`core_mesh_preview` attributed geometry. Ray transport policy and editor lifetime,
profiling and cache ownership remain app-owned. No new shared module is needed.
Shared version changes are unnecessary unless later evidence requires extending
a renderer-neutral contract. No package, remote or Main Edit adoption is implied.

## Frozen image measurement gates

Use a T3 mixed image/procedural receiver, directly visible, reflected in a flat
ideal mirror, and transmitted through an ideal dielectric interface. Measure two
camera distances and two subpixel motion poses, in linear RGB at 160x120. Produce
an independent analytic plane/reflection/Snell reference with 4x4 point samples
per pixel and check convergence at 8x8. A higher-resolution run of the old
unbounded secondary path is not an independent reference: it still loses detail.

On the predeclared receiver interior (excluding geometric silhouette pixels):

| Metric | Required bound |
| --- | --- |
| Reference convergence RMS, linear [0,1] | <= 0.005 |
| Per-channel image mean absolute error | <= 0.04 |
| Absolute composition channel-mean bias | <= 0.01 |
| RMS contrast relative to reference | 0.85 to 1.15 |
| Motion-change residual mean absolute error | <= 0.04 |
| Numeric directions and receiver offsets away from degeneracy | relative <= 1e-8 |

Record baseline failures without calling them implementation failures. Do not
loosen these bounds after an optimization to conceal a regression. Pair the
linear receiver proof with actual headless integrator renders and route parity.

Transport both adjacent origins and directions. Only explicitly eligible ideal
events with known constant surface normals may retain a bounded footprint.
Unknown smooth/microdetail normal derivatives, stochastic GGX, rough cones,
diffuse, volume, photon, grazing and critical-angle branch disagreement remain
explicit conservative fallbacks. Preserve the central ray and medium decision.

## Frozen performance measurement gates

Use actual 640x480 Material preview rendering on named hardware. Benchmark the
settled full-quality path explicitly; production interactive scaling is a
separately labelled measurement, not an optimization credit. Fixture axes are
8K/100K/1M triangles at one instance and 1/10/64 instances of an 8K mesh. Report
per-asset triangles and total submitted triangles; this is not a 100M-triangle
cross product. Use warm-up samples and report median/p95 orbit and edit latency,
geometry preparation, raster/shading, upload and peak process memory.

Optimization acceptance: at least 20% lower median cost in the measured dominant
stage, no more than 10% p95 regression in other measured cases, and no more than
5% peak-memory increase. Repeated noisy measurements must be reported; a single
outlier is not a performance claim. There is no absolute frame-rate promise.
Unchanged pixels, UV identity, tangent handedness and retained source behavior
are mandatory. Attribute reduction requires a separate image-error gate; exact
representation sharing is preferred when it addresses measured cost.

The initially proposed 100-instance cell was attempted before optimization and
rejected by the existing importer limit of 64 mesh instances. Retain that receipt
as a capacity boundary; use 64 for the supported stress measurement. This changes
neither runtime capacity nor the frozen performance/fidelity gates. The initial
1M timing run overlapped an image harness and is diagnostic only; acceptance uses
a fresh, serial baseline with the preserved pre-optimization binary.

## Implemented policies

`Ray3D` carries absolute adjacent origins and directions. Hits retain incident
neighbors and explicit constant-normal eligibility. Ideal continuation preserves
the central ray and the existing dielectric/medium decisions. Differential
failure clears the transported marker and stays unbounded downstream, including
numerical self-hit skip paths. Curves and varying smooth normals have no bounded
normal-derivative model in this slice.

The deterministic reflection path retains its existing sharp-event threshold
(roughness <= 0.08). Transmission retains footprints only when the selected event
has no rough-cone scattering; thin walls use straight transport, and physical
interfaces use their already-selected IOR pair. Stochastic GGX and diffuse paths
remain conservative. This is a local tangent-plane footprint, not a visibility
or silhouette reconstruction algorithm.

One broad trilinear mip tap initially recovered only 78.4% contrast in the far
mirror case. Transported receiver samples therefore integrate four equal-area
subfootprints, centered at `uv +/- .25 dx +/- .25 dy`, each with half derivatives.
Stored premultiplied color, roughness moments and normal vectors are averaged
before decoding. Height-gradient taps use the same policy. Primary rays, editor
preview and unbounded secondary rays keep their existing sampling policy.

Preview work is moved without changing samples: triangle UV/derivative setup is
lazy and frame-local; a 4096-entry per-instance cache reuses exact vertex
transforms/projections; per-corner normals remain separate. Composition reuses
one coordinate calculation for image inputs and response within the current hit.
No authored geometry, UV, tangent, resource or graph representation is reduced.

## Image receipts

The final frozen fixture explicitly uses a solid dielectric (`glass_thin_walled`
false), two distances and two motion poses for all three routes. Native numeric
checks cover oblique planes, two reflections, slab refraction, distinct adjacent
origins, central-ray preservation and conservative fallbacks.

`build/surface_material_t4/secondary-baseline-final/` uses the preserved pre-T4
runtime binary: direct cases pass; reflected and refracted contrast collapses to
zero. `secondary-final/acceptance.json` passes every frozen gate:

- maximum per-channel linear error: 0.009947 (bound 0.04)
- maximum channel-mean bias: 0.000225 (bound 0.01)
- reflected contrast ratio: 0.9569–0.9861; refracted: 0.9980–1.0083
- maximum reference convergence RMS: 0.001341 (bound 0.005)
- maximum motion-change error: 0.002961 (bound 0.04)

These are linear receiver-color measurements before lighting/tone mapping.
Production headless-integrator and retained T0–T3 checks also pass (see below).
Artifacts are ignored local outputs, not a release or installed-app acceptance.


## Preview receipts

The serial comparison uses Apple M2, 16 GiB RAM, macOS 15.7.4, arm64 Clang
with the repository development build flags (`-g`, no optimization override).
There are three warm-up frames and 20 samples per orbit/cache/edit phase.
Each case runs in a fresh native process at actual 640x480 settled quality.
The separately recorded 480x360 interactive frame earns no optimization credit.

| Fixture | Raster/shading p50 before -> after (ms) | Reduction | Orbit frame p95 before -> after (ms) | Edit p95 before -> after (ms) |
| --- | --- | --- | --- | --- |
| 8K triangles, 1 instance | 730.91 -> 551.31 | 24.6% | 754.80 -> 598.43 | 769.34 -> 574.52 |
| 100K triangles, 1 instance | 791.79 -> 602.95 | 23.8% | 819.39 -> 648.24 | 869.31 -> 710.59 |
| 1M triangles, 1 instance | 1228.21 -> 880.96 | 28.3% | 1258.07 -> 923.42 | 1765.50 -> 1464.52 |
| 8K triangles, 10 instances | 197.90 -> 144.30 | 27.1% | 214.03 -> 154.73 | 220.67 -> 176.05 |
| 8K triangles, 64 instances | 115.54 -> 40.30 | 65.1% | 123.31 -> 44.76 | 188.93 -> 108.11 |

All five cases pass the frozen dominant-stage, p95, memory and exact-pixel gates.
Peak RSS ratios range from 0.8200 to 1.0389; memory measurements include native
allocator/driver variability and do not establish a general memory-reduction claim.
Every frame hash and source-geometry digest matches the baseline. Named per-corner
UVs, tangents and full submitted triangle counts are checked. Edit timing includes
document snapshot/serialize/temp-file/runtime reapply/history and explicit preview
refresh; frame timing excludes inspector drawing. These are bounded fixture costs,
not a claim that the whole desktop app meets an interactive frame-rate target.

Receipts: `build/surface_material_t4/performance-baseline-serial/baseline.json`
and `performance-final/baseline.json`. Earlier smoke/first-candidate receipts are
retained and do not replace the final comparison. The first candidate met fidelity
but missed the speed target; the final candidate meets both.

## Repeat the bounded checks

From the repository root, build the native fixture host and headless renderer:

```sh
make -j6 build/toolchains/clang/arm64/tests/scene_editor_workspace_visual_test ray-tracing-render-headless
python3 tests/integration/test_secondary_footprint_t4.py --output-root build/t4-quality-new --require-budgets
python3 tests/integration/test_secondary_integrator_t4.py --output-root build/t4-integrator-new
python3 tests/integration/test_material_performance_t4.py --output-root build/t4-performance-new --samples 20
```

Use the corresponding architecture build directory on other supported hosts.
Each output root must be new. Run performance tests without simultaneous builds
or render/test jobs. `--compare <baseline.json> --require-budgets` enforces the frozen timing,
memory, geometry and frame-hash gates; the comparison requires the same fixture
sample count and machine. The recorded pre-T4 binary is a local baseline artifact,
not a shipped executable. A new machine needs its own measured baseline.

## Production and compatibility closeout

`regressions/secondary-02/acceptance.json` repeats all image/numeric gates on the
final transport implementation and adds two active-resource eligibility cases:
a neutral normal image at nonzero strength stays unbounded even though its center
payload has no perturbation; disabling its strength restores ideal eligibility.
This avoids confusing a neutral center sample with known constant derivatives.

`regressions/integrator-03/acceptance.json` passes eight actual Disney v2 renders:
mirror/refraction, patterned/flat-image control, flattened/TLAS. Each route pair
has byte-identical images. The source changes produce spatial image differences
across the fixed interior, with maximum channel MAE 0.05155 for reflection and
0.03657 for transmission. These display-encoded integration effects are separate
from the linear accuracy gate. Physical glass records 19,200 contributing receiver
hits, refraction events and changed directions, with zero thin-wall events.

The integration fixture selects the actual mirror and transparent presets and
uses unsaturated lighting. It requests one render worker for exact diagnostic
ledger assertions; the initial parallel run lost one ledger increment despite a
complete image. The existing global diagnostic accumulator is not a reliable
exact parallel count, so no parallel ledger-accuracy claim is made here.

Fresh T3 runtime (14 cases and negative preflights), T3 authoring, T1, T2 and T0
recovery scripts pass under `build/surface_material_t4/regressions/`. Geometry,
lighting/material and topology-stability unit groups pass. Bounded test repairs
move three 52 MiB asset-set locals to heap scratch, retain a self-contained PS4D
fixture instead of requiring a sibling checkout, and explicitly select the
flattened route for two hand-built emission fixtures without TLAS contracts.

One legacy unit assertion remains failing:
`runtime_emission_transparency_nested_layers_reaches_behind_layers`. The existing
Main Edit binary reproduces it; its threshold and configuration remain unchanged.
The complete emission group is therefore **not green**. See
`unit/emission-main-edit-control.log` and `unit/emission-final-harness.log`.
This is a separate preexisting receiver-radiance follow-up, not a waived T4 gate.

T4 changes no shared module/version, authored source format, app/worker version,
release package or installed build. Main Edit source adoption and fresh checks
are recorded below. T5 remains a separate prioritization of
unwrap/atlas tools, procedural families, node canvas, multiple UV sets/UDIM and
advanced directional response; none is implicitly started by this closeout.

Final review also clears the optional profiling pixel pointer on buffer resize,
reset and disable, including upload-failure paths. A fresh 20-sample 8K repeat on
the reviewed build passes all comparison gates with 26.5% lower raster/shading
median and identical frame hashes (`performance-reviewed-8k/baseline.json`).
The combined local closeout is `build/surface_material_t4/closeout.json`; failed
fixture attempts remain retained alongside final receipts.


## Main Edit adoption — September 22, 2026

This section records the original adoption before cleanup; the cleanup section
below supersedes its open memory, emission-oracle and diagnostic-counter status.

Both source checkouts were clean before Main Edit fast-forwarded from `0a4c361`
to `51cfb1f`. Fresh development application, native fixture host, unit runner,
mesh compiler and headless renderer builds pass. Source adoption changes no
shared module, package, installed application or release metadata.

Fresh receipts under `build/surface_material_t4_adoption/` record:

- `secondary/acceptance.json`: all 12 linear image cases, analytic transport and
  active-resource eligibility gates pass.
- `integrator/acceptance.json`: all eight production renders pass with identical
  flattened/TLAS image pairs and exact single-worker transport accounting.
- `results.json`: T3 composition and authoring, T1 authoring, T2 resources and T0
  recovery pass, as do geometry, lighting/material and topology unit groups.
- `runtime_emission_transparency.log`: the same one preexisting nested-layer
  radiance assertion still fails. This is not a full-unit-suite green claim.

The immediate cleanup priority is the million-triangle adoption memory gate,
followed by that radiance assertion against its intended transport behavior and
reliable parallel diagnostic accounting.
T5 is useful only as a bounded artist workflow selected from the
[post-M6 plan](surface_material_post_m6_plan.md#current-handoff-after-t4-adoption).
General unwrap, more procedural families, a canvas, UDIM and directional response
are separate choices; adoption does not implicitly implement them.


### Initial adoption memory failure (historical)

`performance/baseline.json` passes exact frame/source identity and every timing
budget across all five cases. Raster/shading medians improve by 24.9–67.7% against
the frozen pre-T4 baseline. Four cases also pass memory; the million-triangle
case records peak-RSS ratio **1.050362**, exceeding the fixed **1.05** gate.
`performance-1m-repeat/baseline.json` repeats that case unchanged and records
ratio **1.112183**, with image and timing gates still passing. Both failures are
retained; no threshold or expected output was relaxed. Earlier isolated passing
measurements remain historical evidence and do not override these fresh failures.

Main Edit source adoption is done. Fresh acceptance is incomplete specifically
on this memory gate, in addition to the separately disclosed preexisting unit
failure. Do not label the whole adoption suite green or infer a memory saving.

A fresh control (`performance-1m-control/baseline.json`) uses the exact saved
pre-T4 binary, hash-verified against the original baseline. Its peak RSS is
3,307,749,376 bytes versus 3,583,180,800 and 3,794,075,648 bytes for the two adopted
runs (ratios 1.08327 and 1.14703). This does not establish the allocation cause,
but it does not explain away the observed increase as only a stale baseline.
The next investigation should separate geometry/document-reapply retention from
native allocator/driver costs, retaining the same geometry and frame checks.


## T4 cleanup and stop before T5

Cleanup source `1f49771` addresses the three named follow-ups without adding T5
features or changing a shared module, source format, version or installed build.

### Exact preview ownership and memory

The first adoption peak RSS was already reached before the timed loops and did
not rise during orbit, cache-hit or edit phases. Inspection found that protected
per-corner attributes forced both full and interactive preview LODs to retain
exact geometry, yet the app allocated two complete meshes and normal arrays.
The preview store now owns one protected full mesh. Interactive lookup borrows
that same mesh and its normals through the existing fallback; reset frees one
owner. The recovered-asset path uses the same rule. Unprotected reduced LODs keep
their existing independent storage. No shared LOD algorithm or limit changed.

The unchanged five-case, 20-sample matrix passes every image, geometry, timing
and memory gate in `build/surface_material_t4_cleanup/performance/baseline.json`.
Peak-RSS ratios to the frozen pre-T4 baseline are 1.0161, 1.0001, 0.9306, 0.9822
and 0.9908 for 8K, 100K, 1M, 10-instance and 64-instance cases respectively.
Dominant raster/shading medians improve 22.4–67.6%. A fixture assertion verifies
both qualities use the same protected mesh; all original frame hashes remain
unchanged. Historical failed runs above remain retained.

### Nested transmission oracle

The original tinted fixture transmits direct radiance 0.028853993 with the rear
emitter enabled and 0.008290302 with it disabled. Thus the emitter is reached;
the old `> 0.05` absolute-brightness assertion conflated transmission reachability
with attenuation through two tinted layers. No renderer behavior or brightness
threshold was weakened to force a pass. The replacement isolates the positive
emitter contribution, verifies half-emission linearity in scalar and all RGB
channels to 1e-8, and compares diffuse bounce depth 1 with 4. The complete
emission/transparency unit group now passes.

### Parallel diagnostic ownership

Existing app-level pthread synchronization protects complete ledger records,
configuration/reset and snapshots. Internal compound records share one lock;
disabled rendering takes only an atomic enabled-flag read. No allocations or
new scheduling system are added. This preserves cross-counter snapshot
invariants, min/max values and floating aggregates. Reset remains an explicit
between-job collection boundary. Integer totals are exact after workers join;
floating sums retain normal order-dependent rounding rather than a bitwise
reproducibility guarantee.

An eight-thread regression records 160,000 rays while taking 1,000 snapshots,
checks compound-counter consistency and exact final counts, then tests disabled
recording and reset. The focused `runtime_render_trace_cost_ledger` group also
runs the existing attribution tests. Eight production render pairs configured for one and four workers have identical BMP hashes and integer transport counts; floating
aggregates agree within relative 1e-10. Each image accounts for 19,200 primary
rays. See `build/surface_material_t4_cleanup/parallel-comparison.json`.

### Regression scope and remaining boundaries

Fresh secondary quality, T2 resource/UV workflows, T3 authoring and T0 recovery
pass. Geometry, lighting/material, topology, emission and focused ledger units
pass. Preview units exposed another preexisting 52 MiB stack-local asset set;
moving only that test scratch to the heap makes the complete preview group pass.

The broader adaptive-scatter/preview suite still has 14 failures. The unchanged
Main Edit control binary produces the identical failure list. They are separate
from the three requested cleanup items and remain recorded in
`ledger-unit.log` and `ledger-broad-control.log`; no full-unit-suite green claim
is made. T5 is explicitly paused. Rough/stochastic/varying-normal footprints,
silhouette reconstruction, the 64-instance limit, hands-on acceptance and release
refresh retain their previously documented boundaries.


### Fresh Main Edit cleanup adoption

Main Edit fast-forwarded cleanly to `1f49771`. Fresh development application,
fixture host, unit runner and headless builds pass. The emission, focused ledger
and repaired preview unit groups pass, as do the eight production renders
configured for four workers. The final 20-sample 1M-triangle repeat passes every
unchanged gate: peak-RSS ratio **1.020911**, raster/shading median reduction
**31.4%**, identical frame/source geometry and passing p95 timing limits.
This repeat and the earlier five-case run bound the observed behavior; allocator
variation remains visible, so no universal memory-reduction claim is made.

Main Edit receipts are in `build/surface_material_t4_cleanup_adoption/`, including
`results.json`, `performance-1m/baseline.json` and the combined `closeout.json`.
The three requested cleanup items are closed. No T5 implementation was started.
