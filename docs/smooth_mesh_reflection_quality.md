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

The high-tier matrix is the correctness/parity gate. For a human-readable
low-poly comparison, use a purpose-built small fixture as well: smooth shading
changes BRDF/reflection response but does not add silhouette geometry. The
current 120-triangle rounded-slab probe confirms distinct flat, smooth, and
60-degree crease-aware behavior. Its earlier fully smooth render exposed a dark
diagonal wedge because the perfect-specular path reflected directly around an
unconstrained interpolated `Ns`. The runtime now blends `Ns` toward `Ng` only as
far as necessary to keep the reflected ray in the geometric surface hemisphere;
the fresh smooth rerender removes that wedge, preserves smooth response, and
keeps the crease-aware hard-edge result distinct.

This acceptance is provisional until the policy is exercised on a real
imported asset compiled after the menu setting is chosen. Reopen the lane if a
close reflective view of that asset shows a diagonal dark wedge, a discontinuity
that tracks triangle boundaries, or loss of an intended hard crease. Capture the
asset sidecar, selected normal mode/crease angle, camera/light/material settings,
and a flat-versus-smooth-versus-crease-aware comparison before changing the
normal algorithm again.

Cold JSON load/normal payload cost must be reported separately from steady-state
render time. A same-scene unexplained render regression above 10 percent is a
review blocker for this lane.
