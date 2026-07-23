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
```

Additional transport, map, estimator, medium-stack, provenance, and scene
population groups are indexed in [`../tests/README.md`](../tests/README.md).

## Current Boundary

This integration makes photon mapping usable and testable as an opt-in system;
it does not promote it to the production default. Operator visual acceptance,
quantitative optical certification, PVA-5, professional readiness, scene
pointer/latest-good promotion, release packaging, publication, and release
readiness remain independent gates. The reference caustic modes should remain
available until a separate deprecation decision is made and verified.
