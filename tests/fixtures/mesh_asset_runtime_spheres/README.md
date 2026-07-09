# Mesh Asset Runtime Spheres MRT0 Fixtures

These fixtures prove the first RayTracing runtime mesh-asset contract before
LineDrawing can emit authored object assets.

## Contents

- `scene_runtime.json`
  - room-style `scene_runtime_v1` fixture
  - three `mesh_asset_instance` objects:
    - `obj_sphere_low` -> `asset_sphere_8x4`
    - `obj_sphere_medium` -> `asset_sphere_16x8`
    - `obj_sphere_high` -> `asset_sphere_32x16`
- `scene_runtime_pressure.json`
  - MRT6 pressure scene with one high-quality mesh instance:
    - `obj_sphere_pressure` -> `asset_sphere_64x32`
- `scene_runtime_pressure_mrt8.json`
  - MRT8 larger pressure scene with one higher-quality mesh instance:
    - `obj_sphere_pressure_mrt8` -> `asset_sphere_128x64`
- `scene_runtime_pressure_mrt10.json`
  - MRT10 `64k+` triangle pressure scene with one higher-quality mesh
    instance:
    - `obj_sphere_pressure_mrt10` -> `asset_sphere_256x128`
- `assets/mesh_assets/*.runtime.json`
  - generated `mesh_asset_runtime_v1` UV-sphere assets
- `fixture_summary.json`
  - expected vertex and triangle counts

## Counts

| Asset | Segments | Rings | Vertices | Triangles |
| --- | ---: | ---: | ---: | ---: |
| `asset_sphere_8x4` | 8 | 4 | 26 | 48 |
| `asset_sphere_16x8` | 16 | 8 | 114 | 224 |
| `asset_sphere_32x16` | 32 | 16 | 482 | 960 |
| `asset_sphere_64x32` | 64 | 32 | 1986 | 3968 |
| `asset_sphere_128x64` | 128 | 64 | 8066 | 16128 |
| `asset_sphere_256x128` | 256 | 128 | 32514 | 65024 |

## Regenerate

```sh
python3 ray_tracing/tools/generate_mesh_asset_sphere_fixtures.py
```

## Validate

```sh
python3 ray_tracing/tools/generate_mesh_asset_sphere_fixtures.py --check
```

The check validates expected counts, triangle index ranges, non-degenerate
outward winding, surface-group spans, and scene references for the base sphere
family plus the MRT6, MRT8, and MRT10 pressure assets.
