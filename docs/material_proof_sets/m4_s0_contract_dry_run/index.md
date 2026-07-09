# M4-S0 Material Proof Package Contract Dry Run

This package validates the M4 proof-set shape without rendering a proof matrix.

- request: `request.json`
- summary: `summary.json`
- intended image artifact: `preview.png`
- image status: `dry_run_not_rendered`
- route: `headless_material_preview` first, selected native render only when a
  later slice needs transport or imported-mesh evidence

## Row Order

Rows are ordered left-to-right, top-to-bottom when an image/contact sheet is
generated.

1. `contract_base_row`

## S0 Result

The package records the required paths, row fields, compatibility bridges, and
deferred field statuses expected by later M4 slices. M4-S1 should replace this
dry-run image placeholder with a real glass roughness/transmission proof
package.
