# M6 typed surface authoring

Implementation checklist (acceptance is recorded separately):

- Shared bounded, typed surface DAG: scalar/color/coordinates, 3D noise,
  independently sampled triplanar checker, arithmetic and color composition.
- Runtime preparation and common viewport/ray material evaluation, explicit
  supported-capability reporting and rejection of incompatible declarations.
- Retained node parameters and connections with revision checks, undo/redo,
  save/reopen, metadata preservation and inspector controls.
- Deterministic planar/box UV generation through the existing attributed OBJ
  compiler, with source hash and per-face chart provenance.
- Agent and Sculpts producers emit the same graph declaration; renderer preflight
  remains the execution authority.
- Shared unit/sanitizer checks, native/headless acceptance, negative capabilities,
  legacy regressions, source documentation and a reviewable optiC checkpoint.

Reuse decision: extend core_authored_texture for pure graph validation/evaluation;
reuse core_mesh_compile's attributed OBJ/tangent pipeline for generated UVs.
JSON, resource identity, editor commands, scene transforms and producer workflows
remain app/tool adapters. No new shared module, runtime or graph UI framework.

The new capability is additive and does not reinterpret legacy material_graph v1
or surface_sampling v1. M6 is a bounded procedural DAG, not a universal shader
language or automatic distortion-minimizing unwrap system.

## Capability and graph contract

`ray_tracing_render_headless --surface-capabilities` emits the executable's JSON
capability manifest. `--validate-surface-graph <graph.json>` validates a standalone
source with that same parser/compiler and exits 2 on failure. Scene preflight
additionally validates identity, geometry, transforms and source combinations.

Store `surface_graph` in the object's `extensions.ray_tracing.authoring.object_materials`
row (the extension is on the scene root). It requires `version: 1`,
`required_capability: "optic.surface_graph_v1"`, `color_space: "linear"`, `nodes`
and `outputs`. Stable node IDs are unique strings of 1–63 bytes. Up to 32 nodes
may appear in any storage order; preparation topologically orders them. All
nodes, including disconnected nodes, must validate and be acyclic. Additional
producer metadata is retained; `required_features` is rejected rather than ignored.

| Node kind | Inputs | Parameters | Output |
|---|---|---|---|
| `scalar` | none | `value` in [0,1] | scalar |
| `color` | none | three linear `value` components in [0,1] | color |
| `coordinate` | none | `space`: object_rest/world; `scale_m`: 1e-6–1e6; three `offset` components | coordinates |
| `noise3d` | coordinates | integer `seed`: 0–4294967295 | scalar |
| `triplanar_checker` | coordinates | `sharpness`: 1–16 | scalar |
| `multiply` | scalar, scalar | none | scalar |
| `mix` | color, color, scalar | none | color |

`inputs` is an ordered array of node IDs. `outputs.base_color` must reference a
color node; optional `outputs.roughness` references a scalar. Other output ports
are rejected. Omitted roughness retains the object's base response. Coordinates
are `position_m / scale_m + offset`; offsets are dimensionless and bounded to
±1e6. Host queries are finite and bounded to ±1e12. Rest coordinates use the
inverse instance/primitive frame; world coordinates remain fixed in the scene.
No node depends on triangle identity, node storage order or camera state.

Noise is deterministic smooth value noise with a 1,048,576-cell coordinate
period. Its contrast fades to the statistical mean over footprints of one cell
or more; this is a stability approximation, not exact volumetric integration.
Triplanar checker independently box-filters YZ, ZX and XY projections, then
blends their scalar results by normalized `abs(normal)^sharpness`. Its projected
axis footprint uses the sum of absolute pixel derivatives. This conservative
separable box is not an anisotropic/EWA filter. Unbounded secondary footprints
use each source's mean before composition, not an exact average of an arbitrary
composed graph. Roughness is a filtered scalar; image RMS/normal-variance policy
belongs to M5's distinct sampling capability.

Prepared programs and frames are immutable during shading. Pixel evaluation
performs no allocation, JSON parsing, file IO or graph compilation. The same
prepared evaluator supplies viewport and ray material values. Viewport lighting
and final lighting/tone mapping still differ.

## Supported combinations

| Source | Mapping/filtering | Support |
|---|---|---|
| M1–M4 stack, legacy layer graph, manifests and region bindings | Their existing declared mappings | Unchanged |
| M5 brick/solid stack and pinned image channels | One planar/axial/named-UV chart, mip filtering | Unchanged |
| M6 typed graph | Object-rest/world coordinates; noise or independent triplanar projections | Implemented |
| M6 graph mixed with M3 binding, legacy graph/stack, authored image source, M5 sampling or surface_mapping | Multiple competing source declarations | Rejected |
| M6 image nodes, normal/displacement outputs, arbitrary shader nodes, UV/UDIM nodes | No implemented execution contract | Rejected |

