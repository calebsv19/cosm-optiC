# include › editor

Editor-facing interfaces shared across the scene editing modules.

- `bezier_editor.h` – Bézier point/handle manipulation API (camera-aware hit testing).
- `object_editor.h` – Object placement and selection controls (exports helpers like `CheckObjectClick`).
- `scene_editor.h` – Scene editor container, mode switching (button or Tab), and shared UI helpers.
- `camera_editor.h` – Camera control interface plus shared utilities such as `GetCurrentMarginPixels` and `RenderEditorHUD`.
- `editor_mode_router.h` – Central editor mode routing/capability contract (mode clamp/cycle rules, space-mode labels, and backend-routed view-context building for future 3D editor expansion).
