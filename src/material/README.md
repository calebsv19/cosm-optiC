Material System (Presets)
==========================

This module defines a small, cache-friendly material model for the 2D ray tracer and the shipped bounded native 3D integrator tiers.

Struct: `Material`
- `diffuse` (0..1) – diffuse energy fraction.
- `specular` (0..1) – specular highlight weight.
- `reflectivity` (0..1) – probability/energy scaling for reflective bounces.
- `roughness` (0..1) – 0: perfect mirror; 1: fully diffuse; blends reflected direction with randomness.
- `base_color` – neutral albedo; intended to be modulated by the SceneObject color.
- `emissive` – preset-driven emissive contribution used by the native 3D `Emission / Transparency` tier.
- `metallic`, `transparency` – bounded preset/material response inputs; `transparency` is used by the native 3D `Emission / Transparency` tier.

Presets (MaterialLibrary):
- `MATERIAL_PRESET_DEFAULT` (0): Matte; diffuse-heavy, light reflectivity.
- `MATERIAL_PRESET_MIRROR` (1): High reflectivity, zero roughness.
- `MATERIAL_PRESET_ROUGH_METAL` (2): Metallic, higher roughness and specular.
- `MATERIAL_PRESET_GLOSSY` (3): Mid reflectivity, low roughness.
- `MATERIAL_PRESET_EMISSIVE` (4): Preset-driven emissive surface with intentionally strong grayscale test output.
- `MATERIAL_PRESET_TRANSPARENT` (5): Preset-driven transmissive surface with high transparency and reduced front-face diffuse weight for clearer grayscale readback.

Notes:
- Presets use neutral base colors so SceneObject color can serve as albedo.
- Material IDs are referenced by SceneObjects; defaults to `MATERIAL_PRESET_DEFAULT`.
- The manager (`material_manager`) initializes the library and clamps invalid IDs to the default preset.
- Optional override: JSON presets in `config/materials/*.json` can redefine the library. Each file supports keys:
  `diffuse`, `specular`, `reflectivity`, `roughness`, `base_color` (array[3]), `emissive` (array[3]),
  `metallic`, `transparency`. If the directory is present at load, shipped preset filenames
  (`default.json`, `mirror.json`, `rough_metal.json`, `glossy.json`, `emissive.json`,
  `transparent.json`) override their canonical preset ids in place; extra custom files append
  after the shipped preset block.
