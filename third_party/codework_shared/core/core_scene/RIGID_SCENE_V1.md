# Rigid scene contract v1

Introduced by core_scene1.3.0. This is **metadata admission**, not a solver,
verified asset loader, pose cache, renderer integration or execution capability.
The optional JSON adapter is independent of the default library's dependencies.

## Ownership

`core_scene_rigid.h` defines typed roles, transform authority and geometry/mass
bindings. `core_scene_rigid_validate` is read-only. The optional
`core_scene_rigid_json.h` adapter uses the existing cJSON implementation;
`core_scene_rigid_json_read` assigns output only after successful validation.
`core_scene_compile` retains normalization and extension preservation. Apps retain
asset verification, physical policies/solver execution, result publication and
render evaluation. Do not use structural acceptance as permission to execute.

## Scene envelope and extension

The input is `scene_authoring_v1` or `scene_runtime_v1`, schema_version1, a stable
scene_id, space_mode_default3d, unit_systemmeters and world_scale1. Preserve the
ordinary scene objects and foreign extensions. Read the declaration at
`extensions.rigid_scene`:

| Field | Meaning |
| --- | --- |
| schema | `core_rigid_scene_v1` |
| coordinate_system | `right_handed_z_up_meters` |
| rate_numerator / rate_denominator | Positive uint32 rational ticks per second |
| origin_tick | Explicit0 in v1 |
| end_tick | Positive uint32 last requested tick; finite budget policy remains app-owned |
| gravity_mps2 | Three finite world components; zero must be explicit |
| contact_policy | `min_restitution_geometric_friction_v1` |
| bindings | One declaration per explicitly bound object |

The contact policy records the existing compound-world combination rule:
minimum of body restitutions and square root of their friction product. BR2 must
explicitly enable that policy in the selected solver. No numerical execution is
introduced by naming it. Initial individual restitution/friction ranges are0–1.

Hierarchy and constraints are unsupported in this first rigid declaration, so
nonempty arrays refuse rather than bypass world-space transform ownership.
Object parent_id must be absent/empty for physical bodies. The document may
contain at most64objects,64asset metadata entries and64bindings. Caller-provided
limits independently bound physical bodies, conservative all-pairs capacity and
collision children. The inspect tools use the intended Ball baseline8/28/64 and
report those limits; they do not establish a generic solver capacity. BR2 must
bind admission limits to its actual world configuration and overflow behavior.

## Roles and motion authority

| role | motion_owner | collides | body_id |
| --- | --- | --- | --- |
| visual_only | authored | false | 0 |
| static | authored | true | Unique positive uint32 |
| dynamic | simulation | true | Unique positive uint32 |
| kinematic | prescribed | Reserved; rejected | Reserved |

visual_only records contain only these fields plus object_id. Unbound ordinary
objects have no collision participation implied by this extension. Static/dynamic
bindings initially require mesh_asset_instance objects with matching
geometry_ref.kindmesh_asset and geometry_ref.id. Multiple instances can share
one mesh/collision asset; object/body identities cannot be duplicated.

For a physical body, object transform.position is **world COM**, legacy Euler
rotation is zero and scale is exactly(1,1,1). Physical principal orientation is
explicit orientation_wxyz. Scale must be baked into independently qualified
geometry and mass properties; implicit runtime scaling is not admitted.

