# Photon Mapping

Photon mapping is available as an experimental, explicit opt-in for native
`3D` rendering. It is off by default. The existing reference caustic modes are
still supported and have not been deprecated.

## Desktop Control

Open the Render workspace and select the caustic/light-transport controls. The
product selector cycles through four mutually exclusive choices:

- `Off`: disables both reference caustics and photon-map population.
- `Reference Analytic`: retains the analytic reference caustic path.
- `Reference Transport`: retains the exploratory lens-transport path.
- `Photon Map (Experimental)`: enables photon-map population and contribution
  during native render preparation.

The existing persisted controls are shared by the selected transport product:

- Surface Cache and Volume Cache choose which photon queries are populated.
- Photon / Path Budget controls the bounded population budget.
- Caustic Path Depth controls maximum photon-path depth.
- Surface Radiance Scale controls the displayed surface contribution scale.

Selecting a reference mode does not leave photon population enabled. Selecting
`Off` also clears the surface/volume cache toggles. Ordinary saved defaults
remain off.

## Headless Requests

Headless native `3D` requests can select the production route with
`inspection.caustic_product_mode = "production"` (or `"photon_map"`) and the
photon transport engine with
`inspection.caustic_transport_engine = "photon_map"`. Population,
contribution, cache queries, and diagnostic readback remain explicit request
fields rather than implied defaults. See
[`headless_agent_render_cli.md`](headless_agent_render_cli.md) and the accepted
field parsing in `src/app/agent_render_request.c` before authoring a request.

For an opaque receiver beneath a single open refractive water surface, set
`inspection.caustic_photon_surface_allow_active_medium_receiver = true`. This
request-only switch accepts the floor hit after one dielectric entry without a
matching exit; it remains false by default so closed solid-dielectric
provenance continues to require reconciled entry/exit counts.

When render-prep population and
`inspection.caustic_photon_surface_diagnostics_enabled` are both active, the
headless renderer writes per-frame `photon_surface_records_NNNN.jsonl` landing
records and `photon_surface_queries_NNNN.jsonl` direct-consumer reconstruction samples.
Landing records expose receiver identity, flux and prepared support. The query
file currently contains an 80-by-80 reconstruction probe on the first landing's
XY plane and identity. It replaces the captured query sample list after rendering;
it is **not** a list of camera pixels or a wall-wide query map.

The same opt-in writer saves `photon_surface_query_aggregate_NNNN.json` before
those synthetic probes reset the consumer counters. It contains query counts,
positive/undersampled-positive counts and an effective-support histogram
(0, 1, 2, 3, 4 or more). These are query calls since the last consumer bind/reset,
not unique pixels or guaranteed full-frame coverage; temporal preparation can
rebind the consumer. Preparation-only capture can legitimately report zero
queries. This snapshot reuses constant-size counters and adds no frame buffers.

### Live receiver accounting

The same diagnostic opt-in also preserves `photon_surface_live_queries_NNNN.jsonl`
before the compatibility grid runs. This bounded list contains up to 8192 actual
consumer calls, not unique camera pixels. It may include recursive or temporal
queries and is empty for preparation-only captures without shading queries.

`photon_receiver_accounting_NNNN.json` groups all calls since the last consumer
bind by object and material in a fixed 64-receiver table (8208 bytes). It records
support, positive and undersampled call counts, irradiance-density sums and
receiver radiance before camera throughput. Overflow is explicit. Sums are
call-weighted observations, not spatially integrated power or final image energy.
Accounting processing is disabled unless the diagnostic request is active; no
per-pixel or full-frame guide arrays are added.

Surface packet flux is already emission/PDF compensated. Reconstruction applies
a normalized area kernel, then the receiver diffuse response once. An undersampled
sample-centered kernel expands its bounded support rather than multiplying its
power by a confidence factor. Camera throughput includes radiance IOR factors
and sampling compensation and is therefore allowed above one; transmitted
caustics preserve finite nonnegative weights rather than clamping them to one.

Legacy `intensity` and an explicit `photon_emission_energy` are separate authored
controls. Gain one does not establish direct/photon radiometric equivalence when
those controls differ. Use landing-flux sums for stored power accounting and
physical-radiance source fixtures for calibration; do not integrate query-call
sums as if they were receiver area measurements.

### Preparation-only path capture

To separate photon transport from image reconstruction, use the local headless
CLI with a direct-map request whose render-prep population, contribution, surface
query and surface diagnostics are enabled:

