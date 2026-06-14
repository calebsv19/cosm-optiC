# Disney V2 D2.5 Canonical Proofs

Status: generated locally 2026-06-13.

These proof renders exercise the experimental `disney_v2` route through the
headless runtime-scene path. They do not promote or alter the shipped `disney`
integrator.

## Requests

- primitive glass corridor:
  `tests/fixtures/disney_v2_d25/primitive_glass_corridor_request.json`
- imported mesh material:
  `tests/fixtures/disney_v2_d25/imported_mesh_material_request.json`
- proof runner:
  `tests/integration/run_ray_tracing_render_headless_disney_v2_d25.sh`

## Primitive Glass Corridor

- summary:
  `build/agent_runs/ray_tracing/disney_v2_d25/primitive_glass_corridor/render_summary.json`
- progress:
  `build/agent_runs/ray_tracing/disney_v2_d25/primitive_glass_corridor/render_progress.json`
- selected frame:
  `build/agent_runs/ray_tracing/disney_v2_d25/primitive_glass_corridor/frames/frame_0000.bmp`
- route: `disney_v2`
- render stats: `visible_pixels=5338`, `nonzero_pixels=20736`,
  `max_radiance=0.550802415`, `max_rgb=[125, 136, 144]`
- BVH: `ready=true`, `triangle_count=26`, `node_count=15`,
  `leaf_count=8`, `trace_calls=51331`, `trace_overflows=0`
- object/material audit:
  - `plane_floor`: `material_id=4`, `alpha=1.0`, `roughness=0.65`,
    `emissive_strength=0.093023`, `triangle_count=2`,
    `primary_hit_pixels=4233`
  - `prism_center`: `material_id=3`, `alpha=1.0`, `roughness=0.65`,
    `emissive_strength=0.104651`, `triangle_count=12`,
    `primary_hit_pixels=866`
  - `prism_offset`: `material_id=5`, `alpha=0.616822`,
    `roughness=0.65`, `emissive_strength=1.0`, `triangle_count=12`,
    `primary_hit_pixels=239`

## Imported Mesh Material

- summary:
  `build/agent_runs/ray_tracing/disney_v2_d25/imported_mesh_material/render_summary.json`
- progress:
  `build/agent_runs/ray_tracing/disney_v2_d25/imported_mesh_material/render_progress.json`
- selected frame:
  `build/agent_runs/ray_tracing/disney_v2_d25/imported_mesh_material/frames/frame_0000.bmp`
- route: `disney_v2`
- render stats: `visible_pixels=14165`, `nonzero_pixels=20736`,
  `max_radiance=0.541403282`, `max_rgb=[143, 138, 142]`
- BVH: `ready=true`, `triangle_count=3974`, `node_count=2047`,
  `leaf_count=1024`, `trace_calls=120804`, `trace_overflows=0`
- object/material audit:
  - `obj_floor`: `material_id=0`, `alpha=1.0`, `roughness=0.8`,
    `triangle_count=2`, `primary_hit_pixels=6216`
  - `obj_back_wall`: `material_id=0`, `alpha=1.0`, `roughness=0.75`,
    `triangle_count=2`, `primary_hit_pixels=4120`
  - `obj_left_wall`: `material_id=0`, `alpha=1.0`, `roughness=0.75`,
    `triangle_count=2`, `primary_hit_pixels=3505`
  - `obj_sphere_pressure`: `material_id=0`, `alpha=1.0`,
    `roughness=0.28`, `triangle_count=3968`,
    `primary_hit_pixels=324`

## Verification

```bash
make -C /Users/calebsv/Desktop/CodeWork/ray_tracing build/toolchains/clang/arm64/tools/cli/ray_tracing_render_headless
ray_tracing/tests/integration/run_ray_tracing_render_headless_disney_v2_d25.sh
```
