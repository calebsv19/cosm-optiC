# Smooth Imported-Mesh Reflection Quality

## RT-MIRROR-1: recursive mirror fidelity and quality fix

Status: **Boundary 1 and Phase 1 diagnostics complete; the first reflected-hit
shared path-vertex cutover is implemented and focused-test clean; the full
visual acceptance remains failing on recursive-radiance composition**

Planning baseline:

- branch: `codex/ray-tracing-main-edit`;
- worktree: `<CodeWork>/_worktrees/ray_tracing_main_edit`;
- source commit: `382b6871e7859ec276e2eee404edf83b27676e49`;
- focused baseline: `TEST_RUNNER_GROUP=runtime_lighting_materials make test`
  passes;
- motivating operator captures: the August 9 and August 16 dragon/bunny room
  renders show direct surfaces that are smoother and more coherent than their
  reflected views, with weak or missing higher-order mirror response.

The captures are evidence, not executable specifications. Before transport or
reconstruction changes, the same failure must be reproduced in a deterministic
headless contract render with its scene, camera, materials, sampling settings,
normal provenance, and output summary retained together.

### Problem statement

Mirror hits do not currently receive one uniform continuation of the full
Disney-v2 surface evaluator. The first reflected geometry hit is traced through
the recursive transport path, but it is also shaded through a separate
direct-light-only endpoint in `runtime_disney_v2_3d.c`. That makes the first
reflected hit structurally different from an ordinary camera hit and creates a
place for reflected materials, emitters, transparency, and additional mirrors
to lose information or be counted inconsistently.

Two additional policies make the visible result worse:

- the Disney-v2 final reconstruction path deliberately copies mirror/glossy
  pixels without filtering, so reflected noise and discontinuities receive
  none of the smoothing available to ordinary stable surfaces;
- rough-reflection sampling silently falls to at most two samples above 512
  scene triangles and exactly one sample above 100,000 triangles. Scene
  complexity therefore changes reflection quality semantics at hard cliffs.

Smooth vertex normals already reach the specular-ray construction path. This
fix must preserve that correct behavior and distinguish it from silhouette
quality: interpolated normals smooth shading, while only denser geometry can
smooth a polygonal outline.

### Design objective and invariants

`RT-MIRROR-1` makes every surface reached through a mirror eligible for the same
material and lighting evaluation as the equivalent surface reached by the
camera. Mirrors are not implemented as transparent surfaces. Reflection and
transmission should share a full recursive path-vertex architecture, while
remaining distinct lobes with different direction generation, Fresnel,
throughput, offset, and medium-stack rules.

The implementation must preserve these invariants:

- `Ng` continues to own sidedness, hit orientation, ray offsets, and
  self-intersection safety;
- `Ns` continues to own smooth BRDF and reflection-direction evaluation, with
  the existing geometric-hemisphere safety correction;
- a reflected hit resolves the same payload, emission, direct light, indirect
  transport, transparency/transmission, and nested specular lobes as a primary
  hit, subject only to explicit path-depth and throughput policy;
- each contribution is accumulated exactly once; removing the special
  endpoint must not replace missing light with double-counted light;
- recursion remains bounded and finite, but scene triangle count does not
  silently redefine requested image quality;
- acceleration route changes (flattened, TLAS/BLAS, or other supported routes)
  cannot change material/normal/transport results;
- denoising may not cross object, reflected-surface, depth, normal, material,
  or disocclusion boundaries merely to make an image look smoother.

### Shared-first boundary decision

Decision: **reuse existing shared contracts; keep this behavior app-local
(`reuse-deferred`)**.

- Reuse `core_mesh_asset` / `core_mesh_compile` normal payloads and provenance.
- Reuse current `core_trace` primitives if the contract render exports trace
  lanes, but keep mirror-specific lane names and path meaning in RayTracing.
- Keep recursive Disney-v2 evaluation, reflection/transmission lobe state,
  estimator budgeting, feature buffers, denoising, final-render geometry, and
  BVHs in RayTracing. The shared ownership catalog explicitly leaves normals'
  renderer interpretation, final rendering, material sampling, and BVHs with
  the host.
- Do not create or version a shared module in this fix. Reconsider extraction
  only after a second renderer demonstrates the same stable path-vertex or
  reconstruction contract.

### Phase 0 - capture reproducible provenance

Before changing behavior:

1. Identify or reconstruct the smallest scene matching the observed failure.
2. Retain the runtime scene plus every mesh/material/light dependency under a
   deterministic test fixture root.
3. Record camera transform, integrator, dimensions, temporal frames, secondary
   samples, specular/transmission depth, denoise state, acceleration route,
   environment settings, random seed/window, and mesh normal provenance.
4. Render the unchanged baseline and retain the raw frame, resolved frame,
   summary JSON, request JSON, and image hashes together.
5. If the original screenshots cannot be reproduced exactly, label them
   qualitative operator evidence and use the deterministic fixture as the
   regression authority.

