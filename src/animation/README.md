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

## Shared scene timeline: current A-C contract

The Render workspace has a shared camera/light timeline backed by the retained
scene document. This is the first camera/light authoring slice. Object rotation/scale, independent paths for multiple lights, capability creation, spawn/
despawn events and simulation-cache playback remain future adapters.

### Owners and identity

- `SceneEditorDocument` owns authored scene data, command history, atomic save,
  rollback and revisions. `SceneEditorDocumentSetTimeline` is one revision-checked
  command; the timeline is not a second save or history system.
- `SceneTimelineSession` owns transient selection, playback and edit state. It
  reuses `PreviewTransport`; it does not own authored keys or write files.
- `RuntimeSceneTimeline` is a derived scene-load cache. Evaluated snapshots are
  detached frame results, never an authoring database.
- `TimelineEntityBindings` maps stable target aliases to stable entity IDs.
  Reordering does not change identity. Missing/replaced entities never silently
  resolve to another array entry. Object and light aliases may share one entity,
  but cannot independently acquire ownership of the same transform.
- Direct position and path progress compete for position ownership. Simulation
  ownership blocks authored transforms while allowing independent properties
  such as emission intensity. These are extension contracts; the current editor
  catalog binds the main camera and existing authored light IDs.

Begin-edit requires current revisions and an editable selection, then pauses
playback. Scene/revision changes cancel stale gestures. Spatial and curve gestures
preview locally and commit once on release, including release outside the pane.
Escape restores the retained state. Undo/redo and save/reopen use the scene owner.

### Time and evaluation

`TimelineRate` is rational; `TimelineRange` supplies an absolute start frame and
frame count. `TimelineSample` includes a rational subframe. Elapsed playback maps
once into that sample and delegates to exact-sample evaluation. Selection does
not move the playhead. A scene timeline, when present, is the camera/light clock;
legacy normalized time is a derived compatibility value.

`RayEvaluatedSceneCaptureSample` supplies the same camera/light values to Preview,
desktop frame preparation and headless rendering. Render's Preview action enters
paused at the current sample and returns Preview's final sample on close. A
one-frame export retains the scene clock/range while using export count one in
its render session: sequence duration and output count are separate concepts.
Snapshot identity includes timeline content and the existing scene/cache identity.

### Persistence and migration

`extensions.ray_tracing.authoring.scene_timeline` uses version 1. Tracks retain
stable track/target/property IDs, enabled state, authored source, units, keys,
interpolation and temporal handles. Scalar and vec3 values are supported by the
codec. Values are stored in authored scene units; runtime world-distance scaling
belongs to the application adapter. Parsing is transactional and rejects invalid
versions, types, capacity, units, bounds and duplicate ownership.

Entering Render shows the timeline pane. Explicit activation creates the retained
timeline. Direct Camera-mode editor entry also synchronizes Render pane visibility;
it does not require selecting Render a second time. Activation seeds the
timeline from existing light animation and camera progress/lens channels. A
malformed existing timeline is not overwritten. The legacy light timeline remains
the spatial-path carrier during migration; shared tracks own its time and values.
Unmigrated scenes keep their compatibility behavior.

File > Save preserves the retained scene timeline when the legacy runtime overlay
replaces the ray-tracing namespace. Runtime caches cannot replace or erase its
tracks, rate or range. Camera/light spatial commands update their retained path
fields; the light command changes the spatial carrier without rewriting shared
temporal channels. Unrelated scene JSON remains owned by the existing document.

### Authoring surface

Render provides play/pause, scrubbing, track selection, key insertion/movement/
deletion, numeric frame/value entry, Hold/Linear/Ease interpolation, and a scalar
curve pane with editable temporal handles. Handle edits preserve bounded values;
light progress also preserves monotonic controls. Neighboring handle times are
fitted when keys move. Track selection uses stable IDs, not row indices, and
routes camera/light selections to their spatial tools without changing time.

Camera authoring includes XYZ/yaw/pitch inspection, path point/handle movement,
path mode/link controls and retained rotation. Light path editing uses the same
retained gesture lifecycle. Brightness timeline selects/creates an intensity
track; camera yaw/pitch channels can also be created. Render replaces the legacy
global brightness/radius sliders with the authored brightness action. Scene
workspace compatibility controls retain their prior behavior.

Camera channels are path progress (0..1), position (world-distance vec3), yaw
(radians), pitch (radians, -pi/2..pi/2), and vertical FOV (degrees, 1..179).
Explicit orientation/lens channels override path-sampled values. Direct position
replaces path position. Progress without a camera path is an error. The initial
scalar panel does not provide vector-key editing; spatial paths use viewport and
inspector commands.

Render Frame All includes geometry, camera/light points, Bezier handles and gizmo
padding. The bounds module is presentation-only. Selected-object and focused-
material framing retain their existing behavior. Queued labels have persistent,
separate backing storage through frame submission.

