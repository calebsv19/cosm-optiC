# src › ui

Menu components presented before starting the renderer.

- `sdl_menu.c` – Builds the SDL UI for toggling interactive/deep render modes, switching between forward-light and camera-path integrators, editing numeric settings through sliders (rays, FPS, tile size, roulette threshold, path SPP/depth, environment brightness, etc.), enabling the tiled renderer, and launching the scene editor or animation loop. Persists user choices back to `Configs/animation_config.json`.
