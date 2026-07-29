# Procedural Surface Geometry

This app-local module owns RayTracing's first procedural-surface proving
contract.

PSG-0 includes:

- a strict version-1 recipe with explicit object-unit feature sizes;
- canonical JSON and SHA-256 recipe identity;
- bounded quality budgets and transactional validation;
- deterministic plane and rectangular-prism topology expectations;
- no field evaluation, vertex generation, material evaluation, UI, or runtime
  renderer integration.

PSG-1 adds:

- a pure deterministic three-dimensional object-space value-noise evaluator;
- recipe-driven FBM, ridge/valley shaping, and domain-separated macro/micro
  streams;
- signed height/macro/micro outputs plus unit-range rock, roughness, and
  field-only snow precursors;
- a caller-owned point-evaluation budget with transactional failures;
- canonical ordered sample summaries and SHA-256 identity;
- no grid, displaced vertex, material binding, UI, or renderer integration.

PSG-2 adds:

- a bounded centered-plane grid generator with caller-owned final buffers;
- fixed-diagonal counter-clockwise triangles and recomputed smooth normals;
- normal displacement from the PSG-1 height output;
- an exact-zero cage boundary lock with deterministic interior fade;
- transactional generation through bounded temporary working storage;
- finite-vertex, amplitude, index, area, winding, normal, bounds, quality, and
  evaluation-budget validation;
- a canonical plane mesh digest over the recipe, vertices, field outputs, and
  triangle indices;
- no rectangular-prism generation, shared mesh-document adapter, renderer,
  material binding, UI, or package integration.

PSG-3 adds:

- one canonical integer boundary lattice for all six rectangular-prism faces;
- one PSG-1 field evaluation per unique boundary vertex;
- deterministic face-interior normal displacement with a smooth edge-lock
  band and exact-zero edge/corner displacement;
- six stable source-face groups and fixed outward triangle winding;
- independent displacement, group, edge-incidence, connectedness, Euler,
  signed-volume, bounds, and geometry-normal validation;
- a narrow adapter into the existing `core_mesh_asset` runtime-v1 document,
  including file save/load round-trip proof;
- no renderer attachment, material binding, scene cache/persistence, UI,
  photon work, or package integration.

PSG-3V adds the first diagnostic render consumer without changing runtime
scene authority:

- `procedural_surface_preview_asset_tool` exports a validated prism shell as a
  real `mesh_asset_runtime_v1` file plus a machine-readable geometry summary;
- `tools/procedural_surface_visual_proof.py` generates a zero-displacement
  control and displaced subject, then routes both through the existing
  headless mesh-asset renderer;
- the frozen proof contract renders a control/subject hero pair and views from
  `+X`, `-X`, `+Y`, `-Y`, `+Z`, and `-Z` under one fixed review-light rig;
- each cell must retain the frozen shell digest and triangle count, produce a
  nonblank image, hit the procedural object, and complete TLAS/BLAS traversal
  without errors or fallback;
- the output pack contains requests, render summaries, individual PNGs,
  control diff artifacts, a labeled contact sheet, an index, and a final
  proof summary;
- this is local diagnostic evidence only. It does not update a saved scene,
  `latest_good`, publication state, package state, or material behavior.

PSG-4 adds the first coupled material contract:

- one pure evaluator consumes each retained PSG-1 field output, the PSG-3
  displaced position, and the recomputed geometry normal without evaluating a
  second noise domain;
- stone color and roughness derive from macro, micro, rock, and roughness
  channels, while snow likelihood combines displaced elevation, world-up
  slope, and deterministic breakup;
- canonical material readback carries the exact recipe and shell digests;
- the runtime adapter maps the same sample into the existing
  `RuntimeMaterialSurfaceEval` and final `RuntimeMaterialPayload3D` authority,
  with focused preview/final parity proof;
- PSG-3V now exports retained per-vertex material artifacts and includes
  material-only, coupled-result, snow-mask, and roughness software-preview
  cells beside the native headless geometry views;
