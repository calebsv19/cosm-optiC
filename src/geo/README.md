# src › geo

Shape asset plumbing used to import external geometry.

- `geolib/` – Pure geometry utilities shared with other projects; no renderer dependencies.
  - `shape_asset.c` – Flattened path storage, JSON load/save, rasterization helpers.
  - `shape_library.c` – Directory loader for `.asset.json` files.
- `shape_adapter.c` – Bridge from `ShapeAsset` to this renderer’s `SceneObject` format (polygons only), with point-count clamping for `MAX_POINTS`.
