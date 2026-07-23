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
These diagnostics expose receiver position, identity, flux, support, query
radius, contributing sample counts, and rejection counters without changing
the rendered result.

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

## Current Boundary

This integration makes photon mapping usable and testable as an opt-in system;
it does not promote it to the production default. Operator visual acceptance,
quantitative optical certification, PVA-5, professional readiness, scene
pointer/latest-good promotion, release packaging, publication, and release
readiness remain independent gates. The reference caustic modes should remain
available until a separate deprecation decision is made and verified.