M6 supports imported mesh instances, planes and rectangular prisms. Primitive
frames must be orthonormal. Mesh pivot policy, when declared, must be
`authored_origin`. Negative/nonuniform mesh scales are supported. Scene preflight
rejects missing/duplicate object identities, unsupported outputs, bad references,
cycles, wrong port types, invalid parameters and required capabilities. Preparation
failure clears partial graph state and aborts the load. Unsupported combinations
never silently become a legacy material.

## Typed editing and agent production

The Material inspector retains source nodes and edits scalar/color values,
coordinate scale/offset/space, noise seed and projection sharpness. Select a node
with the Node control. Input controls cycle through compatible nodes; the full
compiler prevents cyclic connections. Commands check the document revision and
participate in undo/redo. Duplication, save and fresh-process reopen retain the
complete source and unknown producer metadata. Whole-graph replacement uses the
same validated document command; node addition/removal and output rewiring are
available through source/agent authoring. This is a compact typed inspector,
not a spatial node canvas.

```sh
python3 tools/surface_material_m6.py \
  --renderer build/toolchains/clang/arm64/tools/cli/ray_tracing_render_headless \
  --source noise3d --output /tmp/noise-graph.json
```

The tool also accepts `--graph`, `--node`, `--property`, `--value` for a typed
source edit, or `--scene` and `--object-id` to bind a graph. Existing-source edits
require `--expected-sha256`. Outputs must be new paths. The renderer validates
every emitted graph; scene binding still requires the normal scene preflight.

Sculpts (`line_drawing`) agent requests can declare `surface_graph` directly on a
plane, rect_prism or mesh_asset_instance request object, or under its ray_tracing
namespace. The producer preserves the complete declaration into authoring and
compiled runtime scenes and rejects unknown graph capability/encoding and a
simultaneous material prompt/stack. optiC preflight remains authoritative for
node execution and scene compatibility. The companion producer patch is in
[producer_patches/sculpts-surface-graph-m6.patch](producer_patches/sculpts-surface-graph-m6.patch).
The companion is a zero-context patch; use `git apply --unidiff-zero --check`
in the Sculpts repository to check an unapplied copy before adoption.
The actual producer was built and its deterministic output rendered by optiC;
this is source adoption, not a Sculpts desktop release.

## Bounded unwrap tool

`tools/surface_uv_unwrap.py` generates per-corner `planar_xy` or dominant-normal
`box` projections from a triangle OBJ. `--scale-m` is meters per UV unit. Box
projection chooses a signed axis chart per triangle. Vertices, triangle order,
winding and supplied normals survive; old UVs and OBJ material/group assignments
are replaced. The source SHA-256 is mandatory and output paths must be new.
The `.uv.json` sidecar records source/output hashes, UV-set ID and per-triangle
chart provenance. Feed its UV-set ID and generated OBJ to the existing
core_mesh_compile OBJ adapter to obtain runtime UVs/tangents and M4 storage/LOD.

```sh
python3 tools/surface_uv_unwrap.py --source mesh.obj \
  --expected-sha256 <source-digest> --method box --scale-m 0.2 \
  --uv-set-id generated_uv --output /tmp/mesh-uv.obj
```

Projection charts can overlap. This does not pack an atlas, optimize distortion,
cut global seams or create a second UV set. Degenerate geometry, non-triangular
faces, invalid indices and unsupported OBJ directives fail. Degenerate projected
UVs retain the M4 invalid-tangent policy. This is a usable bounded projection tool;
general unwrap/atlas tooling remains follow-up work.

## Verification and adoption

The canonical and vendored shared owner is `core_authored_texture 0.7.0` (additive
minor API); mesh compiler/asset/preview minimums remain M4's 0.8.0/0.7.0/0.6.0.
No new core or kit was added. Application and worker release versions are unchanged.

```sh
make -j4 all scene-editor-workspace-visual-test ray-tracing-render-headless
make -C third_party/codework_shared/core/core_authored_texture test
python3 tests/integration/test_surface_graph_m6.py --output-root build/m6-proof \
  --sculpts-tool <line_drawing-agent-scene-tool>
```

Acceptance checks 48 ray/preview samples per procedural/rest-world case, an
independent rest/world noise-coordinate oracle, primary versus unbounded source
response, real inspector parameter/connection events, stale/invalid command
rejection, undo/redo, duplication and fresh-process source round trips. Four
flattened/TLAS renders match byte-for-byte; maximum adapter color error is below
2.1e-14. Thirteen negative scene preflights and direct malformed graph checks
reject unsupported input. Shared unit and ASan/UBSan checks cover typed cycles,
reference/type failures, independent projection weights, seed changes and
footprint stability. UV output is compiled through the real attributed importer.
Sculpts deterministic producer preservation and rendering are separate gates.

The local report under `build/surface_material_m6/M6_REPORT.md` records the exact
acceptance/regression receipts. Main Edit integration, hands-on acceptance,
installed applications and releases are separate from this source checkpoint.
