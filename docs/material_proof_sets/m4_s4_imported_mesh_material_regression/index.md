# M4-S4 Imported Mesh Material Regression Matrix

This package proves the current object-wide material contract for
imported/runtime mesh geometry in the material-system proof lane.

- request: `request.json`
- native render request: `render_request.json`
- generated render summary: `render/render_summary.json`
- proof summary: `summary.json`
- rendered image: `render/frames/frame_0000.bmp`
- route: `selected_native_render` plus source-backed runtime mesh asset audit
  tests
- source fixture:
  `../../../tests/fixtures/mesh_asset_runtime_spheres/scene_runtime.json`
- source-backed imported fixture:
  `../../../tests/fixtures/mesh_asset_runtime_imported_tetrahedron/scene_runtime.json`

## Row Order

The rendered image contains the three runtime mesh sphere rows as visible
objects in one native render:

1. `runtime_mesh_low_red_matte`
2. `runtime_mesh_medium_blue_matte`
3. `runtime_mesh_high_gold_matte`
4. `imported_tetrahedron_source_contract`

## Readback

The native render route generated `render/frames/frame_0000.bmp` from the
runtime mesh spheres fixture with the `direct_light` integrator. The render
summary reports:

- `scene_applied = true`, `route_native_3d = true`, and `frames_rendered = 1`.
- BVH ready with `1238` triangles and `0` trace overflows.
- `15360` nonzero pixels and `10266` visible pixels.
- object audit rows:
  - `obj_sphere_low`: `48` triangles, `236` primary hit pixels.
  - `obj_sphere_medium`: `224` triangles, `248` primary hit pixels.
  - `obj_sphere_high`: `960` triangles, `268` primary hit pixels.

The focused runtime mesh asset headless audit also passed and confirmed the
source-backed imported tetrahedron row:

- `obj_imported_tetrahedron`
- `asset_imported_tetrahedron_01`
- `4` runtime triangles
- surface group `imported_surface`

This proves the current object-wide material assignment/readback path for
runtime mesh geometry and the imported tetrahedron asset path. It does not
promote mesh-region material authoring.

## Deferred Status

Imported mesh per-region material authoring remains deferred until stable
mesh-region metadata exists. S4 only proves object-wide material assignment and
evaluated payload behavior for imported/runtime mesh objects.
