# Configs

Persistent settings consumed by both the menu and the renderer.

- `scene_config.json` – Window size, ray count, camera origin/zoom/margin, object definitions (circles/polygons with transforms, colours, opacity), and Bézier path control points/handles. Parsed and written by `src/config/config_manager.c`.
- `animation_config.json` – Animation preferences: interactive vs. deep render, bounce behaviour, FPS, light mode, blur mode, editor mode, frame storage directory, and loop options.
- `config.json` – Legacy/lightweight settings file kept for compatibility with older tools; current code primarily uses `animation_config.json`.
- `default.ttf` – Font asset used when rendering UI text. `src/ui/sdl_menu.c` and `src/render/render_helper.c` currently hard-code system fonts, but this file is available for future packaging.
