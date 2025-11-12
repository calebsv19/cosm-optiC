# include › render

Ray-tracing and drawing helper declarations.

- `ray_tracing2.h` – Public API for initialising, rendering, and updating the ray-tracing scene.
- `render_helper.h` – Shape drawing helpers, UI text rendering, and brightness utilities.
- `fast_rng.h` – Tiny PCG32-based RNG used by both integrators.
- `integrator_common.h` – Defines `IntegratorContext`, tile grids, irradiance cache records, and RNG helpers that back both the forward-light and camera-path passes.
- `camera_path_integrator.h` / `forward_light_integrator.h` – Entry points that drive the respective integrators once the context and light description are prepared.
- `irradiance_cache.h` – Interface for populating the per-object directional irradiance bins the camera integrator samples for indirect lighting.
- `surface_mesh.h` – Packed surface-segment representation plus the new `TriangleMesh` data (shared vertices + faces) emitted from the same `SegmentPath` builder, letting visibility code use triangles while the indirect heuristic still references segments.
- `uniform_grid.h` – Shared acceleration structure for intersection tests and point queries; all shadow rays, cache probes, and camera feelers traverse the same grid.
- `ray_types.h` – Basic 2D ray and hit-record structs passed between integrators and the grid.
- `material_bsdf.h` – Shared material representation plus Lambert/GGX BSDF helpers that convert `SceneObject` data into shading parameters.
- `timer_hud_api.h` – Lightweight instrumentation hooks (`ts_start_timer`, etc.) used to profile integrator subpasses without pulling in the entire TimerHUD implementation.
