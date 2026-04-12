# src › ui

Menu components presented before starting the renderer.

- `menu/sdl_menu.c` – Top-level menu orchestrator (SDL lifecycle + event loop dispatch).
- `menu/sdl_menu_input.c` – Menu keyboard/mouse/edit handling (including root path edit/folder/apply controls and slider interactions).
- `menu/sdl_menu_render.c` – Menu layout/render pass (buttons, sliders, manifest dropdown, status text).
- `menu/sdl_menu_state.c` – Runtime menu state and manifest option discovery/scroll policy.
- `menu/text_zoom_shortcuts.c` – Shared text-zoom shortcut handling used by menu and runtime/editor flows.