```bash
<renderer> --request <request.json> --preflight \
  --photon-prepare-diagnostics --photon-path-ids 108,294,509
```

This prepares the scene and photon map without rendering a beauty frame. It adds:

- `photon_transport_paths_NNNN.jsonl`: the actual emission and every traced hit
  for the selected IDs, including object/material identity, BSDF lobe, PDF,
  throughput, medium transitions and termination. Capture observes the existing
  scheduler; it does not retrace paths or change random sampling.
- `photon_selected_receiver_queries_NNNN.jsonl`: direct-consumer probes at stored
  landings for those IDs, with the landing normal and receiver identity, physical
  flux, effective sample count and undersampling status. These are landing probes,
  not camera-pixel samples; their synthetic hits do not carry interpolated UV or
  procedural surface payloads, so the reported radiance is diagnostic.
- The existing landing and XY-plane probe files described above.
- `photon_termination_aggregate_NNNN.json`: counts all observed path endings,
  the requested depth, fixed event capacity and effective supported depth,
  and the count of successfully traced paths with unmatched entry/exit totals.
- `photon_unmatched_paths_NNNN.jsonl`: the first 64 unmatched successful paths,
  including their photon IDs, terminal reasons and ordered object/medium events.
  The aggregate reports omitted exports; this bounded list is not the full
  population when more than 64 paths qualify. A roulette or depth cutoff inside
  a medium can legitimately leave unmatched totals. Stored receiver acceptance
  still requires reconciliation at that receiver.

The ID list must contain 1–64 distinct unsigned integers. Every requested ID must
be observed or the command fails. The mode rejects `--render`, missing photon
population/contribution, and incompatible consumers. Existing diagnostic files
are refused unless `output.overwrite` is true. Paths stream to disk one at a time;
no per-pixel guides or full-frame buffers are added. The observer is scoped to
single-threaded preparation and cleared before image work or teardown.

Selected path rows also include the final numeric `reject_reason`. Photon path
transport treats requested depth as a ceiling and stops normally at the smaller
of that ceiling and the fixed hit-event capacity (currently eight). The cutoff
reports `max_depth`, preserving earlier events for the existing receiver filter;
it does not attempt a ninth hit and discard the whole path as `trace_error`.
This does not provide ten-bounce transport merely because a request specifies
ten, and it does not enlarge event arrays or allocate additional frame buffers.

A fixed photon map can leave stable, undersampled illumination dots that temporal
camera samples and a variance-guided image denoiser cannot reliably distinguish
from real detail. Compare gain 1 against photon-off before changing transport,
then inspect sparse support separately from image noise. A diagnostic gain such
as 500 amplifies the estimator; it is not a physically calibrated material preset.

For a material-guided raw/denoised image comparison, use at least four camera
samples. Below that confidence floor, the stage can report `denoise.applied=true`
while preserving every raw pixel. Require a positive
`render_stats.denoise_reconstructed_pixel_count` and compare the actual images;
the stage flag alone does not qualify filtering. Keep camera sample counts equal
within each pair, and label photon-contribution and denoise comparisons separately
when they use different sample budgets.

### Clear Solid Interfaces and Sparse Surface Reconstruction

Photon BSDF policies for explicitly configured solid glass and runtime water
exclude the inherited legacy Lambertian lobe. Fresnel reflection and authored
transmission loss form separate branches. Bulk color attenuation is evaluated
by the medium stack over travelled distance; it is no longer multiplied into
each clear-solid interface as well. An explicitly authored interface tint is
still honored. Thin-walled and legacy materials retain their mixed-lobe policy.
The expected-energy ledger uses raw branch weights, including interface loss,
rather than normalized selection probabilities. This is a photon-path change;
it does not replace the camera integrator's water/glass material model.

The sample-centered `k_nearest` consumer now uses
`caustic_photon_surface_gather_max_radius` as its maximum query footprint.
Deposits with fewer than the minimum effective samples in their prepared local
support use that bounded footprint. Dense deposits retain their prepared local
support. Each kernel remains normalized over its disk, preserving deposited
energy while lowering isolated peaks. Receiver identity, material, normal and
incident-hemisphere rejection remain active. Undersampling counters still report
the actual available deposits; wider support does not establish convergence.
No per-pixel or full-frame buffers are introduced. Keep the maximum radius
constant across a comparison and inspect both wall speckles and focused caustics
before selecting a scene preset; increasing support trades spatial detail for
lower reconstruction variance.