- these material cells remain useful diagnostic views beside the native
  renderer proof.

PSG-5 adds the first runtime-derived-asset lifecycle:

- `procedural_surface_derived_asset` persists recipe, semantic-cage, shell,
  material, quality, and collision-owner identity in one validated manifest;
- cache identity is canonical over recipe, cage, quality, shell, and material
  digests, and all failed loads leave the destination unchanged;
- runtime scenes retain a `procedural_surface_ref` while the authored
  rectangular prism remains the semantic cage and collision owner;
- manifest, recipe, mesh, and material dependencies are watched for changes,
  so the in-process scene cache cannot hide a changed procedural input;
- the loader carries per-vertex material samples into native triangles, ray
  hits interpolate them barycentrically, and the existing material payload
  and BSDF remain the final shading authority;
- static in-memory BLAS reuse, TLAS binding, save/reopen frame identity, and
  stale-recipe rejection are covered by focused and headless proof.

PSG-6 adds the first UI-free procedural graph/compiler contract:

- schema `ray_tracing.procedural_surface_graph` v1 defines stable typed nodes,
  sockets, links, evaluation budget, and explicit `field_ir`, `geometry`, and
  `material` output domains;
- the bounded v1 node set contains typed constants, `f64_add`, and one
  `recipe_output`; its twenty input sockets map exactly to the proven PSG
  recipe instead of introducing a second field evaluator;
- canonical JSON sorts nodes and links independently of file order, producing
  frozen graph digest
  `05ab6a7f6fcb5d2bbc039b6dbf280e8a26f5ec0813cebdad18d207bf9f317ae5`;
- validation rejects duplicate IDs, missing or multiply-bound sockets, type
  mismatches, cycles, disconnected hidden nodes, unsupported output domains,
  and over-budget evaluation;
- compilation is transactional: failures publish neither a partial recipe nor
  a partial compile plan;
- the golden graph compiles to recipe digest `563d8382...99525` and therefore
  reproduces the existing field, shell, material, and derived-cache identities
  exactly;
- `procedural_surface_graph_tool --graph <path>` emits canonical agent-facing
  compile-plan and compiled-recipe JSON without changing scene or package
  state.

PSG-7 adds the first actual composable spatial-field authoring contract:

- schema `ray_tracing.procedural_surface_field_graph` v1 evaluates object-space
  position, arithmetic, shaping, trigonometric, value-noise, FBM, ridged-FBM,
  and cellular nodes without a UI dependency;
- one evaluation publishes coupled height, macro, micro, cavity, mask, color,
  and roughness outputs, with memoized bounded execution and transactional
  failure;
- four fixtures define pitted concrete, wind-shaped sand, rocky terrain, and a
  central mountain whose color and roughness respond to the same height field;
- `procedural_surface_field_preset_asset_tool` compiles each graph into a
  validated final-quality watertight prism, runtime mesh, retained material,
  derived manifest, and machine-readable summary;
- tessellation density and displacement amplitude are independent. Winding
  proof uses each cage face's declared outward normal, while zero boundary
  edges, one component, Euler-2 topology, normals, and signed volume remain
  required;
- scalable canonical digest storage is derived from already-bounded validated
  counts, allowing detailed shells without weakening identity;
- the native preset proof renders hero and top views at `720 x 540`, asserts
  material binding and BVH health, and rejects visually similar presets.

The current persistent BLAS pack format does not contain procedural material
channels. Procedural assets therefore use the proven in-memory BLAS cache but
deliberately skip persistent BLAS pack reads/writes until that format can
preserve the complete triangle payload.

PSG-8 adds agent authoring, versioned surface bindings, and deterministic
application to arbitrary closed `core_mesh_asset` shells. PSG-8.5 freezes the
terrain-body specialization:

- a mountain or rocky-terrain graph is evaluated only on the
  `upward_facing` selection, rather than independently wrapping all six prism
  faces;
