# Surface material M3: retained documents and face regions

M3 completes the bounded document/region milestone in the isolated surface-material
source lane. It builds on [M1 planar mapping](surface_material_m1_contract.md) and
[M2 axial mapping](surface_material_m2_contract.md). Main Edit adoption, installed
applications and package publication are separate. Desktop/worker versions and
the shared `core_authored_texture` 0.4.0 module are unchanged.

## Authoring and persistence

For an explicitly mapped object, the Materials inspector edits the retained
source document. Enable retained editing on an existing M1/M2 stack, select
**Scope** and **Layer**, then edit opacity, roughness influence, layer U/V offsets,
strength or grain. Enter commits; Escape, changing selection or leaving the field
cancels the draft. The panel reports object default, inherited source or region
override. A face's first source edit copies the current object source into an
explicit override. **Reset region** removes that assignment and restores live
inheritance. It also removes that region's mapping override, if present.

Commands use the existing document revision, undo/redo and dirty-state machinery.
Invalid values, unresolved references, locked edits and stale revisions leave the
document unchanged. Normal workspace Save preserves the authored graph, stable
node/layer identities and unknown producer metadata; a compiled stack is not
written over a retained graph. External producer files remain immutable provenance
inputs. Subsequent UI changes belong to the retained scene source, not those files.

The older stack/graph mutation controls cannot silently edit a derived stack for
mapped objects. Object mapping controls remain in the Scene inspector. Region
source offsets are editable in Materials; full region projection definitions and
named mapping catalogs are authored through JSON/document commands.

## Binding and precedence

The object's existing `extensions.ray_tracing.surface_mapping` remains its default
coordinate definition. Its row in the scene's
`extensions.ray_tracing.authoring.object_materials` opts into M3 with:

```json
{
  "surface_material_binding": {
    "version": 1,
    "required_capability": "optic.surface_material_v3",
    "mappings": [],
    "regions": [],
    "source_documents": []
  }
}
```

A named mapping entry contains `id` and `definition` (a complete M1 or M2 mapping).
Optional `path` and `sha256` must occur together. The absolute local file must
match both the digest and embedded definition. IDs are unique and shorter than
64 bytes. There are at most 16 mappings and 16 source documents.

A region contains a stable `id`, a `face_role`, and optional `mapping`,
`mapping_ref` or `source`. A `source` is a whole retained `material_graph` or
`material_texture_stack`, with the same rules as the object source. The roles are
`front`, `back`, `left`, `right`, `top`, `bottom`; only `front` applies to a plane.
The runtime derives these roles from the primitive surface frame, position and
geometric normal. Triangle order and triangle indices do not define regions.

Precedence is explicit:

1. Start with the object's source and mapping.
2. A matching face region replaces either component only when it supplies it.
3. A layer's `mapping_ref` selects a named mapping over the effective object/region
   mapping. Its source placement then applies within those coordinates.
4. Evaluate all layers in their existing order and normalize the result once.

At most one assignment may target each face; duplicate faces or IDs are rejected.
A region cannot supply both an inline mapping and a mapping reference. An explicit
source replacement is a snapshot: later object-source edits affect inherited
faces, while the override stays independent until reset. This avoids implicit
merging or ambiguous region priority.

## Existing document adapters

| Producer | M3 mapping reference and behavior |
| --- | --- |
| Layer graph | `surface_mapping_ref` selects the object mapping when adopted by the adapter. Individual layer objects may carry `mapping_ref`. Graph/node IDs and producer fields remain retained. |
| Solid material composition graph | Optional `surface_mapping_ref` is serialized by the existing graph loader/saver. Geometry-field weights still come from its existing runtime compiler; supported brick/solid textures use the named common mapping. |
| Authored texture manifest | Optional `surface_mapping_ref` samples the existing image/overlay binding with repeat addressing in named coordinates. Without it, the existing primitive face-island coordinates apply. Existing material-intent response is reused. |
| Surface-authoring document V1 | Optional `surface_mapping: {id, digest_sha256, output_domains: 1}` participates in validation, canonical digest, transactions, save/reopen, compile-plan readback and the existing reference canvas. Documents without it retain their prior canonical representation. |

`tools/surface_material_binding.py` prepares a new local scene candidate from an
existing scene, mapping JSON and digest-pinned source document. It never overwrites
its input or an existing output. For example, from the repository root:

