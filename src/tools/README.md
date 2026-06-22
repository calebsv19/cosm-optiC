# src › tools

Auxiliary helpers that support the main runtime.

- `make_video.c` – Renumbers explicit frame directories and shells out to FFmpeg using explicit `frames dir + output path + fps` arguments.
- `cli/` – Command-line and headless tools:
  - `ray_tracing_render_headless` validates and runs detached render requests.
  - `ray_tracing_job_runner` runs local worker-style render bundles.
  - `ray_tracing_material_preview_headless` renders material preview proofs
    without opening the desktop UI.
  - `native3d_render_audit` and `ray_trace_tool` provide local audit/debug
    surfaces.
  - `shape_asset_tool`, `shape_import_tool`, and `shape_sanity_tool` support
    shape import/export and validation through the geo/import libraries.
- `ShapeLib/` – Standalone shape library used by the geo import pipeline (kept pure).

## Boundaries

- Keep user-facing desktop runtime behavior in `src/app/` and `src/ui/`.
- Keep external scene/mesh/water file adaptation in `src/import/`.
- Keep native `3D` render policy in `src/render/`.
- Keep worker package and remote-host control outside this directory unless the
  code is only a local request/bundle runner. Remote VPS/Linux PC actions must
  use the managed handoff lanes.

## R0 Notes

The tooling lane now owns more than shape import/export. This README was
refreshed during the R0 Structure Pass so later R1/R5 audits can distinguish
headless request parsing, local job-runner behavior, material-preview proofs,
and shape tooling before extracting or consolidating helpers.