Exit gate: another clean checkout can reproduce the baseline hashes and
summary shape, or can explain any intentionally nondeterministic fields.

Boundary 1 completed on 2026-08-23. The retained fixture is
`tests/fixtures/rt_mirror_1_baseline/`, and its focused gate is:

```sh
make test-ray-tracing-mirror-baseline-contract
```

The fixture contains the source STL, authoring description, compiled runtime
mesh, complete scene, raw/resolved requests, and tracked expected contract. It
uses a 642-vertex, 1,280-triangle `generated_smooth` mesh, so the scene already
crosses the current 512-triangle rough-reflection quality cliff. The `1.6`
camera zoom keeps the direct mesh and reflected silhouette large and
simultaneously visible. The current arm64 baseline is reproducible
byte-for-byte: raw
`6d7b75c1ab70556f6ba81809b58370563574ad04b40c389b01df39444b91ec2d`
and resolved
`ef99dad809116c8cdc5f03b453d83cf7c14b19dbb0d6e2995b84ea207a84fefa`.
The gate repeats the raw render and requires the same hash, validates the
stable summary projection, and retains full outputs plus
`baseline_report.json` under the ignored build artifact root. Timing,
scheduler counters, and absolute output paths are intentionally excluded from
the stable projection. The original room screenshots remain qualitative
operator evidence.

### Phase 1 - add failing contract renders

Add one small test family, tentatively
`test-ray-tracing-mirror-recursive-fidelity-contract`, with these fixtures:

1. **Direct/reflected parity:** one smooth imported mesh is visible both
   directly and in a planar mirror. The two views use the same material,
   normal provenance, lights, and camera-relative projected scale.
2. **Nested mirrors:** two opposing or angled mirrors show a distinct depth-1,
   depth-2, and depth-3 sequence. A colored matte target, an emitter, a glass
   target, and a second mirror make missing lobe evaluation diagnosable.
3. **Normal modes:** flat, smooth, and crease-aware versions prove that the
   reflection uses the authored normal mode and preserves intended hard edges.
4. **Complexity thresholds:** visually equivalent scenes immediately below
   and above 512 and 100,000 triangles expose any hidden sample-count cliff.
   Added off-camera or non-contributing geometry must not change reflection
   quality semantics.
5. **Route parity:** flattened and TLAS/BLAS routes must agree within the
   existing numeric/image tolerance and report zero route mismatches,
   traversal overflows, and silent normal fallbacks.
6. **Reconstruction matrix:** denoise off/on and temporal-frame tiers 1, 12,
   and 32 separate transport defects from reconstruction defects.

The summary must expose, at minimum:

- requested and effective specular depth;
- reflection rays, geometry hits, emitter hits, contributing hits, and
  termination reasons by depth;
- requested and effective rough-reflection samples plus the reason for every
  reduction;
- primary and reflected normal provenance/fallback counts;
- reflected object, triangle, material, and path-depth identity for the
  diagnostic probe pixel or bounded probe region;
- mirror/glossy pixels preserved, reconstructed, rejected at a feature edge,
  and rejected for temporal instability.

The initial contract is expected to fail visually or diagnostically on the
current implementation. That failure becomes the before-state; thresholds
must not be loosened simply to make it green.

#### Phase 1A readback - direct/reflected close-up

Completed on 2026-08-23 under
`tests/fixtures/rt_mirror_1_recursive_fidelity/`. The fixture adds two
high-coverage projections over the same Boundary 1 scene: a direct close-up
and a reflected close-up centered on the mesh's virtual position. Their zooms
are calibrated so the direct surface and reflected silhouette occupy
comparable regions.

The immutable current-state capture passes:

```sh
make capture-ray-tracing-mirror-recursive-fidelity-before-state
```

The acceptance target intentionally fails:

```sh
make test-ray-tracing-mirror-recursive-fidelity-contract
```

On the unchanged renderer, the direct image contains 10,672 blue-material
pixels while the reflected image contains only 304, a reflected/direct ratio
of `0.028486` against the acceptance floor of `0.65`. The reflected view is a
large faceted white arc rather than the blue smooth material response. The
acceptance report also records that the renderer does not yet emit the required
`mirror_recursive_fidelity` summary object. That object will carry depth,
rough-reflection sampling, reflected normal provenance, and reflected probe
identity. These are two independent failing reasons: visible material loss and
missing diagnostic accountability.

Phase 1A does not claim that all Phase 1 fixtures exist. Nested mirrors, normal
modes, complexity thresholds, route parity, and the reconstruction matrix
remain subsequent focused slices. The next behavior-neutral implementation
boundary is summary instrumentation for the defined diagnostic object; the
next behavior-changing boundary remains Phase 2 evaluator unification.

### Phase 2 - unify the recursive surface evaluator

Replace the first-reflected-hit special endpoint with a shared app-local
Disney-v2 path-vertex evaluator used by camera, reflection, and transmission
continuations.

Implementation order:

