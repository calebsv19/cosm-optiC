# src › render

Rendering pipeline and ray-tracing engine.

- `ray_tracing2.c` – Manages light source state, performs ray casting (single-threaded and multi-threaded modes), applies blur filters, and renders lit pixels alongside scene geometry.
- `render_helper.c` – SDL drawing helpers for circles, polygons, UI text, and light-strength calculations shared by the renderer and editors.
