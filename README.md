# optiC (`ray_tracing`)

`optiC` is the packaged desktop product for the `ray_tracing` program. It is a
C/SDL2 scene editor and renderer with both legacy `2D` paths and a shipped
native `3D` runtime ladder.

Identity note:

- public product name: `optiC`
- repository/program key: `ray_tracing`
- launcher, binary, log, and source-level contracts still use `RayTracing` or
  `ray_tracing` identifiers where those are part of the current technical
  surface

## Current Scope

- interactive scene editing for objects, camera, and Bézier paths
- legacy `2D` render routes remain available
- shipped native `3D` tiers:
  - `Direct Light`
  - `Diffuse Bounce`
  - `Material`
  - `Emission / Transparency`
  - `Disney`
- deep-render frame export with start/resume controls
- fluid/scene import support for `.vf2d`, `.pack`, `manifest.json`, and `scene_bundle.json`

## Implemented Today

- native `3D` output is now RGB-aware through the full shipped ladder
- native `3D` config now persists a dedicated `3D` integrator lane plus bounded temporal-frame, bounce-depth, roulette, denoise, top-fill, and atmosphere-source settings independently of legacy `2D` state
- native `3D` support layers include:
  - tile preview
  - dirty-rect preview updates
  - shared-frame preview reconstruction
  - explicit native `3D` upscale mode (`OFF` / `Nearest` / `Bilinear`)
  - tile occupancy culling
  - temporal accumulation upgrades
  - stratified + blue-noise sampling support
  - Disney-only denoise
  - optional top-fill lighting
- object authoring separates material assignment from color authoring and now uses compact RGBA sliders
- per-object transparency and emissive-strength controls persist through runtime-scene save/reapply lanes
- runtime-scene digest overlays keep guide-only helpers visible and pickable while excluding them from real native `3D` geometry participation
- deep-render controls can:
  - start at a chosen absolute frame index
  - resume from the next existing saved frame
  - keep output numbering and timeline sampling on the same absolute-frame contract
- the menu/export surface includes current frame-root and video-output-root batch actions, and the native `3D` menu keeps geometry-scene selection separate from optional atmosphere attachment

## Build and Run

Dependencies:
- SDL2
- SDL2_ttf
- json-c
- FFmpeg for MP4 assembly

Core commands:

```bash
make -C ray_tracing
make -C ray_tracing run
make -C ray_tracing debug
make -C ray_tracing release
```

Packaging and Desktop refresh flows produce `optiC.app`; see
`docs/desktop_packaging.md`.

Verification entry points:

```bash
make -C ray_tracing clean && make -C ray_tracing
make -C ray_tracing run-headless-smoke
make -C ray_tracing visual-harness
make -C ray_tracing test-stable
make -C ray_tracing test-legacy
```

Video export helper:

```bash
make -C ray_tracing video
```

This renumbers captured BMP frames from `data/runtime/frames/default/` by default and shells out to FFmpeg to produce `data/runtime/videos/output.mp4`. Override `VIDEO_FRAMES_DIR`, `VIDEO_OUTPUT`, or `VIDEO_FPS` when needed.

## Runtime Notes

- Scene Editor modes still cycle across Bézier path, Object, and Camera editing.
- Native `3D` route activation now reflects the retained runtime-scene path rather than older preview-only truth.
- Native `3D` low-scale output now has an explicit reconstruction mode seam:
  - `OFF` for raw non-smoothed low-resolution presentation
  - `Nearest` for crisp pixel-preserving preview/final output
  - `Bilinear` for smoothed preview/final output
- Native `3D` menu controls and scene-editor digest picking now live behind dedicated helper seams instead of one monolithic UI/editor file.
- Fluid overlays remain available for imported physics data; see `docs/KEYBINDS.md` for the current control list.

## Docs

- public docs index: `docs/README.md`
- runtime controls: `docs/KEYBINDS.md`
- current-state contract: `docs/current_truth.md`
- near-term direction: `docs/future_intent.md`

Read the subdirectory READMEs under `src/` and `include/` for subsystem-specific notes.
