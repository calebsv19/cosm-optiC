# Reusable movement paths (D-M2 / D-M3 camera and light attachment)

In **Render**, choose **Paths** beside Camera and Light. This is the independent
path library. D-M3 adds explicit camera and animated-light attachment below
the object controls.

1. Click **+ New Path**, type a name, and press Enter. A scene-sized two-point
   route starts at the selected object's base position, or the viewport center.
   It is framed automatically and the **Add: Shift-click** placement preview becomes active.
2. **Shift-click** in the viewport to append points. Plain clicks never add points.
   A cursor preview shows the next segment.
   Placement uses the XY plane at **Draw plane Z** in the inspector. Press Escape
   or click **Move** when finished. Shift-click also appends a point in Move mode.
   Timeline Shift-click keeps its separate multi-selection behavior.
3. Use the controls in the left Paths pane to **Move**, **Add: Shift-click**, **Delete point**, or
   **Frame path**. Numbered squares identify route points. Drag points/handles in
   XY; edit their Z numerically. **Split / extend** in the inspector splits the
   following segment without changing its shape, or extends from the final point.
   Deletion reconnects neighbors and may change the shape; at least two points
   remain. Undo restores edits.
4. **Next segment** switches the outgoing segment between straight and cubic
   Bezier. Handle values are offsets from their point in authored scene units.
   Use **Handles** in the inspector to cycle Independent, Smooth / Linked, and Corner.
   Linked aligns opposite directions while retaining the opposite handle length;
   Corner collapses both handles. Pulling a corner handle starts Independent
   shaping. These policies apply to cubic handles; Straight segments retain
   their explicitly linear geometry. Old scenes default to Independent without
   changing geometry. Mode changes, numeric edits and drags use retained undo.
5. Shape the route before attaching anything. Expand **2. Attach followers...**,
   choose a follower with the object arrows, then **Attach on path**. This places
   its origin on the route and creates **Path progress** keys from 0 to 1 across
   the existing timeline range (or a newly established timeline).
6. Click **Edit follower timing**. Seek with Playhead and enter **At playhead** to
   create/update a progress key; or right-click empty channel space and edit the
   selected key's value. `0` is the path start, `1` the end. Equal progress values
   at two frames make a pause; a later different value resumes movement. Curves
   edits easing through distance, while Paths edits the route itself.
7. Use **Save scene + animation** or File > Save. Reopening retains paths,
   bindings and keys. The playhead is session state.

**Option + left-drag** orbits, **right-drag** pans, and middle-drag also pans.
Navigation takes priority over path point/handle picking, even with Shift held.
The path controls and gesture hints stay in the left pane, leaving the viewport
unobstructed.

Selecting Paths clears camera/light point selection and makes their existing
curves passive context. Their edit handles and legacy edit shortcuts do not
compete with the selected reusable route. Choose Camera or Light explicitly to
edit those paths again.

The same route may have several followers, each with its own progress keys.
Click Paths from a follower's timing channel to return to its bound route.
Editing geometry changes the route for all its followers without changing their
key times or progress values. Frame scene includes reusable path points/handles.
Side panes scroll when controls extend below the visible area.

Attachment replaces the active position source. Existing XYZ keys are retained
but disabled and hidden from active timeline rows. **Detach** restores only the
XYZ tracks that were enabled before attachment, or restores static base placement
when none were enabled. Disabled historical keys stay disabled, even when they
form an incomplete XYZ set. Rebinding preserves the original restoration state;
save/reopen and undo/redo preserve it too. A missing prior track refuses detach.
Older bindings without saved restoration metadata explicitly offer **Detach to
static (legacy)**: the prior source is unknown and XYZ keys remain disabled.
Active legacy motion
or simulation ownership rejects attachment. Rotation and scale are unaffected.
Delete a path only after detaching its followers. All these edits use the retained
scene undo/redo system; no sampled pose is written into base geometry.

## Format and evaluation

`extensions.ray_tracing.authoring.motion_paths` uses schema `ray_motion_paths_v1`:

- `paths`: stable `id`, display `name`, and ordered `points` (stable point `id`,
  `position`, `incoming`, `outgoing`, and outgoing `segment`: `line` or `cubic`).
- `bindings`: `object_id`, `path_id`, `enabled`, and explicit `placement: on_path`.
- Optional `restore_xyz_tracks` on each binding stores the previously enabled
  XYZ track IDs. An empty array means static placement; absence means an older
  binding with unknown prior source. No existing keys are rewritten or removed.
- `scene_timeline` owns each follower's scalar `object/path_progress` channel.

M2 supports 16 open, static world-space paths, 32 points per path, and 64 object
bindings, subject to the existing 64 timeline-channel limit. Coincident positions
are allowed; point identities must be unique within a path. Missing references,
non-finite coordinates, duplicate bindings and simultaneous enabled XYZ/path
position ownership fail scene validation. Progress keys are bounded to [0,1];
computed overshoot clamps at evaluation. A completely stationary route holds its
start position. Endpoints are exact, including repeated final points.

