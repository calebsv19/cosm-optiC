# Material Preview Sets

Website-facing material preview artifacts for `ray_tracing` live here.

Use this lane for bounded non-raytraced surface studies:

- one target object
- one material/texture configuration family
- one contact sheet or small preview set
- one JSON summary of effective settings

The source renderer for these sets is:

- [headless_material_preview_cli.md](../headless_material_preview_cli.md)

Typical contents per set:

- `request.json`
- `preview.bmp`
- optional `preview.png`
- optional `preview.svg` for editor/readback proof sheets
- `summary.json`
- `index.md`

`index.md` should list the variant order explicitly so the contact sheet can be
reviewed left-to-right, top-to-bottom without guessing which cell maps to which
candidate.

These sets are meant to be regenerated and replaced as material tuning changes.

## Current Sets

- [`m13_s3_stack_structure_proof_grid`](m13_s3_stack_structure_proof_grid/)
  - promoted M13 editor/readback proof grid showing compact Stack pane
    structure states: base lock, only-overlay move guards, middle-overlay row
    actions, muted preview/export readback, and full-stack add guard.
- [`m12_s5_layer_control_preview_grid`](m12_s5_layer_control_preview_grid/)
  - promoted M12 visual proof grid showing layer opacity, placement strength,
    and signed response influence edits through the headless material preview
    CLI.
- [`m11_s5_material_family_preview_grid`](m11_s5_material_family_preview_grid/)
  - promoted M11 visual proof grid comparing Glass, Mirror, and Rough Metal
    variants through the headless material preview CLI.
- [`grime_overlay_sweep_v1`](grime_overlay_sweep_v1/)
  - earlier grime overlay sweep reference set.