- planar XY projection gives the terrain one continuous object-unit domain;
- `world_up` is honored as the actual prism displacement direction, while the
  legacy prism API remains source-normal compatible;
- side skirts and the underside remain undisplaced closure geometry with
  binding fallback material;
- the focused terrain contract requires positive top relief, exactly zero
  side and bottom displacement, unchanged XY footprint, deterministic mesh
  identity, and the existing closed/component/Euler/winding/volume invariants;
- the A/B native proof retains the old six-face mountain only as a control and
  renders hero, top, side, and underside views of the corrected terrain body.

PSG-9 adds a separate UI-free solid-domain construction lane:

- schema `ray_tracing.procedural_solid_graph` v1 provides sphere, box,
  Z-cylinder, imported-source-mesh, object transform, twist, taper, round,
  union, intersection, difference, and smooth-union nodes;
- graphs retain `semantic_source_id` independently from the generated
  `core_mesh_asset` shell and explicitly select semantic-source or
  derived-shell collision authority;
- translation, XYZ rotation, and non-uniform scale operate in the solid
  domain, while twist/taper alter the continuous field before meshing;
- a bounded conforming marching-tetrahedra compiler records sampling bounds,
  cell size, a two-cell thin-feature floor, sample/vertex/triangle budgets,
  component policy, graph identity, and deterministic mesh identity;
- compilation rejects clipped domains, missing source meshes, exhausted
  evaluation or output budgets, open/nonmanifold results, disallowed component
  counts, degenerate triangles, and non-positive volume transactionally;
- closed connected shells are no longer incorrectly restricted to Euler `2`;
  the PSG-8 field/material applicator now accepts valid genus such as the
  Euler-`0` through-tunnel and preserves it after pitted-concrete evaluation;
- `procedural_solid_asset_tool` compiles agent-authored graphs and optional
  `--source ID=FILE` meshes into a runtime mesh plus a topology/resolution/
  authority receipt;
- the native proof renders object transform, twist/taper, boolean tunnel,
  smooth composition, and imported-mesh deformation from hero, Y-axis, and
  Z-axis views with runtime triangle parity and pairwise visual distinction.

The authored primitive or imported mesh remains semantic source authority.
PSG-9 emits a fresh replaceable runtime shell without silently reclassifying
or deleting that source identity.

PSG-10 adds the first solid-specific agent transaction and convergence layer:

- `procedural_solid_authoring` derives one UI-ready view model from the
  validated graph: stable node/operator/input/output readback plus typed,
  unit-bearing, range-bounded editable parameters;
- edits are optimistic transactions bound to the current canonical graph
  digest. Multi-edit batches validate after every step, save canonically and
  atomically, retain the exact prior graph as undo state, and reject stale
  writes before creating output;
- `procedural_solid_agent_tool` exposes `inspect`, `apply`, and `restore`
  without owning UI state or introducing a second graph evaluator;
- `procedural_solid_remesh` adaptively raises whole-domain resolution until
  component/Euler topology, volume, bounds, and requested two-cell feature
  scale converge, retaining the first passing closed-manifold result;
- every pass records resolution, geometry counts, topology parity, volume and
  bounds deltas, feature floor, convergence decision, and deterministic mesh
  digest;
- the native proof uses `18 -> 36 -> 72` cells for all five solid families.
  The imported-source case now uses a canonical 12-triangle cube shell rather
  than the prior mislabeled tetrahedron, then applies the same twist and
  transform graph;
- this is adaptive quality selection over the existing conforming extractor,
  not a claim of spatially local octree/transition-cell remeshing. Fine
  silhouette faceting and post-CSG cut/blend material regions remain visible
  follow-up boundaries.

PSG-11 adds the first spatially local solid-quality pass:

- a coarse signed-distance classification selects a bounded surface band and
  one deterministic closure ring, then expands only those blocks onto one
  shared fine lattice;
