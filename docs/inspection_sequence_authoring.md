# Inspection orbits and retained sequence authoring — Stages 5–6

This source feature adds state-bound camera inspection and retained sequence editing to the [sequence playback](sequence_playback.md) and [named camera](camera_sequence.md) contracts. It requires a new build. It does not change the numerical solver, the qualified result envelope or an installed application.

## Four clocks and one recorded state

Presentation time advances at the declared output rate. The source sequence maps it to simulation time, including explicit Play and Hold intervals. A named camera retains its continuous, selected or clip motion policy. An inspection has its own exact local clock, clamped to its first and last rendered samples. None of these clocks uses accumulated viewport activity or process history.

An inspection must occupy exactly one existing source Hold segment and at least two output frames. Its recorded target is a rigid body's center of mass at the declared source tick. The source camera's complete pose, lens and orientation are evaluated at the Hold boundary. A derived `camera/inspect_<id>` starts from that pose. Its world-XY circular orbit rotates the camera's existing offset and orientation together, preserving elevation, radius and initial framing rather than introducing an aim snap.

The existing M5 route scheduler supplies a rest-to-rest timing profile using circular arc length, exact curvature and conservative tangential/normal acceleration budgets. Speed, acceleration and braking must be positive and feasible for the requested interval. Duration is `(frame_count - 1) / FPS`: the last rendered Hold frame reaches the endpoint at rest. The accepted nonzero signed angle is bounded to eight pi radians in magnitude. A complete turn restores the entry pose exactly. A partial turn retains its new endpoint. The derived view holds its entry before the inspection and its endpoint afterward; assigning it to a following Play shot keeps that endpoint while physics resumes.

Inspection creation assigns the derived camera only over the Hold. Outside shot pieces retain their prior camera and clip address. To resume physics from the new viewpoint, explicitly assign the derived camera to the following shot. A cut changes camera selection at an existing presentation sample and does not extend output duration.

This is a deliberate freeze-and-inspect phase. It does not promise velocity or jerk continuity with an arbitrary moving incoming camera, automatic collision framing, arbitrary orbit planes, surface-point targets or a general fly-through planner. The separate [event-camera planner](event_camera_planning.md) now solves explicit recorded-event framing and source-local focus references; this legacy orbit contract keeps its original semantics. Those need separate design and visual acceptance.

## Saved ownership and dependency locks

`extensions.ray_tracing.authoring.inspection_moves` retains up to 16 strict `ray_inspection_orbit_v1` entries. Each has exactly `schema`, `id`, `camera_id`, `source_camera_id`, `target_id`, `start_frame`, `frame_count`, `source_tick`, `target_position`, `anchor`, `angle`, `max_speed`, `acceleration`, `braking` and `dependency`. The anchor contains position, yaw, pitch, vertical FOV, zoom, `has_frame` and an orientation frame. Use the creation command to derive these fields instead of hand-writing provenance.

A deep immutable dependency record binds the source/result identity, world scale, exact source Hold, source and derived camera definitions, the source camera's mapped boundary time, and relevant timeline/path/plan declarations. Converted main-camera paths and relevant focus-target carriers are included. Changes that stale this binding refuse atomically. Adding an unrelated camera remains allowed. At use, the source anchor is independently reevaluated and checked; a forged saved anchor cannot supply an authoritative pose. Target position must match the authenticated recorded body state.

Removing an inspection removes its lock; its derived camera and shots remain available for ordinary editing. Recreate the inspection after deliberately changing a bound route, source Hold or camera. Saved overlays preserve the owning rigid-result and managed-asset declarations, including when UI-generated overlays replace the ray-tracing namespace.

Internal evaluated snapshots are schema 7. Inspection ID and exact local time join camera/shot/motion and simulation provenance. Native preparation rejects stale or forged evaluated metadata. Native geometry remains qualified at integer presentation frames; exact camera subframe clock evaluation is supported, but fractional shutter geometry and motion blur are not certified by this slice.

## Native editor workflow

Open **Render → Camera → Timing → Sequence/cameras/inspection**. The scrollable panel uses the existing retained scene document, shared history and Save owner.

