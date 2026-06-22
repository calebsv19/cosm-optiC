# src › import

Import adapters that translate external scene, mesh, pack, manifest, and
simulation handoff data into optiC runtime/editor structures.

This lane is app-local. It owns validation and adaptation at the boundary
between external file formats and the renderer/editor contracts, but it should
not own native `3D` shading policy or worker orchestration policy.

## Ownership

- `runtime_scene_bridge*.c`: runtime-scene bridge and authoring adapters for
  retained scene data, light/camera seeds, and writeback paths.
- `runtime_mesh_asset_*`: mesh asset loading, packing, and staging helpers used
  by imported/runtime mesh scenes.
- `fluid_*` and `water_surface_import.c`: PhysicsSim pack/manifest, VF3D, and
  water-surface sidecar import boundaries.
- pack/manifest helpers: local parsing and preflight for scene bundles that
  feed headless or runtime render paths.

## Boundaries

- Keep JSON/file-format parsing and external-path normalization here.
- Keep render-owned geometry compilation in `src/render/`.
- Keep editor interaction state in `src/editor/`.
- Keep headless request orchestration in `src/app/` or `src/tools/cli/`.
- Do not add remote worker, VPS, Linux PC, or visualizer publication behavior
  here; route those through the managed handoff/worker lanes.

## R0 Notes

- This README was added during the R0 Structure Pass because import ownership
  had grown to include runtime-scene, mesh, pack, VF3D, and water handoff paths
  without a local map.
- Later R1/R5 passes may audit repeated path normalization, request preflight,
  or fixture setup helpers, but R0 does not consolidate them.
