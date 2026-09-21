# Surface material M1: common planar sampling

M1 implements a versioned planar mapping binding and a common primitive material
query. It stops before M2 axial mapping and new inspector controls. See
[surface_material_m0_contract.md](surface_material_m0_contract.md) for the frozen
legacy baseline and ownership decisions.

## Supported binding

A `plane_primitive` or `rect_prism_primitive` can opt in with this object extension:

```json
{
  "extensions": {
    "ray_tracing": {
      "surface_mapping": {
        "version": 1,
        "required_capability": "optic.planar_surface_v1",
        "method": "planar",
        "space": "object_rest",
        "source_domain": "brick_cells_v1",
        "scale_policy": "stretch_with_object",
        "origin_m": [-2, -1, 0],
        "axis_u": [1, 0, 0],
        "axis_v": [0, 1, 0],
        "tile_m": [0.5, 0.5],
        "offset_m": [0, 0],
        "pivot_m": [0, 0],
        "rotation_rad": 0,
        "seed": 1729
      }
    }
  }
}
```

Every shown mapping field is required. Supported spaces are `object_rest` and
`world`. Axes are finite orthonormal vectors; tile dimensions exceed `1e-9`
meters; seed is an unsigned 32-bit integer. The primitive frame must be right
handed and orthonormal, with positive transforms and dimensions. Authored and
scaled runtime dimensions must be at least 0.1 in their respective spaces,
matching the current primitive preview minimum. Mirrored/degenerate frames,
unsupported geometry, unknown methods/versions/capabilities and null bindings
are rejected by this reader's preflight and retained-document command.

The object must have exactly one matching `object_materials` authoring row with
an explicit `material_texture_stack` containing one to eight brick/solid layers.
Each layer needs a unique nonempty stable `id` shorter than 32 bytes. Nonempty
legacy face placements, material graphs (including the camelCase alias), and
image bindings cannot be combined with the new mapping in M1. Their existing
unmapped paths remain available. Invalid required semantics are not silently
converted to legacy mapping. Older readers may ignore this extension: producers
must select a consumer that supports `optic.planar_surface_v1`; the token alone
cannot make an old executable reject unknown fields.

## Coordinate and source meaning

The runtime bridge multiplies authored meter positions by `world_scale`.
Consequently the world adapter divides hit positions by that scale. This is the
verified bridge convention, and supersedes M0's tentative general-units adapter
assumption. Object-rest coordinates invert the prepared primitive translation,
rotation and positive per-axis scale into its authored meter frame. Tile size
therefore stretches with object scaling. World coordinates remain anchored in
authored world meters; object movement changes the sampled material.

For point `p`, project `p - origin_m` onto `axis_u` and `axis_v`, subtract the
meter pivot, rotate by `rotation_rad`, add the pivot and meter offset, then divide
componentwise by `tile_m`. Coordinates stay unwrapped. Each brick source tile is
one cell/row with alternating half-cell row staggering. Layer placement then
rotates about the cell origin, scales frequency, and adds offsets in cell units.
This is explicitly different source-domain meaning from legacy frequency and
placement. Grain still controls the existing source's grain response, without
also changing the mapping's cell size. Solid layers apply existing material
influences. Layer opacity, strength and material response retain stack semantics.

The deterministic seed combines the explicit mapping seed, authored layer seed
and stable layer ID using unsigned arithmetic. It excludes triangle IDs, face
IDs, object indices and stack positions. Existing kernel input uses the low
24 bits of that hash. Coordinates with magnitude at or above one million tiles
are invalid; the payload query reports failure. Optional tangent, footprint and
authored-UV validity flags are absent/false, not fabricated attributes.

## Runtime, viewport and persistence

`core_authored_texture` 0.3.0 owns the additive JSON-free mapping vocabulary and
validation. Its existing manifest APIs are unchanged. optiC owns JSON adapters,
prepared geometry conversion, projection, brick sampling and persistence.

`RuntimeSurfaceMappingCoordinates` consumes the actual hit position.
`RuntimeSurfaceMaterialSamplePrimitive` turns a primitive face location into a
geometric sample and calls the same payload resolver used by ray hits. It also
adapts legacy primitive faces, preserving their final renderer face ordering,
placement and authored texture behavior. Mesh viewport sampling remains on its
existing adapter. Graph execution and general mesh/UV mapping are outside M1.

Material viewport caches use the common primitive sampler. Document/mapping,
image-binding and face-placement revisions participate in invalidation; stack
and base material values also key cached data. Workspace selection does not
rebuild the material grid. The fixed cache holds 32 face grids of 128×128 samples
(about 16 MiB), with bilinear interpolation. Eviction can rebuild grids in larger
working sets. Studio lighting remains approximate, and transparency is sampled
but still displayed by the opaque studio shading path. Final shaded image
identity is not an M1 promise.

`SceneEditorDocumentSetSurfaceMappingForSceneIndex` is the retained editor command
for setting or clearing the extension, including validation/rollback and
undo/redo. Save/reopen preserves unknown producer fields. Duplicating a mapped
object clones its complete source-material row under the new stable object ID;
its seed remains unchanged. Existing graph/source documents are neither baked
nor rewritten into a new graph representation.

Committed transform edits rebuild prepared state. During a provisional transform
drag the viewport reuses the committed material grid on the displayed geometry;
world anchoring is guaranteed after commit, not during that preview. Interactive
world-space resampling is a remaining UI integration item. New mapping controls
and axial/UV/tangent/footprint work are deferred beyond M1.

## Verification

From the repository root, with native desktop access for the visual harness:

```sh
make -j4 BUILD_TOOLCHAIN=clang scene-editor-workspace-visual-test ray-tracing-render-headless
python3 tests/integration/test_surface_mapping_m1.py --output-root build/surface_material_m1/new-run
python3 tests/integration/test_material_viewport_parity.py --output-root build/surface_material_m1/legacy-run
python3 tests/integration/check_surface_material_m1_legacy.py --output-root build/surface_material_m1/legacy-run
make -C third_party/codework_shared/core/core_authored_texture test
```

Output roots must be new. The new-binding harness verifies plane coordinates,
actual ray-hit material channels, alternate triangle diagonals and exact planar
subdivision, translation/rotation/nonuniform scale, world coordinates, duplication
and index changes, cache reuse/invalidation, undo/redo, failed-command rollback,
save/reopen, provenance retention, six prism faces, negative reader cases and
headless renders at world scales 1 and 2.5. Curved-mesh equality is not claimed.

The legacy verifier checks eight final BMP hashes against M0, unchanged hit
coverage, eight unlit channels within `1e-6`, and matching face-override effects.
Its separate cache RGB mean-error bound is 0.05 on this finite fixture set; it is
not a worst-case error guarantee for arbitrary high-frequency materials. Reviewed
M1 results: exact pre-cache channels, all eight BMP hashes unchanged, maximum
cache RGB mean error 0.0431616934175. Native captures support visual inspection;
physical user acceptance remains separate.
