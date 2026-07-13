# include › ui

Headers for the pre-launch SDL menu.

- `sdl_menu.h` – Public menu entrypoints used by app runtime (`RunMenu`) plus shared render helper declarations.
- `sdl_menu_input.h` – Input/event handling contract for menu keyboard/mouse/edit behavior.
- `menu_layout.h` – Authoritative top-level menu screen-zone contract for the stable Scene pane, switchable center workspace, Effective Render pane, and global footer.
- `menu_pane_host.h` – Resizable three-leaf `core_pane` host and `kit_pane` splitter interaction contract.
- `menu_workspace.h` – `core_pane_module` registry and active `Render`, `Output + Batch`, or `Run + Resume` center-module contract.
- `menu_panel_chrome.h` – Shared panel-surface helpers for titled menu cards and consistent menu grouping chrome.
- `menu_batch_panel.h` – `Output + Batch` module contract for render-frame/video roots, frame count, and batch actions.
- `menu_resume_panel.h` – `Run + Resume` module contract for frame inventory, start/resume policy, and next-existing readback; global Preview/Start actions remain owned by Runtime Route.
- `scene_source_ui_labels.h` – User-facing scene-source wording helpers for runtime/fluid labels, button badges, and selection status text.
- `sdl_menu_render.h` – Menu layout and render contracts (button and slider layout structs + frame rendering API).
- `sdl_menu_state.h` – Shared menu runtime state model, manifest list policy, and slider/scroll helpers.
- `text_zoom_shortcuts.h` – Shared text zoom shortcut contract used by menu/runtime/editor paths.
