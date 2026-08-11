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

The surface-authoring foundation adds `ray_tracing.surface_authoring_document`
v1 as a digest-bound composition container. It references independent
material, surface-field, face/region-selector, and attached-asset documents,
validates their output domains and source mesh identity, canonicalizes the
references, provides deterministic SHA-256 identity, and emits a transactional
compile/readback plan. It intentionally
does not evaluate a second graph, deform geometry through a material mask, or
generate attachments; those remain owned by their existing compilers.

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
  component and preserve the source Euler characteristic. Genus-zero fixtures
  therefore report Euler `2`, while imported sculptures with handles or
  tunnels retain their source genus. The imported source mesh is never
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

Dense-source budget note:

- the default PSG-20/21 envelope is `2,000,000` vertices and `8,000,000`
  triangles, exposed as named configuration constants;
- the larger triangle cap is required because each localized refinement pass
  reserves a conservative four-way temporary triangle capacity. Venus's
  `916,966` source triangles therefore require `3,667,864` first-pass slots
  before the selected patch is even evaluated;
- the envelope is still finite and receipt-bound. This change removes the
  dense-source false negative; it does not authorize arbitrary Boolean
  clipping, unbounded refinement, or unsafe allocation growth.

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

PSG-22 adds a separate additive imported-surface growth compiler:

- `procedural_imported_surface_growth` consumes the exact immutable source mesh
  plus a digest-bound PSG-19 carrier and selects source-triangle anchors above
  a typed threshold;
- deterministic radius/height variation and conservative object-space
  clearance choose a bounded set of non-overlapping growth elements;
- each element is emitted as its own closed, positive-volume asymmetric
  ellipsoid. Its exposed cap rises along the carrier triangle normal while its
  attachment base penetrates the source surface by a declared depth;
- the output is a distinct replaceable `core_mesh_asset` runtime document with
  `exposed_growth` and `attachment_base` surface groups plus per-triangle
  source-triangle, element-index, and role provenance;
- acceptance requires exact source/file/carrier/config identity, byte-exact
  repeat artifacts, immutable source readback, one closed manifold component
  per element, positive aggregate volume, zero inter-element overlap pairs,
  and zero self-intersection pairs.

The focused PSG-22 fixture generates a new 4,320-triangle garden-finial STL on
every run and compiles three separated attached components into a
672-triangle derived growth asset. Its 1440 x 1080 proof retains a real beauty
visibility threshold and separates source, hero, grazing silhouette,
attachment-base, clearance, source-triangle provenance, and exact-repeat
views.

PSG-22 proves low-profile mound/coating components attached to locally smooth
carrier triangles. It does not yet conform each mound footprint to a
multi-triangle curved patch, boolean-union growth into the source shell,
generate strand/fiber moss, simulate volumetric soil, or repair arbitrary
growth collisions.

PSG-23A adds a typed rooted strand/fiber authoring foundation:

- `procedural_imported_surface_strands` consumes the exact PSG-19 carrier and
  selects deterministic, clearance-bounded source-triangle roots;
- every strand retains a stable index, source triangle, barycentric root,
  surface normal/tangent frame, ordered control points, and tapered per-point
  radii in a digest-bound semantic strand asset;
- root control points penetrate the carrier by a declared positive distance,
  while deterministic length variation, bend, and curl shape the exposed
  control-point chain;
- continuous segment-to-segment collision checks reject inter-strand overlap
  and non-adjacent self-intersection between control points;
- a bounded triangle-tube proof backend emits one closed positive-volume
  component per strand with `root_cap`, `strand_shaft`, and `tip_cap` surface
  groups plus per-triangle source/strand/segment/role provenance;
- the imported semantic source stays byte-identical and the typed strand asset
  plus proof mesh remain separately replaceable.

The focused fixture generates a new 3,456-triangle smooth scalp-and-neck STL
on every run. It authors 24 strands with 216 control points and compiles 3,456
closed proof triangles. The 1440 x 1080 matrix separates source, hero, grazing,
crown distribution, root/shaft/tip roles, strand IDs, source-triangle roots,
and exact repeat.

PSG-23A does not add a native renderer curve primitive, curve BVH/intersection
lane, motion/deformation, clump-child generation, dynamics, grooming UI, or a
Disney/Chiang hair BSDF. The triangle tubes are a bounded visibility and
attachment proof backend, not a scalable production-hair representation.

PSG-23B consumes the durable PSG-23A asset through a native curve lane:

