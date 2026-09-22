# T4 secondary detail and preview cost

Status: T4 source implementation and its defined gates are complete in the
isolated material lane, based on T3 closeout `0a4c361`. Main Edit adoption remains
separate. The measurement gates below were fixed before renderer optimization.

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
release package or installed build. Next: adopt this source checkpoint into Main
Edit and repeat its adoption checks. T5 remains a separate prioritization of
unwrap/atlas tools, procedural families, node canvas, multiple UV sets/UDIM and
advanced directional response; none is implicitly started by this closeout.

Final review also clears the optional profiling pixel pointer on buffer resize,
reset and disable, including upload-failure paths. A fresh 20-sample 8K repeat on
the reviewed build passes all comparison gates with 26.5% lower raster/shading
median and identical frame hashes (`performance-reviewed-8k/baseline.json`).
The combined local closeout is `build/surface_material_t4/closeout.json`; failed
fixture attempts remain retained alongside final receipts.