1. Extract one evaluator that accepts the hit, incoming direction, payload or
   payload-resolution input, sampling context, current path state, and lobe
   budget.
2. Return structured radiance and diagnostics rather than mutating unrelated
   top-level counters from several side paths.
3. Route the primary Disney-v2 hit through the evaluator without changing its
   output; lock that parity before touching reflected routing.
4. Route the first specular reflection hit through it and remove the independent
   `RuntimeDirectLight3D_ShadeHitWithPayload` endpoint contribution.
5. Route subsequent reflection vertices through the same evaluator.
6. Keep transmission on the same evaluator while preserving its separate
   refraction direction, medium-stack, absorption, and thin-wall state.
7. Add finite-energy and single-accounting assertions at each cutover.

Likely source seams include `runtime_disney_v2_3d.c`,
`runtime_disney_v2_transport_3d.c`, the existing specular reflection helper,
material-payload resolution, and the focused lighting/material transport test
suite. The implementation should add a focused module if extraction would
otherwise enlarge either existing transport file.

Exit gate: the direct/reflected parity fixture shows the same surface response
for the same hit state, and the original primary-hit output remains within the
accepted numeric tolerance.

### Phase 3 - prove recursive mirror depth

Make the meaning of specular depth explicit:

- depth 1 may show the first reflected surface;
- depth 2 must allow a reflected mirror to show its next reflected surface;
- depth 3 must add one more distinguishable nested image;
- lowering depth removes only the continuation beyond that boundary, not the
  already-reached vertex's valid local/emissive contribution.

Verify Fresnel/throughput multiplication, Russian-roulette interaction,
self-intersection offsets, emitter precedence, background misses, and
termination diagnostics at every depth. Keep the configured maximum bounded;
changing the default depth from 3 is a separate measured product decision, not
a substitute for making depths 1-3 correct.

### Phase 4 - replace hard triangle-count sample cliffs

Delete the `>512 -> <=2` and `>100000 -> 1` quality behavior only after the
contract records its cost and replacement policy. The replacement must be a
named, inspectable reflection-quality budget, not an unbounded loop.

Required policy:

- requested quality determines the estimator target; scene triangle count may
  affect predicted cost or scheduling but cannot silently change that target;
- perfect mirrors remain one deterministic lobe direction per path sample,
  while rough reflections receive the requested/adaptive estimator samples;
- any reduction must report requested count, effective count, limiting budget,
  and reason in the render summary;
- preview, review, and final-quality behavior must be explicit settings or
  mappings, not inferred from arbitrary geometry thresholds;
- temporal accumulation and adaptive variance may satisfy a quality target,
  but the same sampling window must produce the same decision across routes;
- safety limits remain finite and cancellation-aware.

Before selecting final defaults, benchmark the Phase 1 scenes across the high,
very-high, and ultra mesh tiers. Record rays, samples, elapsed render time,
memory, variance/convergence readback, and image error against a high-sample
reference. Use that evidence to choose preview/review/final budgets. A
same-scene unexplained steady-state regression above 10 percent remains a
review blocker, but correctness cannot be traded away through an undocumented
quality cliff.

Exit gate: crossing 512 or 100,000 triangles with otherwise equivalent visible
content does not cause a discontinuous reflection-quality or effective-sample
change.

### Phase 5 - reflection-aware reconstruction

Treat reconstruction as a separate commit after recursive transport is proven.
Extend the feature data needed to identify the **secondary surface visible
through the mirror**, rather than filtering from only the primary mirror's
normal/material identity.

Candidate secondary features are reflected object/triangle identity, reflected
depth, reflected normal, reflected material class/roughness, path depth,
motion/disocclusion identity, sample count, and variance. Filter only within a
stable compatible reflected surface. Preserve silhouettes, material changes,
nested-mirror boundaries, glass boundaries, and genuine high-frequency
reflections. Keep a raw/unfiltered output and a denoise-off contract so visual
improvement cannot conceal transport regression.

Exit gate: stable reflected surfaces converge/reconstruct measurably better
than the current unconditional mirror/glossy copy path without cross-boundary
bleeding or loss of nested reflections.

### Phase 6 - real-scene acceptance

Re-render the retained dragon/bunny room or its closest reproducible successor
with identical camera and quality settings. Review direct versus reflected
smoothness, reflected emitters/glass/materials, nested mirrors, temporal
stability, and silhouettes. Retain side-by-side baseline/fixed raw/fixed
reconstructed frames and their summaries. This is local visual proof only;
release, package promotion, visualizer publication, and Registry mutation stay
outside `RT-MIRROR-1` unless separately authorized.

### Verification ladder

Run each narrow gate after its owning slice, then the broader ladder after the
behavior is complete:

```sh
TEST_RUNNER_GROUP=runtime_lighting_materials make test
TEST_RUNNER_GROUP=runtime_native_3d_render make test
make test-smooth-mesh-reflection-fixtures
make test-smooth-mesh-reflection-matrix
make test-ray-tracing-mirror-recursive-fidelity-contract
make test-ray-tracing-mirror-recursive-quality-matrix
make test-stable
make visual-artifact
git diff --check
```

