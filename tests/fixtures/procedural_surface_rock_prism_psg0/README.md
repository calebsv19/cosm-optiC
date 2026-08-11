# PSG-0 Procedural Surface Contract Fixture

This fixture freezes the input, field, and topology proof contract before any
displaced mesh exists.

- `recipe.json` defines recipe schema v1 in object units.
- `cages.json` defines the first open plane and closed rectangular-prism cages.
- `sample_points.json` freezes the PSG-1 object-space samples.
- `expected_field_summary.json` freezes evaluator outputs and determinism
  invariants plus the canonical ordered field-summary digest.
- `expected_topology_summary.json` freezes subdivision-derived count and shell
  requirements without claiming that PSG-0 or PSG-1 generated either mesh.
- `expected_plane_mesh_summary.json` freezes the PSG-2 displaced-plane bounds,
  safety metrics, field-evaluation count, and canonical mesh digest.
- `expected_prism_mesh_summary.json` freezes the PSG-3 welded, displaced,
  watertight-shell metrics and canonical mesh digest.
- `expected_material_summary.json` freezes PSG-4 recipe/shell/material
  identity, snow coverage, and roughness ranges over all 834 retained vertex
  samples.
- `graph.json` is the PSG-6 typed, acyclic, UI-free graph for the same recipe.
- `expected_graph_summary.json` freezes graph/recipe/downstream identities,
  evaluation counts, output domains, and required rejection behavior.
- `visual_proof_contract.json` freezes the PSG-3V control/subject relationship,
  eight camera views, fixed light rig, render resolution, expected object-hit
  silhouettes, tonal floor, rejection conditions, the PSG-4 diagnostic
  material cells, and PSG-5 native runtime/reopen checks.

`base_feature_size_units` and `micro_feature_size_units` describe visible
surface-feature scale. `target_edge_length_units` independently controls the
sampling/tessellation resolution needed to represent those features. Scaling
an object therefore does not silently redefine what a one-unit divot means.

The plane is the first PSG-2 generation target. The prism is the completed
PSG-3 watertight-shell and runtime-mesh handoff target. PSG-3V renders that
same shell through the established headless mesh-asset lane; it does not
change the fixture recipe, shell authority, or saved-scene state.
PSG-4 adds coupled material readback and previews from the retained mesh
records. PSG-5 persists the derived-asset identity, binds the same material
records to native ray hits, proves save/reopen equality and acceleration
reuse, and rejects a changed recipe before rendering.
PSG-6 compiles the typed graph back into that exact recipe and requires all
PSG-0 through PSG-5 identities to remain unchanged.
