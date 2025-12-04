# src › tools

Auxiliary helpers that support the main runtime.

- `make_video.c` – Renumbers captured BMP frames and shells out to FFmpeg to assemble MP4 videos after deep-render sessions.
- `cli/` – Command-line tools for shape import/export (`shape_asset_tool`, `shape_import_tool`) that work with the shared geo/import libraries.
- `ShapeLib/` – Standalone shape library used by the geo import pipeline (kept pure).
