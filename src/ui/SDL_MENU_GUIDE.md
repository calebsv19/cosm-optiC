SDL Menu Guide
==============

Layout overview (left → center → right):
- Left column: mode toggles, bounce/auto, integrator and its sub-toggles.
- Center column: forward falloff, tile renderer.
- Right column: scene editor launch + mode toggle above Start; sliders on the far right.
- Bottom row: Save, Restore Defaults, Preview, Exit w/o Saving, Start.

Buttons (top to bottom)
-----------------------
**Left column**
- Interactive Mode: enable live interaction (disables Deep Render).
- Deep Render: enable offline/deep render settings (disables Interactive).
- Bounce Mode: toggle bounce animation for deep render playback.
- Auto MP4: auto-generate MP4 after deep render finishes.
- Integrator: Forward Light ↔ Camera Path.
- Direct Light: toggle direct lighting when Camera Path integrator is active.
- Roulette: toggle Russian roulette termination when Camera Path integrator is active.
- MIS: toggle multiple importance sampling when Camera Path integrator is active.
- BSDF: Lambert ↔ GGX when Camera Path integrator is active.

**Center column**
- Forward Falloff: cycle falloff model (Quadratic → Linear → None).
- Tile Renderer: toggle tiled renderer on/off.

**Right column**
- Scene Editor: opens the scene editor window.
- Editor Mode: cycles Path ↔ Scene ↔ Camera for the scene editor default tab.
- Start: launches the main run (interactive or deep render depending on toggles).
- Save / Restore Defaults: save current settings or reset to defaults (status toast appears next to Save).
- Preview: launches the lightweight path/camera preview.
- Exit w/o Saving: closes the menu without writing settings.

Sliders (right side)
--------------------
Grouped on the far right. Current set (values persist via config):
- Bounce Limit (0–100): maximum bounces for deep render.
- Frame Limit (1–5000): total frames to render.
- Path Points (1–5000): frames to traverse paths (affects `t_increment`).
- FPS (1–240): target frames per second; updates frame duration.
- Num Rays (0–10000): scene ray count.
- Width (200–4000): render/window width.
- Height (200–2400): render/window height.
- Tile Size (4–256): tile size for tiled renderer (clamped to multiples of 4).
- Roulette Threshold (0.001–2.000): Russian roulette threshold (slider shows threshold*1000).
- Light Intensity (0–20.00): scene light intensity (slider shows *100).
- Falloff Distance (100–40000): forward decay distance (in world units).
- Path SPP (1–128, only when Camera Path integrator is active): samples per pixel for camera path integrator.
- Path Depth (1–16, only when Camera Path integrator is active): max path depth.
- Environment % (0–2.00, only when Camera Path integrator is active): environment brightness (slider shows percent*100).
- Cache Weight % (0–1.00, only when Camera Path integrator is active): irradiance cache contribution weight (slider shows percent*100).

Status toast
------------
- After Save: “Saved” appears next to the Save button and fades.
- After Restore Defaults: “Restored” appears next to the Save button and fades.

Notes
-----
- Scene Editor and Editor Mode sit just above Start on the right; their hitboxes match their drawn size.
- Forward Falloff / Tile hitboxes match their middle-column positions (anchored between the left buttons and slider column).