The shared app-local sampler measures **XYZ arc distance**, using a derived table
of 256 chords per segment and evaluating the cubic after distance inversion.
This is a numerical length approximation, not an acceleration/physics solver.
Tables rebuild on scene hydration and are reused by followers and samples;
geometry participates in runtime revision invalidation. Viewport, picking,
acceleration structures and final rendering consume the existing evaluated-object
transform path. Legacy camera XY-distance and light timing are not converted.

## Agent and verification entry points

`include/editor/scene_editor_motion_paths.h` exposes copied readback and retained,
revision-checked create/set/bind commands. UI and native agents call these same
commands. No separate MCP server or alternate authoring store is introduced.
The JSON scene representation remains usable by existing headless render tools.

From Main Edit, prepare a new task-owned proof directory, then run:

```sh
make TOOLCHAIN=clang -j4 scene-editor-workspace-visual-test ray-tracing-render-headless
python3 tests/integration/check_motion_path_render.py --root build/m2-proof --prepare
RAY_TRACING_PROGRAM_ROOT="$PWD/build/m2-proof" build/toolchains/clang/arm64/tests/scene_editor_workspace_visual_test "$PWD/build/m2-proof" "$PWD/build/m2-proof/scene_runtime.json" --dm2
RAY_TRACING_PROGRAM_ROOT="$PWD/build/m2-proof" build/toolchains/clang/arm64/tests/scene_editor_workspace_visual_test "$PWD/build/m2-proof" "$PWD/build/m2-proof/scene_runtime.json" --dm2-reopen
python3 tests/integration/check_motion_path_render.py --root build/m2-proof --cli build/toolchains/clang/arm64/tools/cli/ray_tracing_render_headless
```

Native checks require normal macOS window access. Preparation refuses an existing
root. Proof edits only the copied scene; render outputs remain under that root.
The render check uses a fixed inspection camera and fixed lighting, verifies
editor/headless positions, pixel-exact baked references, a visible move/hold/resume,
and consecutive frames in one process. It does not claim physical simulation,
orientation following, parenting, or looping. Camera/light migration has its own D-M3 checks below.

The native `--dm2-repairs` and separate `--dm2-repairs-reopen` modes use another
fresh fixture prepared with the same command. They cover exact prior-source and
key-data restoration, inactive partial XYZ histories, rebind, missing-track
refusal, legacy compatibility, undo/redo, and cancellation of numeric/name drafts
when selecting another point or path (including viewport selection).
Selection changes discard unfinished field text; press Enter first to commit it.

For the viewport-first usability check, prepare a separate fixture root and run
the native test with `--dm2-usability`. It covers append, Shift-append, Escape,
plane depth, undo/redo, passive legacy paths, attach-after-shaping, save/reopen
and compact toolbar layout.

## Main camera attachment (first D-M3 slice)

Shape a route, expand **2. Attach followers...**, then scroll to **Attach camera
on this route**. This explicitly replaces main-camera translation with reusable
XYZ-distance sampling. **Edit camera route timing** selects the separate
`camera/route_progress` channel. Geometry changes do not retime its keys.
**Detach camera: restore source** restores the prior legacy-progress or direct
position channel without rewriting it. Bind/rebind/detach use the same retained
document transaction and undo/redo as object bindings.

Camera bindings use `target_id: camera/main` instead of `object_id`, plus
`restore_position_tracks` for the prior position owner. The old source remains
inactive for translation but supplies its original orientation timing through a
copied evaluation; yaw/pitch and FOV channels remain independent. Route timing
therefore does not retime the old camera orientation. Existing scenes without a
camera binding continue through the legacy sampler unchanged. Older versions of
optiC do not support the new typed camera binding/property.

Attachment and explicit legacy conversion are separate commands. Conversion and
opt-in focus-target composition are described below. Multiple-camera authoring
is outside this slice.

Verification: prepare a fresh fixture using `check_motion_path_render.py`, run
`--dm3-camera` and `--dm3-camera-reopen` in separate native harness processes,
then run `tests/integration/check_camera_route_render.py --root <fixture>
--cli <headless-cli>`. The proof checks independent legacy orientation/FOV, route
hold, detach restoration, invalid target refusal and exact baked-camera pixels.

## Animated-light attachment (D-M3)

With the existing light timeline active, select a route in **Paths**, expand
**2. Attach followers...**, and scroll to the light controls. **Attach light on
this route** switches the animated light to reusable XYZ route geometry and
creates a `light/route_progress` channel. **Edit light route timing** opens its
keys; equal values create a hold. The existing intensity channel remains active
with its original key times and values. Light color, radius and other base
properties are unchanged.

