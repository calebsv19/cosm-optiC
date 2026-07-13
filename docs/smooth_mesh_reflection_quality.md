# Smooth Imported-Mesh Reflection Quality

RayTracing can shade imported STL geometry with generated per-vertex normals
without changing the STL file format. STL still supplies tessellated positions;
`core_mesh_compile` welds indexed topology and emits optional normals in the
`mesh_asset_runtime_v1` sidecar.

## Runtime policy

- `Ng` is the world-space geometric face normal. Intersection orientation,
  sidedness, ray offsets, and self-intersection safety use `Ng`.
- `Ns` is the normalized barycentric interpolation of runtime vertex normals.
  Direct, metal, mirror, and BSDF evaluation use `Ns`.
- Missing or invalid optional normals fall back to `Ns = Ng`, preserving old
  position-only runtime assets.
- Non-uniform instance scale transforms normals with the inverse transpose.
- Set `RAY_TRACING_MESH_SHADING_MODE=flat` to reproduce face-normal shading.

A denser STL improves the actual silhouette. Interpolated `Ns` removes
face-normal highlight stepping, but cannot hide a visibly polygonal outline.
Mesh adjacency comes from welded indices and edge connectivity, not BVH
proximity queries.

## Deterministic fixtures and matrix

Generate and compile the bounded fixture ladder:

```sh
make smooth-mesh-runtime-compile-tool
make test-smooth-mesh-reflection-fixtures
```

Run the high-tier analytic sphere, icosphere, organic blob, and crease scene in
smooth/flat and TLAS/BLAS/flattened modes:

```sh
make test-smooth-mesh-reflection-matrix
```

Outputs are written under
`build/agent_runs/ray_tracing/smooth_mesh_reflection_matrix/`. The generated
`matrix_report.json` records asset normal provenance/counts, image hashes,
route mismatches, render timings, and the smooth-versus-flat timing delta.

The reusable fixture generator supports exact density tiers:

- `high`: 20,480 triangles;
- `very_high`: 327,680 triangles;
- `ultra`: 1,310,720 triangles.

Large tiers are pressure/visual gates and are intentionally excluded from the
routine smoke target. Generate them on demand with
`tools/smooth_mesh_reflection/generate_fixtures.py --tier ultra` and compile
the emitted authoring JSON with the `smooth-mesh-runtime-compile-tool` binary.

## Acceptance readback

A trustworthy close-up should show all of the following:

- a smooth silhouette at the chosen density;
- continuous mirror and rough-metal highlights on smooth fixtures;
- preserved hard edges on the crease fixture;
- different smooth and flat image hashes;
- zero route parity mismatches, traversal overflows, and silent fallbacks.

Cold JSON load/normal payload cost must be reported separately from steady-state
render time. A same-scene unexplained render regression above 10 percent is a
review blocker for this lane.
