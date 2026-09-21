# Surface material M0: baseline, ownership and proposed contract

Date: 2026-09-20. Source baseline: `c8f64772d0953f82d56374ee399e47d18ea44d0e`.
Status: M0 complete for review; M1 is not implemented or authorized by this file.
The detailed internal framework plan is authoritative for M0–M6 scope. This
document records the inspected public source boundary and the proposed decisions.

## Baseline and evidence

The eight-case test in `tests/integration/test_material_viewport_parity.py` was
rebuilt with Clang and reproduced in fresh task-owned output under
`build/surface_material_m0/parity-native/`. All eight viewport runs and all eight
640×480 direct-light renders completed. The complete `diagnostic.json` matches
the prior eight-case diagnostic exactly, including mutation/fresh-reopen values.
This is diagnostic completion, **not a parity pass**.

- Plane: 65,536 hits per front/back probe, MAE 19.648814/255; 81.4758% exceed
  3/255 in at least one color channel. The side probes have zero hits and are
  not successful surface comparisons.
- Prism: each face has 65,536 hits. Overrides change 0/64320/64158/63704/63884/64200
  runtime samples and 0/0/0/0/0/0 viewport samples. Native viewport pixels do not
  change between the two prism cases.
- Scale, U/V offset and rotation affect both paths but do not agree. Editor
  scale=2 and offsets=.23/.19 survive a separate process reopen exactly.
- The probe directly calls the ideal pre-cache viewport evaluator and actual
  ray-hit payload resolver at matching surface locations. It does not measure
  cache interpolation, roughness/opacity parity, mouse ergonomics, arbitrary
  mesh charts, six independent BSDFs, or shaded-image equality.

`tests/fixtures/surface_material_m0/legacy_baseline.json` freezes the complete
diagnostic and SHA-256 of scenes, native/albedo captures and final BMPs. These are
same-host/reference-build bytes, not cross-platform image guarantees. Editor-save
scene bytes can contain output-root-dependent persistence metadata. Preserve
legacy material values and same-host final render bytes; viewport bytes are
expected to change when its legacy adapter is corrected.

The new create-only fixture generator emits alternate plane diagonals, an exact
8×4 planar subdivision, 8×4/32×16 spheres and 8/32-segment capped cylinders.
Five fixture tests pass. All seven generated legacy-brick scenes completed
160×120 headless renders under `build/surface_material_m0/geometry-valid/`.
The labeled grid is supplied as SVG and PPM coordinate-oracle assets. New planar
and axial expected samples are data only; no production adapter consumes them yet.
Different final plane BMP hashes are recorded without claiming a diagnosed
material failure: lit image equality is not the strict surface-value oracle.

The first native diagnostic attempt failed at SDL video initialization within
the sandbox. A native-display-authorized rerun succeeded. Both logs are retained.
The initial `geometry/` output predates a fixture parameter-key correction;
`geometry-valid/` is the validated output. Existing graph and surface-authoring
document contract targets also pass, including digest guards and round trips.

## Source-backed capability and owner matrix

I = implemented in the specified path; M = vocabulary/metadata only in that
path; U = unsupported; P = partial. These labels do not imply fresh exhaustive
tests beyond those listed above. Paths are relative to this repository.

