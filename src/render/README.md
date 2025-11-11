# src › render

Rendering pipeline and ray-tracing engine.

- `integrators/` – Dedicated modules for the renderer’s sampling strategies.
  - `integrator_common.c` – Shared context structs, tile helpers, and utility math used across integrators.
  - `forward_light_integrator.c` – Emits rays from the light source, manages multi-threaded energy accumulation, diffusion, and tonemapping.
  - `camera_path_integrator.c` – Fires camera rays per pixel, handles BSDF sampling, direct-lighting, and per-pixel accumulation.
- `ray_tracing2.c` – Orchestrates renderer lifecycle, resource ownership, blur post-process, and delegates to the selected integrator.
- `render_helper.c` – SDL drawing helpers for circles, polygons, UI text, and light-strength calculations shared by the renderer and editors.
- `uniform_grid.c` – Builds the reusable grid accelerator used by shadow rays, camera rays, and the forward-light sampler.
