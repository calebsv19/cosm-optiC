# M4 explicit UV assets

M4 extends the isolated surface-material source checkpoint through imported
per-corner UVs, named UV-set identity and tangent frames. The runtime asset,
packed storage, memory cache, acceleration cache, ray hit and material viewport
preserve these attributes. M0–M3 mapping and legacy fixtures remain supported.
This is source completion; Main Edit adoption and installed/public packages are
separate integration lanes. See [the roadmap](surface_material_mapping_plan.md).

## Asset and import contract

`core_mesh_asset` 0.7.0 appends an optional triangle-major corner stream to
`mesh_asset_runtime_v1`. Corner `3 * triangle + k` belongs to vertex `a`, `b` or
`c` of that triangle. A position shared by two faces can have different UVs and
normals on each corner; UV seams do not require duplicating position indices.
One named UV set is supported per asset. An absent stream keeps legacy meaning.

Within the runtime document's `mesh` object:

```json
"surface_attributes": {
  "version": 1,
  "uv_set_id": "paint_uv",
  "corners": [
    {
      "uv": [0.0, 0.0],
      "normal": {"x": 0.0, "y": 0.0, "z": 1.0},
      "tangent": {"x": 1.0, "y": 0.0, "z": 0.0},
      "handedness": 1,
      "tangent_valid": true
    }
  ]
}
```

The example shows one corner; a complete stream requires exactly three corners
per triangle. IDs are nonempty and shorter than 64 bytes. Coordinates must be
finite; normals are unit length; valid tangents are unit length and orthogonal
to their corner normals. Handedness is +1 or -1, defining
`B = handedness * cross(N, T)`. An invalid tangent has zero tangent and handedness.
Unknown stream versions, incomplete streams and malformed frames are rejected.

`core_mesh_compile` 0.8.0 supports bounded **triangulated OBJ** input through the
existing imported-mesh authoring envelope. Set `authoring.imported_mesh.source_format`
to `obj`, `source_uri` to the OBJ file and `uv_set_id` to the desired identity
(default `uv0`). Existing unit/scale, normal policy and default surface-group
metadata still apply. Records supported are `v x y z`, `vt u v`, `vn x y z`, and
three-corner `f v/vt[/vn]` with positive or negative indices. Authored normals are
used when `preserve_source_normals` is true; otherwise the existing normal policy
provides the normals. Object/group/smoothing/material-library declarations are
metadata only. Material-library assignment, polygons, lines, points, missing UVs
and unsupported records are outside this adapter; triangulate before import.
STL still has no authored UVs and keeps its prior path.

Tangents use triangle UV derivatives, orthogonalized against each corner normal.
Mirrored UVs preserve negative handedness. Degenerate UV derivatives retain the
coordinates but explicitly invalidate the frame; no tangent is fabricated.
This basis is not a MikkTSpace implementation. Source triangles and corner order
are stable through compilation; topology-changing producers must explicitly
preserve/reconstruct attributes before claiming this contract.

## Mapping and retained editing

A mesh object's `extensions.ray_tracing.surface_mapping` can declare:

```json
{
  "version": 3,
  "required_capability": "optic.authored_uv_v1",
  "method": "authored_uv",
  "source_domain": "brick_cells_v1",
  "uv_set_id": "paint_uv",
  "uv_scale": [3, 3],
  "uv_offset": [0.1, 0.2],
  "rotation_rad": 0.17,
  "seed": 1729
}
```

`core_authored_texture` 0.5.0 owns pure UV-coordinate evaluation. Scale and offset
are dimensionless: scale authored UVs, rotate about UV origin, then add offset.
Scales must be finite and nonzero; negative UV scales are allowed. UV-set matching
is exact. Position-only mapping queries cannot evaluate version 3. Primary and
named chart references are checked against the loaded asset; missing, mismatched
or unavailable streams fail preflight. Version 3 is mesh-only. Existing named
mapping/document pinning from [M3](surface_material_m3_contract.md) remains valid.

