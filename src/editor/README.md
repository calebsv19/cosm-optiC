# src › editor

Interactive tooling for shaping the scene.

- `bezier_editor.c` – Adds/removes Bézier control points, manipulates velocity handles, and toggles between quadratic/cubic paths.
- `object_editor.c` – Adds, selects, transforms, and deletes scene objects; manages polygon creation workflows.
- `scene_editor.c` – Hosts the editor window, routes events to the active editor mode, and renders shared UI chrome.
- `camera_editor.c` – Stub for future camera/viewport controls (currently logs placeholder messages).
