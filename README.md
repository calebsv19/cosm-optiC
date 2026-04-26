# RayTracing Project

Experimental 2D ray-tracing sandbox built with SDL2. The project simulates light propagation across user-authored geometry, captures animation frames, and exposes interactive editing tools with plans for future 3D experimentation.

## Docs
- Public docs index: `docs/README.md`
- Runtime controls: `docs/KEYBINDS.md`
- Scaffold references:
  - `docs/current_truth.md`
  - `docs/future_intent.md`

## Directory Layout
- `src/` – All C sources grouped by responsibility (see sub-README for details).
  - `app/` – Main runtime loop and application orchestration.
  - `ui/` – Pre-launch SDL menu.
  - `editor/` – Scene, object, and Bézier path editors.
  - `render/` – Ray-tracing engine and rendering helpers.
- `camera/` – View transform math shared by the renderer and all editors (consistent viewport + margin logic).
- `scene/`, `path/`, `config/`, `tools/` – Supporting subsystems for geometry, Bézier paths, JSON config I/O, and FFmpeg integration.
- `math/` – Vector and matrix utilities (2D/3D) used across paths, camera math, and render helpers.
- `geo/` – Shape asset library + adapters for importing external JSON shapes into scene objects; CLI tools live under `src/tools/cli`. Set `SHAPE_ASSET_DIR=/path/to/third_party/codework_shared/assets/shapes` to load the unified ShapeAsset folder (falls back to `config/objects`).
- `import/fluid_import.*` – Loader for physics_sim volume frames (`.vf2d` v1/v2), `.pack`, `manifest.json`, and `scene_bundle.json`.
- `render/fluid_overlay.*` – Debug fluid overlay with density and optional velocity arrows.
- `include/` – Public headers mirroring the `src/` hierarchy (including the camera interface).
- `docs/` – Public usage/runtime and scaffold reference docs.
- `config/` – Tracked default JSON configuration + bundled font/material/object defaults.
- `assets/` – Static/public assets lane (`assets/animations/` docs placeholders).
- `data/runtime/` – Ignored mutable runtime state lane (scene/animation state + frame/video outputs).
- `tmp/` – Ignored temporary/archive lane used for migration staging and local scratch outputs.

## Build & Run
- Dependencies: SDL2, SDL2_ttf, json-c, and FFmpeg (for MP4 assembly).
- `make` builds `Ray_anim` with objects stored in `build/`. The Makefile auto-discovers every `.c` file beneath `src/`.
- `make run` launches the executable.
- Scaffold verification aliases:
  - `make run-headless-smoke`
  - `make visual-harness`
  - `make test-stable`
  - `make test-legacy`
- `make debug` rebuilds with extra debug flags.
- `make release` / `make relrun` build and (optionally) launch an optimized binary (`Ray_anim_release`) compiled with `-O3 -ffast-math -march=native` for benchmarking. The default `make` remains the correctness build with full debug info.
- `make clean` removes the executable and build directory.
- `make video` renumbers captured BMP frames (default `data/runtime/frames/default/frame_*.bmp`) and shells out to FFmpeg at 30 fps to produce `data/runtime/videos/output.mp4`. Override `VIDEO_FRAMES_DIR`, `VIDEO_OUTPUT`, or `VIDEO_FPS` if needed.

## Render Modes
- **Forward Light Integrator** – Original visualization that emits rays from the light source, accumulating energy into tile-local buffers before tonemapping.
- **Camera Path Integrator** – New per-pixel path tracer that fires rays from the camera through the scene, supports direct-light shadow rays, Russian roulette, and configurable samples-per-pixel / max depth. Toggle the integrator and tweak its quality settings from the SDL start menu.

The in-app Scene Editor exposes three modes (Bezier path, Object, Camera). Use the on-screen buttons or press `Tab` / `Shift+Tab` to cycle between them; every mode renders through the same camera and margin settings so edits line up with what the renderer will show.

## Tooling & Assets
- `function_scanner.py` / `function_dependencies.csv` – Utility + report for mapping function definitions to call sites.

## Shared Diagnostics Contracts
- Render metrics dataset export uses shared `core_data` + `core_io` via `src/export/render_metrics_dataset.c`.
- Fluid import paths use shared `core_pack` (for `.pack`) + `core_scene` (for `scene_bundle.json`) + `core_space` (placement contract consumption).

As of 2026-03-10:
- Slice 1: low-risk `fluid_import` file/manifest helpers now use shared `core_io` (`core_io_path_exists`, `core_io_read_all`).
- Slice 2: render metrics datasets include additive schema metadata:
  - `schema_family=ray_tracing_render_metrics`
  - `schema_variant=runtime`
- Slice 3: shared theme preset persistence in `ui/shared_theme_font_adapter` now uses shared `core_io` for path/read/write operations.

As of 2026-03-11:
- Slice 5 source restore: `src/tools/cli/ray_trace_tool.c` exists again and `make ray_trace_tool` / `make manifest_to_trace` are functional.
- Current trace scaffold contract emits:
  - sample lanes: `frame_dt`, `frame_index`, `grid_cells`, `bounce_depth`
  - marker lane/labels: `events` with `trace_start` and `trace_end`
- Deterministic smoke assertions are available at `make test-manifest-to-trace-export`.
- Slice 4 pack parity guard is available at `make test-fluid-pack-contract-parity` (shared fixture parity for `VFHD/DENS/VELX/VELY` loader contract).

Read the subdirectory READMEs for detailed notes on each module. To sync shapes from the line-drawing exports into the vendored shared asset folder, run `make cli-tools` here (for `shape_asset_tool`) and then `SHAPE_ASSET_DIR=third_party/codework_shared/assets/shapes third_party/codework_shared/shape/sync_exports.sh` from the repo root. To view a physics fluid run, set `fluidManifest` in `config/config.json` (or pass `--fluid-manifest <path>`) and press `F` to show the overlay; use `V` to cycle modes (`density`, `density+velocity arrows`, `velocity heatmap+arrows`), and `[`/`]` to switch frames. Single `.vf2d`, `.pack`, or `scene_bundle.json` files can also be loaded. Menu scene discovery scans `third_party/codework_shared/assets/scenes` for shared bundles. Full runtime controls are listed in `docs/KEYBINDS.md`.