### Dynamic Water Boundary Identity

Imported dynamic water uses one dielectric-volume identity across its authored
side/bottom shell and the frame-selected heightfield surface. The dynamic-water
BVH stores only the changing surface subset, but traced hits are remapped to
prepared-scene global triangle indices before material or medium resolution.
Photon transport validates that the global triangle's primitive and local
triangle identities match the hit before consuming its geometric normal, with
an identity scan as a defensive fallback.

These invariants prevent a cached heightfield hit from borrowing the geometric
normal of an unrelated prepared-scene triangle and prevent the top surface from
being treated as a second nested water object. The headless population summary
includes medium-transition failure reason, depth, direction, normal, object,
and primitive counts for diagnosing future boundary regressions.

## Implementation Map

- `src/ui/menu/menu_caustic_product.c` owns desktop product selection and the
  settings-to-runtime plan.
- `src/render/runtime_native_3d_render_photon_prepare.c` owns guarded native
  render-prep population and contribution.
- `src/render/runtime_caustic_photon_integration_3d.c` and the neighboring
  `runtime_caustic_photon_*` modules own the product adapter, transport,
  storage, estimators, lifecycle, and readback.
- `src/render/runtime_caustic_photon_scene_descriptor_3d.c` bridges eligible
  ordinary runtime-mesh dielectric objects into photon preparation.
- `src/render/runtime_dynamic_geometry_accel_3d.c` preserves prepared-scene
  triangle identity when the dynamic-water cache traces its subset BVH.
- `src/render/runtime_caustic_photon_path_transport_3d.c` resolves validated
  geometric normals and owns medium-transition continuation.
- `tests/test_ui_menu_contracts.c` proves the desktop selector and runtime-plan
  contract. `tests/README.md` lists the focused photon subsystem groups.

## Focused Verification

From the repository root, run the most useful registered integration groups:

```bash
TEST_RUNNER_GROUP=ui_menu_contracts make test
TEST_RUNNER_GROUP=runtime_caustic_photon_integration_3d make test
TEST_RUNNER_GROUP=runtime_caustic_photon_direct_consumer_3d make test
TEST_RUNNER_GROUP=runtime_caustic_photon_sparse_brick_cache_3d make test
make test-ray-tracing-animated-water-photon-caustics
```

Additional transport, map, estimator, medium-stack, provenance, and scene
population groups are indexed in [`../tests/README.md`](../tests/README.md).
The animated-water target builds a deterministic 96-by-96 imported heightfield,
uses an authored rectangle light and near-overhead inspection camera, renders a
static contribution control and an eight-frame moving-water sequence, and
produces raw-landing, reconstructed floor-light, and beauty-image review
artifacts under `build/agent_runs/ray_tracing/`. Pass `--water-manifest` to its
Python driver to retarget a compatible local
`physics_sim_water_manifest_v1` sequence into the same bounded 4-by-2-meter
floor fixture without changing the source cache. Pass `--capillary-review` to
resample that macro surface to a 144-by-144 review mesh and add a deterministic
multi-direction capillary spectrum. This review-only hybrid is useful for
checking whether the photon optics can form pool-like cellular ridges; it is
explicitly not a claim that the added detail came from PhysicsSim or that it is
the final production water model.

For the bounded motion review, run
`tests/integration/run_ray_tracing_pool_caustic_temporal_review.py` with an
explicit `--water-manifest`. It uses eight contiguous PhysicsSim samples,
advances the capillary modes with a shared dispersion-based clock, and retains
the distant-light/tight-gather static reference. The review emits beauty,
raw-landing, and reconstructed floor-light contact sheets and videos plus
pairwise spatial continuity metrics. It is local diagnostic proof and remains
promotion-ineligible.

Before scheduling the expensive static quality comparison, use
`tests/integration/plan_ray_tracing_pool_caustic_quality_sweep.py` with an
explicit PhysicsSim `--water-manifest`. The planner generates 72-, 96-, 144-,
192-, and 256-square samples of the same water phase, freezes the accepted
distant-light/photon/gather/camera settings, and runs headless preflight for
each cell without rendering frames. Its `quality_sweep_plan.json` is the
auditable input manifest for the later local or worker-backed still sweep; the
generated request paths are payload-relative so the frozen sweep can move
between the Mac producer root and a worker package without path rewriting.

### Photon Quality Presets

Headless photon-map requests can select a bounded preset through
`inspection.caustic_photon_quality`:

