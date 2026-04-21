# src › editor

Interactive tooling for shaping the scene.

- `bezier_editor.c` – Adds/removes Bézier control points, manipulates velocity handles, and renders the path using the current camera margin so edits match the live viewport.
- `object_editor.c` – Adds, selects, transforms, and deletes scene objects; manages polygon creation workflows and shows the camera frustum while editing.
- `scene_editor.c` – Hosts the editor window, routes events to the active editor mode (cycle with Tab/Shift+Tab), saves settings, and draws shared HUD elements.
- `camera_editor.c` – Fully interactive camera tooling: click-drag to pan, scroll/± to zoom, adjust the saved margin, edit the camera’s Bézier path (add/delete/toggle cubic/quadratic), and preview both light and camera paths through the active camera.
- `editor_mode_router.c` – Canonical editor mode routing layer that centralizes mode clamp/cycle policy, backend-routed view-context construction, and explicit `3D compat fallback -> 2D backend` capability labeling until full 3D edit math lands.
- `scene_editor_control_surface.c` – Shared control-surface provider that maps backend route + digest state into lane-aware scene-editor shell labels/status/action enablement.
- `scene_editor_surface_render.c` – Shared left/right pane render adapter for the scene-editor shell so mode summaries and status flow stay out of the core event/router file.