| Capability / source authority | Runtime | Shared Material display | Agent, UI and save boundary |
| --- | --- | --- | --- |
| `include/render/runtime_ray_3d.h:27`, `src/render/runtime_ray_3d.c:406`: position, Ng/Ns, barycentrics, indices, optional object XYZ and region payload | I; optional XYZ interpolates corners | P; raster builds its own coordinates | Runtime indices are lookup handles, not persistent material identity; no general UV/frame/footprint query |
| `src/render/runtime_scene_3d_builder_mesh.c:151`: bounds-normalized mesh XYZ | I; payload projects XYZ using dominant **hit shading normal** at `runtime_material_payload_3d.c:260` | I but separately normalized and projected by **local triangle normal**, `scene_editor_mesh_preview_surface.c:293` | Mesh identity and transforms persist; common mapping definition U. Rotation/smooth-normal differences need their own test |
| `src/editor/scene_editor_material_face_metrics.c:233`, `scene_editor_material_face_placement.c` | I; primitive two-triangle face islands, physical dimensions/orientation, placement overrides | U for face placement in current grid | Face controls and overlay save/reload I; placement is not arbitrary per-face BSDF ownership |
| `src/render/materials/runtime_material_payload_3d.c:497` | I fallback uses baryV/W and triangle seed | Separate route | Preserve as explicit legacy fallback, not as a general mesh mapping rule |
| `include/render/runtime_material_texture_stack_3d.h`, `runtime_material_texture_stack_3d.c` | I up to 8 procedural layers, response composition and legacy placement | I object stack/base response through `material_preview_surface_eval.c`; differing inputs | I stack/placement controls and JSON overlay; common physical mapping U |
| `include/render/runtime_material_graph_3d.h`, `scene_editor_material_graph.c:25` | I 16-node layer/channel compiler to stack; channel refs do not become a general shader DAG | I compiled stack subset | I graph source retained and serialized; compiled channel-output refs are M for generic executable image-channel routing |
| `procedural_solid_material_graph.h`, `procedural_solid_material_runtime_program.c:205`, `procedural_solid_authored_material_runtime.c:50` | I 64-node geometry-field/mask graph, corner interpolation, weighted material/texture application at hit | U for this complete graph in object-grid preparation | Agent graph load/save/edit/readback I; generic UI graph editing U; source/digest must survive later adapters |
| `procedural_surface_authoring_document.h`, matching `.c` and tool | I validation/compile-plan envelope, not a renderer by itself | M read-only canvas, not material evaluation | I v1 source object/mesh digest, graph/selector/attachment references, typed domains, guarded replace/save; compile-plan existence is not proof every domain executes |
| `runtime_material_authored_texture_3d_manifest.c`, `runtime_material_payload_3d.c:381` | I plane/prism face base and overlay RGBA, material intent | U in current object-grid preparation | I manifest validation, binding/readback and persistence; shared v1/v2/v5, separate-face semantics |
| `runtime_material_authored_texture_3d.c:31`, `material_editor_texture_channel_readback.c:43` | M generic scalar/normal/bump file references; procedural scalar response is a separate implemented lane. Displacement name classifier exists but Supported rejects it | U generic channel-image execution; cache lacks opacity/normal fields | I channel vocabulary and readback; normal/bump expressly marked future; do not promote via spelling alone |
| Vendored `core_mesh_asset.h:95`, `core_mesh_preview.h:55` | I positions/normals/triangle surface groups; U general corner UV/tangent sets | I LOD vertices/indices; U corner UV, tangent and stable region stream | Agent/shared mesh JSON I; STL has no authored UV contract; M4 must include LOD attribute transport |
| `scene_editor_runtime_scene_persistence.c:358`, `runtime_scene_bridge_authoring.c:140`, retained `scene_editor_document.c` | I reload graph/stack/face/manifest bindings | Cache is derived | I retained scene transaction envelope; detailed material overlays remain a separate legacy mutation lane; universal history/unknown-field preservation inside typed graph serialization is not established |
| `scene_editor_viewport_material.c:8`, `scene_editor_mesh_preview_surface.c:60` | Not final transport | I 32 LRU slots ×128² float samples (~14 MiB), clamp-to-0..1 bilinear lookup, approximate studio light; no opacity storage | I shared display across workspaces; raster signature includes camera/revision, material cache does not include face/asset/image bindings |

The final payload resolves preset/region, then texture stack, water, procedural
surface, and solid authored material (`runtime_material_payload_3d.c:601`). The
new adapter must not reorder these legacy operations. The small graph and rich
solid graph are separate editable sources, not two names for one stack.

## Proposed common boundary