- `ProceduralImportedSurfaceStrands_BuildCurveAsset` converts every adjacent
  control-point pair into a finite positive tapered curve segment while
  retaining strand/segment identity, endpoint tangents, and root/tip caps;
- `RuntimeCurveAsset3D_BuildBLAS` builds a deterministic object-space BVH from
  radius-expanded primitive bounds;
- analytic side and endpoint-cap intersection returns closest-hit identity,
  segment-local `curveU`, interpolated positive radius, and a finite unit
  `curveTangent` through the shared `HitInfo3D` payload;
- curve trace statistics use atomic counters so the readback remains safe
  under the renderer's multithreaded trace shape;
- focused flat-versus-BLAS rays and an independent 64-sided triangle-tube BVH
  provide intersection and tessellation parity rather than self-comparison.

The strict parity set covers 145 side/cap rays with zero hit-state mismatches
and a maximum interior hit-depth delta of 0.0004675102 units. The 1528 x 836
dense diagnostic covers 79,200 rays, reports zero hit-state mismatches, and
retains the expected maximum 0.005716506-unit grazing depth difference between
an analytic circular silhouette and the faceted tube oracle.

PSG-23C makes the native PSG-23B curve asset scene-addressable at runtime:

- `RuntimeScene3D_AddCurveInstance` deep-copies an immutable curve asset plus
  stable asset/object/scene-object identity and a finite position, Euler
  rotation, and positive uniform scale;
- curve instance bounds enter the same scene TLAS as triangle mesh instances,
  and mixed closest-hit traversal preserves deterministic triangle/curve and
  curve-instance tie rules;
- curve hits retain scene instance identity, primitive/strand/segment identity,
  tangent, local parameter, and scaled radius through flattened/TLAS parity;
- `RuntimeSceneCurve3D_ResolveMaterial` delegates curve hits to the existing
  scene-object material payload resolver instead of introducing a curve-only
  material system;
- scene copy/free owns curve data deeply, and geometry signatures include the
  curve primitives and instance transforms.

The focused fixture proves curve-only, triangle-only, and mixed scenes,
translated/rotated/scaled curve instances, scene deep-copy independence,
TLAS diagnostics/statistics, flattened-versus-TLAS parity, and real glossy
material dispatch. PSG-23C still does not serialize curve assets through scene
authoring, support non-uniform curve scaling, add motion blur/dynamics/grooming,
or implement a Disney/Chiang hair BSDF.

PSG-23D adds digest-bound `curve_asset_runtime_v1` sidecars and
`curve_asset_instance` ingestion. Ordered finite control points and positive
radii are loaded into the PSG-23B native curve lane, curve-only scenes select
the native renderer, and deterministic authoring exposes density, spacing,
length, direction, bend, curl, control-point count, and taper. Its
900x700-per-cell variation proof distinguishes six independently generated
curve assets without claiming carrier-aware hair placement.

PSG-23E adds carrier-aware guide and clump authoring:

- `procedural_carrier_curve_groom_authoring.py` binds one authoring document to
  the exact source mesh file, PSG-19 carrier file, carrier value digest, and
  source topology;
- deterministic carrier-weighted farthest-point sampling chooses triangle
  roots and a smaller spatially distributed guide set;
- every child strand retains source-triangle, barycentric root, root normal,
  carrier weight, guide index, and embedded root provenance;
- nearest-guide clump assignment combines length variation, part axis,
  object-space comb, lift, bend/curl, clump strength, tip spread, and taper
  into the existing serialized native-curve asset contract;
- compile fails closed for source or carrier drift, malformed topology,
  impossible guide/strand counts, or stale digest-guarded edits.

The focused contract regenerates a fresh scalp carrier and proves exact repeat,
64 roots, eight covering guides, 576 ordered controls, decreasing positive
radii, exact embedded roots, strong-versus-loose clump convergence, and actual
mixed scalp/curve render hits. The 2716x1456 proof uses a single freshly
generated scalp to isolate six groom states at 900x700 each: loose natural,
soft clumps, strong locks, center part, side sweep, and curled clumps. A
separate guide-assignment view colors roots by guide.

PSG-23E is guide-level deterministic geometry, not production hair density or
shading. It does not add interpolated render children, density/LOD controls,
curve-BLAS performance work, opacity/transmission, a Disney/Chiang hair BSDF,
dynamics, or motion blur. Dense overlapping clumps are intentionally retained
as a later performance/scaling boundary.

PSG-23F adds deterministic guide-to-render-child compilation and a bounded
dense-curve acceleration profile:

