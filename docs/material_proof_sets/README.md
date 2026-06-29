# Material Proof Sets

Material proof sets are bounded review packages for RayTracing material
behavior. They sit above the existing headless material preview and selected
native render routes: the preview/render tool may generate the image, but the
proof package owns the contract for what was requested, what fields were read
back, what route produced the artifact, and which behavior is proven,
qualitative, or deferred.

Use this lane for M4 material-system proof work:

- glass roughness and transmission contrast
- reflectivity, specular, F0, and IOR contrast
- ordered procedural overlay stack response
- imported mesh object-wide material assignment

Do not use this lane for editor UI expansion, texture-channel schema work, node
graph compilation, or new material field promotion.

## Package Layout

Each proof set directory should contain:

- `request.json`: authored proof package request.
- `summary.json`: structured readback for every proof row.
- `index.md`: human review notes and row order.
- one or more image artifacts, usually `preview.png`, `preview.bmp`, a contact
  sheet, or a selected native render output.

Dry-run S0 packages may omit the image file, but must still name the intended
image path and mark the artifact as `dry_run_not_rendered` in `summary.json`.

## Request Contract

Schema id:

```json
"ray_tracing_material_proof_package_request_v1"
```

Required fields:

- `schema`
- `proof_id`
- `title`
- `phase`
- `route`
- `artifacts`
- `rows`

`route` names the selected proof surface:

- `headless_material_preview`: deterministic swatch/contact-sheet proof using
  `ray_tracing_material_preview_headless`.
- `selected_native_render`: bounded native render proof for transport-visible
  behavior that preview cannot validate.
- `hybrid_preview_and_render`: preview first, selected render where the slice
  requires transport evidence.

`artifacts` must name at least:

- `request_path`
- `summary_path`
- `index_path`
- `image_path`

Every row should name:

- `row_id`
- `label`
- `material_family`
- `source_material`
- `expected_behavior`
- `deferred_status`

## Summary Contract

Schema id:

```json
"ray_tracing_material_proof_summary_v1"
```

Required top-level fields:

- `schema`
- `proof_id`
- `phase`
- `route`
- `status`
- `artifacts`
- `rows`
- `readback_notes`

Every row should report these M2/M3 material-contract fields when available:

- `base_color`
- `display_alpha`
- `physical_opacity`
- `transmission_weight`
- `roughness`
- `reflectivity_floor`
- `specular_weight`
- `ior`
- `derived_dielectric_f0`
- `authored_dielectric_f0`
- `metallic`
- `emission`
- `compatibility_bridges`
- `preview_interpretation`
- `bsdf_interpretation`
- `deferred_status`

Missing fields must be explicit. Use `deferred`, `not_exposed_by_route`, or
`not_applicable` instead of silently omitting contract fields that matter to a
slice.

## Route Policy

M4 proof packages should prefer the cheapest route that proves the selected
behavior:

- use headless material preview for row ordering, contact sheets, stack
  overlays, and field-readback review.
- add selected native render only when the behavior depends on physical
  transport, imported mesh geometry, transmission, or Disney/native `3D`
  integrator output.
- keep compatibility bridges labeled in summaries. Do not reinterpret display
  alpha as physical opacity, reflectivity as authored F0, or high dielectric
  reflectivity as metallic.

## Existing Preview Lane

The older preview-set lane remains available for lightweight visual tuning:

- [headless_material_preview_cli.md](../headless_material_preview_cli.md)
- [material_preview_sets](../material_preview_sets/)

M4 proof sets may call that preview tool, but should wrap the output in this
proof package contract so later M5/M6 work can read field semantics without
guessing.
