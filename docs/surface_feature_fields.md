# Surface Feature Fields (PSG-24A)

`surface_feature_field_v1` is a digest-bound, material-only derived asset for
discrete spots. Author `spot_scatter` populations in calibrated object units;
each accepted feature retains source-triangle and barycentric provenance,
normal/tangent frame, radius, aspect, rotation, population, and stable ID.

Use `tools/procedural_surface_feature_field_authoring.py` to compile a field
and its receipt. The bundle exposes `surface_features/dirt/coverage`,
`interior`, `rim`, and `feature_id` as named entrypoints. Sampling rejects
normal-incompatible nearby folds and uses a bounded grid lookup.

The `feature_coverage`, `feature_interior`, `feature_rim`, `feature_id`,
`clamp`, `subtract`, and `remap` material-graph nodes support composition.
They do not alter meshes, silhouettes, acceleration, or hit topology. PSG-17
normal response, PSG-20/21 insets, and PSG-22 deposits remain separate lanes.

For a native runtime scene, add `surface_feature_field_path` beside
`graph_path` in that mesh object's `procedural_solid_material_ref`. The loader
resolves the path relative to the runtime scene, requires the field's source
mesh digest to match the immutable runtime mesh, tracks the field as an asset
dependency, and rejects conflicting bindings across instances of one mesh
asset. When an imported runtime mesh does not serialize vertex normals, the
material runtime derives deterministic area-weighted normals for feature-field
compatibility tests without changing the mesh document or its digest.
Omitting the field reference preserves legacy material behavior.
