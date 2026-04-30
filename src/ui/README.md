# src › ui

Menu components presented before starting the renderer.

- `menu/menu_layout.c` – Top-level menu zone builder for left controls, center utility content, slider panel, route stack, footer, and dropdown reserve.
- `menu/menu_panel_chrome.c` – Shared grouped-panel chrome renderer for titled menu cards, borders, and dividers across the menu.
- `menu/sdl_menu.c` – Top-level menu orchestrator (SDL lifecycle + event loop dispatch).
- `menu/sdl_menu_input.c` – Menu keyboard/mouse/edit handling (including root-path edit/apply controls, renderer control editing, and deep-render start/resume inputs).
- `menu/sdl_menu_render.c` – Menu layout/render pass (buttons, sliders, manifest dropdown, status text, and the current native `3D` control surface).
- `menu/sdl_menu_state.c` – Runtime menu state, manifest option discovery, scroll policy, and persisted menu-side renderer/export state.
- `menu/text_zoom_shortcuts.c` – Shared text-zoom shortcut handling used by menu and runtime/editor flows.
