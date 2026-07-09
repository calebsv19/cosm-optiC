# M4-S2 Reflectivity, Specular, F0, And IOR Matrix

This package proves the current M2/M3 distinction between reflectivity
compatibility floor, specular lobe weight, IOR-derived dielectric F0, and
metallic-deferred behavior.

- request: `request.json`
- preview request: `material_preview_request.json`
- generated preview summary: `material_preview_summary.json`
- proof summary: `summary.json`
- rendered preview/contact sheet: `preview.bmp`
- route: `headless_material_preview` plus source-backed numeric BSDF contract
  readback
- source fixture:
  `../../../tests/fixtures/disney_v2_visual_matrix/transparent_interior_stack/scene_runtime.json`
- target object: `rear_blue_panel`

## Row Order

Contact-sheet cells are ordered left-to-right, top-to-bottom.

1. `ior100_ref000_sp025`
2. `ior133_ref0015_sp025`
3. `ior133_ref018_sp025`
4. `ior145_ref002_sp025`
5. `ior180_ref002_sp025`
6. `ior145_ref004_sp010`
7. `ior145_ref004_sp080`
8. `ior180_ref060_sp025`

## Readback

The preview image shows the reflectivity-floor portion of the matrix. The
current preview request schema does not expose IOR or specular weight controls,
so those values are numeric BSDF-contract readback in `summary.json`.

The rows prove these contract points:

- IOR-derived dielectric F0 is computed from `((ior - 1) / (ior + 1))^2`.
- Reflectivity remains a legacy compatibility floor that raises F0 only when it
  is greater than the IOR-derived value.
- Specular weight scales the lobe/specular F0 contribution and is not the F0
  identity.
- High dielectric reflectivity does not infer metallic; every row keeps
  `metallic = 0.0`.

## Deferred Status

Explicit authored dielectric F0 remains deferred. The preview route still does
not render IOR or specular-weight differences directly; those remain
source-backed numeric proof rows until a later route exposes them visually.
