# Local rigid simulation workflow

The development rigid workflow connects an authored flat runtime scene to Ball
Bounce's verified producer and optiC's ordinary scene editor. Ball owns physics;
optiC plays the accepted poses. The renderer never runs the solver.

This workflow requires the BR2/BR3 Ball tools and the reviewed optional
`core_scene` result API described in [Rigid result playback](rigid_result_playback.md).
It is local development functionality; package installation and worker delivery
are separate. Prepared convex assets and an authored `rigid_scene` contract are
inputs. This does not yet provide a mass-property or collision-shape editor.

## Prepare the first scene

Use explicit paths to the existing producer, native backend, pack reader and
optiC headless renderer. No host name, installed executable, store head or
simulation duration is silently selected.

```sh
python3 ray_tracing/tools/rigid_scene_workflow.py run \
  --scene <input>/scene.json --assets <input>/asset_set.json \
  --ball-tool <ball>/tools/ball_rigid_scene.py \
  --backend <build>/rigid_scene_native --pack-io <build>/rigid_scene_cache_io \
  --renderer <build>/ray_tracing_render_headless \
  --store <results>/store --expected-head none --ticks 180 \
  --output <prepared>/run_001
```

`<prepared>` must already exist; `run_001` must not exist. A new store uses
`none`; an existing store requires its exact expected revision. Ball's ordinary
run owns native admission, numerical stepping/replay, compatible-result reuse
and compare-and-swap publication. A stale head fails instead of choosing another
result. A compatible run reuses its immutable result with zero solver steps.

The command snapshots the verified result, maps each bound object's visible mesh
reference to the contained accepted asset, binds the exact result revision, then
runs normal optiC preflight. Initial transforms, physical fields, camera/light
state and foreign extensions are preserved. It publishes `scene.json`, `result/`,
`preparation.json` and admission/log evidence together with atomic no-replace
publication. The source scene and an existing output are never overwritten.
A source edit during preparation refuses output publication. A verified Ball
result may already have been published even if later optiC admission fails; it
remains a valid producer result, not an accepted optiC scene.

Open the resulting `scene.json` through the editor's ordinary Open Scene flow.
The scene and result can be moved together. References inside this portable
output are relative. `preparation.json` records the full result, physical-input
and engine identities; `native_replay` distinguishes a newly replayed run from
container-only selection or compatible reuse.

To prepare a scene from an existing result without stepping physics:

```sh
python3 ray_tracing/tools/rigid_scene_workflow.py select \
  --scene <input>/scene.json --bundle <accepted>/result \
  --expected-revision <64-character-revision> \
  --ball-tool <ball>/tools/ball_rigid_scene.py \
  --pack-io <build>/rigid_scene_cache_io \
  --renderer <build>/ray_tracing_render_headless \
  --output <prepared>/selected_001
```

Selection verifies container integrity and current render compatibility. It is
not a new numerical replay or physical qualification.

## Inspect and control playback

In the Render workspace, a bound scene shows **Rigid simulation result**:

- **Choose result** selects an existing local bundle directory. Its manifest
  discovers the revision; ordinary admission independently verifies that identity,
  inventory, physical compatibility and actual visible geometry before applying.
  Failure preserves the current result. Selection is one retained command with
  Undo/Redo and becomes persistent when saved. The chosen external directory is
  referenced by absolute path; use the preparation command for a portable copy.
- **Validate** checks the current authored binding against the verified result
  and loaded geometry without changing playback.
- **Save scene** retains the binding and authored initial conditions.
- **Run local profile** starts the configured local preparation asynchronously.
  Save first. The previous scene/result remains active while it runs. Success
  opens the new prepared scene through the same transactional scene switch used
  by normal editor commands. If the document path/revision or dirty state changed,
  the output is retained for later opening and the current document stays active.
  Failure preserves current playback and reports the diagnostic/log path.

The readout shows accepted time, result/physical/engine identity prefixes, body
roles and motion owners, visibility and a current COM in meters. Full identities
remain available in `preparation.json` and the owned-copy
`RuntimeRigidResultInspection()` API. Optional collision/contact overlays are
not implemented in this slice.

Use the existing timeline Play/Pause, Previous/Next, start/end and ruler scrub
controls. With no authored camera/light timeline, transport derives its 240 Hz
range from the accepted endpoint and creates **zero authored channels**. With
an authored timeline, every displayed time must map to an exact covered source
tick. There is no pose interpolation or extrapolation. Playback never changes
saved initial COM/orientation. Camera/light paths remain independent.

## Local run profile

The native Run control asks for a JSON profile with explicit absolute paths.
It passes arguments directly to Python without a shell. Profiles are trusted
local operator configuration and should refer to the intended local tools.
`output` is create-only, so choose a fresh destination per prepared scene.
`ticks` is a decimal string between 1 and 14400.

```json
{
  "schema": "optic_rigid_run_profile_v1",
  "workflow": "/absolute/ray_tracing/tools/rigid_scene_workflow.py",
  "ball_tool": "/absolute/ball_bounce_sim/tools/ball_rigid_scene.py",
  "backend": "/absolute/build/rigid_scene_native",
  "pack_io": "/absolute/build/rigid_scene_cache_io",
  "assets": "/absolute/input/asset_set.json",
  "store": "/absolute/results/store",
  "expected_head": "<exact-current-revision-or-none>",
  "ticks": "180",
  "renderer": "/absolute/build/ray_tracing_render_headless",
  "output": "/absolute/prepared/run_002"
}
```

The scene input is always the saved current document, not a scene path hidden
in the profile. Only one editor preparation runs at a time. CLI native operations
have bounded timeouts (default 60 seconds per operation). Closing the app does
not cancel an already started local preparation; a completed portable output can
be opened later. Pause affects playback, not the already running producer.

## Verification

`make -C ray_tracing rigid-workflow-probe` builds the native retained-document,
Vulkan panel, transport-event and real producer-job probe. Run it only against
an isolated writable prepared scene and a fresh output profile; it saves that
scene. `tests/test_rigid_scene_workflow.py --help` describes the portable CLI
integration suite. Both require explicit task-owned output and native inputs.
The underlying consumer/geometry sanitizer suite remains separate.

The current solver profile remains convex, eight-body, discrete collision with
its previously documented support/angular-motion limits. Kinematic prescribed
colliders, concave expansion, CCD and broader physical/visible qualification
belong to later work.

See [Rigid pillar scene qualification](rigid_scene_qualification.md) for the bounded BR6 physical/visual evidence and static-contact recovery behavior.

For the qualified eight-body, five-second development case, use an explicit
optimized native build and `--wall-seconds 300` on preparation. The producer and
preparation wrapper share that maximum, while retaining their 60-second default.
This is one bounded transaction deadline; a timeout does not publish a replacement
or authorize automatic resubmission. See [six-brick proof](rigid_six_brick_qualification.md).

The duration envelope is now 14,400 exact source ticks (60 seconds at 240 Hz).
`--wall-seconds` accepts an explicit deadline up to 1800 seconds, default 60.
Run profiles may set optional `wall_seconds` as an integer decimal string in
1..1800; old profiles keep the 60-second default. This budget includes recording
and independent replay; it does not change simulated time or automatically
extend a deadline. See [duration qualification](rigid_result_duration.md).
