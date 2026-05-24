# include › render

Ray-tracing and drawing helper declarations.

- `ray_tracing2.h` – Public API for initialising, rendering, and updating the ray-tracing scene.
- `render_helper.h` – Shape drawing helpers, UI text rendering, and brightness utilities.
- `fast_rng.h` – Tiny PCG32-based RNG used by both integrators.
- `integrator_common.h` – Defines `IntegratorContext`, tile grids, irradiance cache records, and RNG helpers that back the forward-light, hybrid camera-path, and direct-only passes.
- `camera_path_integrator.h` / `forward_light_integrator.h` / `direct_light_integrator.h` – Entry points that drive the respective integrators once the context and light description are prepared. Camera-path is the hybrid GI path; direct-light is a single LOS pass.
- `irradiance_cache.h` – Interface for populating the per-object directional irradiance bins the camera integrator samples for indirect lighting.
- `surface_mesh.h` – Packed surface-segment representation plus the new `TriangleMesh` data (shared vertices + faces) emitted from the same `SegmentPath` builder, letting visibility code use triangles while the indirect heuristic still references segments.
- `uniform_grid.h` – Shared acceleration structure for intersection tests and point queries; all shadow rays, cache probes, and camera feelers traverse the same grid.
- `ray_types.h` – Basic 2D ray and hit-record structs passed between integrators and the grid.
- `runtime_camera_3d_rays.h` – Native `3D` camera projector for the runtime lane. It builds a bounded projector from `RuntimeCamera3D`, emits primary rays for proof-scene pixels, and now projects runtime world points back to screen space for live marker/overlay parity.
- `runtime_direct_light_3d.h` – Native direct-light shader for the runtime lane. It shades camera-ray hits using `RuntimeScene3D` light samples plus the native visibility seam, with no legacy brightness fallback.
- `runtime_native_3d_render.h` – Native live-render entrypoint for the bounded runtime lane. It builds `RuntimeScene3D` at the current playback `t`, applies live light/camera overrides, and dispatches through the explicit native `3D` integrator seam. `Direct Light` is the only shipped target today.
- `runtime_native_3d_preview_reconstruction.h` – Shared preview reconstruction seam for the native `3D` lane. It redraws full or dirty host rects from authoritative low-resolution frame truth so tile progress presentation stays frame-coherent instead of tile-local, and now owns the explicit `Nearest` vs `Bilinear` reconstruction policy seam used by both dirty-rect and completed-frame resolves.
- `runtime_ray_3d.h` – Native `3D` ray/hit contract for the runtime lane. It defines `Ray3D`, `HitInfo3D`, bounded offset helpers, triangle intersection, and first-hit scene traversal over `RuntimeScene3D` triangles.
- `runtime_visibility_3d.h` – Native `3D` shadow-visibility helpers for the runtime lane. It traces offset rays from surface hits to authored runtime light positions and reports triangle occlusion without using the XY-only visibility path.
- `space_mode_adapter.h` – Mode adapter seam for camera/world conversion and ray/hit setup entrypoints (`2D` default behavior with `3D` routing placeholder).
- `ray_tracing_mode_backend.h` – Runtime mode backend route contract that resolves explicit route-family ownership (`2D` canonical vs `3D` compat fallback vs bounded native `3D` activation), plus integrator/tile/cache routing and projection fallback behavior for render/event callsites.
- `runtime_scene_3d.h` – Native runtime `3D` contract scaffold for the bounded first slice: explicit primitive scope (`plane` + `rect_prism`), renderer-truth ownership flags, runtime light/camera records, and triangle-scene containers for the upcoming native route.
- `runtime_scene_3d_samples.h` – Render-owned authored-state sampler for the native `3D` lane. It resolves runtime light and camera samples from canonical hydrated path/base state at a normalized timeline parameter without depending on preview modules.
- `runtime_scene_3d_builder.h` – Render-side builder that consumes retained bridge primitive seeds and compiles the bounded first slice (`plane` + `rect_prism`) into native runtime primitives plus deterministic triangle output.
- `material_bsdf.h` – Shared material representation plus Lambert/GGX BSDF helpers that convert `SceneObject` data into shading parameters.
- `timer_hud_api.h` – TimerHUD host surface for profiling integrator subpasses through the app-owned `TimerHUDSession`, including explicit session access for frame/timer/render hooks.
