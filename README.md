# RayTracing Project

Experimental 2D ray-tracing sandbox built with SDL2. The project simulates light propagation across user-authored geometry, captures animation frames, and exposes interactive editing tools with plans for future 3D experimentation.

## Directory Layout
- `src/` – All C sources grouped by responsibility (see sub-README for details).
  - `app/` – Main runtime loop and application orchestration.
  - `ui/` – Pre-launch SDL menu.
  - `editor/` – Scene, object, and Bézier path editors.
  - `render/` – Ray-tracing engine and rendering helpers.
- `camera/` – View transform math shared by the renderer and all editors (consistent viewport + margin logic).
- `scene/`, `path/`, `config/`, `tools/` – Supporting subsystems for geometry, Bézier paths, JSON config I/O, and FFmpeg integration.
- `math/` – Vector and matrix utilities (2D/3D) used across paths, camera math, and render helpers.
- `geo/` – Shape asset library + adapters for importing external JSON shapes into scene objects; CLI tools live under `src/tools/cli`.
- `include/` – Public headers mirroring the `src/` hierarchy (including the camera interface).
- `Configs/` – JSON configuration files and bundled fonts.
- `Animations/` – Frame dumps and rendered videos created by deep-render mode.
- `Other files/` – Archived snapshots and debugging artefacts retained for reference.

## Build & Run
- Dependencies: SDL2, SDL2_ttf, json-c, and FFmpeg (for MP4 assembly).
- `make` builds `Ray_anim` with objects stored in `build/`. The Makefile auto-discovers every `.c` file beneath `src/`.
- `make run` launches the executable.
- `make debug` rebuilds with extra debug flags.
- `make release` / `make relrun` build and (optionally) launch an optimized binary (`Ray_anim_release`) compiled with `-O3 -ffast-math -march=native` for benchmarking. The default `make` remains the correctness build with full debug info.
- `make clean` removes the executable and build directory.
- `make video` renumbers captured BMP frames (default `Animations/default/frame_*.bmp`) and shells out to FFmpeg at 30 fps to produce `Animations/Vids/output.mp4`. Override `VIDEO_FRAMES_DIR`, `VIDEO_OUTPUT`, or `VIDEO_FPS` if needed.

## Render Modes
- **Forward Light Integrator** – Original visualization that emits rays from the light source, accumulating energy into tile-local buffers before tonemapping.
- **Camera Path Integrator** – New per-pixel path tracer that fires rays from the camera through the scene, supports direct-light shadow rays, Russian roulette, and configurable samples-per-pixel / max depth. Toggle the integrator and tweak its quality settings from the SDL start menu.

The in-app Scene Editor exposes three modes (Bezier path, Object, Camera). Use the on-screen buttons or press `Tab` / `Shift+Tab` to cycle between them; every mode renders through the same camera and margin settings so edits line up with what the renderer will show.

## Tooling & Assets
- `function_scanner.py` / `function_dependencies.csv` – Utility + report for mapping function definitions to call sites.
- `output.mp4` – Example video rendered from frames under `Animations/default/`.

Read the subdirectory READMEs for detailed notes on each module.
