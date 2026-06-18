# Mirror Surface Unification Visual Matrix

This fixture is the SU4 proof set for Disney v2 mirror behavior across authored surface families.

- `scene_plane_runtime.json` uses a mirror plane wall.
- `scene_prism_runtime.json` uses a thin rect-prism mirror wall with its front face aligned to the plane wall.
- `scene_mesh_runtime.json` uses a two-triangle runtime mesh-asset mirror wall from `assets/mesh_assets/asset_mirror_wall_quad.runtime.json`.

Each scene keeps the same camera, matte floor, red/blue reflection markers, and finite point light. The request set renders a single no-denoise frame for surface-kind parity and a 12-temporal-frame denoise-on pass for visual policy review.