1. Bind the admitted rigid source to an explicit qualified prefix and final Hold, then set up named cameras. Source setup refuses rates that cannot represent the exact tick mapping.
2. Select, split and change source segments between Play and Hold, or move a segment end boundary. Reflow computes subsequent source starts exactly and retains total output duration. Qualified source coverage and existing inspection locks remain enforced.
3. Select, duplicate or remove cameras. Edit base position, yaw/pitch, vertical FOV, and continuous/selected/clip policy with speed and offset. Edit shot assignment, split points, clip start and end boundaries.
4. Select the recorded target and set inspection angle, speed, acceleration and braking. Create the orbit on an exact Hold, or remove it to unlock its dependencies.
5. Inspect the active camera/shot and separate presentation, simulation, camera-motion and inspection clock readouts. Scrub, Undo/Redo, Save and reopen through the same document owner.

Successful edits pause transport. Invalid or stale edits leave the prior document intact. An active native text field blocks a competing MCP mutation. Queued row text has stable per-row storage for the whole frame.

## MCP and typed commands

The existing scene-editor MCP catalog exposes these commands through the same retained owner. Reads require no revision; mutations require the current `expected_revision`.

| MCP tool | Native command | Purpose |
| --- | --- | --- |
| `sequence_read` | `sequence.read` | Saved declarations, inspection summaries and clock readback |
| `sequence_read_section` | `sequence.read_section` | Bounded `sequence`, `camera_rig`, `inspection_moves` or `clocks` section |
| `sequence_apply` | `sequence.apply` | Atomic complete typed sections |
| `sequence_source_setup` | `sequence.source_setup` | Bind qualified rigid prefix and final Hold |
| `sequence_segment_split` | `sequence.segment_split` | Split without changing source mapping |
| `sequence_segment_boundary` | `sequence.segment_boundary` | Move a boundary and reflow exact source starts |
| `sequence_segment_mode` | `sequence.segment_mode` | Set Play or Hold |
| `sequence_setup` | `sequence.setup` | Create named main camera from current authoring |
| `sequence_camera_upsert` | `sequence.camera_upsert` | Insert or replace a complete camera |
| `sequence_camera_remove` | `sequence.camera_remove` | Remove an unreferenced camera |
| `sequence_clock_set` | `sequence.clock_set` | Replace one complete camera clock |
| `sequence_shots_set` | `sequence.shots_set` | Replace complete gap-free shot coverage |
| `sequence_inspect` | `sequence.inspect` | Capture boundary state and create orbit shot |
| `sequence_inspection_remove` | `sequence.inspection_remove` | Remove lock while retaining camera and shots |

For a scene already declaring a 72-frame Hold at local frame 15, this example requests a complete orbit. Substitute the actual current revision and an admitted target/camera:

```json
{
  "expected_revision": 42,
  "id": "contact_orbit",
  "camera_id": "camera/A",
  "target_id": "copper_bracket",
  "start_frame": 15,
  "frame_count": 72,
  "angle": 6.283185307179586,
  "max_speed": 30,
  "acceleration": 100,
  "braking": 100
}
```

`sequence_apply` takes `changes` with optional complete `sequence`, `camera_rig`, `inspection_moves`, `scene_timeline`, `motion_paths`, `motion_plans` and the Stage 7 `sequence_volume` sections. `null` removes a section; unknown fields and incomplete native contracts refuse. Readbacks are bounded to 48 KiB; use section reads for larger edits. Inspection summaries replace opaque dependency payloads with a lock marker and are not complete replacement declarations. Preserve the saved inspection entries when editing another section rather than round-tripping summaries as provenance.

## Qualified behavior and boundary

Strict and ASan/UBSan orbit tests each pass 12,270 checks, including acceleration budgets, infeasible refusal and exact closed endpoints. Native proof covers 114 samples, 456 exact held-body comparisons, frozen prepared geometry, shuffled seeking, closed and partial endpoints and eight refused cases. A 102-frame native output is exactly 4.25 seconds at 24 FPS with exact PTS. Its 72 held samples contain 71 distinct images; the closed-orbit endpoint repeats its entry image intentionally.

Actual SDL editor controls, shared Undo/Redo, field guards, real stdio MCP edits, Save and fresh-process reopen pass. A converted legacy main-camera route preserves yaw/pitch under arbitrary seeking. Full application compilation and focused runtime/timeline/motion regressions pass. Source remains within the existing Kinetic Machinery Spill result's qualified ticks 0–587. These low-resolution diagnostic frames establish behavior, not artistic composition or final image quality.

[Stages 7–8](sequence_sources_and_multiview.md) now add a pinned complete VF3D v1 adapter, synchronized angle/edit/matrix delivery and explicit temporal review. Other source families still require their own complete-state Hold proof. No release, installed-app refresh or solver extension is implied.
