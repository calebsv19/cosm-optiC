# src › render

Rendering pipeline and ray-tracing engine.

- `ray_tracing2.c` – Manages light source state, casts rays (legacy full-frame or tile-based tile jobs), applies Russian-roulette termination, accumulates energy in per-tile SoA buffers, tonemaps them back into the screen, and runs the separable post-process blur.
- `render_helper.c` – SDL drawing helpers for circles, polygons, UI text, and light-strength calculations shared by the renderer and editors.
