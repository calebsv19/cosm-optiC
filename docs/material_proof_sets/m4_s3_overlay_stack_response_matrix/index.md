# M4-S3 Overlay Stack Material Response Matrix

This package proves current procedural overlay stack behavior for the material
system proof lane.

- request: `request.json`
- preview request: `material_preview_request.json`
- generated preview summary: `material_preview_summary.json`
- proof summary: `summary.json`
- rendered preview/contact sheet: `preview.bmp`
- route: `headless_material_preview` plus source-backed runtime material payload
  tests
- source fixture:
  `../../../tests/fixtures/disney_v2_visual_matrix/transparent_interior_stack/scene_runtime.json`
- target object: `rear_blue_panel`

## Row Order

Contact-sheet cells are ordered left-to-right, top-to-bottom.

1. `base_clean`
2. `rust_overlay`
3. `grime_overlay`
4. `oil_overlay`
5. `fog_overlay`
6. `scratches_overlay`
7. `edge_wear_overlay`
8. `grime_oil_order_contract`

## Readback

The preview image shows the one-overlay visual response for rust, grime, oil,
fog, scratches, and edge wear over one representative base. The current preview
route can only attach one synthetic `preview_overlay` to a variant, so the
order-sensitive grime/oil pair is recorded as source-backed readback in
`summary.json` rather than as a true two-overlay visual cell.

The source-backed contract covers:

- grime darkens, roughens, reduces reflectivity, and reduces specular weight.
- oil lowers roughness, raises reflectivity, raises specular weight, and keeps
  opaque bases opaque.
- grime/oil order changes evaluated color, roughness, or specular weight.
- rust/oil overlays do not reopen transparency.
- rust and fog procedural/legacy paths keep their existing material response
  tests.

## Deferred Status

Authored bitmap channel expansion remains deferred to M6. Authored texture alpha
remains a sample mask, not a promoted physical opacity or channel schema.