- inactive/active interfaces remain outside the zero-set band, so no
  surface-bearing transition template is required. The extracted shell must
  still prove zero boundary and nonmanifold edges; this is equivalent
  crack-free stitching, not an octree/Transvoxel claim;
- bounded zero-set projection reduces RMS surface residual and recomputes
  gradient normals while recording crease candidates, position delta, and
  topology preservation;
- contributor sampling assigns deterministic retained, cut, and blend region
  IDs and reorders triangles into contiguous `core_mesh_asset` surface groups;
- an optional deterministic source-mesh BVH accelerates distance and
  inside/outside queries while preserving exact fallback semantics;
- the five-family `24 -> 48` native proof evaluates only about 14.5% to 24.0%
  of fine cells, improves surface residual by about 71.7% to 82.1%, and
  retains closed-manifold topology in all 15 views.

The runtime mesh ABI still carries one normal per vertex. PSG-11 records
crease-aware provenance and feature candidates, but per-corner hard-edge
normal splitting and further serration reduction remain a later quality
boundary.

PSG-12 adds error-driven quality selection and feature-aware runtime normals:

- `procedural_solid_quality` samples signed-distance and face/field-gradient
  error, then selects a finer bounded local pass only when composite and
  signed-distance error improve;
- `procedural_solid_feature` and `procedural_solid_crease` use full
  transactional snapshots plus deterministic line search, so zero-set and
  QEF-like feature moves cannot publish invalid triangles or changed topology;
- `procedural_solid_shading` partitions incident faces into deterministic
  smoothing islands and emits split vertices with area-weighted normals,
  fitting hard-edge shading into the existing one-normal-per-vertex
  `core_mesh_asset` contract;
- the welded geometric shell is validated before shading splits. The final
  runtime document is structurally validated and receives refreshed identity,
  vertex count, and crease-aware normal provenance without reclassifying
  shading seams as geometric boundaries;
- `procedural_solid_asset_tool --quality-adaptive` exposes baseline/selected
  error, QEF, split-normal, region, topology, and source-acceleration receipts;
- focused, integration, hostile-budget, and five-family native proof cover
  deterministic output at `24 -> 48 -> 96`.

PSG-13 through PSG-15 add the authored-material lane without changing the
derived shell:

- PSG-13 binds retained, cut, and blend regions to materials through exact
  asset, mesh, region, and binding identities;
- PSG-14 loads deterministic authored optical/texture assets and propagates
  them through flattened and TLAS/BLAS runtime hits;
- PSG-15 composes a bounded material graph from geometry-derived inputs.

PSG-16A moves the first continuous geometry inputs to native-hit evaluation:

- each graph-backed asset owns one immutable runtime program containing the
  validated graph, authored materials, and per-corner geometry inputs;
- height, signed-up slope, and object-space position are barycentrically
  interpolated at the ray hit before material-graph evaluation;
- the same program pointer survives flattened triangles, cached BLAS
  triangles, and final TLAS hit remapping;
- raw height, signed-up-slope, and layer-weight views report exact internal
  shared-edge continuity; curvature, cavity, boundary distance, and region
  remain compatibility inputs derived per triangle;
- material evaluation changes neither source geometry nor the replaceable
  derived shell.

PSG-16B removes the remaining procedural-texture threshold switch:

- each enabled authored texture is retained in stable graph-layer order with
  its exact texture identity, evaluated graph weight, and graph-layer index;
- the runtime texture stack evaluates every retained layer at the same placed
  UV and scales effective layer opacity by the continuous graph weight;
- scalar surface properties and procedural textures therefore cross weight
  `0.5` without changing texture identity or geometry;
- a repeated weight produces byte-identical output, and zero texture layers
  preserve the scalar-only surface result.