The recursive-fidelity target now exists and intentionally fails acceptance on
the unchanged renderer; its separate before-state capture passes. The
recursive-quality target remains a planned name. The completed Boundary 1 and
Phase 1A render gates are intentionally focused rather than part of routine
`test-stable`; the full visual matrix should also stay outside that lane unless
its cost becomes appropriately bounded.

### Completion criteria and commit boundaries

The fix is complete only when:

- a retained pre-change contract demonstrates the current defect;
- reflected and directly viewed instances use the same normal/material/surface
  evaluator semantics;
- depth-2 and depth-3 mirror continuations are visibly and diagnostically
  distinct;
- reflected matte, emissive, glass, and mirror targets all retain their full
  eligible shading behavior;
- effective reflection quality has no hidden 512/100,000-triangle cliff;
- mirror-aware reconstruction improves stable regions without edge bleeding;
- all focused, route-parity, stable, and real-scene gates pass with bounded
  finite energy and no silent fallback.

Keep the review sequence separable:

1. fixture/provenance/diagnostic contract;
2. unified path-vertex evaluator and first reflected-hit cutover;
3. nested mirror depth semantics;
4. reflection-quality budget replacing hard caps;
5. reflection-aware reconstruction;
6. real-scene proof and documentation closeout.

### Phase 1B diagnostic boundary readback

The behavior-neutral diagnostic boundary is implemented in the isolated main
edit worktree. Headless summaries now include `mirror_recursive_fidelity` with:

- requested and effective specular depth;
- requested and effective rough-reflection sample counts plus any legacy
  triangle-count reduction reason;
- reflected vertex-interpolated versus flat-fallback hit counts;
- a deterministic reflected-hit probe identity; and
- ray, geometry-hit, emitter-hit, contributing-hit, and termination counts for
  each effective reflection depth.

The close-up before-state contract still reproduces byte-identical BMP hashes,
so this boundary does not change rendering behavior. The refreshed acceptance
contract passes all summary and reflected-provenance requirements and now fails
only the intended material-fidelity condition: `304 / 10672 = 0.028486`
reflected-to-direct blue pixels, below the required `0.65`.

The diagnostic readback also proves that the failing reflected view reaches the
generated-smooth subject through the mirror: the selected probe identifies
`smooth_subject`, material `2`, path depth `1`, with effective
`vertex_interpolated` normals. Therefore the next behavior change should not
start by regenerating normals or raising recursion depth. It should replace the
first reflected geometry hit's direct-light-only shortcut with the same
path-vertex material/BSDF evaluation used for an ordinary camera-visible hit,
while keeping geometric-normal ray-offset authority and bounded recursion.

The legacy rough-reflection triangle caps remain observable rather than fixed
in this boundary. Their removal belongs to the later reflection-quality budget
commit, where requested sample quality must be controlled by an explicit
budget and reported degradation reason instead of the current hidden `>512`
and `>100000` triangle cliffs.

### Phase 2A readback - first reflected-hit evaluator cutover

The first behavior-changing Phase 2 boundary is implemented in
`runtime_disney_v2_3d.c`. A reflected geometry hit now resolves its material
payload and enters the same internal Disney-v2 path-vertex evaluator used by
camera-visible geometry. The evaluator receives the reflected ray's inverse as
its view direction and the inherited trace-pixel context. The former
`RuntimeDirectLight3D_ShadeHitWithPayload` reflected endpoint has been removed.

This cutover intentionally passes `continue_transport=false` for the local
reflected vertex. That permits the shared evaluator to compute payload surface
response, principled BSDF state, direct light, emission, and mirror composition
without starting a duplicate primary-style transport tree. The existing
depth-aware specular recursion remains the sole continuation authority for
later path vertices, preserving bounded mirror-in-mirror behavior and avoiding
double accounting.

Focused transport coverage now requires the first reflected material to report
`specularReflectionLocalPathVertexEvaluated` and to produce a material-colored
local result. The red-material unit scene passes this assertion, and
`TEST_RUNNER_GROUP=runtime_lighting_materials make test` is green.

The fresh close-up render proves both the success and the limit of this slice.
For the reflected camera, the shared first-hit evaluator accumulates a
blue-biased local radiance of approximately
`[12403.816, 14507.513, 20515.333]`. The existing recursive continuation adds
approximately `[68698.690, 70222.325, 75297.383]`, which is much larger and
comparatively neutral. Consequently, image-level blue retention remains
`304 / 10676 = 0.028475`, below the `0.65` contract floor. The new direct and
reflected frame hashes are respectively
`a18136bef68cebc8a2bbd10dd80330a843a86896f51d4f29cce9fc4ca0660516` and
`27823469f50642dafa1c230082f192ca3f1e9a5bc41468e66c329e38f49acd2f`.

