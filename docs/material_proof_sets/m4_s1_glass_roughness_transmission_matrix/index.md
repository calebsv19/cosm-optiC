# M4-S1 Glass Roughness And Transmission Matrix

This package is the first real M4 proof package using the S0 contract.

- request: `request.json`
- preview request: `material_preview_request.json`
- generated preview summary: `material_preview_summary.json`
- proof summary: `summary.json`
- rendered preview/contact sheet: `preview.bmp`
- route: `headless_material_preview`
- source fixture:
  `../../../tests/fixtures/disney_v2_visual_matrix/transparent_interior_stack/scene_runtime.json`
- target object: `glass_front`

## Row Order

Contact-sheet cells are ordered left-to-right, top-to-bottom.

1. `r00_smooth_alpha085`
2. `r01_low_alpha085`
3. `r02_soft_alpha085`
4. `r03_mid_alpha085`
5. `r04_rough_alpha085`
6. `r05_frosted_alpha085`
7. `t00_alpha100`
8. `t01_alpha075`

## Readback

The first six rows sweep roughness from smooth to frosted while holding display
alpha at `0.85`, which maps through the current compatibility bridge to
transmission weight `0.6375`.

The last two rows hold roughness at `0.18` and compare display alpha `1.0`
against `0.75`, showing the current alpha/transparency compatibility bridge
without promoting physical opacity. The existing preview route emitted an
8-cell contact sheet, so S1 uses this as the similarly bounded matrix allowed
by the plan.

The source transparent preset uses IOR `1.45`, so the derived dielectric F0 is
`0.03373594335693461`. The row reflectivity floor is `0.02`, so it does not
raise the IOR-derived F0.

## Deferred Status

This preview package is not a full physical glass transport proof. Caustics,
focal distortion, absorption color/density, explicit authored dielectric F0,
and physical opacity/cutout ownership remain deferred or qualitative.
