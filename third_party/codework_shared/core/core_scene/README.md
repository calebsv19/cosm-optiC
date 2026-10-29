## 1.9.0 — bounded compound immutable-result matching

The optional rigid-result matcher now revalidates up to three collision children
per body through `CORE_SCENE_RIGID_RESULT_CHILDREN`, matching the caller-admitted
bracket/arch profile. The old internal one-child validation refused otherwise
valid compound contracts. Result bytes, physical projection hashing, body/pair/
contact/time budgets and single-child behavior are unchanged. Callers still own
verified geometry, numerical qualification and stricter admission limits. This
adds no numerical physics or renderer dependency.

# core_scene

Shared app-neutral scene contract helpers for cross-program handoff.

## Current Scope
- Typed shared scene-root contract helpers:
  - space-mode vocabulary (`2d` / `3d`)
  - canonical scene-root metadata validation (`scene_id`, mode intent/default, unit kind, world scale)
- Typed shared object contract helpers:
  - canonical object-kind vocabulary for the first supported authoring objects
  - canonical primitive payload validation for:
    - `plane_primitive`
    - `rect_prism_primitive`
  - canonical non-primitive contract validation for:
    - `mesh_asset_instance`
  - shared `geometry_ref.kind` vocabulary for:
    - `shape_asset`
    - `mesh_asset`
  - object-contract validation layered on top of `core_object`
- Existing path/source helpers for scene-bundle ingestion.
- Shared detection for bundle/source file types used by app-specific loaders.
- Shared bundle resolver (`core_scene_bundle_resolve`) for:
  - `fluid_source.path`
  - optional `scene_metadata.camera_path`
  - optional `scene_metadata.light_path`
  - optional `scene_metadata.asset_mapping_profile`

## Current Contract
- `core_scene_root_contract_init(...)` seeds a deterministic 2D + meters + `world_scale = 1.0` root scaffold, but callers must still assign a non-empty `scene_id`.
- `core_scene_root_contract_validate(...)` accepts only known space modes and known `CoreUnitKind` values, then defers world-scale validation to `core_units`.
- `core_scene_space_mode_parse(...)`, `core_scene_object_kind_parse(...)`, and `core_scene_geometry_ref_kind_parse(...)` expose fixed current vocabulary only and clear their output enums to `UNKNOWN` on failure.
- `core_scene_object_contract_prepare(...)` maps object kinds onto `core_object` identity + dimensional-mode policy:
  - `rect_prism_primitive` promotes to full 3D
  - `mesh_asset_instance` promotes to full 3D
  - all other current supported kinds prepare as plane-locked `XY`
- Primitive payload ownership is narrow:
  - `plane_primitive` requires a plane-locked object plus only plane payload
  - `rect_prism_primitive` requires a full-3D object plus only prism payload
  - `mesh_asset_instance` requires a full-3D object and no primitive payload
  - other non-primitive kinds must not carry primitive payloads
- `core_scene_dirname(...)` and `core_scene_resolve_path(...)` are string helpers only. They preserve dot segments and do not canonicalize or enforce security policy.
- `core_scene_bundle_resolve(...)` uses bounded string scanning, not a full JSON parser. It resolves supported source paths plus optional camera/light/profile metadata, but malformed or ambiguous JSON semantics remain intentionally limited to the current scanner behavior.

## Boundaries
- `core_scene` owns typed scene-root/object/primitive contracts plus scene-bundle source/path resolution helpers.
- `core_object` owns base object identity, transform, dimensional-mode, and primitive-independent validation semantics.
- `core_units` owns scalar unit vocabulary and world-scale validation semantics.
- `core_scene_compile` owns authoring-to-runtime normalization, runtime JSON emission, compile metadata, extension preservation, and overlay/diff helpers.
- Apps own retained scene stores, renderer/editor behavior, asset loading, import UX, overlay/writeback policy, and solver/runtime semantics.

## Dependencies
- `core_base`
- `core_object`
- `core_units`

## Status
- Additive typed-scene bootstrap (`v1.2.0`) now recognizes `mesh_asset_instance` as a shared non-primitive object kind and exposes shared `geometry_ref.kind` vocabulary for `shape_asset` and `mesh_asset`.

## Rigid scene metadata admission (1.3.0)

