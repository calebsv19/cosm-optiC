# src

All application sources organised by responsibility. Each subdirectory has a matching header namespace under `include/`.

- `app/` – Entry point and runtime loop (`app/README.md`).
- `ui/` – SDL menu displayed before launching the renderer (`ui/README.md`).
- `editor/` – Scene-editing tools for objects, Bézier paths, and the interactive camera controller (`editor/README.md`).
- `render/` – Ray-tracer implementation and shared rendering helpers (`render/README.md`).
- `camera/` – Viewport math and world/screen transform helpers (`camera/README.md`).
- `scene/` – Scene object management utilities (`scene/README.md`).
- `path/` – Bézier path math powering scripted light motion (`path/README.md`).
- `config/` – JSON loading/saving of animation and scene settings (`config/README.md`).
- `import/` – Shared scene/pack import adapters, including runtime-scene contract preflight for trio handoff readiness.
- `tools/` – Auxiliary modules such as FFmpeg integration (`tools/README.md`).
- `math/` – Core vector/matrix utilities (2D/3D) shared by paths, camera, and render math.
- `geo/` – Shape asset library (geolib) plus adapter code to convert imported shapes into scene objects.
