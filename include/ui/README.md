# include › ui

Headers for the pre-launch SDL menu.

- `sdl_menu.h` – Public menu entrypoints used by app runtime (`RunMenu`) plus shared render helper declarations.
- `sdl_menu_input.h` – Input/event handling contract for menu keyboard/mouse/edit behavior.
- `menu_layout.h` – Authoritative top-level menu screen-zone contract for left/center/right/footer ownership.
- `menu_panel_chrome.h` – Shared panel-surface helpers for titled menu cards and consistent menu grouping chrome.
- `menu_batch_panel.h` – Grouped `Data I/O + Batch` panel contract for render-frame/video roots, frame count, and batch actions.
- `scene_source_ui_labels.h` – User-facing scene-source wording helpers for runtime/fluid labels, button badges, and selection status text.
- `sdl_menu_render.h` – Menu layout and render contracts (button and slider layout structs + frame rendering API).
- `sdl_menu_state.h` – Shared menu runtime state model, manifest list policy, and slider/scroll helpers.
- `text_zoom_shortcuts.h` – Shared text zoom shortcut contract used by menu/runtime/editor paths.
