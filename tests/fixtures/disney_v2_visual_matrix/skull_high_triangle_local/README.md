# Skull High-Triangle Local Matrix

This fixture is prepared into the private workspace because the skull runtime
sidecar is large and lives outside the RayTracing repository.

Prepare the local proof package:

```bash
ray_tracing/tests/integration/prepare_ray_tracing_skull_high_triangle_matrix.py
```

The preparer copies the local skull and stepped-column runtime sidecars into a
private scene package, rewrites `runtime_mesh_path` fields to relative
`assets/mesh_assets/...` paths, and writes a small visual-matrix manifest. The
matrix can then be rendered with:

```bash
ray_tracing/tests/integration/run_ray_tracing_visual_matrix.py \
  --manifest _private_workspace_artifacts/agent_runs/ray_tracing/disney_v2_visual_matrix/skull_high_triangle_local/source_scene/matrix_manifest.json
```

This keeps the repo fixture portable without committing the 18 MB skull runtime
mesh sidecar into source control.
