# config/

Tracked defaults consumed by the menu and renderer.

- `scene_config.json` – Default scene settings (window size, ray count, camera origin/zoom/margin, object definitions, Bézier paths). Runtime writes now go to `data/runtime/scene_config.json`.
- `animation_config.json` – Default animation preferences (interactive/deep render, bounce behaviour, FPS, light mode, blur mode, editor mode, frame storage directory, loop options). Runtime writes now go to `data/runtime/animation_config.json`.
- `animation_config.json` also stores renderer-specific controls:
  - `lightDiffusionEnabled`, `lightDiffusionRadius`, `lightDiffusionStrength` – post-process diffusion blur applied in legacy full-frame mode.
  - `useTiledRenderer` – switches between the original full-buffer path and the new tile-based renderer.
  - `tilePreviewEnabled` – when tiled renderer is enabled, incrementally presents each completed tile during hybrid renders.
  - `tileSize` – tile edge length (multiples of 4, default 16) used when `useTiledRenderer` is true.
  - `rouletteThreshold` – Russian-roulette cutoff controlling when low-energy rays terminate.
  - `integratorMode` – selects the renderer pipeline: 0 = Forward Light (light-emitted rays), 1 = Hybrid (camera-path GI), 2 = Direct Light (single LOS pass). (Disney path is currently paused in the UI.)
  - `pathSamplesPerPixel`, `pathMaxDepth` – per-pixel sampling count and bounce limit for the camera-path mode.
  - `pathDirectLighting`, `pathRussianRoulette` – toggles for next-event estimation and Russian roulette in the camera-path integrator.
  - `environmentBrightness` – scalar environment light applied when rays miss all geometry.
  - `pathSeed` – base RNG seed for camera-path sampling.
  - `forwardDecay` – forward integrator falloff distance in world units (roughly pixels). Increase to let primary/reflection energy travel farther before dimming.
  - `forwardFalloffMode` – 0=quadratic (1/r²), 1=linear (1/r), 2=None. Controls how the forward integrator attenuates energy over distance.
- `config.json` – Legacy/lightweight settings file kept for compatibility with older tools; current code primarily uses `animation_config.json`.
- `default.ttf` – Font asset used when rendering UI text. `src/ui/sdl_menu.c` and `src/render/render_helper.c` currently hard-code system fonts, but this file is available for future packaging.
