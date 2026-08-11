# RayTracing Timeline Foundation

This directory owns authored frame meaning, detached property-track evaluation,
and immutable evaluated-frame snapshots. It exposes a transactional copied-scene
application seam, but does not mutate live global scene state, invalidate
renderer caches, persist data, own editor state/playback, step simulations, or
schedule wall-clock work.

## TAF0 ownership audit

| Concern | Current owner | Foundation relationship |
| --- | --- | --- |
| FPS, authored start, and frame count | RayTracing animation configuration | Represented canonically by `TimelineRate` and `TimelineRange` |
| Absolute render frames and chunk/resume offsets | app and headless render orchestration | Converted to one `TimelineEvaluationContext` without changing orchestration |
| Seconds and normalized compatibility time | preview/headless callers | Derived from authored frame identity; `normalized_t` is never stored as a key address |
| Spatial path and rigid-motion sampling | path and motion modules | Future consumer; unchanged in TAF0-TAF1 |
| Camera, light, object, and material application | scene preparation | Future TAF2+ binding consumer; no mutation here |
| Renderer cache invalidation | scene/render preparation | Future typed-property metadata; no invalidation emitted here |
| Retained simulation-frame selection | scene-project and simulation bridges | Future explicit authored/simulated binding; unchanged here |

The anticipated invalidation domains are camera, lighting, material/shading,
rigid transform, deforming geometry/acceleration, volume, and simulation-cache
selection. TAF0 records those domains but deliberately does not encode them in
track data: TAF2 must introduce typed property descriptors before any track can
claim an application or invalidation policy.

## TAF1 contract

- Stable bounded target, property, and track identifiers.
- Scalar and vector values; rotation is reserved until its interpolation
  semantics are specified.
- Step and linear interpolation for every supported value type. Scalar tracks
  additionally support cubic Bezier temporal interpolation with monotonic
  frame handles and evaluated derivatives.
- Sorted, unique authored-frame keys.
- Detached result evaluation with no access to scene or render state.
- Fixed capacities and explicit status results; no hidden allocation.

`core_time` remains the monotonic runtime clock and `core_sim` remains a
simulation orchestration boundary. Neither library owns authored timeline
documents or keyframe interpolation, so no shared module changed for this wave.

## TAF2 property registry

`timeline_property_registry.*` adds bounded copied descriptors for stable
property identity, target kind, value type, units, authoring ownership,
per-component bounds, allowed interpolation, and renderer invalidation domains.
The first registered meanings are object position, light intensity, light path
progress, light position, and material roughness. `light/path_progress` is a
bounded scalar from zero to one; its temporal curve controls how quickly a
light advances along separately-authored spatial geometry.

Registry validation refuses unknown properties, mismatched targets/types/units,
non-authorable ownership, unsupported interpolation, out-of-range values, and
duplicate ownership of one target/property pair. Detached document evaluation
copies descriptor provenance and invalidation metadata into results only after
the complete request validates; failed evaluation does not mutate caller output.
The mask is descriptive in TAF2 and does not invalidate renderer state.

## TAF3 evaluated-frame snapshot seam

`timeline_frame_snapshot.*` freezes one canonical evaluation context, copied
property results, provenance, and the aggregate invalidation-domain mask. The
snapshot has no mutator API and is consumed through `const` pointers.

Application stays outside the property registry. A caller supplies a typed
adapter plus reusable scratch scene storage; the seam copies the authored base,
applies every property in deterministic track order, validates the candidate,
and commits to caller output only after the complete application succeeds.
Static documents produce an empty snapshot, copy the base scene exactly, and
report no invalidation domains. Failed target resolution, snapshot validation,
property application, or scene validation leaves the authored base, committed
output, and application report unchanged.

## LTA0 light-first motion seam

`timeline_light_motion.*` combines a `light/path_progress` scalar track with
the existing 2D path plus `CameraPath3D` height controls. The path is sampled
into deterministic 3D arc length, so equal changes in progress represent equal
world-space distance even when the spatial Bezier parameterization is uneven.
The result reports position, global path parameter, total path length,
progress-per-frame, and world-units-per-second. Spatial handles therefore shape
where the light travels while temporal handles independently shape when and how
quickly it travels there.

