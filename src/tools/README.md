# src › tools

Auxiliary helpers that support the main runtime.

- `make_video.c` – Renumbers explicit frame directories and shells out to FFmpeg using explicit `frames dir + output path + fps` arguments.
- `cli/` – Command-line tools for shape import/export (`shape_asset_tool`, `shape_import_tool`) that work with the shared geo/import libraries.
- `ShapeLib/` – Standalone shape library used by the geo import pipeline (kept pure).
