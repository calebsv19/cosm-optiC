# Local physical smoke-volume candidate

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
Use `direct_light` with caustics off. The physical profile rejects simultaneous
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