- `preview` or `low`: 32,768 photons for fast composition and caustic-placement
  checks.
- `working`, `default`, or `inspection`: 131,072 photons for the normal
  working-quality baseline.
- `hero`, `production`, or `final`: 262,144 photons for production stills and
  deeper temporal review.

The canonical readback labels remain `preview`, `inspection`, and `final`.
Explicit request fields such as `caustic_photon_sample_budget`, path depth,
estimator, gather radius, and neighbor count are parsed after the preset and
therefore remain authoritative overrides. Photon-map rendering itself remains
opt-in/default-off; “default” here means the selected photon-map working preset,
not that every ordinary render silently builds a 131K map.

### Animated Water Contract

Dynamic water geometry is part of the photon-map lifecycle identity. A new
heightfield frame therefore rebuilds the map even when the static scene,
emitter, and photon settings are unchanged. The water importer also recognizes
the explicit `water_body_boundary_v1` heightfield-volume contract: dry perimeter
samples become a fixed-height rim and the animated top is attached to the
declared side/bottom shell object as one outward-facing medium boundary.
Interior dry samples remain cutouts.

The optional `dynamic_heightfield_volume` closure removes the fixed-rim visual
constraint. With `dry_sample_policy=extend_interior_to_boundary` and a finite
`bottom_height_m`, dry sentinel samples on the outer grid inherit the adjacent
interior height. RayTracing then builds the animated top, four perimeter wall
strips, and bottom cap as one outward-facing primitive and one water-medium
identity. This mode must not also instance the legacy static side/bottom shell.

For aquarium payloads, the heightfield footprint should be derived from the
actual glass mesh's inner wall planes rather than an independently estimated
cavity size. Retarget `sample_origin_*` and `sample_spacing_*` so the final grid
lands just inside those planes with a small positive ray-safe clearance; do not
make the water and glass coplanar. This footprint retarget changes only sample
positions. It must preserve the authored height and normal arrays so water
motion and optical-shape comparisons remain matched. The current collimated-sun
working proof uses a `0.001 m` clearance from each measured inner wall.

A one-sided `rect` or `disk` using legacy-intensity radiometry is treated as a
collimated photon emitter. Its origins remain distributed over the authored
area, its directions remain aligned with the authored normal, and the photon
path scheduler does not replace those directions with lens-guided proposals.
This is the current bounded sun-proxy lane; Lambertian-radiance area lights and
omnidirectional point/sphere lights retain their existing angular behavior.
`intensity` continues to scale direct-light shading. An authored legacy light
may additionally provide `photon_emission_energy` to scale photon-source
selection and emitted photon flux independently. When the field is absent,
photon emission falls back to `intensity`, preserving existing scene behavior;
an explicitly authored zero disables photon emission without disabling direct
illumination. This is particularly useful for footprint-wide collimated
rectangle or disk sun proxies whose direct and photon energy need different
calibration.

Authored rectangular direct lights use the complete deterministic 16-position
area population for each shading evaluation. They do not use the generic
four-sample visibility early exit: switching between four and eight samples on
partially transmitting aquarium glass produced coherent bands and an abrupt
band-to-speckle boundary. Compatibility sphere/point lights and bounded
material-emitter rectangle budgets retain their existing adaptive policies.

The current aquarium collimated-sun working profile keeps these fields matched
for follow-up proofs: 131,072 photons, 12 transmission samples, direct intensity
`9.0`, photon emission energy `0.00135`, the full authored-rectangle 16-position
direct-light population, and no authored-rectangle adaptive visibility exit.
This is a scene-proof baseline, not a global opt-in change for ordinary renders.

For local motion review, first render two adjacent low-photon frames and reject
the run if the water-to-shell rim separates, the dynamic geometry key does not
change, or the caustic pattern is static. Then use a 12- or 16-frame,
24-fps review at 131,072 photons with 12 transmission samples and denoising
disabled. Keep camera, light, estimator, and seed fixed so only the animated
water changes. After motion coherence is accepted, render four selected frames
at 262,144 photons for hero-quality comparison; do not pay the hero cost across
the full clip until that comparison shows a visible benefit.

## Current Boundary

This integration makes photon mapping usable and testable as an opt-in system;
it does not promote it to the production default. Operator visual acceptance,
quantitative optical certification, PVA-5, professional readiness, scene
pointer/latest-good promotion, release packaging, publication, and release
readiness remain independent gates. The reference caustic modes should remain
available until a separate deprecation decision is made and verified.