Therefore Phase 2A is complete, but Phase 2 is not. The next behavior-changing
boundary is to audit and correct how the recursive return is composed through
the first reflected material's BSDF throughput. It must retain nested mirrors
and single accounting; it must not be replaced by an arbitrary color clamp,
disabled recursion, or a weakened image threshold.

### Phase 2B readback - reflected recursive BSDF continuation

The deeper reflected-continuation boundary is now implemented. The audit found
that the common recursive sampler first generated a BSDF direction and PDF but,
whenever the scene had a finite light, replaced specular and diffuse directions
with the direction to that light. For specular paths the original PDF was
retained. The reflection lane therefore traced a different direction than the
one whose BSDF/PDF produced its throughput. At bright metallic vertices this
could drive all color channels into the existing estimator safety ceiling and
turn a colored recursive return nearly neutral.

The recursive sampler now takes an explicit finite-light-direction-proposal
policy. Existing ordinary camera-path behavior retains that proposal for this
bounded change. The specular-reflection continuation disables it, so every
depth after the first reflected hit traces the direction sampled from that
hit's resolved principled BSDF with the matching PDF and throughput. Direct
lighting at the already-reached first reflected vertex remains owned by the
shared Phase 2A evaluator and is not reintroduced as a recursive emitter hit.

Focused contracts prove that:

- the reflection lane's BSDF sample is invariant to finite-light presence;
- a diffuse first reflected material no longer receives a duplicate recursive
  finite-light contribution;
- rough reflected continuations use the same BSDF-only policy; and
- two opposing mirrors retain a depth-2 specular return to the first mirror,
  with the existing depth capacity still bounding the path.

`TEST_RUNNER_GROUP=runtime_lighting_materials make test` passes. A small weak
test stub for `AnimationPreserveCurrentEnvironmentOnNextInit` was also added so
the focused runner remains linkable alongside the concurrent environment-menu
API; it has no runtime behavior.

The fresh close-up readback confirms that the erroneous recursive energy was
removed rather than tinted or clamped. Aggregate reflected recursive radiance
fell from approximately `[68698.690, 70222.325, 75297.383]` to
`[1.283, 1.363, 1.509]`, while the summary still reports 28,293 depth-2 rays and
11,122 depth-3 rays. The new direct/reflected frame hashes are
`375f916a3084708150ce814d8791b975c74498cfa340079954c4841c726c5652` and
`09a8f007f1eb12a9b6885565d3f247865699e9ff0beaec8c42d1143d0b88d416`.

The full visual contract still fails at `305 / 10674 = 0.028574`, below the
`0.65` floor. Because the first reflected local evaluator remains strongly
blue-biased while recursive radiance is now negligible, deeper recursion is no
longer a plausible cause of the white reflected arc. The next bounded audit
must separate the first reflected vertex's local direct/specular composition
from exposure/tonemapping and reconstruction. It must not weaken the fixture or
reapply a color filter to the now-correct recursive return.

### Phase 2C readback - raw radiance, tone mapping, and reconstruction isolation

This diagnostic boundary is complete without another transport-math change.
The headless mirror summary now retains a deterministic high-chroma reflected
geometry probe with its object/material/triangle identity, screen pixel, raw
first-vertex linear RGB, deeper recursive linear RGB, full reflection RGB after
the mirror BSDF, fully composed primary-mirror RGB immediately before tone
mapping, and the byte-domain result predicted by the common native-3D tone
curve. The probe is selected by maximum first-vertex linear chroma, with screen
coordinate tie-breaking, so it measures a visible material-bearing sample
instead of the former lowest-triangle identity probe whose radiance could be
zero.

`request_reflected_raw_probe.json` renders the same close-up with one temporal
sample and denoise disabled. The `radiance-isolation` contract reads the final
BMP at the recorded probe coordinate and requires it to match the summary's
tone-map prediction exactly. It then compares that raw lane with the retained
four-sample reconstructed lane.

The fresh probe isolates the loss:

- first reflected vertex: `[1.026, 3.631, 9.551]` linear RGB, blue/red
  `9.308`;
- after mirror BSDF: the same `[1.026, 3.631, 9.551]` for this sample;
- full mirror-pixel composition before tone mapping:
  `[7.898, 10.656, 16.975]`, still blue/red `2.149`;
- common tone-map result and actual raw BMP pixel: `[239, 243, 247]`, blue/red
  `1.033` and channel range `8`;
- reconstructed probe pixel: also `[239, 243, 247]`, a zero-byte delta from
  the raw lane.

The four-sample summary separately reports 48,683 mirror/glossy pixels
preserved by the Disney-v2 denoiser and only `0.0533` total luma change across
the frame. Reconstruction is therefore not producing the white reflected arc.
The first reflected material evaluation is strongly blue, and its composed
linear mirror pixel is still visibly blue by the acceptance ratio, but the
high neutral host-mirror base raises all three channels into the shoulder of
the shared tone curve. That curve compresses the remaining channel separation
below the fixture's visible-color threshold.