The JSON adapter extracts competing ownership from Ray authoring
object_motion_tracks, motion_paths.bindings and scene_timeline.tracks for
object/transform/* and object/path_progress. It also detects the finite Ball
namespace's body bindings. Enabled physical transform claims refuse; disabled
claims have no authority, and enabled must be boolean when present. Camera/light
paths and non-transform presentation tracks remain independent. Multiple
recognized owner classes on a visual-only object also refuse. Future producers
must register their transform claims with the adapter before adoption; arbitrary
foreign JSON does not become executable motion.

## Geometry and mass binding

Each physical binding additionally requires:

- collision_asset_id, source_asset_id and mesh_asset_id: distinct logical roles;
  collision/source/visible SHA-256 fields bind those identities separately.
- source_center_m[3], principal_to_source[9]: source-space COM and a row-major,
  orthonormal right-handed basis whose columns are principal axes in canonical
  source coordinates. The basis maps principal vectors into source space.
- orientation_wxyz[4]: finite unit principal quaternion, norm tolerance1e-9.
- linear_velocity_mps[3], angular_velocity_radps[3]: finite world components.
- mass_kg, principal_inertia_kgm2[3], restitution and friction.

Asset catalog schema is `core_rigid_asset_metadata_v1`, with an assets array.
Each asset has asset_id, source_asset_id, mesh_asset_id, source_sha256,
mesh_sha256, collision_sha256, shape, child_count, source_center_m,
principal_to_source and inertia_per_kg[3]. Shapes currently declared are
convex_compound and static_triangles. Dynamic triangle meshes refuse.

IDs contain ASCII letters/digits/underscore/hyphen/dot and fit63characters.
SHA-256 values are64lowercase hexadecimal characters. Duplicate catalog IDs and
missing or conflicting bindings refuse. Physical binding mass frames must agree
with catalog metadata within1e-10relative/absolute tolerance; basis validation is
1e-9. Dynamic mass is positive and principal inertia is positive and agrees with
asset inertia_per_kg times mass within1e-10relative tolerance. Static response
mass/inertia and velocities are zero; this represents immovability rather than
an assertion that source geometry has zero physical mass.

Visible vertices follow the existing finite bridge mapping after explicit source
axis/unit conversion:

`world = COM + q * transpose(principal_to_source) * (source_vertex - source_center)`

The external catalog is a **metadata assertion** at this layer. The validator
checks consistency against it, not mesh bytes, decomposition quality, source
conversion correctness or derived mass accuracy. Editing both a declaration and
its catalog can produce another structurally valid assertion. Before BR2
execution, the app must construct the catalog through an independent verified
asset loader and refuse content/provenance mismatches. Synthetic fixtures must
never be represented as qualified collision assets.

## Strictness and compatibility

Input JSON is bounded to1MiB, depth32 and32768nodes, with at most256members per
object. Duplicate decoded keys, escaped/literal NUL, trailing content,
nonfinite values, malformed numbers/types, unsupported schema and unknown fields
inside the owned rigid declaration/catalog refuse. Foreign namespaces are
preserved through normal scene compilation; this adapter does not rewrite them.

Failures return a code, object_id when available and a diagnostic. Typed input
is never modified; JSON output remains unchanged on failure. Existing scene API
behavior and finite ball_collision_scene_v1 packets are unchanged. The current
Ball finite executor and Ray execution paths explicitly refuse a rigid_scene
extension pending BR2/BR4, even when the standalone contract tool accepts it.
There is no migration of old documents or result identity by this API.

## Local commands and proof

From the core_scene directory, use fresh task-owned output roots:

```sh
make OBJ_DIR=/private/tmp/<task>/shared rigid-contract-test rigid-scene-tool
make OBJ_DIR=/private/tmp/<task>/shared PROOF_ROOT=/private/tmp/<task>/proof rigid-admission-test
/private/tmp/<task>/shared/rigid_scene_inspect \
  tests/fixtures/rigid_scene_v1/scene_authoring.json \
  tests/fixtures/rigid_scene_v1/asset_metadata.json
```

The inspect reply labels proof_layercontract_metadata_only and
execution_supportedfalse. The fixture is a hidden floor, two static pillars
sharing an asset and one asymmetric dynamic body with **synthetic metadata**.
It is not runnable physics. The test matrix covers admission/refusal and checks
identical replies across shared, Ball-portable-cJSON and Ray-cJSON tool builds.
Strict legacy core_scene tests, compiler extension round-trip, sanitizer runs,
real execution refusals and the finite bridge regression are separate gates.

## October 27 — native decomposition metadata envelope

Workspace core_scene development1.10.0 extends only the optional rigid result envelope to12 convex children/body. Ball/optiC Main Edits update exact source bindings; the native496 child-pair and four-manifold seals remain, and fast swept response retains its separate three-child eligibility. Existing lower decomposition artifacts are imported app-locally with source-solid mass/provenance verification; core_scene owns metadata meaning and core_pack immutable storage remains reused. Six U-channel/wrench controls pass while a deeper jaw impact fails unchanged physical gates; metadata acceptance is not solver qualification. No subtree rollout, installed consumer update, program VERSION or release action.
