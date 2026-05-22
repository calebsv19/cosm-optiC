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
- `summary.json`
- `index.md`

`index.md` should list the variant order explicitly so the contact sheet can be
reviewed left-to-right, top-to-bottom without guessing which cell maps to which
candidate.

These sets are meant to be regenerated and replaced as material tuning changes.
