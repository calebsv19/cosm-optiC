# Surface Feature Fields (PSG-24A through PSG-24D)

`surface_feature_field_v1` is a digest-bound morphology asset for discrete
spots. Author `spot_scatter` populations in calibrated object units;
each accepted feature retains source-triangle and barycentric provenance,
normal/tangent frame, radius, aspect, rotation, population, and stable ID.

Use `tools/procedural_surface_feature_field_authoring.py` with the source
runtime mesh and its solid receipt to compile a field and receipt. A population
may specify an exact `count` or `density_per_square_unit`, plus calibrated
radius/aspect ranges, rotation, edge softness, rim width, clustering, jitter,
seed, and a signed object-unit `height_or_depth` scalar or range. Zero denotes
material-only influence. Signed values can drive the shallow, one-shell relief
adapter described below. Deep negative cuts remain a PSG-24C wall/floor inset;
genuinely separate mud, moss, or other attached pieces may use PSG-24D.
The optional macro envelope combines receiver-facing and height
bounds with signed shared-edge concavity and a bounded surface-distance
falloff; it can concentrate deposits in troughs and creases without treating
a nearby opposing fold as the same surface.

The compile receipt records accepted population counts, radius/aspect
quantiles, signed height/depth quantiles, concave-edge and envelope metrics, provenance/frame errors,
normal-incompatible candidate pairs, coverage bounds, and compiled grid
occupancy. The bundle exposes `surface_features/dirt/coverage`, `interior`,
`rim`, `height_or_depth`, `feature_id`, `tangent_direction`, and
`macro_envelope` as named entrypoints. Sampling rejects normal-incompatible
nearby folds and uses a bounded 32x32 candidate lookup; native hits never scan
the entire feature set.

```sh
python3 tools/procedural_surface_feature_field_authoring.py \
  --authoring tests/fixtures/procedural_surface_feature_fields_psg24a/curved_plaster_spots.authoring.json \
  --mesh path/to/source.runtime.json \
  --solid-receipt path/to/solid.json \
  --output-root path/to/field_bundle
```

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
Omitting the field reference preserves legacy material behavior. The
purpose-fit proof runner accepts `--surface-feature-authoring`; it regenerates
the closed curved plaster source, compiles against that exact mesh identity,
and emits native hero/close/grazing beauty and coverage/interior/rim views plus
object-aligned feature-ID, macro-envelope, normal, provenance, and exact-repeat
diagnostics.

## Signed selected-face relief

For planes and prism faces, the same signed spot field can displace a refined
PSG-18 selected-face shell. Negative samples move into the host and positive
samples move out of it, while the source cage, selected-face boundary, and all
unselected faces stay fixed. The result is one closed derived mesh: it is the
direct route for shallow concrete pores and aggregate, wood dents and knots,
and similar relief that belongs to the surface rather than to detached assets.

```sh
build/toolchains/clang/arm64/tools/cli/procedural_surface_field_preset_asset_tool \
  --graph path/to/material_graph.json \
  --binding path/to/material.binding.json \
  --base-recipe path/to/base.recipe.json \
  --recipe-out path/to/derived.recipe.json \
  --asset-out path/to/derived.runtime.json \
  --material-out path/to/material.artifact.json \
  --manifest-out path/to/bundle.manifest.json \
  --summary-out path/to/compile.summary.json \
  --width 6 --height 0.5 --depth 4 \
  --target-edge 0.04 --amplitude 0.10 --edge-lock 0.28 \
  --source-asset-id semantic_wall --asset-id formed_concrete_wall \
  --selected-face positive_y \
  --surface-feature-field path/to/surface_feature_field_v1.json \
  --feature-source-mesh-digest exact_flat_source_mesh_sha256 \
  --relief-scale 1.0
```

The recipe amplitude is the maximum displacement budget; every scaled field
value must fit inside it. The `signed_feature_relief` receipt binds the exact
field and source-mesh digests and records negative/positive feature counts,
inward/outward displaced vertices, emitted range, and the bounded compile-time
candidate scan. Native field sampling retains its 32x32 index contract.
For formed-concrete pores in the 0.07–0.12-unit radius range, the visual proof
uses a 0.04-unit relief lattice; the coarser 0.10-unit lattice can collapse a
dimple into one triangular cavity face. This is a tessellation-quality control,
not a change to the field identity or cage-edge contract.

## Surface curves and scratches

PSG-24B adds the separate `surface_feature_curve_field_v1` derived asset for
surface-projected scratches. Its deterministic compiler traces contour-following
polyline segments across mesh adjacency, retaining source triangle,
barycentric start/end roots, compatible normals, tangent, tapered width/depth,
stable curve and segment IDs, and parent-curve identity for bounded branches.
The field samples a distance-to-curve capsule with continuous `coverage`,
`interior`, `rim`, signed depth, depth slope, and tangent direction. Shared-edge
endpoints are identical, opposing-normal folds are rejected, and a bounded
32x32 lookup prevents native hits from scanning every segment.

```sh
python3 tools/procedural_surface_feature_curve_authoring.py \
  --authoring tests/fixtures/procedural_surface_feature_fields_psg24b/curved_plaster_scratches.authoring.json \
  --mesh path/to/source.runtime.json \
  --solid-receipt path/to/solid.json \
  --output-root path/to/curve_field_bundle
```

For a native runtime scene, set `surface_feature_curve_field_path` beside the
material `graph_path`. Existing feature-channel graph nodes consume curve
coverage/interior/rim/ID, while the authored-material runtime uses the signed
cross-curve depth slope and tangent carrier for PSG-17-style shading-normal
grooves. This is currently a material and shading-normal claim only: the source
mesh, silhouette, acceleration, and primary-hit topology remain unchanged.
Physical curve relief is a later adapter boundary; deep scratch insets belong
to PSG-24C, and optional separate deposits belong to PSG-24D.

