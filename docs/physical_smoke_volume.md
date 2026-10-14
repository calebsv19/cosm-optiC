# Physical smoke-volume settings (local source checkpoint)


Saved atmosphere attachments can opt into the same physical kernel through
`extensions.ray_tracing.authoring.atmosphere_attachment.optics`. Scene Editor's
**Environment > Optics** selector and MCP `atmosphere_set` edit this same retained
block, using the existing revision checks, Undo/Redo and File Save:

```json
"optics": {
  "profile": "physical_smoke_v1",
  "sampling_layout": "cell_centered",
  "reference_concentration_kg_m3": 0.0002,
  "mass_extinction_m2_kg": 1250,
  "scattering_albedo": 0.65,
  "anisotropy": 0.35,
  "sky_illumination": true,
  "sky_error_budget": 0
}
```

All fields except `sky_error_budget` are required. Concentration must be positive,
mass extinction nonnegative, albedo within 0..1, anisotropy within -0.95..0.95,
and the sky budget within 0..0.0001. Numeric fields require finite JSON numbers;
unknown keys, null blocks and unsupported vocabulary are refused. Physical
settings require `affects_lighting: true`; selecting the profile in the UI enables
it. Appearance exposes these coefficients, sky illumination, sampling layout,
integration step and sky budget. Artistic presets are unavailable in physical
mode; their retained settings remain available when switching back.

An omitted `optics` block retains the old scene behavior and is not added during
ordinary serialization. `legacy_artistic_v1` retains dormant physical coefficients
for switching back, but renders with the legacy artistic policy and node layout.
Field replacement preserves the optical block and placement. This is an opt-in
contract; existing scenes and VF3D bytes require no migration.

Saved physical settings support **Direct Light and Disney V2 with caustics off**.
Each prepared frame captures material and scattering tuning by value with its
volume attachment. Render queries read that frame's optical snapshot; subsequent
authoring changes cannot change prepared pixels. Static retained physical volumes
can use the existing responsive asynchronous route. Animated retained caches
still select synchronously to preserve the existing cache-selection lifecycle.

Disney V2 applies the existing extinction and single-scattering kernel to camera,
reflection, diffuse continuation, transmission and environment segments. Segment
composition uses incoming path weight times scattering, followed by extinction
of the outgoing path weight. Camera attenuation includes transmission and ambient
contributions once. Legacy artistic scenes preserve their previous transport.
This remains bounded single scattering; combined caustics, multiple scattering,
measured soot calibration and production visual acceptance remain outside this
checkpoint. Other integrators refuse an active physical profile.

Headless rendering consumes the saved block. A retained block owns optical
appearance and sampling: request-level smoke optics, sampling-layout and legacy
optical overrides (including step) are refused. Camera and lighting inspection
remain available. `inspection.saved_atmosphere_optics` in the summary reports the
saved policy; other inspection override fields continue to describe the request.
Portable scene snapshots carry the block and its existing contained cache assets.

The local acceptance runner is `tests/tools/run_atmosphere_optics.py`. Supply
`--native <workspace-visual-test> --cli <headless-cli> --out <fresh-test-root>`;
it copies fixtures before opening an isolated native editor. It verifies native
controls, active-edit/stale/typed refusals, shared history, save/reopen, real MCP,
exact Direct Light kernel parity, exact legacy parity, rendered coefficient
changes and a sealed relocated snapshot. It also checks saved/inspection Disney
V2 parity, both integrators' legacy pixels and Disney coefficient effects. To
reuse a previously verified saved fixture for renderer checks, supply
`--native-fixture <contained-fixture>`; UI/MCP acceptance then comes from that
fixture's original proof, rather than a new UI run.

`scene_editor_workspace_visual_test <isolated-program-root> <saved-physical-scene>
--atmosphere-snapshot` verifies prepared Direct Light/Disney V2 pixels after
mutating all global optical tuning, concurrent distinct-frame rendering and exact
legacy lookup parity. C tests cover analytic attenuation/scattering, independent
snapshot threads and reflection/transmission path weights. These local gates do
not qualify packages or workers.

