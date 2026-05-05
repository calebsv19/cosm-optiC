# include › editor

Editor-facing interfaces shared across the scene editing modules.

- `bezier_editor.h` – Bézier point/handle manipulation API (camera-aware hit testing).
- `object_editor.h` – Object placement and selection controls (exports helpers like `CheckObjectClick`).
- `object_editor_selection_tracker.h` – Selection tracker API for sharing current/last object focus between editor modes.
- `material_editor.h` – Focused material editor API for object-wide procedural texture controls and hit routing.
- `scene_editor.h` – Scene editor container, mode switching (button or Tab), and shared UI helpers.
- `camera_editor.h` – Camera control interface plus shared utilities such as `GetCurrentMarginPixels`.
- `editor_mode_router.h` – Central editor mode routing/capability contract (mode clamp/cycle rules, space-mode labels, and backend-routed view-context building for future 3D editor expansion).
- `scene_editor_control_surface.h` – Shared scene-editor shell contract builder for lane-aware status/control descriptors (`2D` vs controlled `3D` vs native-3D-reserved).
- `scene_editor_surface_render.h` – Shared scene-editor shell pane render API for left-pane mode summaries and right-pane wrapped status flow.