The PSG-16B proof authors a fresh digest-bound object graph for each semantic
family before compiling it. Snow and dunes use separate top-bound terrain
fields; pores use a simple rounded box; sediment uses a cut tunnel; strata use
a twisted/tapered column. Full-size `800 x 600` eight-frame variant rows and
`1024 x 1024` raw mountain inputs replace the old low-resolution repetitive
matrix as the review authority. A dedicated five-panel mountain strip samples
texture weights `0.00`, `0.49`, `0.50`, `0.51`, and `1.00`; the accepted
neighbor steps are balanced and below one percent of the endpoint delta, and
the repeated `0.50` render has zero changed pixels.

PSG-16B does not add bump/normal output, dual contouring, or material-driven
displacement/remeshing. Those remain separate PSG-17 and PSG-18 boundaries.

PSG-17 and PSG-18 keep the material, shading, and geometry lanes distinct:

- PSG-17 adds deterministic procedural microdetail normal output at native
  hits. It perturbs the shading frame only; source vertices, triangle
  topology, silhouette, and acceleration identity stay unchanged;
- PSG-18 refines one selected source face, displaces the derived patch in
  object space, rebuilds its closure walls, and validates the result as a
  replaceable closed shell. This is true silhouette-changing geometry, not a
  material or bump claim.

PSG-19 adds a continuous authored-region carrier for imported source meshes:

- `procedural_imported_surface_region` compiles a bounded set of normalized
  object-space ellipsoid patches into one deterministic weight per immutable
  source vertex;
- the persisted carrier binds the region recipe digest, weight digest,
  canonical mesh digest, exact runtime-sidecar SHA-256, vertex count, and
  triangle count. Loading rejects stale or mismatched mesh identity;
- the material runtime copies those weights to their original triangle
  corners and barycentrically evaluates `authored_region` at native hits.
  Weighted scalar and procedural-texture layers therefore cross the coating
  boundary continuously;
- `surface_region_path` is optional inside
  `procedural_solid_material_ref`. Assets without it preserve the prior graph
  runtime exactly; assets with it require one consistent carrier across all
  instances;
- the PSG-19 proof generates a new purpose-built statue-fragment STL from a
  deterministic recipe on every run, imports it through the production
  `mesh_asset_runtime_v1` harness, and renders aged plaster revealing pitted
  concrete. Raw region and source-triangle views make the blend and immutable
  provenance reviewable.

PSG-19 is material-region authoring on the original imported topology. It does
not cut recesses, create coating sidewalls, displace the source mesh, or grow
secondary geometry. Those physical topology operations remain PSG-20 and later
boundaries.

PSG-20 adds the first topology-changing imported-surface inset compiler:

- `procedural_imported_surface_inset` consumes the exact PSG-19 carrier and
  rejects stale source asset, runtime-file, canonical-mesh, topology, or value
  identity before changing geometry;
- its public inset orchestration and shell-emission host delegates deterministic
  edge collection, transition refinement, connected-component selection, and
  disk-boundary classification to
  `procedural_imported_surface_inset_topology`, a private seam intended for
  later adaptive and multi-region work without widening the PSG-20 API;
- one bounded conforming refinement pass splits carrier-selected source
  triangles and their edge-sharing neighbors, then retains the largest
  connected disk-like candidate patch with a single degree-two boundary loop;
- the selected patch is duplicated and displaced inward along reconstructed
  smooth object-space normals. The original boundary ring is bridged to the
  inset floor with explicit transition-wall triangles, while all other source
  triangles remain the retained surface;
- the compiler emits a distinct replaceable derived runtime mesh, an exact
  per-derived-triangle provenance artifact, and material groups named
  `retained_surface`, `transition_wall`, and `inset_floor`;
- the derived shell must reanalyze as one watertight, manifold, positive-volume
  component with Euler characteristic two. The imported source mesh is never
  overwritten.

The focused PSG-20 fixture generates a fresh sculptural urn STL on every run,
imports it through the runtime-mesh harness, compiles its carrier and inset
twice, and proves exact repeat identity. The accepted result changes 5,440
source triangles into a 5,926-triangle shell with an 81-edge boundary ring,
162 transition-wall triangles, and 181 inset-floor triangles. The 1440 x 1080
proof includes source control, hero and grazing physical views, topology-role,
depth-delta, provenance, and exact-repeat views.