- `procedural_curve_render_children_authoring.py` binds its authoring document
  to the exact PSG-23E guide asset and exact source mesh;
- each guide becomes a selectable preview, interactive, or final set of thin
  render children while the thick guide geometry is excluded from the output;
- child roots remain inside the parent's source triangle, retain source
  triangle/barycentric/root-normal/carrier/guide provenance, and preserve the
  guide's embedded-root relationship to the carrier;
- stable render-child IDs are allocated against the final LOD, so preview and
  interactive assets are exact deterministic subsets of final rather than
  separately jittered representations;
- bounded root spread, length/shape variation, radius scales, seed, and
  monotonic `4 / 16 / 48` children-per-guide defaults remain editable through
  init/inspect/digest-guarded edit/compile operations;
- the curve BLAS now uses deterministic heap sorting instead of quadratic
  insertion sorting and a fixed bounded traversal stack instead of allocating
  per ray.

The focused contract proves exact regeneration, immutable guide/source inputs,
128/320/640-child selectable LOD fixtures, retained provenance, materially
thinner taper, fail-closed stale edits and source drift, and a real 640-child
mixed scalp render. The dense acceleration proof builds 2,048 eight-control
strands (14,336 primitives) into 8,191 nodes at depth 13, traces 4,096 rays
with 22,640 primitive tests, reaches maximum stack depth 5, and reports no
overflow. The high-resolution visual proof holds one fresh scalp and one
104-guide groom constant while comparing 104 thick guides against 416,
1,664, and 4,992 thin render fibers at 900x700 per cell.

PSG-23F proves geometric density, deterministic LOD identity, and bounded
native curve acceleration. Its fibers still use the ordinary surface material
response: no longitudinal/azimuthal hair scattering, multiple scattering,
opacity/transmission model, dynamics, collision simulation, or motion blur is
claimed.

PSG-23G adds an explicit curve-only Disney-v2 single-fiber optics payload:

- `hair_optics_enabled` must be true on the curve object's ordinary
  `object_materials` row; tangent-bearing curve identity is the second
  dispatch key, so triangles and ordinary curves retain their prior response;
- absorption RGB, longitudinal and azimuthal roughness, IOR, and cuticle tilt
  are normalized into a bounded `RuntimeHairOptics3D` payload;
- the isolated evaluator composes deterministic R, TT, TRT, and aggregate
  higher-order internal-reflection lobes using longitudinal distributions,
  trimmed-logistic azimuthal distributions, Fresnel, and absorption;
- tangent-bearing curves now participate in direct-light receiver evaluation,
  and native render stats expose `hair_scattering_pixels` for proof;
- the corrected 2x2 900x700 proof holds one fresh 4,992-fiber groom fixed
  across a surface-BSDF control and brunette, copper, and blond optical
  variants. Every cell retains 115,236 curve-hit pixels, enabled cells record
  115,357 evaluated hair-light samples, maximum radiance remains at or below
  1.793, and clipped-channel plus near-white coverage remain zero;
- curve scene-instance world bounds now participate in conservative tile
  occupancy. Unprojectable bounds fail open, while the normal tiled brunette
  proof skips 257 empty tiles and still matches a full serial reference
  exactly with zero changed pixels and zero maximum channel delta;
- the proof emits tiled/serial/difference/clipped-channel debug views. The
  earlier clipped and saturated PSG-23G matrix is superseded and is not valid
  acceptance evidence.

This is a bounded Chiang-inspired direct-light single-fiber foundation, not
the full production path-traced model. It does not add inter-fiber multiple
scattering, stochastic hair-lobe sampling, opacity/transmission transport,
dynamics, collision simulation, or motion blur.

PSG-24A adds a reusable `surface_feature_field_v1` morphology lane for deterministic
spot scatter. Its mesh-aware compiler derives signed concavity from shared
triangle edges, propagates a bounded surface-distance macro envelope, supports
count- and density-driven populations, and retains triangle/barycentric root,
smooth normal, orthonormal tangent frame, calibrated shape parameters,
population, stable feature ID, and signed object-unit height/depth for every
accepted feature. Zero-amplitude populations remain material-only. Signed spot
values can instead feed `procedural_surface_feature_relief_shell`, which uses
the PSG-18 selected-face refinement to emit one closed shell with shallow
inward and outward relief while holding cage edges and unselected faces fixed.
Deep cuts route to PSG-24C; genuinely separate attached pieces may use PSG-24D. Runtime sampling
uses a conservative 32x32 footprint index and rejects candidates below the
declared normal-compatibility cosine before they contribute coverage, interior,
rim, signed height/depth, ID, or direction. The field changes no mesh bytes,
silhouette, acceleration structure, or hit topology. PSG-17 normal, PSG-20/21
inset, and PSG-22 attached-geometry consumption remain separate.

