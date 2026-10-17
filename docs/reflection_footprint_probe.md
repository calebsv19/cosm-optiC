# Selected-pixel reflection footprint observation

The opt-in native Disney V2 observer records receiver identity, material response,
ray differentials, receiver footprint and base-color texture filtering. It uses
existing pixel selection and does not alter transport, depth, sampling or output.
Enable it before starting an instrumented process:

```sh
RAY_TRACING_NATIVE_3D_PIXEL_PROBE='66,135;177,133;119,129' \
RAY_TRACING_REFLECTION_FOOTPRINT_PROBE=1 <renderer> <normal arguments>
```

Selection is capped at 64 coordinates. `REFLECTION_FOOTPRINT` JSONL is emitted to
stderr in a thread-local primary-shading scope. Each selected call/subpass has
monotonically numbered events, capped at 256, followed by an `end` record with
`events` and `dropped`. Nonzero drops mean the trace is incomplete. With the new
flag absent, no new records or diagnostic texture reads are performed.

Stages are `hit`, `arrive`, `receiver`, `spawn` and `base_color_filter`. Branches
identify the dedicated first reflection, its local material evaluation, the
sampled primary continuation, and recursive continuations. Events identify native
object and triangle indices; map these through the retained scene/summary. Scope
coordinates, sampling sequence and subpass associate the record with existing
`PIXEL_PROBE` primary-shade output. Event order distinguishes separate estimator
visits; repeated material evaluations are not unique rays or independent samples.

For the polished mirror path, a hit/receiver depth labels the arriving reflection
and a spawn depth labels the outgoing continuation. These are callsite path-depth
labels, not a new lobe-counter contract. This observer is not a complete glass,
photon, visibility or arbitrary nested-estimator path transcript.

Receiver fields include bounded/unbounded and transported status, incident
and pixel differentials, constant-normal eligibility, position, material id,
resolved roughness and filtered albedo where available. Outgoing ray flags are
separate from the source hit: a bounded source hit can spawn an unbounded ray.

The base-color image sampler records UV, UV differentials, image dimensions,
actual selected mip level and sampled linear RGB. A separate mip-zero point
lookup at the same UV is recorded as `point_reference`. It is a diagnostic read
only; it is never used by the material or rendered image. It is not a proposed
unfiltered rendering policy or a correct replacement for a transported footprint.
The existing transported-footprint four-tap sampler is observed without changing
its tap positions, weighting, output or LOD semantics.

## Original bounded observation and diagnosis

The saved three-mirror fixture was replayed at 256 square and eight passes with
12 selected pixels. Observer-off, observer-on and the retained pre-instrumentation
M1 image are byte exact. Thread-local scope/event ordering, finite records and
zero dropped events pass; existing recursive mirror fidelity acceptance is a
separate gate. The installed desktop package is not refreshed by this change.

In this fixture, the dedicated A-to-B ray arrives with a bounded transported
footprint. The recursive `RuntimeRay3D_MakeOffset` spawn at B loses differentials
and marks the outgoing ray unbounded. Later receivers inherit that state. The
existing conservative texture policy substitutes UV differentials spanning the
whole texture, selecting mip 9 (the 1-by-1 average) for a 512-square base-color
image. UV still varies and point-reference color varies, while the actual
filtered texture color is effectively constant. The sampled primary branch also
starts with an unbounded offset ray.

The original observation identified the detail-loss mechanism without repairing
transport. Its image/parity receipts remain historical baseline evidence. The
subsequent bounded repair is described below; the sampled cone-rotation helper
is not used as a substitute for the GGX sampler contract.

## Smooth planar GGX continuation contract

`RuntimeSpecularBSDF3D_TransportFootprint` is called after the existing offset-ray
construction for opaque sampled-primary and recursive specular continuations.
For the current NDF sampler and a constant eligible normal, fixed u/v selects a
half-vector independent of view. The helper reconstructs this half-vector from
the central view and sampled outgoing direction, and reflects both incident
camera neighbors around it. Their origins use the existing hit-position
footprint and the central offset. The actual central ray is never replaced.
This is different from rotating an ideal cone footprint toward a rough sample.

The helper requires valid opaque payload/BSDF, resolved roughness no greater
than0.08, no transmission or explicit glass override, no authored/microdetail
normal response, constant normal, no curve tangent, bounded incident/pixel
differentials, finite data and valid reflection hemispheres. Existing ideal
footprint guards are reused. Unsupported inputs clear stale differential fields
and retain the conservative unbounded policy. Diffuse and glass transport,
rough-cone estimators, global mips, scene/UI/MCP schemas and transport estimators
are unchanged. Corrected receiver filtering naturally changes material color and
resulting radiance; no downstream brightness equality is imposed.

Independent tests rerun the real specular sampler with the same u/v and neighboring
view directions at roughness0.02 and0.08. They verify both neighbor directions,
central origin/direction preservation, sampler/PDF/weight repeatability and
refusal/fallback families. Existing lighting/material groups and unchanged
RT-MIRROR-1 acceptance pass. Full unrelated GUI/worker/package qualification is
outside this headless slice.

Twelve matched native controls include depth1/2/3/4/8 at256 square/8passes,
depth4/8 and direct/single-mirror controls at512 square/32passes, denoise off.
Repaired replay is byte exact; all three direct references remain byte exact
against the original renderer. At the selected qualifying chain, two/three
reflections change receiver mip9 to approximately0.643/1.017-1.042, and filtered
texture color varies again. The96primary scopes have zero dropped events and
observer on/off is byte exact. Unsupported alternate visits remain explicitly
reported; this does not qualify every branch or curved/rough/normal-mapped mirror.

Display line-pattern contrast and depth4/8 differences are detail diagnostics,
not an energy-conservation or infinite-depth convergence test. The direct
unfolded views match projection/handedness but differ in lighting/occlusion.
The installed Desktop app is not refreshed by these headless qualification
receipts; the rough-glass checkpoint, existing videos and saved scenes remain
preserved. Main Edit repair source is uncommitted over that checkpoint.

## Smooth triangle reflection qualification

The shared opaque reflection transport now includes the affine world-space vertex-normal field for eligible smooth triangles. A `constant_normal=0` observation can therefore carry a bounded outgoing footprint. Use the outgoing/receiver flags and actual filter mip to assess it; constant-normal status alone no longer denotes refusal. Glass, curves, perturbed/clamped normals and unsupported samples keep their existing eligibility boundaries.