**Detach light: restore source** restores the exact previous light-progress
track. Rebinding to another route retains that original restoration source;
disabled alternative position history stays disabled. These operations are
atomic retained-document edits, with save/reopen and undo/redo. Missing targets,
missing prior tracks and competing active position owners are rejected.

Light bindings use `target_id: light/<stable-light-id>` and
`restore_position_tracks`, following the camera binding format. Native evaluation
and final rendering share route position, world-distance length and speed
sampling. Legacy scenes without a route binding retain their existing sampler.
Older versions of optiC do not support this light binding/property.

This slice supports the one animated light owned by the existing light timeline.
It does not add multiple animated-light slots. Explicit legacy conversion and
combined-follower/focus-target verification are complete as described below.

Verification: prepare a fresh fixture with `check_motion_path_render.py`, run
`--dm3-light` then `--dm3-light-reopen` in separate native harness processes, and
run `tests/integration/check_light_route_render.py --root <fixture> --cli
<headless-cli>`. Tests exercise animated intensity, route hold, rebind, inactive
position history, invalid-target/source/ownership refusal, detach, undo/redo and
four exact native/headless/baked-reference frame comparisons.


## Explicit legacy conversion and focus composition (D-M3 complete)

In **Paths > Attach followers**, use **Convert legacy camera route** or
**Convert legacy light route**. Conversion is available with an empty route
library. It creates reusable XYZ geometry and separate route-progress timing,
retains stable scene/object/light identities, and disables only the old position
source. The follower panel shows its bound route and inactive source track.
Detach restores that source; undo/redo and save/reopen preserve the transaction.
Camera orientation/FOV and light intensity keep their original channels.

Cubic geometry is copied; quadratic geometry is degree-elevated to cubic.
Camera legacy XY-distance timing and light legacy XYZ-distance timing are fitted
to the common route sampler. Conversion checks every integer and eighth-frame
sample in the timeline range against a positional tolerance of
`1e-4 * max(1, route_length)` in authored scene units. This is a sampled bound,
not an analytic guarantee between samples. Limits are 2–32 path points,
4096 timeline frames and 128 integer-frame timing keys. Unsupported sharp
subframe easing, exhausted capacity, existing route timing or an active binding
cause an atomic refusal with the original scene retained. Conversion requires
an active legacy progress source and an activated 3D scene timeline.

For a bound camera in a scene with an authored `camera_focus_target`, click
**Use scene focus target**. This explicitly enables `use_focus_target` on the
camera binding and aims from the final route position. It owns yaw/pitch while
on; FOV stays independent. Turning it off restores authored orientation.
Coincident camera/target positions retain the sampled orientation; vertical
pitch retains the established ±70-degree limit. The default is off, including
older bindings without the flag, so existing scenes keep their previous output.
This control uses the existing scene target; it does not create new focus targets.

Verification uses a fresh `check_motion_path_render.py --prepare` fixture,
then the native harness `--dm3-complete` and `--dm3-complete-reopen` in separate
processes, followed by `check_m3_completion_render.py --root <fixture> --cli
<headless-cli>`. Cubic/scale-2 and quadratic/scale-0.5 cases passed conversion
pose/pixel tolerance, preserved identities, refusal/undo/reopen, combined
mesh/camera/light followers, independent timing, shape edits and focus composition.
Four combined frames match independently baked reference images exactly.
Six frozen legacy images and eight previous camera/light attachment images
remain pixel-identical. Compact-window controls are exercised at 1024×640.
M3 implementation and automated acceptance are complete; hands-on usability
acceptance remains separate. D-M4 smoothness is the next implementation boundary.


## M4 temporal policies

The timeline interpolation menu adds Auto Smooth, Auto Clamped, Flat / stop,
and Broken handles. Select multiple keys to apply a policy atomically. Automatic
slopes use actual frame spacing; retiming, value edits, key insertion/deletion
recompute affected automatic handles. New keys inside automatic cubic segments
inherit that policy. Direct handle edits explicitly switch the key to Broken.
The scalar curve editor remains available.

Auto Smooth permits overshoot and preserves a shared slope through each interior
key. Auto Clamped uses monotone slopes, including zero slope at reversals; a
segment with both endpoints clamped stays between its endpoint values. Mixed
manual/automatic endpoints retain the manual side, so the whole mixed segment
has no automatic no-overshoot guarantee. Flat sets zero slope at a key; it is
not an interval hold. Hold then jump retains the previous value until the next
key. Linear and legacy Bezier ease remain available. Policy selection makes
adjacent segments cubic so both sides can use the chosen tangent.

Bounded properties reject automatic curves whose analytic extrema leave their
allowed range; choose Auto Clamped or change keys. Old scenes omit the optional
`tangent_mode` and keep Broken/manual handles exactly. Loading saved policies
preserves their handles; recomputation happens on edits. Smooth velocity does
not promise acceleration continuity or limits. Full M4 acceptance remains open.