These names describe responsibilities, not frozen public structs. Prepared data
uses interned IDs/handles and immutable revisions; no JSON, allocation, graph
copy or neighbor traversal at a surface sample.

| Part | Required meaning | Optional slots / failure behavior |
| --- | --- | --- |
| Context | Object and asset stable IDs, asset/source digest, region ID; world and object/rest position in declared units; object↔world transform; Ng and Ns; source path kind and barycentric/corner handles when valid | Separate availability bits for UV set/corner UV, tangent/bitangent/handedness, footprint/derivatives, rest position. Never use zero to mean absent. Singular transforms or missing required coordinates are errors |
| Definition | ID, contract version, method (`legacy`, `planar`, then `axial_height`), space (`object_rest`, `world`), stored origin and orthonormal axes, dimensions, seam/pole/address rules | Stable frame must not silently refit to edited bounds. Explicit reframe updates revision. Unsupported required methods fail; reserved methods are not accepted as executable |
| Binding | Stable binding/object ID, definition reference/digest, explicit uint32 appearance seed, revision, optional region selector + source digest, scale policy, layer mapping refs | Object default → one selected region replacement → explicit layer mapping. Reject ambiguous overlaps in first version; later ordered composition must store priority |
| Result | Domain (`UV`, `XYZ`, future weighted projections), unwrapped coordinates, units, chart/region ID, validity and singularity flags | Future UV set, local basis and derivatives have independent validity. Weighted projections sample independently; do not average unrelated UV coordinates |
| Surface response | Adapt existing `RuntimeMaterialSurfaceEval` and payload; common unlit color, roughness, reflection, specular, diffuse, opacity/coverage/transmission meaning | Lighting stays consumer-specific. Do not infer normal maps, displacement or glass transport from channel metadata |

New object-space mapping attaches to the rest/source object by default; object
motion does not change appearance. World-space mapping deliberately moves across
an object when it moves. Seed is independent of triangle number, object array
position, camera, mapping revision and graph compilation order. Duplication copies
appearance seed; explicit variation changes it. New seed arithmetic is specified
with uint32 operations, avoiding legacy signed-overflow-dependent expressions.
Layer variation uses explicit layer seed or a versioned stable-ID hash. Never
use a transient runtime handle as a hash input.

### Units and transform order

Use shared `core_units` conversion once at the boundary: its contract is
`world = meters / world_scale`, so `meters = world * world_scale`. Asset positions
must be tagged as source meters versus runtime world values to avoid double
conversion. New tile widths/heights and offsets are meters; angles are radians.
Legacy `textureScale`, offsets and rotation keep their existing meaning exactly.

For planar mapping, convert the point to the chosen stored frame and physical
units, project onto frame U/V in meters, rotate about the stored chart pivot in
meters, add offset in meters, then divide componentwise by tile width/height.
In notation: `q_tiles = D^-1 * (Rθ * (chart_m - pivot_m) + pivot_m + offset_m)`.
Address only at the texture-source sampling stage. Negative/unbounded coordinates
are legal. Example: chart=(1,0), pivot=(0,0), θ=π/2, offset=(.25,0), tiles=(.5,.25)
produces (.5,4). A later per-layer transform composes after definition mapping;
persist its domain/units so rotation is not accidentally applied twice.

M1 supports `object_rest/stretch_with_object` and explicit world mapping. A
`maintain_world_tile_size` policy must either use a tested surface metric or be
rejected with a diagnostic; especially do not normalize a nonuniformly scaled
sphere and call it a physical circumference. Mirroring preserves signed frame
orientation and flips tangent handedness; singular scale is invalid.

M2 axial mapping evaluates atan2 from the **sample position**, never interpolated
wrapped corner angles. +Z is height; the seam direction is stored (+X fixture),
positive U winds toward +Y. U is angle times declared reference circumference,
then divided by horizontal tile width; V is physical height from stored origin
divided by course height. An explicit integer repeat option reports effective
tile width `2πR/N`. Caps use separate planar regions. Sphere poles report
singularity with deterministic U=0, finite values and disclosed pinching. This
does not promise distortion-free artwork or geodesic masonry.