The next behavior-changing boundary should audit energy conservation in the
host mirror's local-base-plus-reflection composition before changing the global
tone curve. In particular, determine why approximately
`[6.872, 7.025, 7.424]` of comparatively neutral host-surface radiance is added
to this reflected sample and whether mirror dominance/Fresnel should blend or
attenuate those terms instead of summing them. A mirror-specific color boost,
post-tone tint, or disabled reconstruction would hide the accounting defect and
is not an acceptable fix. If an energy-conserving composition still drives the
valid linear reflection into the tone-map shoulder, exposure/tone-curve policy
can then be assessed as a separate global output boundary.

### Phase 2D readback - host-mirror energy composition

This behavior boundary removes the identified local-energy bypass and makes the
remaining composition directly auditable. The mirror policy already derived an
authored `dominance` from reflectivity, specular weight, and roughness, with
`base_attenuation = 1 - dominance`. It attenuated local diffuse response but
left two host-surface approximations outside that budget: the local direct
specular lobe and the native ambient fill. Both were added on top of the
separately traced reflection. A polished mirror could therefore receive nearly
all of its reflected lobe plus a full neutral host highlight and ambient fill.

The corrected policy applies the base attenuation to local diffuse, local
specular, direct-light transport input, and ambient fill. Traced reflection is
kept as its own BSDF-weighted contribution. Emission and transmission remain
separate physical mechanisms, and the summary also exposes stochastic and
unclassified post-shade terms rather than silently assigning them to the
reflection. This is an energy-budget correction inside the current Disney-v2
renderer; it is not a claim that the renderer is already an unbiased spectral
path tracer or that the current lobe sampler implements full MIS.

At the fresh raw representative pixel, the authored polished mirror has
`dominance = 0.98` and `base_attenuation = 0.02`:

- local specular changes from `[6.579349, 6.726210, 7.108047]` to
  `[0.131587, 0.134524, 0.142161]`;
- ambient changes from `[0.158118, 0.161647, 0.170824]` to
  `[0.003162, 0.003233, 0.003416]`;
- traced first-vertex reflection is `[0.541790, 1.917288, 5.042811]`;
- the complete classified sum is `[0.810818, 2.192339, 5.333511]`, matching the
  recorded pre-tone-map pixel to within `1e-9` per channel;
- the raw tone-mapped pixel is `[164, 207, 232]`, retaining blue/red `1.415`
  instead of the prior nearly neutral `[239, 243, 247]`.

Reflectivity, specular weight, roughness, and environment strength remain the
authoring controls. Higher reflectivity and a smoother surface devote more of
the budget to focused reflection; rougher or less reflective surfaces retain
more local/base response. The contract verifies each attenuated RGB term
against the authored base attenuation and does not introduce a mirror-only tint
or post-tone color boost.

The frame-level visible acceptance improves from `0.028574` to `0.531687`
reflected/direct blue-pixel retention, but the required `0.65` gate is still
honestly failing. The zoomed reflected object now retains blue response, while
large areas remain visibly over-bright and low-contrast. The next boundary
should therefore audit the remaining reflected-frame coverage loss spatially,
especially no-hit/environment radiance and reflection/background exposure,
before changing the global tone curve or relaxing the gate.

### Phase 2E: environment misses and recursive single accounting

The spatial audit found `18,258` first-reflection rays and `23,436` deeper
recursive paths terminating without geometry while the authored ambient sky
was enabled. Those paths previously returned black. Primary camera misses,
first mirror misses, and deeper recursive misses now use one directional
environment evaluator. In the fresh proof, first-reflection misses contribute
aggregate RGB `[2124.411, 2277.013, 2576.422]`, and deeper misses contribute
`[2673.542, 2816.488, 3102.216]`. This is physical background radiance, not a
blue-subject color correction, so it is diagnosed separately from the
material-retention ratio.

The same audit found a true single-accounting error in the deterministic mirror
continuation merge. Its deeper result was stored in the dedicated
`specularReflectionRecursiveRadiance` diagnostic, added to
`specularReflectionRadiance`, and then also copied into the host vertex's
independent `recursiveBsdfRadiance` bucket. The last copy is removed. Nested
mirror activity and its dedicated diagnostics remain intact, while final
radiance receives that deterministic continuation once through the reflection
branch.

A second raw probe now selects the highest-luminance, low-chroma pixel whose
first reflected object identity is exactly `smooth_subject`. At `(129, 51)`,
the first reflected vertex and deterministic reflected contribution are both
`[0, 0, 0]`, but the final pre-tone-map pixel is approximately
`[30.8017, 30.8070, 30.8208]`, resolving exactly to `[251, 251, 251]`. Its
disjoint host terms are:

- local specular after the mirror base share: approximately
  `[0.1159, 0.1184, 0.1252]`;
- ambient after the base share: approximately `[0.0032, 0.0032, 0.0034]`;
- light-sampled stochastic direct: approximately `[0.1182, 0.1209, 0.1277]`;
- independently sampled recursive BSDF transport:
  `[30.5645, 30.5645, 30.5645]`.

