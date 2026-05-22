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
- `ior` – authored dielectric index of refraction for transparent materials.
- `absorption_distance` – reference distance at which `base_color` should read as the transmitted color for solid transparent materials.
- `thin_walled` – when true, transparent transport applies one authored tint layer without solid-thickness buildup and preserves straight-through transmission direction.

Per-object procedural texture controls:
- `textureId` selects the procedural overlay (`0` none, `1` rust, `2` fog).
- `textureOffsetU`, `textureOffsetV` pan the procedural sample across each hit triangle.
- `textureScale` controls pattern frequency; lower values read as broader patches and higher values as smaller/more frequent features.
- `textureStrength` controls overlay intensity. Existing scenes default to `0.0`, so procedural overlays are opt-in.
- `texturePatternMode`, `textureCoverage`, `textureGrain`, `textureEdgeSoftness`, `textureContrast`, `textureFlow`, `textureColorDepth`, `textureSurfaceDamage`, and `textureSeed` form the additive texture parameter block used by Material preview and native 3D payload sampling. Missing values normalize to the current default texture behavior.

Presets (MaterialLibrary):
- `MATERIAL_PRESET_DEFAULT` (0): Matte; diffuse-heavy, light reflectivity.
- `MATERIAL_PRESET_MIRROR` (1): High reflectivity, zero roughness.
- `MATERIAL_PRESET_ROUGH_METAL` (2): Metallic, higher roughness and specular.
- `MATERIAL_PRESET_GLOSSY` (3): Mid reflectivity, low roughness.
- `MATERIAL_PRESET_EMISSIVE` (4): Preset-driven emissive surface with moderated grayscale test output.
- `MATERIAL_PRESET_TRANSPARENT` (5): Preset-driven transmissive surface with moderately high transparency and reduced front-face diffuse weight for clearer grayscale readback.
  The shipped preset now also seeds a glass-like `ior` and a finite absorption distance for more grounded transmission behavior.

Notes:
- Presets use neutral base colors so SceneObject color can serve as albedo.
- Material IDs are referenced by SceneObjects; defaults to `MATERIAL_PRESET_DEFAULT`.
- The manager (`material_manager`) initializes the library and clamps invalid IDs to the default preset.
- Native `3D` hit shading resolves procedural overlays through `runtime_material_texture_3d` after base material lookup, using barycentric hit coordinates so the texture remains attached to the triangle as camera/light/object views change.
- Optional override: JSON presets in `config/materials/*.json` can redefine the library. Each file supports keys:
  `diffuse`, `specular`, `reflectivity`, `roughness`, `base_color` (array[3]), `emissive` (array[3]),
  `metallic`, `transparency`, `ior`, `absorption_distance`/`absorptionDistance`,
  `thin_walled`/`thinWalled`. If the directory is present at load, shipped preset filenames
  (`default.json`, `mirror.json`, `rough_metal.json`, `glossy.json`, `emissive.json`,
  `transparent.json`) override their canonical preset ids in place; extra custom files append
  after the shipped preset block.
