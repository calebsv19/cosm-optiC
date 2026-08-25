# RT-MIRROR-1 Boundary 1 Baseline

This fixture is the retained, unchanged-renderer before-state for
`RT-MIRROR-1`. One `generated_smooth` organic runtime mesh is visible directly
and through a low-roughness mirror floor. Red and green reference markers make
reflection orientation and lost reflected contribution easier to diagnose.

The fixture is intentionally small but already crosses the current
rough-reflection `512`-triangle cliff: the smooth mesh has `1,280` triangles.
The scene has no external dependencies. Its complete source and runtime mesh
lineage is retained under `source/` and `assets/mesh_assets/`.

Two requests keep every setting equal except final reconstruction:

- `request_raw.json`: Disney v2, 12 temporal frames, denoise disabled;
- `request_resolved.json`: Disney v2, 12 temporal frames, denoise enabled.

Both requests use a `1.6` camera zoom so the direct mesh and its reflected
silhouette occupy substantial, simultaneously visible regions. This preserves
the direct view as the normal/material reference while making the defective
reflected response large enough for visual review.

Run the Boundary 1 gate from the repository root:

```sh
make test-ray-tracing-mirror-baseline-contract
```

Generated evidence is written under the ignored root:

```text
build/agent_runs/ray_tracing/rt_mirror_1_baseline/
```

That root retains both request projections, summaries, progress files, raw and
resolved BMPs, a repeat-render hash check, and `baseline_report.json`. The
tracked `baseline_expected.json` records the source-baseline identity and
authoritative hashes needed to reproduce this before-state from a clean
checkout.

The original August room screenshots remain qualitative operator evidence.
This fixture is the deterministic regression authority because the complete
source scene and settings behind those screenshots were not retained.