Thus the remaining bright neutral crown is not caused by reflected smooth
normal loss, the reflected material evaluator, output tone mapping, or temporal
reconstruction. It originates in the host mirror's separate stochastic
recursive estimator. A broad attempt to suppress every stochastic specular
path after a deterministic reflection invalidated general material-path tests
and was not retained. The next behavior boundary must instead establish
explicit estimator ownership and MIS/variance rules for polished mirrors,
including whether the deterministic rough-reflection estimator or the generic
stochastic BSDF estimator owns a given specular sample. Do not weaken the
`0.65` gate or introduce a mirror-only color boost before that contract exists.

### Phase 2F: polished-mirror direction/PDF ownership

The white mirror-base hypothesis is visually reasonable but does not explain
the isolated neutral spike. The fixture mirror is a pale blue-white
`[0.88, 0.90, 0.94]`, and the deterministic reflection path normalizes that
base color by luminance, producing only a slight cool tint. Its principled
material is nonmetallic, so the legacy `0.98` reflectivity floor produces an
achromatic dielectric F0; changing the base color would therefore retint one
reflection branch but would not remove the measured neutral recursive energy.

The host recursive estimator instead contained a direction/PDF ownership
error. It first sampled a GGX reflection direction and PDF, then, when a finite
light was present, replaced only the direction with the point-light direction.
The original GGX PDF and throughput were retained. On a polished mirror this
both duplicated direct-light work and could turn a low-probability sample into
a very large neutral contribution. The exact faulty contribution at the
object-specific probe was `[30.5645, 30.5645, 30.5645]`.

For materials classified by the existing host-mirror composition policy, the
recursive path now retains its BSDF-sampled direction with its matching PDF.
Finite-light evaluation remains in the dedicated direct-light estimator. The
ordinary non-mirror recursive fixtures retain their existing finite-light
proposal behavior as a bounded compatibility lane; those fixtures now use a
rough non-mirror host explicitly so they do not accidentally assert polished
mirror behavior. A focused regression compares otherwise identical mirror
recursive paths with and without a finite light and requires their sampled ray
directions to be identical and the light-bearing path not to claim an emitter
hit merely because the light exists.

The same boundary completes deterministic recursion composition. Deeper
radiance owned by the specular-reflection loop enters final `specularRadiance`
once, remains visible through `specularReflectionRecursiveRadiance`, and is not
copied into the host's independent `recursiveBsdfRadiance` bucket.

Fresh radiance-isolation proof now passes. In the reflected frame,
bright-neutral pixels fall from `3,708` to `0` and saturated white pixels fall
from `1,854` to `0`. The raw high-chroma subject probe retains first-vertex
blue/red `9.308`, composed pre-tone-map blue/red `5.422`, and resolves to
`[176, 211, 233]`; its classified linear terms reconstruct the pixel within
`1e-9` per channel. The low-chroma subject probe also reconstructs exactly and
resolves to `[186, 191, 201]` instead of the former `[251, 251, 251]` crown.

The old whole-frame blue-pixel count is now `3,978 / 10,741 = 0.370357`, still
below its fixed `0.65` gate even though the whitewash signature is gone. That
metric counts threshold-crossing pixels across two different projections and
penalizes a darker, physically bounded reflection. It remains reported as a
legacy acceptance failure, but it is no longer a sufficient behavioral oracle.
Future acceptance should be based on the retained reflected-object identity,
raw linear chroma, single-accounted composition, and spatial coverage rather
than relaxing this ratio until it passes.

### Phase 2G: global direct-light MIS ownership and object acceptance

The Phase 2F mirror-only exception was an incomplete boundary: ordinary
materials could still replace a BSDF-sampled direction with the point-light
direction while retaining the BSDF PDF and throughput. That behavior is now
removed globally. BSDF sampling owns only its sampled direction, PDF, and
throughput. Finite-light next-event estimation is a distinct branch at reached
path vertices, uses the material's direct-light response, and is combined with
the BSDF technique using the existing power-heuristic MIS weights.

The result contract now separates recursive direct-light radiance from
recursive BSDF radiance. Both high-chroma and low-chroma mirror probes export
`stochastic_direct`, `stochastic_bsdf`, `recursive_direct`, and
`recursive_bsdf` independently. Their sum, plus the disjoint local, ambient,
emission, transmission, and reflection terms, must reproduce the composed
linear pixel within `1e-6`. The reflection loop's direct and BSDF continuation
enters the host reflection exactly once; it is not also copied into the host's
generic recursive buckets.

Tests that previously asserted a recursive BSDF ray was steered into a point
light or emissive fixture now assert the physically meaningful split: the
independent BSDF ray may miss, while the light-sampled branch contributes and
records MIS ownership without claiming a geometry/emitter hit. This preserves
nested mirror recursion and disabled/depth-limited behavior without the former
hard-coded proposal shortcut.

