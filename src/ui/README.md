# src › ui

Menu components presented before starting the renderer.

- `sdl_menu.c` – Builds the SDL UI for toggling interactive/deep render modes, switching between forward-light and camera-path integrators, editing numeric settings through sliders (rays, FPS, tile size, roulette threshold, falloff distance, falloff softness, path SPP/depth, environment brightness, etc.), enabling the tiled renderer, and launching the scene editor or animation loop. Includes a forward falloff mode button (None / Linear / Quadratic) and persists all settings back to `config/animation_config.json`.
