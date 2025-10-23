# RayTracing Project

Experimental 2D ray-tracing sandbox built with SDL2. The project simulates light propagation across user-authored geometry, captures animation frames, and exposes interactive editing tools with plans for future 3D experimentation.

## Directory Layout
- `src/` – All C sources grouped by responsibility (see sub-README for details).
  - `app/` – Main runtime loop and application orchestration.
  - `ui/` – Pre-launch SDL menu.
  - `editor/` – Scene, object, and Bézier path editors.
  - `render/` – Ray-tracing engine and rendering helpers.
  - `scene/`, `path/`, `config/`, `tools/` – Supporting subsystems for geometry, Bézier paths, JSON config I/O, and FFmpeg integration.
- `include/` – Public headers mirroring the `src/` hierarchy.
- `Configs/` – JSON configuration files and bundled fonts.
- `Animations/` – Frame dumps and rendered videos created by deep-render mode.
- `Other files/` – Archived snapshots and debugging artefacts retained for reference.

## Build & Run
- Dependencies: SDL2, SDL2_ttf, json-c, and FFmpeg (for MP4 assembly).
- `make` builds `Ray_anim` with objects stored in `build/`. The Makefile auto-discovers every `.c` file beneath `src/`.
- `make run` launches the executable.
- `make debug` rebuilds with extra debug flags.
- `make clean` removes the executable and build directory.

## Tooling & Assets
- `function_scanner.py` / `function_dependencies.csv` – Utility + report for mapping function definitions to call sites.
- `output.mp4` – Example video rendered from frames under `Animations/default/`.

Read the subdirectory READMEs for detailed notes on each module.