## Request-level inspection profile

The headless request supports two additive opt-ins. Existing requests keep the
legacy sampling and artistic optical tuning.

```json
{
  "volume": {
    "sampling_layout": "cell_centered",
    "affects_lighting": true
  },
  "inspection": {
    "volume_smoke_optics": {
      "schema": "physical_smoke_v1",
      "reference_concentration_kg_m3": 0.0002,
      "mass_extinction_m2_kg": 1250,
      "scattering_albedo": 0.65,
      "anisotropy": 0.35,
      "sky_illumination": true
    }
  }
}
```

This is a fragment; scene, source, render and output fields are still required.
Use `direct_light` or `disney_v2` with caustics off. For Disney V2 set
`inspection.caustic_mode: "off"` and `inspection.caustic_sidecar_enabled: false`;
physical profiles also refuse photon render contributions. The physical profile rejects simultaneous
legacy density scale/gamma, opacity clamp, absorption gain, scatter gain or
RGB tint/albedo overrides. Step scale remains a numerical integration setting.
Numeric strings, booleans in numeric fields, unknown optical keys and invalid
albedo/anisotropy ranges are rejected. Summary inspection readback records the
layout and all physical parameters.

For cell-centred data the VF3D origin is the physical domain corner. Domain
bounds are `[origin, origin + dimensions * voxel_size)`. Samples are at
`origin + (index + 0.5) * voxel_size`, with constant extension across exterior
half-cells and zero outside the domain. This removes the previous need for a
header-shifted derived cache. The default `legacy_node` layout is unchanged.
Placement transforms apply before lookup, including the lower physical bound.
VF3D bytes and shared `core_pack` carrier APIs are unchanged.

Concentration is raw VF3D density times the explicit reference concentration.
Extinction is concentration times mass extinction, in inverse metres. Scattering
is extinction times scalar albedo; absorption is extinction times one minus
albedo. Camera and incident-light attenuation use the same extinction.
The single-scattering integral uses HG phase with explicit anisotropy and no
legacy 0.12 strength multiplier. This profile does not add smoke emission.

Sky illumination evaluates the existing environment background radiance along
12 fixed icosahedral directions and traces geometry and volume transmittance to
each direction. Discrete HG weights are normalized to preserve constant-sky
energy. The sky must use the ambient environment mode with nonzero background
brightness. Surface ambient strength alone does not supply incident sky light.
This is a bounded, approximate single-scattering quadrature. It is not a
converged angular integral or multiple-scattering solution. Point-light authored
intensity remains an existing renderer convention, not measured fire power.

The example optical parameters are candidate test values, not measured soot
properties. Their units and energy relationships are explicit; selecting them
does not establish empirical calibration or production visual acceptance.

Native `runtime_volume_3d` tests cover floor/centre/edge/placed sampling,
legacy parity, Beer–Lambert attenuation, thin-slab sky energy, anisotropy,
actual ground occlusion, pure absorption, sky opt-out/off and tuning restoration.
`tests/test_headless_smoke_request.py` covers typed request rejection. Existing
visibility and headless preflight/image-export fixtures remain required.

An optional `sky_error_budget` numeric field (default zero, maximum 0.0001)
sets a per-ray absolute RGB omission budget for sky scattering. The renderer
uses unoccluded incident sky as a conservative upper bound: geometry and medium
visibility cannot exceed one. It skips a segment only when its bound fits the
remaining budget, then subtracts that bound. Extinction, point lights and strong
sky contributions retain the full path. Native tests compare a thin slab against
the zero-budget reference and require total omitted radiance to stay within the
budget. This is an explicit integration approximation, not a density threshold.
Summary readback records it. A native 128x96 close-plume comparison at 0.00001
had maximum channel difference 1 and mean difference 0.000122 of an 8-bit value,
with about 101s reference versus 29s candidate elapsed time. Performance depends
on the field and scene. The bound is per ray, not an empirical smoke calibration
or an unconditional bound on the final image after every shading operation.