### Runtime admission and extension requirements

Registry membership declares a property's generic contract, not that every
renderer consumes it. Scene admission checks live target identity and current
consumer availability. Enabled camera channels use the camera adapter. Enabled
light progress/intensity require the matching spatial carrier and an enabled
progress track. Other enabled property families fail with a consumer diagnostic;
disabled future tracks remain retained. The current spatial adapter supplies one
animated light. It matches target ID as well as property ID, and ignores disabled
progress tracks when locating that binding.

A future object/capability adapter must:

1. Preserve stable authored entity identity and expose capability aliases through
   a copied binding catalog; never create an independent object database.
2. Declare units, bounds and property ownership in the registry and binding layer,
   including conflicts with simulation-provided transforms.
3. Persist through retained scene commands, with revision cancellation, undo/redo
   and atomic save/reopen proof.
4. Evaluate from the exact shared frame/subframe into detached snapshots, without
   advancing simulation or mutating authoring during scrubbing/rendering.
5. Extend `runtime_scene_timeline_consumers.c` only alongside the real consumer,
   with identity isolation, accepted ordinary cases, refusal/state-preservation
   checks and Preview/desktop/headless evidence.

Simulation cache identity and frame-mapping requirements above still apply.
Object creation, emitter conversion and lifecycle/event tracks must implement
these contracts before becoming enabled editor capabilities.

### Verification and limits

`make test-scene-timeline-entity-contract` covers stable aliases, competing owners,
simulation ownership, selection/revision behavior, units, codec round trips and
transactional refusal. The `runtime_scene_editor` group covers retained commands,
failure rollback, actual legacy-save persistence, rational rate/nonzero range,
consumer admission and camera controls. Camera evaluation and evaluated-scene
checks cover exact subframes, seek/elapsed agreement and target isolation.

The native `scene_editor_workspace_visual_test --timeline` workflow covers
camera/light X/Y/Z gestures, Y/Z undo/cancel, inspector entry, curve-handle editing,
File > Save, reopen, preserved curve controls, Preview 40->41 handoff, and an
actual single-frame desktop export at frame 40. Start each run with a fresh copied
sample and task-owned working directory: the probe intentionally saves edits.
`tests/integration/check_scene_timeline_render_parity.py` compares those saved
camera/light samples with fresh-process headless preparation and rendering.
This is parameter/sample parity, not desktop/headless pixel equivalence.

Foundation A, pane, navigation and viewport-bridge gates cover adjacent editor
behavior. Main Edit package/launcher checks and installed source/binary identity
are separate from native source-workflow proof. Package refresh does not establish
human acceptance of the installed GUI. Current private work status records exact
checkpoints and unresolved acceptance items; release/Registry/canonical adoption
remain separate from this development slice.

## D1: retained object position authoring

Existing mesh instances, planes and boxes can acquire scalar
`object/transform/position_x`, `position_y` and `position_z` channels targeting
`object/<stable-object-id>`. The three channels are created in one retained
command from the base position; all must be enabled together. Values are absolute
scene-unit positions, scaled once when the runtime timeline loads. Hold, Linear
and scalar Bezier interpolation reuse the existing evaluator and key editor.
Unchanged rotation, scale and mesh pivot semantics stay in the geometry owner.

`runtime_scene_object_timeline.c` validates admission and evaluates positions.
It rejects missing/duplicate targets, unsupported geometry, incomplete XYZ sets
and a target already owned by an enabled legacy motion/simulation track.
Snapshots mark these transforms with `SCENE_TIMELINE` provenance. Exact samples
feed the snapshot and editor; the existing geometry builder's normalized-time
adapter evaluates the same authored frame position without integer rounding.
Retained scene timelines supply canonical normalized time to geometry as well as
camera/light channels; legacy-only scenes retain their previous travel mapping.
Dynamic object channels disable static prepared-scene cache reuse across time.

`scene_editor_object_timeline.c` adds channels through retained document commands,
resolves stable identity, and supplies detached viewport positions. Scrubbing
never writes base transforms. Locked objects cannot be keyed through the timeline
editing commands. Undo/redo and save/reopen use the existing document owner.
Headless summaries expose `evaluated_objects` for the last evaluated frame.

This is position-only authoring: no object path tool, rotation/scale channels,
emitter conversion, lifetime clips, cross-channel key selection or simulation
stepping. A referenced object cannot be removed while its enabled tracks remain;
the retained scene validator refuses the orphaned binding rather than retargeting.
Native `--object-timeline` acceptance covers mesh move/hold/resume, backwards and
subframe seeking, actual generated triangle translation, unchanged base transforms,
invalid edits, undo/redo and save/reopen. The companion
`tests/integration/check_object_timeline_render_parity.py` checks fresh headless
renders at six frames against that saved authoring result.
