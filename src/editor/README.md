# src › editor

Interactive tooling for shaping the scene.

- `bezier_editor.c` – Adds/removes Bézier control points, manipulates velocity handles, and renders the path using the current camera margin so edits match the live viewport.
- `object_editor.c` – Adds, selects, transforms, and deletes scene objects; manages polygon creation workflows and shows the camera frustum while editing.
- `scene_editor.c` – Hosts the editor window, routes events to the active editor mode (cycle with Tab/Shift+Tab), saves settings, and draws shared HUD elements.
- `camera_editor.c` – Fully interactive camera tooling: click-drag to pan, scroll/± to zoom, adjust the saved margin, edit the camera’s Bézier path (add/delete/toggle cubic/quadratic), and preview both light and camera paths through the active camera.