The existing Surface mapping inspector shows the UV-set identity and edits U/V
scale, U/V offset, rotation and seed through retained document commands. Undo,
redo, normal save and fresh-process reopen preserve the declaration and source
provenance. Binding creation and changing UV-set identity use the retained scene
JSON/agent path; this slice adds no unwrap or UV painting UI.

## Runtime, storage and preview

- Runtime JSON writes and reads the full corner stream. The in-memory asset copy
  preserves it. RTMPK formats 5/6 carry the stream (plain/source-keyed); old
  unattributed formats 1–4 remain readable. Unattributed writers remain 3/4.
  The persistent mesh cache schema is 3.
- The acceleration pack format is 2; acceleration schema 2 and triangle layout 3
  invalidate older prepared caches. Rebuilt and cached BLAS triangles retain UVs,
  corner normals and tangent frames. The cache is derived data, not asset truth.
- World transforms use inverse-transpose normals and forward-transformed,
  re-orthogonalized tangents. Negative transform determinant flips handedness.
  Ray hits interpolate UVs and frames, account for face-forward normals and
  explicitly report tangent validity. Nonuniform and mirrored transforms pass an
  independent differential-frame oracle.
- `core_mesh_preview` 0.6.0 uses **exact source-triangle LOD** when UV attributes
  exist. It copies corners and identity and sets `attribute_protected`. Triangle
  budgets are advisory for these assets. Both interactive and settled previews
  preserve source detail; geometry-only assets retain existing clustering.
- Mapped solid-graph programs also protect source triangle identity, so their
  prepared geometry fields can be evaluated at the same triangle/barycentric
  location as the renderer. The M3 renderer-only limitation is removed for its
  supported named-mapping brick/solid graphs. Legacy unmapped graph programs and
  unsupported source/normal-response families do not gain blanket preview support.
  The shared renderer payload evaluates each visible preview sample directly.

This policy favors correctness over reduction. It adds per-corner storage and
larger runtime triangle records, and can increase memory and interaction cost on
large meshes. It is not a claim of seam-aware decimation or a large-asset
performance qualification. Attribute-aware simplification is a future optimization.

## Verification

From the repository root, with a fresh output directory:

```bash
make -j4 BUILD_TOOLCHAIN=clang all scene-editor-workspace-visual-test \
  ray-tracing-render-headless smooth-mesh-runtime-compile-tool
python3 tests/integration/test_surface_mapping_m4.py \
  --output-root build/surface_material_m4/acceptance
```

The native host needs macOS window-server access. The test creates its own OBJ,
compiled assets, configuration, scenes and outputs. It covers a seam at shared
positions, mirrored UVs, nonuniform/negative object scale, degenerate UVs,
JSON/packed round trips, truncated-pack rejection, exact LOD, editor edits,
undo/redo/save/reopen, and ray/viewport material agreement across eight channels.
A separate adapter fixture installs a real snow-accumulation geometry-field
program in the process-owned asset and proves varying graph output against ray
hits; it does not claim to test every external graph producer.

The source acceptance records 182 hit samples per case, with valid-frame counts
182/182/91 and 91 explicitly invalid degenerate frames in the third case. The
maximum eight-channel error is below `7e-16`. Flattened and TLAS/BLAS BMP hashes
match in all three cases, with persistent acceleration-cache hits observed.
Ten negative preflights reject invalid mapping/attribute declarations. Module
tests pass in canonical and vendored copies; M0, M1, M2 and M3 regressions pass.

## M5 entry boundary

M4 supplies the attribute foundation. M5 still owns footprint-aware filtering,
explicit texture color spaces, basis-correct normal/bump and directional response,
distant/grazing motion stability and measured orbit/render cost. Having a valid
stored tangent does not mean normal maps are already applied in that basis.
Multiple UV sets, UDIM/atlas resolution, general unwrap tools and broader typed
source producers remain future work. Arbitrary mesh region painting and release
publication are unchanged by this checkpoint.