## Deep physical feature insets

PSG-24C consumes an accepted spot field and an explicit comma-separated set
of stable feature IDs as physical geometry. Every selected feature must carry
an explicitly negative `height_or_depth`; zero or positive selections fail
closed. The compiler first derives a
vertex carrier, extracts the carrier-supported source-triangle neighborhood
and one closure/stitch ring, and only then invokes the PSG-21 adaptive inset
compiler. The immutable source remains separate; the output is a distinct
closed derived shell with `retained_surface`, `transition_wall`, and
`inset_floor` groups.

```sh
python3 tools/procedural_surface_feature_inset_compiler.py \
  --selection-tool build/toolchains/clang/arm64/tools/cli/procedural_surface_feature_selection_tool \
  --inset-tool build/toolchains/clang/arm64/tools/cli/procedural_imported_surface_inset_tool \
  --mesh path/to/source.runtime.json \
  --field path/to/surface_feature_field_v1.json \
  --base-region path/to/exact_source.region.json \
  --feature-ids 4202831276,3010350944 \
  --out-root path/to/feature_inset_bundle \
  --derived-asset-id physical_feature_inset
```

The receipt binds the source, field, explicit ID list, carrier, derived mesh,
and feature-aware provenance digests. It records local support/stitch counts
and whole-mesh reduction, retained/wall/floor counts, selected islands, depth
range error, and unchanged unselected vertices. Per-derived-triangle
provenance carries the source triangle, selected feature ID, and topology
role. The emitted bundle exposes named `semantic_source`,
`surface_feature_field`, `selected_feature_ids`, and
`physical_surface_inset/{derived_shell,retained_surface,transition_wall,inset_floor}`
entrypoints.

This is true topology-changing depth and therefore requires source/hero,
grazing, topology-role, depth-delta, feature-ID/provenance, and zero-repeat
proof. It is not PSG-24B shading-normal microdetail, PSG-24D attached growth,
or a source-mesh overwrite.

## Optional attached positive-height deposits

PSG-24D consumes an exact spot field plus an explicit list of positive-height
feature IDs and emits one separate PSG-22 growth asset per accepted feature.
Every selected feature must carry an explicitly positive `height_or_depth`;
zero or negative selections fail closed. Authored height controls exposed
height, while the adapter keeps the ellipsoid equator at or below the root
plane so its visible perimeter emerges from the host instead of floating.
Each asset is rooted on the feature's retained source triangle and barycentric
coordinate, rotates and stretches the PSG-22 mound through the retained tangent
frame, and remains separate from both the immutable source and any PSG-24C
inset shell.

```sh
python3 tools/procedural_surface_feature_deposit_compiler.py \
  --selection-tool build/toolchains/clang/arm64/tools/cli/procedural_surface_feature_selection_tool \
  --growth-tool build/toolchains/clang/arm64/tools/cli/procedural_imported_surface_growth_tool \
  --mesh path/to/source.runtime.json \
  --field path/to/surface_feature_field_v1.json \
  --base-region path/to/exact_source.region.json \
  --feature-ids 4202831276,3010350944 \
  --out-root path/to/feature_deposit_bundle \
  --material-semantic dried_mud_deposit
```

The composite receipt binds the immutable source, field, selected IDs,
per-feature carrier, separate mesh assets, and aggregate provenance. It
requires one closed positive-volume component per accepted feature, positive
attachment depth, conservative cross-asset clearance, zero forbidden overlap
or self-intersection pairs, and one material semantic shared by the underlying
field selection and attached element. Per-triangle provenance retains feature
ID, population, feature and attachment source triangle, barycentric root,
element index, role, and material semantic. Named bundle entrypoints expose
the source, field, positive selection, attached asset array, material,
provenance, and receipt.

Use this only when the authored feature is genuinely a separate attached
element. It is not the general solution for raised concrete aggregate, wood
knots, or other relief that should remain part of one surface shell. This is
separate attached geometry, not a Boolean union, conformal
multi-triangle footprint, inset role, or source-mesh replacement. Strongly
curved footprints can still be partially occluded because PSG-22 remains a
single-root local-frame mound; manual visual acceptance is separate from its
closure, attachment, and repeat gates.

## Wood grain runtime consumption

`ray_tracing.wood_grain_field_v1` is an editable, digest-bound surface input.
When a mesh instance supplies its path and matching preset digest through its
procedural solid material reference, the runtime validates the exact mesh it
will consume, samples chroma and roughness at each native hit point, and uses
the same field derivative for shading-normal perturbation.

`ray_tracing.wood_surface_preset_v1` now carries four named grain-relief
profiles in object units:

| Profile | Geometry | Maximum height |
| --- | --- | --- |
| `texture_only` | none | `0.0` |
| `height_subtle` | closed PSG-18 selected-face shell | `0.004` |
| `height_standard` | closed PSG-18 selected-face shell | `0.012` |
| `height_exaggerated` | closed PSG-18 selected-face shell | `0.028` |

Use `tools/procedural_surface_wood_relief_profile.py` to resolve a selected
profile into a digest-bound `wood_grain_relief_request_v1`. `texture_only`
keeps the selected-face topology fixed while preserving grain chroma and
normal response. Every height profile requests a distinct closed derived shell
whose maximum physical displacement is the selected object-unit value; it
does not merely strengthen a normal map. The semantic cage, cage perimeter,
and unselected faces remain fixed under the selected-face-shell contract.

Material, shading normal, shallow grain height, shallow signed knot relief,
and deferred deep topology stay separate claims. The grain field does not
create attached assets, and deep wood cuts remain a separate PSG-24C decision.
