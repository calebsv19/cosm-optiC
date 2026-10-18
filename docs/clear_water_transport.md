# Imported clear-water optical contract

Imported water manifests describe a dielectric boundary and a homogeneous absorbing
medium. Water geometry owns the surface shape; authored roughness owns the
additional microscopic spread. This contract does not model foam, turbidity or
suspended-particle scattering.

The existing resolved material payload carries distinct quantities:

- neutral base/interface color and no diffuse lobe;
- full dielectric transmission eligibility, with reflection/transmission divided
  by Fresnel using the incident and destination IORs;
- bulk RGB transmittance at `absorption_distance_m`, encoded from the manifest's
  per-metre absorption coefficients as `exp(-coefficient * reference_distance)`;
- authored IOR and roughness.

Thus a segment of length `d` has transmittance `exp(-coefficient * d)`. Changing
the reference distance only changes the encoding; it does not change absorption.
No extra reference-distance color is applied at an interface. The hidden 0.92
water transparency/diffuse mixture has been removed from generated water.
The legacy manifest reflectivity field remains readable; it cannot impose a
second reflection floor on this physical water boundary. IOR owns that response.
Ordinary artistic glass and its explicit interface/absorption overrides retain
their separate material contract.

Resolve registered water after generic object/procedural color fallbacks, so those
fallbacks cannot overwrite its optical fields. Camera and photon transport use the
explicit neutral interface and bulk medium fields. The existing straight visibility
attenuation helper now reads explicit absorption color when provided. Visibility
remains a straight-path approximation; this change does not implement refractive
shadow connections or establish exact equivalence of all photon/camera estimators.

## Qualification

The material payload regression checks neutral interfaces, zero-distance and
zero-absorption limits, Beer–Lambert attenuation, segment additivity, reference
encoding invariance, authored roughness and IOR-derived reflectance. The photon
policy checks the absence of diffuse response, neutral interface tint and unit
reflection-plus-transmission weight for clear water.

Use matched native headless renders from unchanged imported scenes to qualify
appearance separately. Keep seed, camera, geometry, lights, sampling, denoising
and photon controls equal between before/after cases. Inspect raw output before
filtered output. A completed render and passing component tests do not by
themselves establish high-resolution water-highlight, caustic or animation
acceptance. No scene-schema migration or published-package change is implied.

The reusable native import gate is
`tests/integration/run_clear_water_optical_contract.py --cli <headless-cli>
--request <existing-water-request.json> --output-root <fresh-output-root>`.
It preserves its input request and verifies resolved fields against the imported
manifest through the actual CLI. `tint_rgb` remains the base/interface readback;
`absorption_transmittance_rgb` and `diffuse_weight` expose the distinct quantities.

## Ownership and editing map

Water has no separate ad hoc reflection formula. The following boundaries keep
geometry, medium state, interface scattering and lighting independently editable:

| Concern | Owner | Contract |
| --- | --- | --- |
| Imported heights and closed-body geometry | `runtime_water_body_prepare_3d.c`, `runtime_water_body_mesh_3d.c` | Prepared surface shape and outward geometric normals; no lighting weights. |
| Smooth top normals | `runtime_heightfield_normals_3d.c` | Optical shading normal; does not replace geometric medium orientation. |
| Water material | `materials/runtime_water_material_3d.c` | Neutral interface, authored IOR/roughness, separate bulk absorption. |
| Medium membership and absorption | `runtime_caustic_photon_medium_stack_3d.c` | Ordered incident/destination media and one attenuation per segment; reused by camera and photons. |
| Boundary optics | `runtime_dielectric_transport_3d.c` | Exact unpolarized Fresnel, Snell direction and TIR from the ordered IOR pair. |
| Camera scattering | `runtime_physical_glass_bsdf_3d.c`, `runtime_glass_microsurface_3d.c` | Sample the dielectric surface; radiance transmission includes the eta-squared conversion. |
| Camera path orchestration | `runtime_physical_glass_3d.c` | Reflection/refraction continuation, receiver/emitter arrival, absorption and compensated termination. |
| Photon branch policy | `runtime_caustic_photon_bsdf_policy_3d.c` | `BuildInterface` consumes a resolved physical pair; `Build` is the separate compatibility/artistic policy. |
| Photon path orchestration | `runtime_caustic_photon_path_transport_3d.c` | Resolve medium before selecting a branch, apply policy, propagate photon flux, record transitions. |
| Actual source hits | `runtime_light_emitter_3d.c` | Geometry/emitter first-hit ordering and emitted radiance. |

### Physical photon interface repair

Physical solid dielectrics, including imported clear water, now use
`RuntimeCausticPhotonBsdfPolicy3D_BuildInterface` in path transport. Its required
inputs include `eta_from` and `eta_to`; it never silently substitutes air.
The prior branch chooser used an air/material Schlick approximation even after
the medium stack had resolved a different pair for refraction. The new policy
uses the same exact Fresnel function as the physical dielectric solver, ignores
an artistic reflection floor, and records `usesResolvedInterface`, `etaFrom` and
`etaTo` in each policy stored with the photon hit event. An unresolved solid
interface is rejected before either branch can conceal the medium error.