The cleanup parity fixture binds the accepted source, refined, selected,
retained, wall, floor, boundary-ring, and derived counts plus source-mesh,
derived-mesh, provenance, and configuration digests. Internal topology changes
must update that baseline deliberately; an extraction alone must preserve it.

PSG-20's original proof is deliberately bounded to one largest connected,
disk-like region and one refinement pass.

PSG-21 evolves that compiler behind the private topology seam:

- a bounded deterministic refinement controller repeatedly splits
  threshold-straddling triangles and their edge-sharing neighbors until the
  maximum selected-boundary edge reaches the typed target or the pass budget
  is exhausted;
- the default automatic target is `0.30` times the initial maximum boundary
  edge. The CLI also exposes `--target-boundary-edge-length`,
  `--adaptive-passes`, and `--minimum-component-triangles`;
- all selected connected components meeting the explicit minimum triangle
  count are retained. Independent degree-two boundary loops are counted and
  reported rather than silently collapsing the result to the largest island;
- the receipt exposes initial, target, and final boundary-edge scale,
  refinement pass count, selected component count, loop count, and explicit
  adaptive-active/converged state;
- the accepted single-region urn result converges in two passes from
  `0.3050905611` to `0.0766662452` object units against a
  `0.0915271683` target. It changes the `5,440`-triangle source into a
  `6,672`-triangle closed shell with `176` boundary-ring edges, `352` wall
  triangles, and `390` floor triangles;
- a second purpose-built carrier proves two retained selected components and
  two independent boundary loops in one deterministic closed manifold shell.

The PSG-21 proof remains bounded carrier-conforming subdivision, not a general
quality remesher. It does not yet prove a hole inside one selected component,
arbitrary Boolean clipping, fracture-quality edge optimization,
self-intersection repair for unbounded depths, or conformal moss/grime growth
shells.

Boundary decisions:

- `core_mesh_asset` remains the intended derived-shell output contract.
- Procedural recipe meaning and mesh compilation remain RayTracing-local until
  a complete producer/consumer proof exists.
- `base_feature_size_units` and `micro_feature_size_units` describe visible
  feature scale; `target_edge_length_units` describes sampling/tessellation
  resolution. Object dimensions determine subdivision counts through
  `ceil(extent / target_edge_length_units)`.
- Plane generation is proven in PSG-2; the first watertight prism shell and
  runtime-mesh handoff are proven in PSG-3.

Focused verification:

```sh
make test-procedural-surface-recipe-contract
make test-procedural-surface-field-contract
make test-procedural-surface-plane-contract
make test-procedural-surface-prism-contract
make test-procedural-surface-material-contract
make test-procedural-surface-derived-asset-contract
make test-procedural-surface-graph-contract
make test-procedural-surface-field-graph-contract
make test-procedural-surface-authoring-contract
make test-procedural-surface-binding-contract
make test-procedural-surface-terrain-contract
make test-procedural-surface-shell-contract
make test-procedural-solid-contract
make test-procedural-solid-authoring
make test-procedural-solid-psg11
make test-procedural-solid-psg12
make test-procedural-solid-agent-flow
make test-procedural-imported-surface-region-psg19
make test-procedural-imported-surface-region-psg19-visual-proof
make test-procedural-solid-psg11-flow
make test-procedural-solid-psg12-flow
make test-procedural-solid-psg12-visual-proof
make test-procedural-surface-visual-proof
make test-procedural-surface-field-preset-visual-proof
make test-procedural-surface-binding-visual-proof
make test-procedural-surface-terrain-visual-proof
make test-procedural-surface-shell-visual-proof
make test-procedural-solid-visual-proof
make test-procedural-surface-agent-iteration
```
