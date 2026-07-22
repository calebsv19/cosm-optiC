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
- a packaged, self-contained Build Week showcase with three original procedural
  STL meshes and editable mirror, glossy, and metal materials

## Implemented Today

- native `3D` output is now RGB-aware through the full shipped ladder
- a default-off production photon-mapping lane is implemented through PPM-23,
  with deterministic emission, general mixed-BSDF scene traversal, multi-bounce
  and nested-medium transport, measured attenuation, transactional surface/beam
  maps, and headless comparison proof; the desktop Caustics control now exposes
  `Off`, both retained reference modes, and `Photon Map (Experimental)`, with
  the photon render-prep population/contribution route applied only when that
  product is selected; it is not yet a promoted product default
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

## OpenAI Build Week 2026

`optiC` is a pre-existing project being meaningfully extended during the
OpenAI Build Week submission period (July 13-21, 2026). The frozen pre-period
baseline is commit `bd8ee85dae5e62707810065d413d94c24475535b`.
The public commit link will be added after the Build Week branch is pushed;
the current public GitHub branch does not yet contain this baseline.

Build Week work after that baseline includes imported-mesh editor preview,
large-mesh LOD recovery, smoother mesh shading and reflection behavior,
viewport navigation improvements, cross-platform authored-texture selection,
asynchronous deep-render completion, and whole-object origin selection. The
submission candidate now includes a self-contained installed demo path at
`config/samples/optic_build_week_showcase/`. Judges can open the packaged scene,
inspect three original imported meshes, edit their materials, and render without
depending on the maintainer's PhysicsSim or LineDrawing workspace. The focused
proof is `make test-optic-build-week-showcase`; full installation and judge
testing instructions are in
[`docs/build_week_judge_guide.md`](docs/build_week_judge_guide.md).

### How Codex and GPT-5.6 contributed

The project was developed iteratively with Codex and GPT-5.6. Codex helped
trace renderer and editor behavior across a large C codebase, build focused
fixtures, compare visual and machine-readable proof, isolate performance and
material-policy defects, maintain release tooling, and reconcile public and
private documentation. The maintainer chose the creative direction, visual
acceptance thresholds, architecture boundaries, release scope, and which
experiments were accepted, deferred, or rejected.

The final eligible end commit, release artifact checksum, and principal Codex
`/feedback` Session ID will be recorded here at submission freeze. The stable
Build Week demo is documented in
[`config/samples/optic_build_week_showcase/README.md`](config/samples/optic_build_week_showcase/README.md);
the broader source-checkout demo path remains in
[`docs/AGENT_DEMO_PACK.md`](docs/AGENT_DEMO_PACK.md).

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
make -C ray_tracing visual-artifact
make -C ray_tracing test-optic-build-week-showcase
make -C ray_tracing test-stable
make -C ray_tracing test-legacy
```

`visual-harness` is a build/readiness gate for the app binary. `visual-artifact`
is the unattended source-run proof: it renders and validates a first-frame BMP,
writes validation metrics, and prints:

```text
ray_tracing visual artifact ready: <repo>/ray_tracing/visual_artifacts/source_first_frame/frames/frame_0000.bmp
```

Generated visual proof files live under the ignored
`ray_tracing/visual_artifacts/` root.

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
- Build Week installation/testing guide: `docs/build_week_judge_guide.md`
- agent-control guide: `docs/AGENT_CONTROL.md`
- canonical agent demo pack: `docs/AGENT_DEMO_PACK.md`
- runtime controls: `docs/KEYBINDS.md`
- current-state contract: `docs/current_truth.md`
- near-term direction: `docs/future_intent.md`

Read the subdirectory READMEs under `src/` and `include/` for subsystem-specific notes.
