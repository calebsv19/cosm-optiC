# include

Public headers mirroring the `src/` hierarchy. Include them using their directory-qualified paths (e.g., `#include "render/ray_tracing2.h"`). Subdirectories match the source layout:

- `app/` – Interfaces for application lifecycle (`animation.h`).
- `ui/` – Menu APIs (`sdl_menu.h`).
- `editor/` – Scene/object/Bézier editor contracts.
- `render/` – Ray-tracing and rendering helpers.
- `scene/` – Scene-object structures and utilities.
- `path/` – Bézier path types and helpers.
- `config/` – Animation/scene configuration structs.
- `tools/` – Auxiliary tools such as video generation.
