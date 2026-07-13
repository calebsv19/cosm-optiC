# src › ui

Menu components presented before starting the renderer.

- `menu/menu_pane_host.c` – Shared-first three-leaf menu shell using `core_pane` solve/constraints and `kit_pane` splitter interaction; accepted widths persist in animation settings.
- `menu/menu_workspace.c` – `core_pane_module` registry and center-module selection for `Render`, `Output + Batch`, and `Run + Resume`.
- `menu/menu_layout.c` – Top-level layout projection from the pane host into stable Scene, switchable workspace, Effective Render, route, footer, and dropdown regions.
- `menu/menu_panel_chrome.c` – Shared grouped-panel chrome renderer for titled menu cards, borders, and dividers across the menu.
- `menu/sdl_menu.c` – Top-level menu orchestrator (SDL lifecycle + event loop dispatch).
- `menu/sdl_menu_input.c` – Menu keyboard/mouse/edit handling, pane splitter drag, workspace selection, renderer controls, and deep-render start/resume inputs.
- `menu/sdl_menu_render.c` – Menu layout/render orchestrator. It owns frame-level panel/layout composition and delegates shared text/button/control drawing helpers instead of keeping the full control surface inline.
- `menu/sdl_menu_render_controls.c` – Shared menu render helpers for button chrome, text fitting, readable-theme colors, and bounded control-surface drawing reused by the main menu render pass.
- `menu/sdl_menu_state.c` – Runtime menu state, manifest discovery, workspace selection, default-off caustic recipe, scroll policy, and persisted renderer/menu state.

The main menu keeps Scene + Mode visible on the left and Effective Render on
the right. The center module separates render recipe editing from output/batch
work and frame-resume configuration. Render has nested Lighting, Performance, and
Caustics views; caustics and photon budgets default off/zero, while the right
pane continuously reports the effective recipe, multiplicative cost warning,
and current compatibility/digest route. Its compact Runtime Route stack owns
Space, Editor, Scene Editor, Preview, and Start, so launch actions remain
available independently of the selected center module. Pane widths and the
active center module persist without introducing a separate shared snapshot
format.
- `menu/text_zoom_shortcuts.c` – Shared text-zoom shortcut handling used by menu and runtime/editor flows.