Additive `core_scene_rigid.h` defines explicit static/dynamic/visual roles,
transform authority, separate collision/visible/source identities and principal
mass-frame validation. `core_scene_rigid_validate` is UI/solver/JSON-free. The
optional `core_scene_rigid_json.c` adapter and `rigid-scene-tool` target reuse
cJSON without adding it to the base library's linkage. See
[Rigid scene v1](RIGID_SCENE_V1.md) for fields, limits, ownership claims,
compatibility, proof boundaries and repeatable commands. Structural metadata
acceptance does not authenticate assets or enable execution. Older APIs and
finite scene behavior remain unchanged.

## Development 1.4.0 — immutable rigid result consumer

The optional `core_scene_rigid_result.h` adapter admits bounded
`core_rigid_motion_result_v1` JSON, checks physical projection compatibility,
resolves exact rational timeline samples and maps source vertices through their
COM/principal frame. It owns no physics, file publication, renderer or UI.
Compile its source separately with cJSON and a host-supplied SHA-256
callback; the default archive and existing APIs remain unchanged. Callers retain
trusted expected bundle identity and authenticate/capture bytes before admission.
See `shared/docs/rigid_motion_result_v1.md` and optiC's BR4 consumer qualification.

## Development 1.5.0 — bounded rigid result capacity

The optional rigid result consumer now accepts up to16 bodies,120 unordered
pairs and120 contact constraints. Existing eight-body results and API calls
retain their meaning. Recompile consumers: the public result struct's embedded
binding storage has grown. Wire schema,240Hz exact sampling and2400-tick limit
are unchanged. This capacity does not establish physical qualification for every
16-body configuration or arbitrary compound contact clusters.

## Development1.6.0 — analytical sphere and contact policy

Optional rigid metadata admits analytic_sphere. The scene/result contract carries
CoreSceneRigidContactPolicy with default legacy0.5m/s restitution cutoff and an
explicit zero-cutoff variant. JSON policy names and physical matching preserve
old default results and bind new choices into result identity. Recompile public
struct consumers. Core owns contract meaning, not collision/inertia computation;
the Ball producer verifies radius/assets and supplies spherical mass properties.
No general concave/compound solver or CCD is introduced.

## Development1.7.0 — explicit contact resistance material lengths

CoreSceneRigidBinding adds optional rolling_resistance_length_m and
 torsional_resistance_length_m. Both are finite nonnegative metre lengths,
default0; contact pairs combine bymaximum. JSON admission and optional result
projection/matching retain zero-field legacy bytes and bind nonzero changes.
Recompile public struct consumers. Core owns data meaning; Ball owns the dry
contact-moment operator and optiC consumes accepted poses. No shared solver or
renderer dependency is introduced. Application/canonical/installed rollout is
separate from this reviewed development-source adoption.

## Optional rigid result envelope (development 1.8.0)

The data-only optional result reader now admits up to32 bodies,496 unordered
pairs/constraints and2400 ticks. Its strict JSON ceiling is64 MiB and4 million
value nodes; depth32, exact rational timestamps, identity, finite/inertia,
physical matching and failure cleanup remain enforced. Existing16-body results
remain supported. CoreSceneRigidResult embeds a larger binding array: recompile
consumers; this development lane promises source compatibility, not a stable
compiled struct ABI. Ball/optiC retained Main Edit source pins explicitly bind
these inputs. Solver capacity and package/inventory policy remain app-owned;
asset capacity stays8 and the existing total bundle byte budget remains64 MiB.
No vendor subtree, canonical app, installed package or production adoption is
implied.32-body physical, result, shared-reader and playback qualification is
recorded in the companion app docs/rigid_capacity32_qualification.md.

## Development 1.10.0 — decomposition result capacity

The optional rigid result validator admits up to 12 convex children per body. This is a metadata envelope, not numerical qualification. App producers retain the 496 child-pair budget and their physical/rollback gates. Existing one-to-three-child results remain compatible. Ball/optiC native decomposition intake is qualified separately in Main Edits; no subtree rollout or installed adoption is implied.

## Development 1.11.0 — minute rigid result envelope

The optional exact rigid result profile now permits 14,400 ticks at 240 Hz
(60 seconds, 14,401 states including tick zero). Producers and consumers should
use `CORE_SCENE_RIGID_RESULT_TICKS` for this ceiling. Existing shorter results
retain their schema and meaning. The 64 MiB JSON, four-million-node, 32-body,
12-child/body and 496-pair/constraint limits remain independent; dense results
can reach a byte or node limit before the duration ceiling. This is a storage
and admission envelope, not a promise that a numerical solver can advance every
scene for a minute. Ball owns physics, exact replay and refusal rollback; optiC
owns immutable result playback. No new interpolation, held tail or sleep policy.