```sh
python3 tools/surface_material_binding.py \
  --scene /absolute/input-scene.json --object-id surface \
  --mapping /absolute/mapping.json --document /absolute/layer-graph.json \
  --kind layer_graph \
  --expected-document-sha256 <document-sha256> \
  --expected-mapping-sha256 <mapping-sha256> \
  --output /absolute/new-scene.json
```

The other kinds are `solid_graph`, `authored_manifest` and
`surface_authoring_document`. The source document must name the mapping. Image
manifests and surface documents must identify the target object. Solid graphs
require an existing mesh's procedural region and authored-material binding; the
adapter attaches the graph path. The scene must already have one retained source
row for the target object. A surface-authoring document attaches provenance and
mapping; its other graph references do not implicitly replace a scene source.

The result is `prepared_requires_renderer_preflight`. Use the existing
[headless request/preflight workflow](headless_agent_render_cli.md) against the
candidate before rendering. Preparation checks references and file identities;
renderer preflight is the supported-source gate. Files are read during preparation
and document reload, never during shading. Moving or modifying pinned resources
requires an explicit new candidate/reference update.

## Supported boundary

Layer graphs are schema 1 graphs of one to eight `layer` nodes compiling to a
brick/solid stack. Graph IDs/node IDs must be present and unique, and layer IDs
must be unique and shorter than 32 bytes. Numeric source values must satisfy the
existing bounded parameter domains. A graph and a stack cannot both define the
same source. Typed channel graphs, arbitrary new node kinds, graph rewiring and
additional source families remain M6 work.

Face-region overrides and per-layer mapping references currently apply to planes
and prisms. M2 axial mesh sources keep their object mapping; arbitrary mesh region
painting and per-layer mesh projection overrides are not implemented. Existing
solid-graph geometry-field selection remains in its own compiler.

Solid-graph mapping integration is renderer-only: the current simplified mesh
preview lacks its geometry-field attributes. Such a binding explicitly displays
**preview unavailable** and a placeholder. It cannot masquerade as a flat material
preview. Mapped solid textures support brick/solid sources with stable material
IDs shorter than 32 bytes and no microdetail normal perturbation; unsupported
mapped evaluation fails rather than falling back to triangle coordinates. The
runtime probe covers a compiled constant-weight graph across distinct triangles,
not all geometry-field graphs. Full preview attribute retention belongs to M4,
and basis-correct normal/bump response belongs to M5.

Authored image integration uses primitive face manifests; it does not add imported
mesh UVs, texture color-space declarations or footprint filtering. Surface-authoring
V2 typed graph expansion is unchanged. Capability restrictions are deliberate;
M3 does not imply universal material graph or mesh support.

## Verification

Build and run from the repository root on a native desktop session:

```sh
make -j4 BUILD_TOOLCHAIN=clang all scene-editor-workspace-visual-test ray-tracing-render-headless
python3 tests/integration/test_surface_mapping_m3.py --output-root build/surface_material_m3/acceptance
```

The M3 suite exercises agent-authored graph adoption, actual native text input,
object and face override edits, reset/inheritance, undo/redo, stale revision
rejection, normal workspace save, fresh-process reopen, retained producer JSON,
authored image sampling and compiled solid-graph mapping. It checks independent
material response on all six faces and front-only placement changes. Actual ray
hits are compared with the primitive material sampler at 65,536 points per face
across eight response channels. Missing references, stale document/mapping bytes,
duplicate nodes, unsupported nodes, graph/stack ambiguity and invalid values must
fail preflight. Exact receipts and captures live under the requested output root.

The M1 and M2 suites, legacy viewport/render parity and all eight frozen legacy
BMP hashes are also required regressions. Existing solid graph, surface-authoring
document, material-contract, runtime and mesh-preview tests cover adjacent APIs.
Visual captures show UI/readback behavior; they are separate from operator
acceptance on an installed build.

## Next milestones

M4 carries explicit per-corner UVs, UV-set identity and tangents through import,
compile, pack, preview and LOD, with seam/mirror/degenerate/round-trip tests.
M5 addresses filtering, color spaces, directional normal/bump response and measured
interactive cost. M6 broadens typed graphs, source families, tools and producer
adoption. See the [overall mapping plan](surface_material_mapping_plan.md).