### Texture-source correction to the proposed plan

An existing brick sample is **not one physical brick per input UV unit**:
`runtime_material_texture_stack_3d.c:301` derives U cell frequency from grain
(3–8) and V frequency as 0.48×U. Its placement helper wraps to 0..1 before
sampling. Merely adding planar/axial coordinates would therefore leave physical
tile controls misleading and can expose wrap discontinuities.

M1 must specify a versioned source-domain adapter: legacy keeps its old frequency
and wrap; the new brick source consumes declared brick-cell units (or an explicitly
documented conversion to the existing kernel) without applying placement twice.
M2 seam closure must include periodic cell identity and grain/noise, not only an
integer circumference. Staggered rows need a consistent horizontal cell count;
if V repeats, its repeat period must contain an even number of rows. Use both a
labeled grid and brick so color/noise cannot hide coordinate defects.

Image color decoding/filtering is a source/sampler concern. New color samples
declare encoding and convert to linear; scalar masks/roughness/normal data do
not receive color gamma. Legacy behavior is frozen separately. Coverage and
transmission are distinct scalars, not synonyms for alpha tint. Normal/bump basis
execution and displacement through geometry compilation remain later gates.

### Persistence, compatibility and cache

Absence of new mapping data means explicit `legacy_v0` in compiled/readback state,
not a silent migration. Existing saved scene schema v1, graph v1, mesh runtime v1,
surface-authoring document v1, and authored manifests v1/v2/v5 remain readable.
New mapping data requires its own versioned required-capability marker. Current
older readers may ignore unknown extensions: this must be reported as an older
consumer incompatibility, not claimed fail-closed behavior. M1 needs a version/
capability check in its new reader and an explicit legacy export policy.

Keep editable graph IDs/nodes, layer IDs, source mesh digests and image dependencies.
UI must modify an exposed source parameter or an explicit override; never replace
the rich graph with its compiled stack. Known typed-graph reserialization needs
explicit unknown-field tests; retained scene-envelope preservation alone does not
prove preservation inside a typed graph. M1 adds the minimal object binding save/
load path needed to test compatibility. M3 expands region/producer/source editing.

Prepare cache keys from geometry/source digest, stable binding/region/UV set,
mapping/frame/revision, transform/scale policy, graph and material revisions,
seed and image dependency digests. Geometry edits invalidate geometry fields;
world-space mapping invalidates on motion. Object-space motion only rerasterizes
unless footprint/scale policy requires new samples. Camera changes update raster
lighting/footprint, not the pattern seed. Face/layer edits, undo/redo and dependency
reload must expose the correct material on the first frame after commit.

Keep bounded per-binding caches or cheap direct evaluation. A single clamped
object 128² grid cannot represent unbounded physical coordinates, distinct faces,
3D fields or arbitrary islands. M1 should instrument cache builds/memory rather
than silently expanding a universal atlas. M5 filtering cannot be used to excuse
severe M2 shimmer; pull forward the minimum sampling required by M2 evidence.

## Shared ownership and adoption recommendation

Canonical definitions live in the ecosystem `shared/` repository; optiC consumes
`third_party/codework_shared`. Current source/header and VERSION readback agrees
for the following modules. This is stronger than stale prose in the adoption
matrix, but is not a claim every file of both trees is identical.