PSG-24B adds a dedicated `surface_feature_curve_field_v1` lane instead of
growing the material-graph implementation. The compiler traces deterministic
surface-adjacent polylines, including bounded branches, and serializes stable
curve/segment/parent IDs with triangle and barycentric endpoint provenance,
normal-compatible tangent frames, tapered object-unit width/depth, edge
softness, and rim width. Native capsule sampling exposes coverage, floor,
edge, signed depth/slope, and tangent direction through the existing feature
graph channels and PSG-17-style shading-normal response. Its 32x32 candidate
index is bounded at 32 segments per cell; incompatible nearby folds are
rejected before contribution. This remains material/normal microdetail until a
physical curve-relief adapter is added; deep cuts remain PSG-24C work.

PSG-24C adds a dedicated deep field-to-inset adapter around the existing PSG-21
compiler. It requires the exact `surface_feature_field_v1`, source mesh,
source-bound carrier metadata, and an explicit stable feature-ID list whose
members all carry negative authored depth. Before
topology changes, the adapter extracts the carrier-supported triangle
neighborhood plus a closure/stitch ring and records its reduction from the
whole source. It emits a separate closed positive-volume shell with retained,
transition-wall, and inset-floor groups plus per-derived-triangle source,
feature-ID, and role provenance. Unselected source vertices remain fixed, and
the deterministic bundle exposes source, field, selection, shell, role, and
receipt entrypoints. This is physical inset topology; it does not start
PSG-24D attached deposits or modify the source shell.

PSG-24D adds an optional positive-height field-to-growth adapter around the
existing PSG-22 geometry lane. It compiles each explicitly selected spot into
its own replaceable closed mesh asset at the retained source triangle and
barycentric root, preserving the feature tangent-frame aspect and rotation.
Aggregate provenance retains feature, population, source triangle,
barycentric attachment, PSG-22 element, role, and material semantic. The
composite gate checks one positive-volume component per asset, positive
attachment, conservative cross-asset clearance, zero forbidden overlap and
self-intersection pairs, immutable source/field identity, exact repeat, and
field-to-element material agreement. Zero/negative selections fail closed and
the adapter keeps each mound equator at or below its root plane to avoid a
floating perimeter. The source and any PSG-24C inset remain
separate; no Boolean union or conformal multi-triangle footprint is claimed.
This lane is for genuinely attached mud, moss, or similar elements, not for
ordinary raised relief such as concrete aggregate or wood knots.

Wood grain is authored through `ray_tracing.wood_surface_preset_v1`. Its named
`texture_only`, `height_subtle`, `height_standard`, and
`height_exaggerated` profiles keep material/normal-only and true PSG-18
selected-face displacement distinct. The physical profiles express maximum
height in object units and produce a separate closed derived shell; they never
relabel a normal perturbation as geometry.

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
make test-procedural-imported-surface-inset-psg21
make test-procedural-imported-surface-inset-psg21-visual-proof
make test-procedural-imported-surface-growth-psg22
make test-procedural-imported-surface-growth-psg22-visual-proof
make test-procedural-imported-surface-strands-psg23a
make test-procedural-imported-surface-strands-psg23a-visual-proof
make test-procedural-imported-surface-strands-psg23b
make test-procedural-imported-surface-strands-psg23b-visual-proof
make test-procedural-imported-surface-strands-psg23c
make test-procedural-imported-surface-strands-psg23d
make test-procedural-imported-surface-strands-psg23d-visual-proof
make test-procedural-imported-surface-strands-psg23e
make test-procedural-imported-surface-strands-psg23e-visual-proof
make test-procedural-imported-surface-strands-psg23f
make test-procedural-imported-surface-strands-psg23f-visual-proof
make test-procedural-surface-feature-field-contract
python3 tests/integration/test_procedural_surface_feature_spot_compiler_psg24a.py
make test-procedural-surface-feature-curve-contract
python3 tests/integration/test_procedural_surface_feature_curve_compiler_psg24b.py
make test-procedural-surface-feature-inset-psg24c
make test-procedural-surface-feature-deposit-psg24d
make test-procedural-surface-feature-deposit-psg24d-visual-proof
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