`runtime_scene_light_timeline_bridge.*` is the first runtime adapter. It
resolves exact unique `light/<runtime-light-id>` identities and applies one
evaluated result transactionally to a caller-owned light array. Missing and
duplicate identities are refused without mutation. Renderer invalidation is
reported as lighting-only metadata; the evaluator itself remains detached from
live render state.

The progress-track contract is stricter than the generic scalar-track
contract. Authored values must remain finite, bounded to `[0,1]`, and
nondecreasing. Cubic segments additionally require ordered control values
`y0 <= y1 <= y2 <= y3`; the generic track validator supplies the matching
ordered time controls. Parser, runtime-document mutation, serialization, and
evaluation all apply this validation so an invalid handle cannot create
temporal reversal or survive save/reopen.

The arc-length table is intentionally rebuilt by this initial pure evaluator.
Interactive playback should cache it by spatial-path revision rather than add
cache ownership to the authored timeline layer.

## LTA1-LTA5 light authoring and P1 intensity slice

Runtime-scene authoring persists one versioned `light_timeline` document.
Schema v1 remains readable with its exact single `progress_track` meaning.
Schema v2 writes a bounded typed `tracks` array with unique
target/property ownership. The required `light/path_progress` track retains
the spatial-motion contract; the optional `light/intensity` scalar track uses
relative-intensity units and finite nonnegative values. A missing intensity
track means the authored base-light intensity, recorded in the evaluated
snapshot as `intensity_authored == false`; it never invents a zero animation.
Legacy `light_path` data can still seed the document, while stale, duplicate,
unknown, mismatched, or invalid track ownership is refused transactionally.
Save/reopen, editor preview, renderer preparation, and headless inspection all
use the same parser and evaluator.

The scene editor keeps the timeline collapsed until a light proxy is selected
and **Animate Light Position** is requested. The bottom center pane then exposes
frame scrubbing, Motion and Intensity lanes, progress/intensity curves,
normalized speed for Motion, key insertion/deletion, key and cubic-handle
dragging, explicit Step/Linear/Bezier selection, and bounded undo/redo. The
first Intensity key gesture lazily creates constant start/end keys at the
selected light's base intensity as one property-scoped undo transaction; it
does not alter Motion keys, the playhead, or target. The viewport remains
visible above the resizable pane.
Selection owns the stable `light/<id>` target and resolves its current array
index only at use sites: reorder preserves the target, disappearance or
duplicate IDs fail closed without retargeting, and selecting another light
while the pane is open is refused until the pane closes.

Frame preparation builds one property snapshot and applies its progress and
optional intensity results to the per-frame light set after the prepared static
scene is copied. Preview, final, and headless consumers therefore receive the
same immutable evaluated snapshot, with separate progress and intensity
provenance and one lighting invalidation domain. This keeps frame identity
authoritative over legacy normalized-time sampling and preserves geometry/TLAS
cache reuse for light-only animation.

## ESP5 evaluated object and simulation channels

`evaluated_scene_snapshot.*` schema v3 retains the fixed-capacity immutable
rigid transform channel array and explicit simulation-cache frame binding.
Compatibility-motion records still carry stable target, position/rotation
presence, provenance, and exact evaluation context. Compound-scene exact-step
records additionally preserve the source quaternion, handoff digest, binding
digest, and packet tick while retaining an Euler compatibility view.

A claimed simulation cache must identify its cache revision, selected and
source frames, rational source rate, frame offset and stride, rational
subframe, interpolation policy, and content digest. `none` remains the explicit
default. Validation rejects duplicate targets, non-finite values, wrong-frame
records, incomplete cache identity, and capacity overflow.

This is a consumer-contract framework, not new puppetry semantics. The S9-C
compound adapter replaces only already-present mapped transform records in a
detached snapshot. Primitive and mesh construction still applies the existing
normalized-time compatibility motion path; source-mesh principal-frame
composition, solving, cache loading, interpolation, and rendering remain
outside the snapshot.

## Next boundary

Complete direct operator review of the P1 Motion/Intensity lane switch,
lazy-create undo/redo, intensity key and handle editing, save/reopen, and
exact-frame Preview response. Keep Preview transport and markers read-only and
retain the one evaluated-scene service. Color, radius, direction, integration,
packaging, and release remain later separately authorized boundaries.
