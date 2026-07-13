SDL Menu Guide
==============

Layout overview (left → center → right):
- Left pane: mode-aware render controls, integrator/source controls, expanded
  scene list, scene roots, and volume/atmosphere controls.
- Center panes: Renderer Controls, Data I/O + Batch, then Frame Resume.
- Right side: Render Settings sliders plus scene/editor/start/preview route actions.
- Footer: Exit w/o Saving, Restore Defaults, and Save, starting to the right of the left pane.

Buttons (top to bottom)
-----------------------
**Left column**
- Interactive Mode: legacy `2D` quick-light sandbox; hidden as a primary
  control in native/compact `3D`.
- Deep Render / 3D Render: enable offline/deep render settings.
- Async Render: opt into the native tiled `3D` Deep Render worker/session path.
  It is off by default; unsupported routes and dynamic volume/water selection
  retain the synchronous Deep Render path.
- Bounce / Bounce Mode: toggle bounce animation for deep render playback.
- Auto MP4: auto-generate MP4 after deep render finishes.
- Integrator: cycles Forward Light ↔ Hybrid (Camera Path) ↔ Direct Light (LOS-only).
- Roulette: toggle Russian roulette termination (used by forward lighting).
- BSDF: Lambert ↔ GGX when Hybrid integrator is active.

**Center column**
- Forward Falloff: cycle falloff model (Quadratic → Linear → None).
- Tile Renderer: toggle tiled renderer on/off.
- Tile Preview: show tiles as they complete during hybrid renders.
- Disney Denoise: toggle Disney-only native `3D` denoise.
- Top Fill: toggle the native `3D` overhead fill light.
- Upscale: cycle native `3D` upscale mode (`OFF` → `Nearest` → `Bilinear`).
- Light Height: cycle the native `3D` light-height preset.
- Data I/O + Batch: owns render-frame root, video-output root, batch frame/video
  actions, and the scene-only worker queue export action.
- Frame Resume: owns resume-existing, start-frame editing, next-existing
  readback, and future frame-inventory diagnostics.

**Right column**
- Scene Editor: opens the scene editor window.
- Editor Mode: cycles Path ↔ Scene ↔ Camera for the scene editor default tab.
- Start: launches the main run (interactive or deep render depending on toggles).
- Preview: launches the lightweight path/camera preview.

**Footer**
- Save / Restore Defaults: save current settings or reset to defaults (status toast appears next to Save).
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
- Roulette Threshold (0.001–2.000, only outside 3D mode): legacy Russian roulette threshold for the older path/integrator lane (slider shows threshold*1000).
- Light Intensity (0–20.00): scene light intensity (slider shows *100).
- Falloff Softness (0.10–5.00): scales how quickly light energy decays with distance (higher = slower decay/longer reach) for forward/direct/hybrid integrators.
- Falloff Distance (100–40000): forward decay distance (in world units).
- 3D Bounce Depth (1–8, only in 3D mode): hard cap for recursive diffuse continuation depth in native `3D`.
- 3D Roulette Threshold (0.000–0.100, only in 3D mode): throughput-luminance threshold where native `3D` diffuse paths begin Russian roulette termination (`0.000` disables it).
- 3D Secondary Samples (4–64, only in 3D mode): secondary diffuse/emissive bounce sample count for native `3D`, clamped to multiples of 4.
- 3D Transmission Samples (4–32, only in 3D mode): transparent-view transmission sample count for native `3D`.
- 3D Temporal Frames (1–32, only in 3D mode): per-resolved-frame native `3D` stochastic subpass count before grayscale tonemap resolve. `1` disables temporal accumulation.
- 3D Render Scale (HiDPI, 1–8, only in 3D mode): native `3D` internal render scale. `HiDPI` traces at the window drawable pixel size when the display exposes more pixels than the logical window; `1x` traces at the logical host resolution; higher values trace fewer pixels and reconstruct back into the normal window through the active `3D Upscale Mode`.
- Path SPP (1–128, only when Camera Path integrator is active): samples per pixel for camera path integrator.
- Path Depth (1–16, only when Camera Path integrator is active): max path depth.

Status toast
------------
- After Save: “Saved” appears next to the Save button and fades.
- After Restore Defaults: “Restored” appears next to the Save button and fades.
- After Export Queue: a worker-export success or failure toast appears. The
  first supported package mode is scene-only and requires a runtime scene plus a
  discoverable render request sidecar.

Notes
-----
- Scene Editor and Editor Mode sit just above Start on the right; their hitboxes match their drawn size.
- Forward Falloff / Tile hitboxes match their middle-column positions (anchored between the left buttons and slider column).
