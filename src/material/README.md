Material System (Presets)
==========================

This module defines a small, cache-friendly material model for the 2D ray tracer and future 3D expansion.

Struct: `Material`
- `diffuse` (0..1) – diffuse energy fraction.
- `specular` (0..1) – specular highlight weight.
- `reflectivity` (0..1) – probability/energy scaling for reflective bounces.
- `roughness` (0..1) – 0: perfect mirror; 1: fully diffuse; blends reflected direction with randomness.
- `base_color` – neutral albedo; intended to be modulated by the SceneObject color.
- `emissive` – reserved for future emissive support.
- `metallic`, `transparency` – reserved for future use.

Presets (MaterialLibrary):
- `MATERIAL_PRESET_DEFAULT` (0): Matte; diffuse-heavy, light reflectivity.
- `MATERIAL_PRESET_MIRROR` (1): High reflectivity, zero roughness.
- `MATERIAL_PRESET_ROUGH_METAL` (2): Metallic, higher roughness and specular.
- `MATERIAL_PRESET_GLOSSY` (3): Mid reflectivity, low roughness.
- `MATERIAL_PRESET_EMISSIVE` (4): Placeholder with small emissive value (not yet used in integrator).

Notes:
- Presets use neutral base colors so SceneObject color can serve as albedo.
- Material IDs are referenced by SceneObjects; defaults to `MATERIAL_PRESET_DEFAULT`.
- The manager (`material_manager`) initializes the library and clamps invalid IDs to the default preset.
- Optional override: JSON presets in `Configs/materials/*.json` can redefine the library. Each file supports keys:
  `diffuse`, `specular`, `reflectivity`, `roughness`, `base_color` (array[3]), `emissive` (array[3]),
  `metallic`, `transparency`. If the directory is present at load, the library is rebuilt from those files (order is file order).
