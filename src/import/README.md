# src › import

Import adapters that translate external scene, mesh, pack, manifest, and
simulation handoff data into optiC runtime/editor structures.

This lane is app-local. It owns validation and adaptation at the boundary
between external file formats and the renderer/editor contracts, but it should
not own native `3D` shading policy or worker orchestration policy.

## Ownership

- `runtime_scene_bridge*.c`: runtime-scene bridge and authoring adapters for
  retained scene data, authored `lights[]` light-list seeds, compatibility
  moving-light/camera seeds, and writeback paths.
- `runtime_mesh_asset_*`: mesh asset loading, packing, and staging helpers used
  by imported/runtime mesh scenes. `runtime_mesh_asset_loader.c` owns runtime
  sidecar path resolution, stable `asset_id` validation, parsed-document cache
  validity, and loader timing/cache diagnostics. Render-side BLAS/TLAS work
  should consume loaded mesh documents or prepared asset handles instead of
  duplicating this path-resolution policy.
- `fluid_*` and `water_surface_import.c`: PhysicsSim pack/manifest, VF3D, and
  water-surface sidecar import boundaries. `water_body_boundary_contract.*`
  owns canonical `water_body_boundary_v1` parsing and normalizes both the
  established static-shell form and the already-unified dynamic form into one
  runtime contract; callers must not reintroduce a stricter duplicate parser.
- pack/manifest helpers: local parsing and preflight for scene bundles that
  feed headless or runtime render paths.
- `compound_scene_handoff_import.c` and
  `compound_scene_binding_manifest.c`: strict app-local ingestion of the
  frozen Ball Bounce transform packet and validation of RayTracing-owned
  object/mesh bindings.
- `compound_scene_evaluated_scene.c`: transactional exact-tick replacement of
  already-present mapped object transforms in a detached evaluated snapshot.
  It preserves exact quaternion and packet provenance plus an Euler
  compatibility view. It does not apply transforms to runtime geometry; the
  render-owned detached application lives in
  `src/render/compound_scene_detached_geometry.c`.
- `compound_scene_assembly_codec.c`: canonical S9-G file-backed metadata for
  up to 16 exact assembly ticks. It binds external source-mesh path/SHA
  references, body bounds and geometry digests, static-plane authority, and
  measured clearance. Exact replay returns metadata only; it never resolves
  mesh bytes or acquires camera, light, material, sampling, worker, or image
  ownership.
- `compound_scene_static_room_import.c`: strict independent reader for the
  Ball-owned six-surface room sidecar. `compound_scene_room_basis.c` owns the
  single provenance join and frozen right-handed `(x,y,z) -> (x,-z,y)` map
  used for every packet body and collision-surface frame. These modules expose
  mapped metadata only; H3 remains responsible for renderer-owned plane
  assembly and visual plane-match proof.
- `compound_scene_ingestion.c`: I-1's typed, app-local descriptor and atomic
  resolver. It binds the frozen handoff and room to existing renderer object/
  mesh identities, applies the registered basis to final owned geometry, and
  returns a separately digested derived result without mutating its base scene.
  It intentionally has no request-file codec or normal render hook; those are
  I-2 decisions.

## Boundaries

- Keep JSON/file-format parsing and external-path normalization here.
- Keep render-owned geometry compilation in `src/render/`.
- Keep mesh-local acceleration structures and scene-level TLAS state in
  render-owned modules; import code should not own ray traversal policy.
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