| Candidate | Canonical / vendored version | Decision and precise boundary |
| --- | --- | --- |
| core_authored_texture | 0.2.0 / 0.2.0 | reuse-extend: proposed mapping definition/binding/result vocabulary and JSON-free validation beside manifest semantics; no BSDF or image I/O |
| core_space | 1.1.0 / 1.1.0 | reuse-adopt frame vocabulary; reuse-extend pure double-precision frame projection/coordinate transform helpers if needed; no camera or host cache policy |
| core_units, core_object, core_scene | 0.2.0, 0.1.1, 1.2.0 / matching | reuse-adopt units, identity and scene envelope; only add shared scene references if the chosen schema actually needs them |
| core_mesh_asset, core_mesh_compile, core_mesh_preview | 0.6.0, 0.7.1, 0.5.0 / matching | reuse-adopt now; reuse-extend in M4 for corner attributes, compiler and LOD provenance; no fabricated UVs now |
| core_math | 1.0.1 / 1.0.1 | reuse-deferred for this double-precision query: public vector helpers are float; core_space already owns double frame types. Do not create duplicate generic math |
| core_pack | 1.1.1 / 1.1.1 | reuse-deferred until optional attribute transport is needed; no wire change in M0–M2 |
| core_data/core_io, core_trace and execution/time/job modules | existing host infrastructure | No new mapping ownership; reuse host I/O/diagnostics as needed, no scheduler/storage project |
| core_theme/core_font and kit_ui | existing host adoption | reuse-adopt presentation only; no material meaning in a kit |

No new generic library is justified. The proposed additive shared mapping API
would ordinarily require core_authored_texture MINOR 0.3.0 and, if extended,
core_space MINOR 1.2.0, each with tests/README and bounded vendored adoption.
Do not bump a module merely because it was inspected. Required persisted semantics
need a schema/capability decision independently of library SemVer. If ABI or old
behavior breaks, reclassify rather than calling it additive. Desktop/worker
versions remain untouched. Shared owner coordination and clean/dirty readback
are prerequisites to the separate canonical edit; M0 changes no shared files.
Adoption updates require the shared compatibility matrix, connection-gaps and
current-state docs. Their current stale candidate descriptions are noted, not
broadly reconciled in this lane.

## Phase ordering and review gates

1. **M1: contract + planar.** Approve shared owner/API and schema first. Add bounded
   common query, explicit legacy adapters, source-domain/seed handling, planar
   object/world mapping, minimal serialization and viewport consumption. Plane
   and primitive prism face layout are in scope; arbitrary UVs, UI shell and the
   separate Preview window are not. Preserve current final legacy render bytes
   on this host and graph provenance. Test all unlit channels at identical points
   (absolute 1e-6); coordinate invariance 1e-9; reorder triangles/objects; transformed
   planes; world_scale; face placement and cache invalidation. Compare cache images
   separately and freeze measured tolerances before claiming M1 acceptance.
2. **M2: first useful checkpoint.** Add axial height brick, explicit seam/reference
   radius and finite poles, one existing-inspector binding edit through the same
   validated path as agents. Plane plus low/high sphere/cylinder captures must
   show aligned courses, stable orbit, supported transforms, copied seed and
   fresh-process save/reopen. Same-surface subdivision remains strict; different
   curved meshes use common analytic samples plus declared geometric bounds.
3. **M3:** broaden layer/region/source-document bindings and reversible edits;
   six independent face materials; stale selector/mesh and overlapping assignment
   rejection; source graph/provenance round trip through UI and agent edits.
4. **M4:** explicit authored per-corner UVs/UV sets, tangent handedness and asset/
   compiler/pack/preview/LOD adoption; seam splits, mirrored UVs and degenerates.
   Does not block generated mapping on current STL geometry.
5. **M5:** footprint filtering, normal/bump basis and response accuracy; separate
   transport tests for mirror, roughness, directional metal and glass; measured
   motion/aliasing and cache/orbit cost. Minimum filtering may move into M2.
6. **M6:** typed graph UI and broader producers, XYZ/triplanar and optional unwrap;
   capability readback must reject unsupported required behavior.

M1 review decisions still needed: confirm the proposed shared extension owner and
authority; confirm object-rest/stretch default with unsupported metric policies
rejected; agree versioned brick-cell source semantics and old-consumer policy.
These are concrete continuation decisions, not permission inferred by this M0
document. M0 ends with the parent review report.