Reflection/transmission probabilities are separate from selected throughput.
Clear neutral water conserves expected photon flux at the interface; do not
apply the camera's eta-squared radiance factor to that flux. Authored interface
tint/loss and distance-dependent bulk absorption remain separate. TIR retains
its existing explicit event and unchanged-medium bookkeeping. Nonphysical and
thin-walled compatibility paths retain their previous policy.

Tests cover independently tabulated air/water and water/glass Fresnel values in
both directions, grazing and equal-index limits, TIR, sampled branch frequency,
flux conservation, invalid-pair refusal, actual nested-media path replay, and
inside-solid initial-medium requirements. This repair does **not** unify the
photon rough-direction approximation with the camera's full microsurface sampler.
That remains an explicit future estimator boundary, rather than an implied
camera/photon equivalence claim.

### Reading reflection evidence

The camera result field `primaryTransmissionRadiance` currently contains the
combined physical dielectric continuation result, including reflected paths.
It is an integration field, not a transmission-only measurement. Use actual
sampled events and emitter/receiver arrivals to evaluate reflection. Likewise,
a legacy local `specular` term is not proof that a reflected path hit a source.
A failed continuation must not be interpreted as an extra bright surface lobe.

### Explicit water/container contact

`water_body_boundary_v1.optical_contact` accepts `"separated"` (the default when
absent) or `"wet_container"`. `container_id` alone never changes optical topology.
Unknown values, non-string values, and wet contact without the full boundary
contract fail import. Author wet contact only when the water should touch glass;
a real air film must retain the separated policy.

The initial supported contact geometry is deliberately bounded: an axis-aligned
rectangular container with four walls and a bottom, each inner/outer planar face
represented by two rectangle triangles. The named container must resolve to one
scene object with explicit solid-glass transport. The five water closure planes
must lie within 10 mm of the corresponding inner faces. Wall coverage and at least
0.4 mm thickness are checked, with an outer-face search limited to 0.5 m. Rotated,
curved, differently tessellated, incomplete, or unsupported containers fail with
a diagnostic naming the failed check and face (min/max X, min/max Y, bottom Z).
This is not a general mesh contact or arbitrary overlap solver.

`runtime_water_contact_3d.c` validates the geometry and adjusts the generated
water closure to overlap glass by 0.1 mm. Imported height values and material
properties are retained; lateral sample spacing extends to the wet walls and the
bottom moves to the contact plane. This is a numerical contact approximation:
the physical interface is displaced 0.1 mm into the glass, and that sliver receives
water absorption. It is larger than the standard camera/photon ray offsets.
Nonstandard offsets that can skip the overlap are not qualified.

The material payload carries the named container index by value into camera and
photon medium entries. Water takes precedence over that container only:

| Crossing | Active-medium change | Optical event |
| --- | --- | --- |
| Container entry while inside its water | Insert container below water | Null: water → water |
| Water exit into the overlap | Remove water, expose container | Physical: water → glass |
| Water entry from the container | Push water above container | Physical: glass → water |
| Container exit while still inside its water | Remove container below water | Null: water → water |

Null contact faces update membership and retain segment absorption but add no
reflection, refraction, tint, opacity, surface emission, roughness or scattering
depth. Camera paths preserve straight-ray parameterization; photon paths preserve
flux and optical depth. Reflections retain membership. The exposed water top
still resolves to air, and glass above the water still resolves normally.
Unrelated exit mismatches and duplicate entries fail closed. Photon hit records
mark `nullContactBoundary` separately from dielectric scattering events. The
24-entry hit ledger is separate from the eight-event physical scattering budget;
map population accepts that larger ledger without increasing optical depth.
Null records retain segment flux/PDF for beam extraction.

Ownership for future edits:

- `src/import/water_body_boundary_contract.c`: authored policy and import refusal.
- `src/render/runtime_water_contact_3d.c`: supported shape, wall matching,
  clearance/overlap tolerances and generated closure coordinates.
- `src/render/runtime_water_body_prepare_3d.c`: transactional mesh preparation
  and publishing the contact partner into the water material override.
- `src/render/runtime_caustic_photon_medium_stack_3d.c`: shared camera/photon
  membership and incident/destination IOR resolution.
- `src/render/runtime_physical_glass_3d.c` and
  `src/render/runtime_caustic_photon_path_transport_3d.c`: null-face continuation;
  physical interfaces retain their existing estimators.

Headless `water_surface.water_body` readback reports `optical_contact` and
`generated_contact_bounds_scene_m` as `[minX, maxX, minY, maxY, bottomZ]`, alongside
the unchanged authored bounds and topology checks. Tests cover reciprocal stack
and actual photon traversal, water/air TIR versus water/glass transmission,
null camera continuation with deliberately tinted/rough/partly opaque glass,
absorption distance, depth, parser defaults/refusals and malformed wall geometry.

This repair does not unify camera/photon rough-surface estimators, implement
arbitrary origin classification inside overlapping solids, change straight-shadow
approximations, tune light energy, or qualify the final Aquarium appearance.

A retained Aquarium diagnostic exposes a secondary-origin limit: a secondary ray
starting less than a micrometre below the tank's outer bottom can skip its entry
face at the numerical ray-start tolerance. The subsequent container exit is rejected for missing membership. This is
a fail-closed boundary-origin case, not permission to infer glass whenever a
water exit is near a wall; origin classification remains separate from contact.