The visual acceptance contract is now schema v2. It no longer gates on a ratio
of thresholded pixels from two different projections. It requires:

- reflected probe identity `smooth_subject`, material `2`,
  `vertex_interpolated` normal provenance, and path depth at least one;
- reflected blue/chroma spatial coverage of at least `4,000`/`8,000` pixels,
  with a blue-signal extent of at least `110` by `145` pixels;
- raw first-vertex, composed-linear, and final tone-mapped blue/red ratios of
  at least `1.15`, plus composed linear chroma of at least `1.0`;
- exact raw tone-map byte agreement and high-/low-chroma host energy sums
  within `1e-6`.

The fresh result passes: `4,808` blue pixels, `9,057` chromatic pixels, a
`122` by `158` blue-signal extent, raw blue/red ratios
`9.308`/`6.587`/`1.240`, and a maximum classified-composition residual of about
`1e-9`. The reflected/direct blue-pixel ratio remains reported as a diagnostic
(`0.661621`) but has no acceptance authority.

Each boundary should be independently reviewable and revertible. Do not mix
release metadata, package publication, unrelated editor work, or broad shared
library changes into these commits.

RayTracing can shade imported STL geometry with generated per-vertex normals
without changing the STL file format. STL still supplies tessellated positions;
`core_mesh_compile` welds indexed topology and emits optional normals in the
`mesh_asset_runtime_v1` sidecar.

## Runtime policy

- `Ng` is the world-space geometric face normal. Intersection orientation,
  sidedness, ray offsets, and self-intersection safety use `Ng`.
- `Ns` is the normalized barycentric interpolation of runtime vertex normals.
  Direct, metal, mirror, and BSDF evaluation use `Ns`.
- Missing or invalid optional normals fall back to `Ns = Ng`, preserving old
  position-only runtime assets.
- Non-uniform instance scale transforms normals with the inverse transpose.
- Set `RAY_TRACING_MESH_SHADING_MODE=flat` to reproduce face-normal shading.

A denser STL improves the actual silhouette. Interpolated `Ns` removes
face-normal highlight stepping, but cannot hide a visibly polygonal outline.
Mesh adjacency comes from welded indices and edge connectivity, not BVH
proximity queries.

## Deterministic fixtures and matrix

Generate and compile the bounded fixture ladder:

```sh
make smooth-mesh-runtime-compile-tool
make test-smooth-mesh-reflection-fixtures
```

Run the high-tier analytic sphere, icosphere, organic blob, and crease scene in
smooth/flat and TLAS/BLAS/flattened modes:

```sh
make test-smooth-mesh-reflection-matrix
```

Outputs are written under
`build/agent_runs/ray_tracing/smooth_mesh_reflection_matrix/`. The generated
`matrix_report.json` records asset normal provenance/counts, image hashes,
route mismatches, render timings, and the smooth-versus-flat timing delta.

The reusable fixture generator supports exact density tiers:

- `high`: 20,480 triangles;
- `very_high`: 327,680 triangles;
- `ultra`: 1,310,720 triangles.

Large tiers are pressure/visual gates and are intentionally excluded from the
routine smoke target. Generate them on demand with
`tools/smooth_mesh_reflection/generate_fixtures.py --tier ultra` and compile
the emitted authoring JSON with the `smooth-mesh-runtime-compile-tool` binary.

## Acceptance readback

A trustworthy close-up should show all of the following:

- a smooth silhouette at the chosen density;
- continuous mirror and rough-metal highlights on smooth fixtures;
- preserved hard edges on the crease fixture;
- different smooth and flat image hashes;
- zero route parity mismatches, traversal overflows, and silent fallbacks.

The high-tier matrix is the correctness/parity gate. For a human-readable
low-poly comparison, use a purpose-built small fixture as well: smooth shading
changes BRDF/reflection response but does not add silhouette geometry. The
current 120-triangle rounded-slab probe confirms distinct flat, smooth, and
60-degree crease-aware behavior. Its earlier fully smooth render exposed a dark
diagonal wedge because the perfect-specular path reflected directly around an
unconstrained interpolated `Ns`. The runtime now blends `Ns` toward `Ng` only as
far as necessary to keep the reflected ray in the geometric surface hemisphere;
the fresh smooth rerender removes that wedge, preserves smooth response, and
keeps the crease-aware hard-edge result distinct.

This acceptance is provisional until the policy is exercised on a real
imported asset compiled after the menu setting is chosen. Reopen the lane if a
close reflective view of that asset shows a diagonal dark wedge, a discontinuity
that tracks triangle boundaries, or loss of an intended hard crease. Capture the
asset sidecar, selected normal mode/crease angle, camera/light/material settings,
and a flat-versus-smooth-versus-crease-aware comparison before changing the
normal algorithm again.

Cold JSON load/normal payload cost must be reported separately from steady-state
render time. A same-scene unexplained render regression above 10 percent is a
review blocker for this lane.
